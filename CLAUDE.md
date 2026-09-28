# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

PeerDesk is a LAN/VPN remote desktop: a Qt6 viewer (`peerdesk-client`) views and controls an Ubuntu X11 host (`peerdesk-server`) over TLS, and can disconnect/reconnect without restarting the server.

## Current code vs. locked production stack

The tree in `client/`, `server/`, `shared/` is a **throwaway senior demo** (JPEG frames, hand-packed structs, self-signed TLS with `SSL_VERIFY_NONE`, system packages). The locked production stack in `.cursor/brain/DECISIONS.md` / `CONTEXT.md` is different: Protobuf wire schemas, FFmpeg H.264 (`libx264`, VAAPI when present), X11 XShm/XDamage + XTest, pinned certs, CMake + vcpkg or Conan, `.app` / `.deb` / AppImage installers. Do not grow `shared/src/jpeg.cpp` or the packed-struct protocol as if they were the contract. For production (B1) work, follow the locked table and `.cursor/brain/CODEMAP.md` (current → target paths). If code diverges from DECISIONS, update DECISIONS too.

## Build and test

System deps (Ubuntu): `cmake g++ qt6-base-dev libssl-dev libjpeg-dev libargon2-dev libx11-dev libxtst-dev pkg-config`.

```bash
cmake --workflow --preset debug          # configure build/ + build all + ctest
cmake --build --preset debug-server      # single target (also debug-client, debug-smoke, debug-client-test, ...)
ctest --preset debug                     # all tests, output on failure
cmake -S . -B build && cmake --build build -j   # without presets
```

Tests (both registered with ctest):
- `peerdesk-smoke` (`tests/peerdesk_smoke.cpp`) — no Qt; hand-rolled `expect()` checks for map, protocol round-trips, Argon2/HMAC, JPEG, and a full session against an in-process `HostServer` (bad password, frames, busy second viewer, reconnect).
- `peerdesk-client-test` (`tests/client_test.cpp`) — QtTest; compiles the client sources directly and runs `SessionWorker`/`MainWindow` against an in-process synthetic `HostServer`. ctest sets `QT_QPA_PLATFORM=offscreen`; set it yourself when running the binary directly.

Run one QtTest function: `QT_QPA_PLATFORM=offscreen ./build/peerdesk-client-test worker_bad_password` (`-functions` lists them).

## Running (never on the user's :1 display)

The server captures and injects on whatever `$DISPLAY` points at (`XOpenDisplay(nullptr)`). The user works over **NoMachine on `:1`**, so never run the non-synthetic server there. Use either:
- `./build/peerdesk-server --synthetic --no-inject --port 4473 --data-dir .peerdesk-demo` (draws its own canvas), or
- a nested display: `Xephyr :5 -screen 1280x800 -ac -br -noreset &`, apps with `DISPLAY=:5`, then `DISPLAY=:5 ./build/peerdesk-server --port 4474 --bind 127.0.0.1 --data-dir .peerdesk-demo-x11`.

Default login `jordan` / `peerdesk` (created only when the data dir's `users` file is empty). The README "Running the server" section has the full flag table. Stop servers with SIGINT. Do not `pkill -f "Xephyr :5"` from a shell command (it matches the calling shell); kill by PID or `pkill -x Xephyr`.

## Architecture

Three CMake targets carry the logic: `peerdesk_shared` (static lib: wire protocol, TLS, auth, JPEG, coordinate mapping; used by both sides), `peerdesk_host` (static lib: users, capture, inject, `HostServer`; linked by `peerdesk-server` and both tests), and the client sources (compiled into `peerdesk-client` and again into `peerdesk-client-test`). Ownership (DECISIONS B1–B4): server owns credentials, session occupancy, capture/encode/inject; client owns UI, decode, render, input capture, coordinate mapping; `shared/` owns wire types, TLS, Argon2/HMAC, mapping.

**Wire format** (`shared/src/tls.cpp`, `protocol.cpp`): every message is `[u32 BE length][u8 MsgType][payload]` over an OpenSSL TLS stream, max payload 8 MB. Payloads are packed with `pack_*`/`unpack_*` helpers in `protocol.hpp`; `MsgType` there is the whole protocol.

**Handshake** (`HostServer::handshake`, `SessionWorker::connectToHost`): `Hello{version, username}` → server `AuthChallenge{argon2 params, salt, nonce}` → client computes Argon2id(password, salt) and sends HMAC-SHA256(hash, nonce) → `AuthOk{width, height}` or `AuthFail{reason}`. The password never crosses the wire; the server stores only Argon2id hashes (`server/users.cpp`). Unknown users still get a challenge (salt = HMAC(`fake_secret_`, username)), so the server does not reveal which usernames exist.

**Server session** (`server/host_server.cpp`): `run()` accepts one connection at a time; `session_active_` enforces one viewer (a second gets `AuthFail{SessionBusy}`); the session runs on its own thread so the accept loop keeps going and reconnect works without restart. `session_loop` runs a capture thread (grab full root window → RGB → JPEG → `VideoFrame`, sleeps `1000/fps`, full frames every tick, no damage tracking) while the calling thread reads `Mouse`/`Key`/`Ping`/`Disconnect` and applies XTest input. `send_mu` serializes sends from both threads. The capture source is `X11Capture` or `SyntheticCapture` (`--synthetic`, explicit flag, never a silent fallback). Wayland without `--synthetic` fails setup.

**Client** (`client/`): `MainWindow` is a `QStackedWidget` (connect form ↔ session). `SessionWorker` lives on its own `QThread`; `connectToHost` is invoked queued and then blocks in `pump()` (40 ms recv polls, flushes queued input, pings every 2 s) and talks to the UI only through signals. `VideoSurface` letterboxes frames with `fit_letterbox` and maps widget points back to host pixels with `map_letterbox_point` before emitting `mappedMouse`/`mappedKey`. Those are wired `Qt::DirectConnection` into `enqueueMouse`/`enqueueKey`, which push to mutex-guarded queues so input never waits on recv. `keymap.cpp` translates Qt keys to X11 keysyms with no Xlib dependency.

## Known issues (not yet fixed)

- `SessionWorker::disconnectSession()` is called from the UI thread and touches `conn_` while the worker thread uses/closes it (confirmed with ThreadSanitizer; makes `peerdesk-client-test` crash occasionally). Intended fix: only set `stop_`; have `pump()` send `Disconnect` on the worker thread.
- Xlib's default IO error handler calls `exit()`, so the server dies if its X display (e.g. Xephyr) goes away.
- `scripts/run_demo.sh` `exec`s the client, so its EXIT trap never stops the server.

## Project process (from `.cursor/rules/`)

- `.cursor/brain/` is the source of truth for scope, journeys (J0–J3 are `build-ready`), and locked decisions. Implement features only for `build-ready` journeys or with a waiver logged in `STORIES.md`.
- Out of scope for v1, do not add: file transfer, audio, clipboard, relay, Wayland host, multi-monitor, view-only role, saved profiles (J4), multi-viewer, Mac host. No stub CRUD for out-of-scope features.
- Plans, architecture proposals, and multi-step proposals start with 1–3 Mermaid diagrams (no spaces in node IDs, quote labels with special characters), then short bullets. Skip diagrams for one-file fixes.
