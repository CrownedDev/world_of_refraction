# Battle HUD refinement — build plan

**Status:** approved by Crown (2026-11-13); building one piece at a time, Crown's PIE review between pieces. Backlog: **B-435**. Branch: `feature/battle-hud-refine` (from `dev` at 8831e4a1f, after B-424).
**Mockups (approved direction):** `docs/Design/screen_mockups/combat/hud/` — `refine_v2/` (layout, target marker), `refine_v3b/` (infusion strip: `c_`, `d_`, `e_`), `refine_v3f/` (skill menu, one image per kind). `refine_v1/`, `refine_v3/`, `refine_v3c/`, `refine_v3d/` and `refine_v3e/` are superseded history (v3d still shows the Omni focus frame).
**Checks against:** FeelGuide 5 and 13, `UI_Theme.md`, Expedition 33's battle menu.

## Decisions this plan builds

| # | Decision | Source |
|---|---|---|
| 1 | Layout: turn order top right; menu stacked above the active card bottom left; idle allies as name + HP cards | Crown, refine_v1/v2 |
| 2 | Active card: big HP/EP numbers, low-HP pulse, class before the status icons ("Darkness Refractor: …"; `Caster` shows as "Refractor") | Crown, refine_v2 |
| 3 | Enemy HP is hidden unless targeted; the in-world target marker shows name, HP, rank, tier | Crown, refine_v2 |
| 4 | Skills: 3 cards on Y / X / A with the description inline; at most 6 per school = 2 pages | Crown, refine_v3 |
| 5 | **Two rows, no labels.** One **kind row**: Abilities, Refractions, Breakthrough, Resonate (A), only the kinds the character has, in the code's names. There is **no Resonate (W) kind**: weapon spells are gone (B-098), so everything a weapon gives is an Ability. Above the kind row, **one row tied to the selected kind**: Refractions → its schools (only schools that hold a spell); Resonate (A) → the adapters; Breakthrough → its sources (Evolution, or the adapter that carries it); Abilities → the two weapon **types** for a Generic carrying two weapons, otherwise no row. Abilities is one list: the weapon's abilities, the crystal's extra slots (B-410) and the augment stone's abilities, so it can run past two pages; the stone gets no tab of its own (it is an attachment of that weapon, a few entries at most). Weapons always show as their type (Sword, Hammer…), never the weapon's name. The cards sit below | Crown, refine_v3f |
| 6 | **Picking an adapter or weapon tab equips it.** It becomes the active one, free as today (CL-18, CL-26, ET-19). The main menu keeps a weapon toggle (D-pad left/right) so a basic Attack can still pick its weapon without opening Skills | This plan (open point 5) |
| 7 | B is always Back. Defend moves to R1, shown as a clickable corner hint (on by default, a setting hides it) | Crown |
| 8 | Infusion on L1 only: tap = level, hold + left/right = source. Levels show as **— / I / II** (L0 is "—": it means no infusion, and "0" is not a numeral in that set) | Crown; "—" picked here |
| 9 | Infusion strip on the target picker: fixed source label for spells; source tabs only where a choice exists (attacks, abilities; evolution spells of Broken Darkness / Reality characters); hidden for item spells, broken-adapter spells and stunned characters; level II warns of HP backlash or crystal wear, red if it would kill | Crown + code facts |
| 10 | Two control models behind an accessibility setting: **Shortcuts** (default: L2 / R2 = kind, D-pad left/right = the row above it, D-pad up/down = more skills) and **Omni D-pad** (up/down = row, left/right = change; L2 / R2 still change kind). Y / X / A use a card in both. The menu opens on the cards and focus resets each time | Crown |

**Default control model: Shortcuts.** It is the faster one (a kind is one trigger press away and its row one D-pad press, against moving focus up first) and it never changes what a button does; Omni D-pad is the simpler-to-learn fallback.

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
| 4b | **Skill cards on screen**, mouse first: the two rows and 3 cards, all clickable; picking an adapter or weapon tab equips it | `BattleCommandMenuWidget.cpp/.h` | Skills opens the cards; click through kinds, their rows and pages; cast one; a Resonator's active adapter follows its tab |
| 5 | **Main menu buttons.** Attack A, Skills X (opens the browser), Items Y; B is Back everywhere; Defend on R1 with the clickable corner hint | `CombatPlayerController.cpp`, `BattleCommandMenuWidget.cpp`, `CommandMenuKeysSpec.cpp` | Every action reachable on pad and keyboard; B never does anything but Back |
| 6 | **Shortcuts controls** for the browser: Y / X / A use a card, L2 / R2 kind, D-pad left/right the row above it, D-pad up/down more skills | `CombatPlayerController.cpp`, `BattleCommandMenuWidget.cpp`, `CommandMenuKeysSpec.cpp` | Cast any equipped skill with the pad alone; two presses for a first-page skill |
| 7 | **Weapon toggle on the main menu.** A small line on the main menu names the active weapon; D-pad left/right or a click switches it (Generic with two weapons only; never while stunned) | `BattleHUDView.h/.cpp` (+ spec), `BattleCommandMenuWidget.cpp` | Switch, then Attack: the basic attack uses the new weapon; the line is absent for everyone else |
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
| Kind previous / next | Q / E | LT / RT (L2 / R2) | `IA_Combat_MenuKindPrev`, `IA_Combat_MenuKindNext` | 6 | new |
| Menu up / down (Shortcuts: more skills; Omni: row) | Up / Down / wheel | D-pad Up / Down | `IA_Combat_MenuUp`, `IA_Combat_MenuDown` | 6, 9 | new |
| Menu left / right (Shortcuts: the row above the kinds in the browser, weapon on the main menu; Omni: change) | Left / Right | D-pad Left / Right | `IA_Combat_MenuLeft`, `IA_Combat_MenuRight` | 6, 7, 9 | Switch weapon (6 / D-pad Left), Change ring (7 / D-pad Right) |
| Target previous / next | Left / Right / mouse hover | D-pad or stick Left / Right | `IA_Combat_TargetPrev`, `IA_Combat_TargetNext` | 3b | CommonUI focus on target rows |
| Flip allies / enemies (target picker) | Tab is taken (skip cinematic): TODO pick | Y | `IA_Combat_TargetSide` | 3b | the Allies / Enemies menu step |
| Infusion level (tap) | T | LB (L1) tap | `IA_Combat_MenuInfusionLevel` | 8a | RB |
| Infusion source (hold + left / right) | R (cycles) | LB (L1) hold + Left / Right | `IA_Combat_MenuInfusionSource` | 8a | LB press |

Flee stays main-menu only (5 / D-pad Down).

## Open points for Crown

1. ~~A backlog number~~ B-435.
2. Where Flee, Step Forward / Back, Let AI Play and End Turn sit in the new main menu.
3. Whether a level II cast that would kill needs a second press, or the red warning is enough.
4. Reality: no special source rule was found in the infusion code beyond its stat boosts; confirm decision 9's "Evolution or Innate" covers it.
5. Picking an adapter or weapon tab makes it the active one, so just browsing changes gear (free, but it also decides which adapter an infused ability draws from). The alternative is to switch only when a skill from that tab is used; say if that is preferred.
6. Two weapons of the same type (Sword and Sword) would give two identical tabs. Proposal: number them (Sword I, Sword II).
7. Resonate (A) has no school row, so one adapter's spells (at most 6) share two pages and the school shows on each card.
