# Hubs — shops & services (two-hub economy)

**Status:** ⚠️ **[DESIGN — not built; depends on the Pool arc].** This is the locked design for the
two-hub shop/service layer. **None of it exists in code** — the hubs, shops, draft/return, and repair
are part of the **next major foundational arc (the Pool/persistence layer)**. The backend *ops* the
hubs will call (purchase / upgrade / downgrade / merge / dismantle) are built today as authority-gated
`UEconomyService` functions ([`Economy.md`](./Economy.md)); the hubs are the **UI + structure layer +
the pool** that drive them. Source-of-truth: `Resources_Design.md` two-hub section (§ "Two-hub
economy", §5.3b).

## The two hubs

| | **Local hub** (persistent / between-runs) | **Run hub** (in-run) |
|---|---|---|
| **When** | between runs, at your home base | during a run, while assembling for the challenge |
| **Role** | **lean restock + respec** | **the active workshop** — build while you prep |
| **Buy currency** | **Prisms** (persistent) | **Gold** (run-volatile) |
| **Does** | purchase · downgrade | purchase · upgrade · repair · merge · roll/reroll · downgrade · draft · return |

The **local hub is deliberately lean** — just restock (purchase) and respec (downgrade). The **run hub
is the active workshop**: almost everything happens *there*, while you assemble your loadout for the
committed challenge.

## Composition — which service in which hub (LOCKED)

| Service | Local hub | Run hub | Cost (carries its own, regardless of hub) |
|---|---|---|---|
| **Purchase** | ✅ local-flagged (Prisms) | ✅ run-flagged (Gold) | currency differs **by hub**; stock = per-item flag |
| **Upgrade** (item tier-up) | — | ✅ **run only** | upgrade essence + ½ Reality (NOT Gold) — see [`Upgrading.md`](./Upgrading.md) |
| **Repair** (fix broken gear) | — | ✅ **run only** | cost TBD |
| **Merge** (crystals) | — | ✅ | Prisms — see [`Merging.md`](./Merging.md) |
| **Roll / reroll** (stats) | — | ✅ | Reality + Prisms — see [`Gear/RerollEconomy.md`](../Gear/RerollEconomy.md) |
| **Downgrade** (respec) | ✅ | ✅ **both** | partial essence refund (½ step, no Reality) — see [`Upgrading.md`](./Upgrading.md) |
| **Draft / Return** | — | ✅ **run only** | Gold (return surplus → pool + Gold) |

> *Note:* the design doc carries two composition tables — an earlier "everything is in BOTH" draft and
> the **later LOCKED** "run hub = active workshop, local hub = lean" version. **This table reflects the
> LOCKED version**: Repair, Merge, and Roll/reroll are **run-hub-only**, alongside Upgrade and
> Draft/Return; only **Purchase** (both, by-hub currency) and **Downgrade** (both) appear locally.

## The principle — run hub builds, local hub restocks

The **run hub is where you power up**, *while assembling for the challenge* (upgrade / repair / merge
/ roll / buy). **Upgrade is run-only** — you build power **where you use it**, and (because upgrading
is immediate + permanent — [`Upgrading.md`](./Upgrading.md)) it persists out of the run. The **local
hub is lean**: purchase (restock) + downgrade (respec). **Downgrade is in both** — respec is a
maintenance action, available wherever you are. This resolves "upgrade is immediate **and** permanent":
all active power-shaping happens **in the run**, and sticks.

## Item-to-shop tag

Each gear / skill / item carries a **shop flag — `local-hub` / `run-hub` / `both`**. The **same item
type can appear in both shops at different currencies** (Prisms locally, Gold in-run) per its flag.
A shop's stock is **flag-filtered rolled stock** — the local and run shops are the *same* rolled-stock
shop system (the §11 shop-roll), parameterized by currency + which flags they stock. The roll uses the
same per-instance machinery as drops ([`Quality.md`](../Gear/Quality.md), [`Gear/PerInstanceRolls.md`](../Gear/PerInstanceRolls.md)).

## Currency follows the action, not the location

The run hub is **multi-currency** — **currency follows what you're *doing*, not where you are**:

- **Gold** to purchase (run gear),
- **upgrade essence + ½ Reality** to upgrade,
- **Prisms** to merge,
- **Reality + Prisms** to roll/reroll.

**Prisms is a carried wallet, not location-locked** — so Prisms *is* spent in the run hub, just for
**services** (merge / roll), never for buying gear (that's Gold). The **local hub is Prisms-only**:
purchase + the essence refund on downgrade. *(Design friction flagged in the source: merge/roll
spending Prisms inside the run hub is coherent under "currency follows the action," but is marked to
revisit if it should change.)* See [`Currency.md`](./Currency.md) for the tokens themselves.

## The run-hub loop — draft → build → return → shop

1. **Draft** — you're offered **~3 inventories drawn from your own pool** → **pick 1** → build your
   loadout from it. The draft is a curated **slice**, not your whole pool ("take 10, get to the end" —
   you may own 100 of everything; a run takes ~10). It gives enough for a full loadout, but **loadout
   slots cap** what you keep.
   - *Themed inventories:* the 3 options are normally random, but meeting a hand-designed themed
     inventory's requirements gives a **chance** to be offered it — a discovery/reward mechanic.
     *(Themed definitions + requirements are a deferred design+content task.)*
2. **Build** — assemble + power up your loadout in the run hub (upgrade / repair / merge / roll / buy).
3. **Return** — **surplus beyond your slots → back to the pool for Gold.** You still **own** it ("I'll
   use it a different run") — **not a loss**: you keep the unlock and get Gold now. That Gold funds the
   run shop. *(Return-for-Gold is distinct from break-for-essence — a voluntary bank, no essence.)*

> **Draft / pool / return are the Pool arc.** None of this loop exists yet — it lands with the
> persistent Pool layer (`UPoolSubsystem` + save + run-state + the draw/return hooks). The pool must
> support the **draft** (pool→run), the **return valve** (run→pool + Gold), and **both shop types**.
> Repair and the break→pool-return half land with it too. See `Resources_Design.md` (Pool-arc plan).

## Known Limitations / TODOs
- **⚠️ Entirely unbuilt** — no hubs, shops, NPCs, draft, return, or repair exist in code. The
  `UEconomyService` ops are the only built piece.
- **Repair is a new mechanic** — NPC + cost, design-only (lands with the Pool arc).
- **Repair cost TBD**; themed-inventory definitions deferred.
- **Currency-follows-action friction** (Prisms spent in the run hub for merge/roll) flagged to revisit.
- **Account-vs-character routing** for Prisms/Diamond is still unwired (see [`Currency.md`](./Currency.md)).

## Related
- [`Currency.md`](./Currency.md) — the tokens (Gold / Prisms / essence / Reality).
- [`Economy.md`](./Economy.md) — the earn→spend loop the hubs surface.
- [`Upgrading.md`](./Upgrading.md) — upgrade + downgrade detail · [`Merging.md`](./Merging.md) — crystal merge · [`Quality.md`](../Gear/Quality.md) — rolled shop/drop quality.
- [`Dismantle.md`](./Dismantle.md) — the scrap faucet (a hub/menu action).
- [`../../Architecture/EconomySystem.md`](../../Architecture/EconomySystem.md) — the backend ops the hubs call.
- `Resources_Design.md` — the locked two-hub spec + the Pool-arc plan (source-of-truth).
