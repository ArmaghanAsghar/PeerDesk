# Code map (current → target)

P0 keeps existing API prefixes and panel code paths; UI routes move. Physical folder moves can follow in later PRs (one slice per PR).

## Frontend modules

```mermaid
flowchart LR
  subgraph current [Current production panels]
    ordersCur[production/orders]
    balesCur[production/bales]
    loomCur[production/loom]
    dyeCur[production/dyeing]
  end

  subgraph target [P0 UI routes]
    ordersRoute["/orders"]
    yarnRoute["/yarn"]
    weavingRoute["/weaving"]
    dyeingRoute["/dyeing"]
  end

  ordersCur --> ordersRoute
  balesCur --> yarnRoute
  loomCur --> weavingRoute
  dyeCur --> dyeingRoute
```

| Current path | Target module | P0 UI route | Notes |
|--------------|---------------|-------------|-------|
| `frontend/src/modules/production/orders/` | Core: Buyer Orders | `/orders` | Re-export panel |
| `frontend/src/modules/production/bales/` | Pack: Yarn | `/yarn` | Cotton bales |
| `frontend/src/modules/production/weighbridge/` | Core: Gate | `/gate/weighbridge` | |
| `frontend/src/modules/production/dispatch/` | Core: Gate | `/gate/dispatch` | |
| `frontend/src/modules/production/loom/` | Pack: Weaving | `/weaving` | Loom schedules; WO in P1 |
| `frontend/src/modules/production/dyeing/` | Pack: Dyeing | `/dyeing` | |
| `frontend/src/modules/production/finishing/` | Pack: Dyeing | `/dyeing/finishing` | |
| `frontend/src/modules/production/failed_lots/` | Pack: Dyeing | `/dyeing/failed-lots` | |
| `frontend/src/modules/inventory/` | Core: Inventory | `/inventory` | Stub until Inventory phase |
| `frontend/src/modules/setup/` | Core: Setup | `/setup`, `/setup/color-codes` | Color Codes live; other masters soon |
| `frontend/src/modules/accounting/` | Paid add-on | `/accounting` | Stub |
| `frontend/src/modules/ai/` | Paid add-on | `/ai` | Stub |
| `frontend/src/modules/compliance/` | Fold into Accounting/FBR later | `/compliance` | Stub; keep route for now |
| `frontend/src/modules/production/ProductionPage.tsx` | Deprecated hub | `/production` → redirect | Hash redirects in AppRouter |

## Backend API (current vs target)

Keep current prefixes in P0. Target rename is documentation only until a dedicated API PR.

```mermaid
flowchart LR
  subgraph apiCurrent [Current API]
    boCur["/api/production/buyer-orders"]
    balesApi["/api/production/bales"]
    wbApi["/api/production/weighbridge-tickets"]
    schApi["/api/production/schedules"]
    dyeApi["/api/production/dyeing-orders"]
  end

  subgraph apiTarget [Target later]
    boTgt["/api/orders/buyer-orders"]
    yarnTgt["/api/yarn/bales"]
    gateTgt["/api/gate/weighbridge-tickets"]
    weaveTgt["/api/weaving/schedules"]
    dyeTgt["/api/dyeing/orders"]
  end

  boCur --> boTgt
  balesApi --> yarnTgt
  wbApi --> gateTgt
  schApi --> weaveTgt
  dyeApi --> dyeTgt
```

| Current API | Target (later) | Owner area |
|-------------|----------------|------------|
| `/api/production/buyer-orders` | `/api/orders/buyer-orders` | Orders |
| `/api/production/order-catalogs` | `/api/orders/catalogs` | Orders |
| `/api/production/bales` | `/api/yarn/bales` | Yarn |
| `/api/production/weighbridge-tickets` | `/api/gate/weighbridge-tickets` | Gate |
| `/api/production/dispatch-notes` | `/api/gate/dispatch-notes` | Gate |
| `/api/production/schedules` | `/api/weaving/schedules` | Weaving |
| `/api/production/dyeing-orders` | `/api/dyeing/orders` | Dyeing |
| `/api/production/finishing-cards` | `/api/dyeing/finishing-cards` | Dyeing |
| `/api/production/failed-lots` | `/api/dyeing/failed-lots` | Dyeing |
| `/api/inventory/status` | `/api/inventory/...` | Inventory |
| `/api/accounting/status` | `/api/accounting/...` | Paid add-on |
| `/api/ai/status` | `/api/ai/...` | Paid add-on |
| `/api/setup/color-codes` | `/api/setup/color-codes` | Setup masters |

## Hash → route redirects (P0)

```mermaid
flowchart LR
  prod["/production"] --> orders["/orders"]
  boHash["/production#buyer-orders"] --> orders
  balesHash["/production#cotton-bales"] --> yarn["/yarn"]
  wbHash["/production#weighbridge"] --> gateWb["/gate/weighbridge"]
  weaveHash["/production#weaving"] --> weaving["/weaving"]
  dyeHash["/production#dyeing"] --> dyeing["/dyeing"]
  finHash["/production#finishing"] --> finishing["/dyeing/finishing"]
  failHash["/production#failed-lots"] --> failed["/dyeing/failed-lots"]
  dispHash["/production#dispatch"] --> dispatch["/gate/dispatch"]
```

| Old | New |
|-----|-----|
| `/production` | `/orders` |
| `/production#buyer-orders` | `/orders` |
| `/production#cotton-bales` | `/yarn` |
| `/production#weighbridge` | `/gate/weighbridge` |
| `/production#weaving` | `/weaving` |
| `/production#dyeing` | `/dyeing` |
| `/production#finishing` | `/dyeing/finishing` |
| `/production#failed-lots` | `/dyeing/failed-lots` |
| `/production#dispatch` | `/gate/dispatch` |
