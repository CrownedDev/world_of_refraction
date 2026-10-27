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
| Guard (timed; Perfect Guard) | (editor context) | (editor context) | (editor) | Built: CC to fill from the editor |
| Dodge (timed) | (editor context) | (editor context) | (editor) | Built: CC to fill from the editor |
| Menu confirm / back | Enter / Esc | A (Cross) / B (Circle) | CommonUI Click / Back | Built (B-103) |

## Notes

- Same verb, same key across modes where possible (switch source = R / LB in the field and in combat).
- Exploration and combat use separate mapping contexts, so a key may mean different things in each (RB = field attack on the map, infusion level in combat).
- Keyboard double-tap and gamepad double-click windows: one named constant (`// TODO: tune`).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | Created: built keys, Crown's double-click L3, standard-practice proposals | docs/input-map |
