# Broken Darkness System

## Overview

Broken Darkness (BD) is a Caster variant: a character who has "broken" under the strain of
casting beyond their stat requirements. A BD no longer regenerates normal energy — they
fuel spells by absorbing elemental energy from attacks they parry or block, build
absorption stacks that scale status effects, can overload when energy exceeds capacity,
and cast with a darkness-tinted visual treatment. The system is implemented around one
runtime component, `UBrokenDarknessManager`, plus a state flag on `UCharacterDataComponent`.

> This document reflects the BD system on `main` after **Session 0** (`feature/bd-break-roll-rules`),
> which rewrote the break-roll triggers and gates. Energy spending, the loadout, and AI
> integration are not yet BD-aware — see *Known Gaps*.

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

**Energy** — absorption energy lives in `AbsorptionEnergy` (cap `MaxAbsorptionEnergy` 100,
`.h:291-295`), entirely separate from `UCharacterDataComponent::CurrentEP`, which is forced
to 0 for BD (`ServerGainEnergy` early-out `.cpp:159`, `ServerSetEP` guard `.cpp:184`).
Energy may exceed max by `OverloadCapacity` (30); crossing max enters **overload**
(`AddAbsorptionEnergy` / `UpdateOverloadState`, `.cpp:359-415`). `ProcessOverloadTick`
(`.cpp:453`, called by `CombatOrchestrator.cpp:1033`) applies aura damage to nearby enemies,
self-damage, and energy drain each turn while overloaded.

**Stacks & alignment** — `ProcessElementAbsorption` (`.cpp:529`) tracks `CurrentAlignmentElement`.
Absorbing the same element consecutively raises `CurrentAbsorptionStacks` (max 3); absorbing
a different element resets stacks and re-aligns. `GetStackStatusMultiplier` returns
1.0 / 1.0 / 2.0 / 4.0 for stacks 0-3 (`.cpp:513-527`), consumed by
`UDamageCalculator::GetBDStackStatusMultiplier` (`DamageCalculator.cpp:375`).

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
| `CharacterDataComponent.cpp` | Owns `bIsBrokenDarkness`; auto-flips it for char-created BD (`:61-66`); `ServerGainEnergy`/`ServerSetEP` suppress regular EP for BD (`:159, 184`); `IsBrokenDarkness()` helper (`:279`); `ServerSetBrokenDarkness` zeroes EP (`:292`). |
| `ActionExecutor.cpp` | `CheckBrokenDarknessBreak` break-roll logic (`:3196`); `OnDefenseResolved` absorption call (`:1236`); `ProcessForbiddenElementCast` gates on `IsBrokenDarkness()` (`:3282`); `bPendingSpellIsBrokenDarkness` visual threading (`:690`). |
| `CombatOrchestrator.cpp` | `ProcessBrokenDarknessOverflow` calls `ProcessOverloadTick` each turn for overloaded BDs (`:993, 1033`). |
| `DamageCalculator.cpp` | `GetBDStackStatusMultiplier` reads the attacker's absorption-stack multiplier when transformed (`:375`). |
| `ItemExecutor.cpp` | When a crystal is used on a BD target (`IsBrokenDarknessCharacter`, `:653`), `ApplyBrokenDarknessBonus` grants absorption energy scaled by crystal tier (`:101-103, 608`). |
| `CharacterPanelWidget.cpp` | Binds `UBrokenDarknessManager` energy/overload delegates (`:89`); EP bar shows `GetAbsorptionEnergy()` for BD (`:329`) tinted by absorbed-element colour (`:377-398`). |
| `ElementColorDebugComponent.cpp` | Mesh tint uses BD blended colour for `IsBrokenDarkness()` characters (`:61`). |
| `HybridSpellColors.cpp` | `bIsBrokenDarkness` parameter selects darkened vs pure element colours (`:189`). |

## Known Gaps / Not-Yet-Implemented

- **BD energy not wired into action validation/spend** — `ActionExecutor::ValidateAction`
  checks `CurrentEP` (0 for BD) and `SpendEnergy` spends from it; `AbsorptionEnergy` /
  `SpendAbsorptionEnergy` are never consulted. BD spellcasting fails the energy gate.
  (Session 1.)
- **Element gate not BD-aware** — `ActionExecutor::ValidateAction`'s Caster element gate
  (`:183-192`) compares against `InnateElement`, restricting a BD to a single element.
  (Session 2.)
- **No BD spell loadout** — BD has no per-element spell pool; it shares the Caster
  `InnateSpells` shape. (Session 3 — separate design doc.)
- **AI not BD-aware** — `AIDecisionManager` evaluates a flat spell list and does not
  consider absorption or BD energy. (Session 4.)
- **`ForceTransformation` dead** — `BrokenDarknessManager.cpp:185`, zero callers.
- **`OnSuccessfulParry` / `OnSuccessfulBlock` unwired** — `BrokenDarknessManager.cpp:288, 314`,
  zero callers; the live absorption path is `OnDefenseResolved`.
- **`CanCastHybridSpell` / `HasAbsorbedElement` near-dead** — `CanCastHybridSpell`
  (`.cpp:588`) has zero callers; `HasAbsorbedElement` (`.cpp:583`) is called only by it.
- **Stale L1/L2 multiplier docs** — `BrokenDarknessManager.h` (RollForBreak doc comment,
  ~`:59-61`) still states "L1 = base × 2, L2 = base × 3"; actual values are 1.5× / 2.0×
  after Session 0. Header left untouched per Session 0's two-file constraint.

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
