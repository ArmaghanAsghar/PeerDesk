# Project context: cross-platform remote desktop app

## Purpose of this document
This is a context handoff for continuing a system design conversation with another LLM. It summarizes the requirements, architecture decisions, and open questions discussed so far for a TeamViewer-like remote desktop application.

## Requirements (as given by the user)

1. Client-server application. Client runs on **macOS and Ubuntu**. Server runs on **Ubuntu only**.
2. Purpose: remote screen viewing and control, similar to TeamViewer, but scoped to **mouse and keyboard control only** (no file transfer, no audio, no clipboard sync mentioned).
3. Backend screen-sharing logic must be **fast** — implemented in **C++**.
4. Client UI must let the user enter: **IP address, username, password, port** of the target (server) machine, then connect.
5. Any modern open-source tech stack is acceptable for the UI layers.

## Architecture overview

Two separate binaries:

- **Client app** (macOS + Ubuntu): connection UI, local input capture, video decoder/renderer, network client.
- **Server app** (Ubuntu only): auth & session manager, screen capture + encoder, input injector, network server.

Data flows both directions over a persistent connection between the two:
- Client → Server: credentials/auth, mouse & keyboard events.
- Server → Client: encoded video frames.

## Component decisions

### Server (Ubuntu, C++)

**Screen capture**
- X11 sessions: `XShm` (shared-memory frame grabs) + `XDamage` extension (capture only on change, avoids constant full-frame polling).
- Wayland sessions: X11 capture APIs don't apply — use **PipeWire** via the `xdg-desktop-portal` screencast interface instead.
- Detect session type via `XDG_SESSION_TYPE` at startup and branch capture backend accordingly.
- Capture runs on its own thread, writing into a double buffer / ring buffer read by the encoder thread.

**Encoding**
- FFmpeg `libavcodec`, H.264 via `libx264` (software) with a **VAAPI** hardware-accelerated path when a GPU is available.
- Tune for low latency over compression ratio: small GOP, no B-frames, `ultrafast`/`zerolatency` x264 presets.

**Input injection**
- X11: `XTest` extension (`XTestFakeMotionEvent`, `XTestFakeButtonEvent`, `XTestFakeKeyEvent`).
- Wayland: XTest generally doesn't work due to the security model — use the kernel **uinput** interface (`/dev/uinput`) to create a virtual input device instead. uinput also works under X11, so it's a candidate for a single unified injection path (requires appropriate permissions — `input` group membership or udev rule).

**Auth & session**
- Never store or transmit plaintext passwords.
- Store salted password hashes (Argon2id via libsodium/libargon2, or bcrypt).
- Challenge-response auth: server issues a nonce, client responds with `HMAC(password_hash, nonce)` — the raw password never crosses the wire.
- All traffic (auth, video, input) runs over **TLS** (OpenSSL), on top of TCP.

### Client (macOS + Ubuntu, C++ with Qt6)

- **UI framework: Qt6** — chosen specifically because the backend is already C++; avoids introducing a second language/runtime (e.g., Electron) and the IPC/serialization boundary that would create between UI and the performance-critical video path.
- Native on both macOS and Ubuntu.
- Video rendering via `QVideoWidget` or a custom `QOpenGLWidget`.
- Decoding via FFmpeg `libavcodec` (shared media module with the server, since both binaries are C++).
- Local input capture scoped to the video widget; Qt's event system largely normalizes macOS vs Ubuntu event handling (raw `CGEvent` only needed later if global capture outside the widget is required).
- Client maps local cursor coordinates to the remote screen's coordinate space before sending input events (needed because client/server screen resolutions will typically differ).
- Connection UI stores fields: IP, port, username, password (with `QSettings` for saved profiles).

## Protocol design

Single persistent TCP connection (or two sockets), split into two logical channels:

1. **Control channel** — handshake, auth challenge/response, keepalive, resolution/codec negotiation, error signaling. Small JSON or Protobuf messages.
2. **Data channel** — length-prefixed binary frames. Message types include `VIDEO_FRAME`, `MOUSE_EVENT`, `KEY_EVENT`, `PING`.
   - Minimal frame header: `[4-byte length][1-byte type][payload]`.
   - Protobuf recommended for schema-driven serialization shared identically by both C++ binaries.

## Threading model

**Server**: capture thread → encode thread → async network I/O thread (Asio/Boost.Asio), with a separate thread for the control channel/auth so a slow encode never blocks keepalives.

**Client**: network receive thread (feeds decode queue) → decode thread → render on Qt main thread. Input events are sent on their own path immediately, not queued behind incoming video, to keep control responsive.

Recommendation: lock-free queues or mutex-guarded ring buffers between pipeline stages — avoid a single global lock around the frame pipeline (common cause of perceived lag in remote-desktop tools).

## Recommended tech stack summary

| Layer | Choice |
|---|---|
| Language | C++17/20 throughout (client + server) |
| UI framework | Qt6 (Widgets or QML) |
| Networking | Asio (standalone or via Boost), or gRPC/Protobuf for schema-driven messaging |
| Video codec | FFmpeg (libavcodec/libavformat) — x264 software + VAAPI hardware path |
| Screen capture | XShm + XDamage (X11), PipeWire/xdg-desktop-portal (Wayland) |
| Input injection | XTest (X11) or uinput (universal) |
| TLS | OpenSSL |
| Password hashing | Argon2id (libsodium or libargon2) |
| Build system | CMake, with vcpkg or Conan for dependency management |
| Packaging | `.app` bundle (macOS), `.deb`/AppImage (Ubuntu) |

## Open design questions flagged for follow-up

- **Wayland vs X11 on the server**: biggest complexity driver. Decide whether to support both from day one, or ship X11-only first and add Wayland later.
- **Resolution mismatch**: client and server screens will usually differ in size — need coordinate scaling logic between client clicks and server screen coordinates.
- **Multi-monitor support**: single-display-only vs full multi-monitor (requires a monitor-selection step in both capture and injection).
- **NAT/firewall traversal**: current design assumes direct IP:port reachability (LAN/VPN use case, like classic VNC). Supporting "connect from anywhere" would require a relay/rendezvous server — explicitly out of scope unless requested.

## Suggested next steps (not yet started)
- Detailed XShm/XDamage capture loop implementation.
- Concrete Protobuf message schemas for the control and data channels.
- Qt client connection dialog and video-rendering widget implementation.
