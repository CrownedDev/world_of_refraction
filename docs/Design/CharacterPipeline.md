# Character Pipeline (2D sheets to UE5 characters)

**Status:** research and recommendation (2026-10-28) for Crown's character session; nothing decided but the VRoid route is the suggested default. Fab prices come from search summaries (listing hosts couldn't be fetched): check before buying.

## VRoid Studio MCP (Crown's question, 2026-10-28)

An MCP server lets Claude Code drive VRoid Studio directly: [brainrom/vroid-studio-mcp](https://github.com/brainrom/vroid-studio-mcp) (fork: [vietgs03](https://github.com/vietgs03/vroid-studio-mcp)) hooks VRoid's internal API through a BepInEx/IL2CPP plugin: presets, every slider, colours, camera, save; works minimised. Unofficial; targets **VRoid Studio 2.14.0** (check the version). The older [nhodges/mcp-vroid](https://github.com/nhodges/mcp-vroid) drives the GUI by screenshots and OCR: fragile, not Windows; skip. Limit: VRoid's clothing set (the military coat with high collar and white belt likely needs a custom texture, or one coat mesh made in Blender and shared by the cast).

---


Research date: 2026-10-28. Web-only; no code edits. **Everything here is a suggestion; Crown decides.**
Caveat: this cloud session could not open fab.com, dev.epicgames.com, vroid.com, cgchannel.com or
forums.unrealengine.com directly (not on the session's allowed network list), so Fab prices and some
official doc wording come from search summaries. Check each listing before buying.

## 0. Short answer

- **Form (body + face + hair):** VRoid Studio (free, MIT-licensed importer VRM4U with official UE 5.8
  support) gets a Crown-designed anime character into UE5 fastest. Treat it as the *first draft*; the
  distinctive hair (blue messy, braided bun, blonde bob) and the big gradation eyes come out well,
  the body needs re-proportioning to Manny's bone lengths in Blender before it animates cleanly.
- **Coats:** nothing on Fab is "long military coat with high black collar on Manny". Make one coat
  mesh (Blender, or Marvelous Designer Personal at $39/month for a month), skin it to Manny, and
  drive the three gem colours (purple, orange-red, ochre) by **material-instance parameters**, never
  by separate meshes.
- **Shading:** a per-character anime material (SDF face shadow, ramp, eye highlight layer) with
  **no outline pass**, on top of the painterly world. The MeshBites Stylized Anime Shader ($35) is
  the closest off-the-shelf fit and its outline is a separate, maskable feature.
- **Rig:** retarget VRoid/Blender output *onto* SK_Manny/SK_Quinn once (IK Retargeter + Auto
  Retarget Chains in 5.8), then all animation packs just work. Don't ship the VRM skeleton.
- **Editor:** a `UCharacterFormDefinition` data asset + an Editor Utility Widget preview + Leader
  Pose assembly at runtime; a 4-field replicated struct carries the choice in co-op. Lyra's
  Character Parts component is the reference pattern.
- **Cost per character (solo):** ~12–25 hours in-house after the first two; or ~$300–$1,000
  commissioned (stylised, game-ready, on the UE5 skeleton), with anime/VTuber specialists quoting
  $3,000–$5,000+.

## 1. Character creation tools — comparison

| Tool | Cost | Licence for a shipped game | Anime fit | UE 5.8 fit | Manny/Quinn path | Learning curve |
|---|---|---|---|---|---|---|
| **VRoid Studio** (Pixiv) | Free | Models you make are yours; the export dialog sets VRM usage flags. Pixiv's own "commercial use" help article is the one to confirm (search could not open it). Third-party VRoid assets on BOOTH carry their own terms. | Excellent: anime faces, gradation eyes, hair strands | Via **VRM4U** (MIT). Its changelog lists UE 5.8 preview support 2026-05-14 and official support 2026-06-22; the README's "5.0–5.4" line is stale. | VRM4U auto-generates IK Rig + IK Retargeter and can target the UE5 Epic skeleton on import. Proportions still need fixing (see §3). | Low (hours) |
| **Reallusion Character Creator 5** | $299 perpetual Standard, ~$479–499 Deluxe (ActorMIXER PRO etc.), or $29/month. Auto Setup for Unreal plugin is free (v2.0 covers 5.5–5.7; one report says a 5.7.2 import was not recognised until updating). | Perpetual licence with commercial export rights; CC content packs (hair, clothes) are licensed separately per pack. | Medium: CC4 "Toon Base" is Disney-stylised, not anime; users are still requesting anime/manga bases on Reallusion's tracker. Headshot 3 does stylised heads from a picture. | Good (official plugin, PBR materials, LODs, ARKit-style face blendshapes out of the box) | Exports to the UE4/UE5 skeleton directly ("no further retargeting"). | Medium; big tool |
| **MetaHuman Creator (5.6+, in-engine)** | Free with UE | Epic licence; MetaHumans may only ship inside UE projects | Poor: scan-based parametric faces and bodies, built for realism. Not a fit for the sheets. | Native | Native (same core hierarchy as Manny) | Low, but wrong look |
| **Daz Studio + Daz to Unreal bridge** | App free; each store asset needs an **Interactive Licence** (roughly $10–$50 extra per product, creator-dependent; not every product offers one) | Per-asset; audit every item | Poor-medium: realistic Genesis bodies; anime add-ons exist but fight the base | Bridge works; heavy meshes | Retarget needed | Medium; licensing is the real cost |
| **MakeHuman / MPFB 2 / MB-Lab** | Free (GPL/AGPL code; MPFB 2.0.17 July 2026 is the maintained one) | Exported meshes are yours (check MakeHuman asset licences) | Poor: realistic proportions, no anime face | Blender → FBX | Rigify via UE-to-Rigify / RIG2U5 | Medium |
| **Blender hand-model + Rigify → Manny** | Free (+ add-ons: Auto-Rig Pro $50 with "Unreal Mannequin" export mode; RIG2U5 for Blender 5.0/UE 5.6–5.7; Unreal Rigging Pro $20, Blender 5.2; free community UE-to-Rigify, GPL) | Yours | As good as Crown's modelling | Direct FBX / Interchange | Auto-Rig Pro's mannequin mode writes Manny bone names and axes; still verify in 5.8 | High (modelling + topology + weights) |

Sources: VRM4U https://github.com/ruyo/VRM4U · CC5 launch (80.lv) https://80.lv/articles/reallusion-changes-licensing-models-for-several-products/ ·
Reallusion store https://reallusion.com/store · Auto Setup download https://www.reallusion.com/auto-setup/unreal-engine/download.html ·
Auto Setup 5.7.2 report https://cdn-www.reallusion.com/FeedBackTracker/Issue/Auto-Setup-for-Unreal-Engine-not-working-properly-in-Unreal-Engine-5-7-2 ·
CC Toon Base https://80.lv/articles/former-disney-character-artist-adopts-reallusion-s-new-character-creator-toon-base/ ·
anime request on Reallusion tracker https://cdn-www.reallusion.com/FeedBackTracker/Issue/Anime-Manga-Toon-content ·
MetaHuman 5.6 https://80.lv/articles/explore-new-metahuman-features-in-unreal-engine-5-6 ·
Daz interactive licence https://www.daz3d.com/interactive-license-info · MakeHuman status https://static.makehumancommunity.org/makehuman/faq/current_status.html ·
MPFB https://extensions.blender.org/add-ons/mpfb/versions/ · Auto-Rig Pro https://superhivemarket.com/products/auto-rig-pro ·
RIG2U5 https://superhivemarket.com/products/rig2u5 · Unreal Rigging Pro https://superhivemarket.com/products/rig-to-unreal-engine ·
UE to Rigify revival https://cgdivetest.super.site/tools/ue2rigify · VRoid terms lead https://steamcommunity.com/app/1486350/discussions/0/5251727781283301470

**Which best matches anime sheets at indie cost:** VRoid Studio for face/hair/eyes (the parts that make the
sheets recognisable), with Blender for the body re-proportion and the coat. CC5 is the better *engine
citizen* (face blendshapes, LODs, plugin) but its bases don't look like the sheets and the $299 + content
packs buys realism Crown doesn't want. MetaHuman is ruled out by look and licence. A hybrid some teams use:
CC5 body (good topology, Manny-compatible) under a VRoid head — more work than it saves for 25 characters.

Known VRoid caveats (from the UE/VRM community, verify in PIE): VRoid bodies are ~7-head anime
proportions and the Manny retarget produces slight arm/leg length mismatch unless proportions are matched
first; VRoid hair uses VRM spring bones (VRM4U has its own spring-bone component, not UE physics); tri counts
are high (a default VRoid export is often 60–100k tris with hair) so a decimation/LOD pass is required.

## 2. Fab / marketplace packs (anime-leaning, Manny/Quinn)

Prices were not readable from this session; check the listing.

- **Modular Character – UE5 Rigged Japanese Outfit Character** — modular anime female base on the
  UE5 Manny skeleton, several outfit variations, physics assets.
  https://www.fab.com/listings/22a01986-5f46-4036-b489-5f4c82988f0a (base-mesh reference only; outfits are not coats)
- **Modular Western Character (Rigged) | UE5 Mannequin + MetaHuman Head** — not anime, but a clean
  example of a 14-part modular Manny-compatible kit to copy the structure of.
  https://www.fab.com/listings/84ff7fda-68ba-48bc-b1c1-6ea3fbe31c24
- **Stylized Anime Hair Collection Pack (60+ styles)** — many formats incl. UE; listing does not state
  hair-card construction or Manny skinning (head-attached static hair is fine for turn-based).
  https://www.fab.com/listings/ff3ef1de-64af-4cdf-bd07-61a1fe8d196c
- **N5: Hair Card Bundle Vol.1** — real hair cards, Manny-skinned, but realistic look (needs an anime
  material swap). https://www.fab.com/listings/1ec1127f-4c6b-4cbf-8f13-747fd917217a
- **Special Forces Modular Soldier** — 59 skeletal parts, retargets to SK_Manny/SK_Quinn; useful as
  a modular-clothing *structure* reference, not for the coats.
  https://www.fab.com/listings/24ba78bb-f13c-477e-8ef0-03f644d433b5
- **Tactical Military Outfit for MetaHuman / Manny / Quinn** (UE 5.3+, tested 5.7, Chaos Cloth) and
  **Clothing Collection for MetaHuman – Male** (military field jacket, per-asset pricing) — nearest
  "uniform on Manny" items; no long coat with high collar found anywhere on Fab.
  https://www.fab.com/listings/3f8ae36b-4670-4356-999c-dd2b850e4080 · https://www.fab.com/listings/252b167f-154f-401e-9dd0-5ffd4ba8e475
- **Shaders:**
  - **MeshBites Stylized Anime Shader for UE5** (~$35, UE 5.4+): SDF face shadows, painted shadows,
    shadow bands, custom moving eye/hair highlights, MatCap, anisotropic hair, "World Penumbra" to
    soften world lighting; outline/inline is a separately masked feature (turn it off). Same author
    sells a Face SDF Generator. https://forums.unrealengine.com/t/meshbites-stylized-anime-shader-for-ue5/2763050 ·
    https://www.fab.com/listings/42bcb5e2-7491-44a3-905d-ff8aae2b9852 · https://dincairwen.itch.io
  - **Stylized Character Toon Shader (No Post-Process)** — per-character cel + angel-ring hair + two
    specular modes; outline is a listed feature (disable). https://www.fab.com/listings/dd792978-b257-407b-9416-43d7f421ad82
  - **Celes Anime Shader** — unlit-model cel shader with outline; older. https://www.fab.com/migrated-listing/576cbdc307b040c38791ae140e80d8b6
  - Avoid whole-scene post-process toon shaders (e.g. Ultra Hybrid Toon Shader) — they would cel-shade
    the painterly world. https://www.fab.com/listings/06aef8cc-759f-4620-998b-08a9bd5fc208
  - Background on the SDF face-shadow technique: https://destiny-fine-0ac.notion.site/SDF-Shadowmap-9527f23116aa42dc89e3c9e17a54ddf3 ·
    Wuthering Waves shading teardown in UE5: https://forums.unrealengine.com/t/deconstructing-replicating-wuthering-waves-post-process-anime-shader-model-in-ue5/2739985
- **Expressions:** no anime pack with ARKit-52 found. Realistic "Female Base Body 1/2 (UE5)" packs ship
  52 ARKit blendshapes on Manny (https://www.fab.com/listings/64d4f278-6711-42eb-9923-ee7d874e9831), and
  **FaceRigTools** (UE 5.7) builds a face Control Rig from existing ARKit morphs
  (https://www.fab.com/listings/8785f08f-03e2-4620-af09-eb6246b6d247). VRoid exports VRM 1.0 expression
  presets (happy/angry/sad/relaxed/surprised + visemes + blink) as morph targets, which VRM4U imports —
  that already covers an "expression sheet" for a turn-based game.

## 3. Recommended pipeline (per character)

1. **Sheet → VRoid Studio (1–3 h).** Build face, eyes (iris texture: paint the gradation + highlight
   from the colour key as a custom iris texture), hair (strand groups per style). Export VRM 1.0 with
   a plain body (no VRoid clothes).
2. **VRM → Blender (2–4 h).** Import (VRM add-on). Re-proportion the body to Manny's bone lengths
   (5'9"–6'1" → scale legs/arms to match SK_Manny's limb ratios so retargeting stops stretching),
   decimate body under the coat, bake hair to a lower tri count, keep expression shape keys.
   Rig with Auto-Rig Pro "Unreal Mannequin" mode or RIG2U5 so bone names are Manny's.
3. **Coat (once, then variants, 6–10 h for the first).** Model the long military coat + high collar +
   belt as *one* skeletal part skinned to Manny (Marvelous Designer Personal $39/month for the pattern
   if Crown prefers simulation; otherwise Blender). Vertex-colour or mask the belt and collar; the gem
   colour is a `VectorParameter` on a material instance per character. Three colours = three MIs, one
   mesh. Chaos Cloth on the coat tail is optional and costs frame time; a skirt bone chain + AnimDynamics
   is cheaper for a fixed-camera turn-based fight.
4. **Import to UE 5.8 (1 h).** FBX/Interchange → skeletal mesh on **SK_Manny/SK_Quinn skeleton**
   (choose the existing skeleton at import; if bone names match, no retarget asset is needed). If VRM4U
   is used directly instead, let it generate the IK Rig + Retargeter to the Epic skeleton, then retarget
   the *mesh* once ("Retarget Animations"/Auto Retarget Chains is in the IK Rig editor) and discard the VRM
   skeleton. Note 5.8 forum reports: IKRetargetIKChainsOp was split into three ops and ARKit facial curves
   can blow up on retarget-export — keep faces as morph targets on the mesh, not curves through a retarget.
   https://forums.unrealengine.com/t/ue-5-8-gasp-ikchain-retargeting-seems-broken/2732399 ·
   https://forums.unrealengine.com/t/arkit-facial-curves-break-when-retargeting-in-ue-5-8/2737961 ·
   https://dev.epicgames.com/documentation/en-us/unreal-engine/auto-retargeting-in-unreal-engine
5. **Face/expressions (1–2 h).** Keep VRoid's VRM expression morphs; add a pose asset per expression
   sheet entry (e.g. `PA_<Name>_Expressions`) driven by an AnimGraph "Blend Poses by Enum" or a curve
   so the combat UI can call `SetExpression(EExpression::Smug)`. ARKit-52 is only needed for live mocap.
6. **Material (2–3 h first, 30 min after).** One master `M_CharacterAnime`: base colour + ramp, SDF
   face shadow texture, no outline, rim light, eye layer (iris gradation + separate highlight plane or
   parallax), hair "angel ring" highlight; a world-lighting penumbra parameter so faces sit in the
   painterly scene. Instances per character carry the colour keys.
7. **Conventions:** `SK_<Name>_Body`, `SK_<Name>_Hair`, `SK_<Name>_Coat`, `MI_<Name>_Skin/Hair/Coat`,
   `DA_Character_<Name>` — per `docs/Conventions/ContentNaming.md`.

**Hours per character (solo, after the first two are done):** ~12–25 h (VRoid 2, Blender 5–8, coat
variant 1–2, import/retarget 1–2, expressions 1–2, materials 1, LODs/physics 1–2). First two characters:
double that while the master material, coat and retarget assets are built.

**What to outsource:** the coat base mesh and the Blender re-proportion/weighting template (one-off,
reused 25×); optionally hair for the three hardest styles. Observed rates (2025–26): Fiverr stylised
game characters $200–$400 per character, 2-week delivery; low-poly rigged from €250; mid studio-level
per-character from $1,000; anime/VTuber-grade VRM specialists $3,000–$5,000+ (hair extra, $200+);
hourly $15–40. https://fiverr.com/resources/guides/costs/3d-sculpting-artist ·
https://fiverr.com/parswa2770/create-cute-3d-stylized-characters-for-games-and-animation ·
https://astrexis.carrd.co/ · https://prixkilltee.carrd.co/ · https://polycount.com/discussion/comment/2501403
Brief for a commission: "game-ready, SK_Manny bone names/orientation, ≤40k tris body+coat LOD0,
masked hair cards, VRM-style expression morphs, source .blend".

## 4. Dev-side character editor in UE5 (no player creator)

**Data (matches CLAUDE.md architecture table):**
- `UCharacterFormDefinition : UPrimaryDataAsset` — `TSoftObjectPtr<USkeletalMesh>` Body, Hair, Coat,
  Extras[]; `TSoftObjectPtr<UMaterialInterface>` per slot or a `FCharacterColourKeys {GemColour, HairTint,
  EyeIris}` struct that the editor bakes into MIs; `UAnimBlueprint` class; expression `UPoseAsset`;
  loadout defaults (existing `UCharacterTemplate`-style ids); lore/name `FText`.
- Existing `CharacterDataSystem` (docs/Architecture/CharacterDataSystem.md) likely already owns the
  template; the form definition should be a field on it, not a parallel asset.

**Preview/assembly window:** an **Editor Utility Widget** (`UEditorUtilityWidget`, Blueprint, docked tab)
with an asset picker for the form definition, a viewport (a `UEditorUtilityWidget` can host a
`PreviewScene` via a C++ `SViewport` child, or simply spawn a preview actor in a `L_CharacterPreview`
level and use the level viewport), slot dropdowns, colour pickers that write to the MIs, and "Assemble",
"Save MIs", "Print form" buttons. Docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/editor-utility-widgets-in-unreal-engine ·
Python API https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/EditorUtilityWidget.html
Pair it with `UFUNCTION(CallInEditor)` Print/Validate buttons on the data asset (CLAUDE.md debug rule).

**Runtime assembly (Epic's "Working with Modular Characters" page):**
https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-modular-characters-in-unreal-engine
- **Leader Pose** (`SetLeaderPoseComponent`, renamed from Master in 5.1): fastest setup; children follow
  the body's bone buffer, no per-part physics/morphs on children, extra render-thread cost per component.
  Fine for ≤4 parts × ≤6 characters on screen in a turn-based fight. **Suggested default.**
- **Copy Pose From Mesh**: AnimGraph node; use when a part needs its own anim (coat tail dynamics).
- **Skeletal Mesh Merge** (`USkeletalMergingLibrary::MergeMeshes`, SkeletalMerging plugin,
  `FSkeletalMeshMergeParams`): one draw-set, cheapest at runtime, but the built-in merge **drops morph
  targets** (expression morphs on the head would be lost) and a merge at spawn can hitch a frame. Only
  worth it if profiling shows Leader Pose is the bottleneck; third-party "SKMerger" / "Skeletal Merge"
  plugins claim morph support. https://www.fab.com/listings/42c1549d-2c58-4c33-9027-1de00c184d9e ·
  https://forums.unrealengine.com/t/stemulation-studio-skmerger-runtime-skeletal-mesh-merging-caching-system/2737677
- All parts must share the one `SK_Manny` skeleton asset.

**Lyra pattern (from the Lyra sample source, `LyraGame/Cosmetics/`; verify against the 5.8 sample):**
`ULyraControllerComponent_CharacterParts` holds the *choice* on the controller and re-applies it to each
new pawn; `ULyraPawnComponent_CharacterParts` holds `FLyraCharacterPartList` (an `FFastArraySerializer` of
`FLyraCharacterPart {PartClass, SocketName, CollisionMode}`) that is replicated and, on each client, spawns
each part as a child actor attached to the pawn mesh with Leader Pose; a gameplay-tag set (`Cosmetic.*`)
picks the animation layer/body style via `FLyraAnimBodyStyleSelectionSet`. `B_CharacterPawnCosmetics`
is just that pawn component placed on the pawn Blueprint. Lyra docs: https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine
For this game: replace "PartClass spawned actor" with "slot index into the form definition" — simpler.

## 5. Performance and co-op

- **Budget for 60 fps / 1080p (suggestion):** ≤40k tris LOD0 per party member (body 12k, coat 15k, hair
  10k), 3 LODs (auto-generated with the Skeletal Mesh Reduction tool at 50/25/10 %), ≤120 bones per
  character incl. a few hair/coat dynamic chains, ≤8 material sections per character. UE's default max
  bones per section is 65536 (mobile cap 75; set per platform under Project Settings → Rendering →
  Skinning). https://dev.epicgames.com/documentation/en-us/unreal-engine/skeletal-mesh-rendering-paths-in-unreal-engine
- **Hair cards:** author as **Masked** (alpha test) with a dither/TAA-softened edge, not Translucent;
  translucent overdraw on stacked cards is the classic anime-hair frame killer. Keep a few translucent
  wisp cards at most. Trim card planes to the painted alpha. https://polycount.com/discussion/comment/2747058
- **Eyes:** one extra small mesh/plane for the highlight with a masked material, or a parallax layer in
  the eye material; avoid a translucent cornea at this art style.
- **Coat:** cloth sim off by default; bone chain + AnimDynamics. Chaos Cloth only on the local player's
  active character if ever.
- **Expressions:** morph targets are cheap (GPU morphs); keep ≤30 per head; skip ARKit-52.
- **Co-op replication:** a `USTRUCT FCharacterFormChoice { FPrimaryAssetId Form; uint8 CoatVariant;
  uint8 HairVariant; uint8 ExpressionPreset; }` replicated on the PlayerState (per-fight state rule
  D-064) with `OnRep_Form` doing the Leader Pose assembly on clients; parts are chosen by index into
  the data asset so nothing heavy crosses the wire. Soft-load the parts async on the client before
  swapping meshes (Ready Player Me's Lyra notes warn that swapping the skeletal mesh at runtime without
  re-initialising the anim instance breaks the component). https://docs.readyplayer.me/ready-player-me/integration-guides/unreal-engine/code-samples/lyra-project-integration-example

## 6. Open questions for Crown

1. VRoid + Blender first-draft route, or commission the first two heroes to set the bar?
2. One coat mesh with colour-key MIs (recommended) vs per-character coat meshes?
3. Buy MeshBites ($35) as the anime material base, or build the SDF/ramp master material in-house?
4. Leader Pose by default (simplest) — agree to defer mesh merging until profiled?
5. Confirm VRoid's current commercial-use article (vroid.com/en/studio/guidelines) before relying on it.
