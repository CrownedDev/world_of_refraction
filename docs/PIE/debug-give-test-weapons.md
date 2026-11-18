# PIE — Debug: give every weapon (for the grip and shield sockets, D-242)

**Branch:** `feature/debug-give-all-weapons` (built and tested on the desktop 2026-11-18). **Built unseen:** not played yet.

**What it is:** a testing command. `wor.Debug.GiveAllWeapons` gives the character you play 2 of every weapon in the project (25 today, the Round Shield included), as owned home gear, so each can be equipped from the inventory and loadout screens.
- `wor.Debug.GiveAllWeapons 1` gives one of each; the number can be 1 to 5.
- **Use it in the hub.** In a trial it refuses, because weapons given there would be lost when the run ends.
- No duplicate prompt opens for the copies.

**Known:** the weapon inventory holds 20 and this goes past it on purpose. After the command, loot and vendor weapons are refused for that character until it is back under 20. The weapons are saved with the character, so use a test save.

**Editor steps:** none.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | In the hub, console `wor.Debug.GiveAllWeapons` | A cyan line: 50 pieces of 25 weapons given, 0 not loaded. The Output Log has one `[GiveAllWeapons]` line per weapon, each "2 of 2 added" | [ ] |
| 2 | Open the inventory | 2 of each weapon, on top of what the character had; no duplicate prompt opened | [ ] |
| 3 | Equip a few from the loadout screen (a sword, an axe, a greatsword, the Round Shield) | Each goes in and shows in the hand; note any that sit wrong on the grip (sheet D-242 grip, check 7) | [ ] |
| 4 | Console `wor.Weapons.PrintSockets` with a shield and a weapon worn | The weapon on `hand_r_grip`, the shield on `hand_l_shield` | [ ] |
| 5 | In a trial, console `wor.Debug.GiveAllWeapons` | "a run is open ... use it in the hub"; nothing is given | [ ] |
| 6 | Any `[GiveAllWeapons]` warning in the Output Log | None. If one names a weapon as skipped or refused, note it here | [ ] |
| 7 | Look at the weapon count or room shown on the inventory screen | It reads sensibly with 50 or more owned (the room is 20). Note here if it shows a negative number or looks broken | [ ] |
