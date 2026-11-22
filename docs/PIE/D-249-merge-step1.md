# PIE - D-249 piece 1: Inventory and Loadout as one screen, the two-sided layout

**Branch:** `feature/D-249-merge-step1` (built and tested on the desktop). **Built unseen:** not played yet. One piece: Crown looks at this before piece 2 (Equip from the inventory).

**What it is (mockups `menus/inventory/v3/1_inventory_alone.png`, `2_loadout_open.png`):** one screen in place of two.
- **The inventory is home.** Tabs Gear, Pouch, Chest; the tab's things as picture tiles in the grid; the lit tile's details in the pop-up card (the same grid and card as D-248).
- **The Loadout button** (top right; Y on a pad, R on a keyboard, or a click) brings the loadout in on the left: its tabs Equipment, Skills, Apparel, Cosmetics, and the weapon sets and slots as before. The inventory moves right and gets narrower (14 tiles to a row become 10). The button again sends it out.
- **Moving between the two sides:** Left off the grid's first column goes to the loadout; Right from the loadout's list comes back. A click on either side brings the keys there.
- **Both menu pages open this screen:** the game menu's Inventory page opens it with the loadout out, its Loadout page (and the Gate's loadout key) with the loadout in.
- **Gone:** the loadout's own Inventory tab, and the old inventory screen's Loadout tab. The button replaces both.

**Not in this piece:**
- **Equip from the inventory** (piece 2): A on an inventory tile only says so for now. Equipping still works the usual way: open the loadout, A on a weapon set, A on a slot; the grid then shows that slot's choices.
- **Give, Store and Take** (piece 3): not on this screen yet. Until then the old inventory screen opens with the console command `wor.UI.InventoryScreen` and still does them.

**Known, for Crown's look:**
- With the keys on the loadout's side, the lit line's details are a card at the grid's left edge, over the tiles (the grid now has the room the details box stood in). It moves to the lit tile when the keys go to the inventory.
- The loadout's list is narrower than it was (it shares the row with the grid), so long names end in "...".
- The Pouch tab shows its stacks only: the empty and the locked slots the old screen drew are not drawn here yet (the heading still says how many slots are used).
- An adapter's tile shows its frame without the crystal inside it.
- Y is the Loadout button on the inventory's side. On the loadout's side Y keeps its own jobs (Options, Confirm), so from there the loadout is closed with the mouse, or by going to the inventory's side first.
- The inventory's sort moved from Y to the View button (H on a keyboard), since Y is now Loadout.

**Editor steps:** none. `wor.Debug.GiveTestWeapons` in the hub gives 12 weapons to fill the grid.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | Game menu, Inventory | **Screenshot.** The merged screen with the loadout out: character tabs, then Gear / Pouch / Chest, the Loadout button top right, the grid across the whole width, 14 tiles to a row | QA passed (agent) |
| 2 | Look at the Gear tiles | Your weapons, adapters, evolutions and apparel as picture tiles: a tick on a worn one, "x2" on a stack | Not tested (agent): partly seen; no apparel, and no "x2" in Gear |
| 3 | Arrows over the tiles | The highlight goes tile to tile and the card follows it: the piece's name, under it its kind and tier, its lines | QA passed (agent) |
| 4 | Up from the top row, then Left / Right, then Down | The keys go to the filter row (no tile is lit); Left / Right change the filter and the tiles; Down goes back to the tiles | QA passed (agent) |
| 5 | Q / E (LB / RB) | Gear, Pouch, Chest in a ring; the Pouch has no filter row; the heading says the pouch's slots | QA passed (agent) |
| 6 | H (the View button) | The sort changes (Kind, Tier, Name) and the count line names it | QA passed (agent) |
| 7 | R (Y on a pad) | **Screenshot.** The loadout comes in on the left with its tabs and the weapon sets; the button reads "Close loadout" and is lit; the grid is narrower, 10 tiles to a row; the keys stay on the inventory | QA passed (agent) |
| 8 | Left from the first column | The keys go to the loadout: its line is lit, the inventory's tile is not; the card stands at the grid's left edge with the lit weapon set's details | QA passed (agent) |
| 9 | Up / Down on the loadout, Q / E | The loadout's lines; its tabs turn (Equipment, Skills, Apparel), not the inventory's | QA passed (agent) |
| 10 | Right | Back on the inventory: its tile is lit again, the loadout's line only marked | QA passed (agent) |
| 11 | Click a tile, then a weapon set, then an inventory tab | Each click brings the keys to that side | QA passed (agent) |
| 12 | On the loadout: A on a weapon set, A on Main hand | As D-248: the slots on the left, that slot's choices in the same grid with the card; pick one, and the grid shows the inventory again | QA passed (agent) |
| 13 | With the loadout's window open (step 12, before picking), click the Loadout button | It stays in and a message says to finish or leave first | QA passed (agent) |
| 14 | On the inventory's side, R (Y) again | The loadout goes out; the grid is wide again | QA passed (agent) |
| 15 | Game menu, Loadout | The same screen opens with the loadout in and the keys on it | QA passed (agent) |
| 16 | The loadout's tabs | No Inventory tab; Equipment, Skills, Apparel work as before; Cosmetics greyed | Not tested (agent): partly seen; the tabs open, no skill or apparel was used |
| 17 | Z / C (LT / RT) | The next character: both sides change to it | QA passed (agent) |
| 18 | A on an inventory tile | A message: equipping from here comes next. Nothing else happens | QA passed (agent) |
| 19 | Console `wor.UI.InventoryScreen` | The old inventory screen, with Give / Store / Take working and no Loadout tab | QA passed (agent) |
| 20 | B on the inventory's side | Back to the game menu | QA passed (agent) |
| 21 | The Skills and Apparel tabs with the loadout in | They work as before; their details are the card at the grid's left edge. Note here anything that reads badly in the narrower list | Not tested (agent): partly seen; the tabs open with empty slots only |

**Agent run, 2026-11-22 (D-251 checkpoint 1):** two passes, each verdict audited against its screenshot by the brain. Screenshots: `qa/2026-11-22/` (sheet1 to 7 first pass, p2 second). Faults: B-574.

**Agent run 2, 2026-11-22 (D-251 checkpoint 1, a fresh tester with the B-573 tool):** the results above are this run's, each audited against its screenshots by the brain (`qa/2026-11-22/run2-1` to `run2-7`). Faults: B-574.
