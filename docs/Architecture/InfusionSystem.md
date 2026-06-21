# Infusion System

## Overview

**Infusion** is the charge mechanic: holding an action before release powers it up
at one of two **charge levels** — L1 (casual, `LEVEL_1_CHARGE_TIME` 0.5s hold) or
L2 (all-in, `LEVEL_2_CHARGE_TIME` 1.5s) — in exchange for a cost. An infused action
carries an **element** (or stays elementless for Raw), pays a cost along one of three
axes, and applies a charge **effect**.

There are **four axes**:

- **Cost — EP** (energy reservoir): the action's base EP × a charge multiplier × a stat surcharge.
- **Cost — HP** (life reservoir): for HP-paying sources, an EP-derived bite out of health.
- **Cost — durability** (crystal reservoir): crystal-backed sources pay wear instead of EP. See `CrystalWear.md` / `DurabilityWear.md`.
- **Effect**: a per-mode, stat-scaled damage **and** status-buildup bonus.

The core principle is **"whose reservoir powers it"** — the chosen infusion *source*
decides which reservoir pays (the caster's EP/HP, or an equipped crystal's durability)
and which element/mode applies. The engine lives in `UActionExecutor`
(`UGameInstanceSubsystem`); pure cost math is in `UInfusionCostHelper`
(`UBlueprintFunctionLibrary`); tunables are in `InfusionConstants.h`.

## Source binding

The infusion source is `EInfusionSourceOption` — `None`, `Raw`, `Innate`, `ActiveRing`,
`PrimaryRing`, `WeaponCrystal`, `Evolution`.

**Spells are 1:1 origin-bound.** A spell's allowed source is fixed by where the spell
comes from. `UActionExecutor::GetAllowedInfusionSourcesForSpell(Actor, Spell)` is the
single source of truth: it resolves the spell's origin via
`ULoadoutComponent::ResolveSpellSource` (an `ESpellSource`) and maps it 1:1:

| Spell origin (`ESpellSource`) | Allowed infusion source |
|---|---|
| `Innate` | `Innate` |
| `RingCrystal` | `ActiveRing` (Resonator) / `PrimaryRing` (Generic/Caster) |
| `WeaponCrystal` | `WeaponCrystal` |
| `Evolution` | `Evolution` — **plus `Innate` if the caster is BD or Reality** (the exception) |
| `Item` / unresolvable | *(empty — not infusable)* |

**Abilities are NOT origin-bound** — the source is chosen, not fixed. The player cycles
it; the AI picks via heuristic (see *AI infusion*).

**Enforcement (6-5-f).** The binding is enforced in two places that share the one
`GetAllowedInfusionSourcesForSpell` definition, so the UI can never offer what the
validator would reject:
- `UActionExecutor::ValidateAction` (the `Spell` case) rejects an infused spell
  (`GetChargeLevel() > 0`, real `SelectedSource`) whose source is not in the allowed set.
- The command-menu source cache (`UCombatCommandMenuSubsystem`) restricts the
  Breakthrough cycle list to the binding. A plain Caster's evolution spell → `{Evolution}`
  (size 1 → cycle button auto-hides); a BD/Reality caster → `{Evolution, Innate}`.

## Generic Spell Resolution (the Generic-inherit feature)

A **Generic** spell (`USpellData::Element == ESpellElement::Generic`) is a *polymorphic
template* — a basic spell that adopts the element of the source it is slotted into. After
the `ESpellElement` rework, `Generic` means **"inherit the source element at cast"**;
`None` is the dedicated non-elemental sentinel (physical-ness lives on the separate
`EPhysicalDamageType` axis). A fixed-element spell keeps its authored element unchanged.

**`UActionExecutor::ResolveSpellCastElement(Caster, Spell)`** is the resolver:

1. **Non-Generic → passthrough.** `Element != Generic` (incl. `None`) returns the authored
   element verbatim.
2. **Broken Darkness → pool walk.** If the caster `IsBrokenDarkness()`, the element comes
   from the **pool the spell occupies**, NOT the caster's innate (Darkness): `InnateSpells`
   membership → `Darkness`; a `BDSpellPools[i]` membership → `Pool.Element`. (A dedicated
   walk, because `ResolveSpellSource` collapses every BD pool to `Innate` and loses the pool.)
3. **Otherwise → origin chain.** `GetAllowedInfusionSourcesForSpell` (above) → the first
   origin source → `GetElementForSourceOption` → that element. Innate→`InnateElement`,
   ring/weapon→crystal element, evolution→evolution element. A **Reality** source yields
   `Reality` (valid — Reality casts Generic spells; the resolver does not reject it).
4. **Unresolvable → `None` + warning.** No slottable origin, or the origin carries no element.

The source used is the spell's **origin** (where it is slotted, via `ResolveSpellSource`),
**not** a player infusion pick — so a slotted Generic spell resolves with no explicit
infusion selection.

**Wired at the cast boundary** (`UActionExecutor::ExecuteSpellAsync`): resolved **once**,
then fed to `AttackElement` (damage/resistance), the forbidden-cast self-damage, the defense
window, the deferred VFX colours (muzzle + Support/AOE/Instant, via a `PendingResolvedElement`
cache mirroring `bPendingSpellIsBrokenDarkness`), and the projectile tint (incl. burst
continuations, via a threaded param + `ActiveBurstElement`). A still-`Generic` result (the
`Generic`-innate misconfiguration edge) is converted to `None` + logged, so `Generic` never
reaches the damage/colour/resistance pipeline. There are **zero raw `Spell->Element` reads**
left in that pipeline.

**Slot validation — Generic is a wildcard.** `ElementHelpers::SpellElementMatchesHost(Spell,
Host)` is the one-place element-match rule: `true` for `Generic` (wildcard), any-source hosts
(`Reality`/`BrokenDarkness`), or an exact match. It backs the four host-element gates (ring /
weapon findings in `LoadoutComponent`, `FWeaponLoadoutEntry`/`FRingLoadoutEntry::IsValid`) and
the BD-pool authoring gates (`FCombatLoadout::ValidateBDSpellLoadout`). Separately,
`IsElementCastable` short-circuits `Generic` to always-castable (resolution is the real gate).
Concrete-element spells in a wrong host are still rejected.

**Naming.** `USpellData::GetDisplayName(ResolvedElement, bIsBrokenDarkness)` prefixes a Generic
spell with its resolved element: `"[Element] [Name]"` ("Fire Ball"); BD → `"Dark [Element]
[Name]"` except Darkness (`"Darkness [Name]"`, no doubling); Reality → `"Reality [Name]"`;
unresolvable/fixed-element → raw `Name`. Wired into the combat spell-menu button label + tint.

See `docs/Design/Completed/GenericSpellInherit.md` (full arc) and `docs/Mechanics/GenericSpells.md`
(player-facing). Debug: `UGenericSpellResolveDebug`.

## Cost model

### EP (energy)

`UActionExecutor::ComputeInfusionCostMultiplier(Level, bIsSpell, Comp)`:

```
multiplier = ChargeMult(Level) × (1 + StatFraction(stat))
```

- **Charge multiplier** — L1 = ×1.5 (+50%), L2 = ×2.0 (+100%). Identical for spells
  (`SPELL_L1/L2_ENERGY_MULT`) and abilities (`L1/L2_ENERGY_MULT`); kept as separate named
  constants but equal in value. (rework 6-2-1)
- **Stat surcharge** (6-2-2) — `StatFraction(stat) = max(0, stat − 1)`, upside-only: a
  below-neutral stat never discounts the cost. Spell scales with `SpellDamage`, ability with
  `GetEffectiveRawDamage()`. Stacking the damage stat raises both the *effect* and the *cost*
  by the identical fraction — self-balancing. `StatFraction` is one shared helper used by both
  cost and effect, so they never drift.
- Final spent EP = `base EP × multiplier × Efficiency`. **Efficiency mitigates EP only.**

**Crystal-zero-EP (6-2-2).** Crystal-backed sources — `ActiveRing`, `PrimaryRing`,
`WeaponCrystal` — pay **0 EP**; they're powered by the crystal's reservoir and pay
**durability** wear instead (`CrystalWear.md`).

### HP (life)

`UInfusionCostHelper::CalculateHPCost(Actor, PreEffInfusedEP)` (6-2-3):

```
HP = round( (PreEffInfusedEP / MaxEP) × MaxHP × max(0, 1 − Resistance) )
```

- `PreEffInfusedEP` is the **pre-Efficiency** infused EP (`base EP × ComputeInfusionCostMultiplier`,
  *without* the Efficiency multiplier) — so **Efficiency mitigates EP, Resistance mitigates HP**
  (clean, non-overlapping channels).
- **Not floored — infusion can kill the caster.** The cost is deducted at
  `FinalizeAsyncAction`, *after* the infused effect resolves, so a lethal cost lands only once
  the action has fired. `WouldKill` compares the same unrounded basis against `CurrentHP`.

**Which sources pay HP:** `Raw` (always), `Innate` **on a spell**, and `Evolution`.
Innate **on an ability** pays no HP (6-2-4). Crystal sources pay durability, not HP.

### Per-source cost summary

| Source | EP | HP | Durability |
|---|---|---|---|
| `Raw` | yes | **yes** | — |
| `Innate` (ability) | yes | no (6-2-4) | — |
| `Innate` (spell) | yes | **yes** | — |
| `ActiveRing` / `PrimaryRing` / `WeaponCrystal` | **0** | no | **yes** |
| `Evolution` | yes | **yes** | yes (evolution crystal) |

## Effect model

The charge effect is a multiplier on both **damage** and **status buildup**, routed by an
**`EInfusionMode`** declared on the equipment/character that owns the source:
`Physical`, `Status`, or `Balanced` (the default on all weapon/ring/evolution/character data).

Two value bands, each L1/L2 (multiplier form, `InfusionConstants.h`):

| Band | L1 | L2 |
|---|---|---|
| **Focus** (`CHARGE_FOCUS_*`) | 1.15 | 1.30 |
| **Off-focus** (`CHARGE_OFFFOCUS_*`) | 1.10 | 1.20 |
| **Balanced** (`CHARGE_BALANCED_*`) | 1.125 | 1.25 |

The mode routes which band goes to which channel:

| Mode | Damage | Status |
|---|---|---|
| `Physical` | Focus | Off-focus |
| `Status` | Off-focus | Focus |
| `Balanced` | Balanced | Balanced |

Both channels scale at **both** levels — L1 now grants a damage bonus too (progressive),
replacing the old exclusive "L1 = status only / L2 = damage only" model.

Each band is then **stat-scaled** by the same `(1 + StatFraction(stat))` surcharge as the
cost: damage uses `SpellDamage` (spell) / `GetEffectiveRawDamage()` (ability), status uses
`GetEffectiveStatusMultiplier()`.

Functions (`UActionExecutor`, all pure const):
- `GetChargeDamageMultiplier(Level, Mode, bIsSpell, Comp)` — L0 (or null `Comp`) → 1.0.
- `GetChargeStatusMultiplier(Level, Mode, Comp)` — L0 (or null `Comp`) → 1.0.
- `ResolveInfusionMode(Source, Actor)` — which equipment's `InfusionMode` applies:
  `Raw`/`WeaponCrystal` → weapon, `Innate` → character data, `ActiveRing`/`PrimaryRing` → ring,
  `Evolution` → primary-slot evolution. Null-safe → `Balanced`.

(rework 6-3 / 6-4)

## AI infusion

`UAIDecisionManager` (Medium+ difficulty; **Easy never infuses**):

- **Source selection (6-5-b).** Spells consume `GetAllowedInfusionSourcesForSpell` via
  `DecideSpellInfusionSource` (first allowed, Evolution preferred for the BD/Reality two-source
  case; empty → don't infuse). Abilities use `DecideAbilityInfusionSource` — Caster → `Innate`,
  Resonator → `ActiveRing`, else first available crystal source, else `Raw`.
- **HP-affordability guard (6-5-b).** `ClampInfusionLevelForHP` gates HP-paying sources
  (`Raw` / `Innate`-on-spell / `Evolution`) through `WouldKill` on the exact pre-Efficiency
  infused EP, dropping L2 → L1 → L0 until survivable — the AI prefers a weaker infusion over
  self-death. Crystal sources skip the guard.
- **Prediction parity (6-5-d).** The AI's damage/status estimates call the same charge getters
  the executor uses (mode-aware, stat-scaled), so its scoring matches what execution applies.
  The source is a pure function of (attacker, skill), so the deciders resolve it and thread it
  into the estimators — no candidate reordering.

See `AISystem.md`.

## Debug

- `GetInfusionModeString(EInfusionMode)` — inline string for an infusion mode (`EInfusionMode.h`).
- `GetBreakabilityString(EBreakability)` — crystal breakability inspection (`EBreakability.h`).
- Charge multipliers/HP cost are pure const queries (`GetChargeDamageMultiplier`,
  `GetChargeStatusMultiplier`, `ComputeInfusionCostMultiplier`, `UInfusionCostHelper::CalculateHPCost`
  / `WouldKill`) — callable for inspection without launching the action.

## Known Limitations / TODOs

1. **AI scores flat at L0.** The AI ranks skills uninfused, then picks a source + charge level
   for the winner — it does not model (source × level) as distinct candidates. Full per-mode AI
   fidelity (comparing "raw L2" vs "ring L1") is a deferred, deeper change.
2. **SHIPPED — BD representation refactor + enum-value deletion.** Broken Darkness is now a
   `bool` toggle (`bBrokenDarknessInnate`) with `InnateElement = Darkness` — the former
   `ESpellElement::BrokenDarkness` enum value has been **deleted** (`feature/bd-value-deletion`,
   PIE-verified). The `ULoadoutComponent` BD-pool loop now bounds on the `None` sentinel
   (`i < (uint8)None`); the dead `PostLoad` migration was removed and the single BD asset
   re-saved. See `docs/Architecture/BrokenDarkness.md`.

## Cross-links

- `AISystem.md` — AI source selection, the HP-affordability guard, prediction parity.
- `CrystalWear.md` / `DurabilityWear.md` — the durability cost axis (wear formula, breakability).
- `CombatOrchestrator.md` — infusion HP cost is lethal and paid at finalize.
- `SkillEffectSystem.md` — source-driven element resolution and infusion DOT factories.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-06-18 | Initial documentation of the infusion-charge rework (6-1..6-5): source binding (`GetAllowedInfusionSourcesForSpell`, 1:1 spell origin + BD/Reality evolution exception, enforced in ValidateAction + UI); cost model (EP ×1.5/×2.0 + upside-only stat surcharge, crystal-zero-EP, EP-derived lethal HP with Resistance mitigation, innate-ability-free); effect model (`EInfusionMode` per-mode stat-scaled damage+status bands, progressive L1); AI infusion (source selection, HP-affordability guard, prediction parity). | feature/realtime-defense |
| 2026-06-21 | New *Generic Spell Resolution* section — `ResolveSpellCastElement` (Generic → source/pool element: passthrough / BD-pool walk / origin chain / unresolvable→None), cast-boundary wiring (resolve-once + `PendingResolvedElement` cache + projectile thread-through + still-Generic→None safety net), `SpellElementMatchesHost` wildcard gates + `IsElementCastable` Generic short-circuit, and `GetDisplayName` naming. `Generic` now means "inherit at cast"; `None` is the non-elemental sentinel (two-axis with `EPhysicalDamageType`). Reality casts Generic spells ("Reality Ball" — not rejected). | feature/generic-spell-inherit |
