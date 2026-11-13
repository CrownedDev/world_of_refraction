# UI Theme — built from Hidris's command menu

**Status:** proposal (2026-11-03). Design only: no widgets or `.uasset` yet. Mockup (V1 vs V2 HUD, states, tokens): https://claude.ai/artifact/5X7f34AN7MKeZdyj4tSsUa (private to Crown).
**Builds on:** `UI_DeckbuilderReuse.md` §1 ("dark stone and steel, lit by crystal", Spectral SC + Lato), `UI_ScreenInventory.md` (Expedition 33 layout), `VisualDirection.md` (painterly world; the UI stays calm so the world carries colour).
**Open on Crown:** D-017 header style (not decided here), B-102 HUD review.

## 1. What Hidris made (`SourceArt/UI/Hidris/CommandMenu/`)

Four shapes: a **tile** with one 45° cut corner (top-left or top-right) and a **long bar** with a chevron point (right or left). Mirrored pairs, so a menu can sit on either side of the screen.

| Set | Files | What it is |
|---|---|---|
| V1 | 16 PNG | The **layers** of each shape, greyscale: outline (1, 5, 9, 13), bevel frame (2, 6, 10, 14), thin stroke (3, 7, 11, 15), glass fill (4, 8, 12, 16). Order: tile-L, bar-R, tile-R, bar-L. |
| V2 | 8 PNG | The layers **assembled**: grey **normal** (1–4) and cyan **selected** (5–8): tile-L, bar-R, bar-L, tile-R. |
| Source | `.afdesign` | Affinity Designer file. 21 "Exported V2" images still to come. |

V1 stacked = V2 normal (checked by compositing). So V1 is the toolkit, V2 the look.

**Missing for a full command menu:** pressed and disabled states; icons and input glyphs; a panel/card frame and bars; 2× resolution (bars are ~400×68 px: fine at 720p, soft at 1440p/4K); 9-slice margins (the bars must stretch for long labels).

## 2. Colour

| Token | Hex | Use |
|---|---|---|
| Stone 900 / 800 / 700 | `#101219` / `#14161C` / `#2A2D35` | Screen ground, panels (85% opacity), bar tracks |
| Plate dark / mid / edge | `#2E2E2E` / `#6A6A6A` / `#DCDCDC` | Hidris's grey plate (sampled from V2) |
| Steel | `#8C9098` | Frames, rules (deckbuilder) |
| Platinum | `#D6DEEC` | Tier, premium (deckbuilder) |
| Teal deep / Teal / Crystal | `#06595C` / `#337F8D` / `#9FECF5` | **Focus**: Hidris's selected state. The one UI accent. |
| **Proposed (2026-11-14, not approved):** Violet / Violet light | `#5B3A99` / `#CDB8FF` | A focus colour that sits on the title's purple nebula, where the teal clashes (Crown). Drawn on `screen_mockups/front/title/1_title.png` and, game-wide, `run/rest_spot/1_rest_menu_violet.png` (the kit's `WOR_ACCENT=violet`). White text on the fill is about 8:1. Insight from the deckbuilder, whose navigation buttons are purple; no asset of its is used. **Costs if approved:** Hidris's selected plates are cyan and would need a violet tint or redraw; and violet is already the Void element and the Secret quest colour, so focus would share a hue with them |
| Text / secondary / disabled | `#EEF1F5` / `#A9AFB8` / `#6B7078` | All pass 4.5:1 on Stone 800 except disabled (by design) |
| HP / HP low / EP | `#E8ECF2` / `#F07A5A` / `#6FD6E4` | Bars stay neutral so element colour stays meaningful |

Rule (deckbuilder §1, Hades' "colour is information"): chrome is grey-steel; **cyan = focus**; **element colour only where the element matters** (skills, status bars, portrait gem, proc banner).

## 3. Element accents (UI display values)

`ElementColors.h` uses pure primaries (e.g. Water `(0, 0.5, 1)` linear = `#00BCFF` on screen, almost Hidris's focus cyan; Darkness `#282828` vanishes on dark UI). Proposed UI values keep each hue, differ in lightness, and leave cyan to focus:

| Fire | Lightning | Earth | Wind | Water | Light | Darkness | Void | Reality | Generic |
|---|---|---|---|---|---|---|---|---|---|
| `#E5483A` | `#F2A23A` | `#D8B842` | `#4FC98A` | `#3A7BEA` | `#F3EBD0` | `#2B2633` + rim `#A79BC2` | `#A64BF0` | `#5B47C8` | `#B08A5C` |

Updating `ElementColors.h` to match is a separate code job (`UI_ScreenInventory.md`: "lock the palette").

## 4. Shapes, type, spacing

- **Shapes:** Hidris's language everywhere: straight edges, **one 45° cut corner** (14 px at 1080p), chevron points on lists. No rounded corners. Frames Hidris didn't draw (enemy plate, cards) copy this with a clip-path or a 9-slice.
- **Type:** **Spectral SC** SemiBold for names/titles/banners; **Lato** Bold caps (+16% tracking) for command labels, Lato Regular for body and tabular numbers. Both SIL OFL, already cleared (D-018).
- **Spacing:** 8 px grid: 4 icon→label, 8 in a row, 12 between command rows, 16 card padding, 24 between cards, 5% safe area at screen edges.

## 5. Button states

| State | Look |
|---|---|
| Normal | V2 grey plate |
| Focus / hover | V2 cyan plate. Gamepad focus and mouse hover are one state (CommonUI, pillar 9). |
| Pressed | Cyan plate 20% darker, 2 px down, 80 ms |
| Disabled | Grey plate at 45%, no gloss, grey text; still focusable to show why |
| Selected (toggle on) | Grey plate + V1 stroke tinted cyan (infusion source, chosen target) |

Focus must be **bright**, never dark: Expedition 33 players complained its black selection highlight was hard to see ([Steam discussion](https://steamcommunity.com/app/1903340/discussions/0/592896433808511656)). Metaphor's director: each menu has its own look but must stay "simple, practical, and usable" ([Kotaku](https://kotaku.com/-metaphor-refantazio-persona-5-menu-ui-hashino-1851667088)); Hidris's set fits that.

## 6. Combat HUD use (mockup)

Layout (Crown, 2026-11-03): party cards stacked **top-left**, turn order strip **top-right**, command menu **bottom-left**, input hints bottom-right. The top centre shows the **target's name, rank and Combat Tier** (it replaced the "Turn 3 · Orthos" label; the strip already shows whose turn it is). Later: Expedition 33's arrangement, the active character standing between the two halves of the command menu.

Commands: the Command Menu artifact (Attack A / Skills X / Items Y top-down, Defend RB mirrored beside Attack, B back, Tactics in the pause menu; sub-lists replace the menu in place, two mirrored columns past three entries; the tile is the infusion button).

Enemies: no name plate over them. Each has a full-length **HP** bar with a shorter **status bar** under it (the Hidris bar piece 10/11 twice, flattened to 60% height); the status bar takes the last hit's element, cracks as it fills and shatters when broken. The target adds a TARGET marker, the numbers and the break effect with turns left. No Weak / Resist labels. A **boss** gets a bar across the top instead (name, rank, tier, HP with reached phases only, status bar, break count).

**HP is rose `#E0407F`** (HP under 30% `#FF8FB4`, pulsing): no element uses that hue, while the old red sat on Fire's.

**Combat Tier (D-118):** every combatant shows a letter F–S, an overall danger read worked out from gear and skills. It sits beside the rank on the target line and the boss bar, and caps the tip of each party card's name plate. An octagon badge with a dark letter: silver rising from F (`#6B7078`) to B (`#E1E4E9`), gold for A (`#E8C979`) and S (`#D9B44A`, with a glow).

## 7. Next steps

1. Crown picks **V2 glass** or **V1 linework** (mockup row 1). Recommendation: **V2** for commands (reads on a busy painterly scene), V1 layers kept as the source for tints and extra states.
2. Ask Hidris for: pressed/disabled, 2× exports, a card frame, the 21 V2 exports.
3. After B-102: import as Slate brushes (Box draw, 9-slice) under `Content/UI/Library/`.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-03 | Created from Hidris's command menu art; mockup artifact | chore/ui-theme |
| 2026-11-03 | §6 rewritten: new HUD layout, enemy HUD, rose HP, Combat Tier badge (D-118) | chore/ui-theme |
