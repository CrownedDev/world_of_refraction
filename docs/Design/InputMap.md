# Input Map

**Status:** living list (Crown, 2026-10-27): fixed keys for now, **standard practice** defaults, one unique input action per row so rebinding can come later (D-063). **Every new input gets its row here in the same change** (CC). Change any key: it's one row.
Gamepad names are Xbox (PlayStation in brackets). `Built` = in code today; `Planned` = decided, not built; `Proposed` = standard-practice default, Crown may change.

## Exploration (hub and trial map)

| Action | Keyboard / mouse | Gamepad | Input action | State |
|---|---|---|---|---|
| Move | WASD | Left stick | — | Built |
| Camera | Mouse | Right stick | — | Built |
| Sprint (fast, free) | Left Shift (hold) | L3 click (toggle) | `IA_Sprint` | Built (key: check editor) |
| Field movement (double-click sprint: Enhanced Sprint or the equipped field movement; D-099) | Double-tap Left Shift | **Double-click L3** | `IA_Field_MovementSkill` | Planned (Crown) |
| Jump | Space | A (Cross) | `IA_Field_Jump` | Proposed |
| Field attack (hold: charged; Water source → Ice) | Left mouse | RB (R1) | `IA_Field_Attack` | Proposed |
| Aim (free aim for field spells / attacks) | Right mouse (hold) | LT (L2) (hold) | `IA_Field_Aim` | Proposed |
| Field command menu (D-099) | Q | Y (Triangle) | `IA_Field_CommandMenu` | Proposed |
| Switch field source (innate / weapon / ring) | R | LB (L1) | `IA_Field_SwitchSource` | Proposed (matches combat infusion source) |
| Interact (doors, merchants, recruits, fragments, rest spots) | E | X (Square) | `IA_Interact` | Built (gamepad key: check editor) |
| Menu / pause | Esc | Menu (Options) | `IA_ToggleMenu` | Built (keys: check editor) |
| Map | M | View (Touchpad) | `IA_Field_Map` | Proposed |
| Journal (memories, lore) | J | D-pad Up | `IA_Field_Journal` | Proposed |

## Combat

| Action | Keyboard / mouse | Gamepad | Input action | State |
|---|---|---|---|---|
| Defend (from the menu) | G | Y (Triangle) | `IA_Combat_MenuDefend` | Built (B-093) |
| Infusion source | R | LB (L1) | `IA_Combat_MenuInfusionSource` | Built (B-093) |
| Infusion level | T | RB (R1) | `IA_Combat_MenuInfusionLevel` | Built (B-093) |
| Attack (main menu only) | 1 | A (Cross) | `IA_Combat_MenuAttack` | Built (B-391, Q-391) |
| Skills: Abilities, else Resonate (W) (main menu only) | 2 | X (Square) | `IA_Combat_MenuSkills` | Built (B-391, Q-391) |
| Spells: Refractions, else Breakthrough, else Resonate (R) (main menu only) | 3 | B (Circle) | `IA_Combat_MenuSpells` | Built (B-391, Q-391). B is Back everywhere but the main menu, where there is nothing to go back to |
| Items (main menu only) | 4 | D-pad Up | `IA_Combat_MenuItems` | Built (B-391, Q-391) |
| Flee (main menu only) | 5 | D-pad Down | `IA_Combat_MenuFlee` | Built (B-391, Q-391) |
| Switch weapon (main menu only) | 6 | D-pad Left | `IA_Combat_MenuSwitchWeapon` | Built (B-391, Q-391) |
| Change ring (main menu only) | 7 | D-pad Right | `IA_Combat_MenuChangeRing` | Built (B-391, Q-391) |
| End Turn (main menu only; shows with `wor.Turns.ActionPips 1`): ends the turn, the actions left stay banked | Backspace | View (Touchpad) | `IA_Combat_MenuEndTurn` | Written, unbuilt (B-170e, Q-170e2; `feature/B-170e-action-pips-ui-cloud`). Proposed default: Backspace is XCOM 2's End Turn key; View is the one gamepad button combat doesn't use. Code-built like the other menu keys (no `IA_` asset) |
| Guard (timed; Perfect Guard) | (editor context) | (editor context) | (editor) | Built: CC to fill from the editor. Co-op: a client's press goes to the server (`Server_SubmitDefenseInput`, B-229) |
| Dodge (timed) | (editor context) | (editor context) | (editor) | Built: one Dodge (D-135, B-343: the `DodgeLeftAction` binding; no left / right split, no Parry). CC to fill from the editor; removing `IA_Parry` and `IA_DodgeRight` is an editor step. Co-op: as Guard (B-229) |
| Menu confirm / back | Enter / Esc | A (Cross) / B (Circle) | CommonUI Click / Back | Built (B-103) for the old HUD. **New battle HUD (B-391, `wor.UI.NewHUD 1`):** picking is a mouse click and Back is a menu row; a gamepad can't pick inside a submenu yet (Q-391). Co-op: a client's confirmed pick, with its infusion source and level, goes to the server (`Server_SubmitMenuPick`, B-229); back stays local |
| Move the target highlight (camera follows, B-400) | Arrow keys (menu focus) / mouse hover over a target button | D-pad / left stick (menu focus) | CommonUI navigation (no new input action) | Built (B-400): focus or hover on a target button calls `HighlightTarget`; debug `wor.Camera.TargetCycle` |

## Notes

- Same verb, same key across modes where possible (switch source = R / LB in the field and in combat).
- Exploration and combat use separate mapping contexts, so a key may mean different things in each (RB = field attack on the map, infusion level in combat).
- Keyboard double-tap and gamepad double-click windows: one named constant (`// TODO: tune`).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | Created: built keys, Crown's double-click L3, standard-practice proposals | docs/input-map |
| 2026-11-03 | B-229: Guard, Dodge and the menu confirm reach the server from a co-op client (no key changes) | feature/B-229-input-rpcs |
| 2026-11-03 | B-343 (D-135): only Guard and Dodge; Parry and Dodge Right no longer bound | chore/B-343-guard-dodge-only |
| 2026-11-09 | B-391 (Q-391): main-menu category keys (Attack, Skills, Spells, Items, Flee, switches); the new battle HUD's menu shows each row's key | feature/B-391-battle-hud-cloud |
| 2026-11-09 | B-170e (Q-170e2): End Turn on Backspace / View, main menu only, in an action-pips fight | feature/B-170e-action-pips-ui-cloud |
| 2026-11-09 | B-400: menu focus / hover on a target button now moves the target highlight (and the camera); no new key or input action | feature/B-400-target-cut-cloud |
