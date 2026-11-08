# Combat Tuning Data

## Overview
Combat tuning numbers that designers can change without a rebuild (B-218, plan `docs/analysis/StatMathsRedesign.md` §3, Q-146). Ladders are curve-table rows read through `FScalableFloat`. Single values are fields on one data asset. With no asset, or with a ladder that has no curve, the game uses today's C++ constants, so a missing asset can't change the game.

## Architecture
- `Combat/Tuning/CombatTuning.h`: `ECombatLadder` and `WoRTuning::GetLadderValue(Ladder, Key, CodeValue)`. Light on purpose (no UObject include), so the ladder headers can call it.
- `Combat/Tuning/CombatTuningData.h`: `UCombatTuningData` (`UPrimaryDataAsset`), and `WoRTuning::Get()` (the asset, else the class defaults).
  - Ladders (`FScalableFloat`), with the key each is read at:

    | Ladder | Key |
    |---|---|
    | `TierPower` | tier index, F = 0 .. S = 6 |
    | `TierGapDamage` | gap −4..+4 |
    | `TierGapCost` | gap −4..+4 |
    | `RequirementGap` | gap −5..+5 |
    | `ScalingGrade` | S = 0 .. F = 6 |
    | `PillarCurve` (B-224) | live pillar level 0 .. 7 (the soft cap `1 - (1 - L/7)^2`) |
    | `WeaponReferencePower` (B-224) | tier index, F = 0 .. S = 6 (F 40, E 52, D 68, C 88, B 114, A 148, S 192) |
    | `WeaponRequirementByTier` (B-224) | tier index, F = 0 .. S = 6 (main pillar: F 0, E 1, D 2, C 3, B 5, A 6, S 7) |
    | `EnemyRankHealth` (B-224) | `EEncounterRank` value (Normal 1, Mini-boss 3, Boss 6, Elite 1.5) |

    Past the last key, the gap ladders hold their end value, as the C++ clamp does. Index ladders outside their keys keep the C++ value.
  - Single values: `WeaponReqFactorMin` (B-224, 0.5: the floor on a weapon's requirement factor, weapon part only), the six infusion charge bands and the six Broken Darkness strain values. Their defaults are the C++ constants.
  - `LadderTable` and the `FillLadderTable` button write the C++ ladders into a curve table as constant (step) keys and point each ladder at its row.
- The C++ tables stay as the fallback, as `Code*` functions: `CodeTierGapDamage`, `CodeTierGapCost`, `CodeRequirementGap` (`TierGapConstants.h`), `CodeTierPower` (`TierPowerConstants.h`), `CodeScalingTierCoefficient` (`EScalingTier.h`); the B-224 baseline tables as `CombatBaseline::CodePillarCurve`, `CodeWeaponReferencePower`, `CodeWeaponRequirement`, `CodeEnemyRankHealth` (`Combat/Tuning/CombatBaselineConstants.h`, the Decided list of `docs/analysis/BaselineNumbers.md`).
- Asset path: `/Game/Data/Combat/DA_CombatTuning_Default`.
  - A miss is remembered until the next world starts.
  - Outside the editor the asset stays loaded (rooted).

## Integration Points
- The ladders are read by `GetTierGapDamageMultiplier`, `GetTierGapCostMultiplier`, `GetRequirementGapMultiplier`, `GetTierPowerMultiplier` and `GetScalingTierCoefficient`. These are unchanged signatures, so every caller reads the tuning.
- `UActionExecutor::GetChargeDamageMultiplier` and `GetChargeStatusMultiplier` read the charge bands. The infusion strain deficit is read in `ActionExecutor`.
- `UBrokenDarknessManager` reads the strain per cast, its floor, the break threshold and the snapshot log.
- B-224: `WoRWeaponPower::GetPillarCurve` reads `PillarCurve`; `GetReferencePower` / `GetReferenceRequirement` read the two weapon reference tables (authoring guides shown by `wor.Weapon.Print`, not read by damage); `TrialGate::GetRankHealthMultiplier` reads `EnemyRankHealth` (applied by `ApplyEnemyRankHealth` in `ABattleGameMode` and the balance sim); `FWorldStatRequirements::GetWeaponRequirementFactor` reads `WeaponReqFactorMin`.
- The Defense ceiling (75%) and the crit base (x1.25) are C++ constants (Q-218: caps move with B-042); `wor.Tuning.Print` lists them so one print shows the whole baseline.
- B-042 will use the same `FScalableFloat`s as GameplayEffect magnitudes.

## Known Limitations / TODOs
- No asset is committed yet; creating one needs the editor (`docs/PIE/B-218.md`).
- TODO: add a cook rule for the asset path before the first packaged build.
- Q-218: stat caps and per-point bases (`UNIVERSAL_STAT_CAP`, `RESISTANCE_PER_POINT`, …) stay in C++. Constexpr formulas derive from them (`STAT_DERIVE_DENOM`), so they move with B-042's MMCs.
- In a saved asset, a field you never edited keeps following the C++ default. Unreal saves only edited values.

## Debug
- `wor.Tuning.Print` (alias `wor.Tuning.Print`, B-224): which tuning is live, every value it gives, and the C++ caps.
- `wor.Tuning.Compare`: lists every value that differs from the C++ constants.
- Details-panel buttons: `PrintTuning`, `CompareWithCode`, `FillLadderTable`.

## Changelog
| Date | Change | Branch |
|------|--------|--------|
| 2026-10-27 | Created (B-218): `UCombatTuningData`, `WoRTuning` accessor, five ladders + charge bands + BD strain values, debug, spec `WoR.Tuning.CombatData`. | feature/B-218-combat-tuning-data |
| 2026-11-03 | B-224 (BaselineNumbers.md Decided): four baseline ladders (`PillarCurve`, `WeaponReferencePower`, `WeaponRequirementByTier`, `EnemyRankHealth`) and `WeaponReqFactorMin` 0.5; C++ tables in `CombatBaselineConstants.h`; `wor.Tuning.Print`; spec `WoR.Tuning.Baseline`. Written unbuilt in a cloud session. | feature/B-224-combat-numbers |
| 2026-11-03 | B-293d (Q-293d, Crown): rank HP Mini-boss x4 / Boss x8 (`ENEMY_RANK_HEALTH_*`); the requirement floor `WEAPON_REQ_FACTOR_MIN` 0.25. Specs `WoR.Tuning.Baseline`, `WoR.Trial.RankHealthPerTeam`. Unbuilt (cloud). | chore/B-293d-decided-balance |
