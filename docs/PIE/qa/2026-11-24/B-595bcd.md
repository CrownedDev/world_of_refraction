# B-595b/c/d combined: agent check (2026-11-24, `feature/B-595bcd-sword-skills`)

QA save slot 0, 1280x720. `wor.UI.VendorScreen CombatMaster` in the hub after merging B-595b, B-595c and B-595d onto dev `9fd7b99ef`.

![Combat Master](B-595bcd/combat_master_five.png)

| # | Check | Result |
|---|---|---|
| 1 | The Combat Master sells all five refined sword skills | Pass: Quick Strike, Force Strike, Over Under, Heavy Attack, WhirlWind; tier F, 25 Gold + 10 Dormant Essence each. "Already known" because the QA save learned them in earlier checks |
| 2 | Its stock file (binary) after the merges | B-595b's side taken, Heavy Attack and WhirlWind re-added in the editor and read back (5 lines) |
