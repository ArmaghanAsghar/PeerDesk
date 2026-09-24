# Project design — Remote Desktop for Small Teams

Generated from the `.cursor/brain/` project brain (PRODUCT, DECISIONS, PERSONAS, FLOWS, MODULES, PHASES). This file is a snapshot — the brain files are the source of truth if they diverge later.

## 1. Project mind map

```mermaid
mindmap
  root((Remote Desktop<br/>for Small Teams))
    Product
      North star
        View and control Ubuntu hosts
        From macOS or Ubuntu
        Reconnect without restart
      Deploy
        Desktop installers
        Self hosted on LAN or VPN
        No cloud relay
      NFRs
        TLS everywhere
        No plaintext passwords
        C plus plus fast capture path
        One session per host
    Personas
      Sam Viewer on Mac
      Riley Viewer on Ubuntu
      Jordan Host operator
    Journeys D0 core
      J0 Host prepares server
      J1 Connect and authenticate
      J2 View and control
      J3 Disconnect and reconnect
    Journeys D1 deferred
      J4 Saved profiles
      J5 Wayland capture
      J6 Multi monitor
    Out of scope v1
      File transfer
      Audio
      Clipboard sync
      Mobile or web clients
      Mac as host
      NAT relay
      View only role
    Architecture locked
      Client Qt6 C plus plus
      Protocol TLS TCP plus Protobuf
      Capture X11 plus FFmpeg
      Inject XTest
      Auth Argon2id challenge response
    Modules
      Core
        Client UI
        Session protocol
        Auth and TLS
        Capture and encode
        Input inject
        Decode and render
      Optional later
        Saved profiles
        Wayland capture
        Multi monitor
        NAT relay
```

## 2. System architecture (technical design)

```mermaid
flowchart LR
  subgraph viewer["Viewer machine — Mac or Ubuntu"]
    UI["Qt6 connection UI"]
    LocalInput["Local mouse and keyboard capture"]
    Decode["FFmpeg decode and render"]
    UI --> LocalInput
    Decode --> UI
  end

  subgraph host["Ubuntu host"]
    Creds["Argon2id credential hashes"]
    AuthTLS["Auth and TLS challenge response"]
    Capture["X11 capture — primary display"]
    Encode["FFmpeg encode"]
    Inject["XTest input inject"]
    Capture --> Encode
  end

  UI -->|"TLS connect + credentials"| AuthTLS
  AuthTLS --> Creds
  AuthTLS -->|"session ok"| Encode
  Encode -->|"encoded video frames"| Decode
  LocalInput -->|"mapped mouse and key events"| Inject
```

## 3. Journey pipeline and build gate

```mermaid
flowchart TD
  J0["J0 — Host prepares server<br/>(build-ready)"] --> J1["J1 — Connect and authenticate<br/>(build-ready)"]
  J1 --> J2["J2 — View and control<br/>(build-ready)"]
  J2 --> J3["J3 — Disconnect and reconnect<br/>(build-ready)"]
  J3 -.deferred D1.-> J4["J4 — Saved profiles<br/>(draft)"]
  J2 -.deferred D1.-> J5["J5 — Wayland capture<br/>(draft)"]
  J2 -.deferred D1.-> J6["J6 — Multi-monitor<br/>(draft)"]

  subgraph gate["Status gate — every journey must pass through this before code is written"]
    direction LR
    draft --> personaready["persona-ready"]
    personaready --> validated["client-validated"]
    validated --> buildready["build-ready"]
  end

  buildready -->|"unlocks"| code["Feature code allowed"]

  archLock{"Architecture locked?<br/>(currently: locked 2026-08-30)"}
  archLock -->|"no"| blocked["No build — awaiting human lock"]
  archLock -->|"yes"| code
```

## Current status

- Architecture: **locked** — FFmpeg H.264, Protobuf, vcpkg/Conan, installers ([CONTEXT.md](.cursor/brain/CONTEXT.md))
- Journeys J0–J3: `build-ready`
- Planning snapshot: **production build has not started**. JPEG/packed-struct trees are a throwaway demo, not the starter.
