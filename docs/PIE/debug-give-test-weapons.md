# PIE — Debug: give the test weapons (for the grip and shield sockets, D-242)

**Branch:** `feature/debug-give-test-weapons` (built and tested on the desktop 2026-11-18). **Built unseen:** not played yet. Replaces `wor.Debug.GiveAllWeapons`, which gave too much.

**What it is:** a testing command. `wor.Debug.GiveTestWeapons` gives the character you play 2 each of four weapons, as owned home gear, to equip from the inventory and loadout screens:
- Sword (`DA_Weapon_Sword`)
- Shield (`DA_Weapon_Shield`)
- Hand axe (`DA_Weapon_Axe_Hand`)
- Bearded axe (`DA_Weapon_Axe_Bearded`)

**Use it in the hub.** In a trial it refuses, because weapons given there would be lost when the run ends. No duplicate prompt opens for the copies. The weapons are saved with the character.

**Later:** guns and a bow join the list when their assets exist (one line each in `GiveTestWeaponsDebug.h`).

**Editor steps:** none.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | In the hub, console `wor.Debug.GiveTestWeapons` | A cyan line: 8 of 8 test weapons given. The Output Log has one `[GiveTestWeapons]` line per weapon, each "2 of 2 added" | [ ] |
| 2 | Open the inventory | 2 each of the sword, shield, hand axe and bearded axe, on top of what the character had; no duplicate prompt opened | [ ] |
| 3 | Equip each from the loadout screen | Each goes in and shows in the hand; note any that sit wrong on the grip (sheet D-242 grip, check 7) | [ ] |
| 4 | Console `wor.Weapons.PrintSockets` with the sword and the shield worn | The sword on `hand_r_grip`, the shield on `hand_l_shield` | [ ] |
| 5 | In a trial, console `wor.Debug.GiveTestWeapons` | "a run is open ... use it in the hub"; nothing is given | [ ] |
| 6 | Any `[GiveTestWeapons]` warning in the Output Log | None. If one names a weapon as skipped or refused (no room: the character already holds close to 20 weapons), note it here | [ ] |
