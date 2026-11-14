# Input Map

**Status:** living list (Crown, 2026-10-27): fixed keys for now, **standard practice** defaults, one unique input action per row so rebinding can come later (D-063). **Every new input gets its row here in the same change** (CC). Change any key: it's one row.
Gamepad names are Xbox (PlayStation in brackets). `Built` = in code today; `Planned` = decided, not built; `Proposed` = standard-practice default, Crown may change.

## Exploration (hub and trial map)

| Action | Keyboard / mouse | Gamepad | Input action | State |
|---|---|---|---|---|
| Move | WASD | Left stick | — | Built |
| Camera | Mouse | Right stick | — | Built |
| Sprint (fast, free) | Left Shift (hold) | L3 click (toggle) | `IA_Sprint` | Built (key: check editor) |
| Field movement (double-click sprint: Enhanced Sprint or the equipped field movement; D-099) | Double-tap Left Shift | **Double-click L3** | `IA_Field_MovementSkill` | Code bound (B-214a: `AWoRHubController`, config `DefaultFieldMovementSkillAction`, Triggered). B-214g editor step: the asset (Digital) with a double-tap trigger (Tap ×2 within 0.3 s; Crown checks the feel), mapped, the ini line uncommented; until then `wor.Field.Activate`. With no skill picked it dashes (Q-214a) |
| Jump | Space | A (Cross) | `IA_Field_Jump` | Proposed |
| Field attack (hold 0.4 s, read on release: the held attack; Water source → Ice) | Left mouse | RB (R1) | `IA_Field_Attack` | Code bound (B-214g: config `DefaultFieldAttackAction`; Started stamps the press, Completed attacks; held 0.4 s or more = the held attack). Editor step: the asset (Digital, no trigger), mapped; until then `wor.Field.Attack [Hold]` |
| Aim (free aim for field spells / attacks) | Right mouse (hold) | LT (L2) (hold) | `IA_Field_Aim` | Proposed |
| Field command menu (D-099) | Q | Y (Triangle) | `IA_Field_CommandMenu` | Proposed |
| Switch field source (innate / weapon / ring) | R | LB (L1) | `IA_Field_SwitchSource` | Code bound (B-214g: config `DefaultFieldSwitchSourceAction`, Started → the field attack's source Innate → Weapon → Ring; matches combat infusion source). Editor step: the asset, mapped; until then `wor.Field.SwitchSource` |
| Fly up / down (while flying, D-099) | Space (up) / Left Ctrl (down) | A (Cross) up / R3 click down | `IA_Field_FlyVertical` | Code bound (B-214g, Q-214g: config `DefaultFieldFlyVerticalAction`, a 1D axis, Triggered → `AddFlightVerticalInput`; Minecraft-style flight: jump up, crouch / right stick click down; B stays Back). Space / A are also Jump: a jump does nothing while flying. Editor step: the asset (Axis1D), Space / A positive, Left Ctrl / R3 with a Negate modifier; until then `wor.Field.FlyVertical <1\|-1>` |
| Interact (doors, merchants, recruits, fragments, rest spots; the hub HUD's prompt shows this key, B-443) | E | X (Square) | `IA_Interact` | Built (gamepad key: check editor) |
| Menu / pause | Esc | Menu (Options) | `IA_ToggleMenu` | Built (B-439: opens the game menu, see Menus; keys: check editor) |
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
| Guard (timed; Perfect Guard) | (editor context) | (editor context) | (editor) | Built: CC to fill from the editor. Co-op: a client's press goes to the server (`Server_SubmitDefenseInput`, B-229) |
| Dodge (timed) | (editor context) | (editor context) | (editor) | Built: one Dodge (D-135, B-343: the `DodgeLeftAction` binding; no left / right split, no Parry). CC to fill from the editor; removing `IA_Parry` and `IA_DodgeRight` is an editor step. Co-op: as Guard (B-229) |
| Menu confirm / back | Enter / Esc | A (Cross) / B (Circle) | CommonUI Click / Back | Built (B-103) for the old HUD. **New battle HUD (B-391, `wor.UI.NewHUD 1`):** picking is a mouse click and Back is a menu row; a gamepad can't pick inside a submenu yet (Q-391). Co-op: a client's confirmed pick, with its infusion source and level, goes to the server (`Server_SubmitMenuPick`, B-229); back stays local |
| Move the target highlight (camera follows, B-400) | Arrow keys (menu focus) / mouse hover over a target button | D-pad / left stick (menu focus) | CommonUI navigation (no new input action) | Built (B-400): focus or hover on a target button calls `HighlightTarget`; debug `wor.Camera.TargetCycle` |
| Skip boss cinematic (opening, phase change) | Tab | Menu (Options) | `IA_Combat_SkipCinematic` | Built (B-406b, Q-406b): press, in the code-built menu-keys context; does nothing unless a boss cinematic plays. Tab: no row here uses it (not Space, the usual dodge key and Slate's Accept; not Enter / Esc / Backspace / 1–7). The key doesn't consume input, so **PIE must confirm** the editor combat context puts no Guard / Dodge / pause on Tab or Menu (`docs/PIE/B-406b.md`). Co-op: ends this machine's view only; the server's hold ends on its own end or its cap |

## Menus (CommonUI screens)

Fixed keys read by the screen itself while it's open (game input is off then), so they need no input action or mapping context yet. `UIScreens.md` proposes the shared set (tabs, sub-tabs, screen actions) for the screens to come; each gets its row here when it's built.

| Action | Keyboard / mouse | Gamepad | Input action | State |
|---|---|---|---|---|
| Open the game menu | Esc | Menu (Options) | `IA_ToggleMenu` | Built (B-439): `AWoRHubController::OpenGameMenu`, hub and trial map (not in a battle). Console: `wor.UI.Pause`. In the editor Esc ends PIE, so use the gamepad or the console |
| Move up / down a list | Up / Down, W / S, mouse over a row | D-pad, left stick | none (the screen's keys) | Built (B-439, game menu) |
| Select | Enter, Space, left click | A (Cross) | none | Built (B-439, game menu) |
| Hold to confirm (Quit) | hold Enter, Space or the left button | hold A (Cross) | none | Built (B-439, D-176): 2 seconds, after a first press asks |
| Hold to confirm (New Game over a save, the title) | hold Enter, Space or the left button | hold A (Cross) | none | Built (B-467): 2 seconds, after New Game asks; letting go early does nothing, B cancels |
| Credits: scroll faster | hold Enter, Space or the left button | hold A (Cross) | none | Built (B-470): five times faster while held; B / Esc leaves |
| Previous / next page (the top tab row) | Q / E, click a tab | LB / RB (L1 / R1) | none (the screen's keys; `UIScreens.md` proposes `IA_UI_TabPrev` / `Next`) | Built (B-452, settings screen) |
| Change a setting | Left / Right, A / D, Enter, click the row | D-pad left / right, left stick, A (Cross) | none | Built (B-452, settings screen). These letters are read only while a menu has the keys, so they don't clash with the field's |
| Previous / next character (sub-tabs) | Z / C, click a tab | LT / RT (L2 / R2) | none (the screen's keys; `UIScreens.md` proposes `IA_UI_SubTabPrev` / `Next`) | Built (B-440a, loadout screen) |
| Loadout: change the lit gear row (opens the picker); in the picker, put the lit piece in | Enter, Space, left click | A (Cross) | none (the screen's keys) | Built (B-440b, loadout screen): `WoRMenuStyle::IsConfirmKey`; B / Esc closes the picker |
| Loadout: unequip the lit gear row | X | X (Square) | none (the screen's keys) | Built (B-440b, loadout screen): `WoRMenuStyle::IsToggleKey` |
| Move across a grid of tiles | arrows, W A S D, mouse over a tile | D-pad, left stick | none | Built (B-441, inventory screen) |
| Open a tile's verb menu, choose a verb or a receiver | Enter, Space, left click | A (Cross) | none | Built (B-441, inventory screen) |
| Whole stack on / off (a stack verb moves one, or all) | X | X (Square) | none (the screen's keys) | Built (B-441, inventory screen; B-456a, vendor screen: sell or dismantle one, or the whole stack): `WoRMenuStyle::IsToggleKey`, the screen's own switch |
| Inventory: sort the tiles (Kind, Tier, Name) | R | Y (Triangle) | none (the screen's keys; `UIScreens.md` proposes `IA_UI_Action2`) | Built (B-461, inventory screen; Q-461): `WoRMenuStyle::IsSecondActionKey` |
| Inventory: the filter row (Gear and Chest tabs) | Up from the top row of tiles, then Left / Right; Down, Enter or Esc back to the tiles; click a filter | D-pad or left stick up, then left / right; down, A or B back | none | Built (B-461, inventory screen; Q-461) |
| Shop: put the focused line in the cart / take one back | Enter, Space, Right, D, left click / Left, A, right click | A (Cross), D-pad right, stick right / D-pad left, stick left | none (the screen's keys) | Built (B-456b, vendor screen, Buy tab; Q-456b) |
| Shop: buy the cart | X, click "Purchase" | X (Square) | none (the screen's keys) | Built (B-456b, vendor screen, Buy tab): `WoRMenuStyle::IsToggleKey` |
| Shop: who it is bought for ("Buying for") | R, Z / C, click the line | Y (Triangle), LT / RT | none (the screen's keys; `UIScreens.md` proposes `IA_UI_Action2`) | Built (B-456b, vendor screen, Buy tab): `WoRMenuStyle::IsSecondActionKey` |
| Journal: quest filter, memory character, Ready / Played (sub-tabs) | Z / C, click a tab | LT / RT (L2 / R2) | none (the screen's keys) | Built (B-463, journal screen; D-198): `WoRMenuStyle::IsSubTabPrevKey` / `NextKey` |
| Journal: show / hide the focused secret quest | X | X (Square) | none (the screen's keys) | Built (B-463, D-198): `WoRMenuStyle::IsToggleKey`; the hint reads Show or Hide |
| Journal: hide every shown secret quest ("Hide all") | R | Y (Triangle) | none (the screen's keys) | Built (B-463, D-198): `WoRMenuStyle::IsSecondActionKey`, the Quests page only |
| Journal: play a ready conversation's next line | Enter, Space, left click | A (Cross) | none | Built (B-463, Q-463): until NPC talk (B-471) |
| Journal: scroll the reader's text | Page Up / Page Down, mouse wheel | Right stick up / down | none (the screen's keys) | Built (B-463) |
| Back / close | Esc, Backspace | B (Circle), Menu (Options) | CommonUI Back when `BP_CommonInputData` has it, else the screen's keys | Built (B-439, game menu) |

## Mapping contexts (B-129)

Higher priority wins a key two contexts both map. The numbers live in `WoRInputPriority` (`Public/Player/InputPriorities.h`); no literal priority is left in C++.

| Context | Added by | Priority | State |
|---|---|---|---|
| Exploration (hub and trial map) | `AExplorationPlayerController` (`ExplorationMappingContext`; a BP may override `ExplorationMappingPriority`) | `EXPLORATION` = 0 | Built (B-129: named) |
| Combat (editor-authored: Guard, Dodge) | `ACombatPlayerController::CombatMappingContext` | `COMBAT` = 0 | Built (B-129: named) |
| Combat menu keys (code-built: Defend, infusion, category keys, skip cinematic) | `ACombatPlayerController::BuildMenuKeys` | `COMBAT_MENU_KEYS` = `COMBAT` + 1 | Built (B-129: named; was `CombatMenuKeys::CONTEXT_PRIORITY`) |

## Notes

- Same verb, same key across modes where possible (switch source = R / LB in the field and in combat).
- Exploration and combat use separate mapping contexts, so a key may mean different things in each (RB = field attack on the map, infusion level in combat).
- Exploration and combat share priority 0 because their two controllers never run at the same time; give one a different priority before a map ever runs both.
- Keyboard double-tap and gamepad double-click windows: one named constant (`// TODO: tune`).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | Created: built keys, Crown's double-click L3, standard-practice proposals | docs/input-map |
| 2026-11-03 | B-229: Guard, Dodge and the menu confirm reach the server from a co-op client (no key changes) | feature/B-229-input-rpcs |
| 2026-11-03 | B-343 (D-135): only Guard and Dodge; Parry and Dodge Right no longer bound | chore/B-343-guard-dodge-only |
| 2026-11-09 | B-391 (Q-391): main-menu category keys (Attack, Skills, Spells, Items, Flee, switches); the new battle HUD's menu shows each row's key | feature/B-391-battle-hud-cloud |
| 2026-11-09 | B-170e (Q-170e2): End Turn on Backspace / View, main menu only, in an action-pips fight | feature/B-170e-action-pips-ui-cloud |
| 2026-11-12 | B-170e (Crown, 2026-11-12): End Turn's key removed; **a key for End Turn comes later** and gets its row here when it lands. The End Turn button stays (and a setting can hide it) | feature/B-170e-action-pips-ui-cloud |
| 2026-11-09 | B-400: menu focus / hover on a target button now moves the target highlight (and the camera); no new key or input action | feature/B-400-target-cut-cloud |
| 2026-11-09 | B-400b: in the new battle HUD (B-391), hovering a target row moves the target highlight; its rows stay non-focusable, so keys still pick | feature/B-400b-new-hud-target-highlight |
| 2026-11-09 | B-406b: Skip boss cinematic (Tab / Menu), `IA_Combat_SkipCinematic` (review: moved off Space) | feature/B-406b-boss-cinematic-skip |
| 2026-11-12 | B-129: mapping-context priorities named (`WoRInputPriority`: exploration 0, combat 0, combat menu keys 1) and listed under "Mapping contexts"; no key or priority changes | feature/B-129-input-priorities-cloud |
| 2026-11-14 | B-439: the game menu opens on `IA_ToggleMenu` (bound in C++); new **Menus** section: move, select, hold to confirm, back, read by the screen itself | feature/B-439-pause-menu |
| 2026-11-14 | B-440a: previous / next character on LT / RT (Z / C) in the loadout screen, read by the screen itself | feature/B-440-loadout-screen |
| 2026-11-14 | B-440b: A Change (the gear picker) and X Unequip in the loadout screen, read by the screen itself | feature/B-440b-loadout-change-gear |
| 2026-11-14 | B-443: the hub HUD prompt shows the Interact key (E / X); no new key | feature/B-443-hub-hud |
| 2026-11-14 | B-452: previous / next page on LB / RB (Q / E) and change a setting on Left / Right (A / D) in the settings screen, read by the screen itself | feature/B-452-settings-screen |
| 2026-11-14 | B-456d: the vendor screen's Repair tab: A repairs the focused piece, X repairs everything (each asks first); Curses: A clears; Merge: Left / Right picks the tier to make, A merges; read by the screen itself | feature/B-456d-spiritualist-services |
| 2026-11-14 | B-467: the title screen: Up / Down and Select as every menu; New Game over a save is a 2-second hold of Select; Back closes a panel, never the title; read by the screen itself | feature/B-467-title-screen |
| 2026-11-14 | B-470: the credits: hold Select to scroll faster, Back leaves; read by the screen itself | feature/B-470-credits-screen |
| 2026-11-14 | B-456c: the vendor screen's Item slots tab uses Confirm (A asks, the second A buys) and Back; no new key | feature/B-456c-item-slots |
| 2026-11-14 | B-456b: the vendor screen's Buy tab: A / Right adds the focused line to the cart, Left takes one back, X buys the cart, R / Y (or LT / RT) changes who it is bought for; read by the screen itself | feature/B-456b-vendor-buy |
| 2026-11-14 | B-463: the journal screen: LT / RT (Z / C) for its sub-tabs, X shows or hides the focused secret quest, Y (R) hides them all, A plays a ready conversation, the right stick or Page Up / Down scrolls the reader; read by the screen itself | feature/B-463-journal-screen |
