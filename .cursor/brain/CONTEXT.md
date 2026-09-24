# Memory (compact)

**PeerDesk** — LAN/VPN remote desktop: Ubuntu host, macOS/Ubuntu viewer, mouse+keyboard, reconnect without restarting the server. No files/audio/clipboard/relay/Wayland/multi-mon in v1.

```mermaid
flowchart LR
  QtUI[Qt6_Widgets] -->|"TLS_Protobuf_input"| Host
  Host[FFmpeg_H264_X11] -->|"video"| QtUI
```

## Stack lock (use this — PRD / production)

| Layer | Locked |
|-------|--------|
| Client | C++17/20 + Qt6 **Widgets** (macOS + Ubuntu) |
| Host | C++ Ubuntu only; X11 XShm/XDamage + XTest |
| Wire | TLS TCP; **Protobuf** control + length-prefixed frames |
| Video | **FFmpeg** H.264 (`libx264`, VAAPI when present) |
| Auth | Argon2id hashes; nonce + HMAC; no plaintext password |
| Build | **CMake + vcpkg or Conan** |
| Ship | macOS `.app` client; Ubuntu `.deb`/AppImage client+server |

**Planning snapshot (2026-08-30):** production **build has not started**. Treat `client/` `server/` `shared/` JPEG + packed-struct binaries as a throwaway senior demo — **not** the locked starter. Next B1 work implements this table, not JPEG.

J0–J3 = `build-ready`. Paths: [CODEMAP.md](./CODEMAP.md). Detail: [DECISIONS.md](./DECISIONS.md), [PROJECT_DESIGN.md](../../PROJECT_DESIGN.md).
