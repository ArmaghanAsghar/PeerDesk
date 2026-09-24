# Requirements checklist

**Live LoomLogic brain** (`.cursor/brain`). Tick as interview fills gaps.  
Do **not** re-propose stack (already locked) unless human asks.

## Problem & scope

- [x] North star + audience — [PRODUCT.md](./PRODUCT.md)
- [x] Success metric (first useful version) — complete ops ERP **minus** Analytics + LoomSage (2026-08-27)
- [x] Pains + current workaround — PRODUCT
- [x] Out of scope (v1) — mobile native; hosted multi-tenant SaaS; Analytics/LoomSage as later add-ons
- [x] Assumptions / risks noted — PRODUCT / CLIENT conflicts

## Users

- [x] User model (solo / team / multi-tenant) — on-prem single mill; roles — PRODUCT / DECISIONS
- [x] Personas with role cards — [PERSONAS.md](./PERSONAS.md) (roles provisional C-J0-1)

## Functional (D0)

- [x] Journey list + D0 details — [FLOWS.md](./FLOWS.md) J0–J3 persona-ready
- [x] Acceptance stories for D0 journeys — [STORIES.md](./STORIES.md)
- [x] Provisional modules — [MODULES.md](./MODULES.md)
- [ ] D1 journeys J4–J9 still mostly `draft` — expand later (needed for “complete ERP” success metric)

## Platforms & deploy

- [x] Devices day one — PRODUCT — **browser only** (2026-08-27)
- [x] Deploy / distribution target — PRODUCT + DECISIONS — **on-prem single mill** (2026-08-27)

## Non-functionals

- [x] NFR scale — **≤50 users**; backup — **automatic scheduled** (no manual routine intervention); security/a11y/compliance noted (2026-08-27)

## Stack locked

- [x] Architecture profile + concrete starter locked — React / FastAPI / Postgres RLS (on-prem one org)
- [x] RULES / stack-contracts step done — `.cursor/rules/stack-contracts.mdc` (2026-08-27)

## Evidence (optional)

- [x] CLIENT inbox process understood — [CLIENT.md](./CLIENT.md)
- [x] Open conflicts **parked** — C-J0-1 roles, C-J0-2 StoreKeeper nav stay provisional until mill meeting (2026-08-27)

## Still missing

- J4–J9 journey depth (D1) — required for “complete ERP” success metric; not blocking structure
- Mill meeting to clear C-J0-1 / C-J0-2 before J0 `client-validated` / `build-ready`

## Training status

**ERP reference instance** — ships with Brainiac as a filled brain for agents to learn ERP/mill/factory patterns. Not live Cursor memory (that remains `.cursor/brain/`); not “ready to replace `.cursor`”.

| Field | Value |
|-------|--------|
| Role | ERP **reference** instance (learn patterns; do not treat as live LoomLogic memory) |
| Seeded from | Live `.cursor/brain/` (refresh overwrite) |
| Path | `brainiac/instances/loomlogic/` |
| Deploy / devices | On-prem single mill; browser only |
| Success / scale | Complete ops ERP minus Analytics/LoomSage; ≤50 users |
| Backup | Automatic scheduled — no manual routine intervention |
| CLIENT conflicts | Parked until mill meeting |
| Ready to replace `.cursor`? | **No** — reference copy only |
| RULES / stack-contracts | Mirror live brain; apply via kit when scaffolding new projects |
