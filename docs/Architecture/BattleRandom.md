# Battle Random Stream

## Overview

Every random roll inside a fight comes from one seeded `FRandomStream` per battle
(`UBattleRandomSubsystem`). The seed is set when combat starts and written to the log,
so a fight can be replayed exactly, and tests can pin the outcome of a roll.

Rolls outside a fight stay on the global `FMath` RNG: equipment substat rolls
(`FEquipmentStatBonus.cpp`, `FResistanceBonus.cpp`), loot quality (`EconomyYield.h`) and
effect IDs (`SkillEffectManager.cpp`, an identity, not a game roll; B-062 replaces them).

## Architecture

### `UBattleRandomSubsystem` (`UWorldSubsystem`, `Public/Combat/Mechanics/BattleRandomSubsystem.h`)

- World-scoped: one per world, so it lives and dies with the battle map. It is not on the
  GameInstance, so it adds no per-fight state there (D-064).
- `BeginBattle()`: seeds from the `wor.Battle.Seed` console variable when it is non-zero,
  otherwise from a fresh GUID-derived seed (never 0). Logs
  `[BattleRandom] Battle seed N (replay with: wor.Battle.Seed N)` on `LogWoRCombat`.
- `SeedStream(Seed)`: restarts the stream (tests, replays). `GetSeed()`, `GetDrawCount()`.
- `Initialize` seeds a fresh stream, so rolls before the first battle are still valid.
- Static draw helpers taking a world context: `FRand`, `FRandRange`, `RandRange`
  (inclusive), `RandBool`. With no world (a CDO, a null context) they fall back to `FMath`.

### Who draws from it

| Caller | Rolls |
|---|---|
| `UDamageCalculator` | Crit in `CalculateDamage`; `RollCriticalHit` |
| `UBrokenDarknessManager` | Break roll; luck strain-skip |
| `UAIDecisionManager` | Easy random action/target/spell/ability, thinking delay, defense attempt, Medium dodge coin, Hard/Expert parry, dodge direction, aim jitter, Easy `SelectBestTarget` |
| `UActionExecutor::GetEffectTargets` | Random distinct targets |
| `USkillEffectManager::ApplyTriggeredSkillEffect` | `RandomDebuff` pick |
| `UCharacterDataComponent::RollLuckChance` | Every luck roll through it |
| `UItemExecutor::ExecuteGambleEffect` | Amethyst gamble: buff/debuff, effect, resistance category |
| `UCrystalManager` | Luck break-skips on crystal, evolution and gear-evolution wear |

## Integration Points

- `ACombatOrchestrator::StartCombat` calls `BeginBattle()` after the teams are validated,
  before `TurnManager->InitializeCombat` (turn 1 can act inside it).
- GameInstance subsystems pass `this`; their world comes from the GameInstance's current
  world. Components pass `this` (the owner's world).

## Debug

- `UBattleRandomDebug::GetBattleRandomString` / `PrintBattleRandomState`
  (`Public/Debug/Combat/BattleRandomDebug.h`).
- Console: `wor.Battle.PrintRandom` (seed, draw count, replay command);
  `wor.Battle.Seed <N>` forces the seed of every following battle (0 = fresh).
- Tests: `WoR.Combat.BattleRandom` (`Private/Tests/BattleRandomSpec.cpp`).

## Known Limitations / TODOs

- A replay matches only if the inputs match too: the player's own choices and defense
  timing are not recorded, so "same seed, same fight" holds for AI-only fights and for
  identical player inputs.
- The stream is shared by every system, so a new roll added anywhere shifts every later
  roll for the same seed. That is fine for replaying a build, not across builds.
- Not replicated. Co-op would seed on the server and send the seed (ArchitectureReview §1.6).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | Created: per-battle seeded stream, seeded at combat start, every in-fight roll moved to it, `wor.Battle.Seed` / `wor.Battle.PrintRandom`, 6 tests (B-010) | `feature/B-010-battle-random-stream` |
