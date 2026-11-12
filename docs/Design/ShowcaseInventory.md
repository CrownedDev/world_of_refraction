# Showcase asset inventory

**2026-11-12.** What the project actually has for the showcase battle, read from the live editor's asset registry (unreal-mcp, this worktree's editor). Scope and look references: `ShowcaseReferences.md`. Earlier audit by file names: `AssetAudit.md`. **Names only:** nothing here has been judged by eye yet. Crown reviews each piece in the editor before it is used, one at a time.

Key: ✅ have a good candidate · 🔧 have something, needs work or an eye check · ❌ missing.

## Across the board

- **Sound is the big gap.** No combat sounds. All we have are 17 PONETI Freesound stand-ins (licence check per file: `hit-impact-sword-2`, `hit-metallic-01`, `melee-attack-with-voice-2`, `attack-punch`, `magic-missiles`, `magic-gargle-spell`, `energy-weapon-001`, `wpn-2-generic`, `monster-attack` …) and SoStylized lightning (`S_Lightning_Charge`, `SC_LightningStrike`). Every row below is ❌ or 🔧 stand-in for sound. Real fix: one combat SFX pack.
- **No weapon trails** (no trail Niagara). Needs one ribbon trail made.
- **Spell VFX come in 10 sets** (Big Pack Magic: Fire, Ice, Water, Earth, Air, Lightning, Light, Dark, Blood, Poison). Void and Reality have none (recolour). Heal exists in Light only; debuff exists in Blood, Dark and Poison only; beams exist in Light, Lightning, Blood and Poison only.
- **Built from scratch (Crown, 2026-11-12):** the existing weapon, ring, evolution and Generic spell data assets are ignored; new data assets are made, named per `Conventions/ContentNaming.md`. Only the raw art below counts (meshes, anims, VFX, sounds).
- **Bodies:** Manny and Quinn only.

## Picks (Crown, in the editor)

Crown goes through it in sets: a weapon, then that weapon's skills, then the next set. Set 1 = sword and shield; Set 2 = axe; then spells and projectiles.

| Set | Item | Picked | Path |
|---|---|---|---|
| 1 | Sword mesh | **SK_Sword_1H_Generic_01** (2026-11-12) | `/Game/StylizedWeapons/WeaponPack2/Meshes/Item/Weapons/Sword/SK_Sword_1H_Generic_01` |
| 1 | Shield mesh | **SK_Shield_Fantasy_06** (2026-11-12) | `/Game/StylizedWeapons/WeaponPack1/Meshes/Item/Weapons/Shield/SK_Shield_Fantasy_06` |
| 1 | Light, heavy, finisher, crippling, taunt, second wind, battle cry | — | |

## Weapon families: meshes shared across styles (Crown, 2026-11-12)

**One mesh family per weapon kind; the animation set decides the style, not the mesh.** The picked sword mesh serves sword and shield, single sword, dual swords, katana and dual katana; the one-handed axe family serves a single axe, twin axes and axe and shield. (A real katana or a greatsword can get its own mesh later; the showcase uses the shared one.)

| Style | Animation sets (Combat Master unless named; RM = root motion, IP = in place) | Guard |
|---|---|---|
| Sword + shield | **DSAS V1 / V2 / V3** (IP + RM), HandsomeSwordAndShield (RM; buff, evade, heavy slash / stab) | ✅ DSAS V2 `Block`, `BlockHit`, block moves |
| Single sword | HandsomeSword V1 / V2 (RM; V2 heavy + execution), PowerfulSwordAnims (RM); Paragon Greystone, Kwang, Feng Mao | ❌ |
| Dual swords | HandsomeTwinSword (RM, 25) | 🔧 `Anim_Tb_Block` (one) |
| Katana | DynamicKatana V1 (`DK`, IP + RM, 53), V2 (`DK2`, IP + RM, 63), V3 (`KT3`, RM, 36), HandsomeKatana (30) | 🔧 `Katana_Block` (HandsomeKatana) |
| Dual katana | **No pack found by name.** Stand-in: HandsomeTwinSword | 🔧 `Anim_Tb_Block` |
| Single axe (1H) | DynamicAxe (RM; combos, common attacks, buff) | ❌ |
| Twin axes | DynamicTwinAxe (RM) | ✅ `Anim_DTA_Block_Start/Loop/Hit/End` |
| Axe + shield | Paragon Terra (action timing) | ✅ Terra `RMB_*` shield guard |

## Gear

| Item | Mesh | Anims | VFX | Sound | Notes |
|---|---|---|---|---|---|
| **Weapon 1: Sword + Shield** | ✅ `StylizedWeapons/WeaponPack1/.../SK_Sword_1H_Fantasy_01–03` + `SK_Shield_Fantasy_01–09`; `CombatMasterAnimBundle/Weapon/Meshes/SM_Sword_DSAS` / `SM_Shield_DSAS` | ✅ DSAS V1–V3 (in-place and root motion, full guard in V2), HandsomeSword V2 (heavy, execution), Paragon Greystone / Kwang (sword heroes) | 🔧 element slash sets; ❌ trail | ❌ (stand-ins: sword hit, metal hit) | Best-covered weapon. |
| **Weapon 2: Axe** (Crown) | See **Axe** below | See **Axe** below | 🔧 element slash / shockwave; ❌ trail | ❌ (stand-ins: sword hit, metal hit) | Most complete set: the **Dynamic Twin Axe** (attacks + full guard). |
| **2 adapters** (rings) | ❌ **no ring meshes at all** | — | 🔧 `NS_Magic_Circle1_*` (15) for a cast ring | ❌ | A ring mesh is needed, or the adapter shows as VFX only. |
| **2 evolutions** | — (passive items) | — | 🔧 auras (`NS_Aura_Pack_Aura_1–25`, element `Aura`) if they should glow | ❌ | No visual of their own. |

## Axe (second weapon)

**Meshes** (all skeletal unless marked SM):

| Kind | Assets | Notes |
|---|---|---|
| One-handed | `StylizedWeapons/WeaponPack1/Meshes/Item/Weapons/Axe/SK_Axe_1H_Fantasy_01–06`; `StylizedWeapons/WeaponPack2/Meshes/Item/Weapons/Axe/SK_Axe_1H_Generic_01–09` (+ `SK_Axe_1H_Generic_03Off`, an off-hand twin) | Textured, same style as the sword and shield in use |
| Two-handed | `.../WeaponPack1/.../Axe/SK_Axe_2HL_Fantasy_01–06`; `.../WeaponPack2/.../Axe/SK_Axe_2HL_Generic_01–06`, `SK_Axe_2HL_Ud_01–02` | Textured |
| Combat Master | `CombatMasterAnimBundle/Weapon/Meshes/SM_Axe` (SM, the one-handed axe pack's axe) and `SM_DTA` (SM, the twin-axe pack's axe) | Match the Combat Master axe anims' grip |
| Basemesh (untextured) | `Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_/Axe`, `Knight_Axe`, `Rough_Axe`; halberds `Adventurer_s_Halberd`, `Demonic_Halberd`, `Runic_Halberd`; `Pick_Ax` | Flat materials; shape reference only |

**Animation sets** (all on Manny; Combat Master ones are root motion only, so opt in per montage):

| Set | Path | What's in it | Missing |
|---|---|---|---|
| **Dynamic Twin Axe** (dual) | `CombatMasterAnimBundle/Animations/DynamicTwinAxeCombatAnims/Manny_UE5/RootMotion/` | Combos A1–A4, B1–B4, C1–C3, D (+ full-chain `ComboA/B/C`); **guard** `Block_Start/Loop/Hit/End`; hits 1–4; idle 1–2; equip / unequip; jump | No buff, no death, no named heavy or finisher (pick from the combo ends by eye) |
| **Dynamic Axe** (one-handed) | `CombatMasterAnimBundle/Animations/DynamicAxeAnims/Manny_UE5/RootMotion/` | Combos A1–A4, B1–B5, C1–C4 (+ full chains); `CommonAttack_A/B/C`; **`Buff`**; hits A/B; `Die`; idle A/B | **No guard** |
| **Paragon Terra** (axe + shield) | `Imports/Paragon/ParagonAnimations/terramanny/` | `Primary_Melee_A–D_slow` (+ recovery), `Primary_Melee_Air`, `Q_swing_B`, `E_ability`, ultimate `R_Intro` / `R_ending`, shield guard `RMB_intro` / `RMB_Shield_pose` / `RMB_HitReact` / `RMB_melee` / `RMB_end`, `Emote_HooRah` (battle cry), `Emote_SharpenAxe`, stun, hit reacts | Action-game timing (long recoveries); Terra's own axe and shield meshes are not imported |
| Two-handed axe | — | **No dedicated 2H axe anims.** Stand-in: Dynamic Great Sword (`DynamicGreatSwordAnims`, root motion, full block set) | 2H axe moves |
| Project anims | `Data/Skills/Weapons/Axe/DualAxe/Anims/Anim_DualBlade_Attack_3–6`, `Anim_Dual_Whirlwind1–2`; `Data/Skills/Weapons/DualAxe/Abilities/Anims/Anim_DualBlade_Attack_1–2`, `Anim_Dual_Whirlwind` | Older project copies (dual-blade attacks, a whirlwind) | — |
| Duplicate | `Imports/CombatMasterAnimBundle/...` | The same Combat Master pack a second time (B-122 cleanup) | — |

**Most complete:** the **Dynamic Twin Axe**. It has the most attacks and the only full axe guard set, and it differs most from the sword and shield. Its gaps (buff, finisher) are covered by the one-handed Dynamic Axe `Buff`, Paragon Terra `Emote_HooRah` and the battle-cry picks below. Alternatives: a one-handed **Dynamic Axe** (good attacks plus a buff, but no guard), or **Terra** (an axe and shield with a guard, an ultimate and a battle cry, but long action-game timing and close to weapon 1's shield).

## Spells (Generic, take the element)

| Spell | Anim | VFX | Sound | Missing |
|---|---|---|---|---|
| **Elemental Bolt** (Destruction) | ✅ `Mage_Fireball_R_IP`, `AS_Cast_FireBall`, `AS_Cast_PrepareAndFire` | ✅ per element `Muzzle` → `Projectile` → `Hit` (Water: `Projectile1–4`; Muzzle missing for Dark/Light/Lightning/Poison) | 🔧 `magic-missiles` | Scorch decal ❌; hit-stop / shake are code |
| **Elemental Lance** (Conjuration) | ✅ hold-then-throw: `Mage_Telekinesis_Start/Loop/End`, `AS_Cast_PrepareAndFire`; Paragon Kwang `Ability_Q_Throw` (a sword throw) | 🔧 `NS_Ice_Magic_Spear` + `SpearSplash`, Fire `SpearSplash`, `NS_Light_Magic_Sword_Line*`; motes forming: element `Aura` / `Muzzle` | ❌ | **A lance projectile per element** (Ice only today; others recolour the Ice spear) |
| Buff (Enhancement) | ✅ `AS_Cast_Enchants`, `AS_Cast_CollectMana` | ✅ element `Buff` (all 10) + `Aura` | ❌ | Icon pop is UI |
| Heal (Restoration) | 🔧 `AS_Cast_Worshiped`, `AS_Cast_ReadsTheSpell` | 🔧 `NS_Light_Magic_Heal` / `Heal_Hit` only | 🔧 none fitting | Rising motes in other colours: recolour |
| Manipulation (blink) | ✅ Paragon Muriel `Ultimate_Teleport_Start` / `_Land`; `AS_Cast_Hypnotizing` | ✅ element `Dash` (all 10) at both ends | ❌ | — |
| Debuff (Impairment) | 🔧 `AS_Cast_Bewitchment` | 🔧 `Blood/Dark/Poison_Magic_Debuff` only | ❌ | Other elements: recolour; down-arrow icon is UI |

## Projectiles to tune

| Kind | VFX we have | Anim | Missing |
|---|---|---|---|
| Fast bolt | ✅ element `Projectile` (several styles each: Fire 1–5, Light 1–4, Water 1–4, Lightning 1–3, Poison 1–6; which read thin is an eye check) | `Mage_Fireball_R` | ❌ a thin trail ribbon |
| Heavy orb | ✅ element `Orb` (Fire, Ice, Water, Earth, Air, Dark, Light, Lightning, Blood, Poison) | `Mage_Telekinesis_*`, Paragon TwinBlast `Ability_ChargeBlast_*` | Camera push-in is code |
| Volley | ✅ `NS_Air_Magic_Arrow1–4`, any projectile ×N | `Mage_Fireball_L` / `_R` alternating; `AS_Cast_MagicAttack` | Spacing is code |
| Arcing lob | 🔧 `NS_Earth_Magic_Meteor`, element `Projectile`; landing circle ✅ element `Target` | `AS_Cast_MagicBall` | Arc path is code |
| Homing | 🔧 any projectile | any cast | Curved path is code |
| Beam / line | 🔧 `Light_Magic_Beam`, `Lightning_Magic_Beam`, `Blood_Magic_Beam1–2`, `Posion_Magic_Beam` | ✅ `Mage_Beam_2h_Start/Loop/End` | Beams for Fire, Water, Earth, Air, Dark: recolour |

## Weapon abilities (per weapon)

| Ability | Sword + Shield anim | Axe anim (Twin Axe unless named) | VFX | Sound | Missing |
|---|---|---|---|---|---|
| Light hit | ✅ DSAS V1 `Combo1_IP`; Greystone `Attack_A_Fast` | ✅ `Anim_DTA_ComboA1_RM` / `B1` (first hits) | 🔧 element `Slash` | 🔧 sword hit | Trail ❌ |
| Heavy hit | ✅ `Anim_SwordV2_HeavyAttack`, `Anim_HSAS_Slash_Down_Heavy`; Greystone `Attack_A_Slow` | 🔧 a combo end (`ComboA4` / `C3` / `D`) by eye; Terra `Primary_Melee_A_slow` | 🔧 element `Shockwave` / `Hit` | 🔧 metal hit | Impact pause is code |
| Finisher | ✅ `Anim_SwordV2_Attack_Execution` (+ the victim's `_Executed`), Greystone `Ultimate_Entry/Part1/Part2` | 🔧 full chain `Anim_DTA_ComboD_RM` / `ComboC_RM`; Terra `R_Intro` / `R_ending` | 🔧 element `AOE` / `Explosion` | ❌ | No named axe finisher; slow-mo / close cut is camera code |
| Crippling strike | 🔧 no clearly named low cut; check DSAS V2 and Greystone attacks by eye | 🔧 by eye in the DTA / DA combos; Terra `Q_swing_B` | 🔧 element `Debuff` (Blood/Dark/Poison) | ❌ | A low-slash anim may be missing |
| Taunt | ✅ Greystone `Emote_Taunt_SwordPoint_T1` / `_BringItOn_T1`; `Mage_Shout` | ✅ Grux `Emote_Taunt_FootSlide_T1`, Terra `Emote_SharpenAxe` | 🔧 red ring: `NS_Magic_Circle1_*` tinted | ❌ shout | Enemy turning to face is code |
| Second wind | ✅ Paragon Kwang `Emote_DeepBreath_Intro/Loop/Outro` | (same, any body) | 🔧 `NS_Light_Magic_Heal` (green recolour) | 🔧 none fitting | — |
| Battle cry | ✅ Kwang `Emote_RallyUp_*`, `Anim_DSS3_Buff_RM`, `Mage_Shout` | ✅ Terra `Emote_HooRah`, Grux `Ultimate_Roar` / `Emote_Master_BattleCry_T3`, `Anim_DA_Buff_RM` | ✅ element `Shockwave` (Earth, Fire, Ice, Lightning, Water) + `Buff` on each ally | ❌ shout | Buff icon is UI |
| Guard | ✅ DSAS V2 `Block` / `BlockHit` | ✅ `Anim_DTA_Block_Start/Loop/Hit/End_RM` | — | 🔧 metal hit | — |

## Plain summary for Crown

- **Good enough to start:** Sword + Shield; the axe (meshes plus the Twin Axe set with a guard); the Bolt, buff and blink spells; the projectile shapes; taunt, second wind and battle cry; the sword's light, heavy and finisher animations.
- **Needs work:**
  - The Lance: only an Ice spear exists.
  - Heal and debuff in most elements, and the beam in five elements: recolours.
  - A weapon trail.
  - The axe's heavy, finisher and crippling moves: there are no named ones, so they're picked from the combos by eye.
  - The crippling low cut for the sword.
- **Missing:** every combat sound (buy a pack), ring meshes for the adapters, and two-handed axe animations (a one-handed or twin axe avoids that).
- **Combat Master axe anims are root motion only, and Paragon moves are action-game timing.** Each needs an eye check for a turn-based, fixed-position fight.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-12 | Created from the live editor's asset registry (unreal-mcp) | docs/showcase-inventory |
| 2026-11-12 | Picks table (Set 1 sword: SK_Sword_1H_Generic_01); weapon families share meshes across styles (Crown), animation sets per style | docs/showcase-inventory |
| 2026-11-12 | Crown: built from scratch (existing data assets ignored); weapon 2 is an axe (bow / staff dropped): axe meshes and anim sets listed | docs/showcase-inventory |
