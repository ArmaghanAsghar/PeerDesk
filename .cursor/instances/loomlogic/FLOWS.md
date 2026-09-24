# Flows — integrated golden path

**Profile:** `integrated` · Yarn → Weave → Dye/Finish → QC → Pack → Dispatch.  
**Statuses:** `draft` → `persona-ready` → `client-validated` → `build-ready`.  
Feature PRs need **`build-ready`** or an explicit human **persona-ready waiver** ([STORIES.md](./STORIES.md), [docs/SDLC.md](../../docs/SDLC.md)).

```mermaid
flowchart LR
  J0 --> J1 --> J2 --> J3 --> J4 --> J5 --> J6 --> J7 --> J8 --> J9
```

| ID | Title | Status | Primary personas |
|----|-------|--------|------------------|
| J0 | Login, roles, who can see what | **persona-ready** | Admin, all |
| J1 | Setup masters enough to run a day | **persona-ready** | Admin, StoreKeeper, Sales |
| J2 | Buyer order → CONFIRMED | **persona-ready** | Sales, Admin |
| J3 | Yarn receive / bale lots | **persona-ready** | StoreKeeper |
| J4 | Weaving DRAFT WO → production | draft | FloorSupervisor, Operator |
| J5 | Dye / finish / failed lots | draft | FloorSupervisor, Operator |
| J6 | QC grade + lock | draft | FloorSupervisor, StoreKeeper |
| J7 | Pack + dispatch | draft | StoreKeeper, Sales |
| J8 | Stock truth / shrinkage | draft | StoreKeeper |
| J9 | Boundary pass (Core / packs / add-ons) | draft | Admin |

Do **not** start J4+ feature code until J0–J3 are at least `persona-ready` (met) and preferably `client-validated` / `build-ready`.

---

## J0 — Login, roles, visibility

**Status:** persona-ready _(partial owner evidence 2026-08-27; roles + nav await mill — not yet `client-validated`)_  
**Goal:** Right people reach the right nav; locked/must-change-password work; unpaid add-ons show **Upgrade to Pro+** (no fake CRUD); Admin can revoke all sessions.

```mermaid
sequenceDiagram
  participant U as User
  participant UI as Frontend
  participant API as AuthAPI
  participant DB as Postgres_RLS

  U->>UI: open /login
  U->>UI: email plus password
  UI->>API: POST /api/auth/login
  API->>DB: set tenant then lookup
  alt locked or bad creds
    API-->>UI: 401 or 429
  else must_change_password
    API-->>UI: JWT plus flag
    UI->>UI: force change-password
    UI->>API: change password ge12 plus uppercase
    API->>API: revoke_all_sessions
    UI->>UI: land_on_Home
  else ok
    API-->>UI: JWT plus role
    UI->>UI: nav from entitlements plus role
  end
```

```mermaid
stateDiagram-v2
  [*] --> Guest
  Guest --> Active: login_ok
  Guest --> MustChange: login_must_change
  MustChange --> Active: password_changed_revoke_sessions
  Active --> Locked: brute_force
  Locked --> Active: unlock_or_expiry
  Active --> Guest: logout
  Active --> Guest: admin_sign_out_everywhere
```

**Happy path:** Admin creates users → user logs in → must-change if flagged → Home → sees integrated nav for role.  
**Failures:** Wrong password; lockout; peer Admin cannot delete peer Admin; weak password (<12 or no uppercase).  
**Evidence:** [CLIENT.md](./CLIENT.md) §J0 (J0-E1…E10). **Open:** C-J0-1 roles, C-J0-2 StoreKeeper nav.

---

## J1 — Setup masters for a mill day

**Status:** persona-ready  
**Goal:** Minimum masters so J2–J3 can run: parties (buyer, yarn supplier), articles (yarn count / fabric construction), UoM, machines/looms (light).

```mermaid
sequenceDiagram
  participant Admin
  participant Setup as SetupUI
  participant API as SetupAPI

  Admin->>Setup: create Party buyer
  Admin->>Setup: create Party yarn_supplier
  Admin->>Setup: create Article yarn_and_greige
  Admin->>Setup: create UoM kg_m_bale
  Admin->>Setup: register looms_or_machines_light
  Setup->>API: persist masters
  API-->>Setup: ids ready for Orders_Yarn
```

**Happy path:** Masters exist before first BPO and first weighbridge ticket.  
**Out of scope here:** Full BOM trees; bin locations (later Inventory).  
**Evidence:** CLIENT §J1.

---

## J2 — Buyer order → CONFIRMED

**Status:** persona-ready  
**Goal:** Sales creates BPO; on `CONFIRMED`, system creates **DRAFT** work docs only for **enabled** packs (weaving/dyeing/yarn as entitled)—humans confirm later (J4+).

```mermaid
stateDiagram-v2
  [*] --> Draft: Sales_creates
  Draft --> Confirmed: Sales_confirms
  Confirmed --> InProgress: packs_consume
  InProgress --> Closed: fully_dispatched
  Draft --> Cancelled: void
  Confirmed --> Cancelled: void_with_rules
```

```mermaid
sequenceDiagram
  participant Sales
  participant Orders as OrdersUI
  participant API as OrdersAPI
  participant Packs as EnabledPacks

  Sales->>Orders: create BPO lines
  Sales->>Orders: CONFIRMED
  Orders->>API: confirm
  API->>Packs: create DRAFT work docs
  Packs-->>API: draft ids
  API-->>Orders: BPO confirmed plus drafts
```

**Happy path:** Confirmed BPO shows linked DRAFT WOs/job docs; rollup stays on Orders.  
**Rules:** No auto-start of production; Inventory not posted on confirm.  
**Evidence:** CLIENT §J2.

---

## J3 — Yarn receive / bale lots

**Status:** persona-ready  
**Goal:** Truck at gate → weighbridge ticket → yarn/bale lots in Yarn pack; Inventory **proposal** then StoreKeeper **post** (or single post if ticket is the receive).

```mermaid
sequenceDiagram
  participant SK as StoreKeeper
  participant Gate as GateWeighbridge
  participant Yarn as YarnPack
  participant Inv as Inventory

  SK->>Gate: create weighbridge ticket
  Gate->>Gate: capture gross_tare_net
  SK->>Yarn: create bale_or_lot from ticket
  Yarn->>Inv: propose inbound stock
  SK->>Inv: post inbound
  Inv-->>SK: stock available for issue
```

**Happy path:** Net weight and supplier party link to lots; lots visible on `/yarn`.  
**Edge:** Job-work inbound (converter-like) deferred; integrated path is own purchase/receive first.  
**Evidence:** CLIENT §J3.

---

## J4 — Weaving DRAFT WO → production (draft)

**Status:** draft  
**Goal:** FloorSupervisor confirms DRAFT weaving WO; Operator/Supervisor records greige output; Yarn issue proposed → Inventory posts.

```mermaid
sequenceDiagram
  participant FS as FloorSupervisor
  participant Weave as WeavingPack
  participant Inv as Inventory

  FS->>Weave: confirm DRAFT WO
  FS->>Weave: assign looms_job_cards
  Weave->>Inv: propose yarn_issue
  FS->>Inv: post issue
  FS->>Weave: record greige_produced
  Weave->>Inv: propose greige_receipt
  FS->>Inv: post receipt
```

---

## J5 — Dye / finish / failed lots (draft)

**Status:** draft  
**Goal:** Confirm dyeing/finishing DRAFTs; process greige; open failed lots; propose yield/shrinkage.

```mermaid
sequenceDiagram
  participant FS as FloorSupervisor
  participant Dye as DyeingPack
  participant Inv as Inventory

  FS->>Dye: confirm DRAFT process docs
  FS->>Dye: run dye_and_finish
  alt quality_fail
    FS->>Dye: open failed_lot
  else ok
    Dye->>Inv: propose finished_goods
    FS->>Inv: post
  end
```

---

## J6 — QC grade + lock (draft)

**Status:** draft  
**Goal:** Shared QC grades fabric; grade_lock on Inventory; Gate cannot dispatch locked stock.

```mermaid
sequenceDiagram
  participant QC as SharedQC
  participant Inv as Inventory
  participant Gate as GateDispatch

  QC->>QC: 4point_or_grade
  QC->>Inv: grade_lock
  Gate->>Inv: check lock before dispatch
  alt locked
    Inv-->>Gate: block
  else clear
    Inv-->>Gate: allow
  end
```

---

## J7 — Pack + dispatch (draft)

**Status:** draft  
**Goal:** Pack finished goods; Dispatch note; weighbridge out; BPO progress updates.

```mermaid
sequenceDiagram
  participant SK as StoreKeeper
  participant Gate as GateDispatch
  participant Orders as BuyerOrders
  participant Inv as Inventory

  SK->>Gate: create pack_list
  SK->>Inv: post pack_issue
  SK->>Gate: create dispatch_note
  Gate->>Inv: verify unlocked stock
  Gate->>Orders: update delivery_progress
```

---

## J8 — Stock truth / shrinkage (draft)

**Status:** draft  
**Goal:** Inventory is system of record; multi-UOM; shrinkage/yield from packs posted, not double-counted in packs.

```mermaid
flowchart LR
  PackPropose[Pack_proposes_move] --> InvPost[Inventory_posts]
  InvPost --> Truth[Stock_truth]
  Truth --> Reports[Core_ops_views]
```

---

## J9 — Boundary pass (draft → revised with discovery)

**Status:** draft (working conclusions in [MODULES.md](./MODULES.md) / [DECISIONS.md](./DECISIONS.md) §Boundary)  
**Goal:** Lock what is Core vs process pack vs Analytics vs LoomSage after J0–J8 shape is clear.

See **Boundary revision (J9)** in DECISIONS — provisional answers from golden-path analysis; client may override.
