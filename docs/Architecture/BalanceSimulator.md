# Balance Simulator

## Overview

Runs seeded, all-AI fights headless and records what happened (turns, rounds, damage, hits-to-kill, win rate), so balance can be checked by numbers instead of by hand. Design and targets: `docs/Design/BalanceSimulator.md`. Test and dev builds only (`WITH_DEV_AUTOMATION_TESTS`); nothing ships.

## Architecture

All under `Source/world_of_refraction/Private/Balance/`:

| Piece | Role |
|---|---|
| `BalanceSimTypes.h` | `FBalanceArchetype` (one combatant: class, pillars, sub-stat points, weapon tier / Power / scaling / requirement, attack and ability base), `FBalanceMatchup` (party archetypes vs an enemy archetype x count, rank, AI difficulty, zone tier, trial difficulty), `FBalanceFightRecord` (one fight; `==` for the replay check, `ToString`), `FBalanceAggregate` (per matchup means and win rate, `ToString`) |
| `BalanceSimRecorder.h` | `UBalanceSimRecorder`: binds the fight's broadcasts and fills one record; `Begin` per fight |
| `BalanceArchetypes.h/.cpp` | the archetype table (`BalanceSimulator.md` §4, pillars clamped to 7, plus `P-GenericC` / `P-GenericS` for the Mid / Late parties) and presets `quick` / `nightly` (18 + 2 matchups: there is no Elite rank) |
| `BalanceSimRunner.h/.cpp` | `FBalanceSimRunner`: `Begin` / `End`, `RunFight(Matchup, Seed)`, `RunMatchup(Matchup, N, BaseSeed)` |

One fight: spawn the archetypes as AI pawns (transient `UCharacterData` / `UWeaponData` / `UAbilityData`, the weapon owned and the ability learned so combat start keeps them), set `wor.Battle.Seed`, apply `TrialGate::ApplyEnemyStrength` (trial difficulty x zone tier, as `BattleGameMode`) and the rank HP stand-in (total HP = strength x rank), `StartCombat`, tick until `OnCombatResultReady` or `FIGHT_TICK_CAP` (= `Stalled`, logged with its seed), then destroy the actors. The world and orchestrator are reused across fights.

Between `Begin` and `End` the runner sets `wor.Combat.InstantImpacts 1` and `wor.AI.ThinkDelayScale 0` (B-291), drops `LogTemp` / `LogWoRCombat` / `LogWoREffects` / `LogWoRAI` to Warning (all set at console priority), and makes a local party with **Let AI finish** (otherwise D-045 hands the fallen party's AI to a player who isn't there). `End` restores the console variables and log levels.

## Integration Points

- Reads: `UActionExecutor::OnDamageDealt`, `OnTargetKilled`; `ACombatOrchestrator::OnActorTurnStarted`, `OnCombatResultReady`; `UTurnManager::OnRoundAdded`; `UStatusBuildupManager::OnStatusTriggered`.
- Uses: `FWoRTestWorld` (`Private/Tests/WoRTestWorld.h`), `TrialGate`, `AEnemyZone::GetTierStrengthMultiplier`, `UPartySessionSubsystem::RestoreLocalParty`.
- Spec: `WoR.Balance.Sim.Runner` (`Private/Tests/BalanceSimSpec.cpp`).

## Known Limitations / TODOs

- Rank HP is the game's own since B-224 (`TrialGate::ApplyEnemyRankHealth`, the `EnemyRankHealth` ladder). `EEncounterRank` now has Elite (x1.5), but the presets don't use it yet.
- **No spells yet**: the Resonator and Caster archetypes fight with weapon and ability (B-293).
- Enemy HP by tier power since B-224b (`UCharacterData::EnemyTier`, Q-224d); the sim's enemies take the archetype's tier.
- Damage with no attacker (damage over time, status) counts as a hit, on neither side.
- Targets, reports (CSV / JSON / Markdown), GAS parity and the `wor.Balance.Run` command are B-293.
- Party-side AI difficulty is fixed at Hard (`AIConstants::ALLY_AI_DIFFICULTY`).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-03 | B-292: types, recorder, archetypes and presets, runner (seeded fights in one reused world, enemy strength, rank HP stand-in, stall cap, quiet logs, Let AI finish); spec `WoR.Balance.Sim.Runner`. | feature/B-292-sim-runner |
| 2026-11-03 | B-224: the rank HP stand-in is gone; the runner calls `TrialGate::ApplyEnemyRankHealth` after `ApplyEnemyStrength`, as `ABattleGameMode` does. Unbuilt (cloud). | feature/B-224-combat-numbers |
| 2026-11-03 | B-224b: enemies fight at rank x tier power HP (`UCharacterData::EnemyTier` from the archetype's tier; E-C x2.2, E-S x4.8), as §2 row 6 planned. Unbuilt (cloud). | feature/B-224b-rank-hp-per-team |
