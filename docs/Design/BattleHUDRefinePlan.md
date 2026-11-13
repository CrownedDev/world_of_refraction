# Battle HUD refinement — build plan

**Status:** approved by Crown (2026-11-13, the `refine_final/` set). Building one piece at a time, Crown's PIE review between pieces. Backlog: **B-435**. Branch: `feature/battle-hud-refine` (from `dev` at 8831e4a1f, after B-424).
**Mockups:** `docs/Design/screen_mockups/combat/hud/refine_final/` is the whole set: main menu (`1`), one image per skill kind (`2` to `5`), target picker with the infusion strip (`6` to `10`). `refine_v2/` still shows the overall frame. Everything else (`refine_v1/`, `refine_v3/` to `refine_v3f/`) is history.
**Checks against:** FeelGuide 5 and 13, `UI_Theme.md`, Expedition 33's battle menu.

## Decisions this plan builds

| # | Decision | Source |
|---|---|---|
| 1 | Layout: turn order top right; menu stacked above the active card bottom left; idle allies as name + HP cards | Crown, refine_v1/v2 |
| 2 | Active card: big HP/EP numbers, low-HP pulse, class before the status icons ("Darkness Refractor: …"; `Caster` shows as "Refractor") | Crown, refine_v2 |
| 3 | Enemy HP is hidden unless targeted; the in-world target marker shows name, HP, rank, tier | Crown, refine_v2 |
| 4 | Skills: 3 cards on Y / X / A with the description inline; at most 6 per school = 2 pages | Crown, refine_v3 |
| 5 | **Two rows, no labels.** The **kind row on top**: Abilities, Refractions, Breakthrough, Resonate, only the kinds the character has. "Resonate" is the code's "Resonate (A)", renamed (Crown): with no Resonate (W) left (weapon spells are gone, B-098; everything a weapon gives is an Ability) the "(A)" says nothing. **Under it, one row tied to the selected kind**: Refractions → its schools (only schools that hold a spell); Resonate → the adapters; Breakthrough → its sources (Evolution, or the adapter that carries it); Abilities → the two weapon **types** for a Generic carrying two weapons, otherwise no row. Abilities is one list: the weapon's own abilities (at most 6) and the crystal's extra slots (B-410), so it can run past two pages. **Augment-stone abilities are not shown or planned for:** Crown is considering removing augment stones. Weapons always show as their type (Sword, Hammer…), never the weapon's name. The cards sit below | Crown, refine_final |
| 6 | **Picking an adapter or weapon tab equips it.** It becomes the active one, free as today (CL-18, CL-26, ET-19). **A basic Attack shows its weapon type as the top tab row on its target screen**, above the infusion rows; a two-weapon Generic switches weapon there (D-pad up/down, or a click), so there is no weapon toggle on the main menu | Crown, refine_final `6` |
| 7 | B is always Back. Defend moves to R1, shown as a clickable corner hint (on by default, a setting hides it) | Crown |
| 8 | **Infusion uses the same tab rows as the skill menu**, on the target screen: a row of the available **elements**, and under it a level row **— / I / II** (L0 is "—": it means no infusion, and "0" is not a numeral in that set). No hold, no overlay. **Controls: L2 / R2 = element, L1 = level** (cycles — → I → II). Why: the triggers already mean "the top tab row" in the skill menu; nothing is held; and neither touches left/right, which stays the target. Mouse: click a tab | Crown; controls picked here |
| 9 | **What the rows show: elements, not items.** Attacks and abilities: None, Raw, then **each element the character can infuse**, in the game's element order. Duplicates collapse: two Fire sources make one Fire tab, so the row stays short. Innate's and the evolution's elements count like any other. A spell is origin-bound, so its element is a **single lit tab**; the one exception is an evolution spell of a Broken Darkness / Reality character (two tabs). **No rows at all** for item spells, broken-adapter spells and stunned characters. **When several sources give the picked element, the action uses all of them** (Crown, 2026-11-13; the rule and its build are a separate job, this plan only displays it): each pays its own cost and the hit gains status buildup per extra source, capped. The tab shows once, marked with the count ("Fire ×2"). **A "via" line under the readout lists every source used and every cost** ("via Ember Ring + Innate · wear + 12 EP"). **Picking an element whose source isn't active switches to it** (free): another adapter, or on a basic Attack the other weapon (the weapon row follows). **Needs a code change:** today `GetAvailableInfusionSources` offers only the active adapter and the active weapon's crystal (`ActionExecutor.cpp:4480-4519`). An **ability** counts only its own weapon's crystal; the other weapon's crystal can power a basic Attack only. Under the rows: the damage / size / EP readout, the via line, and at level II a warning of HP backlash or crystal wear, red if it would kill. **Wording:** a single-target skill reads "<skill> · choose a target"; an all-enemies, all-allies or self skill has nothing to choose, so it reads "<skill> · all enemies · confirm" and everything in range is marked (name and HP bar on each) | Crown + code facts |
| 10 | Two control models behind an accessibility setting: **Shortcuts** (default: L2 / R2 = kind, D-pad left/right = the row under it, D-pad up/down = more skills) and **Omni D-pad** (up/down = row, left/right = change; L2 / R2 still change kind). Y / X / A use a card in both. The menu opens on the cards and focus resets each time | Crown |

**Default control model: Shortcuts.** It is the faster one (a kind is one trigger press away and the row under it one D-pad press, against moving focus up first) and it never changes what a button does; Omni D-pad is the simpler-to-learn fallback.

## Where the code is

- `UCombatCommandMenuSubsystem` keeps the menu rules and broadcasts the buttons. It stays the rule owner.
- `BattleHUDView` is the HUD as plain data and `FText`, checked headless by `BattleHUDSpec`. New display logic goes here first, with a spec.
- `UBattleHUDWidget` (cards, turn strip, statuses, damage numbers) and `UBattleCommandMenuWidget` (menu rows) only copy the view into widgets: a fixed pool built once, no Tick (TransBuffer rule).
- `ACombatPlayerController::GetMenuKeys` is the code-built key table (`CommandMenuKeysSpec`).
- Browsing state (kind, school, page, focused row) is per fight, so it lives on the menu widget, not in a subsystem field (D-064).

## Pieces, in order

Each piece is one change of at most 3 files (docs aside), ends with build + tests + a short PIE check, and waits for Crown's review before the next. Player text is `FText`. Each piece adds its line to `docs/Architecture/UISystem.md`.

| # | Piece | Files | PIE check |
|---|---|---|---|
| 1 | **Layout.** Turn strip top right, menu above the active card bottom left, idle allies as compact cards, no top banner | `BattleHUDWidget.cpp/.h`, `CombatHUD.cpp` | A fight looks like `refine_v2/a_command.png`'s frame at 1080p and 1440p |
| 2 | **Active card.** HP/EP as numbers at the larger size, low-HP colour and pulse, class line before the status icons | `BattleHUDView.h/.cpp` (+ spec), `BattleHUDWidget.cpp` | Each class reads right (Generic, "<Element> Refractor", Resonator); HP under 30% pulses |
| 3a | **Enemy HP hidden unless targeted** and the marker card (name, HP, rank, tier) over the highlighted target | `BattleHUDView.h/.cpp` (+ spec), `BattleHUDWidget.cpp` | No enemy bars in the command state; the bar appears only on the highlighted target |
| 3b | **Pick targets in the world.** Left/right (and mouse hover/click on the enemy) move the marker; the target list rows are hidden; one button flips allies/enemies for "Anyone" skills | `BattleCommandMenuWidget.cpp/.h`, `CombatPlayerController.cpp` | Cycle 3 enemies with pad, keys and mouse; camera follows as in B-400 |
| 4a | **Skill cards.** A browser model (the kind row, the selected kind's row, pages of 3) built from `FCombatCapabilities`, with card text (name, cost, target, element, school, description, blocked reason) | `BattleHUDView.h/.cpp`, `BattleHUDSpec.cpp` | None (headless): spec covers each kind's row, paging, empty schools, blocked spells, two weapons, several adapters |
| 4b | **Skill cards on screen**, mouse first: the kind row, the row under it and 3 cards, all clickable; picking an adapter or weapon tab equips it | `BattleCommandMenuWidget.cpp/.h` | Skills opens the cards; click through kinds, their rows and pages; cast one; a Resonator's active adapter follows its tab |
| 5 | **Main menu buttons.** Attack A, Skills X (opens the browser), Items Y; B is Back everywhere; Defend on R1 with the clickable corner hint | `CombatPlayerController.cpp`, `BattleCommandMenuWidget.cpp`, `CommandMenuKeysSpec.cpp` | Every action reachable on pad and keyboard; B never does anything but Back |
| 6 | **Shortcuts controls** for the browser: Y / X / A use a card, L2 / R2 kind, D-pad left/right the row under it, D-pad up/down more skills | `CombatPlayerController.cpp`, `BattleCommandMenuWidget.cpp`, `CommandMenuKeysSpec.cpp` | Cast any equipped skill with the pad alone; two presses for a first-page skill |
| 7 | **Weapon on Attack's target screen.** A basic Attack's weapon type is the top tab row; a Generic with two weapons gets two tabs there and switches with D-pad up/down or a click (never while stunned). Switching re-picks the target if the new weapon can't reach the current one | `BattleHUDView.h/.cpp` (+ spec), `BattleCommandMenuWidget.cpp`, `CombatPlayerController.cpp` | Switch on the target screen, confirm: the basic attack uses the new weapon; one weapon shows as a plain label |
| 8a | **Infusion buttons and elements.** L1 = level (cycles), L2 / R2 = element; the old source and level buttons go. The element list is built from every weapon crystal and adapter the character carries, Innate and the evolution; a source that isn't active is switched to first (free). Using several sources at once is the separate job's | `CombatPlayerController.cpp`, `ActionExecutor.cpp` (`GetAvailableInfusionSources`), `CommandMenuKeysSpec.cpp` (+ a spec for the element list) | Level and element change on the new buttons without moving the target; a Resonator infuses from any carried adapter; a two-weapon Generic's Attack from either crystal |
| 8b | **Infusion rows.** Element tabs (with the ×N count) and level tabs per decisions 8 and 9, the readout, the via line, the level II warning; clickable | `BattleHUDView.h/.cpp` (+ spec), `BattleCommandMenuWidget.cpp` | Matches `refine_final` `6` to `10`; a fixed element is one lit tab; no rows for items or a stunned character |
| 9 | **Omni D-pad** model and the setting that picks it (a console variable until a settings screen exists) | `BattleCommandMenuWidget.cpp/.h`, `CombatPlayerController.cpp` | Both models play a full turn; the hint text matches the model |

Not in this plan: the all-statuses panel (it exists since B-391; restyle later), the Hidris art pass (after B-102), Flee / Step / Let AI Play / End Turn placement (they keep today's main-menu rows until Crown says where they go).

## InputMap rows (added to `docs/Design/InputMap.md` in the piece that builds each)

Keyboard keys are proposals; they follow InputMap's standard-practice defaults when built.

| Action | Keyboard / mouse | Gamepad | Input action | Piece | Replaces |
|---|---|---|---|---|---|
| Attack (main menu) | 1 | A | `IA_Combat_MenuAttack` | 5 | unchanged |
| Skills (main menu, opens the browser) | 2 | X | `IA_Combat_MenuSkills` | 5 | Skills X + Spells B |
| Items (main menu) | 3 | Y | `IA_Combat_MenuItems` | 5 | 4 / D-pad Up |
| Back (everywhere) | Esc | B | `IA_Combat_MenuBack` | 5 | Back as a menu row; Spells on B |
| Defend | G / click the hint | RB (R1) | `IA_Combat_MenuDefend` | 5 | Y |
| Use card 1 / 2 / 3 | 1 / 2 / 3 / click | Y / X / A | `IA_Combat_MenuCard1..3` | 6 | new |
| Kind previous / next | Q / E | LT / RT (L2 / R2) | `IA_Combat_MenuKindPrev`, `IA_Combat_MenuKindNext` | 6 | new |
| Menu up / down (Shortcuts: more skills; on Attack's target screen: weapon; Omni: row) | Up / Down / wheel | D-pad Up / Down | `IA_Combat_MenuUp`, `IA_Combat_MenuDown` | 6, 7, 9 | new |
| Menu left / right (Shortcuts: the row under the kinds; Omni: change) | Left / Right | D-pad Left / Right | `IA_Combat_MenuLeft`, `IA_Combat_MenuRight` | 6, 9 | Switch weapon (6 / D-pad Left), Change ring (7 / D-pad Right) |
| Target previous / next | Left / Right / mouse hover | D-pad or stick Left / Right | `IA_Combat_TargetPrev`, `IA_Combat_TargetNext` | 3b | CommonUI focus on target rows |
| Flip allies / enemies (target picker) | Tab is taken (skip cinematic): TODO pick | Y | `IA_Combat_TargetSide` | 3b | the Allies / Enemies menu step |
| Infusion level (cycles) | T | LB (L1) | `IA_Combat_MenuInfusionLevel` | 8a | RB |
| Infusion element previous / next (target screen) | R (next) | LT / RT (L2 / R2) | `IA_Combat_MenuInfusionElementPrev`, `IA_Combat_MenuInfusionElementNext` | 8a | LB press (`IA_Combat_MenuInfusionSource`) |

Flee stays main-menu only (5 / D-pad Down).

## Open points for Crown

1. ~~A backlog number~~ B-435.
2. Where Flee, Step Forward / Back, Let AI Play and End Turn sit in the new main menu.
3. Whether a level II cast that would kill needs a second press, or the red warning is enough.
4. Reality: no special source rule was found in the infusion code beyond its stat boosts; confirm decision 9's "Evolution or Innate" covers it.
5. Picking an adapter or weapon tab makes it the active one, so just browsing changes gear (free, but it also decides which adapter an infused ability draws from). The alternative is to switch only when a skill from that tab is used; say if that is preferred.
6. Two weapons of the same type (Sword and Sword) would give two identical tabs. Proposal: number them (Sword I, Sword II).
7. Resonate has no school row, so one adapter's spells (at most 6) share two pages and the school shows on each card.
8. Weapon types come from the code (Sword, Greatsword, Spear, Staff, Dagger, Axe, Hammer, Bow, Gauntlets, Scythe, Gun). "Sword & Shield" is not one of them; showing it needs a new display name per weapon family. It now matters only for the weapon row and the via line.
9. Renaming "Resonate (A)" to "Resonate" in the game text happens in piece 4b, with the kind row.
