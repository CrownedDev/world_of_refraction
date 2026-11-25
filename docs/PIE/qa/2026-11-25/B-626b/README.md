# B-626b: Super Saiyan-style flame candidates (2026-11-25, agent shots)

Practice fight on Crown's save, PIE only. The effects were put on the live hero, nothing was saved and no asset was made or changed. The equipped sword was `SK_Sword_1H_Fantasy_01`.

**Weapon.** The sword-infusion effects are in `/Game/SwordVFX`, not in SoftTofu. `SlashFX_SoftTofu` only has slashes, hits and one-shot charges. Its `NS_Charge_Energy_Fire` is over before a shot can catch it, and `NS_Hit_Slash_Loop_Fire_` is a screen-filling flash. The SwordVFX effects are built for their own sword, so they need a yaw of -90 to run along this blade.

| # | System | Pack | Tintable | Notes |
|---|---|---|---|---|
| 1 | `/Game/SwordVFX/SwordEffects/NS_FireSword` | SwordVFX | no | Orange flames along the blade, loops. **Crown's pick: weapon infusion default for Fire** |
| 2 | `.../NS_FireSwordB` | SwordVFX | no | The same flames in blue (a separate copy). **Crown: good as the Water default** |
| 3 | `.../NS_FireSwordG` | SwordVFX | no | The same flames in green |
| 4 | `.../NS_Rainbow` | SwordVFX | no | Bright gold flame wrapped round the blade, the closest to SSJ. `MainColor` changes nothing |

**Crown's plan:** one default per element, the rest become cosmetics; all of them (defaults too) recoloured to the element. **Recolour:** `User.MainColor` only feeds the fire's start colour; a later Color-from-curve module overwrites it, which is why it does nothing. A copy with that module multiplied by `MainColor` (or switched off) would tint: editor work, not done yet. **Shape:** the fire samples the mesh it is attached to (`SampleStaticMesh`, source Default; falls back to `SM_Sword_Blade`), so it follows any static-mesh weapon. All our weapon data assets use static meshes.

**Level 2 look (Crown: "that works for now"):** L1 is a small concentrated flame, L2 is aggressive and wide. Test copy `/Game/QA_Scratch/B626/NS_FireSword_L2Test` (unsaved): Fire emitter InitializeParticle Sprite Size Min (2,5)→(5,12), Max (4,6)→(10,15), Lifetime Max 1.75→2.4, StaticMeshLocation Normal Offset 0→5; component overrides Rate2 500→2000, Speed1 100→300, Speed2 50→150. The shipped effect's user params alone (1.5× rate/speed) made no visible change.

**Recolour (`/Game/QA_Scratch/B626/NS_FireSword_Tint`, and `_Tint_L2` with the L2 size edits; unsaved):** Fire, Trail and Sparks Color modules rewired. Fire: `ScaleLinearColorByCurve(LerpLinearColors(User.CoreColor → User.MainColor by NormalizedAge), curve RGB 50→12 (t0.113)→0 (t0.71), A 1→0 (t0.9))`, the original's brightness shape. Sparks use CoreColor, Trail MainColor. **Palette test (Crown: Wind as jade #00A86B, the rest follow its saturation/depth):** main = hue at jade's S 1.0 / V 0.66, core = same hue, S×0.55, V 1. Fire #A81100, **Lightning orange #A84F00, Earth yellow #A89200** (Crown: "set in stone": red Fire, blue Water, yellow Earth, green/jade Wind, orange Lightning, as in `docs/Design/CharacterDesigns/DriveFacts.md:48`; `ElementColors.h` has these two the other way round), first-pass values were Lightning #A88600 / Earth #A84C00, Wind #00A86B, Water #0049A8, Ice #5494A8 (S 0.5). Linear main/core: Fire (0.392,0.006,0)/(1,0.219,0.171); Lightning (0.393,0.057,0)/(1,0.133,0) deep #A84300, core = Crown's swatch #FF6600; Earth (0.393,0.393,0.007)/(1,1,0.014) deep #A8A814, core = Crown's swatch #FFFF1F (Crown found them too alike and sent these two swatches); Void (0.307,0.016,0.393)/(0.806,0.084,1) #9622A8 (moved toward magenta, apart from Reality); Ice now (0.057,0.351,0.393)/(0.205,0.906,1) #43A0A8 cyan (apart from Water); L1 brightness curve lowered to 20→7→0 (120 blew every core out to white); Reality (0.012,0,0.393)/(0.255,0.171,1) #1C00A8; **White rule (Crown):** no white except Light. Light = pure white core fading to light grey; Darkness = the same in dark greys. Component values are pre-divided by the brightness curve so the greys stay grey: Light L1 main 0.075 / core 1, L2 main 0.13 / core 1; Darkness: **pure black** core and main (Crown; the flame material `M_SwordSmoke` is translucent, not additive, so black shows). **Untainted cores (Crown):** every other core is its own hue at full saturation and brightness, no white mixed in: Fire #FF1900 (1,0.01,0), Water #006FFF (0,0.157,1), Wind #00FFA2 (0,1,0.359), Ice #66F2FF (0.133,0.89,1), Void #E433FF (0.774,0.033,1), Reality #2A00FF (0.024,0,1); Lightning and Earth keep Crown's swatches. **Smoke:** the `Blood_Smoke` emitter is switched off in both scratch copies; Wind (0,0.392,0.147)/(0.171,1,0.604); Water (0,0.066,0.392)/(0.171,0.432,1); Ice (0.089,0.298,0.392)/(0.484,0.86,1). Not yet in `ElementColors.h`.

Showed nothing on this sword: `NS_ElectricSword`, `NS_Vortex`.

**Personal (body).** All 23 Aura Pack effects were shot (#14 is the overview). Aura Pack effects take colour (`Color_*` user params) and size (`Scale_All`, `Offset`). This was checked on 10, 17 and 25.

| # | System | Pack | Tintable | Notes |
|---|---|---|---|---|
| 5 | `/Game/Imports/Aura_Pack/VFX_Niagara/NS_Aura_Pack_Aura_25`, tinted gold | Aura_Pack | yes | Rising flame shell round the whole body. **Most like the SSJ power-up.** Recommended |
| 6 | Aura_25 as shipped | Aura_Pack | yes | Green |
| 7 | `NS_Aura_Pack_Aura_10` | Aura_Pack | yes | Fire rising with lava cracks on the ground |
| 8 | `NS_Aura_Pack_Aura_17` | Aura_Pack | yes | Yellow lightning round the body with ground cracks, like SSJ2 |
| 9 | `NS_Aura_Pack_Aura_11` | Aura_Pack | yes (pack pattern, not checked) | Soft green rising glow |

**Ring (right hand).** Placed at the sword grip with world-up rotation and `Scale_All` reduced.

| # | System | Scale | Notes |
|---|---|---|---|
| 10 | Aura_25 gold | 0.20 | Gold flame licking up from the hand, subtle |
| 11 | Aura_25 gold | 0.35 | Too big: it covers the arm and back |
| 12 | Aura_10 | 0.25 | A fire swirl wrapping the hand. **Best "hand glows" look.** Recommended |
| 13 | Aura_17 | 0.25 | Yellow lightning crackling at the hand |
