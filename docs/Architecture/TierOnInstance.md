# Tier-on-Instance

## Overview

**Tier is per-instance, not per-asset.** Every owned weapon, ring, evolution, spell, and
ability carries its **own mutable `EItemTier Tier`** (plus a rolled `EItemQuality Quality`),
seeded from the asset at acquisition and thereafter mutated by leveling. The data asset
(`UWeaponData`, `USpellData`, …) is the immutable **template** — it defines the **floor**
(authored base tier) and all the shared/authored fields; it never changes.

Why: leveling raises *this copy's* power (`Base × TIER_POWER(instanceTier)`), so a leveled
weapon hits harder, costs/wears differently, dismantles for more, and gets bigger loadout
budgets — without authoring 7 assets per item. Loot rolls a `Quality` per drop. Dismantle/
respec read the leveled tier. See [`../Mechanics/Leveling.md`](../Mechanics/Leveling.md) and
[`EconomySystem.md`](./EconomySystem.md).

Distinct from [`PerInstanceRollSystem.md`](./PerInstanceRollSystem.md): that covers the rolled
**stat/resistance** layers (`GeneratedStatBonus`/pools); this covers the **tier + quality** axis.

## Architecture

### Per-type instance home (where the mutable tier lives)

| Type | Instance struct | Tier field | Identity | Owning store |
|---|---|---|---|---|
| Weapon | `FWeaponInventoryEntry` | `.Tier`, `.Quality` | `FGuid PersistentID` | `UInventoryComponent::Weapons` |
| Ring | `FRingInventoryEntry` | `.Tier`, `.Quality` | `FGuid PersistentID` | `UInventoryComponent::Rings` |
| Evolution (owned) | `FEvolutionInventoryEntry` | `.Tier`, `.Quality` | `FGuid InstanceID` | `UEvolutionInventoryComponent::Entries` |
| Evolution (slotted) | `FEvolutionAttachment` | `.Tier` | `FGuid InstanceID` (gear) | the holder's `FRuntimeAttachedItem.Evolution` / `FCombatLoadout::PrimaryEvolution` |
| Spell | `FSpellInstance{ Spell, Tier, Quality, InstanceID }` | `.Tier`, `.Quality` | `FGuid InstanceID` | `FSpellCollection::LearnedSpells` (on `UInventoryComponent::Spells`) |
| Ability | `FAbilityInstance{ Ability, Tier, Quality, InstanceID }` | `.Tier`, `.Quality` | `FGuid InstanceID` | `FAbilityCollection::LearnedAbilities` (on `UInventoryComponent::Abilities`) |

Crystals are the exception: a crystal **is** its `FCrystalId{Type, Tier}` (no asset/instance
split — the tier is intrinsic), so they tier-up by **merging** (`MergeCrystals`), not leveling.

### Seed points (asset → instance)
The instance tier is seeded from the asset at acquisition:
- **Weapons/rings:** `FWeaponInventoryEntry::CreateFromWeapon` / `FRingInventoryEntry::CreateFromRing`
  set `Entry.Tier = Asset->Tier`, `Entry.Quality = C_Quality` (placeholder). Seeded in the **factory**
  (not just `AddWeapon`) so loadout-inflated entries get it too.
- **Evolution:** `UEvolutionInventoryComponent::AddInstance` seeds `Tier = Item->Tier`, `Quality`,
  `CurrentDurability = MaxDurability`, mints `InstanceID`.
- **Spells/abilities:** `FSpellCollection::LearnSpell` / `FAbilityCollection::LearnAbility` mint the
  `InstanceID` and seed `Tier = Asset->Tier`, `Quality = C_Quality` at **learn time** (identity
  originates at ownership).
- **Loot quality:** when the asset opts in (`bRandomGenerateOnPickup`), acquisition rolls
  `Quality = EconomyYield::RollQuality(ownerLuck)` (§11 curve) beside the stat roll; toggle-off
  keeps the `C_Quality` placeholder.

### Reads (everything is instance-tier-aware)
- **Weapon/ability action tier:** `UActionExecutor::ResolveActionTier` reads the active weapon's
  instance entry (`ResolveActiveWeaponEntry → WeaponEntry.Tier`) / the skill's resolved tier.
- **Evolution:** budget/combat/display read the attachment's `.Tier` (primary slot + weapon-attached).
- **Cost / commit-cost / infusion-wear:** read the instance tier at each site.
- **Dismantle:** reads the instance/leveled tier (weapon/ring `Entry.Tier`, evo `Entry.Tier`,
  spell/ability via the resolver) → leveled items dismantle for their current value.
- **Purchase:** reads the **asset** tier (you're buying, not owned yet — no instance exists).

### The spell/ability resolver pattern (asset-keyed)

Unlike weapons/rings/evolution (equipped **by instance** — the loadout carries the `PersistentID`/
`InstanceID`), spells/abilities are equipped **by value**: the loadout and cast path hold bare
`USpellData*`/`UAbilityData*` (`AssignedSpells`, `InnateSpells`, `BDSpellPools`, `EvolutionSpells`,
`FCombatCapabilities`, the command-menu `DataRef`, `Action.SpellData`). The instance `Tier` lives on
the owned `FSpellInstance`/`FAbilityInstance`, which is **never threaded through the equip chain**.

**Key simplifier: duplicates are impossible.** `FSpellCollection::LearnSpell` /
`FAbilityCollection::LearnAbility` reject dups (`HasSpell`/`HasAbility` guards), so an owner holds
**≤1 instance per asset** → the asset is a **unique key** into the owned collection. So tier resolves
**by asset at the read site** — no equip-chain threading needed:

- **`UInventoryComponent::ResolveSpellTier(const AActor* Caster, const USpellData*) → EItemTier`**
  (static) and the ability twin `ResolveAbilityTier(const AActor*, const USkillDataBase*)`: resolve the
  caster's `UInventoryComponent`, look up the owned instance by asset, return its `.Tier`; **asset-fallback**
  (`Spell->Tier`) when not owned (enemies / authored loadouts don't level). Static so both the combat
  executor **and** the AI preview share one path.
- Backed by header-inline **`FSpellCollection::TryGetSpellTier(const USpellData*, EItemTier& Out)`** /
  `FAbilityCollection::TryGetAbilityTier(...)` (pointer-match, no deref) and the mutable
  `FindSpellInstanceMutable`/`FindAbilityInstanceMutable` (leveling writes).

### Saved-side identity — `FSpellRef` + the dormant `InstanceID`
`FSavedLoadout`'s spell arrays are `TArray<FSpellRef>` where `FSpellRef{ USpellData* Spell; FGuid InstanceID; }`
(flat: `InnateSpells`, `EvolutionSpells`, `FResonatorRingSlot::AssignedSpells`; BD pools via the split-out
`FSavedBDElementSpellPool{ Element, TArray<FSpellRef> }`). At inflation
(`FCombatLoadout::CreateFromSavedLoadout`) `FSpellRef::ExtractSpells` / `ToRuntimePool()` copy `.Spell`
**out** into the **bare** runtime arrays — so combat stays bare and the `InstanceID` is carried but
**unresolved**. This is **dormant groundwork**: with duplicates impossible today, resolution is asset-keyed
(above) and the `InstanceID` is unused; the day duplicates are allowed, switch the resolver from asset-keyed
to `InstanceID`-keyed (and thread the ID through the cast path) without re-architecting storage.

### Loadout inflation (asset vs owned-instance)
`FCombatLoadout::CreateFromSavedLoadout` has a context overload: when a slot's instance ref (weapon/ring
`PersistentID`, evolution `PrimaryEvolutionInstance`) is valid **and found** in the owned inventory, the
**owned entry is copied wholesale** (carrying its leveled `.Tier`, quality, rolls, GUID); invalid/unfound
refs fall back to an **asset build** (base tier). For gear-attached evolution, `FRuntimeAttachedItem::FromAttachedItem`
seeds the attachment from the **asset** (base tier, invalid `InstanceID` = authored-locked); the player-attach
op (`AttachEvolutionToWeapon/Ring`) writes the leveled instance tier + a valid `InstanceID`.

## Integration Points

- **Leveling/respec writers:** `UEconomyService::LevelUp*`/`Downgrade*` mutate the instance `.Tier`
  via the mutable accessors (see [`EconomySystem.md`](./EconomySystem.md)).
- **Combat readers:** `UActionExecutor` (action/cost/wear tier), `ULoadoutComponent` (budgets),
  `UCrystalManager` (evolution wear), `UAIDecisionManager` (own-tier preview) — all read instance tier.
- **Resolver home:** `UInventoryComponent::ResolveSpellTier`/`ResolveAbilityTier` (static).
- **Quality:** `EconomyYield::RollQuality` at the `bRandomGenerateOnPickup` mint points.

## How to test
- Level a weapon (`LevelUpWeapon`) → its `FWeaponInventoryEntry.Tier` rises; confirm combat
  damage/cost/wear + dismantle yield all reflect the new tier (PIE + Print Wallet for the spend/yield).
- Learn a spell, `LevelUpSpell` → cast it: `ResolveSpellTier(caster, spell)` returns the leveled tier;
  an enemy casting the same asset reads the asset tier (asset-fallback).
- `InventoryDebug` dumps owned spells/abilities with their per-instance tier.

## Known Limitations / TODOs
- **⚠️ `FSavedLoadout` re-save caveat:** the spell arrays' type changed (`TArray<USpellData*>` →
  `TArray<FSpellRef>` / `FSavedBDElementSpellPool`). UE tagged-property serialization can't auto-convert
  the old inner type, so **existing authored loadout assets load those spell arrays EMPTY** until
  re-authored/re-saved in-editor. Acceptable pre-release (no shipped saves); **must re-save authored
  loadouts** before relying on their spell lists.
- **Instance tier is run-scoped today.** Leveling writes the run inventory; persistence across runs
  awaits the Pool arc.
- **`SkillDataBase::GetTierString`** (display) is owner-less → shows the **asset** base tier, not the
  caster's instance tier. Instance-aware display needs the owner threaded to the call (deferred).
- **Asset-keyed resolution assumes no duplicates.** Correct today (learn guards). If duplicate
  spells/abilities are ever allowed, upgrade the resolver to `InstanceID`-keyed (the `FSpellRef.InstanceID`
  groundwork is in place) + thread the ID through the cast path.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-06-24 | Initial doc — the per-instance tier/quality axis across weapon/ring/evolution/spell/ability, the asset-keyed spell resolver, `FSpellRef` saved-side pairing + dormant `InstanceID`, seed points, inflation, and the re-save caveat. | feature/currency-component |
