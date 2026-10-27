# Mob clothing for V — options (B-144)

**Job:** B-144 (research only, no editor, nothing bought). **For:** V's black suit on the Manny/Quinn skeleton (`Design/Enemies.md` §1, D-069, D-079), the elite V and element-trim variations, and role outfits/props for the hub NPCs (B-195). The void material itself (violet body, galaxy head and hands, smoke) is B-143.

**How to read it:**
- Prices and versions are from Fab listing pages and Unreal forum posts as found on 2026-10-27. Fab blocks automated page reads, so **open each listing and confirm the price, engine version (5.8) and skeleton** before buying. Prices marked *(check)* weren't visible.
- "Same skeleton" means the clothing is skinned to the UE5 mannequin skeleton, so it plays V's animations with no retarget: it follows the body through a **Leader Pose** component (Epic's quickest modular-character setup).
- Licence: Fab's **Standard License** allows commercial use in a game and modifying the asset. It forbids reselling or giving the asset away on its own. The Personal tier is for buyers under about $100k revenue in the last 12 months; the Professional tier is for those above (forum answer; check the Fab EULA before release).

---

## What V needs

| Need | From the design | What it means for clothing |
|---|---|---|
| Black suit | `Enemies.md` §1 | A jacket + trousers silhouette that reads at combat-camera distance |
| Gold tie with a star | `Enemies.md` §1 | A separate tie piece (or a masked region) that can take its own material |
| Faceless head and hands, galaxy pattern | B-143 | Head and hands stay the body mesh; the suit must **not** cover them |
| 7 ft, 160 kg | `Enemies.md` §1 | V is Manny scaled up (about ×1.18 uniform); the clothing scales with the actor |
| One body for every mob | D-069 | One suit, varied by material instances (element tint) and a few add-on pieces (elite) |
| Hub NPCs are V in role outfits | D-079, B-195 | The same suit plus a role prop or an over-piece (apron, coat) per service |

## Options

| | Option | Cost | Licence | Retarget / setup effort | Look |
|---|---|---|---|---|---|
| **A** | **Material-only suit on V's body (B-143's master material) + a gold tie mesh on a socket** | Free | Ours (Epic's mannequin is free to ship in UE projects) | Low: masks in B-143's material; tie as a small skeletal or static mesh on the `spine_05`/`neck_01` area | Mannequin silhouette with a suit painted on; no lapels or loose cloth |
| **B** | **Buy a rigged suit pack for MetaHuman + Manny/Quinn** (e.g. the Agent Suit modular outfit, the FBI agent packs, Vu.'s formal suit) | About $20–40 per pack *(check)*; Atira Studio's outfits list at $29.99 | Fab Standard | Low: same skeleton, add as Leader Pose child, recolour with a material instance, hide the hidden-under-cloth body parts | A real suit silhouette (jacket, lapels, trousers); tie usually included |
| **C** | **Buy single pieces from a large MetaHuman/Manny collection** (e.g. "Clothing Collection for MetaHuman – Male", 55 assets at $9 each; NDart's Business Vest pack: 2 shirts, 5 vests, 4 trousers) | About $9 per piece, so ~$30–45 for jacket/shirt/trousers/tie *(check)* | Fab Standard | Low, as B; more pieces to line up | Mix-and-match: also covers hub outfits (aprons, coats, vests) |
| **D** | **Custom suit** made in Marvelous Designer (or commissioned), skinned to Manny in-engine with weight transfer (Dataflow Transfer Skin Weights, or Epic's skin-weight action utility) | A Marvelous Designer subscription, or an artist commission *(quote)* | Ours outright | High: pattern, export, weight transfer, clean-up; MD 2025.2+ pairs with UE 5.6+ for Chaos Cloth | Exactly V's concept (`Characters/V_turnaround.png`) |
| **E** | **A rigged character pack with its own suit** (a full "businessman" character), then retarget | $20–60 *(check)* | Fab Standard | Medium–high: if its skeleton differs, IK-retarget and re-skin; brings a face/hands V doesn't want | Off-model; mainly a source of parts |

Skeleton warning (`Design/Resources_Design.md`, hub locomotion notes): the project's combat character is on the **UE5** Manny skeleton; some in-project content is on the UE4 mannequin. Buy only listings that say **UE5 Manny/Quinn** (or MetaHuman + UE5 skeleton), not UE4.

## Variations

- **Element trims (all options):** one material instance per element on the suit's lining/tie/trim mask, the same instances B-143 makes for the body. This is how monster recolours work in Monster Hunter (subspecies) and Pokémon (colour variants): one model, many reads, and players accept it when the colour means something (here: the mob's element).
- **Elite V (mini-boss):** the same suit plus **one add-on piece** (a long coat, gold cufflinks/epaulettes as a mask) and a slightly larger scale. Destiny's "majors" do this: the same enemy with different armour colours and a little extra size, and players read them as the tough one at a glance. B and C packs usually include a coat; the "Coat for MetaHuman" listing is UE5 Manny/Quinn rigged.
- **Hub NPC roles (B-195):** the suit plus a **prop on a hand socket** and at most one over-piece:

| Service | Prop / over-piece (suggestion) |
|---|---|
| Blacksmith | Hammer; leather apron |
| Jeweler | Loupe; tray of rings |
| Stone Smith | Chisel; stone apron |
| Spiritualist | Censer or lantern; a shawl |
| Ability shop | Scroll or book |
| Spell shop | Floating crystal (Niagara) |
| Loadout vendor | Ledger; satchel |

Props are static meshes (Fab's free content or bought props, under the Fab Standard License; Megascans are no longer all free since the move to Fab, so check each) attached by the actor, so no skinning. Over-pieces (aprons, shawls) come from a C-type collection.

## Recommendation

**A now, then B (Crown picks the pack).**

1. **A** ships with B-143 for free: the suit, tie and element trims as masks in the one V master material, the tie as a small attached mesh. Every mob and hub NPC looks like V straight away, and nothing waits on a purchase.
2. Then **B**: one rigged suit pack on the UE5 skeleton, added as a Leader Pose child with B-143's material instances on it. It gives V a real suit silhouette for about the price of one pack, with no retarget. Keep A's body masks under it (only head and hands show).
3. **Elite V and the hub roles** reuse the same suit with one add-on piece each, from the same pack or C-type single pieces, plus socketed props.
4. **D (custom)** only if Crown wants V's exact concept silhouette later; it's the one option that matches the turnaround sheet, at the highest cost.

Why: Expedition 33's Nevrons and Destiny's factions get their variety from a few shared bodies with clear colour and armour reads, not a model per enemy. That's D-069's "one body" rule. A material-first pass then a bought silhouette keeps the cost low and the look improvable without touching gameplay.

**Crown decides:** which pack (B), and whether to commission D later. Buying is Crown's (Claude doesn't buy). The material masks (A) are built in B-143 (CC+UE).

## Sources

- Fab listings (UE5 Manny/Quinn + MetaHuman rigged): [Agent Suit Outfit, modular](https://www.fab.com/listings/afbb1e4a-21c0-4614-8afa-6d0ac9c6c3e8), [Coat for MetaHuman](https://www.fab.com/listings/a95700e5-8a2d-48d6-843f-c6d673e56d65), [Work Jumpsuit](https://www.fab.com/listings/9c9cccb5-03ac-4f29-b0c4-c0568a5c9963), [Casual Jacket, Jeans & Shoes](https://www.fab.com/listings/4bbf0568-997b-4a3d-ae31-8b8f515f81a3), [NDart Business Vest pack](https://www.fab.com/listings/73d1de0b-44ff-43a4-9b02-c585782b49f8) ([forum post](https://forums.unrealengine.com/t/ndart-business-vest-outfit-pack-metahumans-ue5-skeleton/2737940)), [Vu. White Formal Suit](https://forums.unrealengine.com/t/vu-white-formal-suit-outfit-for-metahumans/2770088), [Atira Studio's store](https://www.fab.com/sellers/Atira_Studio).
- Licence: [Fab licence summary](https://www.fab.com/cla); [CG Channel on the Fab Standard License](https://www.cgchannel.com/2024/11/epic-games-to-share-new-free-assets-on-fab-every-two-weeks).
- Setup: [Epic: working with modular characters](https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-modular-characters-in-unreal-engine) (Leader Pose, Copy Pose, Mesh Merge); [Epic: skin-weight transfer action utility](https://dev.epicgames.com/documentation/unreal-engine/create-an-action-utility-for-transferring-skin-weights-in-unreal-engine); [Marvelous Designer to MetaHuman workflow](https://support.marvelousdesigner.com/hc/en-us/articles/52699135975705-Marvelous-Designer-to-MetaHuman-USD-Garment-Integration-Workflow).
- Scaling: [UNAmedia: retargeting large characters](https://docs.unamedia.com/ue5-mixamo/retargeting-large-characters).
