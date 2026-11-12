# Asset audit: what the art we own can build

**2026-11-09**, side session for the brain (desktop operator). Read-only: built from file and folder **names and sizes** in the main checkout (`D:/github/world_of_refraction/Content`, which holds the git-ignored vendor packs) and `SourceArt/`. No `.uasset` was opened, and Unreal was closed. Inputs: `VisualGameplayPlan.md`, D-163 (six schools), D-170 (two guards, parry effect), `FeelGuide.md`, `VisualDirection.md`, `ContentNaming.md`.

**Confidence key** (every judgement carries one):

| Mark | Means |
|---|---|
| **H** | The name and folder say it plainly (e.g. `Mage_Fireball_R_IP` under `InPlace/`). It exists and is that kind of thing. |
| **M** | The name suggests it, but fit is unchecked: skeleton, retarget quality, scale, colour or timing. |
| **L** | A guess from a pack or folder name only. |
| **Eye** | Needs Crown's eye or an editor thumbnail. Names never say whether something **looks good** or fits the painterly Rematch brief. |

Nothing in this report says anything looks good. Every "ready" means "exists and is the right kind of asset", not "approved".

---

## 1. Inventory

### Where it lives

| Place | In git? | What |
|---|---|---|
| `Content/Data`, `Blueprints`, `Core`, `Level`, `Testing`, `UI/Core`, `UI/UMG`, `VFX`, `RPGIcons`, `MSPresets` | yes (LFS) | Project data, old HUD widgets, a Niagara **library** (not effects), icon and material-preset packs |
| `SourceArt/UI/Hidris/commandmenu` | yes | Hidris command-menu art: V1 16 PNG, V2 7 PNG, the `.afdesign` source |
| `Content/Imports/*`, `CombatMasterAnimBundle`, `StylizedWeapons`, `SoStylized`, `UI/deckbuilder` | **no** (git-ignored) | Every vendor pack below. Only on Crown's machine. |

About **62,900 files, ~19.6 GB**, not counting `__External*`. 49,088 of the files are `UI/deckbuilder` (the Godot deckbuilder's art and import files).

**Duplicate:** `Content/CombatMasterAnimBundle` (1,430 files, 1.4 GB) and `Content/Imports/CombatMasterAnimBundle` are the same pack twice. Belongs to B-122's cleanup. **H**

### Characters and skeletons

| Asset | Count | Notes | Conf. |
|---|---|---|---|
| `SKM_Manny_Base`, `SKM_Quinn` (UE5 mannequins) | 2 | The only player/enemy bodies in `Content`. Every animation pack below targets this skeleton. | H |
| Combat Master demo mannequins (`SKM_Manny_DSAS1`, `_DK1` …) | ~65 | Mannequin copies per sub-pack; same body. | H |
| UE4 mannequin (`SK_Mannequin`, Mage pack) | 1 | UE4 skeleton. | H |
| Paragon **meshes** | **0** | Only the Paragon **animations** were imported (retargeted onto Manny). | H |
| MetaHuman (Eric), VRoid (V, Amethyst) | 0 here | On unmerged branches `chore/eric-metahuman-test`, `chore/B-280-vroid-pipeline`. | M |
| Creature / non-human skeletons | **0** | None. | H |
| `BP_CombatCharacter_*` (12, one per element + Generic, Resonator, BrokenDarkness), `BP_Enemy_Generic`, `DA_Enemy_Darkness/Generic/Resonator` | — | Gameplay Blueprints and data, not art. | H |

### Weapons and shields

| Pack | Meshes | Kinds | Conf. |
|---|---|---|---|
| StylizedWeapons WeaponPack1 (Fantasy) | 42 SK | 1H axe 6, 2H axe 6, dagger 2, hammer 4, polearm 2, **shield 9**, staff 4, 1H sword 3, 2H sword 6 | H |
| StylizedWeapons WeaponPack2 (Generic/Runic/Ud) | 70 SK | arrow 5, 1H axe 10 (one `_03Off` off-hand), 2H axe 8, **bow 17** (+5 bow anims), **fist 6** (claws, `MetalPunch`), quiver 8, 1H sword 12, 2H sword 4 | H |
| 100 Fantasy Weapons Basemesh | ~105 | swords, katanas, greatswords, rapier, daggers, sai, scythes, halberds, spears, bows, guns, hammer, wand, **shield 4**. "Basemesh" with 4 flat materials (`mat_Black_Matte` …), so likely untextured. | H (exists) / M (untextured) |
| Combat Master weapons | 12 SM | axe, twin axe, greatsword, katana ×3, sword ×3, **shield ×2** (`SM_HSAS_Shield`, `SM_Shield_DSAS`) | H |
| **Rings (adapters)** | **0** | No ring meshes. | H |

### Animations (all on the UE5 Manny skeleton unless noted)

Counts are by keyword in the name and are approximate (± a few per cell). **M** overall.

| Pack | Total | In-place | Parry/block | Dodge | Death | Hit | Cast | Attack | Idle | Locomotion | Victory/intro |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Paragon (32 hero sets, `*Manny`) | 5,359 | — | 8 | 53 | 68 | 271 | 49 + many `Ability_*` | 1,380 | 712 | 2,358 | 218 (32 `LevelStart`, 4 `VictoryPose_1`, 2 `Victory_Emote`) |
| Combat Master bundle (17 sub-packs; UE5 copies ≈ 650) | 1,259 | 220 | 79 | 21 | 13 | 89 | 5 (buffs) | 732 | 41 | 261 | 4 |
| Mage Animation Set (54 IP + 54 RM pairs) | 108 | 54 | 0 | 16 | 2 | 10 | 32 | 6 | 2 | 36 | `EnterCombat` / `ExitCombat` |
| MagicAnimations (Manny/Quinn) | 61 | ? | 0 | 0 | 0 | 0 | 61 | — | — | — | — |
| Dodge pack | 51 | ? | — | 51 | — | — | — | — | — | — | — |
| Sport / Free packs | 64 | — | Mostly basketball, golf, eating: **not useful** | | | | | | | | |
| Project `Anim_*` / `AM_*` | 17 | — | — | — | — | — | — | 13 (DualBlade, SAS) | 2 | 2 (leap in/out) | — |

**Weapon sub-packs in Combat Master** (UE5 only):

| Sub-pack | Weapon | Anims | Guard anims | Hit / death | Root motion? | Conf. |
|---|---|---|---|---|---|---|
| DSAS V1 / V2 / V3 | Sword + shield | 74 / 73 / 38 | **V2: Block, BlockHit, Block_Move ×8, turns** | V2 8 hits; V3 death | both IP and RM | H |
| HandsomeSwordAndShield | Sword + shield | 25 | — | 4 hits, death, evade, buff | RM | H |
| HandsomeSword V1 / V2, PowerfulSword | 1H sword | 30 / 32 / 35 | **none** | a few hits | IP + RM | H |
| DynamicGreatSword | Greatsword | 22 | **Block Start / Loop / Hit / End** | 4 hits, death | **RM only** | H |
| DynamicAxe | 1H axe | 25 | **none** | 3 hits, death, buff | RM | H |
| DynamicTwinAxe | Dual axe | 29 | **Block Start / Loop / Hit / End** | 5 hits | RM | H |
| HandsomeTwinSword | Dual sword | 26 | `Anim_Tb_Block` | 3 hits | RM | H |
| DynamicHandtoHand V1 / V2 | Fists / gauntlets | 25 / 36 | **V2: Block1–3, BlockHit1–3** | V2 9 hits | RM | H |
| Katana ×4 packs | Katana | 182 | `Katana_Block` | some | IP + RM | H |

### VFX

| Pack | Niagara systems | Contents | Conf. |
|---|---|---|---|
| **Big Pack Magic VFX** | **337** | 10 sets: Fire 31, Ice 29, Water 30, Earth 30, Air 35, Lightning 37, Light 42, Dark 34, Blood 37, Poison 32. Each has projectile, muzzle, hit, AOE, aura, buff, shield, wall, slash, target, dash; some have circle, beam, debuff, heal. | H (exists) / Eye (style) |
| Aura Pack | 25 | `NS_Aura_Pack_Aura_1–25`, numbered only. | H / Eye |
| Magic Circle 1 | 15 | `NS_Magic_Circle1_Circle1–15` + 77 MIs: a ready source for the ring cast and the parry-effect cosmetic. | H / Eye |
| SoStylized effects | 7 | lightning strike, electric build-up, rain and fog, snow, wind lines, meteor shower, plus camera shakes. | H |
| Mage pack FX | 11 | `*_P` names: **Cascade** particles (legacy), e.g. `FireballTrail_P`, `Beam_2h_P`, `Telekinesis_*_P`. | M (Cascade from the suffix) |
| `Content/VFX` | 0 effects | The Niagara library (enums, modules, functions), not effects. | H |

**By the game's elements** (`ESpellElement`: Fire, Water, Earth, Wind, Light, Darkness, Lightning, Void, Reality):

| Element | Source | Status | Conf. |
|---|---|---|---|
| Fire | Fire_Magic (31) | Ready | H / Eye |
| Water | Water_Magic (30) + Ice_Magic (29) for the ice spells (IceLance, Blizzard, AbsoluteZero) | Ready | H / Eye |
| Earth | Earth_Magic (30) | Ready | H / Eye |
| Wind | Air_Magic (35) | Ready | H / Eye |
| Light | Light_Magic (42), the only `Heal` effects | Ready | H / Eye |
| Darkness | Dark_Magic (34) | Ready | H / Eye |
| Lightning | Lightning_Magic (37) + SoStylized strike | Ready | H / Eye |
| **Void** | none | **Tweaks:** recolour Dark or Blood to violet; the galaxy material (VisualDirection §2b) for the look | M |
| **Reality** | none | **Missing / design:** no obvious source. Could recolour Light plus magic circles, but Reality's look isn't decided. | L |
| Generic (inherit / none) | Magic circles, auras | Tweaks: neutral tint | M |
| Extra: Blood, Poison | 69 | Not elements. Good for bleed / poison build-up and debuff markers. | M |

### Materials

| Asset | Status | Conf. |
|---|---|---|
| Pack materials (VFX MIs, weapon MIs: 327 in StylizedWeapons) | Exist per pack | H |
| `MSPresets` (43, Megascans presets), `M_DebugElement` | Exist | H |
| Painterly masters `M_Painted_Opaque`, `M_Crystal`, `M_Void` (VisualDirection §1) | **Missing**: make | H |

### UI art

| Asset | Count | Notes | Conf. |
|---|---|---|---|
| `SourceArt/UI/Hidris/commandmenu` | 23 PNG + source | Crown's chosen HUD art (UI theme hand-over). | H |
| `RPGIcons` | 148 textures | Generic RPG icons. | H / Eye |
| IdleTycoon icons | 500 | Cartoon upgrade icons; probably off-style. | L / Eye |
| DynamicPieMenu | 9 widgets | Radial menu sample. | H |
| `UI/deckbuilder` (Godot) | 49,088 files | PONETI (36.5k), 1_Free_Pack (6.6k), casual-ui, FantasyGameGUI, Kitsinu, plus a library of frames, badges and icons. Reuse rules: `UI_DeckbuilderReuse.md`. | H |
| Old `WBP_*` HUD | 13 | Replaced by B-391's plain C++ HUD. | H |

### Audio

| Asset | Count | Notes | Conf. |
|---|---|---|---|
| SoStylized sounds | 22 | rain, wind, nature, lightning, dirt footsteps | H |
| PONETI sounds (deckbuilder) | 17 | Freesound files (e.g. `hit-metallic-01`, `melee-attack-with-voice`, `magic-missiles`). **Licence per file**: check before use. | H / M (licence) |
| Combat SFX (clang, swing, impact, cast per element, UI) | **0** | **Missing** | H |
| Music | 0 | Parked (FeelGuide 12). | H |

---

## 2. Mapping onto the game

**Status:** ✅ ready now · 🔧 buildable with tweaks · ❌ missing (buy or make).

### Skills by school (D-163)

| School | What it needs | What we own | Status | Conf. |
|---|---|---|---|---|
| **Destruction** (damage) | cast anim, muzzle, projectile/beam/AOE, hit | Mage `Fireball_L/R_IP`, `Beam_*`, `GroundSlam`; MagicAnimations `PrepareAndFire`, `MagicBall`, `Turn-basedAttackPL/RT`; per-element Projectile / Muzzle / Hit / AOE / Beam / Meteor / Slash | ✅ for the 7 owned elements; 🔧 for Void; ❌ for Reality | H (exists) / Eye |
| **Conjuration** (a conjured thing) | a thing that appears and persists | Walls (Fire, Ice, Water, Earth ×2, Dark ×2, Blood ×2), Spikes, `Ice_Spear`, `Light_Sword_*` (7), `Blood_Crystal1–6`, Tornadoes; Mage `Telekinesis` | 🔧 They're bursts: a looping "held" state may need to be built for things that persist | M |
| **Enhancement** (buffs) | buff aura, shield | `*_Buff`, `*_Aura`, `*_Shield` per element + 25 Aura Pack; casts `Enchants`, `CollectMana`, `Shout`, CM `Buff` | ✅ | H / Eye |
| **Restoration** (heals, cleanses) | heal burst, cleanse | Only `Light_Magic_Heal` / `Heal_Hit`. Water splash and flow could stand in. Casts `Worshiped`, `ReadsTheSpell` | 🔧 Recolour the light heal per element; a cleanse effect is ❌ (make) | M |
| **Impairment** (stun, skip turn, silence, debuffs, build-up) | debuff marker, stun loop, freeze | `*_Debuff` (Dark, Blood, Poison), `Ice_Frozen`, SoStylized `ElectricBuildup`; Paragon `Stun_Start/Loop`; casts `Bewitchment` | 🔧 Stun and silence markers are ❌; status icons (small, per FeelGuide 5) are ❌ | M |
| **Manipulation** (confusion) | a "mind" effect | casts `Hypnotizing`, `Bewitchment` (names fit); nothing in VFX | 🔧 animation / ❌ VFX (recolour a circle or aura over the head) | L |

### Weapon families, their anims and guards (D-170)

Families come from `Data/Pool/Generic/Weapons`.

**Meshes are shared per family (Crown, 2026-11-12):** one sword mesh family serves sword and shield, dual swords, katana and dual katana; one one-handed axe family serves single axe, twin axes and axe and shield. The animation set decides the style. Styles and their sets: `ShowcaseInventory.md` "Weapon families".

| Family (data) | Meshes | Attack anims | Weapon guard | Status | Conf. |
|---|---|---|---|---|---|
| Sword + Shield (4) | swords + 15 shields | DSAS V1–V3, HSAS (IP available) | ✅ `Anim_SAS_Block`, V2 `BlockHit`, block moves | ✅ | H |
| Greatsword (4) | 2H swords (10 SK + many basemesh) | DynamicGreatSword 22, **root motion only** | ✅ Block Start/Loop/Hit/End | 🔧 Attacks need in-place versions or per-montage root-motion opt-in | H |
| Dual Axe (2) | 1H axes incl. `_03Off` off-hand | DynamicTwinAxe 29 + project `Anim_DualBlade_*` | ✅ Block set | ✅ | H |
| Dual Sword (3) | 1H swords, katanas | HandsomeTwinSword 26, katana packs | ✅ `Anim_Tb_Block` (one clip) | 🔧 Only one clip: no block-hit | H |
| Gauntlets (1) | 6 fist weapons | DynamicHandtoHand V1/V2 61 | ✅ V2 `Block1–3`, `BlockHit1–3` | ✅ | H |
| Sword 1H (6) | many | HandsomeSword V1/V2, PowerfulSword, katana packs | ❌ No 1H block clip (Katana has `Katana_Block`; Paragon `Greystone Deflect_InPlace` may fit) | 🔧 | M |
| Axe 1H / 2H (4) | 18 1H + 14 2H SK | DynamicAxe 25 | ❌ none | 🔧 Borrow DynamicTwinAxe's block, or ❌ buy | M |
| Dual Dagger (3: Sai, Twin Daggers, Kris) | 2 SK daggers + basemesh daggers, `Sai`, `Sai_2` | none dedicated; Paragon **Kallari** (dual blades) | Kallari `Ability_Block` | 🔧 Retarget check | M / Eye |
| Spear (test only), Staff, Bow | polearms, staffs, 17 bows | Paragon Wukong (staff), Sparrow (bow); 5 `A_Bow_*` | — | No such family in data yet | L |
| **Adapters (rings)** | ❌ no ring meshes | casts as above | the unarmed guard (below) | Rings are worn, so a mesh may not be needed (design question) | M |

**Unarmed guard** (adapter users, Refractors, default Generic): DynamicHandtoHand V2 `Block1–3` + `BlockHit1–3` is the closest owned "body" guard. Mage set has no block. 🔧 Root motion is small for a block, but it needs a check. **M / Eye**

**Parry-effect cosmetic** (landed parry): `NS_Magic_Circle1_Circle1–15` (77 colour MIs) and per-element `*_Shield_Splash` (Blood, Poison, Dark, Light, Lightning, Water) both fit "a magic circle, coloured by the element". 🔧 Needs an element-colour parameter. **M / Eye**

**Weapon parry sparks + clang** (FeelGuide 3): ❌ no spark burst and no metal clang in the packs (PONETI's `hit-metallic-01` is a stand-in; check its licence). Make a small Niagara burst, or take one from Epic's free Niagara Examples Pack. **H**

### Enemies

| Need | What we own | Status | Conf. |
|---|---|---|---|
| Humanoid enemy body | Manny / Quinn + an element MI | 🔧 Placeholder now: any Manny enemy reuses every player animation (fits FeelGuide 14: telegraphs belong to the skill) | H |
| Distinct humanoid enemies | Paragon animations for 32 heroes + **minions** (85 anims: attack, hit, death); the Paragon **meshes** aren't imported | 🔧 The Paragon characters are free on Fab (UE-only licence): import the meshes we want. Style vs the painterly brief: **Eye** | M |
| Story characters (V, Eric, Amethyst) | other branches (MetaHuman, VRoid) | 🔧 Merge when those pipelines land | M |
| Creatures / monsters | nothing | ❌ Buy: no non-human skeletons or anims at all | H |
| Bosses | Paragon hero sets (e.g. Grux, Greystone, Rampage-like brutes) | 🔧 Same as distinct humanoids; boss bar is UI | L |

### Vertical-slice fight (B-391 to B-395)

| Slice moment | Assets | Status | Conf. |
|---|---|---|---|
| Arena + sky | `TestLevel_Encounter`; SoStylized `BP_StylizedSky` + 9 presets + weather | 🔧 Grey-box with the sky; no environment art | H / Eye |
| **Crystal-break entry** (FeelGuide 1) | nothing named "shatter"; `Blood_Magic_Crystal*`, `Earth_Spike*` as inspiration only | ❌ Make: screen capture + shatter material or a fractured-mesh Niagara | H |
| Player (Generic, sword + shield) | Manny + `SM_Sword_DSAS` / `SM_Shield_DSAS`; DSAS V1 in-place combos; `DA_SAS_Attack_F` exists | ✅ | H / Eye |
| Caster spell | Mage `Fireball_R_IP` + `NS_Fire_Magic_Muzzle` → `Projectile` → `Hit`; `DA_Spells_Fire_FireBall` exists | ✅ | H / Eye |
| Ring cast VFX (B-394 default) | `NS_Magic_Circle1_*` | 🔧 Pick one, add an element colour | M / Eye |
| Enemy | Manny + dark MI + `DA_Enemy_Darkness`; gauntlet anims (DHTH) | 🔧 | M |
| Hits (B-395, FeelGuide 4) | DSAS V2 `Hit_F_Small/Middle_IP`, Mage `Stagger_F/B/L/R_IP`, Paragon `HitReact_*` | ✅ | H |
| Defence: parry (B-392/393) | weapon guard SAS V2 `Block/BlockHit`; unarmed DHTH2 `Block`; Magic Circle cosmetic | 🔧 Sparks + clang ❌ | H / M |
| Defence: dodge (`IA_DodgeLeft/Right`) | `Mage_Dodge_L/R_IP`, Dodge pack (51), CM dodges | ✅ | H / Eye |
| Death | CM `Die`, `Mage_Death_IP`, HSAS `Dead`, Paragon `Death_*` | ✅ | H |
| Victory pose (big fights only) | Paragon `VictoryPose_1` (FengMao, Phase, Wraith, Steel), `Victory_Emote` (Kallari, Steel) | 🔧 Retarget check | M / Eye |
| HUD (B-391) | plain C++ widgets, no art needed; Hidris later | ✅ | H |
| Status icons, damage-number colours | RPGIcons (generic); no status set | 🔧 Placeholder text is enough for the slice | M |
| Combat SFX | none | ❌ | H |

---

## 3. Gaps and the top 10 to build first

### Biggest gaps

| Gap | Fix | Conf. |
|---|---|---|
| **Combat audio**: no clang, swing, impact or cast sounds | Buy one combat SFX pack, or use checked-licence Freesound files | H |
| **Enemy and creature bodies**: only mannequins | Import Paragon meshes (free) for humanoids; buy a creature pack later | H |
| **Crystal-break transition** | Make it (material + Niagara) | H |
| **Parry sparks** | Make a small Niagara burst, or take one from Epic's free Niagara Examples | H |
| **Void and Reality VFX** | Recolour for Void; Reality needs a look decided first | M |
| **Restoration, Impairment, Manipulation effects**: thin (one heal, three debuffs, no confusion, stun or silence) | Recolour existing pieces; a buff/debuff pack (VisualDirection §2d) if recolours look weak | M |
| **Guard clips for 1H sword, axe, dual dagger** | Borrow from a neighbour pack or buy a sword and shield/guard pack | M |
| **Painterly master materials** (`M_Painted_Opaque`, `M_Crystal`, `M_Void`) | Make (VisualDirection §1); every pack is unstyled until this exists | H |
| **Ring meshes** | Only if rings show on the body: a design question | M |

### Top 10 for the vertical slice

In build order. Each is a small job Crown reviews (VisualGameplayPlan step 2).

| # | Build | From | Status | Conf. |
|---|---|---|---|---|
| 1 | **Player on screen**: Manny, sword + shield, idle + in-place attack | DSAS V1 `Combat_Idle_IP`, `Combo1_IP`, `SM_Sword_DSAS`, `SM_Shield_DSAS` | ✅ | H / Eye |
| 2 | **Enemy on screen**: Manny with a dark MI, gauntlet set | DHTH V1/V2, `DA_Enemy_Darkness` | 🔧 | M / Eye |
| 3 | **Hit reactions + death** for both | DSAS V2 `Hit_F_*_IP`, Mage `Stagger_*_IP`, `Mage_Death_IP`, CM `Die` | ✅ | H / Eye |
| 4 | **Fire spell**: cast, projectile, impact | `Mage_Fireball_R_IP`, `NS_Fire_Magic_Muzzle` / `Projectile` / `Hit` | ✅ | H / Eye |
| 5 | **Two guards** (B-392) | weapon: SAS V2 `Block` / `BlockHit`; unarmed: DHTH2 `Block1` / `BlockHit1` | 🔧 | M / Eye |
| 6 | **Parry sparks + clang** (FeelGuide 3) | make a Niagara burst; a stand-in sound | ❌ | H |
| 7 | **Parry-effect cosmetic and ring cast** (B-393/394) | one `NS_Magic_Circle1_*` with an element colour | 🔧 | M / Eye |
| 8 | **Dodge left/right** | `Mage_Dodge_L/R_IP` or the Dodge pack | ✅ | H / Eye |
| 9 | **Crystal-break entry** (FeelGuide 1) | make | ❌ | H |
| 10 | **Arena, sky and victory pose** | `TestLevel_Encounter` + a SoStylized sky preset; Paragon `VictoryPose_1` | 🔧 | M / Eye |

---

## 4. Confidence: how far to trust this

| What | Trust | Why |
|---|---|---|
| An asset **exists** and **is the kind named** (anim, NS, mesh) | High | Names, prefixes and folders are explicit. |
| Counts by category | Medium | Keyword matching on names. Off by a few per cell (e.g. `_R_` read as an attack). |
| **In-place vs root motion** | High where the folder says `InPlace` / `RootMotion` or `_IP` / `_RM`; unknown for MagicAnimations and the Dodge pack | Folder names. |
| **Skeleton** | Medium | Folder names (`Manny_UE5`, `*Manny`) and demo mannequins, not the asset's real skeleton reference. Paragon `*Manny` folders look like retargets onto Manny: check one in the editor. |
| **Retarget quality** (feet sliding, weapon grip, hand positions) | **None** | Needs the editor. Paragon and Kallari's daggers especially. |
| **Looks good / fits the painterly Rematch brief** | **None** | Names say nothing about style. Big Pack, Aura Pack and the weapon packs are "stylised" by name only. Crown's eye. |
| VFX **scale, colour, overdraw, 5.8 compatibility** | None | The editor or PIE. The Mage pack's Cascade effects may need replacing. |
| **Licences** (PONETI audio, deckbuilder art, Paragon UE-only) | Low | Not readable from names. Check before shipping. |

**What Claude can judge, and how to make it useful:** from names, only existence. With the editor open, Claude can render thumbnails or short captures through the MCP and spot gross faults: T-poses, broken materials, wrong scale, clipping, a weapon off the hand, an effect in the wrong colour. It can also check timing marks against `SkillPresentationPipeline.md`. **Taste and feel stay Crown's call.** A useful next step: a contact sheet of thumbnails for the top-10 candidates (one editor session), for Crown to pick from before any job starts.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-09 | Created: inventory, mapping, top 10, confidence | docs/asset-audit |
