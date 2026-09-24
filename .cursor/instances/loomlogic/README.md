# LoomLogic — ERP reference instance

**Role:** Filled Brainiac brain used as the **reference project** when a user asks to build an ERP / mill / factory / ops system.

**Do not** treat this folder as the live Cursor memory for LoomLogic (that is `.cursor/brain/`). This copy ships with the Brainiac kit so agents can learn patterns.

## What to learn

- Journey status gate: draft → persona-ready → client-validated → build-ready
- PRODUCT depth: north star, pains, deploy, NFRs, success metric, out of scope
- MODULES shape: Core vs process packs vs paid add-ons (defer Analytics/AI)
- CLIENT ingest + conflict parking until client meeting
- On-prem single-org + browser-only + automatic backup patterns (example locks)

## What NOT to do

- Do **not** copy textile entities (Yarn, Weave, Dye, bales, etc.) into a new ERP unless the user is building a textile mill product.
- Do **not** skip DISCOVERY interview — always ask 1–2 questions at a time (see kit `ERP_REFERENCE.md` + `DISCOVERY.md`).
- Rename journeys for the user’s domain (login → setup → order → inventory/ops…).

## Kit entrypoints

- [ERP_REFERENCE.md](../../ERP_REFERENCE.md) — when user says “make an ERP”
- [DISCOVERY.md](../../DISCOVERY.md) — B-S10 ERP path
- [REQUIREMENTS.md](./REQUIREMENTS.md) — completeness checklist for this reference
