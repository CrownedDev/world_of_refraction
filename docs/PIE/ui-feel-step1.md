# PIE - Menu feel, step 1: the merged Inventory / Loadout screen answers at once

**Branch:** `feature/ui-feel-step1` (built and tested on the desktop). **Built unseen:** not played yet. Rules: `docs/Design/UI_Theme.md` section 5a.

**What changed (nothing in what the screen does, only how it feels):**
1. **A move redraws only what changed.** Moving the highlight or the mouse recolours the old and the new item and fills the card; the whole screen is no longer redrawn for it.
2. **The card lands in place the same frame.** It is measured as it is filled; the second full redraw a frame later is gone.
3. **Our own hold-to-repeat.** A held direction repeats after 0.35 s, then every 0.09 s, the same on every PC. The system's own key repeat is ignored.
4. **Motion.** The highlight fades to the new item (0.09 s). A pressed item goes darker and 2 px lower for 0.08 s. The list and the grid glide to the lit item (0.12 s) in place of jumping.
5. **Sounds.** A cue for a move, a confirm, a back and a refusal. Silent until the sound files are set (below).

**Editor steps (the brain):** import the sounds and set them in `Config/DefaultGame.ini`:
```
[/Script/world_of_refraction.LoadoutV2ScreenWidget]
MoveSound=/Game/<path>/<asset>.<asset>
ConfirmSound=/Game/<path>/<asset>.<asset>
BackSound=/Game/<path>/<asset>.<asset>
RefusedSound=/Game/<path>/<asset>.<asset>
```
A line left out is silent. The Move cue starts 0.125 s into its file (UI_Theme 5a: the glass clink's lead-in).

| # | Do this | You should see / hear | Result |
|---|---|---|---|
| 1 | Game menu, Inventory; tap an arrow once | The highlight moves one tile at once and fades onto it (not a hard jump); the card is beside the new tile straight away and does not shift a moment later | [ ] |
| 2 | Hold an arrow down | One step at once, a short wait (about a third of a second), then steady quick steps; it stops the moment you let go | [ ] |
| 3 | Hold Down on the loadout's list (open it with R / Y, go Left) | The same rhythm on the list | [ ] |
| 4 | Hold a direction, then alt-tab away and back, or click elsewhere | The repeat has stopped; nothing runs on by itself | [ ] |
| 5 | Move fast across many tiles | No stutter per step; the highlight keeps up (each new press cuts the last fade short) | [ ] |
| 6 | With more rows than fit: hold Down past the last row in view | The grid glides to the next row (no jump); the card is already beside where the tile ends up | [ ] |
| 7 | A long loadout list (a window's list of pieces): Down past the last line in view | The list glides | [ ] |
| 8 | A (Enter) on a weapon set, on a slot, on a tile | The pressed item dips: darker and slightly lower for an instant; the action happens at once, not after it | [ ] |
| 9 | Click the Loadout button | It dips the same way | [ ] |
| 10 | Move the mouse over tiles and lines | The hover lights fade in and out; the card follows the tile without lag | [ ] |
| 11 | With the sounds set: move, confirm, go back, press A on a locked tile | Four different cues: a soft move, a firm confirm, a lower back, a "nope". A move against an edge (nothing moves) is silent | [ ] |
| 12 | With no sounds set | Silent, and nothing in the Output Log complains | [ ] |
| 13 | Everything the screen did before (D-248 grid and card, D-249 both sides, filling a slot) | Unchanged | [ ] |
| 14 | Note here anything that still feels stiff or late | | [ ] |
