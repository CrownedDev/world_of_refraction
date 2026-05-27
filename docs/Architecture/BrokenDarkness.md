# Broken Darkness System

## Overview

Broken Darkness (BD) is a Caster variant: a character who has "broken" under the strain of
casting beyond their stat requirements. A BD no longer regenerates normal energy — they
fuel spells by absorbing elemental energy from attacks they parry or block, build
absorption stacks that scale status effects, can overload when energy exceeds capacity,
and cast with a darkness-tinted visual treatment. The system is implemented around one
runtime component, `UBrokenDarknessManager`, plus a state flag on `UCharacterDataComponent`.

> This document reflects the BD system after **Session 5** (`feature/bd-ai-awareness`).
> Sessions 1–2 wired BD energy and the Caster element gate into action validation;
> Session 3 added the BD spell-pool loadout; Session 4 made AI BD-aware; Session 5
> unified BD absorption energy onto `UCharacterDataComponent::CurrentEP` — see
> *Energy Model*.

## Two Paths to Broken Darkness

A character is behaviourally BD via one of two routes:

- **Character-created** — the `UCharacterData` asset has `InnateElement == ESpellElement::BrokenDarkness`.
  `UCharacterDataComponent` detects this on init and auto-flips `bIsBrokenDarkness` + zeroes
  `CurrentEP` (`CharacterDataComponent.cpp:61-66`). `UBrokenDarknessManager::BeginPlay` mirrors
  it onto `bIsTransformed` so the manager's methods don't short-circuit (`BrokenDarknessManager.cpp:95-106`).
- **Runtime-transformed** — a non-BD character passes a break roll mid-combat;
  `RollForBreak` → `TriggerTransformation` sets `bIsTransformed` and calls
  `ServerSetBrokenDarkness(true)` (`BrokenDarknessManager.cpp:194-231`). The `UCharacterData`
  asset is **not** mutated — `InnateElement` keeps its original value (e.g. `Darkness`).

`UCharacterDataComponent::IsBrokenDarkness()` unifies both: it returns true if
`bIsBrokenDarkness` is set **or** the asset's `InnateElement == BrokenDarkness`
(`CharacterDataComponent.cpp:279-290`). All BD-aware code is expected to call this helper
rather than reading either field directly (`CharacterDataComponent.h:69, 233-242`).

## State Model

| State | Type / location | Represents | Written by | Read by |
|---|---|---|---|---|
| `bIsBrokenDarkness` | `bool`, `UCharacterDataComponent` (`.h:75-76`), `SaveGame` + `Replicated` | Runtime "is BD" flag | `CharacterDataComponent.cpp:64` (char-created), `ServerSetBrokenDarkness` (`.cpp:292-309`) | `IsBrokenDarkness()` only — never read directly |
| `bIsTransformed` | `bool`, `UBrokenDarknessManager` (`.h:285`) | Manager-local "is BD" flag; gates every absorption/overload method | `BeginPlay` (`.cpp:101`), `TriggerTransformation` (`.cpp:201`) | `IsTransformed()`, internal guards |
| `InnateElement` | `ESpellElement`, `UCharacterData` asset | Immutable innate element; `BrokenDarkness` marks a character-created BD | Asset author | `IsBrokenDarkness()`, break-roll Darkness gate, visuals |
| `AbsorbedElements` | `TArray<ESpellElement>`, `UBrokenDarknessManager` (`.h:309`) | Distinct elements absorbed this session | `RecordAbsorbedElement` (`.cpp:383-385`) | `HasAbsorbedElement` (`.cpp:583`) |
| `LastAbsorbedElement` | `ESpellElement`, `UBrokenDarknessManager` (`.h:313`) | Most recent absorbed element; drives visuals | `ProcessElementAbsorption` (`.cpp:572`) | `GetHybridElement()` |

`bIsBrokenDarkness` is `SaveGame`-tagged for future persistence but session-only today —
no save system exists (`CharacterDataComponent.h:72-73`).

## Break-Roll System (current behaviour after Session 0)

`UActionExecutor::CheckBrokenDarknessBreak` (`ActionExecutor.cpp:3196`) is the single
break-roll entry point. It is called from `ExecuteAction` (`:319`) and `ExecuteActionAsync`
(`:451`). `UBrokenDarknessManager::RollForBreak` (`BrokenDarknessManager.cpp:111`) is the
only function that can roll, and `CheckBrokenDarknessBreak` is its only caller.

**Gates** (all must pass, in order — `ActionExecutor.cpp:3198-3214`):
1. Actor has a `UBrokenDarknessManager` component.
2. `!BDManager->IsTransformed()` — already-BD characters never re-roll.
3. `CharData` valid and `InnateElement == ESpellElement::Darkness` — only innate-Darkness
   characters can break. (Added Session 0.)

**Triggers:**
- **Spell** (`ActionExecutor.cpp:3217-3243`) — rolls if the spell exceeds stat requirements
  (`DoesSpellExceedRequirements`, `BrokenDarknessManager.cpp:163`) **OR** is infused at L1/L2.
- **Ability** (`ActionExecutor.cpp:3244-3270`) — rolls only if **all** hold: the ability is
  infused (`SelectedSource != None`), the infusion source resolves to the character's innate
  element via `GetElementForSourceOption` (i.e. Darkness), **and** the ability exceeds stat
  requirements (`DoesAbilityExceedRequirements`, `BrokenDarknessManager.cpp:174`).
- All other action types do not roll.

**Chance** — `RollForBreak` computes `BaseChance × InfusionMultiplier`
(`BrokenDarknessManager.cpp:118-120`); roll succeeds if `FRand() < Chance`.

Tier base chances (`BrokenDarknessConstants`, `BrokenDarknessManager.cpp:18-24`):

| Tier | S | A | B | C | D | E | F |
|---|---|---|---|---|---|---|---|
| Base break chance | 1.5% | 1.0% | 0.6% | 0.3% | 0.1% | 0% | 0% |

Infusion multipliers (`BrokenDarknessManager.cpp:25-26`, `GetInfusionMultiplier` `:62-70`):
**L0 = 1.0×, L1 = 1.5×, L2 = 2.0×**. E/F tier (chance 0) short-circuits before the roll
(`BrokenDarknessManager.cpp:125-131`).

On success `RollForBreak` calls `TriggerTransformation` (`.cpp:146`).
`ForceTransformation` (`.cpp:185`) is a guaranteed, gate-free transform — it exists but has
zero callers.

## Absorption System

When transformed, a BD gains absorption energy from successful defends. The live path is
`OnDefenseResolved` (`BrokenDarknessManager.cpp:601`), called by `ActionExecutor.cpp:1236`
when a defense window closes:

- Only `Block` and `Parry` absorb — `Dodge` and failed defenses do not (`.cpp:609-627`).
- The attack element must be absorbable — `CanAbsorbElement` excludes `Generic`, `Reality`,
  and `BrokenDarkness` (`.cpp:240-251`).
- Energy gained = `AttackEnergyCost × mult`, mult = `PARRY_ABSORPTION_MULT` (0.30) or
  `BLOCK_ABSORPTION_MULT` (0.15) (`CalculateAbsorptionEnergy`, `.cpp:656-673`).

`OnSuccessfulParry` / `OnSuccessfulBlock` (`.cpp:288, 314`) are alternative public entry
points using a different formula (`DamageBlocked × ParryAbsorptionRate/BlockAbsorptionRate`);
they currently have no callers.

**Energy** — since Session 5, BD absorption energy is stored on
`UCharacterDataComponent::CurrentEP` — the same field a non-BD caster spends. BD's gain
rule survives: passive regen is suppressed (`ServerGainEnergy` BD early-out), and the
event-driven gain path is `CharacterDataComponent::ServerGainBrokenDarknessEnergy`, which
bypasses that early-out and permits `CurrentEP` to exceed `MaxEP` into overload.
`AddAbsorptionEnergy` routes defense and crystal absorption through it; the ceiling is
`MaxEP + GetOverloadCapacity()`, where `GetOverloadCapacity()` is derived as **30% of
`MaxEP`** (`OVERLOAD_CAPACITY_FRACTION`) — no stored field, so the overload buffer scales
with the BD's energy pool. Crossing `MaxEP` enters **overload** — `UpdateOverloadState`
is driven by a binding to `CharacterDataComponent::OnEPChanged` (`BeginPlay`), the single
overload trigger, so absorption gain, cast spend, and overload drain all re-evaluate it.
`ProcessOverloadTick` (called by `CombatOrchestrator.cpp`) applies aura damage to nearby
enemies, self-damage, and energy drain (via `ServerSpendEnergy`) each turn while overloaded.

**Stacks & alignment** — `ProcessElementAbsorption` (`.cpp:529`) tracks `CurrentAlignmentElement`.
Absorbing the same element consecutively raises `CurrentAbsorptionStacks` (max 3); absorbing
a different element resets stacks and re-aligns. `GetStackStatusMultiplier` returns
1.0 / 1.0 / 2.0 / 4.0 for stacks 0-3 (`.cpp:513-527`), consumed by
`UDamageCalculator::GetBDStackStatusMultiplier` (`DamageCalculator.cpp:375`).

## Energy Model

A single energy pool backs spellcasting for every character. Whether anything is charged
at all depends on `FAction::SpellSource`.

**Pool** — BD and non-BD casters alike spend `UCharacterDataComponent::CurrentEP`
(unified in Session 5). `UActionExecutor::ValidateAction` and `SpendEnergy` compare and
debit `CurrentEP` directly — no BD energy branch. The only difference between a BD and a
non-BD caster is the *gain* rule: a non-BD regenerates `CurrentEP` passively; a BD does
not (`ServerGainEnergy` BD early-out) and instead gains it event-driven via absorption.
The overload threshold is therefore the BD's stat-derived `MaxEP`, not a flat cap.

**Cost by spell source** — `CalculateActionEnergyCost` reads `SpellSource`
(`ActionExecutor.cpp:269-273`):

| `ESpellSource` | Non-BD cost | BD cost |
|---|---|---|
| `Innate` | Full EP | Full EP |
| `Evolution` (primary-slot) | Full EP + HP backlash + self-status | **0 EP** — durability wear is the cost. *Except* `SpellInfusionLevel ≥ 1` with `SelectedSource == Innate` (Darkness conversion): pays normal EP **plus** wear. |
| `RingCrystal` | **0** EP | **0** EP |
| `WeaponCrystal` | **0** EP | **0** EP |

A ring- or weapon-attached evolution routes through `RingCrystal` / `WeaponCrystal`, so it
is free; only a *primary-slot* evolution routes through `Evolution`. A free spell
(cost 0) trivially passes the energy gate — a BD with empty `CurrentEP` can still cast
ring/weapon spells.

**Wear-as-cost (BD evolution).** For a BD, the standalone primary-slot evolution is
free at the EP layer; the cost is paid in durability wear via
`UCrystalManager::ProcessPostCastEvolutionWear`, hooked from
`UActionExecutor::ExecuteSpellAsync` after `SpendEnergy` succeeds. The substat wear
formula self-handles zero-wear cases (matched-tier uninfused = no wear), and the
per-asset `bCanBreak` gate is bypassed via the new
`FEvolutionAttachment::ApplyWear(_, bForceWear=true)` flag — BD's mechanic is
intrinsic, so per-asset opt-in would silently fail and contradict BD's identity. The
Innate-source carve-out (above) captures *absorbed energy converting the spell's
element* — the only case where BD's evolution cast still pays EP, **on top of**
wear. Full formula + constants: `CrystalWear.md`.

## Element Access

Casting an element is gated for the Caster class only (Generic and Resonator have no
element gate). The check is the shared static predicate
`UBrokenDarknessManager::IsElementCastable(Actor, CharComp, BDManager, Element)`
(`BrokenDarknessManager.cpp:590`), called by both `ActionExecutor::ValidateAction`
(`ActionExecutor.cpp:204`) and `LoadoutComponent::GetValidationErrors`
(`LoadoutComponent.cpp:527`) so combat and loadout validation never disagree.

An element is castable when **any** of these hold:

- **Non-BD** — the element matches `InnateElement`; or `InnateElement` is itself an
  any-element source (`Reality` / `BrokenDarkness`, via `ElementHelpers::IsAnySpellSource`);
  or an equipped crystal channels the element.
- **BD** — the element is `Darkness` (the BD default); or it was absorbed this session
  (`HasAbsorbedElement`); or an equipped crystal channels the element.

The equipment channel is `ULoadoutComponent::HasEquippedSourceForElement(Actor, Element)`
(`LoadoutComponent.cpp:1202`): it walks every slot from `GetEquippedCrystals()` — weapon
and ring crystals plus the primary evolution slot — and returns true if any crystal's
`GetAssociatedElement()` matches. A Fire Caster with a Water ring crystal can therefore
cast Water spells.

`IsElementCastable` returns true (castable) whenever the character cannot be resolved, so
missing data never blocks a cast. Separately, `ValidateAction` still unlocks *all* elements
when the selected infusion **source** is itself an any-element source — the source-side
`bAnyElement` check (`ActionExecutor.cpp:196-198`), distinct from the equipment channel above.

## BD Spell Pools

A Broken Darkness character's spell loadout is split into a Darkness pool plus seven
per-element pools.

- **Darkness pool** — `FCombatLoadout::InnateSpells` (the existing Caster field). Max 6
  spells, every entry must be `Darkness` element. Always castable.
- **Element pools** — `FCombatLoadout::BDSpellPools`, a `TArray<FBDElementSpellPool>`
  (`FCombatLoadout.h`). Seven pools — `Fire`, `Water`, `Earth`, `Wind`, `Light`,
  `Lightning`, `Void` — each holding up to 6 spells that must match the pool's element.
  `Reality` is excluded: it cannot be absorbed (`CanAbsorbElement`), so it is never a pool.

`ULoadoutComponent::InitializeBDPools` builds the seven empty pools — one per absorbable
non-Darkness element — and is idempotent (authored pools survive, missing ones are added).
`ApplyBDPoolsIfBroken` runs it when the owning character is `IsBrokenDarkness()`, at
loadout creation (via `InitializeFromCharacterData`, including the empty-loadout
soft-fail path). Non-BD characters get no pools.

**Validation** — `FCombatLoadout::ValidateBDSpellLoadout` (static, shared between
runtime `FCombatLoadout` and the inline-asset `FSavedLoadout`) enforces the
structural rules: Darkness pool ≤ 6 and all-Darkness; ≤ 7 element pools; each
pool ≤ 6 and element-matched. `LoadoutComponent::GetValidationErrors` calls it
for BD characters at runtime; `FSavedLoadout::GetValidationErrors` calls it on
authored saved loadouts (each `UInventoryData::SavedLoadouts[i]`) when the
entry has authored `BDSpellPools`.

**Single-slot absorption** — absorption has one active slot. `HasAbsorbedElement(Element)`
returns true only when `Element` is the most recent absorption — `AbsorbedElements.Last()`.
`RecordAbsorbedElement` moves a re-absorbed element to the end, so the array is a distinct,
recency-ordered history and `Last()` is always the active element. Earlier entries are
historical (retained for possible future "re-tap" abilities) but are not active.

**Castable filter (BD)** — the spells a BD can cast are every Darkness-pool spell
(`InnateSpells`), plus the spells of the single `BDSpellPools` entry whose element is
currently absorbed. `FCombatCapabilities::BuildFrom` assembles this into `RefractionSpells`
(Caster branch): it starts with `InnateSpells`, then for each pool appends `Pool.Spells`
when `HasAbsorbedElement(Pool.Element)`. Because absorption is single-slot, at most one
element pool contributes at a time.

## Forbidden Elements

`Light` and `Void` are *forbidden* elements for BD. `UBrokenDarknessManager::IsForbiddenElement`
(`.cpp:235-238`) is the predicate. When a BD casts a forbidden-element spell,
`UActionExecutor::ProcessForbiddenElementCast` (`ActionExecutor.cpp:3272`, called at `:705`)
routes to `ProcessForbiddenCast` (`BrokenDarknessManager.cpp:258`), which applies self-damage
equal to `SpellBaseDamage × ForbiddenCastSelfDamagePercent` (0.25, `.h:359`). Casting a
forbidden element is **not** a break-roll trigger and does not block the cast — it only
costs HP. `UHybridSpellColors::IsForbiddenElement` (`HybridSpellColors.cpp:64`) is a
separate, identical predicate used by the colour system for VFX intensity.

## Visual Treatment

BD spells render the normal element colour darkened — the element is "infused through
darkness". `UHybridSpellColors` (`HybridSpellColors.h/.cpp`) is a `UBlueprintFunctionLibrary`
that produces `FHybridSpellColorData` (primary / secondary / blended colours, darkness
blend amount, forbidden flag).

The unified entry point is `GetInfusionColors(Element, bIsBrokenDarkness)`
(`HybridSpellColors.cpp:189`): a non-BD caster gets the pure element colour with zero
darkness blend; a BD gets the hybrid darkness-overlaid colour. Weapon and ability variants
(`GetWeaponInfusionColors`, `GetAbilityInfusionColors`) use lighter blends for visibility.

`UActionExecutor` threads a per-cast `bPendingSpellIsBrokenDarkness` flag, set from
`BDManager->IsTransformed()` at cast time (`ActionExecutor.cpp:690-691`), through the spell
spawn paths (`SpawnSpellEffects`, `SpawnSupportSpellEffect`, `SpawnAOEEffect`,
`ResolveInstantSpell` — `:2389-2625`) into `GetInfusionColors`, and resets it afterward
(`:3791`).

`UElementColorDebugComponent` (`ElementColorDebugComponent.cpp`) is a debug component that
tints a character's mesh: for a BD (`IsBrokenDarkness()` true) it uses the blended colour
of `GetHybridElement()`'s absorbed element, or pure BD black if nothing is absorbed
(`.cpp:61-84`). Marked a temporary testing tool (`.h:3`).

## Integration Points

Files outside `UBrokenDarknessManager` that branch on BD state:

| File | BD branch |
|---|---|
| `CharacterDataComponent.cpp` | Owns `bIsBrokenDarkness`; auto-flips it for char-created BD (zeroes `CurrentEP` so they start at 0); `IsBrokenDarkness()` helper; `ServerGainEnergy` BD early-out suppresses *passive regen only*; `ServerGainBrokenDarknessEnergy` is the BD absorption-gain path — overload-aware, bypasses the early-out; `ServerSetBrokenDarkness` no longer zeroes EP — energy carries over on runtime transform. |
| `ActionExecutor.cpp` | `ValidateAction` and `SpendEnergy` compare/debit `CurrentEP` for all characters — no BD energy branch (unified Session 5); `ValidateAction` runs the Caster element gate through the shared `IsElementCastable` predicate; `CalculateActionEnergyCost` returns 0 for `SpellSource == RingCrystal`/`WeaponCrystal` — free equipment-channel casts; **for BD also returns 0 on `SpellSource == Evolution`, *unless* `SpellInfusionLevel ≥ 1 && SelectedSource == Innate` (Darkness conversion, pays normal EP)**; `ExecuteSpellAsync` calls `CrystalManager->ProcessPostCastEvolutionWear` after `SpendEnergy` for every BD evolution-source cast (the wear-as-cost counterpart); `CheckBrokenDarknessBreak` break-roll logic; `OnDefenseResolved` absorption call; `ProcessForbiddenElementCast` gates on `IsBrokenDarkness()`; `bPendingSpellIsBrokenDarkness` visual threading. |
| `LoadoutComponent.cpp` | `HasEquippedSourceForElement` iterates equipped crystals + the primary evolution slot, returning true if any crystal channels the given element — the equipment unlock channel for `IsElementCastable` (`:1202`); `GetValidationErrors` runs the shared element gate for normal Casters and `FCombatLoadout::ValidateBDSpellLoadout` for BD; `InitializeBDPools` / `ApplyBDPoolsIfBroken` build the seven BD element pools for BD characters at loadout creation; `GetAvailableSpells` BD branch appends `BDSpellPools[i].Spells` where `HasAbsorbedElement(pool.Element)` — the BD-aware castable set shared by 12 callers including the 8 AI spell-list sites. |
| `FCombatCapabilities.cpp` | `BuildFrom` Caster branch: for a BD character, appends each `BDSpellPools` entry's spells to `RefractionSpells` when `HasAbsorbedElement(Pool.Element)` — the always-on Darkness pool (`InnateSpells`) plus the single absorbed element's pool. |
| `AIDecisionManager.cpp` | `GetCurrentEP` returns `CharComp->CurrentEP` for all characters — no BD branch (unified Session 5) — feeding `CanAffordSpell` / `CanAffordAbility` and the heal/cleanse checks. `DecideSpell`/`AbilityInfusionLevel` read `CurrentEP`/`MaxEP` directly for the infusion `EnergyPercent`, which is now correct for BD too. Spell lists come from the BD-aware `GetAvailableSpells`. |
| `CombatOrchestrator.cpp` | `ProcessBrokenDarknessOverflow` calls `ProcessOverloadTick` each turn for overloaded BDs (`:993, 1033`); `ApplyBetweenCombatCrystalDestruction` also clears a broken standalone primary evolution via `ULoadoutComponent::ClearBrokenPrimaryEvolution` (the `GetEquippedCrystals` loop misses self-holder evolutions). |
| `CrystalManager.cpp` | `ProcessPostCastEvolutionWear` is the BD-evolution sibling of `ProcessPostCastWear` — reads crystal-modified substat fractions, calls `UBreakCalculator::CalculateDurabilityWearWithSubstats`, writes via `ULoadoutComponent::ApplyWearToActivePrimaryEvolution(_, bForceWear=true)`. No Luck-skip, no per-cast broadcast (between-combat sweep cleans up). See `CrystalWear.md`. |
| `LoadoutComponent.cpp` | (above, plus) two BD-aware wear writers: `ApplyWearToActivePrimaryEvolution(Amount, bForceWear)` and `ClearBrokenPrimaryEvolution`. Both BlueprintCallable; both write the live `SavedLoadouts[ActiveLoadoutIndex]` storage (not a `GetActiveLoadout` copy). |
| `FEvolutionAttachment.cpp` | `ApplyWear(Amount, bForceWear=false)` — `bForceWear=true` bypasses the per-asset `bCanBreak` gate. BD's wear path is the only caller passing `true`; the struct itself stays BD-agnostic. |
| `DamageCalculator.cpp` | `GetBDStackStatusMultiplier` reads the attacker's absorption-stack multiplier when transformed (`:375`). |
| `ItemExecutor.cpp` | When a crystal is used on a BD target (`IsBrokenDarknessCharacter`), `ApplyBrokenDarknessBonus` grants absorption energy (crystal-tier-scaled) via `BDManager->GrantAbsorptionEnergy`. Session 5 fixed a latent bug here — it previously called `ServerGainEnergy`, which the BD early-out silently no-op'd, granting nothing. |
| `CharacterPanelWidget.cpp` | Binds `UBrokenDarknessManager` absorption/overload delegates; for a BD the energy bar shows `CurrentEP`/`MaxEP` (labelled "Absorb"), tinted by absorbed-element colour. |
| `ElementColorDebugComponent.cpp` | Mesh tint uses BD blended colour for `IsBrokenDarkness()` characters (`:61`). |
| `HybridSpellColors.cpp` | `bIsBrokenDarkness` parameter selects darkened vs pure element colours (`:189`). |

## Known Gaps / Not-Yet-Implemented

- **`ForceTransformation` dead** — `BrokenDarknessManager.cpp`, zero production
  callers; intentionally retained as a documented debug/test hook.
- **`OnSuccessfulParry` / `OnSuccessfulBlock` unwired** — `BrokenDarknessManager.cpp:288, 314`,
  zero callers; the live absorption path is `OnDefenseResolved`.

## File Index

| File | Purpose |
|---|---|
| `Public/BrokenDarknessManager.h` / `Private/BrokenDarknessManager.cpp` | Core BD component — transformation, break rolls, absorption, stacks, overload, forbidden-cast self-damage. |
| `Public/CharacterDataComponent.h` / `Private/CharacterDataComponent.cpp` | Owns `bIsBrokenDarkness` and `IsBrokenDarkness()`; suppresses regular EP for BD. |
| `Private/ActionExecutor.cpp` | Break-roll entry point, absorption trigger, forbidden-cast routing, BD visual flag threading. |
| `Public/HybridSpellColors.h` / `Private/HybridSpellColors.cpp` | Darkness-tinted colour data for BD spell/weapon/ability VFX. |
| `Public/ElementColorDebugComponent.h` / `Private/ElementColorDebugComponent.cpp` | Debug mesh-tint component; BD-aware colouring. |
| `Private/CombatOrchestrator.cpp` | Drives `ProcessOverloadTick` each turn for overloaded BDs. |
| `Private/DamageCalculator.cpp` | Reads BD absorption-stack status multiplier into damage. |
| `Private/ItemExecutor.cpp` | Grants absorption energy when a crystal is used on a BD target. |
| `Private/UI/Combat/CharacterPanelWidget.cpp` | Displays BD absorption energy and absorbed-element bar colour. |

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-18 | Session 0 — break-roll rewrite: Darkness innate-element gate added; spell triggers = over-requirement OR L1/L2 infused; ability triggers = infused + innate-Darkness source + over-requirement; ability infusion-overcharge and non-Darkness triggers removed; L1/L2 break multipliers changed 2.0/3.0 → 1.5/2.0 | feature/bd-break-roll-rules |
| 2026-05-18 | Document created — reference state of the BD system after Session 0 | feature/bd-break-roll-rules |
| 2026-05-18 | Sessions 1–2 — element gate now BD-aware and equipment-aware (rings, weapons, primary + attached evolutions); ring/weapon spells now cost 0 energy; BD spends from `AbsorptionEnergy` via `ValidateAction`/`SpendEnergy`; shared `IsElementCastable` predicate between `ValidateAction` and `GetValidationErrors`; new *Energy Model* and *Element Access* sections | feature/bd-element-gate |
| 2026-05-18 | Session 3 — BD spell-pool loadout: `FBDElementSpellPool` struct + `BDSpellPools` field (7 element pools) on `FCombatLoadout` and `ULoadoutData`; `InitializeBDPools` 7-pool init; `ValidateBDSpellLoadout` structural validation; `FCombatCapabilities` surfaces the Darkness pool + the absorbed element's pool; single-slot absorption (`HasAbsorbedElement` matches `AbsorbedElements.Last()`); removed dead `CanCastHybridSpell` and `GetCombatSpells`; new *BD Spell Pools* section | feature/bd-spell-pools |
| 2026-05-18 | Session 4 — AI BD-awareness: `ULoadoutComponent::GetAvailableSpells` now appends absorbed BD pools; `AIDecisionManager::GetCurrentEP` routes BD to `AbsorptionEnergy`. All 8 AI spell-evaluation sites benefit through the shared method. | feature/bd-ai-awareness |
| 2026-05-18 | Session 5 — energy unification: BD absorption energy merged onto `UCharacterDataComponent::CurrentEP`. Deleted `AbsorptionEnergy` / `MaxAbsorptionEnergy` fields and `GetAbsorptionEnergy` / `SpendAbsorptionEnergy` / `CanAffordEnergy` / `GetMaxAbsorptionEnergy`. New `ServerGainBrokenDarknessEnergy` (overload-aware gain, bypasses the passive-regen early-out) + public `GrantAbsorptionEnergy`. Overload now re-evaluated via a `CharacterDataComponent::OnEPChanged` binding. BD branches in `ValidateAction` / `SpendEnergy` / `GetCurrentEP` collapsed. Runtime-transform energy carries over (`ServerSetBrokenDarkness` no longer zeroes EP); overload threshold is the stat-derived `MaxEP`. Fixed `ItemExecutor::ApplyBrokenDarknessBonus` granting nothing on BD. | feature/bd-ai-awareness |
| 2026-05-18 | Session 5 follow-up — `OverloadCapacity` is no longer a flat 30.0f field; `GetOverloadCapacity()` derives it as 30% of `MaxEP` (`OVERLOAD_CAPACITY_FRACTION`) so the overload buffer scales with the BD's energy pool. | feature/bd-energy-unification |
| 2026-05-19 | Session 5 follow-up, Batch A — `ForceTransformation` documented as a debug/test hook; `ResetToMax` now zeroes `CurrentEP` for BD (no passive regen, so a full reset would violate the design); stale L1/L2 break-multiplier doc comment corrected (×2 / ×3 → ×1.5 / ×2.0). | feature/bd-energy-unification |
| 2026-05-19 | Session 5 follow-up, Batch B — combat command menu refreshes the BD spell list live: `CombatCommandMenuSubsystem` binds `BrokenDarknessManager::OnAlignmentChanged` while a BD's menu is open and rebuilds capabilities + the current view on absorption. `FCombatCapabilities::BuildFrom` (Session 3) already filtered `RefractionSpells` to the Darkness pool + absorbed-element pools — this batch adds the live-refresh trigger. | feature/bd-energy-unification |
| 2026-05-19 | AI infusion heuristic confirmed working post-Session-5 unification; sites tidied to use `GetCurrentEP`/`GetMaxEP` helpers for divide-by-zero safety. | feature/ai-infusion-tidy |
| 2026-05-21 | Inventory redesign merged — `ULoadoutData` deleted. BD validation references updated: `ValidateBDSpellLoadout` is now shared between `FCombatLoadout` and `FSavedLoadout` (inline on `UInventoryData::SavedLoadouts`); authored-asset BD pool validation goes through `FSavedLoadout::GetValidationErrors`. `InitializeFromAsset` reference in `ApplyBDPoolsIfBroken` removed (method was deleted). | feature/inventory-refactor |
| 2026-05-27 | BD evolution **wear-as-cost** model — primary-slot evolution casts swap full EP for stat-scaled durability wear via `UCrystalManager::ProcessPostCastEvolutionWear`, hooked from `UActionExecutor::ExecuteSpellAsync`. **Innate-source carve-out** retains normal EP cost on L1/L2 Innate-infused evolution casts (the Darkness conversion). `FEvolutionAttachment::ApplyWear` gains `bForceWear` so BD bypasses the per-asset `bCanBreak` gate intrinsically. Between-combat sweep extended via `ULoadoutComponent::ClearBrokenPrimaryEvolution`. Energy Model table refreshed; new Integration Points rows for `CrystalManager`, `LoadoutComponent`, `FEvolutionAttachment`. Formula and constants split out to `CrystalWear.md`. | feature/crystal-wear-substat-modifier |
