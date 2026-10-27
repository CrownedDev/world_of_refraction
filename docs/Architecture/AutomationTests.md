# Automation Tests

## Overview

Headless Unreal Automation Spec tests for the combat maths and turn flow. They run without the editor UI or PIE, so a rule can be checked in seconds, not by setting up the exact fight by hand. `Scripts/ue/test.sh` runs them; the Stop hook runs them when `UE_ROOT` is set.

Every test is named `WoR.<System>.<Case>`. The default filter is `WoR.`, with the dot: a bare `WoR` also matches engine tests by substring (e.g. "Frame**wor**k").

## Architecture

All files live in `Source/world_of_refraction/Private/Tests/`, wrapped in `WITH_DEV_AUTOMATION_TESTS` (compiled out of Shipping).

| File | Covers |
|---|---|
| `WoRTestWorld.h` | `FWoRTestWorld`: wraps the engine's `FTestWorldWrapper`. Real `UGameInstance`, so every WoR subsystem exists. Ticked by hand. `SpawnCombatant` gives a bare actor (player path) or an unpossessed pawn (AI path) that carries only a `UCharacterDataComponent` |
| `DamageCalculatorSpec.cpp` | `WoR.Damage`: Pattern-P stat cap (physical + spell), defense reduction, its 0.5 clamp, `bIgnoreDefense`, `MIN_DAMAGE`, crit damage (×1.0 with no investment, capped at ×1.5), crit gating |
| `TierGapScalingSpec.cpp` | `WoR.Scaling`: tier-gap damage + cost ladders and the requirement-gap ladder (`TierGapConstants.h`). Pure functions, so no world |
| `TurnManagerSpec.cpp` | `WoR.TurnManager`: full-tie order (local party first), alternation at equal speed, a faster combatant acting first and more often |
| `CombatHangFixesSpec.cpp` | `WoR.Combat.HangFixes`: B-001 regressions through a real `ACombatOrchestrator`. Stunned turn passes on the next tick; a normal win clears status bars, effects, `OnDied` binds, the grid and the TurnManager; an invalid AI action passes the turn; an invalid player action re-requests without stranding the turn |

Combatants are bare on purpose: no loadout, no grid slot, no effects. Expected values come from `CombatConstants` rather than literals, so a tuning change doesn't break them, but a formula change does.

## Integration Points

- `Scripts/ue/test.sh [filter]` launches `UnrealEditor-Cmd -nullrhi`. It reads the report's `index.json` and prints each test's state with its error messages. It counts `succeeded` + `succeededWithWarnings` as passes. A test that logs a `UE_LOG` warning (the orchestrator does, on purpose) still passes.
- The log goes to `Saved/ClaudeLogs/test.log` via `-abslog`, and the report to `Saved/ClaudeLogs/tests/`.
- The editor must be closed for `build.sh`: the open editor locks the module DLL, and the link step fails with `LNK1104`.

## Known Limitations / TODOs

- No test yet covers gear: loadouts, stones, gear-ceiling multipliers. The gear-beyond-the-cap half of Pattern P is untested.
- Crit *chance* comes from Luck and is rolled with `FMath::FRand`, so the tests force it with `OverrideCritChance`. B-010 (per-battle `FRandomStream`) will make rolled paths testable.
- `CombatHangFixesSpec` drives turns by calling `OnActionCompleted` and `SubmitActionAsync` directly. Real ability or spell execution (montages, the async runner) is not exercised.
- The tests can't see the `OnCombatResultReady` payload: dynamic delegates need a `UFUNCTION` listener. They check the observable state after the win instead.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | Created: test world helper + Damage, Scaling, TurnManager, Combat.HangFixes specs (26 tests). test.sh: per-test output, warnings count as passes, `-abslog`, default filter `WoR.` | `feature/B-007-automation-tests` |
| 2026-10-26 | Dev-only tooling compiled out of Shipping. All `wor.*`/`WoR.*` console commands sit in `#if !UE_BUILD_SHIPPING`. The 5 legacy `*TestActor` classes keep their constructor + BeginPlay, but their logic is guarded, with empty UFUNCTION stubs in Shipping (UHT thunks need them). New console commands and test actors follow the same pattern | `chore/B-008-shipping-guards` |
| 2026-10-27 | B-119: `test.sh` runs the `DataValidation` commandlet after the tests pass, on `PrimaryDataAsset` subclasses only (`-AssetType`, ~12 s; all of `/Game` took 200 s+ and 8 GB on vendor anims). Errors are normalised to `<path under Content/>: <message>` and compared with `Scripts/ue/validation_baseline.txt` (known content debt, 13 lines today, fixed in B-206): a new error fails, a baseline line that no longer appears prints as FIXED. `WOR_SKIP_VALIDATION=1` skips it. Not covered: assets that don't load (an LFS pointer file is skipped by the asset registry). | chore/B-119-data-validation |
