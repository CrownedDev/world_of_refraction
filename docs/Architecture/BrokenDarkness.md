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

A character is behaviourally BD via one of two routes. **Both now produce the same asset
shape — `InnateElement == Darkness` — and differ only in whether the born-BD marker
`bBrokenDarknessInnate` is set** (the representation collapse, `feature/bd-representation-refactor`).

- **Character-created ("born") BD** — the `UCharacterData` asset has `InnateElement ==
  ESpellElement::Darkness` **and** the `bBrokenDarknessInnate` toggle set (`CharacterData.h:136`).
  The toggle is the born-BD *seed*: `UCharacterDataComponent` reads it on init and auto-flips
  `bIsBrokenDarkness` + zeroes `CurrentEP` to put the character in BD runtime state without a
  transform event (`CharacterDataComponent.cpp:62-73`). `UBrokenDarknessManager::BeginPlay`
  mirrors the runtime flag onto `bIsFlipped` so the manager's methods don't short-circuit
  (`BrokenDarknessManager.cpp:118-123`).
- **Runtime-transformed** — a non-BD Darkness caster passes a break roll mid-combat;
  `RollForBreak` → `TriggerTransformation` sets `bIsFlipped` and calls
  `ServerSetBrokenDarkness(true)` (`BrokenDarknessManager.cpp:216-230`). The `UCharacterData`
  asset is **not** mutated — `InnateElement` stays `Darkness`. After the collapse this path is
  **identical in asset shape** to a re-saved born-BD; the only difference is `bBrokenDarknessInnate`
  (false for a transformed Darkness caster, true for a born BD).

⚠️ **Migration.** Legacy assets authored as `InnateElement == BrokenDarkness` are
PostLoad-migrated to `InnateElement = Darkness` + `bBrokenDarknessInnate = true`
(`CharacterData.cpp:47-63`). PostLoad does **not** dirty the package, so the migration is
transient until each asset is re-saved in-editor — the BD assets were re-saved during this arc,
which bakes it (and unblocks the Phase-2 enum deletion; see *Known Gaps*).

`UCharacterDataComponent::IsBrokenDarkness()` now returns `bIsBrokenDarkness` **only** — the
former `InnateElement == BrokenDarkness` fallback was dropped (`CharacterDataComponent.cpp:342-349`).
⚠️ The fallback had to go so a reverted born-BD (arc 2) reads `false`: the runtime flag is the
sole authority for "is currently BD", and born-nature is queried separately via
`UCharacterData::bBrokenDarknessInnate`. All BD-aware code calls `IsBrokenDarkness()` rather
than reading either field directly.

## State Model

| State | Type / location | Represents | Written by | Read by |
|---|---|---|---|---|
| `bIsBrokenDarkness` | `bool`, `UCharacterDataComponent` (`.h:117-133`), `SaveGame` + `Replicated` | Runtime "is currently BD" flag — the **sole authority** | `CharacterDataComponent.cpp:71` (born seed), `ServerSetBrokenDarkness` (`.cpp:366-378`) | `IsBrokenDarkness()` only — never read directly (EP gates excepted) |
| `bIsFlipped` | `bool`, `UBrokenDarknessManager` (`.h:352`) | Manager's mirror of "is currently BD"; gates every absorption/overload method. **Renamed from `bIsTransformed`**; the accessor `IsTransformed()` is kept for BP/API stability (`.h:55`) | `BeginPlay` (`.cpp:120`), `TriggerTransformation` (`.cpp:230`) | `IsTransformed()`, internal guards |
| `bBrokenDarknessInnate` | `bool`, `UCharacterData` asset (`.h:136`) | **Born-BD marker** — design-time "this character is BD from creation". The init seed for `bIsBrokenDarkness` | Asset author (or PostLoad migration) | `CharacterDataComponent` init auto-flip, `ClassInnateResistanceTable::ResolveRow` design-time resistance, debug |
| `InnateElement` | `ESpellElement`, `UCharacterData` asset | Immutable innate element — now always `Darkness` for a BD. `BrokenDarkness` is **no longer a valid innate value** (enum value Hidden, Phase-2-deletion-pending); the born-BD marker is `bBrokenDarknessInnate`, not this field | Asset author | break-roll Darkness gate, `GetDisplayElement()`, visuals |
| `AbsorbedElements` | `TArray<ESpellElement>`, `UBrokenDarknessManager` | Recency-ordered absorbed history; `Last()` is the **active pool**. Seeded to `{Darkness}` on transform | `RecordAbsorbedElement`, `SeedBaseElement` (transform seed) | **`GetActivePool()`** (= `Last()`, `Darkness` when empty) — the single source of truth | `HasAbsorbedElement` |
| ~~`LastAbsorbedElement` / `GetHybridElement()`~~ | — | **RETIRED this arc** — all readers route through `GetActivePool()` instead. A fresh BD reports `Darkness` (seeded), not the old Generic-as-empty | — | — |

`bIsBrokenDarkness` is `SaveGame`-tagged for future persistence but session-only today —
no save system exists (`CharacterDataComponent.h:72-73`).

## Break-Roll System (current behaviour after Session 0)

`UActionExecutor::CheckBrokenDarknessBreak` (`ActionExecutor.cpp:3196`) is the single
break-roll entry point. It is called from `ExecuteAction` (`:319`) and `ExecuteActionAsync`
(`:451`). `UBrokenDarknessManager::RollForBreak` (`BrokenDarknessManager.cpp:111`) is the
only function that can roll, and `CheckBrokenDarknessBreak` is its only caller.

**Gates** (all must pass, in order — `ActionExecutor.cpp:3198-3214`):
1. Actor has a `UBrokenDarknessManager` component.
2. `!BDManager->IsTransformed()` — already-BD characters never re-roll. ⚠️ **Post-collapse this
   is what excludes a born-BD**: a born-BD now has `InnateElement == Darkness` (so it would pass
   gate 3), but the init auto-flip leaves it `IsTransformed()`/`bIsFlipped` true, so gate 2 stops
   it re-rolling.
3. `CharData` valid and `InnateElement == ESpellElement::Darkness` — only innate-Darkness
   characters can break. (Added Session 0.) A born-BD also satisfies this now (its innate element
   *is* Darkness post-collapse) — the exclusion is gate 2, not this gate.

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
`OnDefenseResolved` (`BrokenDarknessManager.cpp`), called from
`ActionExecutor::OnDefenseWindowClosed` when a defense window closes:

- Only `Block` and `Parry` absorb — `Dodge` and failed defenses do not.
- The attack element must be absorbable — `CanAbsorbElement` is an **allowlist**: it rejects
  `Generic`, `None`, `Reality`, and the `BrokenDarkness` enum value, and accepts the 7
  elemental types **plus `Darkness`**. Darkness is absorbable because it is the BD's
  **rotation target back to the base pool** (single-active-pool model, see *BD Spell Pools*) —
  not a no-op.
- Energy gained (`CalculateAbsorptionEnergy`):

      EnergyAbsorbed = AttackBaseEnergyCost × BaseRate × (1 + EfficiencyFactor × K) × PerfectMultiplier

  - **BaseRate** — `PARRY_BASE_RATE` (0.10) / `BLOCK_BASE_RATE` (0.05); parry is 2× block.
  - **EfficiencyFactor** = `GetScalingFraction(ESubStat::Efficiency, owner's GetEffectiveEfficiencyMultiplier())`
    — 0 at no investment → 0.5 at max stat → ~0.9 with gear. Reuses the scaling-arc helper, which reads
    Efficiency *investment* (resolving the stat's lower-is-better inversion: more Efficiency → more absorption).
  - **K** = `ABSORPTION_EFFICIENCY_K` (8.0) → max-stat reaches 5× the base rate (parry 50% / block 25% of the
    attack's base energy cost).
  - **PerfectMultiplier** = `PERFECT_ABSORPTION_MULT` (2.0) on a perfect parry/block, else 1.0. Perfect is
    timing-based and type-agnostic (`FDefenseInputMatch::bPerfect`), threaded via
    `FPendingDefenseContext::bResolvedPerfect` → `OnDefenseResolved` → `CalculateAbsorptionEnergy` — so a
    perfect **block** absorbs double too, not just a perfect parry.

Endpoints (% of the attack's base energy cost; **coefficients are TUNABLE** — Crown may adjust the
zero-Efficiency floor / K / perfect multiplier):

| Efficiency | Parry (normal / perfect) | Block (normal / perfect) |
|------------|--------------------------|--------------------------|
| 0 invest   | 10% / 20%                | 5% / 10%                 |
| max stat   | 50% / 100%               | 25% / 50%                |
| max + gear | ~82% / ~164%             | ~41% / ~82%              |

⚠️ **Behavior shift vs the pre-rework flat model:** at zero Efficiency absorption is now 10%/5% (was a flat
30%/15%) — the floor is intentionally lower, rising past the old rate with Efficiency investment. (The dead
`OnSuccessfulParry` / `OnSuccessfulBlock` pair and their `ParryAbsorptionRate` / `BlockAbsorptionRate` fields
were removed in this rework — zero callers.)

**Debug** — `WoR.AbsorptionSnapshot` (console command, `BrokenDarknessManager.cpp`) resolves the active
combat's first BD and logs the per-type normal/perfect absorption across its current Efficiency level, so the
curve is inspectable without landing an exact parry.

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
`ProcessOverloadTick` (`BrokenDarknessManager.cpp:532`, called by `CombatOrchestrator.cpp`)
applies aura damage to nearby enemies, self-damage, and a **coupled energy leak** each turn
while overloaded: a single `Released = BaseEnergyRelease × StatusMultiplier↑ × Efficiency↓`
value drives both the energy drain (`ServerSpendEnergy`) **and** a self status-buildup in the
alignment element (`UStatusBuildupManager::AddStatusBuildup(..., bSkipBaseStatAmp=true)` — the
release is pre-amplified, so the drain and the self-status read one value; gap 4.3 is built).

**Overload UI surfacing (sweep-5).** Since the bar percent is clamped at 1.0,
`CharacterPanelWidget::RefreshEnergyBar` signals overload through the EP text colour:
white below cap, yellow at ≥ 100%, orange at ≥ 110%, red at ≥ 120% (`CombatConstants::
OVERLOAD_YELLOW_THRESHOLD` / `OVERLOAD_ORANGE_THRESHOLD` / `OVERLOAD_RED_THRESHOLD`).
These thresholds sit **inside the real overload window of `[1.00, 1.30]`** — `CurrentEP`
can never exceed `MaxEP × (1 + OVERLOAD_CAPACITY_FRACTION) = 1.30 × MaxEP`, so any
future tweak must keep the bands within that range or they become dead.

**Stacks & alignment** — `ProcessElementAbsorption` (`.cpp:588`) tracks `CurrentAlignmentElement`.
Absorbing the same element consecutively raises `CurrentAbsorptionStacks` (max 3); absorbing
a different element resets stacks and re-aligns (broadcasts `OnAlignmentChanged` +
`OnStacksChanged` together). Single-alignment model — never multiple elements simultaneously.

`GetStackStatusMultiplier` (`.cpp:572-586`) returns **1.0 / 1.0 / 2.0 / 4.0** for stacks
0-3 (constants `STACK_0_MULT..STACK_3_MULT` in `BrokenDarknessConstants`). This is a
**status-buildup multiplier**, not a damage buff and not a status-effect-output buff —
it amplifies the buildup the BD deposits on a target's status bar, and only when the
spell's element matches the BD's current alignment. The element gate now lives on the
manager itself: `UBrokenDarknessManager::GetElementStackStatusMultiplier(Element)`
(`BrokenDarknessManager.cpp:674`) returns `GetStackStatusMultiplier()` when
`Element == CurrentAlignmentElement`, else `1.0`. It is consumed directly by
`UStatusBuildupManager::AddStatusBuildup` as **step 5c** (`StatusBuildupManager.cpp:383`).
The former `UDamageCalculator::GetBDStackStatusMultiplier` wrapper was **deleted**
(`feature/fix-bd-stack-multiplier`) — it lost its only caller when `CalculateStatusBuildup`
was removed (documented at `DamageCalculator.h:230-235`). Stack 0 and stack 1 both return
`1.0` — the buff only kicks in at stack 2.

**Stacks display (sweep-5).** `CharacterPanelWidget` binds `OnStacksChanged`,
`OnAlignmentChanged`, and `OnTransformed`; their handlers route through
`RefreshEffectsList`, which appends a synthetic `FActiveSkillEffect` to the array passed
to the BP-side `RebuildEffectsList` whenever the bound character is a transformed BD
with `GetCurrentStackCount() > 0`. The synthetic entry uses `EffectType =
StatusMultiplierBuff` (truthful — the stacks are a status-multiplier buff), `Element =
CurrentAlignment`, `bCanStack = true`, `CurrentStacks = GetCurrentStackCount()`,
`MaxStacks = GetMaxStacks()` (3), `bPermanent = true`, `EffectName` = element display
name. The existing `SkillEffectBlueprintLibrary` helpers (`GetEffectDisplayName`,
`GetEffectStackString`, `IsEffectBuff`) render it without BP changes. Auto-clears: when
stacks drop to 0 or alignment switches, the next refresh simply doesn't append the
entry. See `UISystem.md` for the panel data-flow side.

### Non-defense absorption (crystals) + the Reality cleanse (`fix/bd-item-absorption-element`)

A crystal used on a BD reaches absorption through the **element-aware** public entry point
`GrantAbsorptionEnergy(float Amount, ESpellElement Element)`
(`BrokenDarknessManager.cpp`). It runs `AddAbsorptionEnergy(Amount)` then
`RecordAbsorbedElement(Element)` — and because `RecordAbsorbedElement` **self-guards**
`CanAbsorbElement`, an **absorbable** crystal both grants energy **and rotates** the active
pool (firing `OnAlignmentChanged`), while a non-absorbable element no-ops the rotation.

**`DrainAndRevertToBase(float Amount)`** is the **Reality cleanse** primitive — the opposite of
absorption:

1. `GetCharComp()->ServerSpendEnergy(RoundToInt(Amount))` — drains EP, **clamped at 0**, fires
   `OnEPChanged`.
2. `SeedBaseElement()` — resets `AbsorbedElements` to `{Darkness}` + `CurrentAlignmentElement = Darkness`. The character **stays BD** (no `bIsFlipped` change — *not* `RevertTransformation`).
3. An **explicit `OnAlignmentChanged.Broadcast(GetOwner(), OldActive, Darkness)`**, guarded
   `OldActive != Darkness`.

⚠️ **Gotcha — `SeedBaseElement` is SILENT.** It sets `CurrentAlignmentElement` directly (it is
the transform-seed path), so it does **not** fire `OnAlignmentChanged`. The explicit broadcast in
step 3 is therefore **required** — without it the EP/Absorb bar would not re-tint to near-black
after a cleanse. (The defense path gets its broadcast from `RecordAbsorbedElement → ProcessElementAbsorption`; the cleanse path must broadcast manually.)

**Two call sites of the cleanse:**

- **Item path — `UItemExecutor::ApplyBrokenDarknessBonus`** is now a **three-way** branch on
  `ItemIdentity::GetElement(Id)`:
  - `Reality` (Iolite) → `DrainAndRevertToBase(BonusEnergy)` — drain + revert.
  - `None` (Quartz) → **no-op** (no `GrantAbsorptionEnergy` call). ⚠️ **Behaviour change:** Quartz
    previously fell into `GrantAbsorptionEnergy` and **granted energy** (rotation no-op'd) — that
    energy grant is **removed**; Quartz now does nothing to absorption.
  - real element → `GrantAbsorptionEnergy(BonusEnergy, Elem)` — grant + rotate.
  - `OutResult.BrokenDarknessEnergyGained` is the honest `CurrentEP` delta (negative on a Reality
    drain, 0 on a Quartz no-op, positive on a grant).
- **Defense path — `OnDefenseResolved`** gains a Reality branch placed **BEFORE** the generic
  `!CanAbsorbElement` return: `if (AttackElement == Reality)` → drains
  `CalculateAbsorptionEnergy(...)` (the would-be-gain — same function/args as the absorb path, so
  perfect parry/block doubles the drain) via `DrainAndRevertToBase` and returns. ⚠️ **Order
  matters** — `CanAbsorbElement(Reality)` is false, so without the early Reality branch the
  generic check would swallow it with no drain. A **Generic spell resolved to Reality** arrives as
  `AttackElement == Reality`, so it is covered identically.

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

`Generic` short-circuits to **always castable** at the top of `IsElementCastable` — a Generic
(polymorphic) spell resolves to a real element at cast (see *Generic Spell Resolution* in
[`InfusionSystem.md`](InfusionSystem.md)), so the element itself never gates. Otherwise an
element is castable when **any** of these hold:

- **Non-BD** — the element matches `InnateElement`; or `InnateElement` is itself an
  any-element source (`Reality` / `BrokenDarkness`, via `ElementHelpers::IsAnySpellSource`);
  or an equipped crystal channels the element.
- **BD (single active pool)** — the element is the **active pool**, `GetActivePool()`
  (`AbsorbedElements.Last()`, `Darkness` when seeded); or an equipped crystal channels the
  element. The former always-on `Element == Darkness` clause was **dropped**: Darkness is
  castable only while it IS the active pool (it is seeded as the base pool on transform and
  rotates like any other element).

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

- **Darkness pool** — `FCombatLoadout::InnateSpells` (the existing Caster field). Up to
  `SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL` (6) spells, every entry `Darkness` element
  (a `Generic` spell is also accepted — it resolves to Darkness here). It is the **base
  pool**: seeded as the active pool on transform, and the rotation target when Darkness is
  re-absorbed. Castable only while it is the active pool — **not** always-on (see *single
  active pool* below).
- **Element pools** — `FCombatLoadout::BDSpellPools`, a `TArray<FBDElementSpellPool>`
  (`FCombatLoadout.h`). Up to `MAX_BD_ELEMENT_POOLS` (7) pools — `Fire`, `Water`, `Earth`,
  `Wind`, `Light`, `Lightning`, `Void` — each holding up to `MAX_EQUIPPED_SLOT_POOL` (6)
  spells that must match the pool's element. `Reality` is excluded: it cannot be absorbed
  (`CanAbsorbElement`), so it is never a pool.

Two independent limits apply per the weighted-budget model
([`InnateSpellPoolBudget.md`](../Design/InnateSpellPoolBudget.md)): the per-pool **count**
cap above, and a single shared **weight budget** — Σ spell cost (tier, mastery-discounted)
≤ `BD_SPELL_BUDGET` (48) across the Darkness pool + every element pool (double the non-BD
innate 24).

`ULoadoutComponent::InitializeBDPools` builds the seven empty pools — one per absorbable
non-Darkness element — and is idempotent (authored pools survive, missing ones are added).
`ApplyBDPoolsIfBroken` runs it when the owning character is `IsBrokenDarkness()`, at
loadout creation (via `InitializeFromCharacterData`, including the empty-loadout
soft-fail path). Non-BD characters get no pools.

**Validation** — `FCombatLoadout::ValidateBDSpellLoadout(InnateSpells, BDSpellPools,
int32 Discount = 0, bool bCheckWeight = false)` (static, shared between runtime
`FCombatLoadout` and the inline-asset `FSavedLoadout`) enforces:
- **Structural (always):** Darkness pool ≤ `MAX_EQUIPPED_SLOT_POOL` and all-Darkness;
  ≤ `MAX_BD_ELEMENT_POOLS` element pools; each pool ≤ `MAX_EQUIPPED_SLOT_POOL` and
  element-matched.
- **Weight budget (only when `bCheckWeight`):** Σ `SpellSlotEffectiveCost(tier, Discount)`
  across the Darkness pool + every element pool ≤ `BD_SPELL_BUDGET` (48) — one shared total.

`LoadoutComponent::GetValidationErrors` + `CollectInvalidSlotFindings` call it for BD
characters at runtime, computing the discount from the character's raw world-pillar levels
and passing `bCheckWeight = true`. `FSavedLoadout::GetValidationErrors` calls it on authored
saved loadouts (each `UInventoryData::SavedLoadouts[i]` with authored `BDSpellPools`) with
the defaults — **count + element only, no weight** (the asset path has no character for the
discount).

**Single active pool (the rotation model)** — a BD casts from exactly **one** pool at a time:
the *active pool*, `UBrokenDarknessManager::GetActivePool()` = `AbsorbedElements.Last()`, or
`Darkness` when nothing is seeded. Darkness is **seeded** as the active pool on transform
(`SeedBaseElement()`, called from `TriggerTransformation` and the born-BD `BeginPlay` branch)
so a fresh BD starts able to cast its base pool without first parrying — element axis only,
**no absorption energy granted**. Absorbing an element **rotates** the active pool to it; the
prior pool goes dormant. Absorbing `Darkness` rotates back to the base (innate Darkness) pool.
`HasAbsorbedElement(Element)` returns true only for the active pool (`Last()`);
`RecordAbsorbedElement` moves a re-absorbed element to the end (recency-ordered history;
earlier entries are dormant, retained for possible future "re-tap" abilities).

> **Model change (this arc).** This replaced the earlier model where the Darkness pool was
> *always* castable **and** every absorbed element pool was castable simultaneously. Now it is
> strictly one active pool, Darkness seeded on transform and rotated like any element.

**Castable filter (BD)** — the spells a BD can cast are the spells of the **single active
pool only**: the Darkness/base pool (`InnateSpells`) when `GetActivePool() == Darkness`,
otherwise the matching `BDSpellPools` entry's spells. Both `ULoadoutComponent::GetAvailableSpells`
and `FCombatCapabilities::BuildFrom` (Caster branch) route through `GetActivePool()` and return
only that pool — gated on `IsBrokenDarkness()`, so the non-BD Caster path (full `GetAllSpells`)
is unchanged. The equipped-crystal channel in `IsElementCastable` is a separate per-element
unlock and is **not** added to this spell list.

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

> **Colour model (collapsed this arc).** *BD IS Darkness* — there is **one** BD/Darkness
> near-black (`ElementColors::Darkness` ≈ `0.02`), and `ElementColors::BrokenDarkness` is an
> **alias** of it. The separate purple "pure BD" colours (`PURE_BD_PRIMARY` / `PURE_BD_SECONDARY`)
> were **deleted** — no purple. The rule is uniform: **Darkness is the black; an absorbed
> element = black-over-element = the darkened element** (`GetHybridSpellColors(Darkness)` →
> pure near-black; `GetHybridSpellColors(Fire)` → dark red; etc.).

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
of the **active pool** (`GetActivePool()`) — the base Darkness pool resolves to the single
BD near-black. Marked a temporary testing tool (`.h:3`).

## Post-Collapse Status Dispatch (Silence vs Drain)

After the representation collapse a BD emits **Darkness** like any other Darkness caster, so the
bar-cap trigger for a Darkness hit needs to know whether the *source* is a BD to preserve BD's
identity: a Darkness hit normally maps to **Silenced**, but a BD's drain must stay **DrainEnergy**.

`BarCapTriggerResolver::ResolveTrigger` gained a `bSourceIsBrokenDarkness` parameter
(`BarCapTriggerResolver.h:30-31`): a Darkness hit from a **BD source** → `DrainEnergy`; from a
**non-BD source** → `Silenced` (`.h:50-51`). BD-ness is the caster's property, not the element's.

The flag is computed at the **dispatch's source**, in `UStatusBuildupManager`, **before the
immunity gate** — so the corrected trigger feeds both the immunity check and the cap-fire reuse:

- Live path — `AddStatusBuildup` looks up the source's `IsBrokenDarkness()` at entry
  (`StatusBuildupManager.cpp:315-328`), ahead of the per-trigger immunity gate.
- Preview path — `GetPendingTrigger` resolves the same way from the recorded `LastSource`
  (`StatusBuildupManager.cpp:137-146`) so the UI preview matches the live result.

Without this, a post-collapse BD's Darkness emission would Silence like a normal Darkness caster;
the source-side branch is what keeps it draining energy.

## Integration Points

Files outside `UBrokenDarknessManager` that branch on BD state:

| File | BD branch |
|---|---|
| `CharacterDataComponent.cpp` | Owns `bIsBrokenDarkness`; auto-flips it for a born BD from `UCharacterData::bBrokenDarknessInnate` (zeroes `CurrentEP` so they start at 0); `IsBrokenDarkness()` helper (now returns the runtime flag **directly** — no `InnateElement` fallback); `ServerGainEnergy` BD early-out suppresses *passive regen only*; `ServerGainBrokenDarknessEnergy` is the BD absorption-gain path — overload-aware, bypasses the early-out; `ServerSetBrokenDarkness` no longer zeroes EP — energy carries over on runtime transform. |
| `ActionExecutor.cpp` | `ValidateAction` and `SpendEnergy` compare/debit `CurrentEP` for all characters — no BD energy branch (unified Session 5); `ValidateAction` runs the Caster element gate through the shared `IsElementCastable` predicate; `CalculateActionEnergyCost` returns 0 for `SpellSource == RingCrystal`/`WeaponCrystal` — free equipment-channel casts; **for BD also returns 0 on `SpellSource == Evolution`, *unless* `SpellInfusionLevel ≥ 1 && SelectedSource == Innate` (Darkness conversion, pays normal EP)**; `ExecuteSpellAsync` calls `CrystalManager->ProcessPostCastEvolutionWear` after `SpendEnergy` for every BD evolution-source cast (the wear-as-cost counterpart); `CheckBrokenDarknessBreak` break-roll logic; `OnDefenseResolved` absorption call; `ProcessForbiddenElementCast` gates on `IsBrokenDarkness()`; `bPendingSpellIsBrokenDarkness` visual threading. |
| `LoadoutComponent.cpp` | `HasEquippedSourceForElement` iterates equipped crystals + the primary evolution slot, returning true if any crystal channels the given element — the equipment unlock channel for `IsElementCastable` (`:1202`); `GetValidationErrors` runs the shared element gate for normal Casters and `FCombatLoadout::ValidateBDSpellLoadout` for BD; `InitializeBDPools` / `ApplyBDPoolsIfBroken` build the seven BD element pools for BD characters at loadout creation; `GetAvailableSpells` BD branch appends `BDSpellPools[i].Spells` where `HasAbsorbedElement(pool.Element)` — the BD-aware castable set shared by 12 callers including the 8 AI spell-list sites. |
| `FCombatCapabilities.cpp` | `BuildFrom` Caster branch: for a BD character, appends each `BDSpellPools` entry's spells to `RefractionSpells` when `HasAbsorbedElement(Pool.Element)` — the always-on Darkness pool (`InnateSpells`) plus the single absorbed element's pool. |
| `AIDecisionManager.cpp` | `GetCurrentEP` returns `CharComp->CurrentEP` for all characters — no BD branch (unified Session 5) — feeding `CanAffordSpell` / `CanAffordAbility` and the heal/cleanse checks. `DecideSpell`/`AbilityInfusionLevel` read `CurrentEP`/`MaxEP` directly for the infusion `EnergyPercent`, which is now correct for BD too. Spell lists come from the BD-aware `GetAvailableSpells`. |
| `CombatOrchestrator.cpp` | `ProcessBrokenDarknessOverflow` calls `ProcessOverloadTick` each turn for overloaded BDs (`:993, 1033`); `ApplyBetweenCombatCrystalDestruction` also clears a broken standalone primary evolution via `ULoadoutComponent::ClearBrokenPrimaryEvolution` (the `GetEquippedCrystals` loop misses self-holder evolutions). |
| `CrystalManager.cpp` | `ProcessPostCastEvolutionWear` is the BD-evolution sibling of `ProcessPostCastWear` — reads crystal-modified substat fractions, calls `UBreakCalculator::CalculateDurabilityWearWithSubstats`, writes via `ULoadoutComponent::ApplyWearToActivePrimaryEvolution(_, bForceWear=true)`. No Luck-skip, no per-cast broadcast (between-combat sweep cleans up). See `CrystalWear.md`. |
| `LoadoutComponent.cpp` | (above, plus) two BD-aware wear writers: `ApplyWearToActivePrimaryEvolution(Amount, bForceWear)` and `ClearBrokenPrimaryEvolution`. Both BlueprintCallable; both write the live `SavedLoadouts[ActiveLoadoutIndex]` storage (not a `GetActiveLoadout` copy). |
| `FEvolutionAttachment.cpp` | `ApplyWear(Amount, bForceWear=false)` — `bForceWear=true` bypasses the per-asset `bCanBreak` gate. BD's wear path is the only caller passing `true`; the struct itself stays BD-agnostic. |
| `StatusBuildupManager.cpp` | `AddStatusBuildup` **step 5c** multiplies the deposited buildup by the *source* BD's `GetElementStackStatusMultiplier(Element)` — matching-alignment only — the live consumer of the absorption-stack status-buildup multiplier (`:383`). The former `DamageCalculator::GetBDStackStatusMultiplier` wrapper was deleted (`feature/fix-bd-stack-multiplier`). **Also computes `bSourceIsBrokenDarkness` at entry (`:315-328`) and passes it to `BarCapTriggerResolver::ResolveTrigger`** so a Darkness hit from a BD source resolves to `DrainEnergy` not `Silenced` (post-collapse identity); `GetPendingTrigger` mirrors it for the UI preview (`:137-146`). See *Post-Collapse Status Dispatch*. |
| `ItemExecutor.cpp` | When a crystal is used on a BD target (`IsBrokenDarknessCharacter`), `ApplyBrokenDarknessBonus` grants absorption energy scaled as **% of target MaxEP** (sweep-1: F=10% .. S=70% via `CrystalEffectTable::GetBrokenDarknessEnergyPercent` × `TargetComp->MaxEP`) via `BDManager->GrantAbsorptionEnergy` — overload-aware. Replaces the prior flat tier values. Session 5 fixed a latent bug here — it previously called `ServerGainEnergy`, which the BD early-out silently no-op'd, granting nothing. See `ItemSystem.md`. |
| `CharacterPanelWidget.cpp` | Binds `UBrokenDarknessManager` absorption/overload delegates **plus (sweep-5) `OnStacksChanged`/`OnAlignmentChanged`/`OnTransformed`**. For a BD the energy bar shows `CurrentEP`/`MaxEP` (labelled "Absorb"), tinted by absorbed-element colour; overload past `MaxEP` colours the EP text white→yellow→orange→red within the `[1.00, 1.30]` cap. Absorption stacks render in the effects panel as a synthetic `StatusMultiplierBuff` row (element-aligned, `xN` count). See `UISystem.md`. |
| `CharacterDataComponent.cpp` *(sweep-5)* | Adds `GetDisplayElement()` UI-facing element accessor: returns `BrokenDarkness` whenever `IsBrokenDarkness()` is true, else delegates to `CharacterData->GetElement()` (Caster → `InnateElement`; others → `Generic`). Single source of truth for panels/labels — gameplay-internal element reads continue to use existing paths. |
| `ElementColorDebugComponent.cpp` | Mesh tint uses BD blended colour for `IsBrokenDarkness()` characters (`:61`). |
| `HybridSpellColors.cpp` | `bIsBrokenDarkness` parameter selects darkened vs pure element colours (`:189`). |

## Known Gaps / Not-Yet-Implemented

- **`ForceTransformation` dead** — `BrokenDarknessManager.cpp`, zero production
  callers; intentionally retained as a documented debug/test hook.
- **Forbidden-cast self-buildup unbuilt (gap 4.2).** When a BD casts a forbidden
  element (Light/Void), `ProcessForbiddenCast` applies self-**damage** only; the
  designed self-**status-buildup** half (scaled by `StatusMultiplier`, element =
  forbidden cast element) is not yet wired. The dead `ApplySelfStatusBuildup`
  helper on `ActionExecutor` is retained as the intended apply hook. See
  `docs/Gaps/IntegrationGaps.md` §4.2.
- **`bIsBrokenDarkness` save persistence (gap 4.4) / un-transform path
  (gap 4.5).** Both designed, neither built. See `docs/Gaps/IntegrationGaps.md`.
- **Phase 2 — delete `ESpellElement::BrokenDarkness` (SHIPPED, PIE-verified).** The enum value
  is **deleted** (`feature/bd-value-deletion`). BD is now represented **only** by
  `bBrokenDarknessInnate` + `InnateElement == Darkness` — there is no BD element value. Both
  gates that blocked the deletion are resolved: **(a)** the single BD asset was re-saved
  (`InnateElement=Darkness` + toggle), so nothing serialises the value, and **(b)** the
  `InitializeBDPools` loop bound moved to the explicit `None` sentinel (`i < (uint8)None`, now
  iterating the real elements 0..9). The dead PostLoad migration was removed; all dead BD-value
  branches (immunity / resistance alias / `IsAnySpellSource` / `CanAbsorbElement` / display
  colour) were stripped first (behaviour preserved by live paths). `None` is now value 10.
- **Arc 2 — BD → Darkness revert (shipped, PIE-verified).** The direct runtime model
  (`IsBrokenDarkness()` reading the flag only) plus the two queryable fields (born =
  `bBrokenDarknessInnate`, current = `bIsBrokenDarkness`) made the BD→Darkness direction of the
  switch buildable. `UBrokenDarknessManager::RevertTransformation()` (`BlueprintCallable`) is the
  **forced** revert — no random trigger; it guards `!bIsFlipped` as a no-op, clears all BD runtime
  state (`ExitOverload()` + `ResetStacks()` + alignment / `AbsorbedElements` / `LastAbsorbedElement`),
  calls `ServerSetBrokenDarkness(false)`, and fires the new `OnReverted` delegate (reuses
  `FOnBrokenDarknessTransformed`; separate edge from `OnTransformed`). `ServerSetBrokenDarkness(false)`
  now has a real body — resets `CurrentEP = MaxEP` and broadcasts `OnEPChanged` (relabels the bar
  Absorb→EP); asymmetric vs the BD-activate branch, which carries EP over. The Darkness→BD direction
  (break-roll via `TriggerTransformation`) is unchanged. Debug: `WoR.TestBDRevert` (permanent) fires
  the revert on the first transformed BD in the active combat. **The full BD↔Darkness switch is now
  mechanically complete (both directions exist); only the *trigger* that calls `RevertTransformation`
  (healer / item / interaction) is pending — future work.**

## File Index

| File | Purpose |
|---|---|
| `Public/BrokenDarknessManager.h` / `Private/BrokenDarknessManager.cpp` | Core BD component — transformation, break rolls, absorption, stacks, overload, forbidden-cast self-damage. |
| `Public/CharacterDataComponent.h` / `Private/CharacterDataComponent.cpp` | Owns `bIsBrokenDarkness` and `IsBrokenDarkness()`; suppresses regular EP for BD. |
| `Private/ActionExecutor.cpp` | Break-roll entry point, absorption trigger, forbidden-cast routing, BD visual flag threading. |
| `Public/HybridSpellColors.h` / `Private/HybridSpellColors.cpp` | Darkness-tinted colour data for BD spell/weapon/ability VFX. |
| `Public/ElementColorDebugComponent.h` / `Private/ElementColorDebugComponent.cpp` | Debug mesh-tint component; BD-aware colouring. |
| `Private/CombatOrchestrator.cpp` | Drives `ProcessOverloadTick` each turn for overloaded BDs. |
| `Private/StatusBuildupManager.cpp` | `AddStatusBuildup` step 5c — applies the source BD's `GetElementStackStatusMultiplier` (matching-alignment **status-buildup** multiplier) to deposited buildup. Live consumer since `DamageCalculator::GetBDStackStatusMultiplier` was removed. |
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
| 2026-05-28 | Sweep-1 — crystal absorption energy refactored to **% of target MaxEP** (`BD_ENERGY_PERCENT_*` F=10% .. S=70%; `CrystalEffectTable::GetBrokenDarknessEnergyPercent`); was previously flat tier values. Sweep-5 — added `UCharacterDataComponent::GetDisplayElement()` UI helper; stack-line refs corrected (`.cpp:588`/`.cpp:572-586`/`DamageCalculator.cpp:356-371`); stacks now render in the panel's effects list as a synthetic `StatusMultiplierBuff` row (replaces the standalone-text approach); overload EP-text colour bands rescaled into the real `[1.00, 1.30]` window. Known Gaps section captures unbuilt 4.2/4.3/4.4/4.5 with cross-links to `IntegrationGaps.md`. Stack-multiplier prose tightened — it's a **status-buildup** multiplier (matching-element only), not a damage buff. | feature/integration-gaps-sweep-1, feature/integration-gaps-sweep-5 |
| 2026-06-15 | Planned: per-impact energy absorption (energy cost split across impacts proportional to the damage split, on parried/blocked impacts) — arising from reactive per-impact defense; build after Stage 3. See docs/Design/BrokenDarkness_ReactiveDefense.md. | feature/realtime-defense |
| 2026-06-16 | Doc-sync: `UDamageCalculator::GetBDStackStatusMultiplier` was **deleted** (`feature/fix-bd-stack-multiplier`) — the element-gated accessor now lives on the manager as `UBrokenDarknessManager::GetElementStackStatusMultiplier(Element)` and is consumed by `UStatusBuildupManager::AddStatusBuildup` as **step 5c** (`StatusBuildupManager.cpp:383`). Updated §Stacks, the Integration table, and the File Index (DamageCalculator → StatusBuildupManager). **Gap 4.3 closed** — `ProcessOverloadTick` now wires the coupled energy leak: one pre-amplified `Released` value drives both `ServerSpendEnergy` and a self `AddStatusBuildup(..., bSkipBaseStatAmp=true)`; removed it from Known Gaps. | feature/realtime-defense |
| 2026-06-20 | BD spell pools gain the weighted-budget model — per-pool count cap re-pointed to `SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL` (6); `ValidateBDSpellLoadout` gained `(int32 Discount, bool bCheckWeight)` and now also enforces ONE shared `BD_SPELL_BUDGET` (48) across the Darkness pool + all element pools (Σ `SpellSlotEffectiveCost`, mastery-discounted), at the runtime gates only (asset path = count + element). Element-match + `MAX_BD_ELEMENT_POOLS` (≤7) unchanged; `MAX_BD_POOL_SPELLS` retired. See `InnateSpellPoolBudget.md`. | feature/innate-bd-spell-budget |
| 2026-06-18 | **BD representation collapse (arc 1)** — character-created BD is now `InnateElement = Darkness` + `bBrokenDarknessInnate` toggle (was `InnateElement == BrokenDarkness`); both BD paths now share one asset shape (Darkness), differing only in the born marker. `IsBrokenDarkness()` returns the runtime flag **directly** — dropped the `InnateElement == BrokenDarkness` fallback (a reverted born-BD must read false). `bIsTransformed` renamed `bIsFlipped` (accessor `IsTransformed()` kept for BP/API stability). **Silence/Drain fix:** `BarCapTriggerResolver::ResolveTrigger` gains `bSourceIsBrokenDarkness`, computed at the dispatch source in `StatusBuildupManager` before the immunity gate, so a Darkness hit from a BD source → `DrainEnergy`, from a non-BD source → `Silenced`. Legacy `InnateElement == BrokenDarkness` assets PostLoad-migrated → Darkness + toggle (transient until re-saved; BD assets re-saved this arc). `ESpellElement::BrokenDarkness` is **Hidden, not deleted** — Phase 2 deletion deferred (gated on re-save [done] + `InitializeBDPools` loop Max-sentinel [pending]). Arc 2 (BD↔Darkness revert) recorded as next. Updated *Two Paths*, *State Model*, break-roll gates, new *Post-Collapse Status Dispatch* section, Integration table, Known Gaps. | feature/bd-representation-refactor |
| 2026-06-18 | **Absorption rework** — replaced the flat parry/block rates (0.30/0.15) with an Efficiency-scaled, perfect-doubling model: `EnergyAbsorbed = AttackBaseEnergyCost × BaseRate(0.10/0.05) × (1 + GetScalingFraction(Efficiency) × K(8.0)) × PerfectMultiplier(2.0)`. Perfect (parry **or** block) doubles, threaded via `FPendingDefenseContext::bResolvedPerfect` → `OnDefenseResolved` → `CalculateAbsorptionEnergy`. Zero-Efficiency floor is now lower (10%/5%, was 30%/15%), rising past the old rate with investment; max-stat 50%/25%, max-gear ~82%/41%. Removed the dead `OnSuccessfulParry`/`OnSuccessfulBlock` pair + `ParryAbsorptionRate`/`BlockAbsorptionRate` fields. Debug: `WoR.AbsorptionSnapshot`. Coefficients TUNABLE. Per-impact absorption (`BrokenDarkness_ReactiveDefense.md` §8c) remains deferred. | feature/bd-absorption-rework |
| 2026-06-21 | **Generic Spell Inheritance arc — BD Model-B single active pool** (`feature/generic-spell-inherit`, PIE-verified). BD absorption is now a single-active-pool **rotation**: `GetActivePool()` = `AbsorbedElements.Last()`, with `Darkness` **seeded** on transform via `SeedBaseElement()` (from `TriggerTransformation` + born-BD `BeginPlay`; element axis only, no energy). Absorbing rotates the active pool, prior pool dormant; absorbing Darkness returns to the base pool. `IsElementCastable`, `ULoadoutComponent::GetAvailableSpells`, and `FCombatCapabilities::BuildFrom` all route through `GetActivePool()` — **dropped** the always-on `Element == Darkness` clause and the Model-A "all absorbed pools at once" append (both now show only the active pool, gated on `IsBrokenDarkness()`). `CanAbsorbElement` is now an **allowlist** (rejects `Generic`/`None`/`Reality`/`BrokenDarkness`-value; accepts the 7 elements + `Darkness`-as-rotation-target). `IsElementCastable` gains a `Generic`-always-castable short-circuit (Generic resolves at cast). Updated *Absorption System*, *Element Access*, *BD Spell Pools*. Full arc (enum `None` append, ~40-site `Generic→None` sentinel migration, the `ResolveSpellCastElement` resolver, cast-boundary wiring, `SpellElementMatchesHost` gates, naming) in `docs/Design/Completed/GenericSpellInherit.md`. | feature/generic-spell-inherit |
| 2026-06-18 | **Forced BD→Darkness revert (arc 2)** — built the BD→Darkness direction of the runtime switch. New `UBrokenDarknessManager::RevertTransformation()` (`BlueprintCallable`): guard `!bIsFlipped` → no-op; else `bIsFlipped=false` → `ExitOverload()` + `ResetStacks()` + clear alignment / `AbsorbedElements` / `LastAbsorbedElement` → `ServerSetBrokenDarkness(false)` → `OnReverted.Broadcast()`. Mirrors `TriggerTransformation`'s structure. New `OnReverted` delegate (reuses `FOnBrokenDarknessTransformed`; separate edge so listeners bind specifically). `ServerSetBrokenDarkness(false)` gained a real body — `CurrentEP=MaxEP` + `OnEPChanged` broadcast (relabels bar Absorb→EP); asymmetric vs the activate branch (which carries EP over), direct field set bypasses the BD EP guard (flag already cleared). `WoR.TestBDRevert` console command added as permanent debug tooling (reverts the first transformed BD in combat, logs result). UI auto-corrects via existing `IsBrokenDarkness()` + `OnEPChanged` bindings. The Darkness→BD direction (break-roll) is unchanged. The **trigger** that calls `RevertTransformation` (healer / item / interaction) is **not** built — mechanism only; the BD↔Darkness switch is now mechanically complete pending a trigger. Updated the arc-2 Known-Limitations bullet → shipped. | feature/bd-switch |
| 2026-06-21 | **Phase-2 `ESpellElement::BrokenDarkness` value DELETED** (`feature/bd-value-deletion`, PIE-verified). The enum value is gone; BD is represented **only** by `bBrokenDarknessInnate` + `InnateElement=Darkness`. `InitializeBDPools` loop bound moved to the `None` sentinel (`i < (uint8)None`, iterating real elements 0..9); dead PostLoad migration removed; single BD asset re-saved; `None` is now value 10. All dead BD-value branches stripped first (immunity maps, the `GetElementColumn` BD→Darkness alias, `IsAnySpellSource`, `CanAbsorbElement`'s self-reject) — behaviour preserved by live paths. **Reconciliation:** `LastAbsorbedElement` / `GetHybridElement()` **retired** — readers route through `GetActivePool()` (Model-B single source of truth; a fresh BD reports seeded `Darkness`). **Colour collapse:** one BD/Darkness near-black (`0.02`); purple `PURE_BD_PRIMARY`/`PURE_BD_SECONDARY` deleted; `ElementColors::BrokenDarkness` aliased to `Darkness` — *BD IS Darkness; absorb = black-over-element*. **EP/Absorb bar** now tints to the active-pool hybrid colour (`GetHybridSpellColors(GetActivePool()).BlendedColor`) and re-tints on rotation (`HandleBDAlignmentChanged` → `ApplyEnergyBarTint`). Updated *State Model*, *Element Access*, *BD Spell Pools*, *Visual Treatment*, Known Gaps. | feature/bd-value-deletion |
| 2026-06-21 | **Crystal-on-BD rotation + the Reality cleanse** (`fix/bd-item-absorption-element`, PIE-verified). `GrantAbsorptionEnergy` is now element-aware — `(float Amount, ESpellElement Element)` — running `AddAbsorptionEnergy` + `RecordAbsorbedElement` (self-guards `CanAbsorbElement`), so an absorbable crystal **grants energy AND rotates** the active pool. New **`DrainAndRevertToBase(float Amount)`** — the Reality cleanse: `ServerSpendEnergy` (clamped 0) + `SeedBaseElement` (clear `AbsorbedElements`→`{Darkness}`, stays BD) + an **explicit `OnAlignmentChanged` broadcast** (⚠️ `SeedBaseElement` is silent — the manual broadcast is required for the bar to re-tint). Two call sites: `UItemExecutor::ApplyBrokenDarknessBonus` is now a **three-way** (Reality→`DrainAndRevertToBase` / `None`/Quartz→**no-op**, previously granted energy — removed / real→grant+rotate), and `OnDefenseResolved` gains a Reality branch **before** the generic `!CanAbsorbElement` return that drains the would-be-gain (`CalculateAbsorptionEnergy`, perfect-doubles). Generic-resolved-to-Reality arrives as `AttackElement == Reality` → covered. Updated *Absorption System*. Player-facing docs: `docs/Mechanics/Archetypes/{Reality,BrokenDarkness}.md`, `docs/Mechanics/Items.md`. | fix/bd-item-absorption-element |
