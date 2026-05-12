# Codebase Technical Reference — 2026-05-11

**Audience:** an engineer joining the project. Skim end-to-end once; afterwards, look up a specific subsystem.
**Scope:** every major subsystem in `Source/world_of_refraction/`. One section each — what it does, where it lives, how it talks to other systems, what doesn't quite make sense conceptually.
**Companion docs:** `Codebase_Analysis_Pass1_Map.md` (smell inventory), `Codebase_Analysis_Pass1_StatusUpdate_2026-05-11.md` (verified deltas), `Codebase_Analysis_Pass2_ApplyConsolidation.md` (closed refactor arc).

**Reading order:** ActionExecutor → CombatOrchestrator → TurnManager → DefenseSystem → DamageCalculator gets you the action pipeline end-to-end. StatusEffectManager + CharacterDataComponent fill in mutation. The rest is supporting cast.

---

## ActionExecutor

**What.** The combat action engine. Validates an `FAction`, opens defense windows for each target, applies damage + status buildup through `ApplyHit`, fires lifecycle delegates. After the Phase A–D consolidation, this is the **only** path for Spell / Ability / Attack actions — sync execution paths for those three types were retired. Item and Defend remain instant-resolution sync paths.

**Where.** `Public/ActionExecutor.h` (~850 LOC), `Private/ActionExecutor.cpp` (~4,130 LOC — largest file in the repo). `UGameInstanceSubsystem`.

**Talks to.**
- **Inbound:** `CombatPlayerController::OnConfirmAction`, `ACombatOrchestrator::SubmitAction/SubmitActionAsync`, `AIDecisionManager` (action submission), `CombatOrchestrator::DebugExecuteAsync*` (editor debug buttons).
- **Outbound:** `UDefenseSystem::OpenDefenseWindow` (per target), `UDamageCalculator::CalculateDamage`, `UStatusEffectManager::AddStatusBuildup` (via `ApplyHit`), `UCharacterDataComponent::ServerTakeDamage/Heal/SpendEnergy`, `URingManager`/`UWeaponManager::ProcessPostCastWear`, `UBrokenDarknessManager` (forbidden-element check, BD-state queries), `UCombatMovementComponent::StartApproach`, `UInfusionVFXComponent`, `UInfusionChargeManager`.
- **Delegates:** `OnActionStarted`, `OnActionCompleted`, `OnDamageDealt`, `OnHealingDone`, `OnTargetKilled`, `OnAsyncActionCompleted`, `OnDefenseWindowRequested`.

**Conceptual flags.**
- **`ApplyCommitCosts` (217 lines)** — single switch over `EInfusionSourceOption`. `ActiveRing` and `PrimaryRing` branches are near-verbatim copies. Reads like several distinct responsibilities (HP cost, crystal wear, evolution self-status) crammed into one place.
- **`ExecuteActionAsync` (151 lines)** — does validation, context construction, Broken Darkness checks, instant-action shortcut, item shortcut, commit costs, ActionMods compute, and movement bind/start. Multiple phases inlined. Hard to test individual phases.
- **`ApplyDamage` / `ProcessMultiHit` / `ApplyDamageAfterDefense`** — kept alive past the Phase D retirement; flagged for Phase E. `ApplyDamage` still exposes a `UFUNCTION` wrapper whose BP bindings are unverified.
- **`ApplyAbilityEffects` orphan** — sync `ExecuteAbility` was its only caller. `FinalizeAsyncAction` (`:1257-1288`) has a Spell post-action branch but **no Ability branch** — so `Ability->Effects[]` (drain, conditional `OnHit`/`OnCrit`/`OnKill`) may not actually fire for any async ability today. Pre-existing gap exposed by Phase D's deletion. Needs PIE confirmation with a populated `Effects[]`.
- **`RollCriticalHit` orphan** at `:1965` — `UDamageCalculator::RollCriticalHit` is the one used; this stub has zero callers.
- **`ApplySpellSizeL2Cost` declaration without body** at `ActionExecutor.h:491`.
- **Magic numbers** — `0.3f` defense-window duration (4 sites, all TODO'd), `InfusionCost = 5` (`:800`, TODO'd), `100.0f` (10 sites — most are %-conversions).
- **UFUNCTION Category typo** — `"Action Execfutor|Infusion"` at `ActionExecutor.h:136`.
- **Dead-after-redirect branch** — in `ExecuteSpellAsync`, `if (Spell->bIsRawMode) { SpellStatusType = BurstDamage; }` at `:619` is immediately overridden to `None` by `ApplyRawModeRedirect`. Harmless but confusing — Phase E sweep candidate.

---

## CombatOrchestrator

**What.** The combat session boss. Owns combat state (`ECombatState`), runs the turn loop, dispatches actions to `ActionExecutor`, processes end-of-turn effects, applies between-combat repair / crystal destruction, and exposes a wall of `Debug*` editor buttons for in-engine testing.

**Where.** `Public/CombatOrchestrator.h` (~459 LOC), `Private/CombatOrchestrator.cpp` (~2,414 LOC). `AActor` placed in the level.

**Talks to.**
- **Inbound:** Editor `Debug*` buttons, `CombatPlayerController` (action submission via `SubmitAction`), `TurnManager` (turn-tick events), `UTurnOrderStripWidget` / `UCombatHUDRoot` (UI bind to its events).
- **Outbound:** `UActionExecutor::ExecuteActionAsync`, `UTurnManager` (init/end), `UStatusEffectManager` (end-of-turn processing, clear-on-combat-end), `UCombatGridSubsystem` (positioning), `UCombatCameraManager` (state transitions), `URingManager`/`UWeaponManager` (between-combat repair/destruction), `UInventoryComponent`/`ULoadoutComponent` (consume used items, reset battle state), `UWeatherStateManager` (init leaders).
- **Delegates:** `OnCombatStateChanged`, `OnActionExecuted`, `OnTurnAdvanced`, `OnPreviewTurnOrderChanged` (and more — ~10 broadcast types).

**Conceptual flags.**
- **22+ `Debug*` methods in production class** — Pass 1 flagged this; Phase D shrank the three biggest from sync to async but the bulk of debug methods still live here. Project convention is to extract to `*Debug.cpp` per system.
- **`SubmitAction` (316) vs `SubmitActionAsync` (412)** — duplicate setup, semantics differ in how they branch per action type. Unclear why two entry points exist; needs clarification.
- **`ApplyBetweenCombatCrystalDestruction` (97 lines) + `ApplyBetweenCombatRepair` (103 lines)** — parallel structures iterating combatants and walking loadout entries. Mild duplication.
- **Two-state design (orchestrator + turn manager)** — `CombatState` (Idle/Preparing/InProgress/Victory/Defeat) lives here; `TurnManager` tracks turn-order state independently. Some queries hit both. Single-source-of-truth question for "is combat live."
- **`OnActionCompleted` (65 lines)** — turn-end orchestration with state change, win check, AI handoff, advance all inlined.

---

## TurnManager

**What.** Debt-based turn-order subsystem. All combatants accrue turn debt each round proportional to their speed; the actor with the highest net debt goes next. Handles speed changes (via stat buffs), team-aware queries, and tie-breaking.

**Where.** `Public/TurnManager.h` (~198 LOC), `Private/TurnManager.cpp` (~423 LOC). `UGameInstanceSubsystem`.

**Talks to.**
- **Inbound:** `CombatOrchestrator::InitializeCombat`, `UStatusEffectManager` (speed-change events), AI deciders.
- **Outbound:** None directly — listeners subscribe to its delegates.
- **Delegates:** `OnTurnStarted`, `OnTurnEnded`, `OnCombatEnded`, `OnPreviewTurnOrderChanged`.

**Conceptual flags.**
- **`PreviewTurnOrder` reimplements `GetNextCombatant`'s scan** — two parallel implementations of the debt-pick algorithm. Drift risk if balance changes hit one but not the other.
- **Tie-breaker priority undocumented** — `ShouldBreakTieInFavor()` uses cached `Speed`, `ActionSpeed`, `Mind`, `Body`, `Spirit` but the priority order isn't in the header. Reading the implementation is the only way to know.
- **Stale comment at `:234`** — `// Level 2: AttackSpeed (higher wins)`. Field was renamed `AttackSpeed → MovementSpeed → ActionSpeed`. Pass 1 flagged; unchanged.
- **`GlobalTurnCount`** — visible property; no accessor. Likely meant for listeners; the missing getter is a tell.

---

## DefenseSystem

**What.** Real-time defense window mechanic. `ActionExecutor` calls `OpenDefenseWindow` per target; the defender (player or AI) has a short timed window to Block / Parry / Dodge. The system resolves the result and fires `OnDefenseWindowClosed`, which `ActionExecutor` then routes through `ApplyDamageAfterDefense` → `ApplyHit`.

**Where.** `Public/DefenseSystem.h` (~341 LOC), `Private/DefenseSystem.cpp` (~485 LOC). `UGameInstanceSubsystem`.

**Talks to.**
- **Inbound:** `UActionExecutor::OpenDefenseWindowsForTargets`, defense input handlers (player) and `UAIDecisionManager::ScheduleDefenseDecision` (AI).
- **Outbound:** None — defense outcome is broadcast via delegate; ActionExecutor binds to it.
- **Delegates:** `OnDefenseWindowOpened`, `OnDefenseWindowClosed`, `OnDefenseInputReceived`, `OnDefenseCueTriggered`, `OnParryReflect`.

**Conceptual flags.**
- **Tuning is `UPROPERTY`s on a stateless subsystem** — same architectural question as `DamageCalculator`. Block/Parry reduction %, Dodge threshold, reflect %, window duration all live as runtime fields. A config asset might be more appropriate.
- **Dodge threshold reads `AttackSize` from `FCharacterData::GetDodgeThreshold()`** — late-bound. Suggests dodge originally binary; size-aware version layered on.
- **Manual close + timer auto-close** — both paths exist. Race condition risk if caller closes while timer is firing. Not verified in this pass.

---

## DamageCalculator

**What.** Pure damage math subsystem. Given `FDamageCalculationInput` (attacker, target, base damage, element, action mods, stat snapshot), returns `FDamageCalculationResult` with crit roll, defense application, resistance, BD-stack multiplier, grid modifier, and element interaction.

**Where.** `Public/DamageCalculator.h` (~349 LOC), `Private/DamageCalculator.cpp` (~628 LOC). `UGameInstanceSubsystem`.

**Talks to.**
- **Inbound:** `UActionExecutor::ApplyHit` (the unified hit path post Phase A), AI value-estimation (`UAIDecisionManager::EstimateBestDamage`).
- **Outbound:** `UCharacterDataComponent` (HP/stat reads), `UBrokenDarknessManager::GetStackStatusMultiplier`, `UCombatGridSubsystem::GetDamageModifier`, `UStatusEffectManager` (status-mod queries).
- **Delegates:** None (pure-function subsystem).

**Conceptual flags.**
- **`CalculateDamage` is 119 lines** — grid lookup, attacker mult, defense, resistance, BD-stack mult, status mods, element interaction, crit roll, status-effect modifier, final assembly all inlined.
- **`CalculateSpellDamage` (`:145`, 47 lines)** — has 2 internal callers (`:312, 449`) but no external callers. Effectively dead from the action-pipeline side; only used by `CalculateAbilityDamage` and a healing variant. Survives Phase D's sweep but is on the Phase E watch list.
- **`CalculateAbilityDamage` (`:193`, 43 lines)** — zero callers anywhere. Genuinely orphan.
- **27 tuning `UPROPERTY` floats on a stateless subsystem** — config-asset architectural question, not a DRY one.
- **Element weakness stubs** — `IsWeakTo()` and `ResistsElement()` always return false. The 9-element advantage system exists in design docs but is not implemented at the damage-calc layer.
- **Coupling concern** — for a "calculator," it reaches into BrokenDarknessManager, StatusEffectManager, and CombatGridSubsystem. That's three high-level subsystems for a pure-function module. Tight coupling, but each lookup is genuinely needed for the formula.
- **AI value-estimation blind to ActionMods** — convenience wrappers (`CalculateAttackDamage` / `CalculateSpellDamage`) don't populate `Input.ActionMods`. AI score-time damage diverges from runtime when Reality / Evolution / Luck buffs are in play. Pre-existing limitation; **made worse** by Action Data Parity adding `Ability->StatusBuildup` and `bIsRawMode` that AI also doesn't read.

---

## StatusEffectManager

**What.** Owns status-effect application (immediate and threshold-triggered), the status-buildup bar per actor, status-effect ticking (end-of-turn), removal helpers, and immunity gating. Two parallel paths: classic effects (DOT, defense debuff, etc. with duration + magnitude) and the newer buildup-bar (status accumulates until threshold → fires the triggered effect).

**Where.** `Public/StatusEffectManager.h` (~504 LOC), `Private/StatusEffectManager.cpp` (~1,762 LOC — third largest). `UGameInstanceSubsystem`.

**Talks to.**
- **Inbound:** `UActionExecutor::ApplyHit` (buildup adds), `UItemExecutor` (immediate applies + cleanse), `UCombatOrchestrator` (end-of-turn tick, clear-all-on-combat-end), `UStatusEffectComponent` if any.
- **Outbound:** `UCharacterDataComponent::ServerTakeDamage`/`ServerHeal`/`ServerSetEP` (status effect side-effects), `UTurnManager` (speed-change events when an effect modifies speed).
- **Delegates:** `OnEffectApplied`, `OnEffectRemoved`, `OnEffectDurationChanged`, `OnStatusBarBuildupChanged`, `OnStatusBarThresholdTriggered`, `OnImmediateStatusApplied`, etc.

**Conceptual flags.**
- **Two parallel switch tables for the same status enum** — `ApplyImmediateStatus` (78 lines, `:1577`) and `ApplyTriggeredStatus` (107 lines, `:1656`) handle the same `EStatusType` cases (DOT, DefenseDebuff, SpeedDebuff, CritDebuff, EnergyDebuff, RandomDebuff, BurstDamage) with different formula tables. Adding a new status type requires editing both. Magic numbers (10/15/25/30/40/50/100 — durations, percents) embedded in each switch.
- **`IsImmuneToEffectType` stub silently disables the immunity gate** — returns `false` unconditionally at `:1161`; `ApplyEffect:49` calls it. Anything claiming immunity is ignored. Three sibling stubs (`IsStunned`/`IsSilenced`/`IsRooted`) return `false` and have no callers.
- **Status-bar block at `:1394+` is a separate subsystem-within-subsystem** — `FStatusBarState` private struct, `StatusBarStates` map, 6 methods. Deserves its own `.h/.cpp` split per the existing pattern in this codebase.
- **6+ near-identical removal loops** — `RemoveAll{Buffs,Debuffs,DOTs}`, `RemoveEffectByID`, `RemoveEffectsByName`, `RemoveEffectsByType`, `RemoveAllEffects`, `RemoveEffectsBySource`. Same reverse-iterate-and-broadcast pattern, slightly different filter predicates.
- **Magic `100.0f` 4× in `IsTriggerConditionMet`** — HP-percent thresholds.
- **`ApplyImmediateStatus` was intentionally dropped** for spell raw-mode in C1 (the locked design: buildup-bar threshold is the single statusing path). The function lives on for non-spell paths.

---

## CharacterData / CharacterDataComponent

**What.** `UCharacterData` is the immutable data-asset template (identity, class, innate element, sub-stat pool, animation refs). `UCharacterDataComponent` is the per-actor runtime wrapper holding mutable state (current HP/EP, alive/dead, Broken Darkness flag) plus a reference to the template.

**Where.** `Public/CharacterData.h` (~524 LOC, mostly inline calculators), `Private/CharacterData.cpp` (~50 LOC now — used to be 147 before `CalculateEvolutionCost` was removed). `Public/CharacterDataComponent.h` (~210 LOC), `Private/CharacterDataComponent.cpp` (~350 LOC).

**Talks to.**
- **Inbound:** Nearly every combat subsystem reads from it. `UActionExecutor`, `UDamageCalculator`, `UStatusEffectManager`, AI, UI all query `FindComponentByClass<UCharacterDataComponent>()` then walk to `CharacterData` for templated values.
- **Outbound:** Component fires `OnHPChanged`, `OnEPChanged`, `OnDied`, `OnResurrected`.

**Conceptual flags.**
- **Stat-pillar membership hardcoded in 5 places** — sub-stat declarations (`.h:119-155`), total getters (`:273-309`), base sums (`:314-332`), effective scalers (`:333-348`), calculators (`:360-468`). Adding a sub-stat touches all 5 sites. The recent Luck addition went through cleanly but the pattern is fragile.
- **Stat-pillar migration in flight** — comments at `:389/448/475/484` reference pillar moves (`Spirit → Mind`, `Mind → Spirit`, `Spirit → Body`) that are still partly inconsistent across calculators. **Status: in active migration** per `AbilityData.cpp:128` comment "Phase 1; moves to Spirit in Phase 2b."
- **`CalculateMaxHealth` / `CalculateMaxEnergy` stubs on the component** return hardcoded `100` (`:290-306`) even though `UCharacterData` has proper asset-based methods. `InitializeFromTemplate` (`:81-82`) calls the stubs; `BeginPlay` (`:29-32`) calls the correct asset-based path. Inconsistent init paths.
- **Resonator EP-suppression logic duplicated** across `ServerGainEnergy` / `ServerSetEP` (`:144-192`). Same predicate, different early-return shapes.
- **BD dual-path design** — a character can be in Broken Darkness in two ways: (a) `InnateElement == BrokenDarkness` at template level, or (b) runtime transformation via `bIsBrokenDarkness` flag. `IsBrokenDarkness()` helper unifies the two; risk of asset/runtime desync if serialization touches one.
- **Replicated component, only 5 properties** — could optimise Max values to compute from template ref rather than store. Bandwidth tradeoff.
- **Element weakness/resistance stubs** — `IsWeakTo()` returns false everywhere. Same stubs replicated on `DamageCalculator` and `BrokenDarknessManager`. **Triple-stub for the same unimplemented design.**

---

## DefenseSystem (covered above with combat pipeline)

See entry above.

---

## BrokenDarknessManager

**What.** Per-character component holding the Caster's special "Broken Darkness" mechanic. Tracks absorption energy gained via parry/block, manages stacks that multiply status buildup, handles hybrid spells (Darkness + absorbed element), overload state ticks, and forbidden-element self-damage when BD characters cast Dark Light / Dark Void.

**Where.** `Public/BrokenDarknessManager.h` (~360 LOC), `Private/BrokenDarknessManager.cpp` (~704 LOC). `UActorComponent`.

**Talks to.**
- **Inbound:** `UActionExecutor::CheckBrokenDarknessBreak` (per-action transformation roll), `UDefenseSystem` (parry/block success notification), `UActionExecutor::ProcessForbiddenElementCast` (self-damage).
- **Outbound:** `UCharacterDataComponent::ServerSetBrokenDarkness` (transformation), `UCharacterDataComponent::ServerTakeDamage` (forbidden self-damage), `UStatusEffectManager` (stack-multiplied buildup queries).
- **Delegates:** `OnTransformed`, `OnEnergyAbsorbed`, `OnOverloadStateChanged`, `OnStacksChanged`, `OnAlignmentChanged`, `OnOverloadDamage`.

**Conceptual flags.**
- **Naming overload** — `RollForBreak()` is the *transformation* roll (cast → "break into BD"), not durability "break." `IsTransformed()` is the runtime state check. Within the file they're consistent, but the BD codebase intersects with crystal-break code elsewhere; cross-system reader can get confused.
- **Transformation chance tier-keyed inline** — S=1.5%, A=1.0%, etc. + infusion multiplier. Documented in inline comments, not a constants file.
- **`ForbiddenCastSelfDamagePercent = 25%`** hardcoded. Edge-case interaction: if overloaded, absorption swallows the self-damage. Worth design-doc'ing.
- **`ProcessOverloadTick` callers must pre-compute** `StatusMultiplierBonus` and `EfficiencyPercent` — fragile. Could be internalised.

---

## CombatGridSubsystem

**What.** Per-team 3×3 grid manager. Assigns combatants to grid positions, applies row-based damage/defense modifiers (front +5%, middle 0%, back −5%), handles position-mutation operations (push back, pull forward, swap), and provides world-space placement for actor spawning.

**Where.** `Public/CombatGridSubsystem.h` (~263 LOC), `Private/CombatGridSubsystem.cpp` (~744 LOC). `UGameInstanceSubsystem`.

**Talks to.**
- **Inbound:** `UCombatOrchestrator::StartCombat` (assignment), `UDamageCalculator` (row modifier reads), `UActionExecutor::FilterValidTargets` indirectly via targeting helpers, AI decision helpers (target selection), UI (debug draw).
- **Outbound:** None (queries only — pure data subsystem with mutators).
- **Delegates:** None visible at the subsystem level.

**Conceptual flags.**
- **Many `BlueprintCallable` methods with no C++ callers** — `PushActorBack`, `PullActorForward`, `SwapActorPositions`, `MoveActorToRow`, `GetTeamActors`, `GetActorsInRow`, `GetTeamCount`, `RemoveFromGrid`, `GetActorAtPosition`, `IsPositionOccupied`. Likely Blueprint-invoked; needs an editor audit to confirm.
- **Magic modifiers** — front/middle/back ±5% in code comments, not a constants table. Hard to tune.
- **Auto-assign naive** — `AutoAssignTeam()` fills columns left-to-right, one per row. No formation strategy support. Callers must manually assign for custom layouts.
- **Position-key hashing** — `GetPositionKey()` converts grid coords to int32 for map lookups. Implementation isn't visible from the header; collision risk if naive.

---

## WeatherStateManager

**What.** Tracks each team's "leader" — the combatant with the highest world-stat total — and exposes their element. Designed to drive a weather-based damage/defense climate effect, though the climate-lookup itself appears to live elsewhere (or is unimplemented).

**Where.** `Public/WeatherStateManager.h`, `Private/WeatherStateManager.cpp` (sizes not re-verified; Pass 1 didn't flag bulk).

**Talks to.**
- **Inbound:** `UCombatOrchestrator::InitializeCombat` (init leaders), death-event subscribers (`OnTeam0LeaderDied`, `OnTeam1LeaderDied`).
- **Outbound:** None directly.
- **Delegates:** `OnWeatherChanged` (Team0DA, Team1DA, BlendValue).

**Conceptual flags.**
- **Public API incomplete** — exposes leader queries but no "get current weather climate" method. Callers must walk leader → element → look up weather DA externally. The "weather" concept lives only by convention.
- **Team0/Team1 mirror code** — `OnTeam0LeaderDied` and `OnTeam1LeaderDied` (`:173-195`) are byte-identical except for "0"/"1" string. Same shape in `EndCombat` (`:43-58`).
- **Dead public API** — `GetLeaderElement`, `GetTeam0Leader`, `GetTeam1Leader` have no callers (Pass 1; unchanged).
- **British spelling** — `Initialise` here vs `Initialize` everywhere else. Minor consistency issue.

---

## LoadoutComponent

**What.** Per-character "what you have equipped" manager — primary/secondary weapons, primary/secondary rings, evolution crystals, innate spells, item-slot mapping, capacity validation, between-battle state reset. Holds the rules for class-conditional slot eligibility (Generic dual-wield, Caster crystal slots, Resonator ring loadouts).

**Where.** `Public/LoadoutComponent.h` (~425 LOC), `Private/LoadoutComponent.cpp` (~1,732 LOC — fourth largest). `UActorComponent`.

**Talks to.**
- **Inbound:** Loadout-edit UI, `UCombatOrchestrator::ConsumeAllUsedItems` + `ResetBattleState`, `UInventoryComponent` (cross-reference for capacity), `UWeaponManager` / `URingManager` (active-weapon / active-ring queries), AI (capability discovery).
- **Outbound:** `UInventoryComponent::UseItem` (consume), validation feedback to UI.
- **Delegates:** Several change broadcasts; not enumerated here.

**Conceptual flags.**
- **`GetValidationErrors` (209 lines)** — 7 distinct validation passes (primary weapon, primary ring, primary evolution, secondary, ring loadout, innate spells, items + duplicates) in one method. Hard to test individual rules.
- **Orphan brace block bug at `:487-495`** — see Status Update Bug #3. Every Resonator validation now reports two slot-capacity errors instead of one.
- **Validation triplication** — same rules appear in `ULoadoutData::GetValidationErrors` (148 lines) and `FCombatLoadout::Validate*`. Three validators with partial overlap. Change a rule, touch three files.
- **`CreateAndConfigureLoadout` (132 lines)** — triple-loop over spells/abilities/items with near-identical null/has/log patterns.
- **`GetActiveWeapon` (54 lines)** — class-conditional 5-return method. Generic dual-wield, Caster, Resonator, Evolution paths all inlined.
- **`CreateAndConfigureLoadout`, `DuplicateLoadout`, `RenameLoadout`, `DeleteLoadout`** — no C++ callers visible. Likely Blueprint-callable; needs editor audit.

---

## AIDecisionManager

**What.** AI brain. Picks the next action for an AI-controlled actor: scores attacks/spells/abilities/items by estimated damage and threat, decides infusion level, decides defense reaction (block/parry/dodge timing), schedules thinking delays per difficulty. Difficulty levels modulate accuracy of both attack picks and defense reactions.

**Where.** `Public/AIDecisionManager.h` (~202 LOC), `Private/AIDecisionManager.cpp` (~1,548 LOC — fifth largest). `UGameInstanceSubsystem`.

**Talks to.**
- **Inbound:** `UCombatOrchestrator` (it's our turn — pick something), `UDefenseSystem::OnDefenseWindowOpened` (defense reaction).
- **Outbound:** `UActionExecutor::ExecuteActionAsync` (action submission), `UDefenseSystem::SubmitDefenseInput` (defense input), `UDamageCalculator` (value-estimation reads), `UStatusEffectManager` (threshold queries), `UCharacterDataComponent` (HP/EP/threat reads).
- **Delegates:** None — it consumes events and submits via subsystem APIs.

**Conceptual flags.**
- **`BuildOffensiveAction` (223 lines, largest method in the repo)** — nested switch/score loops over Attack/Spell/Ability with damage estimation, then a second nearly-identical switch picks the winner. Helpers `EstimateBestDamage` / `ScoreTarget` exist but the method re-does the per-action-type score loop locally.
- **`DecideSpellInfusionLevel` / `DecideAbilityInfusionLevel` near-byte-identical** (~80% duplication). Differ only in `Spell->CalculateDamage` vs `Ability->CalculateDamage` and the StatusBuildup source.
- **AI value-estimation diverges from runtime** — see DamageCalculator flag. Now **worse**: `AbilityData::StatusBuildup` and `bIsRawMode` are new fields runtime reads, but AIDecisionManager still calls `Ability->CalculateStatusBuildup()` (the stat-derived constant-formula). AI evaluates a different version of the action than the game runs.
- **Three difficulty ladders** — `GetThinkingDelayRange`, `GetDefenseAttemptChance`, `GetDefenseAccuracy` — same skeleton, three duplicates.
- **`FindHealingItem` / `FindCleanseItem` / `FindEnergyItem`** — three near-identical loops differing only in `ECrystalType` literal.
- **Threat multiplier copy-paste look** — `CalculateThreatLevel` uses both `EFFECT_DAMAGE_THREAT_MULT` and `SPELL_POWER_THREAT_MULT` against the same `GetTotalEffectDamage`. Suspicious.
- **Dead helpers** — `GetCurrentEP`, `GetMaxEP`, `GetCharacterData` (`:1503-1549`). Added but never wired.
- **Magic numbers** — `1.3f` (L2 dmg mult), `1.5f` (L1 buildup mult), `0.70f` (status threshold), `0.7f` / `0.4f` (parry literals), fallback damage `50`. Same constants in named form elsewhere.

---

## CombatCommandMenuSubsystem (UI cluster)

**What.** Drives the radial/pie command menu UI. Builds buttons per available action category (Attack / Refractions / Abilities / Items / Defend / Switch / Flee), routes selection through a state machine, handles target selection sub-flow (single, group, AOE), and stamps infusion choices onto the outgoing `FAction`.

**Where.** `Private/UI/Combat/CombatCommandMenuSubsystem.cpp` (~1,479 LOC). `UGameInstanceSubsystem`.

**Talks to.**
- **Inbound:** Player input events from `UCombatCommandMenuWidget`, `UCombatPlayerController`.
- **Outbound:** `UActionExecutor` (final action submission), `ULoadoutComponent` (capability discovery), `UCombatOrchestrator` (submit through the orchestrator's `SubmitAction`).
- **Delegates:** UI-bound; broadcasts button-state changes.

**Conceptual flags.**
- **`BuildTargetButtons` and `BuildGroupTargetButtons` share a byte-identical 71-line infusion-controls block** — at `:949-1019` and `:1080-1149`. Comment even says "Same block as BuildTargetButtons." Two copies that must not drift.
- **Source-label mapping in 4 places** — `OpenTargetSelection:797-816`, `BuildTargetButtons:970-989`, `BuildGroupTargetButtons:1100-1118`, `MapCategoryToSpellSource:1296`. Four implementations of the same Innate / WeaponCrystal / Ring / Evolution → label mapping.
- **`HandleSelection` (145 lines)** — 14-case switch with two cases (`CycleSource`, `CycleLevel`) sharing post-actions.
- **Heavy verbose logging in production path** — `OpenTargetSelection` has 5+ verbose log calls. Output Log noise during normal play.

---

## ItemExecutor

**What.** Sub-executor for item actions. Knows how to apply each crystal item type (Garnet→damage, Sapphire→heal, Citrine→energy, Emerald→buff, Amber→buff, Opal→buff, Onyx→debuff, Amethyst→gamble, Iolite→cleanse with tiered removal, Quartz→absorb-and-transform). Class-conditional secondary effects (Generic gets resistance bonus, BD gets energy bonus on use).

**Where.** `Public/ItemExecutor.h` (~331 LOC), `Private/ItemExecutor.cpp` (~741 LOC). `UGameInstanceSubsystem`.

**Talks to.**
- **Inbound:** `UActionExecutor::ExecuteItemAsync` (delegates here), `UAIDecisionManager` (item-finding helpers).
- **Outbound:** `UCharacterDataComponent` (HP/EP changes), `UStatusEffectManager` (buff/debuff/cleanse), `UBrokenDarknessManager` (Quartz absorption tracking).
- **Delegates:** `OnItemUsed`, `OnQuartzTransformed`, `OnGambleResult`.

**Conceptual flags.**
- **`CanQuartzTransform` is named like a predicate, 66 lines** — likely also performs the transformation side-effect. Name-vs-behaviour mismatch.
- **10 `Execute*Effect` handlers share the same shape** — `GetCharacterDataComponent → null guard → operation → result fields` repeated each time.
- **Quartz transformation needs manual call** — `TransformQuartz()` is a separate subsystem method; nothing auto-triggers when threshold crossed. Risk of drift if damage flow forgets `NotifyQuartzDamage`.
- **Amethyst gamble — no visible odds in header** — caller can't predict outcome without reading the cpp.

---

## WeaponManager

**What.** Tracks active weapon per actor (primary/secondary slot, conjured weapons for Caster), handles weapon switching, infusion toggle (Generic only), per-spell durability wear, post-attack physical-status buildup mapping (Slash→Bleed, Pierce→ArmorBreak, Impact→Stun). Conjuration locks spells (Caster mechanic — unwired).

**Where.** `Public/WeaponManager.h` (~489 LOC), `Private/WeaponManager.cpp` (~888 LOC). `UGameInstanceSubsystem`. **Note:** dropped from 1,199 → 888 LOC in Phase C2 (ExecuteAttack chain deletion).

**Talks to.**
- **Inbound:** `UActionExecutor::ExecuteAttackAsync` (active-attack lookup, post-cast wear), `UInventoryComponent` (weapon-state init), `ULoadoutComponent` (active-weapon queries), `UCombatOrchestrator::ApplyBetweenCombatRepair`.
- **Outbound:** `UStatusEffectManager::AddStatusBuildup` (now goes through `ApplyHit`, indirectly), durability-event broadcasts.
- **Delegates:** `OnWeaponSwitched`, `OnInfusionToggled`, `OnPhysicalStatusTriggered`, `OnConjurationStarted/Ended`. **Note:** `OnWeaponAttackExecuted` exists but is declared+stored, never broadcast, never bound — orphan delegate.

**Conceptual flags.**
- **`FWeaponAttackResult` + `FOnWeaponAttackExecuted` + `OnWeaponAttackExecuted` field — orphan triad** — flagged in Phase C2 for deletion. Survives.
- **`ConjureWeapon` / `DispelConjuredWeapon` / `HasConjuredWeapon` / `AreSpellsSealed`** — Caster mechanic, no callers. Planned-but-unwired.
- **`STATUS_THRESHOLD_BLEED / ARMOR_BREAK / STUN = 100.0f`** — three constants, same value, no per-character scaling.
- **Crystal-subscription bookkeeping mirrors RingManager** — `SubscribeToActorWeaponCrystals` / `UnsubscribeFromActorWeaponCrystals` / `HandleWeaponCrystalBroken` / `FindOwnerOfCrystal` — same shape as `RingManager`'s versions. Cross-subsystem helper candidate.
- **Class checks duplicated** — `IsGenericCharacter()`, `IsCasterCharacter()`, `IsResonatorCharacter()` re-implemented vs the same logic in `ItemExecutor`.
- **Historical-note comment block** at `:389-395` still references `UActionExecutor::ExecuteAttack` (now deleted). Mild stale-comment; deferred to Phase E sweep per project conventions.

---

## RingManager

**What.** Tracks active ring per actor (Resonator class), handles post-cast durability wear on ring crystals, auto-switches to next non-broken ring when active crystal breaks.

**Where.** `Public/RingManager.h` (~128 LOC), `Private/RingManager.cpp` (~470 LOC). `UGameInstanceSubsystem`.

**Talks to.**
- **Inbound:** `UActionExecutor::ApplyCommitCosts` (ring crystal wear at commit-time), `UActionExecutor::ProcessPostCastBySource`, `UInventoryComponent` (ring init).
- **Outbound:** Durability events to UI.
- **Delegates:** `OnRingCrystalBroken`, `OnRingDurabilityChanged`.

**Conceptual flags.**
- **Crystal subscription mirrors WeaponManager** — see WeaponManager note. Cross-subsystem helper candidate.
- **`ProcessPostCastWear` signature missing Element** — takes `EItemTier` + `InfusionLevel` + `bIsSpell` but not the spell's element. If wear should scale with element, can't.
- **Auto-switch on all-broken** — when all rings broken, behaviour unclear from the header. Edge case for Resonator endgame.
- **`GetEvolutionMaxSpells()` static, identical to `GetMaxSpells()`, zero callers** — confirmed dead.

---

## InventoryComponent

**What.** Per-character inventory storage. Holds owned spells, abilities, weapons, rings, item crystals. Tracks crystal attachment points (which crystal is in which weapon/ring), evolution crystals, and capacity. Separate from `LoadoutComponent` — inventory is "what you own"; loadout is "what's equipped."

**Where.** `Public/InventoryComponent.h` (~244 LOC), `Private/InventoryComponent.cpp` (~500 LOC). `UActorComponent`.

**Talks to.**
- **Inbound:** Inventory-edit UI, `ULoadoutComponent` (capacity cross-check), `UItemExecutor::UseItem` (consume), `UCombatOrchestrator::ConsumeAllUsedItems`.
- **Outbound:** None visible.
- **Delegates:** None visible.

**Conceptual flags.**
- **`AttachCrystalToWeapon` / `AttachCrystalToRing` and `ApplyEvolutionToWeapon` / `ApplyEvolutionToRing` are structural near-duplicates** — pairwise mirrors.
- **Crystal slotted state lives on the weapon/ring data asset, not on the inventory entry** — `WeaponData::SlottedCrystal` is the source of truth. There's an old comment at `ActionExecutor.cpp:2865-2868` flagging this as "legacy path that diverges from runtime." Architectural drift; not resolved.
- **No persistence layer visible** — clear data and operations exist, but no serialize/deserialize. Save-game story incomplete.
- **Hardcoded max 50 spells/abilities** — comment-documented, not constants.

---

## CombatCameraManager

**What.** Switches camera state during combat — Home (overview), Character (turn-start close-up), Selection (target pick), Action (attack execution with action-phase sub-states). Manages blend times and camera positioning. Camera actors are placed in-level.

**Where.** `Public/CombatCameraManager.h` (~239 LOC), `Private/CombatCameraManager.cpp` (~641 LOC). `AActor`.

**Talks to.**
- **Inbound:** Subscribes to `UTurnManager::OnTurnStarted`, `UActionExecutor::OnActionStarted/Completed`, `CombatCommandMenuSubsystem` target-selection events.
- **Outbound:** None directly (just moves the camera).

**Conceptual flags.**
- **Two state enums in one manager** — `ECombatCameraState` (Home/Character/Selection/Action) and `EActionCameraPhase` (Approach/Recovery/...). Coupling between them is tight; reading transitions is hard.
- **Dynamic camera spawn on demand** — if a placed camera isn't found, the manager creates one. Cleanup on `EndCombat` not verified.
- **8 blend times + 4 positioning parameters** — granular but undocumented. Artists need a tuning guide.
- **TODOs** — `:77` ("Bind to ActionExecutor events when available") and `:466` ("Get target from action and transition to Action camera"). Incomplete integration.

---

## CombatPlayerController

**What.** Player input during combat. Maps Enhanced Input actions to charge-start / charge-trigger / charge-complete (held button charges infusion level), cycle-source (rotate through available infusion sources), confirm/cancel. Pushes input into the relevant subsystems.

**Where.** `Public/CombatPlayerController.h` (~123 LOC), `Private/CombatPlayerController.cpp` (~313 LOC). `APlayerController`.

**Talks to.**
- **Inbound:** Engine input.
- **Outbound:** `UInfusionChargeManager` (charge state), `UCombatCommandMenuSubsystem` (selection input), `UActionExecutor::ExecuteActionAsync` (confirm).
- **Delegates:** None — pure input handler.

**Conceptual flags.**
- **Test-data bloat** — `TestSpell`, `TestAbility`, `TestTarget`, `TestChargeType` `UPROPERTY`s on a shipped controller, plus "Target self for testing" comments. Pass 1 flagged; unchanged. Either clean up or relocate to a test path.
- **Only class to mention `ACombatPlayerController` in source is itself** — possibly debug-only.

---

## CombatMovementComponent

**What.** Per-character "move toward target before action" handler. Approaches the target up to the action's `ExecutionRange`, plays optional approach montage, broadcasts completion when within range. Returns the actor to home after action.

**Where.** `Public/CombatMovementComponent.h` (~220 LOC), `Private/CombatMovementComponent.cpp` (~554 LOC). `UActorComponent`.

**Talks to.**
- **Inbound:** `UActionExecutor::BindMovementComplete` then `StartApproach`.
- **Outbound:** Broadcasts `OnMovementComplete`.

**Conceptual flags.**
- **`StartApproach` (127 lines)** — target resolution, range calc, state setup, montage decisions, VFX hooks all inlined.
- **TODOs at `:157, 168`** — VFX/SFX integration.

---

## InfusionChargeManager / InfusionVFXComponent / InfusionCostHelper

**What.**
- `InfusionChargeManager` — runtime state machine for "hold the charge button" → L0/L1/L2 infusion level. Subsystem.
- `InfusionVFXComponent` — VFX swap on element/source change. ActorComponent.
- `InfusionCostHelper` — pure functions for HP-cost lookup by source × infusion level. Static.

**Where.** `Public/Infusion*.h` (various sizes; small/medium).

**Talks to.**
- `InfusionChargeManager` ↔ `CombatPlayerController` (input drives state), `UCombatCommandMenuSubsystem` (read level for action stamping), `InfusionVFXComponent` (visual sync).
- `InfusionCostHelper` is read by `UActionExecutor::ApplyHPCostInternal`.

**Conceptual flags.**
- **`InfusionVFXComponent::SpawnVFX` (64 lines)** — three switch arms (Weapon/Body/Aura) with near-identical `SpawnSystemAttached` calls differing only in socket name.
- **`InfusionVFXComponent` source-cycling helpers (`CacheAvailableSources`, `CycleToNextSource`, `ActivateCurrentSource`, `GetCurrentSourceName`)** — look mis-located. Reads like loadout/source-manager state on a VFX component. Mixed concerns.
- **Tick literals** `1.0f / 60.0f` (`:75, :383`) — should be a named tick constant.
- **`InfusionCostHelper`** — exemplary single-responsibility. No issues.

---

## Data assets — USpellData / UAbilityData / UWeaponAttackData

**What.** Designer-tunable immutable data. Each describes one "thing you can do" — a spell, an ability, an attack. Stat formulas live on `UCharacterData`; these assets carry per-action constants (base damage, hit count, energy cost, element, raw mode, status buildup, immune-to-infusion). All three now carry parity fields for `bIsRawMode`, `StatusBuildup`, and `bImmuneToInfusion` post Action Data Parity Commits 1–3.

**Where.** `Public/SpellData.h` (~269 LOC), `Public/AbilityData.h` (~221 LOC + new fields), `Public/WeaponAttackData.h` (~113 LOC + new fields). All `UPrimaryDataAsset`.

**Conceptual flags.**
- **`UAbilityData::StatusBuildup` is unwired in `AIDecisionManager`** — runtime gained the field; AI still calls the constant-formula `CalculateStatusBuildup()`. AI value-estimation diverges from runtime.
- **`UAbilityData::CalculateStatusBuildup()` ignores the new field** — uses `CombatConstants::BASE_STATUS_BUILDUP_PER_HIT * Character->CalculateStatusMultiplier() * HitCount`. This is the version AI uses; runtime (in raw mode) reads `Ability->StatusBuildup`. **Two formulas for ability buildup.**
- **`Ability->Effects[]` may not process for async ability actions** — see ActionExecutor flag. Possible production regression depending on whether `Effects[]` is populated.
- **`USpellData::BaseSize` + `HitboxRatio`** — fields use "size" naming after `SpellSize → SpellSpeed` rename. Verify whether these are legitimately runtime VFX scaling or stale.
- **Inverted-logic bug pair in `UItemData`** (`GetEvolutionTypeName`, `GetEvolutionStatSummary`) — see Status Update Bug #4. Two functions, still inverted.
- **`UWeaponAttackData::GetBuildupStatusType()`** — physical-damage-type → status-type mapping. Slash → DOT (Bleed), but the Slash fallthrough in `FStatusEffect::CreateFromPhysicalDamageType` (Bug #2) means Slash actually produces Armor Break.

---

## UI cluster — Combat HUD, Durability headers, Command Menu, Character Panel

**What.** Health/energy bars, status bar, turn-order strip, defense prompt, command menu (radial/pie), character panel (buffs/debuffs), durability headers (weapon + ring), turn-order slot widgets.

**Where.** `Private/UI/Combat/*` and `Public/UI/Combat/*`.

**Talks to.** Inbound from every gameplay subsystem (HP/EP changes, turn events, action events, status events, defense windows). Outbound to player input.

**Conceptual flags.**
- **TransBuffer-crash exposure** — `CombatHUDRoot::InitialiseForCombat` (`:36-67`) creates `UCharacterPanelWidget` instances dynamically inside `PlayerTeamContainer` / `EnemyTeamContainer`. No postmortem-cite comment. Pass 1 flagged; unverified.
- **`DurabilityHeaderWidget::RefreshForActor` (135 lines)** — resource detection, manager binding, slot dispatch, visibility flip all inlined. Plus slot-updater triplet (`UpdateSlot1FromRing`, `UpdateSlot1FromWeapon`, `UpdateSlot2FromWeapon`) — three near-identical 25-line methods.
- **Ring-resource detection lambda redefined verbatim** at `RefreshForActor:86-92` and `HandleWeaponDurabilityChanged:363-369`. Two copies of the same loadout-scan.
- **`CharacterPanelWidget` delegate handlers byte-identical** — `HandleEffectApplied`, `HandleEffectRemoved`, `HandleEffectDurationChanged` (`:255-280`) all do `if (Target != BoundActor.Get()) return; RefreshBuffDebuffList();`.
- **`DefensePromptWidget` is a stub** — no-op shell with 6 TODOs. Feature-completion target, not refactor target.
- **`CombatCommandMenuSubsystem`** — see dedicated section above.

---

## Cross-cutting concerns

### Element-weakness system is stubbed across three places

`UCharacterData::IsWeakTo()`, `UDamageCalculator::IsWeakTo()`, `UBrokenDarknessManager` element-check helpers all return `false`. The 9-element advantage system exists in design docs and on data assets, but the runtime path doesn't read it. **Triple-stub.**

### AI is increasingly out of sync with runtime

Three accumulating gaps:
1. AI doesn't read `ActionMods` in damage previews (pre-existing).
2. AI doesn't read `Ability->StatusBuildup` (added in Commit 1; AI still calls the constant formula).
3. AI doesn't read `bIsRawMode` (added in Commit 1; AI's value-estimation ignores raw-mode redirect).

Combined, AI evaluates a version of each action that no longer matches what runtime executes.

### "Phase E sweep" carries multiple Phase D-D leftover items

Inline comments in `ActionExecutor.cpp` and the Phase D commit message flag:
- `ApplyDamage` UFUNCTION wrapper (BP-binding audit pending).
- `ProcessMultiHit` — kept; "fold inline or keep as helper" decision pending.
- Multi-hit precision-loss bug (audit risk #3 from Pass-2 doc).
- `UDamageCalculator::CalculateSpellDamage` / `CalculateAbilityDamage` orphans (Phase D's recon confirmed; Phase E to delete).
- Dead-after-redirect `SpellStatusType = BurstDamage` branch.
- `RollCriticalHit` orphan in ActionExecutor.
- `ApplySpellSizeL2Cost` declaration without body.
- `FWeaponAttackResult` triad in WeaponManager.

### Validation logic in 3 files for the same rules

`ULoadoutComponent::GetValidationErrors` (209 lines), `ULoadoutData::GetValidationErrors` (148 lines), `FCombatLoadout::Validate*` (assorted). Partial overlap. TODOs in `FCombatLoadout` say "validate when component exists" — implies an intent to consolidate that hasn't shipped.

### Magic-number disposal record

| Constant | Before | Now | Notes |
|---|---:|---:|---|
| `0.7f` element penalty (3 sites) | 3 | 0 | Closed by Phase D — sites were inside deleted sync paths. |
| `100.0f` in ActionExecutor | 13 | 10 | Reduced by Phase D. Mostly %-conversions; less harmful. |
| `0.3f` defense window | 4 | 4 | Unchanged. All 4 have explicit TODOs. |
| `InfusionCost = 5` | 3 | 1 (verified) | Two of three sites lived in deleted sync paths; the async survivor is at `ExecuteAttackAsync:800` with the same TODO. |
| `STATUS_THRESHOLD_BLEED/ARMOR_BREAK/STUN = 100.0f` | 3 | 3 | Unchanged. |

---

## Reading list, if you're new

If you skim this end-to-end and want to actually understand the action pipeline:

1. **`UAction` and `FAction`** (`ActionStructs.h`) — the data flowing through.
2. **`UActionExecutor::ExecuteActionAsync`** — the entry point.
3. **`UActionExecutor::ExecuteSpellAsync` / `ExecuteAbilityAsync` / `ExecuteAttackAsync`** — the three executors. They prepare `FActionHitInput`, fold raw-mode redirects, then call `OpenDefenseWindowsForTargets`.
4. **`UDefenseSystem::OpenDefenseWindow`** — what happens while the defender reacts.
5. **`UActionExecutor::OnDefenseWindowClosed`** → `ApplyDamageAfterDefense` → `ApplyHit`. The hit lands.
6. **`UDamageCalculator::CalculateDamage`** — what `ApplyHit` calls for the number.
7. **`UStatusEffectManager::AddStatusBuildup`** — what `ApplyHit` calls for the buildup.
8. **`UCombatOrchestrator::OnActionCompleted`** — turn-end, win check, advance.

That's the spine. Everything else hangs off it.
