# Encounter System

## Overview
A field encounter starts a fight: a `UEncounterComponent` on an enemy pawn in a trial level opens a short join window when the party touches it, then hands the rosters to `UTrialRunSubsystem::EnterEncounter`, which opens the trial's battle stage. `ABattleGameMode` spawns fresh pawns there and starts combat. Since B-074, the enemy side can be a team authored as data (`UEncounterData`), up to three enemies.

## Architecture
- **`UEncounterData`** (`Public/Combat/Encounter/EncounterData.h`), a `UPrimaryDataAsset`:
  - `Enemies`: the team, in battle-line order.
  - `GetEnemyRoster()`: the team that takes the field, empty entries skipped, at most `EncounterConstants::ENCOUNTER_MAX_ENEMIES` (3, `docs/Design/Enemies.md` §3).
  - `BuildOpposingRoster(Encounter, Fallback)`: the team, or the encounter's own pawn alone when there is no team (Q-074: the team replaces the pawn). The encounter aborts only when this is empty.
  - Debug: `GetEncounterString()`, `PrintEncounter` (CallInEditor).
- **`UEncounterComponent`** (`Public/Combat/Encounter/EncounterComponent.h`): the trigger sphere, join window and arena shell. Per placement it holds `Difficulty`, `Rank` (D-091) and the optional `EncounterData`. `GetEncounterId()` (the placed owner's name) is the key a win records. When the window closes, `GatherCaughtEncounters` takes every armed encounter within `ArenaSphereMaxRadius` of the arena centre; `BuildCaughtRoster` makes the fight (Q-184). In `BeginPlay`, an encounter beaten earlier this run destroys its owner.
- **`AFieldEnemyTeam`** (`Public/Combat/Encounter/FieldEnemyTeam.h`, B-184): a placed team on the map: placeholder cylinder, a `UEncounterComponent` (team in its `EncounterData`), optional `PatrolPoints` walked in order and back at `PatrolSpeed` (engine `UInterpToMovementComponent`). Debug `wor.Trial.PrintTeams`.
- **`AEnemyZone`** (`Public/Combat/Encounter/EnemyZone.h`, B-186): a box marker (no collision) with `FEncounterZoneInfo` (`Tier` 0-4, `LootPool` source tag). `FindZoneAt` gives the highest-tier zone containing a point (tier 0 outside); `GetTierStrengthMultiplier` is the named table. `UEncounterComponent` looks it up at the arena centre, `UTrialRunSubsystem::PendingZone` carries it, `ABattleGameMode` multiplies it into the enemy strength. Debug `wor.Trial.PrintZones`.
- **Trial map features** (`Public/Trial/TrialMapFeature.h`, B-188): `ATrialMapFeature` (walk-up actor like the trial door; the exploration controller interacts with the nearest) with `ATrialChest` (rolls `TrialLoot` from its own or its zone's pool, gives it to the player, once per run), `ATrialLoreObject` (placeholder journal: recorded in `UTrialRunSubsystem::RecordFound`) and `ABossArenaTrigger` (Boss-rank fight against the variation's boss, its id goes in `PendingEncounterSources` so a win marks it beaten). `TrialLoot` (`Public/Trial/TrialLoot.h`): asset-registry gather by source tag, seeded pick, grant. Debug `wor.Trial.Pool <tag>`, `wor.Trial.Print` (found list).
- **Beaten for the run:** `UTrialRunSubsystem::SetPendingEncounterSources` / `MarkPendingEncounterBeaten` (from `ABattleGameMode::HandleCombatEnd`, Victory only) / `IsEncounterBeaten`; cleared on trial entry and hub return.
- **Trial gate** (`Public/Trial/TrialGate.h`, B-183): `ETrialDifficulty` (Easy / Medium / Hard) with one named enemy-strength and payout multiplier per level; `ApplyEnemyStrength` gives an enemy persistent max-HP and damage buffs (debuffs below 1) and refills its HP. `UTrialData::Variations` (`FTrialVariation`: name, description, boss). `UTrialRunSubsystem` holds the run's choice (`SetGateChoice`, `GetTrialDifficulty`, `GetChosenVariation`, `GetEnemyStrengthMultiplier`, `GetPayoutMultiplier`), reset in `ExitTrial`; `wor.Trial.Print`. The door opens the placeholder `UTrialGateWidget` (D-024).
- **`ABattleGameMode`** (`Private/Trial/BattleGameMode.cpp`): spawns the party in a line on one side and every opposing entry in a line facing it (`TEAM_X_OFFSET`, `COMBATANT_Y_SPACING`).

## Integration Points
- `UTrialRunSubsystem::EnterEncounter` stores both rosters as hard references, so they survive the level swap.
- `ACombatOrchestrator::StartCombat` takes both rosters; rank and difficulty are set before the fight starts.

## Known Limitations / TODOs
- Difficulty and rank are set per placement, not per team.
- The battle lines are not centred; a formation per team is not authored.
- The payout multiplier is not read yet (no run-end payout; B-073). The variation's boss is not read yet (B-188).
- Patrols are a fixed back-and-forth path; teams don't chase the player. First strike is judged from contact geometry until the field ability exists (GAS).

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | B-188 (Q-188): trial map features: chest (zone loot pool, seeded, once per run), lore object (found list), boss arena trigger (boss fight on interact). Spec `WoR.Trial.MapFeature`. | feature/B-188-map-features |
| 2026-10-26 | B-186 (Q-186): `AEnemyZone` sets the tier (enemy strength) and loot pool of fights started inside it; strength multiplies with the door's difficulty. Spec `WoR.Trial.EnemyZone`. | feature/B-186-enemy-zones |
| 2026-10-26 | B-185 (Q-185): first strike judged at first contact (`JudgeFirstStrike`, `NoticeHalfAngleDegrees`; a walking team faces the way it walks), carried by `UTrialRunSubsystem::PendingFirstStrike` and set on the orchestrator by `ABattleGameMode`. | feature/B-185-first-strike |
| 2026-10-26 | B-184 (Q-184): `AFieldEnemyTeam` (idle or waypoint patrol); teams caught together send their strongest each (max three); a win beats every caught encounter for the run, and a beaten one is gone when the trial level reloads. Spec `WoR.Trial.FieldEnemyTeam`. | feature/B-184-field-enemy-teams |
| 2026-10-26 | B-183 (Q-183): the trial gate. The door asks for a variation and a difficulty (placeholder `UTrialGateWidget`); the run holds the choice; `ABattleGameMode` scales every enemy's max HP and damage by the level before `StartCombat`. `wor.Trial.Print`. Spec `WoR.Trial.Gate`. | feature/B-183-gate-choice |
| 2026-10-26 | B-074: `UEncounterData` (a team of up to three); `UEncounterComponent::EncounterData` sends that team to the battle stage instead of the pawn alone. Spec `WoR.Combat.EncounterData`. | feature/B-074-encounter-data |
