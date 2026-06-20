# Inventory System — runtime per-character ownership layer

**Status:** Design locked (design + research-validated), NOT YET BUILT. No code.
**Touches (when built):** new `UInventoryComponent`, new `FItemInstance` struct, new
`UInventoryItemDefinition` (or reuse existing `UWeaponData`/`URingData`/`UEvolutionItemData`
as the definition layer), `CombatOrchestrator` (seed at creation), the loadout/equip path.
**Depends on:** nothing new — sits on the existing data-asset + def-identity foundation.
**Unblocks:** the shop (`AppliedBuffs` purchases), progression, loot, the multiplayer gear draft.

## 1. Problem

Characters currently carry only their **authored template defaults** (the
`DefaultSpells`/`DefaultAbilities` on the character/equipment templates, plus starting gear).
There is no per-character **runtime** record of "what *this specific* character has acquired" —
no owned-items list, no bought skills, no per-item applied buffs, no currency, no socket state.

"What a character has" == "what their template authored." That's fine for a fixed demo fight,
but the moment there is a shop, progression, or loot, there must be somewhere to store
*acquisitions* that is **distinct from the immutable template**. The shop especially is
meaningless without it — "buy a buff" means "add to *my* runtime data," and there is nowhere
to add to yet.

## 2. Core model — definition vs instance (the industry-standard split)

Two layers, mirroring the same reference-not-copy discipline used throughout the effect system:

| Layer          | What it is                              | Shared? | Mutable? |
| -------------- | --------------------------------------- | ------- | -------- |
| **Definition** | the design-time template (the DataAsset) | shared  | no (design-time only) |
| **Instance**   | one player's concrete copy               | per-player | yes (runtime state) |

- **Definition** = the existing data assets (`UWeaponData`, `URingData`, `UEvolutionItemData`,
  skill assets, `UEffectDefinition`). Immutable, shared across every character that uses them.
- **Instance** = a lightweight runtime struct holding a **reference** to the definition plus
  **per-player state**:

  ```
  FItemInstance {
      TObjectPtr<UEquipmentDataBase> Definition;   // reference — NOT a data copy
      TArray<FAppliedBuff>            AppliedBuffs;  // shop-stamped effect snapshots (frozen)
      TArray<FCrystalSocket>          Sockets;       // socketed crystals
      float                           Durability;    // per-instance wear
      // (ItemLevel / rolled substats / tags as needed)
  }
  ```

**The rule:** *reference the definition, instance the runtime state.* A character's sword entry =
"→ Greatsword asset (reference, edits propagate) + {its durability, its applied buffs, its
sockets} (per-player instance data)."

This is the canonical pattern — every shipped RPG inventory does this (definition DataAsset +
runtime instance referencing it + instance-specific fields like durability/enchantments). It is
*not* a full data copy: editing the Greatsword definition still propagates to every instance,
because instances hold a **pointer**, not a baked copy of the stats.

## 3. The seed-at-creation model (the chosen ownership shape)

Rather than the game merging "template defaults + inventory additions" at every read, the
inventory is **seeded from the template once at character creation** and is the sole read source
thereafter.

- At creation, the character's authored starting set (default skills, starting gear) is placed
  **into the inventory as references** — not as data copies.
- After creation, the game reads **only the inventory**. Defaults and acquisitions live in one
  list, indistinguishable — both are references.
- Because the seeded entries are **references**, editing a definition (buff Slash, retune the
  Greatsword) **still propagates** to every character holding it. This keeps the Option-A win
  (edits reach everyone) while getting Option-B simplicity (one place to look).

### Why this over the alternatives

| Approach | Stores | Edits propagate? | Read complexity |
| -------- | ------ | ---------------- | --------------- |
| Merge-on-demand (additive) | only acquisitions; merge template + inventory at read | yes | merge every read |
| **Seed references (chosen)** | references seeded from template + acquisitions, one list | **yes** (references resolve live) | single read |
| Full copy (rejected) | full copy of definition DATA into the save | **no** (copied data is frozen) | single read |

Full copy is the rejected one — it bakes definition *data* into each character, so balance edits
never reach existing characters (the live-link trap rejected everywhere else this project).
Seed-references avoids it precisely because it copies the **reference**, not the data.

### Known consequence (acceptable)

A default **added to the template later** is NOT auto-granted to **already-created** characters
(their inventory was seeded before that default existed). New characters get it. If "every
existing character instantly gains this new default" is ever wanted, re-run the seed for them.
For the demo this is a non-issue.

## 4. What the inventory holds

| Bucket | Nature | Notes |
| ------ | ------ | ----- |
| Owned items | `TArray<FItemInstance>` | weapons/rings/evolution crystals owned (not just equipped) |
| Equipped state | slot → instance ref | equipped = a constrained sub-inventory; same `FItemInstance` structure |
| Applied buffs | on each `FItemInstance` | the shop's `AppliedBuffs` snapshot — copied `UEffectDefinition` values, frozen per-player |
| Granted skills | `TArray<ref>` | abilities/spells beyond the template defaults (seeded defaults + acquisitions, all references) |
| Currency | Prisms / Roll Points | pure runtime; no template default |
| Socket state | on each `FItemInstance` | which crystals socketed where (the evolution/socketing bench) |

Equipment slots are just a smaller, constrained inventory (each slot accepts certain item types)
sharing the same `FItemInstance` structure — no separate "equipped item" type.

## 5. Where it lives — `UInventoryComponent`

A `UActorComponent` on the character (matches the project rule: *runtime state = component*).
Holds the buckets above as `UPROPERTY`s. Found via `GetOwner()->FindComponentByClass<>()`,
cached once.

**Event-driven, not polled:** the component exposes an `OnInventoryChanged` dynamic multicast
delegate (added/removed/equipped/loaded). UI and dependent systems subscribe; nothing polls the
inventory per-tick. (Consistent with the project's delegate-over-polling preference.)

## 6. Saving (DEFERRED — design note only, not building now)

The component is the **live runtime** face; persistence is a **separate, later** layer. The
pattern that makes it painless if the component is designed save-ready from day one:

- Keep the component's data **plain and serializable** — structs, asset **paths / soft-pointers**
  (NOT raw runtime `UObject*` that don't survive a reload), per-instance state as plain fields.
- A save operation serializes those into a `USaveGame` (or JSON) → disk; load reads them back
  into the component.
- Item references serialize as **asset paths**, which is exactly why the reference model survives
  reload cleanly — you store "→ /Game/.../Greatsword", not the Greatsword's data.

If the component is built this way, adding saving later is "write a SaveGame wrapper," not a
refactor. **Not built now** — flagged so the component is designed compatibly.

## 7. Stacking nuance (bake in from the start)

Two instances of the same definition **stack/merge only if their per-instance state matches.**
An "Epic Sword +10 STR" does NOT stack with an "Epic Sword +12 STR" — different rolled state,
different stack. This is the **same identity logic as the effect-system def-identity merge**
(same identity + same state → merge; different state → separate). Worth encoding in `FItemInstance`
equality from day one rather than retrofitting.

## 8. Scope — demo vs MMO vision

The pitch demo doesn't need full persistent/networked inventory, but the **shape** should be
MMO-compatible (the long-term vision). So:

- **Build (demo):** `UInventoryComponent`, `FItemInstance`, seed-at-creation, owned/equipped,
  `AppliedBuffs`, currency, sockets, the change delegate.
- **Defer:** cross-session save (`USaveGame`), server-authoritative sync. For multiplayer, the
  standard is server-authoritative (client requests an action → server validates → applies),
  matching the project's "player-host authority for v1" multiplayer design — deferred to when
  multiplayer inventory is actually wired.

## 9. Dependency chain (for the shop)

The shop sits on top of this; it cannot exist without it:

1. **Runtime per-character inventory** (THIS DOC) — the foundation. Stores acquisitions distinct
   from the template.
2. **Shop** — reads currency, writes purchases into the inventory: `AppliedBuffs` for effects (the
   snapshot copy onto an `FItemInstance` — `Price` already lives on `UEffectDefinition`), granted
   skills, owned items.
3. **Economy** — Prisms (performance feats → buff-layer shop) + Roll Points (gear feats → reroll
   substats), already designed in `Multiplayer_Modes_Design.md`.

The effect/skill *capability* already exists (effects merge by definition; weapons grant skills
via `DefaultSpells`/`DefaultAbilities`). What's missing — and what this doc specifies — is the
**runtime storage for "what a player acquired"** that the shop writes into.

## 10. Open questions (resolve before build)

- Definition layer: reuse the existing `UWeaponData`/`URingData`/`UEvolutionItemData` as the
  item definitions directly, or introduce a thin `UInventoryItemDefinition` wrapper? (Lean: reuse
  existing — they already are the definitions.)
- `FItemInstance` identity/equality fields for the §7 stacking rule — which per-instance fields
  count toward "same instance"?
- Granted-skills representation — a flat reference list on the component, or do skills also become
  `FItemInstance`-like (if skills can ever carry per-instance state)? (Lean: flat references —
  skills don't carry per-player state the way gear does.)
- Currency: on the inventory component, or a separate economy component? (Lean: on the inventory
  for the demo; split if the economy grows.)
