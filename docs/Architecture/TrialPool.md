# Trial Pool — what a trial gives is what its enemies use

## Overview
D-225 (design reading copy: `docs/Design/TrialPoolDesign.md`). Each trial names a pool; its enemies are built from that pool (what they always have, plus a roll inside their limits), and what they drop comes from it. Built in stages (the design's §9): 1 the look, 2 the pool, 3 the enemy's parts, 4 the roll, 5 drops, 6 duplicates, 7 the merchant, 8 seeing a kit. The stopgap fill and the old inventories stay until Crown's PIE pass on stage 4.

## Architecture
- **`WoRTrialPoolLook`** (`Trial/TrialPoolLook.h`, stage 1): what the three screens say, with no widget in it.
  - `FEnemyKitCard` (name, class line, piece lines, skills), `FTrialDropView` (item, tier, prebuilt, piece or not), `FDuplicateDropView` (the drop, the tier held, the convert's essence), `EDuplicateChoice` (Replace, Add, Convert).
  - `GetDroppedLine`, `GetDuplicateQuestion`, `GetDuplicateChoiceText`; `DUPLICATE_CHOICES`, `DUPLICATE_BACK_CHOICE` (Add, Q-565).
  - `Make*Example`: stage 1's made-up contents (the design's example 2). `GetKitCardString` for logs.
- **`UTrialData::Pool`** (stage 2): the trial's pool tag. Items carry it in their `SourceTags` and `TrialLoot::GatherPool` finds them (weapons, adapters, skills, prebuilt finds, apparel, evolutions). A zone's `LootPool` overrides it in that zone.
- **`TrialPool`** (`Trial/TrialPool.h`, stage 2): `GetZoneTierRange` (zone 0 F to E ... zone 4 F to A), `GetPoolTag`, `GetPoolCrystals` (the five base gems, Q-566), `GetCrystalWeight` / `GetCrystalTierWeight` (the trial's element: `OWN_ELEMENT_WEIGHT` 3, `OWN_ELEMENT_TIER_STEP` +1 per tier), `PickCrystal`, `GetTrialPoolString`.
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
