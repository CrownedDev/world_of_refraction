# Placeholder assets

**Living list** (Crown, 2026-11-15): every stand-in asset in the game, where it's plugged in, and how to swap in a v2. "These are just placeholders for now."

**Every job that adds a placeholder adds its row here, in the same change.** When a v2 lands, flip its status to **Final** (or delete the row if the placeholder is gone).

**How to add a row:** pick the area's table (or start a new one). Fill:
- **Asset**: name and Content path (or engine path).
- **What it is**: plain words.
- **Assigned at**: the thing that points at it. A data-asset field (`DA_X.FieldName`), a config line (`Config/File.ini:line`) or a code constant (`File.cpp:line`). `.uasset` files are LFS binaries: find this through the code, config, `docs/PIE/` and `docs/Architecture/`, never by reading the asset.
- **Made by**: script, console command, editor worker (MCP), copied from the deckbuilder, or engine content.
- **Status**: Placeholder / Final.
- **Swap in a v2**: "overwrite at the same path" (nothing else changes) or "repoint <field>".

Line numbers drift as files change; the field or constant name is the stable part.

---

## 1. Battle transition (B-500, B-500b, B-503d)

| Asset | What it is | Assigned at | Made by | Status | Swap in a v2 |
|---|---|---|---|---|---|
| `M_BattleTransition_Shard` (`/Game/VFX/Transition/`) | The material on the crystal shards the screen breaks into: shows the captured frame, the prism light and coloured edges | `DA_BattleTransition_Default.ShardMaterial` (`BattleTransitionData.h:109`) | Editor worker, to `docs/PIE/B-500.md` step 1 and `B-500b.md` | Placeholder | Overwrite at the same path, keeping the parameter names the code sets (`Frame`, `FrameBrightness` and B-500b's prism scalars), or repoint `ShardMaterial` |
| `DA_BattleTransition_Default` (`/Game/Data/Combat/`) | The transition's timings and look values (crack, break, fly-off, prism) | Loaded by path: `BattleTransitionData.h:21` (`PATH`) | Editor worker (`docs/PIE/B-500.md` step 2); Blow Away Ease 0.3 set by the editor worker (B-503d) | Placeholder (values still being tuned) | Edit the values in place. A new asset must sit at the same path, or change `PATH` |
| `/Engine/BasicShapes/BasicShapeMaterial` | Plain violet shards when `ShardMaterial` is empty | `BattleTransitionShards.cpp:30` (`FALLBACK_MATERIAL_PATH`) | Engine content | Placeholder (fallback only) | Nothing to swap: it stops showing once `ShardMaterial` is set |

## 2. Field attack (B-424, B-503b, B-503c, B-503d)

| Asset | What it is | Assigned at | Made by | Status | Swap in a v2 |
|---|---|---|---|---|---|
| `AM_SAS_LightHit` (`/Game/Data/Skills/Weapons/SwordAndShield/LightHit/`) | The battle's light hit: a dash in (a stretched hop), then the `ComboAttack02_01` strike | `DA_SAS_Attack_LightHit.SkillMontage` (`Scripts/editor/build_light_hit.py:52`) | Console `wor.Editor.BuildLightHitMontage` + `build_light_hit.py` (`LightHitMontageBuilder.cpp:30`), from `AS_SAS_LightHit_Dash` and `Anim_SAS_V2_ComboAttack02_01_IP` | Placeholder (prototype, B-424) | Re-run the builder after changing its clip constants (overwrites the same path), or repoint `SkillMontage` |
| `AM_SAS_LightHit_Return` (same folder) | The hop back after the light hit | `DA_SAS_Attack_LightHit.ReturnMontage` (`build_light_hit.py:53`) | Same builder (`LightHitMontageBuilder.cpp:31`), from `AS_SAS_LightHit_HopBack` | Placeholder | As above, or repoint `ReturnMontage` |
| `AM_SAS_LightHit_Field` (same folder) | The map's sword swing: the strike alone, no dash (VFX0 0.30 s, SFX0 0.325 s, Impact0 0.40 s) | `DA_SAS_Attack_LightHit.FieldMontage` (`build_light_hit.py:67`); read as `FFieldAttackEntry::SwingMontage` | Console `wor.Editor.BuildLightHitFieldMontage` via `build_light_hit.py` (`LightHitMontageBuilder.cpp:32`) | Placeholder | As above, or repoint `FieldMontage` |
| `DA_SAS_Attack_LightHit` (same folder) | The starting sword and shield's attack skill (Light Hit) | `DA_Equipment_Generic_SAS.WeaponAttack` (`build_light_hit.py:58`) | `build_light_hit.py` (from a template skill) | Placeholder (prototype) | Edit in place, or repoint `WeaponAttack` on the weapon |
| `AM_Field_Pulse` (`/Game/Data/Skills/Field/Pulse/`) | The field pulse's cast: the right hand's throw, 4.0–5.2 s of `AS_Cast_FireBall`, Impact0 at 0.5 s | `Config/DefaultGame.ini:64` (`FieldPulseMontage`, `FieldMovementComponent.h:195`) | Console `wor.Editor.BuildFieldPulseMontage` (`LightHitMontageBuilder.cpp:36`), saved by the editor worker | Placeholder | Overwrite at the same path, keeping an Impact notify (index 0) at the release, or change the ini line |
| `FXS_Field_Pulse` (`/Game/VFX/Field/`) | The pulse's burst: a ball travelling from the hand to where it stops, in the element's colour | `Config/DefaultGame.ini:65` (`FieldPulseVFX`, `FieldMovementComponent.h:200`) | Editor worker, to `docs/PIE/B-503c.md` step 2 | Placeholder | Overwrite at the same path, keeping user parameters `PulseColor` (Linear Color) and `PulseEnd` (Vector), or change the ini line |
| The two `FieldPulse*` lines, `Config/DefaultGame.ini:64-65` | Turn the montage and burst on | `[/Script/world_of_refraction.FieldMovementComponent]` | Uncommented by the editor worker (B-503d) | — | Comment them out to fall back to the plain line and ball |
| Debug line and sphere | The pulse's look when `FieldPulseVFX` is empty | `FieldMovementAttack.cpp:314-316` (`FieldPulse::PLACEHOLDER_*`) | Code | Placeholder (fallback only) | Nothing to swap: it stops once `FieldPulseVFX` is set |

## 3. UI icons (B-485, B-486)

All `T_Icon_*` textures here live in `/Game/UI/Icons/`. They were copied from the deckbuilder's PONETI art (originals and credits in `SourceArt/UI/Deckbuilder/`, `CREDITS.md`) and imported by the editor worker. The table is loaded by path.

| Asset | What it is | Assigned at | Made by | Status | Swap in a v2 |
|---|---|---|---|---|---|
| `DA_UIIcon_Table` (`/Game/UI/Icons/`) | The icon table every screen asks for icons | Loaded by path: `WoRIconTable.cpp:14` (`ICON_TABLE_ASSET_PATH`) | Editor worker (B-486) | Placeholder (its contents) | Repoint entries in place; keep the asset at this path |
| `T_Icon_Element_<Fire, Water, Earth, Wind, Light, Darkness, Lightning, Void>` (8) | Diamond element badges. **Also the crystal placeholders** (Crown, 2026-11-14): a crystal shows its element's diamond; Ice uses Water's | `DA_UIIcon_Table.ElementBadges` (`WoRIconTable.h:49`) | Copied from the deckbuilder (`library/badges/element_*.png`) | **Placeholder (crystals especially)** | Overwrite at the same path, or repoint the `ElementBadges` entry. Separate crystal art would need its own map: a code change |
| `T_Icon_Element_Reality` | Iolite's badge (B-584) | `DA_UIIcon_Table.ElementBadges` (`WoRIconTable.h:49`) | The Void badge's gem tinted #5A2ED4 | Placeholder | Overwrite, or repoint the entry |
| `T_Icon_Gem_<Fire, Water, Earth, Wind, Light, Darkness, Lightning, Void, Reality>` (9) | A crystal or evolution in no socket: the gem, no frame (D-256) | `DA_UIIcon_Table.LooseGems` (`WoRIconTable.h:53`) | Cut from the element badges (`gem_loose_*.png`) | Placeholder | Overwrite, or repoint the entry. Check a blue one isn't imported as a normal map |
| `T_Icon_Socket_Empty` | An empty gear socket | `DA_UIIcon_Table.EmptySocket` (`WoRIconTable.h:56`) | Copied from the deckbuilder (`element_socket.png`) | Placeholder | Overwrite, or repoint `EmptySocket` |
| — (`StoneSocket`) | The stone socket icon | `DA_UIIcon_Table.StoneSocket` (`WoRIconTable.h:60`) | Empty until Crown picks one | Missing | Set `StoneSocket` |
| `T_Icon_Tier_<F, E, D, C, B, A, S>` (7) | Tier badges | `DA_UIIcon_Table.TierBadges` (`WoRIconTable.h:63`) | Copied from the deckbuilder (`tier_*.png`) | Placeholder | Overwrite, or repoint the entry |
| `T_Icon_Weapon_<Sword, Spear, Axe, Bow, Dagger, Gauntlets, Hammer, Staff>` (8) | Weapon-type icons | `DA_UIIcon_Table.WeaponTypes` (`WoRIconTable.h:66`) | Copied from PONETI `ClassicFantasyRPG_UI/ARTWORKS/UIicons/Colored/` | Placeholder | Overwrite, or repoint the entry |
| `T_Icon_Effect_*` (36): Amp ×9, Dot ×10 (incl. Ice), Resistance ×9, Barrier, Damage, Defense, Energy, ExtraTurn, Heal, Revive, Shield | Effect-kind icons on gear and statuses | `DA_UIIcon_Table.EffectKinds` (`WoRIconTable.h:70`) | Copied from the deckbuilder (`library/icons/effect_*.png`) | Placeholder | Overwrite, or repoint the entry |

Not ours: `T_Icon_Particle_*`, `T_Icon_Scalability_*` and `T_Icon_Solver_*` in `/Game/VFX/Icons/Modules/` belong to the Niagara library.

## 4. Test map boxes (`TestLevel_Trial_01`, B-447, B-449, B-502)

| Asset | What it is | Assigned at | Made by | Status | Swap in a v2 |
|---|---|---|---|---|---|
| `TestLevel_Trial_01` (`/Game/Level/`) | The grey-box trial map used for every trial test | `Config/DefaultGame.ini:99` (`MapsToCook`) | Crown in the editor (commit `362dd80ed` added the boxes) | Placeholder (grey box) | A real trial map: add it to `MapsToCook`; keep this one for tests |
| Three rest spots (`ATrialRestSpot`, placed) | Purple boxes where the party rests and uses a service | Placed actors in `TestLevel_Trial_01`; the service is set per actor (`Rest Spot > Service`, `docs/PIE/B-502.md`) | Crown in the editor | Placeholder (boxes until the anvil, tree, pedestal, shrine and V art) | Per actor: swap its mesh, or use a Blueprint subclass of `ATrialRestSpot` with the art |
| `MI_Test_RestSpot_Purple` (`/Game/Testing/`) | The purple material on those boxes | The rest-spot actors' mesh material in `TestLevel_Trial_01` | Crown in the editor (`362dd80ed`) | Placeholder | Goes with the boxes; nothing points at it from code |
| Trial NPC boxes (`ATrialNPC`, beside each rest spot) | Shop-coloured boxes for V and the rotating seller; **obsolete** since B-502 | Placed actors in `TestLevel_Trial_01` | Crown in the editor | Placeholder, to remove | Replace with a plain coloured static mesh (`docs/PIE/B-502.md` step 3), later the real service art |
| `/Engine/BasicShapes/Cube` | The box mesh every trial map feature (rest spot, NPC, chest, lore object, boss trigger) gets by default | `TrialMapFeature.cpp:56` (`FeatureMesh`) | Engine content | Placeholder | Set the mesh per placed actor or per Blueprint subclass; change the default here only for all features at once |

## 5. Other stand-ins found in a sweep (2026-11-15)

From the code's hard-coded paths, the placeholder UI headers, and `AssetAudit.md`. Not checked in the editor.

| Asset | What it is | Assigned at | Made by | Status | Swap in a v2 |
|---|---|---|---|---|---|
| `SKM_Manny_Base`, `SKM_Quinn` (`/Game/Data/Characters/Models/CharacterBase/`) | UE5 mannequins: every player and enemy body | `WoRPlayerState.cpp:98-99` (`PlayerFormLook::MANNY_MESH` / `QUINN_MESH`); the `BP_CombatCharacter_*` Blueprints | Engine starter content | Placeholder | Per character: its mesh in the character data or Blueprint; the form defaults in `PlayerFormLook` |
| `/Niagara/DefaultAssets/Templates/Systems/RadialBurst` | The Perfect Guard flash | `CosmeticsData.h:27` (`PERFECT_GUARD_FLASH_VFX`) | Engine content (editor builds only) | Placeholder | Repoint the constant, or set the cosmetic's own VFX |
| `/Engine/EditorSounds/Notifications/CompileSuccess_Cue` | The Perfect Guard sound (an editor beep) | `CosmeticsData.h:28` (`PERFECT_GUARD_SOUND`) | Engine content (editor builds only) | Placeholder | Repoint the constant, or set the cosmetic's own sound |
| `/Engine/BasicShapes/Cylinder` | The field enemy team's marker on the map | `FieldEnemyTeam.cpp:32` | Engine content | Placeholder | Set the mesh on the placed team or a Blueprint subclass |
| `/Engine/BasicShapes/Sphere` | The arena sphere drawn round a fight | `EncounterComponent.cpp:269`; its material is `EncounterComponent.ArenaSphereMaterial` | Engine content | Placeholder | Repoint the mesh in code; the material per encounter |
| `/Engine/BasicShapes/Cube` | Hub merchant and trial door boxes | `MerchantInteractable.cpp:32`, `TrialDoor.cpp:34` | Engine content | Placeholder | Mesh per placed actor or Blueprint subclass |
| `DA_Test_Ability` (`/Game/Testing/Weapons/Abilities/`) | A test ability for debug commands | `CombatOrchestratorDebug.cpp:495`, `:1036` | Older test data | Placeholder (debug only) | Not shipped; leave |
| `BP_Test_TurnManager`, `BP_Test_StatusEffectManager` (`/Game/Testing/`) | Legacy test actors, retired after a spec and Crown's PIE (D-067) | `docs/PIE/B-350.md` | Older test data | Placeholder (to retire) | Delete under B-350, not swap |
| **Placeholder C++ UI** (Backlog-mode rule 4, D-024): `BattleHUDWidget`, `BattleCommandMenuWidget`, `DamageNumberView`, `CharacterPanelWidget`; the hub screens (`RestSpotWidget`, `InventoryWidget`, `StorageWidget`, `DismantleWidget`, `LevelDropWidget` …); the menus (`PauseMenuWidget`, `LoadoutV2ScreenWidget`, `VendorScreenWidget` …); party and co-op screens; `GateScreenWidget`, `TrialNPCWidget`, `FieldSkillPickerWidget`, `WoRTestScreen` | Plain buttons and text built in C++ (no `.uasset`) | `Source/world_of_refraction/Public/UI/**` (each header says "placeholder"); opened by its console command or the HUD | Code (writer sessions) | Placeholder | Replaced by CommonUI screens (D-024), screen by screen per `UI_ScreenInventory.md`; not an overwrite |
| Vendor packs (Big Pack Magic VFX, Combat Master anims, Mage pack, StylizedWeapons …) | Bought or free art used as-is | Per skill and weapon data asset | Vendor packs (git-ignored, on Crown's PC) | Placeholder until the painterly pass (`VisualDirection.md`) | Per asset; see `AssetAudit.md` |

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-15 | Created: battle transition, field attack, UI icons, test map boxes, and a sweep of other stand-ins | docs/placeholder-assets |
