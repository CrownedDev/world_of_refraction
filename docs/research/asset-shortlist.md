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
| **One extra body ("Body of …")** | A free **Paragon** hero from Epic (e.g. Sparrow, matching the bow test; [Epic's release](https://www.pcgamer.com/epic-has-released-dollar12-million-worth-of-paragon-assets-free-of-charge)). Its animations are already in the project | Different proportions from Manny, its own skeleton, real materials | **The body swap with gear on**: do weapon and ring sockets exist on the new skeleton, at the same names? Do Manny animations retarget cleanly? Does worn apparel hide, refit or block the swap? Saving and loading the chosen body. Licence: free to use in a shipped Unreal game, but not under the Paragon name, and not outside Unreal (section 4, corrected) |
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

## 4. Paragon: what is on disk, what it covers, the licence (added 2026-11-17)

**Searched:** every folder under `Content/` (tracked and git-ignored), plus `D:\github`, `D:\Epic Games` and the launcher's vault cache, for "Paragon" or a hero's name.

**Result: only animations. No Paragon character is on disk.** The single Paragon folder is `Content/Imports/Paragon/ParagonAnimations/`, with one `<Hero>Manny` folder per hero (so already retargeted to Manny). It holds no skeletal mesh, skeleton, material, texture or skin. The other "Manny" folders found are the mannequin demos inside other packs.

### 4.1 Hero by hero

Weapon is from memory of Paragon plus the clip names (sure: **high** / **medium** / **low**). "Editor" means only opening the clips will confirm it.

| Hero | Animation files | Clip hints | Weapon (sure) |
|---|---|---|---|
| Aurora | 189 | Ability, Attacks | Ice melee (low; editor) |
| Belica | 143 | Aiming, Attack | Hand cannon / gun (medium) |
| Countess | 230 | Attack, TravelMode | Twin blades, dagger-like (medium) |
| Crunch | 144 | Combo_01…, Attack_Air | Big fists → gauntlets (high) |
| Dekker | 162 | Targeting, Attack | Staff-like device (low) |
| Drongo | 169 | Primary_Fire | Gun (high) |
| Feng Mao | 196 | Dash_Attack, Attack_Stack | Sword (high) |
| Fey | 173 | Ability_E | Caster (low) |
| Gadget | 102 | LMB_Fire, Reload | Gun (high) |
| Gideon | 199 | Primary_Attack_A/B/C | Caster, bare hands (medium) |
| Greystone | 154 | Jump_Melee | Greatsword (high) |
| Grux | 108 | PrimaryAttack_FourStrikes | Two maces → hammer (high) |
| Kallari | 188 | Evade, Block, MarkedForDeath | Twin daggers (high) |
| Khaimera | 146 | Melee_A… | Claw blades → gauntlets (medium) |
| Kwang | 180 | SwordInWorld, Catch | Greatsword (high) |
| Minions | 85 | Attack_A/B | Generic melee (low) |
| Morigesh | 157 | Ability_E, Targeting | Caster (low) |
| Murdock | 193 | Fire_Reload, Moving_Fire | Shotgun → gun (high) |
| Muriel | 166 | Primary_Fire | Caster (medium) |
| Narbash | 172 | In_Air_Melee | Mace (and drum) → hammer (medium) |
| Phase | 161 | Ability_E | Caster (low) |
| Revenant | 211 | Fire_Fast_Var_A–C | Revolver → gun (high) |
| Riktor | 157 | Ability_Hook | Chain hook (low) |
| Serath | 177 | In_Air_Melee, Wing_Flap | Polearm / scythe-like blade (low; editor) |
| Sparrow | 150 | RMB_Fire, Fast_Fire | Bow (high) |
| Steel | 186 | Bash, Block_Idle | Shield (high) |
| Terra | 135 | RMB_melee, Q_swing | Axe and shield (high) |
| TwinBlast | 254 | DoubleShot_Fire_Lft/Rt | Twin pistols → gun (high) |
| Wraith | 229 | Fire, Ability_E | Sniper rifle → gun (high) |
| Wukong | 172 | Primary_Melee_A… | Staff (high) |
| Yin | 163 | Whip | Sword and whip (low; editor) |
| Zinx | 119 | Ability_DoT | Caster (low) |

### 4.2 The twelve weapon types

| Type | Best heroes | Sure | Note |
|---|---|---|---|
| Sword | Feng Mao | High | CombatMaster already covers it |
| Greatsword | Greystone, Kwang | High | CombatMaster too |
| Spear | Wukong (staff clips as a spear), Serath | Low | **Editor**: no true spear hero |
| Staff | Wukong | High | Also MageAnimationSet for casting |
| Dagger | Kallari, Countess | High / medium | |
| Axe | Terra | High | CombatMaster too |
| Hammer | Grux, Narbash | High / medium | Maces, one-handed; **no two-handed hammer** |
| Bow | Sparrow | High | The bow placeholder in section 2 |
| Gauntlets | Crunch, Khaimera | High / medium | CombatMaster hand to hand too |
| Scythe | Serath | Low | **Editor**; likely a gap |
| Gun | Revenant, Murdock (one gun); TwinBlast (two) | High | |
| Shield | Steel, Terra | High | |

The caster heroes (Gideon, Muriel, Fey, Zinx) are useful for spell casts. Paragon clips are action-game moves (many "MSA" or additive variants, jogs, aim offsets), so expect to cut single attack clips into our montages and drop the locomotion ones.

### 4.3 Bodies ("Body of …", D-241)

**None on disk.** To try one, the free Epic packs to add are named "Paragon: <Hero>" on Fab. 19 heroes were released free in 2018, among them Sparrow, Kwang, Greystone, Kallari, Murdock, Revenant, TwinBlast, Wraith, Grux, Countess, Steel, Terra, Feng Mao, Gideon, Muriel and Aurora ([80.lv, final round](https://80.lv/articles/final-round-of-free-paragon-assets-valued-at-over-5-million); a forum note says some were briefly missing from Fab, then restored: [thread](https://forums.unrealengine.com/t/missing-paragon-characters-on-fab/2117593)). The listing says each pack holds the model, animations, Animation Blueprints, skins and FX. Nothing was added.

- **Skeleton:** each Paragon hero ships on **its own skeleton**, not UE5 Manny. That is why our animations sit in `<Hero>Manny` copies. The work to use one as a body:
  1. Retarget the hero mesh onto Manny's skeleton, or set up an IK Retargeter both ways.
  2. Add our standard sockets (weapon, shield, ring) to it.
  3. Check that Manny-based apparel can't be worn on it (hide apparel on that body, or make a version).
  4. Check its proportions against our attack spacing.
- **Colour settings:** Paragon materials have per-skin colour and emissive settings, but whether any can take our element tint needs the **editor**.
- **Style:** Paragon heroes are realistic and painterly, not anime. They fit a placeholder or an unlockable "other world" look better than the main cast (`character-look-tools.md`).

### 4.4 The licence

- **Source pages could not be opened** (2026-11-17: Epic's Paragon download page and the Sparrow listing both returned "forbidden"; Fab's licence page shows only a summary without an Epic-content clause). So this rests on the most reliable secondary sources.
- **Allowed:** using the assets in your own commercial game built in Unreal, with no charge beyond Unreal's normal terms. Epic's release said "no strings attached" (quoted from Epic's page in [this forum thread](https://forums.unrealengine.com/t/can-i-use-the-paragon-characters-for-commercial-purposes/521533), 2022; [PC Gamer, 2018](https://www.pcgamer.com/epic-has-released-dollar12-million-worth-of-paragon-assets-free-of-charge)).
- **Not allowed:**
  - **Use outside Unreal.** The Sparrow listing reads "licensed for use only with UE based products" (from the [listing](https://docs.unrealengine.com/marketplace/ja/product/paragon-sparrow) via a search summary).
  - **The Paragon name and logo.**
  - **Making a Paragon sequel, remake or re-creation without Epic's written permission** (Epic's content terms as quoted in the same forum thread).
- **Unclear:** one forum user saw "UE4 based products" on an older listing and asked Epic about UE5. The listing now names UE 5.0–5.3. No formal Epic answer was found.
- **Correction:** section 2 said a Paragon hero "can only ever be a placeholder". **That was too strict.** On these sources a Paragon body may ship as a look in an Unreal game, renamed (no "Paragon", and safer not to use the hero's own name) and not presented as Paragon. Whether it should is a style question (it is not anime), and the licence wording should be read on the Fab page before shipping.

## 5. Two weapons, and sword and shield, as templates (D-242)

### 5.1 The two-weapon and shield sets we own

Counts are from file names. "Kept" leaves out Paragon's `_MSA` and additive copies. Root motion (RM) or in place (IP) is from the `_RM` / `_IP` suffix. **Editor:** whether a clip really fits its label.

| Set | Pairing it covers | Files (kept) | Idle | Basic attacks | Heavier / skill | Block | Hit | Death | Note |
|---|---|---|---|---|---|---|---|---|---|
| CombatMaster **DSAS V1** | Sword and shield | 150 (147, IP) | 3 | 6 combos + slashes, kick (~98) | (combo enders) | 1 | 0 | 0 | The Light Hit's source |
| CombatMaster **DSAS V2** | Sword and shield | 146 (IP) | 2 | ~48 | — | **~44** (block moves and turns) | 16 | 0 | The best guard set |
| CombatMaster DSAS V3 | Sword and shield | 76 (**RM only**) | — | 3 combos | — | — | — | — | Root motion: needs an in-place pass |
| CombatMaster Handsome SAS | Sword and shield | 25 | 1 | 13 | 2 | 0 | 4 | 1 | Small, complete |
| CombatMaster **Handsome Twin Sword** | Two swords | 51 | 2 | 20 slashes, 6 stabs, combo | (jump attacks) | 2 | 6 | 0 | Real two-sword set |
| CombatMaster Dynamic Twin Axe | Two axes | 58 (**RM only**) | 4 | 4 combos | — | 8 (start, loop, hit, end) | 8 | 0 | Needs an in-place pass |
| CombatMaster Dynamic Hand to Hand (V1 RM, V2) | Gauntlets | 50 + 64 | 2 | 5 combos + common attacks | — | — | — | — | Only V2 has some in-place clips |
| Paragon **Kallari** | Two daggers | 188 (172) | 22 | 12 | 7 | 3 (block, evade) | 7 | 6 | The most complete dual set |
| Paragon **Countess** | Two blades (daggers or swords) | 230 (189) | 20 | 21 | 15 | 0 | 6 | 1 | |
| Paragon **TwinBlast** | Two guns | 254 (225) | 23 | 15 (left and right shots) | 22 | 0 | 7 | 2 | |
| Paragon **Grux** | Two maces (hammers) | 108 (104) | 18 | 12 (four-strike combo) | 1 | 0 | 8 | 2 | |
| Paragon **Khaimera** | Claw blades (gauntlets) | 146 (130) | 26 | 13 | 6 | 0 | 6 | 2 | |
| Paragon **Crunch** | Fists (gauntlets) | 144 (128) | 25 | 16 | 24 | 0 | 7 | 1 | |
| Paragon **Steel** | Shield (with a fist) | 186 (162) | 11 | 24 (bash) | 27 | 3 (block idle) | 11 | 2 | Shield-first: few weapon swings |
| Paragon **Terra** | Axe and shield | 135 (107) | 19 | 11 | 1 | 0 | 7 | 1 | Weapon-and-shield in one hero |
| Paragon Yin | Sword and whip (**editor**) | 163 (134) | 21 | 17 | 17 | 0 | 7 | 2 | A possible mixed pair |

**Gaps:**
- No set for a **mixed pair** (sword and axe).
- **Two axes and two-sword skill moves** exist only as root motion or a small set.
- Death clips are missing from most CombatMaster sets; a generic death would serve all.
- StylizedWeapons' own clips are only the bow string's draw. No other set on disk is a pair.

### 5.2 How the game treats a pair today

- **One weapon for slots.** A pair is one entry; the main hand's sockets serve it; a weapon and a shield count as one weapon and keep the main hand's slots (CL-23b, D-211, D-224 Q8).
- **Same type only.** Each weapon has an `EWeaponHandling` (single only / dual only / either) and **two stance entries** (alone, paired); the pair uses the **first-chosen** weapon's paired stance (D-209). Wield mode is visual only (CL-23).
- **The attack** in the field is one swing per weapon type (D-215); in battle the skills belong to the loadout (D-224).

**What a pair template needs beside a single-weapon one:**
1. **Which hand each piece sits in:** a main-hand socket and an off-hand socket, and for a shield a forearm socket (not the hand).
2. **A pair stance:** idle and guard poses that hold both pieces.
3. **Pair attack montages** that move both hands, with hit and trail notifies on **both** pieces, or only the main hand's.
4. **Block clips:** a shield's guard differs from a two-blade parry.
5. **A rule for what the off hand does in a skill:** carried, or part of the swing.

**Two options for Crown:**
- **(a) Every pair uses the main hand's single template; the off-hand piece is just carried.** No new animation work; it looks wrong for pairs (an idle off hand), but works for every pairing at once.
- **(b) Named pair templates where a real set exists, else (a).** Named:
  - **Sword and shield:** DSAS V1 and V2; Terra for an axe and shield.
  - **Two swords:** Handsome Twin Sword.
  - **Two daggers:** Kallari.
  - **Two guns:** TwinBlast.
  - **Two maces or hammers:** Grux.
  - **Gauntlets:** Crunch, or CombatMaster hand to hand.

  **Falls back to (a):**
  - Two axes, until the twin-axe set gets an in-place pass.
  - Mixed pairs, which are not allowed yet anyway (D-209).
  - Two spears, staves, scythes or bows (two-handed types, which shouldn't pair).

**The evidence favours (b).** It matches D-209's stance-per-pairing idea and the games below, and with (a) as the fallback nothing is ever blocked.

### 5.3 How other games do it

- **Monster Hunter:** Sword and Shield and Dual Blades are **separate weapon types** among its 14, each with its own moves ([Icy Veins, Wilds list](https://www.icy-veins.com/monster-hunter-wilds/weapon-type-list)).
- **Dark Souls 3:** paired weapons are **one item** with one moveset ([Fextralife](https://eldenring.wiki.fextralife.com/paired+weapons)).
- **Elden Ring:** two weapons **of the same type** switch to a "power stance" moveset automatically, and paired weapons take one slot ([Den of Geek](https://www.denofgeek.com/games/elden-ring-dark-souls-2-best-feature-powerstancing-dual-wielding), [wiki.gg](https://eldenring.wiki.gg/wiki/Paired_Weapons)). It is liked; mixing types (a longsword and a dagger) gives no special moves.
- **Our D-209** already reads like Elden Ring's rule (same type, the pair has its own stance). Option (b) is that rule with Paragon and CombatMaster supplying the stance and swings.

## 6. Paragon heroes not on disk

**What we have is the free animation-only pack.** Our folder (`ParagonAnimations`, 5,370 files in `<Hero>Manny` folders) matches the free Fab pack "Paragon animations retargeted to Manny" by a third-party seller: "Animation sequences only", about 5,300 animations, £0 ([listing](https://www.fab.com/listings/e6de87b1-b755-478d-9228-a9eb89ff4411), checked 2026-11-17). Its page does not list which heroes it includes, so I could not confirm whether the missing heroes were left out by that pack or only by our import.

**Heroes Epic released free but not on disk** (from the 2018 release coverage plus memory of Paragon's roster; the full official list could not be opened: [80.lv](https://80.lv/articles/final-round-of-free-paragon-assets-valued-at-over-5-million), [CG Channel](https://www.cgchannel.com/2018/09/get-free-paragon-ue4-character-and-environment-assets)):

| Hero | Weapon | Could help with | Sure |
|---|---|---|---|
| **Sevarog** | Giant hammer ([Game Informer](https://gameinformer.com/games/paragon/b/playstation4/archive/2016/03/29/playable-hero-sevarog-arrives-in-latest-update.aspx)) | **Two-handed hammer**, the gap Grux leaves (maces) | Medium |
| Shinbi | Melee with spirit wolves (weapon not confirmed) | Possibly sword or dagger | Low |
| Rampage | Giant fists, thrown rocks | Gauntlets (a heavy variant) | Medium |
| Howitzer | Long-range mech, artillery | Gun (heavy) at best | Low |
| Iggy & Scorch | Fire turrets and thrown molotovs ([PC Gamer](https://www.pcgamer.com/iggy-scorch-join-paragons-roster-of-heroes/)) | None of the twelve (a caster) | Medium |
| Boris | Announced, never shipped in the game; released as an asset | Not known | Low |

- **Spear and scythe:** **no missing hero clearly fills them** either. Serath (on disk) remains the only weak scythe lead.
- **A second dual or shield set:** none of the missing heroes adds one. The best addition is **Sevarog for a two-handed hammer**.
- **Where they exist:** these heroes come **inside Epic's full hero packs** ("Paragon: Sevarog" etc.: model, skins, animations, Animation Blueprint) on **their own skeletons**. Their animations would need retargeting to Manny, unlike the pack we already have. Whether any animation-only Manny copy exists for them: **not verified**.
