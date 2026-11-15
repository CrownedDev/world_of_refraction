# Tools for making our characters' look (UE 5.8)

**Researcher, 2026-11-15.** Crown's question: what can we use to create each character's look, with something like MetaHuman, with sliders, or with presets we can combine? This page builds on `docs/Design/CharacterPipeline.md` (research from 2026-10-28; VRoid route decided). It adds news since then and the tools that page did not cover. It is web research only; check listings and licences before buying. Everything here is a suggestion, and Crown decides.

**What we need (from `DesignPillars.md` and the Drive sheets):** anime-style turnaround sheets with clean lines and flat colours, slim realistic proportions (5'9"–6'1"), gem-coloured long coats, and an authored, Expedition 33–level presentation. Each character needs a distinct face and hair; outfits are separate cosmetics that players mix (`CharacterCustomisation.md`). Everything has to run on SK_Manny so the bought sword-and-shield and magic packs keep working.

## Short answer

1. **VRoid Studio + VRM4U** stays the best fit for the faces, hair and eyes on the sheets. It is free, works with 5.8, and retargets to Manny.
2. **Fab modular anime kits plus Epic's Mutable** for the mix-and-match part (outfits and presets). These kits are built on the Manny skeleton, so the animation packs work directly, and they avoid VRoid's licence grey area for outfits players can mix.
3. **Character Creator 5** is the backup if VRoid bodies fight the retarget. It has the best UE plugin and Manny-ready skeletons, but it looks realistic or Disney-like, not anime.

MetaHuman is strong (Expedition 33 used it) but realistic at heart. To stylise it you need Maya and your own sculpted mesh, which is not the anime look on our sheets. Ready Player Me is gone (shut down 2026-01-31).

## Tool by tool

### MetaHuman (in-engine Creator, 5.8)
- **Stylised/anime fit:** low for anime, good for semi-real painterly looks. The sliders stay inside real human ranges. To go further you use **"From Custom Mesh"** (5.8+): you model the stylised character yourself, MetaHuman fits its topology and rig to it, then you fix the face in **Maya** (Maya plus MetaHuman for Maya are required). Epic warns that heavily stylised anatomy won't match well on import. The docs say nothing about anime eyes or faces. 5.8 adds material overrides for look development.
- **Expedition 33:** Sandfall first tried Character Creator 3, then combined MetaHuman with Mesh Morpher, and still resculpted models. That gives E33's semi-real look, not an anime one.
- **UE 5.8 / Manny:** native. The body uses the UE5 skeleton naming, so our packs play through the standard retarget. Characters are tied to the engine version: each upgrade means re-running assembly.
- **Control:** high (face and body sliders, blend between presets, hair and clothing library).
- **Licence/cost:** free. Since June 2025 MetaHumans may be used and sold outside UE too; studios earning over $1M a year need UE seats. One 2026 blog disputes this, so check Epic's EULA.
- **Solo effort:** low for realistic characters; **high** for stylised ones (Maya, sculpting, rig fixes).
- **Showcase:** [Stylized character tutorial](https://dev.epicgames.com/documentation/metahuman/metahuman-stylized-character-tutorial) · [Daughter of the Inner Stars (stylised MetaHumans)](https://www.unrealengine.com/spotlights/crafting-stylized-digital-humans-on-daughter-of-the-inner-stars-with-metahuman-and-ue5) · [5.8 stylised feedback thread](https://forums.unrealengine.com/t/mesh-to-metahuman-feedback-in-ue5-main-early-ue-5-8-for-stylized-characters/2707537) · [E33 character pipeline (4Gamer, JP)](https://www.4gamer.net/games/807/G080770/20260728023/) · [CGWorld (JP)](https://cgworld.jp/article/202609-coe33-01.html) · [MetaHuman licence change](https://www.cgchannel.com/2025/06/you-can-now-sell-metahumans-or-use-them-in-unity-or-godot/)

### VRoid Studio + VRM4U (already chosen, B-280)
- **Stylised/anime fit:** excellent. Anime faces, gradient eyes and hair strands are what it is made for.
- **UE 5.8 / Manny:** VRM4U (MIT licence) has **official 5.8 support since 2026-06-22**. It auto-builds an IK Rig and Retargeter to Manny. Known problem: **twisted feet** after the auto-retarget, usually fixed in the generated IK Rig. Also expect slight limb-length mismatch, high triangle counts (needs an LOD pass), and spring-bone hair (VRM4U's component, not UE physics).
- **Animation packs:** yes, after the one-time retarget to Manny.
- **Control:** sliders for body and face, preset hair, clothing and textures. Hair is drawn strand by strand. Presets can be saved, and the cast's shared proportion preset is already planned.
- **Licence/cost:** free; commercial use allowed (checked 2026-11-02). Flag: player-side outfit mixing of VRoid meshes is a grey area, so keep shared outfit pieces as our own meshes.
- **Solo effort:** low, a few hours per character to the first draft.
- **Showcase:** [VRM4U](https://github.com/ruyo/VRM4U) · [VRoid → UE 5.6 community tutorial](https://forums.unrealengine.com/t/community-tutorial-from-vroid-avatar-to-unreal-engine-ultimate-vtuber-workflow-tutorial-part-1/2651477) · [twisted feet thread](https://forums.unrealengine.com/t/fix-for-twisted-feet/2721385) · [VRM to Kawaii (CGWorld, 2026-02)](https://cgworld.jp/flashnews/01-202602-VRM-to-Kawaii.html)

### Character Creator 5 / iClone + Auto Setup
- **Stylised/anime fit:** medium. It handles realistic and stylised people; there is a Toon base (Disney-like) and a Caricature MIXER, but **no anime base**, and users are still asking for one. Auto Setup brings realistic shaders, so an anime look needs our own material.
- **UE 5.8 / Manny:** very good. **Auto Setup v2.03 supports UE 5.6–5.8** (needs CC 5.11+). It exports on the UE skeleton with Control Rig and Live Link and sets up shaders automatically.
- **Animation packs:** yes, directly.
- **Control:** very high (morph sliders, ActorMIXER to blend presets, Headshot from a photo or drawing, huge content store).
- **Licence/cost:** about **$299 perpetual** (one review says $498), $499 Deluxe, or $29/month. Commercial game export is included. Some CC components need an **Extended Licence** for mass character production in games, so check each pack.
- **Solo effort:** medium. It is a big tool, but its round trip to UE is the smoothest of all the tools here.
- **Showcase:** [CC5 review (Creative Bloq)](https://www.creativebloq.com/3d/character-creator-5-review-unreal-engine-support-and-auto-rigging-make-it-a-joy-to-use) · [Auto Setup download](https://www.reallusion.com/auto-setup/unreal-engine/download.html) · [CC5 caricatures](https://80.lv/articles/create-animated-hollywood-style-3d-caricatures-without-ai-using-cc5-actormixer) · [pricing](https://www.reallusion.com/plan-and-pricing/individual/perpetual?product=cc)

### Daz Studio + Daz to Unreal
- **Stylised/anime fit:** low to medium. Genesis bodies are realistic; anime add-ons exist but fight the base.
- **UE 5.8 / Manny:** **weak.** The official bridge lists UE 5.1–5.2. For 5.7 users patch it themselves (April 2026 walkthrough), and nothing for 5.8 turned up. There is a community Genesis 9 → UE5 IK retarget.
- **Animation packs:** yes, via retarget.
- **Control:** very high (morphs, huge store).
- **Licence/cost:** the app is free, but **every store item needs an Interactive Licence** for games, often costing extra. Licensing is the real cost.
- **Solo effort:** medium to high (broken bridge plus a licence audit).
- **Showcase:** [Daz to Unreal](https://daz3d.com/unreal-bridge) · [5.7 walkthrough](https://www.daz3d.com/forums/discussion/759611/daz-to-5-7-full-walkthrough-control-rig-physics-asset-and-more) · [G9 retarget](https://www.github.com/DarknessFX/IKRetarget_DazG9_UE5) · [Interactive licence](https://www.daz3d.com/interactive-license-info)

### Ready Player Me / Avaturn
- **Ready Player Me: gone.** Netflix bought it on 2025-12-19, and its public creator, APIs and SDKs shut down on **2026-01-31**. Don't use it. ([shutdown](https://genies.com/blog/ready-player-me-shutdown), [Pocket Gamer](https://www.pocketgamer.biz/netflix-acquires-avatar-creation-platform-ready-player-me))
- **Avaturn:** selfie-to-avatar with a semi-real look, not anime. It has a UE 5.0+ SDK (the native one is beta). Using it in UE is free, but the branded Pro plan is about **$800/month**. It suits metaverse avatars, not authored story characters. ([UE SDK](https://docs.avaturn.me/docs/integration/unreal/native_sdk/), [pricing summary](https://toolradar.com/tools/avaturn/pricing))
- **Fit for us:** none. Our cast is designed by hand, and these tools make players' own faces.

### Fab modular stylised kits with presets
- **Stylised/anime fit:** good for some kits. You buy a base with parts and presets, then recolour it with masks.
- **UE 5.8 / Manny:** most kits are built **on the UE5 Mannequin skeleton**, so the animation packs work with no retarget. Check each listing's engine version.
- **Control:** presets plus mixing parts and recolouring. The quality is fixed by the kit's art.
- **Examples:**
  - [Modular Anime Girl](https://www.fab.com/listings/363f85d6-feef-495c-9b63-eefb359a4f95): outfit swap, 50+ expression morphs, RGB-mask colours, sliders.
  - [Taro Flameheart, stylised anime boy](https://forums.unrealengine.com/t/2558768): UE 5.6 Mannequin skeleton, modular parts.
  - [Shenya2](https://miica37.github.io/docs/characters/shenya2/): pre-combined presets plus modular Blueprint presets.
  - [Modular Japanese Outfit Character](https://www.fab.com/listings/22a01986-5f46-4036-b489-5f4c82988f0a): on Manny.
  - [Xandra Character Creator](https://www.artstation.com/marketplace/p/rrG8z/xandra-character-creator-deluxe-edition): runtime sliders, but not anime.
- **The tool that joins it all up: Mutable** (Epic, free). It builds skeletal meshes, materials and textures from parts and parameters, at runtime or in the editor. Epic calls it production-ready in 5.8 and has updated its sample. It is the "presets we can combine" system for outfits and colour keys, and it would cover the player outfit mixing in `CharacterCustomisation.md`. ([Mutable sample for 5.8](https://www.unrealengine.com/news/mutable-sample-for-runtime-character-customization-now-updated-for-ue-5-8), [docs wiki](https://github.com/anticto/Mutable-Documentation/wiki/))
- **Licence/cost:** usually $20–$100 per kit under the Fab Standard licence; commercial use is fine inside our game.
- **Solo effort:** low to buy and drop in. It is medium if the kit's face style doesn't match the sheets, because then we would only use its body and clothes.

### Blender add-ons (Human Generator, MPFB, MB-Lab, others)
- **Human Generator V3:** realistic people, well rated; no anime and no special UE path (FBX plus a rig add-on). ([listing](https://blendermarket.com/products/humgen3d/ratings))
- **MPFB 2 (MakeHuman):** free, GPL, still maintained (Blender 5.1 fixes). Realistic.
- **MB-Lab:** last release 1.8.0 (2024), and **its output models are AGPL**, which is risky for a closed game. Skip it. ([licence](https://mb-lab-docs.readthedocs.io/en/latest/license.html))
- **Character Creator for Blender** (Superhive): generates rigged characters in realistic, cartoon, **anime** and chibi styles (Blender 4.2+). It has no UE export path, so you would go FBX plus Auto-Rig Pro's Unreal mode. ([listing](https://superhivemarket.com/products/character-creator-rigged-character-generator-for-blender))
- **UE / Manny:** all of these go Blender → FBX, rigged to Manny bone names with Auto-Rig Pro ($50) or RIG2U5.
- **Fit for us:** a support tool (fixing proportions, making the coat), not the main creator.

### Other anime creators that reach UE
- **VRoid** is the only mature free one. Everything else is either a ready-made Fab kit (above) or a VRChat or BOOTH base model under its own licence (many say "commercial use OK, no resale"; check each one).
- Koikatsu, Honey Select and similar have **no game-use licence**, so they are out.

## Comparison at a glance

| Tool | Anime fit | UE 5.8 + Manny | Our anim packs | Control | Cost (commercial) | Solo effort |
|---|---|---|---|---|---|---|
| VRoid + VRM4U | ★★★ | Yes (5.8 official), fix feet | Yes, after retarget | Sliders + presets | Free | Low |
| Fab anime kits + Mutable | ★★–★★★ per kit | Yes, Manny skeleton | Yes, direct | Presets + parts + colours | ~$20–100/kit; Mutable free | Low–Med |
| Character Creator 5 | ★ (toon, not anime) | Yes (Auto Setup 5.6–5.8) | Yes, direct | Highest | ~$299 + packs | Medium |
| MetaHuman | ★ (needs Maya to stylise) | Native | Yes | High (real range) | Free | Low real / High stylised |
| Daz to Unreal | ★ | Bridge stale (≤5.2 official) | Yes, retarget | High | Free + per-item licences | Med–High |
| Blender add-ons | ★–★★ | Via FBX + rig add-on | Yes, if rigged to Manny | Medium | Free–$100 | High |
| Ready Player Me | — | Shut down | — | — | — | — |
| Avaturn | ✗ (photo-real) | UE 5.0+ SDK (beta) | Partly | Low | ~$800/mo Pro | Low |

## Ranked top 3

1. **VRoid Studio + VRM4U (keep it).** It is the only tool whose own look matches the sheets: anime face, gradient eyes, strand hair. It is free with a clear commercial licence, and 5.8 is officially supported. **What people say:** VTuber and indie creators praise how fast it goes from nothing to a usable anime model, and it is the standard for anime avatars. Common complaints are generic "VRoid face" sameness unless you paint custom textures, heavy meshes, and retarget foot and limb fixes in UE. Plan a custom iris and face texture per character so the cast doesn't look like stock VRoid.
2. **Fab modular anime kits + Mutable (for outfits and presets).** This is what "presets we can combine" really means in UE. The parts are on the Manny skeleton, so the sword-and-shield and magic packs play with no retarget, and Mutable mixes body, hair, coat and colour keys into one character. It also keeps player-mixable outfits on our own or Fab meshes, which sidesteps the VRoid outfit licence flag. **What people say:** developers like Mutable's power and Epic's sample but call its node graph a learning curve (it was only marked production-ready in 5.8). Fab kits are praised for saving weeks, but their faces are fixed, so pick kits for their clothes and bodies and keep VRoid for faces.
3. **Character Creator 5 (backup body pipeline).** Use it if VRoid bodies keep fighting the Manny retarget. CC5 has the smoothest UE handoff (Auto Setup 5.8, UE skeleton, Control Rig) and the most sliders. **What people say:** reviews call its UE support and auto-rigging "a joy to use", and Expedition 33's team started with CC3 for its speed. The downsides are cost creep from content packs and Extended Licences, plus a realistic or Disney look that needs our own anime material.

**Not recommended:** MetaHuman for this cast (realistic core; stylising needs Maya and sculpting skills), Daz (5.8 bridge missing, per-item licences), MB-Lab (AGPL output), Ready Player Me (dead), Avaturn (photo avatars).

## Suggested next step
Finish B-280 (Amethyst in VRoid → UE) first and check the feet and limb retarget. In parallel, a small spike could try **one Fab anime kit + Mutable** with the coat as a part, to see whether presets plus colour keys can cover the cast's outfits.
