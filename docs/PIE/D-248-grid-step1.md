# PIE - D-248 step 1: the picture grid on the Equipment tab

**Branch:** `feature/D-248-grid-step1` (built and tested on the desktop 2026-11-19). **Built unseen:** not played yet. One piece: Crown looks at this before step 2 (the pop-up card).

**What it is (mockup `menus/loadout/v4/1b_equipment_grid_tooltip.png`):** on the Equipment tab, when you open a weapon slot (Main hand or Off hand) of a weapon set, the choices are now a grid of picture tiles in place of the text list. The set's slots stay on the left with the one being filled lit, so both are on screen at once.
- **A tile:** the weapon type's picture, a violet tick if the piece is worn, "x2" for a stack, greyed with a lock if it can't go in the slot.
- **Filter tabs** above the tiles: All, then one per weapon type you own.
- **Keys:** the arrows (or W A S D, the D-pad, the left stick) go tile to tile; Q / E (LB / RB) turn the filter; A picks; B goes back to the slots. The mouse wheel scrolls the grid; a click lights a tile, a second click picks it; a click on another slot row opens that slot.
- **The details box stays on the right** for now. Its first line says what the old list line said: why a locked piece can't go in (in orange), or the cells after and "the off hand comes off".
- **Names (D-248):** a piece is written as its name, and under it its type with the tier in brackets, "Simple Sword" over "Sword (F)", with no tier badge. This is on the weapon set rows, the slot rows and the details box's header.

**Not in this step:** the pop-up card, the Inventory page, buttons, a picture per weapon. Sockets, adapters and the evolution still open the old list. The Inventory, Skills and Apparel tabs are not changed.

**Known, for Crown's look:**
- With the details box kept, the grid is 7 tiles wide (the mockup's 10 comes when the card replaces the box).
- Every weapon of a type has the same picture, so two swords look alike until one is lit and the box names it.
- A filter shows only its type, so the worn piece is hidden under another type's tab (its Remove is on All). The grid always opens on All.
- The mouse wheel scrolls the grid when it has more rows than fit; when everything fits it moves the highlight one tile. Slot rows do not light under the mouse while the grid shows (a click on one still opens it).
- The tick and the lock are plain shapes drawn in code until the icon set has them. A type with no picture in the icon table shows its word on the tile ("Bow").

**Editor steps:** none. `wor.Debug.GiveTestWeapons` in the hub gives 12 weapons to fill the grid.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | Equipment tab: look at the weapon set rows | Each reads its name, and under it the type and tier in brackets ("Claymore" over "Greatsword (E)"); no tier badge after the name | [ ] |
| 2 | A on a weapon set, then look at the slot rows | A filled slot: the weapon's name, under it "Sword (F)", the slot's word beside it. An empty slot leads with the slot's word | [ ] |
| 3 | A on Main hand | **Screenshot.** Three columns: the slots on the left (Main hand lit), the grid in the middle, the details box on the right. The grid's header says "Main hand" and how many weapons | [ ] |
| 4 | Look at the tiles | One tile per weapon (a stack is one tile with "x2"); each shows its type's picture; the worn one has a violet tick in its corner | [ ] |
| 5 | Look at the filter tabs | "All" (filled), then one picture tab for each type you own | [ ] |
| 6 | Arrows, then W A S D or the D-pad | The highlight goes tile to tile: left and right along the row and on into the next row, up and down in the column; the details box follows it | [ ] |
| 7 | Light a tile, look at the box | Its name, under it "Type (Tier)", its stats; the first line says the cells after | [ ] |
| 8 | E (RB), then Q (LB) | The filter moves to the next tab and back; only that type's tiles show; the highlight starts on the first tile | [ ] |
| 9 | Click a filter tab, then a tile, then the same tile again | The filter changes; the tile is lit; the second click picks it | [ ] |
| 10 | Pick a weapon (A) | Back on the slots: the Main hand row shows the new weapon; the grid is gone and the list and box are their usual width | [ ] |
| 11 | With a sword in the Main hand, A on Off hand | **Screenshot.** The grid for the off hand: swords and shields open; the others (Claymore, bows, guns, axes) greyed with a lock | [ ] |
| 12 | Light a locked tile | The box's first line, in orange, says why ("Pairs only with a Sword.", or the like); A on it does not pick it and shows the reason | [ ] |
| 13 | With two sets worn, open a slot of one | The weapon worn in the other set has a tick and a lock; its reason: "In another entry." | [ ] |
| 14 | Main hand with an off hand set: light the Claymore | Not locked; the box's first line ends "the off hand comes off" | [ ] |
| 15 | With more tiles than fit (or a small window): the mouse wheel over the grid, and Down past the last row you can see | The wheel scrolls the grid inside its panel; with the keys the lit tile is always brought into view; nothing draws outside the panel. (With everything in view the wheel moves the highlight one tile.) | [ ] |
| 16 | Click another slot row while the grid shows | That slot opens (the grid for Off hand, or the old list for a socket) | [ ] |
| 17 | B in the grid | Back on the slots, as before | [ ] |
| 18 | A on Augment socket 1 | The old list, as before (not a grid in this step) | [ ] |
| 19 | The details box on a weapon set (the Equipment list) | Each piece: its name, then under it "Type (Tier)", no tier badge | [ ] |
| 20 | The Inventory, Skills and Apparel tabs | As before | [ ] |
| 21 | Look at the tick and the lock | Note here if the tick shows as an empty box (a missing character), or if either reads badly | [ ] |
