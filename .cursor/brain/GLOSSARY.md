# Glossary

> Add terms as they appear in discovery. Prefer client language over invented jargon.

## Process terms

| Term | Meaning | Notes |
|------|---------|-------|
| Journey status | `draft` → `persona-ready` → `client-validated` → `build-ready` | Solo may self-mark `client-validated` |
| D0 / D1 | Discovery waves | First 2–4 journeys, then the rest |
| Waiver | Human allow of feature work before `build-ready` | Log in STORIES.md |
| RE / requirements engineer | Agent role for discovery | Stories before stack (B-S9) |
| Deploy target | Where the finished app lives for users | Hosted web, stores, installers, on-prem, local-only |
| NFR | Non-functional requirement | Scale, security, backup, a11y, compliance, builder limits |

## Domain terms

| Term | Meaning | Notes |
|------|---------|-------|
| Client | App on the viewer’s Mac or Ubuntu machine | Shows remote picture; sends mouse/keyboard |
| Server | App on the Ubuntu machine being viewed | Captures screen; injects input; authenticates |
| Host | The Ubuntu computer that runs the server | Also “the machine you control” |
| Viewer | Person connecting from another computer | Same human may also be the host operator |
| Host operator | Person who starts the server and owns host logins | Sets username/password on that machine |
| Session | Authenticated connection from one client to one host | v1: one active session per host |
| Control channel | Small messages: hello, auth, keepalive, errors, negotiate | Not the video bytes |
| Data channel | Length-prefixed frames: video, mouse, key, ping | Same TCP/TLS connection, logical split |
| Capture | Grabbing pixels of the host desktop | X11 path in v1 |
| Encode / decode | Compress frames (H.264) then unpack on the client | Low-latency presets, not archival quality |
| Input injection | Making the host OS see fake mouse/keyboard events | As if someone sat at that desk |
| Coordinate mapping | Scale a click in the client window to host screen pixels | Required when sizes differ |
| Challenge-response | Server sends a nonce; client proves it knows the password without sending the password | See auth NFR |
| Reconnect | Client drops, then connects again; server still running | D0 / J3 — not “saved profiles” |
| Saved profile | Remembered IP/user/port in the client UI | D1 — not the same as reconnect |
| PeerDesk | Shipped product name | Repo folder may still say TeamViewer |
| Synthetic capture | Server paints a test canvas instead of X11 | Explicit `--synthetic`; demo only |
| Relay / rendezvous | Extra cloud server so you can connect through NAT | Out of scope for v1 |
| X11 | Older Linux desktop session type | v1 capture/inject target |
| Wayland | Newer Linux desktop session type | D1 — different capture APIs |
| LAN/VPN reachability | Client can already open TCP to host IP:port | Assumed; no TeamViewer-style “anywhere” |
