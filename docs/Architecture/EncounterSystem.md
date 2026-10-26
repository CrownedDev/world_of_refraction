# Encounter System

## Overview
A field encounter starts a fight: a `UEncounterComponent` on an enemy pawn in a trial level opens a short join window when the party touches it, then hands the rosters to `UTrialRunSubsystem::EnterEncounter`, which opens the trial's battle stage. `ABattleGameMode` spawns fresh pawns there and starts combat. Since B-074, the enemy side can be a team authored as data (`UEncounterData`), up to three enemies.

## Architecture
- **`UEncounterData`** (`Public/Combat/Encounter/EncounterData.h`), a `UPrimaryDataAsset`:
  - `Enemies`: the team, in battle-line order.
  - `GetEnemyRoster()`: the team that takes the field, empty entries skipped, at most `EncounterConstants::ENCOUNTER_MAX_ENEMIES` (3, `docs/Design/Enemies.md` §3).
  - `BuildOpposingRoster(Encounter, Fallback)`: the team, or the encounter's own pawn alone when there is no team (Q-074: the team replaces the pawn). The encounter aborts only when this is empty.
  - Debug: `GetEncounterString()`, `PrintEncounter` (CallInEditor).
- **`UEncounterComponent`** (`Public/Combat/Encounter/EncounterComponent.h`): the trigger sphere, join window and arena shell. Per placement it holds `Difficulty`, `Rank` (D-091) and the optional `EncounterData`.
- **`ABattleGameMode`** (`Private/Trial/BattleGameMode.cpp`): spawns the party in a line on one side and every opposing entry in a line facing it (`TEAM_X_OFFSET`, `COMBATANT_Y_SPACING`).

## Integration Points
- `UTrialRunSubsystem::EnterEncounter` stores both rosters as hard references, so they survive the level swap.
- `ACombatOrchestrator::StartCombat` takes both rosters; rank and difficulty are set before the fight starts.

## Known Limitations / TODOs
- Difficulty and rank are set per placement, not per team.
- The battle lines are not centred; a formation per team is not authored.
- B-184 (`AFieldEnemyTeam`, patrols, beaten-stays-beaten) is the full map-side version; it can reuse `UEncounterData`.

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | B-074: `UEncounterData` (a team of up to three); `UEncounterComponent::EncounterData` sends that team to the battle stage instead of the pawn alone. Spec `WoR.Combat.EncounterData`. | feature/B-074-encounter-data |
