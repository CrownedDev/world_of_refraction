# Trial Pool — what a trial gives is what its enemies use

## Overview
D-225 (design reading copy: `docs/Design/TrialPoolDesign.md`). Each trial names a pool; its enemies are built from that pool (what they always have, plus a roll inside their limits), and what they drop comes from it. Built in stages (the design's §9): 1 the look, 2 the pool, 3 the enemy's parts, 4 the roll, 5 drops, 6 duplicates, 7 the merchant, 8 seeing a kit. The stopgap fill and the old inventories stay until Crown's PIE pass on stage 4.

## Architecture
- **`WoRTrialPoolLook`** (`Trial/TrialPoolLook.h`, stage 1): what the three screens say, with no widget in it.
  - `FEnemyKitCard` (name, class line, piece lines, skills), `FTrialDropView` (item, tier, prebuilt, piece or not), `FDuplicateDropView` (the drop, the tier held, the convert's essence), `EDuplicateChoice` (Replace, Add, Convert).
  - `GetDroppedLine`, `GetDuplicateQuestion`, `GetDuplicateChoiceText`; `DUPLICATE_CHOICES`, `DUPLICATE_BACK_CHOICE` (Add, Q-565).
  - `Make*Example`: stage 1's made-up contents (the design's example 2). `GetKitCardString` for logs.
- **`UTrialData::Pool`** (stage 2): the trial's pool tag. Items carry it in their `SourceTags` and `TrialLoot::GatherPool` finds them (weapons, adapters, skills, prebuilt finds, apparel, evolutions). A zone's `LootPool` overrides it in that zone.
- **`TrialPool`** (`Trial/TrialPool.h`, stage 2): `GetZoneTierRange` (zone 0 F to E ... zone 4 F to A), `GetPoolTag`, `GetPoolCrystals` (every element's gem, Quartz out, Q-566), `GetCrystalWeight` / `GetCrystalTierWeight` (the trial's element: `OWN_ELEMENT_WEIGHT` 3, `OWN_ELEMENT_TIER_STEP` +1 per tier), `PickCrystal`, `GetTrialPoolString`.
- **`FEnemyKit`** on `UCharacterData::EnemyKit` (`Trial/EnemyKit.h`, stage 3): `Pieces` (`FEnemyKitPiece`: an item and its given tier) and `Skills` it always has; the limits (B-567b/c: `MostGear`, `MostSkillsPerPiece`, `MostInnate` (points, D-240), `Kinds` (`EEnemyKitKind`) with `WeaponTypes`, `Elements`, the tier range), the numbers with an inline toggle (unset = any; B-567c: `MostSkillsPerPiece` unset = every slot, `MostInnate` (points) unset = `EnemyKitRules::GetInnateBudget`).
- **`EnemyKitRules`** (stage 3): `GetKind`, `GetMostGear` (the class's item count, lowered by the kit's most gear), `GetMostOfKind` (the class's per kind, 0 for a kind left out), `AllowsKind` (B-567b), `GetMostSkillsOnPiece` (never past the piece's slots), `GetInnateBudget` / `GetMostInnate` / `FitInnateBudget` (D-240's point budget), `AllowsWeaponType`, `AllowsElement`, `GetTierRange` (the zone's, narrowed), `CheckAlwaysHas` (class limits; validation warning), `DescribeOutsideZoneRange` (in `AEnemyZone::FindEncountersBelowGuide`), `GetEnemyKitString` (Print Enemy Kit).
- **`UEnemyKitCardWidget`** (`UI/Trial/`): the kit card, a CommonUI screen on the root layout's menu layer. B closes. No rank word (D-187).
- **`UDuplicateDropWidget`** (`UI/Trial/`): the three-choice prompt. Up / Down, A, mouse hover and click; Back keeps both. `Open(PC, View, OnChosen)`: OnChosen runs once with the choice.
- **`UCombatResultsWidget::SetDroppedLines`**: the fight's results show one "Dropped: ..." line each (`DroppedText`, BindWidgetOptional; hidden when empty).

## Integration Points
- Stage 5 calls `SetDroppedLines` from the battle game mode with the real drop; stage 6 opens `UDuplicateDropWidget` and acts on its choice; stage 8 opens the kit card from a real enemy.

## Debug
- `wor.Trial.Pool` (no tag: the active trial's pool, zone by zone; spec `WoR.Trial.Pool`). `wor.UI.PreviewKitCard`, `wor.UI.PreviewDuplicate`, `wor.UI.PreviewFightReward` (made-up contents). Spec `WoR.Trial.PoolLook`.

## Known Limitations / TODOs
- Stage 1 is built unseen: the look waits for Crown's eye (PIE sheet B-565).
- The convert's essence number is made up until stage 6.
- Nothing in play opens the screens yet.
- Apparel is listed in a pool but not given yet (`TrialLoot::GrantTo`, stage 5).

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-17 | B-565 (stage 1, the look): `WoRTrialPoolLook`, `UEnemyKitCardWidget`, `UDuplicateDropWidget`, the results screen's dropped line, three previews. Spec `WoR.Trial.PoolLook`. Built unseen. | `feature/B-565-trial-pool-look` |
| 2026-11-17 | B-566 (stage 2, the pool): `UTrialData::Pool`; `TrialPool` (zone tier ranges, the pool tag with the zone override, the five base gems with the element lean, `PickCrystal`, `GetTrialPoolString`); `TrialLoot` reads and grants evolutions, and `PickFromPool` skips what `CanGrant` refuses (apparel until stage 5, so a chest never sticks); `wor.Trial.Pool` with no tag prints the active trial. Tag `Source.Trial.Fire`. Spec `WoR.Trial.Pool`. | `feature/B-566-trial-pool-items` |
| 2026-11-17 | B-567 (stage 3, the enemy's parts): `FEnemyKit` on `UCharacterData::EnemyKit`, `EnemyKitRules`, Print Enemy Kit, the validation warning, the zone print's range warning (note 5). Spec `WoR.Trial.EnemyKit`. | `feature/B-567-enemy-parts` |
| 2026-11-17 | B-566b (Q-566, brain for Crown): the pool's crystals are every element's gem (Light, Darkness, Void and Reality added); Quartz stays out. | `fix/B-566b-every-element-crystal` |
| 2026-11-17 | B-568 (stage 4, the roll): `EnemyKitRoll` (`Roll`, `RollTeam`, `MakeInput`, `MakeSeed`, `GetRolledKitString`), `TrialPool::PickCrystal` with an allowed-types filter, `UTrialRunSubsystem::GetPendingEncounterSources`, `ABattleGameMode::SpawnCombatant` hands the kit over, `UInventoryComponent::ApplyRolledKit` wears it (the authored inventory stays for fights with no pool). `wor.Trial.PrintEnemyKits`. Spec `WoR.Trial.EnemyKitRoll`. Built unseen. | `feature/B-568-enemy-kit-roll` |
| 2026-11-17 | B-567b (Crown, the limits merged): `FEnemyKit` has five limits: `MostGear`, `MostSkills`, `Kinds` (`EEnemyKitKind`) beside `WeaponTypes`, `Elements`, the tier range; `EnemyKitRules::GetMostGear`, `GetMostOfKind`, `AllowsKind`, `GetKind` replace `GetMostWeapons` / `GetMostAdapters`; evolution allowed is a kind. | `refactor/B-567b-kit-limits-merged` |
| 2026-11-17 | B-567c (Crown): `MostSkillsPerPiece` and `MostInnate` replace `MostSkills` (no default 3); `EnemyKitRules::GetMostSkillsOnPiece`, `GetInnateBudget`, `GetMostInnate`, `FitInnateBudget`; the roll keeps each piece's picks (`FRolledKitPiece::Abilities` / `Spells`, `EnemyKitRoll::GetPieceSlots`, `MakeSocket`), `ApplyRolledKit` wears them (no `FillEmptyPicksFromKnown`) and keeps the authored innate spells that fit the budget. | `refactor/B-567c-skills-per-piece` |
