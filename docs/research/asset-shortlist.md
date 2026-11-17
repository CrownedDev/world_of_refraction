# Asset shortlist: placeholders that prove the pipeline

**Researcher, 2026-11-17 (revised the same day).** Crown's aim: "with bodies and everything else we just need placeholders, to see how it would look, find issues we may come across, and standardise themes, code etc." So for each need this page picks **one good-enough placeholder**, preferring: (1) what the project's packs already hold, (2) a free asset, (3) a cheap one only if nothing free fits. Each pick says what issue it should flush out. Final-art candidates are a short appendix.

How it was checked: the packs were read by folder and file name only (no editor; `.uasset` files can't be read). Web checks were done on 2026-11-17. **Fab pages couldn't be opened from this session**, so prices and licences come from search summaries and need checking on the page. Nothing was bought, downloaded or signed into. It fits D-223 (clean gear set), D-234 / D-241 (Male and Female Nobody; "Body of …" looks) and D-225 (trial pool).

## 1. What the project already has (short)

| Pack | Holds | Used today |
|---|---|---|
| `StylizedWeapons` (Pack 1 + 2) | **Skeletal** painted weapons on their own `SKEL_Weapon` / `SKEL_Weapon_Bow`. 1H and 2H swords, 1H and 2H axes, daggers, hammers, polearms, staves, shields, **bows with their own draw animations** (`A_Bow_Pull_*`), fist weapons. Colour comes as **one material instance per colour** (Bl, Rd, Gn, Pe, Ye, Go …), from an `M_Armor_Master` | The SAS's sword and shield |
| `Imports/_100_Fantasy_Weapons_Basemesh_Pack_V1_` | 113 **untextured** meshes with no name prefix: includes scythes, spears, guns (Hunter Gun), wand | The old axes |
| `CombatMasterAnimBundle` (**and a duplicate in `Imports/`**, ~1.4 GB) | Manny combat animations in `_RM` / `_IP` pairs: SAS, sword, greatsword, axe, twin axe, katana, hand to hand, twin swords; its own **static** weapons (`SM_*`) | The Light Hit montages |
| `Imports/Paragon/ParagonAnimations` | 32 heroes' animations in `<Hero>Manny` folders (retargeted to Manny), including bow, gun, dagger and staff heroes | Nothing |
| `Imports/MageAnimationSet` | Caster animations on the **UE4** mannequin (`UE4_Mannequin_Skeleton`, `SK_Mannequin`) | Probably the field pulse's `AS_Cast_FireBall` (not checked) |
| `Imports/Animations` | Dodge, parry, block, magic, free pack | Dodge, parry and block montages |
| `RPGIcons` (148 spells), `IdleTycoonUpgradeIconsMegaPack` (500, incl. Ruby, Sapphire, Emerald, Amethyst, crystal shard) | Icons in two different painted styles | IdleTycoon: old item icons |
| VFX: `Big_Pack_Magic_VFX`, `Aura_Pack`, `Magic_Circle1`; `SoStylized` environment | Spells, auras; world and weather | Rain |
| Bodies | `SKM_Manny_Base`, `SKM_Quinn` only | Male and Female Nobody |

## 2. One placeholder per need

| Need | Pick (cost) | Why this one | Issues it should flush out |
|---|---|---|---|
| **One extra weapon type with its animations** | **Bow**: `SK_Bow_Generic_*` from StylizedWeapons, its own draw clips `A_Bow_*`, plus `Imports/Paragon/.../SparrowManny` for the body's attacks (**free, in project**) | The hardest common case: a **skeletal** weapon with its own skeleton and animation, two-handed, held in the **left** hand, firing a projectile | Grip on `hand_l` vs `hand_r`; syncing the bow's draw clip with the body's attack; arrow spawn socket; reach and range in a fixed-position fight; Paragon clip timing against our montage notifies; whether skeletal weapons fit the gear code (today's pieces are static) |
| Second check, cheap if needed | **Staff**: `SK_Staff_Fantasy_*` + `MageAnimationSet` (**free, in project**) | Animations on the **UE4** skeleton | The UE4 → UE5 retarget path (IK Retargeter) once, so every UE4-era pack we own is usable |
| **One ring (adapter)** | Engine `BasicShapes/Cylinder` scaled into a band, plus the crystal mesh below as its inset (**free**) | No rigged ring for Manny exists free; most games don't draw rings at all, so the inset gem is what reads | A **ring socket** on `hand_r` (or the wrist) on Manny and Quinn alike; scale at hand size; the element tint driving the inset; whether a ring is visible at the battle camera at all (if not, use a glow on the hand) |
| **One crystal mesh** | [Low Poly Gems & Crystals, Free Sampler](https://www.fab.com/listings/f1c0d0e0-d64f-4236-af36-b3ab9d74875f) (free per search summary; diamond, cluster, heart gem) or [Stylized Crystal Cluster](https://www.fab.com/listings/34429737-bc6e-4184-b29f-2efde853cb33) (free, commercial use per summary) | One mesh can serve as pickup, ring inset and socket icon render | **One mesh, nine elements** through a tint parameter (not nine meshes); emissive for Light and Darkness and Void; size when used as a pickup vs an inset; the S-tier look (an extra glow?) |
| **One apparel piece that swaps on Manny and Quinn** | [Tokusatsu Outfit for Manny and Quinn](https://www.unrealengine.com/marketplace/en-US/product/tokusatsu-outfit-for-manny-quinn-and-metahumans) (stylised; Manny **and** Quinn versions; price not shown, check the Vault). Fallback: the cheapest Manny-plus-Quinn piece on Fab | Has both body variants, which is exactly the problem to test | **Leader Pose** on Manny vs Quinn; whether one piece needs two meshes (one per body); clipping with the body (hide body sections under it?); the **tint parameters** for gem colour; how weapons and rings sit over sleeves |
| **One extra body ("Body of …")** | A free **Paragon** hero from Epic (e.g. Sparrow, matching the bow test; [Epic's release](https://www.pcgamer.com/epic-has-released-dollar12-million-worth-of-paragon-assets-free-of-charge)). Its animations are already in the project | Different proportions from Manny, its own skeleton, real materials | **The body swap with gear on**: do weapon and ring sockets exist on the new skeleton, at the same names? Do Manny animations retarget cleanly? Does worn apparel hide, refit or block the swap? Saving and loading the chosen body. Licence: Epic staff say it's fine in UE games ([forum](https://forums.unrealengine.com/t/can-i-use-the-paragon-characters-for-commercial-purposes/521533/8)), but it can't be named "Paragon": a placeholder only |
| **Icons** | Reuse in-project: `RPGIcons` for skills, IdleTycoon gems for crystals, PONETI for weapon types (**free**); fill the four missing weapon types (Greatsword, Scythe, Gun, Shield) from the same PONETI source if it has them | Enough to see every screen filled | The **icon size and frame** each screen expects; transparent vs boxed backgrounds; how an icon per element per tier is built (one icon plus a tint, or 63 icons?) |

**Order:**
1. Bow: the gear code and sockets.
2. Ring and crystal together: sockets and tint.
3. Apparel on both bodies.
4. Paragon body swap with all of the above worn.
5. Staff via UE4 retarget, whenever.

Icons can go in any time.

## 3. What a house standard needs to pin down

What the packs already do differently, so a rulebook (`docs/Conventions/`, beside `ContentNaming.md`) can set one rule for each:

| Topic | What we have now | The standard should say |
|---|---|---|
| **Weapon mesh kind** | StylizedWeapons: skeletal on `SKEL_Weapon`; CombatMaster: static `SM_*`; basemesh: static, no prefix | Static by default; skeletal only when the weapon moves (bow string, chain). One rule for how the gear code holds both |
| **Grip and pivot** | Unknown per pack (not visible by name; check in the editor) | The pivot is at the grip; the forward axis is the blade or barrel direction; the up axis; the right-hand grip is the default, and left-hand pieces (shield, bow) are named |
| **Sockets** | Not visible by name; the pack demos may use their own | Fixed socket names on Manny and Quinn and on every body: right weapon, left weapon or shield, ring, back sheath, arrow spawn, VFX point. Any new body must carry them all |
| **Scale** | PSX-style packs are UE4-scaled; others unknown | Centimetres; checked against Manny's hand. A weapon import check: length range per type |
| **Colour and tint** | StylizedWeapons: one material instance per colour (Bl, Rd, Gn …); basemesh: grey matte; CombatMaster: one material per weapon | **One master material per kind** (weapon, apparel, crystal) with named parameters: `ElementTint`, `EmissiveStrength`, `TierGlow`. Element colour comes from the element table, never a separate instance per colour |
| **Skeleton** | UE5 Manny (most), UE4 mannequin (MageAnimationSet), Paragon heroes retargeted into `<Hero>Manny` folders | Everything plays on the UE5 Manny skeleton; UE4 packs go through one shared IK Retargeter; Quinn shares the skeleton |
| **Root motion** | CombatMaster ships `_RM` and `_IP` pairs; Paragon mostly in place | In place by default (CLAUDE.md); root motion opt-in per montage. Keep only the `_IP` copy in data |
| **Animation naming** | `Anim_*_RM` (CombatMaster), `A_*` (StylizedWeapons), no prefix (Paragon), `AN_` / `AS_` (Imports) | One prefix per kind per `ContentNaming.md` (new assets only, D-058) |
| **Mesh naming** | `SK_` / `PA_` (StylizedWeapons), `SM_` (CombatMaster), plain names (basemesh) | Per `ContentNaming.md`; plus the type in the name (`Sword_1H`, `Bow`) so a picker can filter by it |
| **Icons** | Three styles (PONETI, RPGIcons, IdleTycoon); several sizes | One style, one size (e.g. 256 px), transparent background, frame drawn by the UI not baked in |
| **Duplicates and leftovers** | `CombatMasterAnimBundle` twice; demo maps in most packs; DynamicPieMenu unused since the quick strip | Packs are imported once, under one folder; demo content stays out of cooked builds |
| **Heavy references** | — | Soft pointers for new visual fields (D-057) |

## Appendix: final-art candidates (later, not now)

Prices and licences unverified (Fab pages not opened).
- **Animations on Manny, in place:**
  - [Spear Combat Animation V1](https://www.fab.com/listings/50dadbed-60d7-4940-a737-f512fb3803bd).
  - [Archery Combat Animation V1](https://fab.com/s/502562f24bc9).
  - [Dagger Combat Animation V1](https://fab.com/s/86257e2bbf88).
  - These look like one series. No scythe or hammer set was found.
- **Weapons for the missing types:** [Ultimate Fantasy Weapon Pack](https://www.fab.com/listings/7fa312d1-86a8-4056-b61f-48f075246e27) (scythes, staves, bows); [Weapon Props Bundle](https://www.fab.com/listings/80e0a2cc-f160-4fca-bb49-98ac0264c7d9) (pistols, muskets).
- **Crystals:** [Stylized Gem Collection](https://www.fab.com/listings/4a62a8ee-71bb-4a85-a035-9fdcddafc42a) (5 gems × 11 colours).
- **Icons, one family:** [skills](https://www.fab.com/listings/be632c89-e1a2-4ed5-aada-ac75bfc45810), [weapons and armour](https://www.fab.com/listings/eb649cd4-11bc-4ca8-8800-03ebbacf1306), [jewelry and gems](https://www.fab.com/listings/cc12e99f-03e7-48db-95af-279bcd39bdb9).
- **Apparel and bodies:** [Stylized LowPoly Characters](https://www.fab.com/listings/2192d706-f2b5-4ca7-90b4-06987ecf4e5a) (13 modular outfits, UE5 skeleton); [Stylized Fighter](https://www.fab.com/listings/1649d221-c00b-46d6-a3cc-1a9e676135ca) (male and female on the UE5 Mannequin). The anime cast route is in `character-look-tools.md`.

Not checked: grips, sockets and scale inside any pack (they need the editor); whether the Tokusatsu outfit is free; whether `AS_Cast_FireBall` comes from MageAnimationSet; Paragon's exact hero-to-weapon match.
