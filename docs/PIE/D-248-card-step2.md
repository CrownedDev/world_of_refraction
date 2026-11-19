# PIE - D-248 step 2: the pop-up card on the Equipment grid

**Branch:** `feature/D-248-card-step2` (built and tested on the desktop 2026-11-19). **Built unseen:** not played yet. One piece: Crown looks at this before step 3 (the buttons).

**What it is (mockup `menus/loadout/v4/1b_equipment_grid_tooltip.png`):** while the grid is open (Main hand or Off hand being filled) the fixed details box on the right is gone. The lit tile's details show in a card beside that tile instead, and the grid is wider.
- **The card** has what the box had: the piece's name, under it "Type (Tier)", its stats and sockets; first of all, in orange, why a locked piece can't go in, or else the cells after and "the off hand comes off". It has a thin light frame so it reads over the tiles.
- **It follows the focus:** the lit tile with the keys, the tile under the mouse with the mouse.
- **It stays on screen:** on the tile's right with their tops level; on the tile's left when there is no room on the right; with their bottoms level when it would run past the bottom.
- **The grid is ten tiles wide** with bigger tiles (the mockup's size).
- **Everything else is as step 1:** the tiles' tick, count and lock, the filter tabs, the keys, the mouse, the slots on the left. Outside the grid the details box is where it always was.

**Known, for Crown's look:**
- The card covers the tiles beside the lit one (as the mockup's does). A tile under the card can still be lit with the mouse or the keys; the card then moves to it.
- The card is set in place twice: once at the key or mouse move, and once more a frame later when the text's real height is known. It should not be seen to jump; check 16 asks.
- The mouse and the wheel go through the card to the tiles under it.
- A very tall card (the full view of a piece with many effects) starts at the top of the tiles and can run down over the legend line.

**Editor steps:** none. `wor.Debug.GiveTestWeapons` in the hub gives 12 weapons to fill the grid.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | Equipment tab, A on a weapon set, A on Main hand | **Screenshot.** Two columns: the slots on the left, the grid filling the rest. No details box on the right. Ten tiles to a row | [ ] |
| 2 | Look at the first tile's card | A card right of the lit tile, its top level with the tile's: the name, "Type (Tier)" under it, the stats and sockets, in a thin light frame | [ ] |
| 3 | Right along the row | The card moves with the lit tile, always on its right while there is room | [ ] |
| 4 | Go to the last tiles of a row (or move the mouse there) | **Screenshot.** The card is on the tile's left; none of it is off the panel's right edge | [ ] |
| 5 | With enough weapons for the lower rows (or a small window): light a tile near the bottom | The card's bottom is level with the tile's bottom; none of it runs below the screen | [ ] |
| 6 | Move the mouse over tiles | The card follows the tile under the mouse; moving off the tiles puts it back beside the lit tile | [ ] |
| 7 | Move the mouse onto a tile the card covers | That tile lights under the mouse and the card moves to it (the card never blocks the mouse) | [ ] |
| 8 | Light a locked tile | The card's first line, in orange, says why it can't go in | [ ] |
| 9 | Main hand with an off hand set: light a weapon that can't pair | The card's first line ends "the off hand comes off" | [ ] |
| 10 | Q / E (the filter), then a pick with A | As step 1: the filter turns, the card shows the first tile's piece; A picks and goes back to the slots | [ ] |
| 11 | Back on the slots (B, or after a pick) | The details box is back on the right, in its old place and width, with no frame | [ ] |
| 12 | The Equipment list, and the Inventory, Skills and Apparel tabs | The details box is where it was before this step | [ ] |
| 13 | A on Augment socket 1 | The old list with the details box on the right (not a grid, no card) | [ ] |
| 14 | X Compare in the grid | The compare line still shows under the columns | [ ] |
| 15 | The mouse wheel with more rows than fit, then a key | The grid scrolls and the card moves with its tile (off the top or bottom it stays at the view's edge); a key brings the lit tile into view with its card | [ ] |
| 16 | Open the grid and move tile to tile, watching the card as it arrives | It lands beside the tile and stays. Note here if it is seen to jump or shift a moment after it appears | [ ] |
| 17 | The mouse wheel with the mouse over the card | The grid scrolls (or, with everything in view, the highlight moves a tile), as over a tile: the card does not stop the wheel | [ ] |
