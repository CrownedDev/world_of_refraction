# Boss Phases

## Overview
A boss fight changes as it goes (Enemies §5, D-098, Q-193). A boss has ordered **phases**. Each phase is a list of conditions that must all hold: an HP point, or hidden ones the player discovers. Each phase also lists what changes once it is reached. Every change is optional and authored per boss.

Phases are reached in order and **stay reached**, even if the boss heals back above the line. Damage carries past a phase point, so one big hit can pass several phases. There is no cinematic. Instead, the **weather** pulls toward the boss harder with each phase, and the boss's AI plays harder. Each status break on a boss raises its status resistance for the rest of the fight.

Mini-bosses author no phases: their **style** tag (pillar 8: Planning, Timing, Both) picks a placeholder preset. There is no built-in enrage; a turn count is just one condition among others.

## Architecture
- **`UBossData`** (`Public/Combat/Boss/BossData.h`, a `UPrimaryDataAsset`):
  - `Boss` is the character in the fight. `Style` is an `EBossStyle`. `Phases` is a `TArray<FBossPhase>`.
  - `GetPhasesToUse` returns the authored phases, or the style preset when none are authored.
  - `GetPresetPhases(Style)` returns the placeholder presets (`// TODO: tune`).
  - `GetMemoryAddedPhase` is the last stand a memory adds: HP 15%, full weather.
  - `FindFor(Character)` looks the data up through the asset registry.
  - Debug: `GetBossDataString`, plus a **Print Boss Data** button in the editor.
- **`FBossPhase`**:
  - `PhaseId`.
  - `Conditions` (`FBossPhaseCondition`: `Kind` + `Value`). The kinds are HP at or below a percent, the boss's own turns taken, Broken Darkness absorption maxed, the crystal on its active weapon broken, and status breaks. *InfusionHits* is never met yet (TODO).
  - `PhaseEffects` (`UEffectDefinition`): put on the boss for the fight as gear, so no cleanse strips them. This is where resistance and weakness overrides and stat changes go.
  - `AggressionSteps`, `WeatherIntensity` (0-1).
  - `DialogueId`, `bChangeElement` / `Element` and `AddedSkills` are logged only (TODO).
- **`UBossPhaseComponent`** (`Public/Combat/Boss/BossPhaseComponent.h`): per-fight state on the boss (D-064).
  - `Setup(Data)` copies the phases and binds the boss's `OnHPChanged` and `UTurnManager::OnTurnStarted`. The subsystem is fetched each time, never cached. `EndPlay` unbinds.
  - `EvaluatePhases` reaches every next phase whose conditions all hold. It is guarded against re-entry, because phase effects can change HP.
  - `RecordStatusBreak`, `SkipNextPhase`, `AddPhase`.
  - `GetAggressionSteps` is the sum over reached phases. `GetWeatherIntensity` is the highest over reached phases. `GetStatusResistanceBonus` is 0.1 per break (`// TODO: tune`).
  - `OnBossPhaseReached(Boss, PhaseIndex)`.
  - Phase effect ids: 193000 + phase index, packed per effect (at most 10 per phase).

## Integration Points
- **`ABattleGameMode`**: any opposing combatant whose character has a `UBossData` gets the component. This happens before reactions and memories run.
- **`ReactionTriggers::ApplyMemoryBossModifiers`**: a run memory's SkipPhase calls `SkipNextPhase`, and AddPhase calls `AddPhase(GetMemoryAddedPhase())`.
- **`UAIDecisionManager::GetDifficultyFor`**: the base difficulty plus the boss's aggression steps, Expert at most.
- **`UWeatherStateManager`**:
  - `RefreshWeather` is called when a phase is reached.
  - `RecalculateWeather` pulls the blend to at least `-intensity` (negative = the opposing side) for the alive opposing boss with the strongest pull. That boss's weather asset wins over the opposing leader's.
- **`UStatusBuildupManager`**: a status bar that fills on a boss calls `RecordStatusBreak`. `GetTotalStatusResistance` adds the boss's bonus to the matched layer.
- Debug: `wor.Boss.Print` lists every boss in the level with its phases (reached or not), own turns, breaks, AI steps and weather.

## Known Limitations / TODOs
- Dialogue, element change and added skills are logged only. They need the dialogue system (B-194), a runtime element override and mid-fight enemy skills.
- *InfusionHits* is never met (infusion hits aren't counted per target yet).
- Phase effects always go on the boss; payload targets (the party) are ignored.
- The rising resistance is one blanket step for every bar, not per bar.
- Boss revival is still open (D-098).
- A boss whose class has no weather asset (Generic, Resonator) only moves the blend toward the opposing side's sky; it doesn't bring its own.
- The weather blend override (`FindPhasedBoss`) has no spec yet; the spec checks the component's intensity only (PIE covers the sky).
- A dying boss reaches no phase: the killing blow broadcasts HP 0 before death, so HP 0 is skipped.
- `FindFor` loads every boss data asset each boss fight (fine for a handful).
- Packaged builds: boss data must be in the Asset Manager's rules (the same TODO as `TrialLoot`).

## Changelog
| 2026-11-04 | B-355 (D-144, Q-355): the boss's one rule list. `FBossRule` (`BossRules.h`: `EBossRuleWhen`, a `FWoRConditionSet`, `EBossRuleResult` with NextPhase), `UBossData::Rules`; `BossRules::GetRules` reads old phases (`MakePhaseRule`: conditions + "phases reached = index") and triggers (`MakeTriggerRule`) across. `UBossPhaseComponent::EvaluatePhases` fires state-change rules (authored once per fight, then the phases', repeating while one fires); `BossTriggers::ApplyTo` the fight-start ones; moments still from the old list in `UFightTriggers` (B-355b). Seven `Boss*` condition subjects read `FWoRBossState` (`MakeBossState`). Old phase / trigger condition types OBSOLETE. Spec `WoR.Boss.Rules`. Built and tested on the desktop (`WoR.Boss` 17 green, the 4 `WoR.Boss.Rules` cases included); must-fix was a missing declaration (the cloud branch never built: `BossPhaseComponent.h` now forward-declares `FWoRBossState`); should-fixes applied (one per-fight rule state per boss shared by the fight-start and state-change rules, with a spec; a warning for an authored `AtMoment` rule, which nothing fires until B-355b). | feature/B-355-boss-rules |
| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | B-212 (Q-212): `FBossTrigger::When`; `BossTriggers::ApplyResults` + `FBossFightState` (net steps, skip / add once each); `ApplyTo` fires only fight-start triggers, the rest wait in `UFightTriggers` (`Reaction/FightTriggers.h`), which also listens to `OnBossPhaseReached`. | feature/B-212-mid-fight-triggers |
| 2026-10-27 | B-211 (D-100 part 1, Q-211): boss triggers. `UBossData::Triggers` (`FBossTrigger`: `FBossTriggerCondition` PartyMember / GearWorn / Hidden, `EBossTriggerResult` Weaken / Strengthen / SkipPhase / AddPhase / Dialogue), shown by `wor.Boss.Print`; `BossTriggers::Apply` at fight start after phase setup, steps through `ReactionTriggers::ApplyBossSteps`. Memories no longer skip or add phases (`ApplyMemoryBossModifiers` obsolete, no caller). Spec `WoR.Boss.Triggers`. | feature/B-211-boss-triggers |
| 2026-10-26 | B-193 (Q-193): `UBossData`, `UBossPhaseComponent`, style presets, the battle / memory / AI / weather / status hooks; spec `WoR.Boss.Phases`. | feature/B-193-boss-phases |
| 2026-11-02 | B-263 (P9): `UBossData::FindFor` reads the searchable `Boss` tag from the asset registry and loads only the match; an untagged (older) asset is loaded, and a tag naming a missing or redirected path takes the slow check; a non-boss loads nothing. | refactor/B-263-boss-lookup |
| 2026-11-07 | B-355b (D-144, Q-355b): moment rules fire from the one list. `UFightTriggers::StartBoss` queues `BossRules::GetRules`' AtMoment rules (by value; the `BossDatas` keep-alive is gone); `Fire` calls `BossTriggers::FireMomentRule`, which reads the conditions at the moment (a failing rule keeps waiting); `wor.Boss.Print` lists fight-start and moment rules; the AtMoment warning is gone. `BossInfusionHits` still reads false (needs per-target infusion hit reports). `FBossTrigger` readers flagged for retirement after PIE. Unbuilt (cloud pre-write). | feature/B-355b-boss-moment-rules-cloud |
| 2026-11-09 | B-406: `UBossData::OpeningSequence` and `FBossPhase::CinematicSequence` (soft, empty = none), read by the camera director (`CombatCamera.md`); `UBossPhaseComponent::GetPhaseCinematic` / `GetBossData`; `PrintBossData` names them. | feature/B-406-boss-cinematics-cloud (UNBUILT) |
| 2026-11-09 | B-406b: a boss phase cinematic also holds a ritual's Execution turn (it fires when the cinematic ends), and Space / Menu skips a boss cinematic (`CombatCamera.md`, Q-406b). | feature/B-406b-boss-cinematic-skip-cloud (UNBUILT) |
