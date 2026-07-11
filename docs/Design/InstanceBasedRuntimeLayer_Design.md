# Instance-Based Runtime Layer — Master Design

**Status:** Design master. Reconciled to current `main` (2026-06-21 gap sweep). Mix of
BUILT (reconciled) and GAP (to build). **Supersedes `InventorySystem_Design.md`** — that doc
predated the discovery that the instance layer already largely exists; this doc absorbs it.
**Scope:** how items / spells / abilities / crystals / evolutions / effects / stats are
referenced vs instanced, how they're granted at runtime, and the specific gaps to close so
loot + shop + progression all work on one coherent foundation.

---

## 1. Core principle — definition vs instance (already the codebase's pattern)

The project already follows the industry-standard split (confirmed in code, not aspirational):

| Layer | What it is | Shared? | Mutable? | Example |
| ----- | ---------- | ------- | -------- | ------- |
| **Definition** | the design-time DataAsset (template) | shared | no | `UWeaponData`, `USpellData`, `UEvolutionItemData`, `UEffectDefinition` |
| **Instance** | one player's concrete owned copy | per-player | yes | `FWeaponInventoryEntry`, `FRingInventoryEntry`, `FEvolutionInventoryEntry` |

**The rule:** *reference the definition, instance the per-player state.* Asset edits propagate
(you reference, never copy the data); only the per-player state (durability, sockets, buffs,
rolls) is instanced. This is the same reference-not-copy discipline used in the dynamic-effect
system (def-identity).

`FWeaponInventoryEntry`'s own header states it verbatim: *"WeaponData = immutable template;
FWeaponInventoryEntry = mutable runtime state. Multiple characters can reference the same
WeaponData but have different crystals attached."* The pattern is **built and correct** — this
doc is about applying it *consistently* and filling the grant/runtime gaps around it.

## 2. Per-type reference-vs-instance table (what each thing IS)

| Type | Carries per-player state? | Storage (runtime home) | Status |
| ---- | ------------------------- | ---------------------- | ------ |
| **Spells** | No — stateless behaviour | `UInventoryComponent::Spells` (`TArray<USpellData*>`, by reference) | BUILT — `LearnSpell` |
| **Abilities** | No | `UInventoryComponent::Abilities` (`TArray<UAbilityData*>`) | BUILT — `LearnAbility` |
| **Weapons** | Yes — crystal, spells, bonuses, durability, (future) buffs | `FWeaponInventoryEntry` | BUILT — `AddWeapon` (mints `PersistentID`) |
| **Rings** | Yes | `FRingInventoryEntry` | BUILT — `AddRing` |
| **Items/Crystals (fungible)** | Quantity only | `UCrystalInventoryComponent` `TMap<FCrystalId,int32>` | BUILT — `AddItemCount` / `AddRefinedCount` |
| **Evolution crystals** | Yes — instance, durability | `FEvolutionInventoryEntry` | BUILT — `AddInstance` |
| **Effects on a weapon** | Yes — bought buffs | *(none yet — see §5 #8)* | **GAP** |
| **Stat points (earned)** | Yes — earned/spent | *(none yet — see §5 #6)* | **GAP** |
| **World-stat levels (earned)** | Yes — runtime deltas | *(none yet — see §5 #7)* | **GAP** |

**Key reconciliation:** equipment instancing — the thing we worried was missing — is **fully
built**. Spells/abilities correctly stay references. The gaps are narrower than feared: effects
on weapons, earned stats, earned world-levels, currency, and the change signal.

## 3. The acquisition model — every avenue calls the same grant methods

Players gain things three ways, and ALL of them route through the same runtime grant surface
(so loot, shop, and progression share one code path per type, not three):

- **Loot** — a drop source holds a *reference* to what it can drop (`USpellData*` /
  `UWeaponData*` / `FCrystalId`), rolls chance/quality (Luck-driven, §7), and on the drop event
  calls the grant method.
- **Shop** — spends currency (§5 #9), then calls the same grant method (or `AppliedBuffs` for
  buying effects onto a weapon, §5 #8).
- **Progression** — level-up / feats grant stat points (§5 #6), world-stat levels (§5 #7), or
  unlock spells via the same `LearnSpell`.

**Grant is not creation.** Loot never creates a new asset — every spell/weapon already exists on
disk, authored once. A grant adds a *reference* (spells/abilities) or creates an *instance*
referencing the asset (equipment/evolution). The asset stays shared; edits propagate.

### Grant surface — what exists today (loot/shop/progression can call now)

| Grant | Method | Result |
| ----- | ------ | ------ |
| Spell | `LearnSpell(USpellData*)` | appends reference (cap 50) |
| Ability | `LearnAbility(UAbilityData*)` | appends reference (cap 50) |
| Weapon | `AddWeapon(UWeaponData*)` | new instance, fresh `PersistentID` — the entry factory always copies the template's default crystal + `DefaultSpells` (the old `bCopyDefaultCrystal` opt-in was removed 2026-07, `feature/hub-merchants`) |
| Ring | `AddRing(URingData*)` | new instance, fresh `PersistentID` — same always-copy factory |
| Item/refined crystal | `AddItemCount` / `AddRefinedCount(FCrystalId, n)` | stacked count |
| Evolution crystal | `AddInstance(UEvolutionItemData*)` | new instance, `FGuid` id |

## 4. Seed-at-creation (BUILT)

Characters seed their inventory from the template once at creation: `UCharacterData::Inventory`
(`UInventoryData*`) is the authoring surface; `UInventoryComponent::InitializeFromCharacterData`
reads it and inflates each `FSavedLoadout` into a runtime `FCombatLoadout`. The game reads the
inventory thereafter. Because seeded entries reference the assets, asset edits still propagate.
This is the seed-references model — and it's **already implemented**, not a gap.

## 5. The confirmed gaps (what to build)

### #6 — Stat-point runtime layer (GAP, scope L, blocked-on: design)
Stats live only on `UCharacterData` (template) + `FEquipmentStatBonus` (gear, read-only at
combat). No earned/spent store. There is inert scaffolding (`FWeaponInventoryEntry` `StatPool`/
`StatMaxPool` — "STORAGE ONLY, nothing reads these"). **Build:** a runtime component/struct
holding earned + allocated stat points, layered on top of the `UCharacterData` base, so
"level-up → gain a point → spend it" persists without mutating the template. Same shape as the
instance pattern: template = base, runtime layer = earned deltas.

### #7 — World-stat level grant (GAP, scope L, blocked-on: design)
`WorldMind/Body/Spirit` (0–7) are template-only (`UCharacterData`); the only writer is a test
fixture. No runtime setter, no XP/level-up loop. **Build:** a runtime grant path (component
field holding earned world-level deltas, or a setter) so Mind 3→4 is possible at runtime.

### #8 — Effects onto a weapon instance / AppliedBuffs (GAP, scope M)
`FWeaponInventoryEntry` has no buff collection — confirmed (no `AppliedBuffs`/`FAppliedBuff`).
The shop's `Price` hook exists on `UEffectDefinition`, and `InstanceID` is "reserved for future
equip/effect wiring." **Build:** add `TArray<TObjectPtr<UEffectDefinition>> AppliedBuffs` (a
reference list — consistent with reference-not-copy) to `FWeaponInventoryEntry` + a grant
method, and have the existing equipment effect-gather include it.
**This rides on the dynamic-effect work already shipped** — the `Get…EffectsGathered` → apply
path with def-identity packing already fires referenced `UEffectDefinition`s. A weapon-instance
`AppliedBuffs` list, once gathered alongside the weapon's other referenced effects, fires *for
free* through that machinery. The only new code is the field + grant + including it in the gather.

### #9 — Currency wallet (GAP, scope M, blocked-on: design)
No currency store anywhere (Prisms / Roll Points / wallet — zero hits). **Build:** a runtime
wallet (component field or small wallet component) holding Prisms + Roll Points, with grant
(earn) and spend (with sufficiency check) methods. The shop reads/spends it; performance/gear
feats grant it (per `Multiplayer_Modes_Design.md` economy).

### #10 — OnInventoryChanged delegate (GAP, scope S, blocked-on: nothing)
`UInventoryComponent` broadcasts nothing on mutation; the `Add*`/`Learn*` methods return `bool`
silently. **Build:** a `DECLARE_DYNAMIC_MULTICAST_DELEGATE` (e.g. `OnInventoryChanged`,
optionally typed by what changed) broadcast from every grant method, so loot/shop/UI react
without polling. **This is the foundation signal** — build it first; everything else broadcasts
on it. (Sibling delegates exist on `ULoadoutComponent`/`UItemExecutor`; mirror their shape.)

## 6. Attach / detach operations (crystals + evolutions)

### Crystal attach (PARTIAL, scope M)
Storage is `FRuntimeAttachedItem AttachedItem` on the weapon/ring entry (`Kind`-discriminated).
**Detach is BUILT** (`RemoveCrystalFromWeapon`/`Ring`). **Attach is a GAP** — `AttachedItem` is
written only at build-time (`FromAttachedItem`); no runtime `AttachCrystal*`. **Build:** a
runtime attach method (socket a crystal into a weapon/ring instance), the socketing-bench
action.

### Evolution attach (GAP, scope M)
Same `AttachedItem` slot, same build-time-only path; no runtime `AttachEvolution`. **Build:** an
evolution attach action.

### Evolution primary-slot weapon-lock (BUILT — implicit)
When an evolution occupies the PRIMARY slot, `PrimarySlotType` becomes `Evolution`;
`FCombatCapabilities` `bCanSwitchWeapon` structurally requires `PrimarySlotType == Weapon`, so
switching is already blocked. **No build needed** — the lock Crown described already exists. (The
stale "Gap 2 — spurious switch on evolution" backlog item is effectively resolved.)

### Evolution removal (GAP, scope M) — design resolved
**There is NO hard "irreversible" rule** — grep-confirmed zero hits. The "permanent" language in
docs refers only to *durability immunity* (evolution crystals don't break), NOT slot-locking.
The only soft framing is "committed character archetypes" (narrative, not mechanical). **Verdict:
removal is a clean gap to build, no contradiction.** **Build:** a `RemoveEvolution`/
`DetachEvolution` method that clears the slot AND **resets `PrimarySlotType` away from
`Evolution`** — critical, or weapon-switching stays locked even after removal. Existing clears
(`ClearBrokenPrimaryEvolution`, the `ClearInvalidSlots` evolution case) are narrow and don't
reset `PrimarySlotType`, so they're not a clean player-unequip.
*(Open sub-decision: is removal free anytime, or only at a between-rounds bench per the
multiplayer socketing design? Lean: bench-gated, matching `Multiplayer_Modes_Design.md`.)*

## 7. Luck-driven loot (designed, unbuilt — cross-reference)
Loot chance/quality is meant to be Luck-driven (`Futurework/Luck_Consumers_Design.md`):
drop-chance, drop-quality, and the world-stat progression hierarchy are designed but unbuilt,
blocked on the loot system. When the loot system is built, it calls the §3 grant surface; Luck
modulates *what* and *how good*, not *how* it's granted.

## 8. Build order (dependency-sorted)

1. **#10 OnInventoryChanged** — tiny, blocks nothing, everything reacts to it. **First.**
2. **#9 Currency wallet** — the shop's prerequisite; standalone, no deps.
3. **#8 AppliedBuffs on weapon** — small, rides on shipped effect machinery; needs #10 for UI.
4. **Crystal attach + Evolution attach + Evolution removal** — the socketing-bench operations;
   related, can cluster.
5. **#6 Stat-point runtime layer** — larger, foundational for progression; needs design pass.
6. **#7 World-stat level grant + XP/level-up loop** — largest; needs the progression design.
7. **Loot system** — drives grants via §3; needs Luck consumers (§7). Built on top of all above.

Persistence (`USaveGame`) is deferred but every runtime store above should be designed
save-ready (plain serializable fields, asset *paths* not raw pointers — `PersistentID` is
already `SaveGame`-tagged for this).

## 9. What this supersedes
This doc absorbs `InventorySystem_Design.md`, which was written before the gap sweep confirmed
the instance layer (`FWeaponInventoryEntry` et al.) already exists. The old doc's "NOT YET BUILT"
framing is incorrect; its design conclusions (definition/instance split, seed-references,
event-driven, stacking-needs-matching-state) were right and are preserved here, reconciled to
the built reality. Retire `InventorySystem_Design.md` (or mark it superseded → this doc).

## 10. Open design questions (resolve before the L-scope builds)
- Stat-point layer (#6): what grants points (XP? feats? both?), is allocation free-respec or
  committed, where does the earned/spent store live (new component vs extend an existing one)?
- World-stat progression (#7): the XP/level curve, the 21-point hierarchy (`Luck_Consumers`),
  what raises a world level.
- Currency (#9): one wallet component vs fields on inventory; how Prisms vs Roll Points are
  earned (the feat definitions).
- Evolution removal (#6 above): free anytime vs bench-gated.
- Loot (§7): drop tables on enemies/encounters; Luck's exact chance/quality formula.
