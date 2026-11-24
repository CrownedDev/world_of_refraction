# SoftTofu "Slash and Hit VFX": catalogue

**What it is:** SoftTofuVFX's "Slash and Hit VFX" pack (`fab.com/listings/9d886fdb-8011-49e7-a10d-0d8897fe9dc2`), bought 2026-11-24 for D-273. It lives in `Content/SlashFX_SoftTofu/` (git-ignored, Crown's checkout only), 297 files. Read-only catalogue: nothing is wired in by this page.

**How this was read (2026-11-24):** from the folder on disk and the asset names; Crown was testing in Unreal, so no editor time was taken. The editor's asset queries answered "does not exist" for every asset during the session (even ours), so the per-asset details (lengths, skeletons, loop flags) were not read. Descriptions come from the names, the pack's store listing and the two effects already used in game (B-595e: `NS_Slash_Projectile_Horizontal_Wind`, `NS_Hit_Slash_Wind`). Lines marked **(check)** need a look in the editor.

**Licence:** no licence or readme file is in the folder (it holds only `.uasset` / `.umap`). A Fab purchase comes under Fab's Standard License (use in a shipped game; no reselling the raw assets). The Paragon characters and animations in `DemoRoom/` are Epic's free Paragon assets, licensed for Unreal projects.
**AI-generated:** the Fab listing was marked "Generated with AI: No" in Crown's Fab sweep (D-273). Nothing in the folder says otherwise (no AI-tool names in any file or folder name).

**Styles:** most effects come in up to 8 colour styles. The style is the folder (`Niagara/<Style>/`) and the name suffix:

| Style | Look (from the name and listing) | Use for us (D-273) |
|---|---|---|
| **Basic** | plain white / pale slash | **the plain physical look** (no element) |
| **Wind** | pale white crescent with wind streaks (seen in B-595e) | the plain look, and the **Wind** tint |
| **Distortion** | a clear heat-haze ripple | the plain look; the **Impact shockwave** (D-273) |
| Fire | orange flame slash | the **Fire** tint, or recolour Basic instead |
| Lightning | electric blue slash | the **Lightning** tint |
| Cosmic | purple / starry | the **Void** or **Darkness** tint (check) |
| Scifi | neon / cyan tech | none: off-tone (D-273: no magic sparkle) |
| Stylized | (only a slash and a hit) | check |

D-273 says the shape stays the same and the element only tints it. So the cleanest route is **Basic / Wind / Distortion recoloured per element** (colour and size are exposed parameters). Use the Fire / Lightning / Cosmic versions only if a recolour doesn't read well enough.

## 1. VFX (Niagara)

Paths are under `Content/SlashFX_SoftTofu/Niagara/`. Each row is one effect type; `<S>` is the style (`Basic/…`, `Wind/…_Wind`, etc.).

| Effect | Path (Basic / Wind) | What it is | Fits |
|---|---|---|---|
| **Charge energy** | `Basic/NS_Charge_Energy_Basic`, `Wind/NS_Charge_Energy_Wind` (+ all styles) | Energy gathering in to a point, held | **Ritual cast / charge**: **Heavy Attack's wind-up turn** (Crown's pick); camera moment **wind-up**. Also any `ActivationDelay` skill's arm turn |
| **Slash (in place)** | `Basic/NS_Slash_Basic`, `Wind/NS_Slash_Wind` | One crescent swipe on the swing | **Quick Strike**, **Over Under** (two, one per cut), **Heavy Attack** release; camera **hit** |
| **Claw slash** | `Basic/NS_Slash_Claw`, `Wind/NS_Slash_Claw_Wind` | Three parallel claw cuts | Not a sword look. Maybe Over Under's rising cut (check); later fist or claw weapons |
| **Projectile slash (flat)** | `Basic/NS_Slash_Projectile_Horizontal`, `Wind/NS_Slash_Projectile_Horizontal_Wind` | A flying crescent that travels | **Force Strike** (in use since B-595e); camera **cast** |
| **Projectile slash (spinning)** | `Basic/NS_Slash_Projectile_rotate`, `Wind/NS_Slash_Projectile_rotate_Wind` | A spinning crescent that travels | **WhirlWind**'s spin, played in place (check it can stay put); a stronger Force Strike variant |
| **Sword pierce** | `Basic/NS_Sword_Pierce_Basic`, `Wind/NS_Sword_Pierce_Wind` | A straight thrust of air | D-273's **Pierce** projectile; Lunge (a possible 5th sword skill) |
| **Hit** | `Basic/NS_Hit_Slash`, `Wind/NS_Hit_Slash_Wind` | A burst on the target | **Every skill's hit** (in use for Force Strike); camera **hit** |
| **Directional hit** | `Basic/NS_Hit_Slash_Direction`, `…_Direction_Wind` | A hit burst that points along the cut | **Quick Strike / Over Under** hits (a down cut, then an up cut); a **crit** |
| **Looping hit** | `Basic/NS_Hit_Slash_Loop_`, `…_Loop__Wind` | A hit that keeps flashing | A multi-hit or held hit; **WhirlWind** on each enemy during the spin |
| **Parry hit** | `Basic/NS_Hit_Parrying`, `Wind/NS_Hit_Parrying_Wind` | Sparks from a blocked blade | **Defence: guard / parry** (the DEFEND! window's success) |
| **Bottom spark** | `Basic/NS_BottomSpark`, `Wind/NS_BottomSpark_Wind` | Sparks / dust at the feet | **WhirlWind's slam**, **Heavy Attack's landing**; camera **slam**; a **dodge** step (check) |
| **Stylized slash / hit** | `Stylized/NS_Slash_Stylized`, `Stylized/NS_Hit_Slash_Stylized` | A second drawn-style slash and hit | check |
| **Judgement Cut** | `../JudgementCut/NS_JudgementCut_Start`, `_Start2`, `NS_JudgementCut`, `_End` | Many cuts across a space, start → cuts → end, with a refraction and RGB-split screen effect (`M_Refraction`, `M_PostProcess_RGB`, `MPC_JudgementCut`) and a sequence `LS_JudgementCut` | A **finisher / Overdrive** cinematic; a big **slam** or AoE skill. Too big for the four sword skills |

**Blueprints** (`Content/SlashFX_SoftTofu/Blueprint/`): the demo's ready-made actors that wrap the effects, for example `BP_Slash_Basic`, `BP_Slash_twice` (two slashes, an Over Under preview), `BP_Parring`, the `BP_Slash_Projectile_Horizontal_<S>` / `_Rotate_<S>` families and `BP_Slash_Projectile_Start` / `_Start_2`. They're useful for previewing; we spawn the Niagara systems ourselves, so we don't need them.

**Materials / meshes worth knowing:** `Material/M_Shockwave_`, `M_shokW_basic`, `M_SW_distort_Sprite` (shockwave and distortion: D-273's **Impact** shockwave); `M_Crack` (ground cracks: a **slam**); `M_AnimeHit` (an anime hit flash: the **hit** moment); `M_Spark`; `Resource/Mesh/SM_Slash_*`, `SM_Slashtwo_*`, `SM_Wind_*`, `SM_swiring` (the crescent and swirl shapes the effects are made from).

## 2. Sounds

Paths are under `Content/SlashFX_SoftTofu/Resource/Audio/`. `SW_` is a wave and `SC_` a cue (the cue picks one of its waves at random). All are short one-shots; Charging is the longest (judged by file size; check the lengths).

| Sound | Path | What it is | Fits |
|---|---|---|---|
| **Charging** | `charge/SC_Charging_Cue` (`SW_Charging`) | A rising charge-up | **Ritual cast / charge**: **Heavy Attack's wind-up**, paired with `NS_Charge_Energy` |
| **Basic slash** | `SharpSlash/SC_Basic_Slash_Cue` (`SW_Basic_Slash_01-03`) | A clean, sharp swish, 3 variations | **Quick Strike**, **Over Under** (one per cut), **Heavy Attack**; the plain physical sound |
| **Distortion slash** | `SharpSlash/SC_Distortion_Slash` | A swish with a warped edge | **Force Strike**'s flying cut; an Impact shockwave |
| **Parry** | `SharpSlash/SW_Parrying_01_Cue`, `SW_Parrying_02_Cue` | A metal clash | **Defence: guard / parry** |
| **Wind slash** | `Wind/SC_Wind_Slash_Cue` (`SW_Wind_Slash_01-02`) | An airy whoosh | **Force Strike**'s projectile in flight; the **rush** (dash) whoosh; Wind tint |
| **Rotating slash** | `RotatingSlash/SC_RotatingSlash1_Cue`, `SW_RotationgSlash_02_Cue` | A spinning whoosh | **WhirlWind**'s spin |
| **Hit** | `Hit/SC_Hit_Cue` (`SW_Hit_01-02`) | A flesh / impact hit | **Every skill's hit**; camera **hit** |
| **Pierce** | `pierce/SW_pierce_Cue` | A stab | D-273's **Pierce** projectile; Lunge |
| Fire slash | `Fire/SC_Fire_Slash_01_Cue` | A flame whoosh | **Fire** tint layer (check against D-273: no magic) |
| Lightning | `Lightning/SC_Lightning_Cue` | An electric crack | **Lightning** tint layer |
| Dark slash | `Dark/SC_Slash_Dark_Cue`, `SC_Slash_Dark2_Cue` | A deep, heavy swish | **Heavy Attack**'s release (weighty); **Darkness** tint |
| Cyberpunk slash | `Cyberpunk/SC_Slash_Cyberpunk_Cue` | A synthy swish | none: off-tone |
| Attenuation | `SA_Attenuation_Short`, `SA_Attenuation_Middle` | Distance falloff settings | Reuse for our combat sounds if ours lack them |

## 3. Animations (demo only)

The pack has no animations of its own. Its demo room includes Epic's free **Paragon** heroes to show the effects. They use Paragon skeletons, not our UE5 Manny, so we'd need to **retarget** them before use. Our sword skills already use the Combat Master bundle on Manny.

| Animation | Path (`DemoRoom/…`) | What it is | Fits |
|---|---|---|---|
| Kwang `Ability_R_Intro_<Style>` (7: Basic, Cosmic, Distortion, Fire, Lightning, Scifi, Wind) | `ParagonKwang/Characters/Heroes/Kwang/` | Kwang (a sword hero) drawing up for his ultimate; one copy per style, probably each with that style's effect notifies (check) | A reference for how the pack times **charge → slash**: a **ritual cast** model. Retarget only if Heavy Attack's wind-up needs a better pose than our Buff gesture |
| Greystone `Attack_A_Fast`, `Attack_B_Fast`, `Attack_PrimaryC` | `ParagonGreystone/…/Animations/` | A greatsword hero's light combo swings | Reference only (two-handed); possible **greatsword** skills later |
| Greystone `Ability_Q`, `Ability_E`, `Ability_E_Slash`, `Ability_R` | same | His skills: a leap / slam, a charged slash, an ultimate | Reference for **slam** and **charged** timing (WhirlWind's slam, Heavy Attack) |

## 4. Demo levels

- **`Map/Overview_3`**: the showcase map, every effect in every style laid out. Open it to compare styles before picking (it doesn't need PIE).
- **`JudgementCut/L_JudgementCut`** + `LS_JudgementCut`: the Judgement Cut cinematic's demo, with the camera sequence and the refraction / RGB-split screen effect. A model for a later finisher or Overdrive cinematic. The refraction screen effect suits this game's name.
- **`DemoRoom/`**: the room meshes, lighting and the Paragon characters. Nothing for us.

## 5. Best picks per sword skill

Plain look = Basic or Wind, recoloured per element (D-273).

- **Quick Strike:** `NS_Slash_Basic` on the swing; `NS_Hit_Slash_Direction` on the hit; `SC_Basic_Slash_Cue` + `SC_Hit_Cue`; `SC_Wind_Slash_Cue` on the dash.
- **Force Strike** (in use): `NS_Slash_Projectile_Horizontal_Wind` in flight, `NS_Hit_Slash_Wind` on arrival; add `SC_Wind_Slash_Cue` (release) + `SC_Hit_Cue` (arrival). No sound yet.
- **Over Under:** two `NS_Slash_Basic` (or `BP_Slash_twice` as the model), one per Impact; two `NS_Hit_Slash_Direction` (down, then up); `SC_Basic_Slash_Cue` per cut.
- **Heavy Attack:** wind-up turn = **`NS_Charge_Energy_Basic` + `SC_Charging_Cue`** (the ritual cast); release = `NS_Slash_Basic` scaled up + `NS_Hit_Slash` + `NS_BottomSpark` and `M_Crack` on the landing; `SC_Slash_Dark_Cue` for weight.
- **WhirlWind:** `NS_Slash_Projectile_rotate_Wind` kept in place for the spin (check) + `SC_RotatingSlash1_Cue`; the slam = `NS_BottomSpark` + a `Distortion` shockwave + `NS_Hit_Slash` on every enemy.
- **Defence (all skills):** guard / parry = `NS_Hit_Parrying` + `SW_Parrying_01_Cue` / `_02_Cue`.
- **Later:** the Judgement Cut set for a finisher or Overdrive; `NS_Sword_Pierce` + `SW_pierce_Cue` for a Lunge skill.
