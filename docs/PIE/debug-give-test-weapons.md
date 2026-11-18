# PIE — Debug: give the test weapons (for the grip and shield sockets, D-242)

**Branch:** `feature/debug-test-weapons-set2` (built and tested on the desktop 2026-11-18). **Built unseen:** not played yet. The set is Crown's second pick (2026-11-18).

**What it is:** a testing command. `wor.Debug.GiveTestWeapons` gives the character you play 12 weapons, as owned home gear, to equip from the inventory and loadout screens. One of each:
- Swords: Simple Sword, Short Sword
- Greatswords: Claymore, Longsword
- Axes: Knight Axe, War Axe
- Shields: Plain Shield, Guard Shield
- Bows: Falconer Bow, Mechanical Bow
- Guns: Hunter Gun, Hunter Gun 2

**Use it in the hub.** In a trial it refuses, because weapons given there would be lost when the run ends. No duplicate prompt opens. The weapons are saved with the character.

**Needs:** the ten new weapon assets (made in the editor 2026-11-18) in the checkout you play. A weapon whose asset is missing is skipped with a warning in the Output Log.

**To change the set:** one line per weapon (its path and how many) in `GiveTestWeaponsDebug.h`.

**Editor steps:** none.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | In the hub, console `wor.Debug.GiveTestWeapons` | A cyan line: 12 of 12 test weapons given. The Output Log has one `[GiveTestWeapons]` line per weapon, each "1 of 1 added" | [ ] |
| 2 | Open the inventory | The 12 weapons above, on top of what the character had; no duplicate prompt opened | [ ] |
| 3 | Equip each from the loadout screen | Each goes in and shows in the hand; note any that sit wrong on the grip (sheet D-242 grip, check 7) | [ ] |
| 4 | Console `wor.Weapons.PrintSockets` with a sword and a shield worn | The sword on `hand_r_grip`, the shield on `hand_l_shield` | [ ] |
| 5 | The same with the other shield | The same hold; note if one shield sits differently from the other | [ ] |
| 6 | In a trial, console `wor.Debug.GiveTestWeapons` | "a run is open ... use it in the hub"; nothing is given | [ ] |
| 7 | Any `[GiveTestWeapons]` warning in the Output Log | None. If one names a weapon as skipped (asset missing) or refused (no room: the character already holds close to 20 weapons), note it here | [ ] |
