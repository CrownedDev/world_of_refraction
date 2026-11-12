# Showcase asset inventory

**2026-11-12.** What the project actually has for the showcase battle, read from the live editor's asset registry (unreal-mcp, this worktree's editor). Scope and look references: `ShowcaseReferences.md`. Earlier audit by file names: `AssetAudit.md`. **Names only:** nothing here has been judged by eye yet. Crown reviews each piece in the editor before it is used, one at a time.

Key: ✅ have a good candidate · 🔧 have something, needs work or an eye check · ❌ missing.

## Across the board

- **Sound is the big gap.** No combat sounds. All we have are 17 PONETI Freesound stand-ins (licence check per file: `hit-impact-sword-2`, `hit-metallic-01`, `melee-attack-with-voice-2`, `attack-punch`, `magic-missiles`, `magic-gargle-spell`, `energy-weapon-001`, `wpn-2-generic`, `monster-attack` …) and SoStylized lightning (`S_Lightning_Charge`, `SC_LightningStrike`). Every row below is ❌ or 🔧 stand-in for sound. Real fix: one combat SFX pack.
- **No weapon trails** (no trail Niagara). Needs one ribbon trail made.
- **Spell VFX come in 10 sets** (Big Pack Magic: Fire, Ice, Water, Earth, Air, Lightning, Light, Dark, Blood, Poison). Void and Reality have none (recolour). Heal exists in Light only; debuff exists in Blood, Dark and Poison only; beams exist in Light, Lightning, Blood and Poison only.
- **The Generic spell data assets exist but are empty shells:** `DA_Spell_Bolt`, `_Lance`, `_Missile`, `_Storm`, `_Wave`, `_Aura`, `_EnhancementAura`, `_Grace`, `_Might`, `_Warding`, `_Cleanse`, `_Mend`, `_Rally`, `_Restore`, `_Renewal`. None references an animation or a VFX yet. **Bolt and Lance sit in the Conjuration folder;** Crown now wants Bolt to be Destruction. There is no Generic Manipulation or Impairment spell.
- **Bodies:** Manny and Quinn only.

## Gear

| Item | Mesh | Anims | VFX | Sound | Notes |
|---|---|---|---|---|---|
| **Melee: Sword + Shield** | ✅ `SK_Sword_1H_Fantasy_01` + `SK_Shield_Fantasy_01` (on `DA_Equipment_Generic_SAS`); also `SM_Sword_DSAS` / `SM_Shield_DSAS` | ✅ DSAS V1–V3 (in-place and root motion, full guard in V2), HandsomeSword V2 (heavy, execution), Paragon Greystone / Kwang (sword heroes) | 🔧 element slash sets; ❌ trail | ❌ (stand-ins: sword hit, metal hit) | Best-covered weapon. Data exists (`DA_Weapon_SAS`, `DA_Equipment_Generic_SAS`). |
| **Ranged/caster: Bow** (recommended) | ✅ 22 bows (`SK_Bow_Generic_01–14`, `Great_Crystal_Bow`, `Frost_Bow_Ignavus` …), arrows and quivers | ✅ Paragon **Sparrow** full bow kit: `Primary_Fire_Fast/Med/Slow`, `RMB_Drawback/Loop/Fire` (charged shot), `R_Ability_*_Fire` (volley), `Q_Ability`, taunt, stun; StylizedWeapons `A_Bow_Pull_Start/Loop/End`, `A_Bow_Attack` | 🔧 `NS_Air_Magic_Arrow1–4`, element projectiles | ❌ | **No bow weapon data and no arrow-shooting weapon path checked yet:** a code question before building. |
| Ranged/caster: Staff (alternative) | 🔧 `SK_Staff_Fantasy_01–04`, one `Wand` | ❌ no staff-holding anims (Mage set and MagicAnimations cast empty-handed) | ✅ the spell sets | ❌ | Weaker than the bow: casts would look empty-handed. |
| **2 adapters** (rings) | ❌ **no ring meshes at all** | — | 🔧 `NS_Magic_Circle1_*` (15) for a cast ring | ❌ | Data exists (`DA_Ring_Generic` + 48 element rings), but nothing to show on the hand. A ring mesh is needed, or the adapter shows as VFX only. |
| **2 evolutions** | — (passive items) | — | 🔧 auras (`NS_Aura_Pack_Aura_1–25`, element `Aura`) if they should glow | ❌ | Data exists (27 element evolutions, e.g. `DA_Evo_Earth_SecondWind`, `DA_Evo_Fire_EmberVow`); no visual of their own. |

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
| Volley | ✅ `NS_Air_Magic_Arrow1–4`, any projectile ×N | Sparrow `R_Ability_*_Fire` | Spacing is code |
| Arcing lob | 🔧 `NS_Earth_Magic_Meteor`, element `Projectile`; landing circle ✅ element `Target` | `AS_Cast_MagicBall` | Arc path is code |
| Homing | 🔧 any projectile | any cast | Curved path is code |
| Beam / line | 🔧 `Light_Magic_Beam`, `Lightning_Magic_Beam`, `Blood_Magic_Beam1–2`, `Posion_Magic_Beam` | ✅ `Mage_Beam_2h_Start/Loop/End` | Beams for Fire, Water, Earth, Air, Dark: recolour |

## Weapon abilities (per weapon)

| Ability | Sword + Shield anim | Bow anim | VFX | Sound | Missing |
|---|---|---|---|---|---|
| Light hit | ✅ DSAS V1 `Combo1_IP`; Greystone `Attack_A_Fast` | ✅ Sparrow `Primary_Fire_Fast` | 🔧 element `Slash`; arrow / `Air_Magic_Arrow` | 🔧 sword hit | Trail ❌ |
| Heavy hit | ✅ `Anim_SwordV2_HeavyAttack`, `Anim_HSAS_Slash_Down_Heavy`; Greystone `Attack_A_Slow` | ✅ Sparrow `RMB_Drawback` → `RMB_Fire` (charged) | 🔧 element `Shockwave` / `Hit` | 🔧 metal hit | Impact pause is code |
| Finisher | ✅ `Anim_SwordV2_Attack_Execution` (+ the victim's `_Executed`), Greystone `Ultimate_Entry/Part1/Part2` | 🔧 Sparrow `R_Ability_Slow_Fire` | 🔧 element `AOE` / `Explosion` | ❌ | Slow-mo / close cut is camera code |
| Crippling strike | 🔧 no clearly named low cut; check DSAS V2 and Greystone attacks by eye | 🔧 Sparrow `Q_Ability` | 🔧 element `Debuff` (Blood/Dark/Poison) | ❌ | A low-slash anim may be missing |
| Taunt | ✅ Greystone `Emote_Taunt_SwordPoint_T1` / `_BringItOn_T1`; `Mage_Shout` | ✅ Sparrow `Emote_Taunt_KickUpDust_M2` | 🔧 red ring: `NS_Magic_Circle1_*` tinted | ❌ shout | Enemy turning to face is code |
| Second wind | ✅ Paragon Kwang `Emote_DeepBreath_Intro/Loop/Outro` | (same, any body) | 🔧 `NS_Light_Magic_Heal` (green recolour) | 🔧 none fitting | — |
| Battle cry | ✅ Kwang `Emote_RallyUp_*`, Grux `Ultimate_Roar`, `Anim_DSS3_Buff_RM`, `Mage_Shout` | (same) | ✅ element `Shockwave` (Earth, Fire, Ice, Lightning, Water) + `Buff` on each ally | ❌ shout | Buff icon is UI |

## Plain summary for Crown

- **Good enough to start:** Sword + Shield, the Bolt, the buff, the blink, the projectile shapes, taunt, second wind, battle cry and the light, heavy and finisher animations.
- **Needs work:** the Lance (only an Ice spear exists), heal and debuff in most elements (recolours), the beam in five elements, a weapon trail, and the crippling low-cut animation.
- **Missing:** every combat sound (buy a pack), ring meshes for the adapters, and a bow weapon in the data. The bow path also needs a code check before it is built.
- **Paragon animations are action-game moves** (many are root motion and long). Each needs an eye check for a turn-based, fixed-position fight.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-12 | Created from the live editor's asset registry (unreal-mcp) | docs/showcase-inventory |
