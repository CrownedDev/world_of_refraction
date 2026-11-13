# Battle HUD refinement — build plan

**Status:** plan, waiting for Crown's go (2026-11-13). No code yet. Branch: `feature/battle-hud-refine` (from `dev` at 8831e4a1f, after B-424).
**Mockups (approved direction):** `docs/Design/screen_mockups/combat/hud/` — `refine_v2/` (layout, target marker), `refine_v3b/` (infusion strip: `c_`, `d_`, `e_`), `refine_v3c/` (Shortcuts controls), `refine_v3d/` (Omni D-pad controls). `refine_v1/` and `refine_v3/` are superseded history.
**Checks against:** FeelGuide 5 and 13, `UI_Theme.md`, Expedition 33's battle menu.

## Decisions this plan builds

| # | Decision | Source |
|---|---|---|
| 1 | Layout: turn order top right; menu stacked above the active card bottom left; idle allies as name + HP cards | Crown, refine_v1/v2 |
| 2 | Active card: big HP/EP numbers, low-HP pulse, class before the status icons ("Darkness Refractor: …"; `Caster` shows as "Refractor") | Crown, refine_v2 |
| 3 | Enemy HP is hidden unless targeted; the in-world target marker shows name, HP, rank, tier | Crown, refine_v2 |
| 4 | Skills: 3 cards on Y / X / A with the description inline; at most 6 per school = 2 pages | Crown, refine_v3 |
| 5 | Kind tabs use the code's names (Abilities, Refractions, Breakthrough, Resonate (W), Resonate (A)); spells add a School row with only the schools that hold a spell | Crown, refine_v3b/c |
| 6 | Gear row: Switch Weapon / Switch Adapter as a tab row (free, as today: CL-18, CL-26, ET-19) | Crown, refine_v3 |
| 7 | B is always Back. Defend moves to R1, shown as a clickable corner hint (on by default, a setting hides it) | Crown |
| 8 | Infusion on L1 only: tap = level, hold + left/right = source. Levels show as **— / I / II** (L0 is "—": it means no infusion, and "0" is not a numeral in that set) | Crown; "—" picked here |
| 9 | Infusion strip on the target picker: fixed source label for spells; source tabs only where a choice exists (attacks, abilities; evolution spells of Broken Darkness / Reality characters); hidden for item spells, broken-adapter spells and stunned characters; level II warns of HP backlash or crystal wear, red if it would kill | Crown + code facts |
| 10 | Two control models behind an accessibility setting: **Shortcuts** (default) and **Omni D-pad**. Y / X / A use a card in both; L2 / R2 page in both. The menu opens on the cards and focus resets each time | Crown |

**Default control model: Shortcuts.** It is the faster one (switching adapter is 1 press against 4) and every row names its own button; Omni D-pad is the simpler-to-learn fallback.

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
| 4a | **Skill cards.** A browser model (kinds, schools, pages of 3) built from `FCombatCapabilities`, with card text (name, cost, target, element, description, blocked reason) | `BattleHUDView.h/.cpp`, `BattleHUDSpec.cpp` | None (headless): spec covers paging, empty schools, blocked spells |
| 4b | **Skill cards on screen**, mouse first: 3 cards, page count, kind tabs and School row, all clickable | `BattleCommandMenuWidget.cpp/.h` | Skills opens the cards; click through kinds, schools and pages; cast one |
| 5 | **Main menu buttons.** Attack A, Skills X (opens the browser), Items Y; B is Back everywhere; Defend on R1 with the clickable corner hint | `CombatPlayerController.cpp`, `BattleCommandMenuWidget.cpp`, `CommandMenuKeysSpec.cpp` | Every action reachable on pad and keyboard; B never does anything but Back |
| 6 | **Shortcuts controls** for the browser: Y / X / A use a card, L2 / R2 page then next kind, D-pad up/down school | `CombatPlayerController.cpp`, `BattleCommandMenuWidget.cpp`, `CommandMenuKeysSpec.cpp` | Cast any equipped skill with the pad alone; two presses for a first-page skill |
| 7 | **Gear row.** Switch Weapon / Switch Adapter as a labelled tab row, D-pad left/right, click | `BattleHUDView.h/.cpp` (+ spec), `BattleCommandMenuWidget.cpp` | Generic with two weapons and Resonator with 2+ adapters switch freely; row absent otherwise and while stunned |
| 8a | **Infusion on L1.** Tap = level, hold + left/right = source; the old source button goes | `CombatPlayerController.cpp`, `CombatCommandMenuSubsystem.cpp`, `CommandMenuKeysSpec.cpp` | Level and source change as before, on one button |
| 8b | **Infusion strip.** Source label or tabs per decision 9, levels — / I / II, damage / size / EP readout, level II warning | `BattleHUDView.h/.cpp` (+ spec), `BattleCommandMenuWidget.cpp` | Matches `refine_v3b` `c_`, `d_`, `e_`; no strip for items or a stunned character |
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
| Page previous / next | Q / E / wheel | LT / RT (L2 / R2) | `IA_Combat_MenuPagePrev`, `IA_Combat_MenuPageNext` | 6 | new |
| Menu up / down (Shortcuts: school; Omni: row) | Up / Down | D-pad Up / Down | `IA_Combat_MenuUp`, `IA_Combat_MenuDown` | 6, 9 | new |
| Menu left / right (Shortcuts: gear; Omni: change) | Left / Right | D-pad Left / Right | `IA_Combat_MenuLeft`, `IA_Combat_MenuRight` | 7, 9 | Switch weapon (6 / D-pad Left), Change ring (7 / D-pad Right) |
| Target previous / next | Left / Right / mouse hover | D-pad or stick Left / Right | `IA_Combat_TargetPrev`, `IA_Combat_TargetNext` | 3b | CommonUI focus on target rows |
| Flip allies / enemies (target picker) | Tab is taken (skip cinematic): TODO pick | Y | `IA_Combat_TargetSide` | 3b | the Allies / Enemies menu step |
| Infusion level (tap) | T | LB (L1) tap | `IA_Combat_MenuInfusionLevel` | 8a | RB |
| Infusion source (hold + left / right) | R (cycles) | LB (L1) hold + Left / Right | `IA_Combat_MenuInfusionSource` | 8a | LB press |

Flee stays main-menu only (5 / D-pad Down).

## Open points for Crown

1. A backlog number for this work (the branch has none yet).
2. Where Flee, Step Forward / Back, Let AI Play and End Turn sit in the new main menu.
3. Whether a level II cast that would kill needs a second press, or the red warning is enough.
4. Reality: no special source rule was found in the infusion code beyond its stat boosts; confirm decision 9's "Evolution or Innate" covers it.
