# Flows

> **Filled by:** DISCOVERY FLOWS step.  
> **Statuses:** `draft` → `persona-ready` → `client-validated` → `build-ready`.  
> Feature code needs **`build-ready`** or an explicit waiver in [STORIES.md](./STORIES.md).  
> **D0:** detail first 2–4 journeys only; later titles may stay thin until D1.

```mermaid
flowchart LR
  J0[J0_ServerReady] --> J1[J1_ConnectAuth]
  J1 --> J2[J2_ViewControl]
  J2 --> J3[J3_DisconnectReconnect]
  J3 -.-> J4[J4_SavedProfiles]
  J2 -.-> J5[J5_Wayland]
  J2 -.-> J6[J6_MultiMonitor]
```

| ID | Title | Status | Primary personas | Wave |
|----|-------|--------|------------------|------|
| J0 | Host prepares the Ubuntu server | build-ready | P3 | D0 |
| J1 | Viewer connects and authenticates | build-ready | P1, P2 | D0 |
| J2 | View remote screen and control mouse/keyboard | build-ready | P1, P2 | D0 |
| J3 | Disconnect and reconnect without restarting the server | build-ready | P1, P2, P3 | D0 |
| J4 | Saved connection profiles | draft | P1, P2 | D1 |
| J5 | Wayland host capture and inject | draft | P3 | D1 |
| J6 | Multi-monitor host | draft | P1, P3 | D1 |
| J7 | NAT relay / connect from anywhere | draft | — | Out of scope v1 |

---

## J0 — Host prepares the Ubuntu server

**Status:** build-ready  
**Goal:** Jordan can install or run the server on an X11 Ubuntu session so it listens on a port with at least one login, and the team can reach that IP:port on the LAN/VPN.

```mermaid
sequenceDiagram
  participant Jordan as Jordan_P3
  participant Server as Server_app
  participant OS as Ubuntu_X11
  Jordan->>OS: install_or_build_server
  Jordan->>Server: start_with_port
  Server->>OS: check_session_is_X11
  Server->>Server: load_or_create_login_hashes
  Server-->>Jordan: listening_on_IP_port
```

```mermaid
stateDiagram-v2
  [*] --> Stopped
  Stopped --> Starting: operator_starts
  Starting --> Listening: X11_and_port_ok
  Starting --> Failed: Wayland_or_bind_or_perms
  Listening --> Stopped: operator_stops
```

**Happy path:** Jordan starts the server on X11 Ubuntu; it binds the chosen port; at least one username exists with a hashed password; status shows it is listening.  
**Failures:** Not X11 (Wayland) → clear error, do not hang. Port in use. Missing capture/inject permissions. No credential configured. Firewall still blocking (document; app cannot fix the network).  
**Evidence:** [CLIENT.md](./CLIENT.md) E10–E12  
**Open conflicts:** none (C1: Wayland deferred)

---

## J1 — Viewer connects and authenticates

**Status:** build-ready  
**Goal:** Sam or Riley enter IP, port, username, and password and either get an authenticated session or a clear refusal.

```mermaid
sequenceDiagram
  participant Viewer as Sam_or_Riley
  participant Client as Client_app
  participant Server as Server_app
  Viewer->>Client: IP_port_user_password
  Client->>Server: TLS_connect
  Server-->>Client: auth_challenge_nonce
  Client->>Server: challenge_response
  alt credentials_ok
    Server-->>Client: session_ok
    Client-->>Viewer: connected
  else bad_creds_or_busy
    Server-->>Client: reject
    Client-->>Viewer: error_message
  end
```

```mermaid
stateDiagram-v2
  [*] --> Form
  Form --> Connecting: submit
  Connecting --> Authed: challenge_ok
  Connecting --> Form: TLS_or_auth_fail
  Authed --> Form: user_cancels_before_video
```

**Happy path:** Reachable host, valid login, no existing session → client is authenticated and ready to receive video (J2). Password is not stored or sent as plaintext.  
**Failures:** Wrong IP/port; TLS/handshake fail; wrong user/password; host already has a viewer; timeout. Errors are specific enough to act (unreachable vs rejected vs busy) without leaking extra accounts.  
**Evidence:** [CLIENT.md](./CLIENT.md) E20–E22, E4  
**Open conflicts:** none

---

## J2 — View remote screen and control mouse/keyboard

**Status:** build-ready  
**Goal:** After auth, the client shows the host desktop and the viewer’s mouse and keyboard drive that desktop. Clicks land on the right host pixels even when the window size differs.

```mermaid
sequenceDiagram
  participant Viewer as Viewer
  participant Client as Client_app
  participant Server as Server_app
  participant Desk as Host_desktop
  Server->>Desk: capture_primary_X11
  Server->>Client: encoded_frames
  Client-->>Viewer: painted_video
  Viewer->>Client: mouse_or_key_in_widget
  Client->>Client: map_coords_to_host
  Client->>Server: input_event
  Server->>Desk: inject_input
```

```mermaid
stateDiagram-v2
  [*] --> Streaming
  Streaming --> Streaming: frames_and_input
  Streaming --> Degraded: video_lag_input_still_live
  Streaming --> Ended: disconnect_or_error
```

**Happy path:** Live picture of the primary display; move, click, type take effect on the host; input is not stuck behind a slow video queue; widget coordinates are scaled to host resolution.  
**Failures:** Capture fails after auth (permissions, display gone) → error, session ends cleanly. Decode/render fail → error, not a frozen silent window. Keys that cannot map → ignored or logged, session continues.  
**Evidence:** [CLIENT.md](./CLIENT.md) E30–E34  
**Open conflicts:** none (single display, X11)

---

## J3 — Disconnect and reconnect without restarting the server

**Status:** build-ready  
**Goal:** Viewer closes or loses the client; the server keeps listening; the same or another teammate can authenticate again and resume view/control without Jordan restarting the process.

```mermaid
sequenceDiagram
  participant Viewer as Viewer
  participant Client as Client_app
  participant Server as Server_app
  Viewer->>Client: disconnect_or_crash
  Client--xServer: connection_gone
  Server->>Server: end_session_keep_listening
  Viewer->>Client: connect_again_same_host
  Client->>Server: TLS_and_auth
  Server-->>Client: session_ok
```

```mermaid
stateDiagram-v2
  [*] --> InSession
  InSession --> ListeningIdle: client_gone
  ListeningIdle --> InSession: new_auth_ok
  ListeningIdle --> Stopped: operator_stops_server
```

**Happy path:** Clean disconnect or abrupt drop → server returns to listening; new connect with valid credentials streams again (J2). Jordan does not restart anything.  
**Failures:** Server died with the client (bug) → J3 fails the success metric. Stale session blocks the new client → must release on drop. Auth still required on reconnect (no anonymous resume).  
**Evidence:** [CLIENT.md](./CLIENT.md) E40–E41, E7  
**Open conflicts:** none (C2 resolved: J3 is D0)

---

## J4 — Saved connection profiles (D1)

**Status:** draft  
**Goal:** Remember IP/port/username (not necessarily password) so viewers do not retype every time.  
**Evidence:** context client QSettings note  
**Open conflicts:** none

---

## J5 — Wayland host capture and inject (D1)

**Status:** draft  
**Goal:** Server works when `XDG_SESSION_TYPE=wayland` via portal/PipeWire and a permitted inject path.  
**Evidence:** context open questions  
**Open conflicts:** none

---

## J6 — Multi-monitor host (D1)

**Status:** draft  
**Goal:** Operator or viewer selects which host display to capture and where to inject.  
**Evidence:** context open questions  
**Open conflicts:** none

---

## Template for additional journeys

Copy for J7+ when entering D1:

```markdown
## Jn — _

**Status:** draft  
**Goal:** _

sequenceDiagram ...

**Happy path:** _  
**Failures:** _  
**Evidence:** [CLIENT.md](./CLIENT.md) §Jn  
**Open conflicts:** _
```
