# Reaction Triggers

## Overview
One structure for every hidden link between gear and characters (RunStructure §5, D-032): **if these items (all worn) while facing this character → effects, a quest, a dialogue.** Bound gear stories, memory boss changes and later quest hooks all go through it. Checked once at the start of each trial fight.

## Architecture
- **`FReactionTrigger`** (`Public/Reaction/ReactionTrigger.h`): `TriggerId`, `RequiredItems` (weapons / rings, all worn by one party member), `FacingCharacter` (in the opposing team), `Effects` (`FSkillEffect`, the effects condition model: an Enemy payload lands on the faced character, the rest on the wearer; marked gear, so no cleanse strips them), `QuestId`, `DialogueId` (logged only).
- **`UReactionTriggerData`** (`UPrimaryDataAsset`): a list of triggers; any number of these assets, gathered by the asset registry and sorted by path. `PrintTriggers` editor button.
- **`ReactionTriggers`** namespace: `GatherTriggers`, `GetWornItems` (primary and secondary weapon, primary and active ring), `Matches`, `FindFired` (each trigger at most once per fight), `ApplyFired`, `ApplyMemoryBossModifiers` (the run memories naming a boss in the fight add up to one net change: weaken -1 / strengthen +1 step, 15% max HP and damage each, at most 3; then full HP; skip / add phase change the boss's `UBossPhaseComponent`, B-193), `GetTriggerString`.

## Integration Points
- `ABattleGameMode` runs `FindFired` → `ApplyFired` → `ApplyMemoryBossModifiers` after the trial's enemy strength and before `StartCombat`.
- Reads `UMemoryData::BossModifiers` (Memory System) and `UTrialRunSubsystem::GetRunMemories`.
- Debug: `wor.Reaction.Print` (every authored trigger); the battle log line "Reactions: N of M fired".

## Known Limitations / TODOs
- Payload `TargetCount` is ignored (one wearer or one faced character).
- A mid-fight gear swap neither starts nor stops a reaction.
- Quests and dialogue don't exist yet; the memory step is untuned.
- Packaged builds: the trigger assets must be in the Asset Manager's rules (same TODO as `TrialLoot`).

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | B-212 (Q-212): `FReactionTrigger::When` (`FFightMoment`, `Reaction/FightMoment.h`); new per-fight `UFightTriggers` (owned by `ABattleGameMode`) keeps mid-fight reactions and boss triggers and fires each once on its moment (turn start, hit, damage taken, HP line crossed, boss phase); `ApplyBossSteps(..., bBeforeFight)` keeps the HP share mid-fight, a net of 0 removes the change. | feature/B-212-mid-fight-triggers |
| 2026-10-27 | B-211: the weaken / strengthen effect moved into `ReactionTriggers::ApplyBossSteps` (shared with boss triggers, once per boss per fight); `ApplyMemoryBossModifiers` obsolete (no game caller; memories no longer change bosses, D-100). | feature/B-211-boss-triggers |
| 2026-10-26 | B-191 (Q-191): `FReactionTrigger`, `UReactionTriggerData`, the start-of-fight hook, memory boss weaken / strengthen; spec `WoR.Reaction.Trigger`. | feature/B-191-reaction-triggers |
| 2026-10-26 | B-193: memory SkipPhase / AddPhase now change the boss's phases (`UBossPhaseComponent::SkipNextPhase` / `AddPhase`). | feature/B-193-boss-phases |
