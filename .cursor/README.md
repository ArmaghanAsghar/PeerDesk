# Brainiac

Portable **product brain** for any software project. Agents act as a **requirements engineer**: interview for product details, deploy targets, NFRs, and user stories — then suggest one deploy-aligned tech stack — before feature code.

Domain-agnostic blank stubs in `brain/`. Includes an **ERP reference** filled brain at [`instances/loomlogic/`](./instances/loomlogic/) (learn patterns; always interview).

## Install into a fresh repo

From the Brainiac kit root (this folder, or a clone of the kit repo):

```bash
mkdir -p .cursor
cp -R brain/. .cursor/brain/
cp DISCOVERY.md ARCHITECTURE_DEFAULTS.md ERP_REFERENCE.md .cursor/brain/
cp -R rules/. .cursor/rules/
# Optional — keep ERP reference next to the kit or copy for agents:
# cp -R instances .cursor/brainiac-instances
```

For a normal (non-ERP) app, only `brain/` + `rules/` + DISCOVERY/ARCHITECTURE_DEFAULTS are required. Keep `instances/loomlogic/` available in the kit repo (or clone) so ERP requests can load the reference.

**Target layout after install:**

```text
.cursor/
  brain/
    DISCOVERY.md
    ARCHITECTURE_DEFAULTS.md
    ERP_REFERENCE.md          # if copied
    REQUIREMENTS.md
    PRODUCT.md … GLOSSARY.md
  rules/
    project-core.mdc
    mermaid-plans.mdc
    stack-contracts.mdc
```

Open Cursor and say: **“Run discovery”**, **“Gather requirements”**, or **“Make an ERP.”**

- Small-app example: [examples/solo-habit-tracker.md](./examples/solo-habit-tracker.md)
- ERP path: [ERP_REFERENCE.md](./ERP_REFERENCE.md) + [instances/loomlogic/](./instances/loomlogic/)

## Solo-dev / RE promise (brain behavior stories)

| ID | Story |
|----|--------|
| B-S1 | Agent notices an empty brain and **starts discovery** — you never face a blank PRODUCT alone. |
| B-S2 | Questions are **plain language**, 1–2 at a time (who, what device, alone vs others). |
| B-S3 | After requirements + stories, agent **proposes one architecture**; you say yes or tweak one thing. |
| B-S4 | Agent drafts **personas + journeys**; you correct instead of inventing from scratch. |
| B-S5 | **User stories** are written journey-by-journey so build order is clear. |
| B-S6 | **CLIENT inbox** accepts messy notes; agent ingests into structured evidence. |
| B-S7 | Feature code is **blocked** until a journey is `build-ready` (or you waive). |
| B-S8 | Setup is **copy + “run discovery”** — minutes, not a workshop. |
| B-S9 | Stack is proposed **after** D0 stories (unless you ask to suggest stack early). |
| B-S10 | **ERP request** → learn from `instances/loomlogic/`, ask questions, rename journeys for your domain. |

Full playbook: [DISCOVERY.md](./DISCOVERY.md). Completeness: brain [REQUIREMENTS.md](./brain/REQUIREMENTS.md). Stack maps: [ARCHITECTURE_DEFAULTS.md](./ARCHITECTURE_DEFAULTS.md).

## Journey statuses

`draft` → `persona-ready` → `client-validated` → `build-ready`

Solo developers may self-mark `client-validated` after a walkthrough.

## Kit layout

| Path | Role |
|------|------|
| `brain/` | Blank stubs to copy into `.cursor/brain/` |
| `instances/loomlogic/` | **ERP reference** filled brain (patterns only) |
| `ERP_REFERENCE.md` | Playbook when user says “make an ERP” |
| `examples/` | Tiny non-ERP filled snippets |
| `rules/` | Cursor rules for new repos |

## What’s in the brain (stubs)

| File | Role |
|------|------|
| REQUIREMENTS.md | RE completeness checklist (index) |
| PRODUCT.md | North star, devices, deploy, pains, NFRs |
| PERSONAS.md | Role cards (Job / Sees / Does / Does not) |
| FLOWS.md | Journeys + status gate (D0 wave first) |
| STORIES.md | Acceptance stories (build gate) |
| CLIENT.md | Notes / interview evidence + ingest |
| MODULES.md | Core vs optional (provisional) |
| PHASES.md | P0 → D0 → D1 → build-by-journey |
| DECISIONS.md | Locked product + deploy + NFRs + architecture |
| CODEMAP.md | Code paths → modules (fill when code exists) |
| GLOSSARY.md | Domain + process terms |

## Rules

- `rules/project-core.mdc` — RE role; discovery-first; ERP path B-S10; journey build gate
- `rules/mermaid-plans.mdc` — plans lead with Mermaid diagrams
- `rules/stack-contracts.mdc` — after architecture lock: no silent fallbacks; no fake CRUD

## Not included

- No app code scaffolding.
- Blank `brain/` stubs stay domain-agnostic; textile detail lives only under `instances/loomlogic/` as reference.
