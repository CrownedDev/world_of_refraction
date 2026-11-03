# Balance Simulator

## Overview

Runs seeded, all-AI fights headless and records what happened (turns, rounds, damage, hits-to-kill, win rate), so balance can be checked by numbers instead of by hand. Design and targets: `docs/Design/BalanceSimulator.md`. Test and dev builds only (`WITH_DEV_AUTOMATION_TESTS`); nothing ships.

## Architecture

All under `Source/world_of_refraction/Private/Balance/`:

| Piece | Role |
|---|---|
| `BalanceSimTypes.h` | `FBalanceArchetype` (one combatant: class, pillars, sub-stat points, weapon tier / Power / scaling / requirement, attack and ability base), `FBalanceMatchup` (party archetypes vs an enemy archetype x count, rank, AI difficulty, zone tier, trial difficulty), `FBalanceFightRecord` (one fight; `==` for the replay check, `ToString`), `FBalanceAggregate` (per matchup means and win rate, `ToString`) |
| `BalanceSimRecorder.h` | `UBalanceSimRecorder`: binds the fight's broadcasts and fills one record; `Begin` per fight |
| `BalanceArchetypes.h/.cpp` | the archetype table (`BalanceSimulator.md` §4, pillars clamped to 7, plus `P-GenericC` / `P-GenericS` for the Mid / Late parties, whose stat points grow with the tier like the same-tier enemy's since Q-293e) and presets `quick` / `nightly` (18 + 2 matchups: there is no Elite rank) |
| `BalanceSimRunner.h/.cpp` | `FBalanceSimRunner`: `Begin` / `End`, `RunFight(Matchup, Seed)`, `RunMatchup(Matchup, N, BaseSeed)`, `SetUseGAS` (B-293: the effect reads on `wor.Effects.UseGAS` per fight, restored at `End`) |
| `BalanceSimReport.h/.cpp` (B-293) | the targets (`EvaluateTargets`: BalanceSimulator.md §3 per matchup, error or warn severity), the three reports (`ToCSV` one row per fight, `ToJSON` aggregates + targets, `ToMarkdown` the summary) and `WriteReport` into `Saved/Balance/<preset>_<stamp>.{csv,json,md}` |
| `Debug/Combat/BalanceSimDebug.h/.cpp` (B-293) | `wor.Balance.Run <quick|nightly|full> [fights] [seed] [gas]`: plays the preset, writes the reports, logs them |

One fight: spawn the archetypes as AI pawns (transient `UCharacterData` / `UWeaponData` / `UAbilityData`, the weapon owned and the ability learned so combat start keeps them), set `wor.Battle.Seed`, apply `TrialGate::ApplyEnemyStrength` (trial difficulty x zone tier, as `BattleGameMode`) and the rank HP stand-in (total HP = strength x rank), `StartCombat`, tick until `OnCombatResultReady` or `FIGHT_TICK_CAP` (= `Stalled`, logged with its seed), then destroy the actors. The world and orchestrator are reused across fights.

Between `Begin` and `End` the runner sets `wor.Combat.InstantImpacts 1` and `wor.AI.ThinkDelayScale 0` (B-291), drops `LogTemp` / `LogWoRCombat` / `LogWoREffects` / `LogWoRAI` to Warning (all set at console priority), and makes a local party with **Let AI finish** (otherwise D-045 hands the fallen party's AI to a player who isn't there). `End` restores the console variables and log levels.

## Integration Points

- Reads: `UActionExecutor::OnDamageDealt`, `OnTargetKilled`; `ACombatOrchestrator::OnActorTurnStarted`, `OnCombatResultReady`; `UTurnManager::OnRoundAdded`; `UStatusBuildupManager::OnStatusTriggered`.
- Uses: `FWoRTestWorld` (`Private/Tests/WoRTestWorld.h`), `TrialGate`, `AEnemyZone::GetTierStrengthMultiplier`, `UPartySessionSubsystem::RestoreLocalParty`.
- Specs: `WoR.Balance.Sim.Runner` (`Private/Tests/BalanceSimSpec.cpp`); `WoR.Balance.Sim.Targets` and `WoR.Balance.Sim.GASParity` (`Private/Tests/BalanceTargetsSpec.cpp`, B-293; the preset from `-WoRBalance=`).
- Reads `UWoREffectMirrorComponent::ReadsFromGAS` / `SetReadsFromGAS` (B-293 parity).

## Known Limitations / TODOs

- Rank HP is the game's own since B-224 (`TrialGate::ApplyEnemyRankHealth`, the `EnemyRankHealth` ladder). `EEncounterRank` now has Elite (x1.5), but the presets don't use it yet.
- Spells since B-293: the Resonator (`P-MidResonatorC`) casts one Instant Fire spell through a transient adapter holding an intact Garnet; the Caster (`P-LateCasterS`) has one innate. Projectile and burst spells are still not simulated.
- Enemy HP by tier power since B-224b (`UCharacterData::EnemyTier`, Q-224d); the sim's enemies take the archetype's tier.
- Damage with no attacker (damage over time, status) counts as a hit, on neither side.
- Targets (B-293): on `quick` only stalls fail; on `nightly` / `full` the error targets (party turns for a normal fight, mini-boss and boss rounds, win rates at Medium, the pillar-6 bands, stalls) fail `WoR.Balance.Sim.Targets` and the warn targets (p90s, hits-to-kill bands, the worked-example hit medians, crit rate and ratio, the boss win-rate band) are reported. The Decided numbers were never run through the sim (no Unreal in the cloud): expect the first nightly run to fail bands until the archetypes are tuned; the reports say which.
- The worked-example hit medians (44 / 132 / 382) are warn-only: the sim's Mid and Late archetypes clamp Mind to 7 where the examples use 8 and 14 (Q-293).
- The Defense-cap target (max observed 1 − taken/raw ≤ 0.76) isn't checked: the damage broadcast carries the final number only.
- "Pillar 6 reverse" checks only that the party clears (win rate ≥ 1 %), not §3's "mean turns ≤ 2× the matched-gear archetype" (TODO).
- The hit-median target uses every party hit (attacks, abilities, spells), not basic attacks alone, so it is a rough guide.
- Elite is only in `full`; `nightly` keeps 3 ranks (§4 of the design doc lists 4: TODO, decide with the first nightly's run time).
- GAS parity has not run yet: until B-293 the sim's actors had no ability system, so earlier "parity" was the enum path twice. Expect real differences on the first build; each names its seed.
- Presets: `quick` 3 fights (the suite), `nightly` 50, `full` 200 with the Elite rank (`WOR_BALANCE=nightly|full Scripts/ue/test.sh WoR.Balance.` or `wor.Balance.Run`).
- Party-side AI difficulty is fixed at Hard (`AIConstants::ALLY_AI_DIFFICULTY`).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-03 | B-292: types, recorder, archetypes and presets, runner (seeded fights in one reused world, enemy strength, rank HP stand-in, stall cap, quiet logs, Let AI finish); spec `WoR.Balance.Sim.Runner`. | feature/B-292-sim-runner |
| 2026-11-03 | B-224: the rank HP stand-in is gone; the runner calls `TrialGate::ApplyEnemyRankHealth` after `ApplyEnemyStrength`, as `ABattleGameMode` does. Unbuilt (cloud). | feature/B-224-combat-numbers |
| 2026-11-03 | B-293: targets with tolerances (`BalanceSimReport::EvaluateTargets`), CSV / JSON / Markdown reports in `Saved/Balance/`, `WoR.Balance.Sim.Targets` (preset from `-WoRBalance=`, `WOR_BALANCE` in `test.sh`) and `WoR.Balance.Sim.GASParity`, `wor.Balance.Run`, presets quick / nightly / full (Elite), spells on the Resonator (adapter + gem) and Caster (innate) archetypes, p90 / median / crit-rate in the aggregate. Written unbuilt in a cloud session. | feature/B-293-balance-targets |
| 2026-11-03 | B-293 desktop build: `FBalanceFightRecord::SpellCasts` (executor `OnActionStarted`; compared by parity; the targets spec fails if no spell is cast); quick writes `quick_latest.*`. First nightly: 19 bands fail (B-293c); parity covers the attribute path only (B-293b) | feature/B-293-balance-targets |
| 2026-11-03 | B-224b: enemies fight at rank x tier power HP (`UCharacterData::EnemyTier` from the archetype's tier; E-C x2.2, E-S x4.8), as §2 row 6 planned. Built on the desktop. | feature/B-224b-rank-hp-per-team |
| 2026-11-03 | B-293b: parity covers effects: `FBalanceArchetype::StatusBuildup` on attack / ability / spell, `P-StatusBuilder` (40 per hit) in `BalanceArchetypes::ParityStatusMatchup` (not in any preset), and a `GASParity` case asserting 0 diffs with at least one status break. Unbuilt (cloud). | feature/B-293b-parity-effects |
| 2026-11-03 | B-293c (Q-293c): the normal-fight target counts party rounds (member turns / party size, BaselineNumbers §2.9); the runner passes zone HP and difficulty-x-zone damage separately. Tuning values in `AIDecisionConstants.h` and `TrialGate.cpp`; findings in `docs/analysis/Expedition33Balance.md`. Unbuilt (cloud). | chore/B-293c-balance-tuning |
| 2026-11-03 | B-327 (D-120): the sim's `ApplyEnemyStrength` passes only the door's shares: the zone scales nothing (its `ZoneTier` stays in the matchup as a placement input); enemy HP = own × rank × Combat Tier. The audit's "zone counted twice" was checked and isn't (`MaxHPBuff` percents add). The nightly grid's zone tier is 0, so its numbers don't move. | fix/B-327-zone-health-once |
| 2026-11-03 | B-293d (Q-293e): `P-GenericC` / `P-GenericS` get their own stat points (Body 5/7, Raw Damage 8/20, Defense 15/25, Max Health 20/35, skill base 30/40), the same-tier enemy's sheet and the §3 examples; they were the early build with only the weapon swapped, so Mid/Late parties fought C/S enemies on early HP. Unbuilt (cloud). | chore/B-293d-decided-balance |
