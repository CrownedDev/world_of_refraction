# Gear and body standard: the house rulebook

**Status:** in line with D-242 (every paragraph added on 2026-11-17) and D-243. Crown has answered the first round of questions; section 11 holds what is still open. Nothing is built from it yet. It also follows D-241 / D-234 / D-238 (bodies and colour), D-209 / D-211 / D-215 / D-224 (pairs, the standard attack, skills), D-223 (the clean gear set), D-058 and `docs/Conventions/ContentNaming.md`. The facts about the packs are from `docs/research/asset-shortlist.md`.
**Marks:** *Ruled* = Crown's words in a D-entry. *Default* = proposed here. *Measured* = from `docs/research/weapon-orientation.md` (37 weapon models and the hand attach points on both bodies) or from D-242's "The holds, measured" (the hero packs), both read through the editor on 2026-11-17. **(editor)** = still needs the editor or Crown's eyes in PIE.

## 1. The idea in five lines

1. Every weapon type has **one template**: its grip, stance, standard attack and a pool of moves. Every weapon of that type uses it.
2. **Any single piece can override** part of the template, alone or in a pair.
3. A pair with a real animation set gets a **named pair template**; every other pair swings the main hand and carries the off hand. Every attack says which hand it uses: **main, off or dual**.
4. The moves in a template **become skills the player chooses**; a template is a source of skills, not a fixed move list.
5. Placeholders go in now, to one set of rules, so Crown's own art later **drops into the same places with no code change**.

## 2. A weapon type template

**Ruled (D-242):** "all weapons will have a universal default": one template per weapon type, built from what the project owns.

| Part | What it is | Today |
|---|---|---|
| **Model slot** | Where the piece's model goes. The model is the piece's own; the template only says how it is held | Each weapon asset holds its model, static or animated (`Public/Equipment/Weapons/WeaponData.h:123-126`) |
| **Grip** | Which hand, which attach point, and the turn that makes it sit right | The attach point is picked by weapon type in code, and only sword, dagger, greatsword and axe are mapped; any other type logs an error and shows nothing (`Private/Equipment/Weapons/WeaponMeshComponent.cpp:77-93,111-123`). The turn is on each piece (`WeaponData.h:130,149`) |
| **Stance** | The idle pose with the weapon out | On each weapon asset (`WeaponData.h:74`); a stance is one idle animation (`Public/Character/StanceData.h:39`) |
| **Standard attack** | The basic swing: the same one in the field and in a fight (D-215) | On each weapon asset (`WeaponData.h:60`), with its fight animation and its field swing (`Public/Skills/Definitions/SkillDataBase.h:320,325`) |
| **Pool of moves** | The clips that can become skills (section 5) | Skills are their own assets that name the weapon type they need (`Public/Skills/Definitions/AbilityData.h:42`); a weapon carries none (D-224). The old list field is still on the asset (`WeaponData.h:65`) |
| **Guard** | The block or parry with this weapon | Per weapon (`WeaponData.h:109-111`), else the character's own block (`Public/Character/CosmeticsData.h:53`) |
| **Hit and death reactions** | Being struck and falling while holding it | No per-weapon field found in the weapon or cosmetics data (not checked further). Most owned sets lack a death clip; one shared death serves all (shortlist §5.1) |

**The gap the template closes:** today every weapon asset must carry its own attack and stance, and there is no default behind them. A weapon without them cannot fight; nine such weapons were deleted for that reason (`3a517b96b`). With a template, a new weapon needs a model and a type, nothing more.

**Default:** a template is one small data asset per type (`DA_WeaponType_<Type>`), looked up by the weapon's type. The fields on the weapon asset stay and become the override (section 3), so nothing existing breaks.

## 3. The override

**Ruled (D-242):** "we will also include an override": any single piece can replace the default for that piece, alone or in a pair.

| A piece may replace | Alone | In a pair |
|---|---|---|
| Its **standard attack** | Its own swing instead of the type's | The off-hand piece's override replaces the main hand's attack (ruled) |
| Its **stance** | Its own idle | Its own paired idle (today: `WeaponData.h:84`) |
| **Moves** | Skills only this piece can use | The same |
| Its **grip turn** | A model that sits differently | The same, per hand |

**Order, most specific first:** the off-hand piece's override (**ruled: when both pieces carry one, the off hand's wins**), then the main-hand piece's override, then the main hand's type template. That is how a pair gets its own attack: "for example dual blades", the second blade brings the two-blade attack.

**Attack hand types (ruled, D-242).** Each attack says which hand it uses:

| Hand type | Used when |
|---|---|
| **Main hand** | The piece is in the main hand, alone or paired |
| **Off hand** | The piece is in the off hand |
| **Dual** | Both hands are filled: a pair's own attack is a dual one |

A piece brings the attacks that fit where it is held. This replaces the question "does every shield bring the paired attack": a shield in the off hand brings its off-hand and dual attacks, and a shield may be a main weapon one day and bring main-hand ones. "We should use the main hand, unless the shield has an override; that way we can add sword and shield attacks" still holds.

**Mirroring (ruled):** there is one set of moves. A move made for one hand is **mirrored** when the other hand uses it, with one mirror set-up per skeleton. How a mirrored move looks with a shield on the other arm is watched in the check of section 10, step 3.

**Today, and one correction to D-242:** the entry's `OverrideAttack` (`Public/Loadout/Entries/FWeaponLoadoutEntry.h:53-57`) is not unused. The field swing and the fight both read it before the weapon's own attack (`Private/Field/FieldMovementAttack.cpp:110`; `Private/Loadout/LoadoutComponent.cpp:1419-1440,2534-2536`). But it sits on the **loadout entry**, not on a piece, so it cannot travel with a shield. Default: the override lives on the piece's asset; the entry's field stays as it is until the new path is proven.

## 4. Pairs

**Ruled (D-242):** a pair with a real animation set of its own gets a **named pair template**; the rest use the main hand's template with the off hand carried. A pair can use the main hand's moves as well as its pair moves. A pair is one weapon for slots (D-211, D-224).

**Today:** only same-type pairs, plus any pairable weapon with a shield (D-209, B-515). The pair's stance is the main hand's paired stance, and a shield pair uses the main hand's single stance (`Private/Loadout/WeaponSetRules.cpp:106-123`).

| Pairing | Template | Source | Note |
|---|---|---|---|
| Sword and shield | **Named** | CombatMaster DSAS V1 (attacks) and V2 (guard) | In use today (the Light Hit) |
| Axe and shield | **Named** | Paragon Terra | |
| Two swords | **Named** | CombatMaster Handsome Twin Sword | |
| Two daggers | **Named** | Paragon Kallari (Countess as a second) | The most complete set |
| Two guns | **Named** | Paragon TwinBlast | |
| Two hammers | **Named** | Paragon Grux | One-handed maces |
| Gauntlets | **Named** | Paragon Crunch, or CombatMaster hand to hand | Always a pair |
| Two axes | Fallback | CombatMaster Twin Axe is root motion only | Named once it has an in-place pass |
| Dagger, hammer or gun with a shield | Fallback | None | Main hand swings, shield carried |
| Mixed pairs (sword and axe) | Fallback | None | Not allowed yet (D-209) |
| Greatsword, spear, staff, scythe, bow | Never paired | — | Two-handed |

**A pair template adds** to a single one: which hand each piece sits in (a shield on the forearm, not the hand); a paired idle; paired moves that use both hands; a guard that fits (a shield block differs from a two-blade parry).

## 5. From a move to a skill

**Ruled (D-242):** "we will be turning these into skills that the player can choose from". And skills are never built into gear (D-224): a move becomes a skill asset that names what it needs, and reaches the player through a trial's pool or a shop (D-225).

| For one skill, authored | Shared from the template |
|---|---|
| One attack cut from the hero's clip, as its own animation, in place | The grip, attach points and stance |
| Its hit moments marked on it (the fight reads them in order, `SkillDataBase.h:304`) | The guard and reactions |
| The skill asset: name, the weapon type or pair it needs, damage kind, cost, tier, icon | The weapon's trail and hit effects |
| Its pool tag, so a trial can give it | The skeleton it plays on |

**How many a hero's set could give:** the heroes hold 11 to 24 basic attack clips and 1 to 27 heavier ones (shortlist §5.1), but many are variants of one move. The project already has about 225 weapon skill assets for sword, sword and shield, greatsword, axe, two axes and gauntlets (`ContentNaming.md:46`; D-243).

**The first set (ruled, D-242): one standard attack and six skills per weapon type** (it was three; Crown raised it). They are picked from the hero pack's own moves: **Kwang for the sword, Greystone for sword and shield, Sun Wukong for the staff**. The six are **proposed, not picked**: the packs' move lists were read by name and length only, and Crown has not watched the moves (section 11).

**A shared move carries its hit moment per weapon type (ruled, D-242).** The scythe and the spear use the staff's moves. Each move is looked at for the weapon's shape: "a wide swing with a staff would have a later delay than a scythe, as a scythe has more reach". So the animation is shared, and **the moment the hit lands** (when the defender must react) is set for each weapon type that uses the move. Today a hit moment is marked once on the animation (`SkillDataBase.h:304`), so this is new.

### How a weapon skill's damage is counted (D-243, not built)

**Ruled:** an attack or skill used through a weapon deals **the weapon's part times the skill's multiple**: a standard attack is x1, a heavy skill x2. Today the weapon's part is added to the skill's own damage (D-101), so a strong weapon drowns the skill out. Multiplied, a x2 skill is always double a standard attack and the player can read it at a glance. Weapon Power keeps its present ladder (F 40 to S 192). Spells are not affected.

**Not decided, to be planned before code:** how the skill's tier power, the tier gap, infusion and status build-up sit with the multiple; turning the existing skills' damage into multiples. It is a job of its own after the trial pool stages, and it blocks the numbers of the six skills, not their moves.

## 6. The standards

| # | Topic | The rule | Why |
|---|---|---|---|
| 1 | **How the model is built and held** | **The Paragon grip is the reference** (ruled, D-242), and **Kwang's hold** is the one measured for it. Every weapon model and every body is lined up to it once. The values are in full below; the matching attach point on Manny and Quinn is not made yet | Crown: Paragon is "tried and proven"; "if we can get them to work, all we need to do is tweak everything else to fit". And: "we won't have to play around with positioning" |
| 2 | **Put away** | **Hidden** (ruled, D-242), as today (`WeaponMeshComponent.cpp:172-182`). The back and hip points are named on the body now, unused; Kwang's back mount is measured (below) | Nothing to align today; showing a weapon on the back later needs no new body work |
| 3 | **Scale** | Real size in centimetres, checked against Manny's hand; a length range per type | Packs differ (**editor**); one check on import catches a giant dagger |
| 4 | **Attach-point names** | One fixed list on every body: right hand (`hand_r_weapon`), left hand (`hand_l_weapon`), shield, one ring point per finger (below), back, hip, arrow or muzzle (`weapon_r_muzzle`, on both bodies today), effect point. **One per hand, not one per weapon type**, and the **same position and turn on every body** | Measured: seven of twelve types have no point today and show nothing; Manny and Quinn carry different turns under the same names; Quinn already has the two per-hand points, Manny does not (`weapon-orientation.md` §2). With rule 1, one point per hand serves every type |
| 5 | **Model kind** | Static by default; animated only when the piece itself moves (a bow string, a chain). The gear code holds both (`WeaponData.h:123-126`) | Static is cheaper and simpler; StylizedWeapons ships everything animated, so most of its pieces can be simplified |
| 6 | **Colour** | One material per kind (weapon, apparel, crystal, body) with named settings: `Tint`, glow strength, tier glow. The element's colour comes from the element table. Never a copy per colour | Bodies already work this way (`Private/Character/Forms/SpiritLook.cpp:37-38,68-72`). StylizedWeapons does not: its colours are separate copies with no tint setting (`docs/Architecture/WeaponSystem.md`, `UWeaponData`), so it needs one new material (**editor**) |
| 7 | **Animation in place** | In place by default; root motion only where a move is marked for it (CLAUDE.md). Keep only the in-place copy | Fights are fixed-position. CombatMaster's root-motion-only sets (DSAS V3, Twin Axe) wait for an in-place pass |
| 8 | **Naming** | New assets follow `ContentNaming.md`: `SM_` / `SK_` models with the type in the name (`SM_Sword_1H_...`), `AS_` clips, `AM_` montages, skills as `DA_<Family>_<Attack or Ability>_<Name>`. Pack assets keep their names (D-058) | A picker can filter by type; new files sit beside their siblings |
| 9 | **Icons** | One style, one size, clear background; the frame is drawn by the screen, not baked in. An adapter's icon is the frame with its crystal's icon inside (D-229) | Three styles are in use today; one icon plus a tint beats one per element per tier |
| 10 | **Heavy files** | New model, texture, sound and effect fields are loaded on demand (D-057) | Standing rule |

### Rule 1 in full: the Paragon grip is the reference (Crown, 2026-11-17)

**Ruled (D-242, "Paragon is the reference"):** how a Paragon hero holds its weapon is the standard, because the animations were made for it. Every weapon model, the StylizedWeapons ones included, and every body is lined up to it once, not the other way round. This replaces this document's earlier draft, which took the StylizedWeapons pattern as the standard.

**What the reference is:** the weapon's place and turn relative to the hand, in the hero's own skeleton. It has two halves, and both are lined up to it:

| Half | Lined up to the reference by |
|---|---|
| **The body's attach point** (one per hand, rule 4) | Sitting at the same place and turn, relative to the hand, as the hero's weapon bone |
| **The weapon model** | Being built the way the hero's weapon is built on that bone: the same length axis, the same end for the tip, the origin at the same spot on the grip |

With both true, a model sits right in the hand with no turn and no offset, on every body, for every Paragon clip.

**The holds, measured** (D-242, "The holds, measured"; the heroes' own skeletons in their reference pose; place as x, y, z in centimetres, then turn as pitch / yaw / roll in degrees). Crown added three hero packs, kept out of git: Kwang, Greystone and Sun Wukong.

| Hold | From | Place | Turn | Note |
|---|---|---|---|---|
| **Kwang, main hand** (`weapon_r`) | `hand_r` | -10.144, 5.902, 2.104 | 16.348 / 51.134 / 109.024 | **The reference** |
| Kwang, off hand (`weapon_l`) | `hand_l` | The mirror of the main hand | | |
| Kwang, back mount (`weapon_backMount_r`) | `spine_02` | -28.904, 2.131, 57.391 | -90 / -158.321 / 0 | For the back point of rule 2 |
| Sun Wukong, main hand (`weapon_r`) | `hand_r` | -8.099, 3.863, -0.617 | 17.026 / 59.274 / 109.680 | **Matches Kwang**, within about 8 degrees: one hold serves both |
| Greystone, sword (`sword_bottom`) | `hand_r` | -6.960, 2.908, 0 | 0 / 0 / -10.651 | **Differs**: his own bones, blade along +Z |
| Greystone, shield (`shield_inner`) | `hand_l` | -14.486, -0.819, -0.160 | 0.632 / 3.236 / 81.473 | The only shield measured; carried on the forearm |
| Ours today, Manny `hand_r_sword` | `hand_r` | -3.7, 2.65, -6.47 | 10.9 / 89.24 / -67.42 | **Matches none of them**: about 11 cm from Kwang's |

| Part of the rule | Value |
|---|---|
| Main-hand grip | Kwang's `weapon_r` hold, above |
| Off-hand grip | Its mirror |
| Shield | Greystone's forearm hold, above, until Crown has looked at it (**editor**) |
| The model's length axis and tip end | On Kwang the blade runs along the hold's **-Y**. StylizedWeapons models run along **+Y**, so they sit back to front on it until turned (below) |
| The model's origin | At the grip, as StylizedWeapons already is |
| Bows, fist weapons, long two-handers | Not measured: no bow or fist hero pack is added. Wukong's staff uses the same hold as Kwang's sword |
| Kind and bones | One root bone and no attach points of its own, or a plain static model (rule 5) |

**The next step, for Crown to look at:** one new attach point per hand on Manny and on Quinn at Kwang's numbers, with the weapon turned for the axis. Not done (section 10, step 1). The turns of our old points and Kwang's cannot be compared like for like, because the blade axes differ.

**The hero packs, read (D-242):** in every pack the weapon is **baked into the hero's body** and moved by bones; none has a separate weapon model. Crown: "don't bother for now" about cutting a weapon out. So:
- **Our weapons come from StylizedWeapons.**
- **The heroes are whole bodies** (a "Body of", cosmetics: Kwang has 6 skins, Greystone 5, Wukong 7) **and the pattern for the hold.**
- A hero's skins and weapon looks belong to the Cosmetics side, not to gear.

**What the placeholder models do today** (measured, `weapon-orientation.md` §1 and "The most common pattern"). This is no longer the standard; it is the starting point the correction works from:

| Pack | Its pattern today | To reach the reference |
|---|---|---|
| **StylizedWeapons** (about 110 models; all four weapons in use) | One pattern throughout: along **Y**, tip or head to **+Y**, origin at the grip (19 to 22 cm from a sword's or dagger's pommel, 40 to 53 cm from an axe's or hammer's butt), edge along Z. Shields: face along X, origin near the centre of the back | Because the pack agrees with itself, **one and the same correction for the whole pack**: turned end for end, since Kwang's blade runs along -Y. The origin already sits at the grip |
| StylizedWeapons staff, two-handed hammer, one two-handed axe | Same axis; origin near the middle of the shaft | The pack's correction, plus the origin moved if the hero's hand sits elsewhere |
| StylizedWeapons fist weapons | Along Z, origin inside the fist | Their own correction, from a fist hero (Crunch) when gauntlets go in |
| **100 Fantasy pack** (grey: scythes, guns, spears) | Not one pattern: most along Z, guns along X, some origins off the end | A correction per piece, for the few used (scythe, gun) |
| **CombatMaster weapons** | Blades to -Z; not consistent with itself | None: its weapons are not used, only its animations |

**The one-off correction (default):** the model is **corrected once and re-imported** to the reference, so the corrected file is the one the game keeps and later models are simply made that way. Not an attach point added to the model: the gear code reads none, and it hides the problem instead of removing it. Not an offset on the weapon data: there is no position field there today, and a per-piece fix is exactly what Crown wants gone. Today's per-piece turn (`WeaponData.h:130,149`) stays only as the deliberate override of section 3; the two old assets that use it to flip a model (`DA_Weapon_Tomahawk`, `DA_Resonator_Equpment_Sword`) are old-format and go with the old assets.

For the first look (step 1), the four pieces in use may carry the turn in their existing per-piece field, as a placeholder; the re-import follows once Crown has agreed the hold.

**The bodies are lined up too.** Today the fix-ups live in the hand attach points: one per weapon type, with different turns on Manny and Quinn (`weapon-orientation.md` §2). Under the reference there is one point per hand, at Kwang's place and turn, the same on every body (rule 4).

**Still to check by eye:** a hero's hand is not Manny's hand, so Kwang's numbers may need one small, fixed adjustment on Manny. And Greystone, the source of the sword-and-shield moves, holds his sword differently from the reference, so his moves must be watched with the sword on Kwang's hold.

**Rings (ruled, D-242):** in a fight a ring is **a glow, not a model**, and **each finger stands for one slot**: a Resonator's five adapters are five fingers. So the body carries one ring point per finger. Which hand, and which finger is which slot, is not ruled (section 11).

## 7. Bodies

**Ruled:** Body is how a character looks ("Body of Orthos", D-241). Male Nobody and Female Nobody are Manny and Quinn (D-234). A new character's body takes the colour of its element or class, and the chest light too (D-238).

**Today:** the two bodies are fixed paths in code, and the colour is set through one setting called `Tint` plus three chest-light settings (`SpiritLook.cpp:32-38,68-72`). **Measured:** Manny and Quinn are on **two different skeleton assets** with the same 161 bone names, and their weapon attach points have the same names and positions but different turns (`weapon-orientation.md` §2). So at least one body likely holds weapons at a different angle today (**editor**: needs eyes in PIE).

**A new body must have:**

| Need | Rule |
|---|---|
| **Skeleton** | The UE5 Manny bone set and names. Every animation the game owns plays on it. Default: **one skeleton asset for every body**, so attach points are made once; today's two are brought to one, or kept in step by a check (**editor** picks) |
| **Attach points** | The full list in section 6, rule 4, with the same names, positions and turns as Manny's |
| **Colour settings** | `Tint` and the chest light on the two base bodies only. **An unlocked body keeps its own look** and does not take the element's colour (ruled, D-242) |
| **Proportions** | Close enough to Manny that attack reach and hand positions hold (**editor**) |
| **Apparel** | Either it wears Manny's or Quinn's apparel, or apparel is hidden on it |

**Hero packs as bodies (D-242):** Kwang, Greystone and Sun Wukong are on disk, out of git, each on its own skeleton. They are used as **whole bodies** and for their skins (cosmetics). Each carries its own weapon inside the body, and nothing is cut out for now; what that means when the body is worn with the game's gear is open (section 11).

**Converting a body on its own skeleton** (every Paragon hero is one; shortlist §4.3):
1. Move the model onto Manny's skeleton, once, in the editor.
2. Add the standard attach points.
3. Decide its apparel: hidden, or a fitted version.
4. Check its reach against the fight's spacing.
5. Check whether its materials can take `Tint` (**editor**).

UE4-era packs (MageAnimationSet) go through one shared converter, set up once.

## 8. The twelve weapon types

"Pack pattern" is whether the model follows the StylizedWeapons pattern, so the pack's one correction (turned end for end) brings it to Kwang's hold (measured, `weapon-orientation.md` §1). "Shows today" is whether the code has an attach point for the type (§2 there).

| Type | Model | Pack pattern | Shows today | Animations | Pair set | Missing |
|---|---|---|---|---|---|---|
| Sword | StylizedWeapons | Yes | Yes | **Paragon Kwang** (D-242: the six skills); CombatMaster sword | Two swords; with shield (**Greystone**, D-242) | Nothing |
| Greatsword | StylizedWeapons | Yes | Yes | CombatMaster; Paragon Greystone, Kwang | — | Nothing |
| Spear | StylizedWeapons polearm (painted) | Yes | **No** | **The staff's moves** (ruled), its own hit moments | — | Its own animations, later |
| Staff | StylizedWeapons | Axis yes; origin mid-shaft | **No** | **Paragon Sun Wukong** (D-242: the six skills); MageAnimationSet for casting | — | The UE4 conversion |
| Dagger | StylizedWeapons | Yes | Yes (the sword's point) | Paragon Kallari, Countess | Two daggers | Nothing |
| Axe | StylizedWeapons | Yes | Yes | CombatMaster axe; Paragon Terra | With shield; two axes by fallback | In-place two-axe clips |
| Hammer | StylizedWeapons | One-handed yes; two-handed origin mid-shaft | **No** | Paragon Grux, Narbash | Two hammers | A two-handed hammer set |
| Bow | StylizedWeapons (animated, own draw clips) | Yes (the bow form) | **No** | Paragon Sparrow | — | Left-hand hold; the arrow point exists (`weapon_r_muzzle`) |
| Gauntlets | StylizedWeapons fist weapons | The stated exception | **No** | Paragon Crunch, Khaimera; CombatMaster | Gauntlets | Nothing else |
| Scythe | **Grey model** (Fantasy pack) | **No**: one-off correction | **No** | **The staff's moves** (ruled), its own hit moments: a longer reach lands earlier | — | A painted model |
| Gun | **Grey model** (Fantasy pack) | **No**: runs along X; one-off correction | **No** | Paragon Revenant, Murdock, Wraith | Two guns | A painted model |
| Shield | StylizedWeapons | Yes | Yes | Paragon Steel, Terra; CombatMaster DSAS | With sword; with axe | Nothing (off hand only, B-515) |

The seven types that do not show today all start showing with rule 4's one point per hand; none needs a point of its own.

## 9. A checklist for any new asset

1. **Name** follows `ContentNaming.md`, with the weapon type in it.
2. **Scale** is real size; it passes the length range for its type.
3. **Built to rule 1:** on Kwang's hold it sits as his weapon does (origin at the grip, tip along the hold's -Y); held, it needs no turn and no offset.
4. **Kind:** static, unless the piece itself moves.
5. **Colour:** it uses the kind's one material; the element tint works; no copy per colour.
6. **Type:** its weapon type is set, so the template gives it a stance and an attack with nothing else filled in.
7. **Animations** play on the Manny skeleton, in place, with their hit moments marked for each weapon type that uses them, and each attack's hand type (main, off, dual) set.
8. **A body** carries every attach point and, if it shows an element, the colour settings.
9. **An icon** in the house style.
10. **Git:** only the assets the game uses are committed; the rest of the pack stays out (CLAUDE.md).
11. **Licence** read at its source before release (Paragon: no Paragon name or logo).

## 10. The order to put the placeholders in

The first three steps come from D-242; the rest follow the shortlist's order. Each step in Crown's order: apply, Crown looks, align code and data, test the real path.

| # | Step | Crown sees | The problem it is meant to find |
|---|---|---|---|
| 0 | **Measure the Paragon grip. Done** (2026-11-17): Kwang, Greystone and Sun Wukong added and their holds measured (section 6, rule 1) | — | It found: Kwang and Wukong share one hold; Greystone differs; our sword points match none; the blade axis is the other way round |
| 1 | **A matching attach point on Manny and on Quinn,** one per hand at Kwang's numbers, with the sword turned for the axis. **Next** | The sword, shield and both axes in Manny's hands and in Quinn's, side by side, and beside Kwang in the same clip | Whether Kwang's hold looks right on Manny, or needs one fixed adjustment; whether both bodies now hold alike (their turns differ today); whether one point per hand serves every type |
| 2 | **The template itself,** on the sword and on sword and shield, from what is already in the game (D-242's "look into first") | Nothing new: the same sword, now with no attack or stance on the piece | Whether the default-then-override order holds in the field and the fight; whether the hand types (main, off, dual) fit the shield |
| 3 | **The check: staff, scythe and spear on one swing** (D-242). Attach each in turn, play the same staff swing, and set each weapon's hit moment. In the same check, play a **mirrored move with a shield** on the other arm | Three weapons through one swing; a mirrored move beside the original | Whether the scythe and spear can truly share the staff's moves; how much earlier a longer reach lands; where the hand sits on a long shaft; whether a mirrored move looks right with a shield |
| 4 | **Bow** (StylizedWeapons bow, Sparrow's clips; no bow hero pack is added yet) | A bow drawn and fired | An animated weapon in the left hand; the bow's own clip in time with the body's; the arrow point (`weapon_r_muzzle`); reach in a fixed-position fight |
| 5 | **Ring glow and crystal** together | A glow on a finger in its element's colour; five on a Resonator | One ring point per finger on both bodies; one crystal model tinted nine ways; whether the glow can be seen at the fight's camera |
| 6 | **Apparel on both bodies** | One piece on Male and Female Nobody | One piece or one per body; clipping; tint; how weapons and ring glows sit over sleeves |
| 7 | **A hero body** (Kwang) with all of the above worn | A different body holding the same gear | The attach points on a new skeleton; the hero's own baked-in weapon beside the game's; apparel on a body it was not made for; saving the chosen body |
| 8 | **Staff casting** through the UE4 conversion, whenever | A staff cast | The one shared converter, so every UE4-era pack becomes usable |
| 9 | **One corrected grey model** (the gun), whenever | A gun in the hand with no turn set | The one-off correction itself: that a re-imported model needs nothing more |
| — | **Icons,** any time | Every screen filled | The size and frame each screen expects |

## 11. Answered, and still open

**Answered by Crown (D-242, 2026-11-17):**

| The question was | Crown's answer |
|---|---|
| Both pieces of a pair carry an override: which wins? | **The off hand's.** That is how a pair gets its own attack |
| Does every shield bring the paired attack? | Replaced: **each attack carries a hand type** (main, off, dual); a move for the other hand is **mirrored** |
| Rings in a fight: a model or a glow? | **A glow, one finger per slot** |
| Does an unlocked body show the element's colour? | **No, it keeps its own look** |
| Put away: hidden or on the back? | **Hidden** |
| How big is the first set? | **One standard attack and six skills** per weapon type (not three) |
| Spear and scythe animations? | **They share the staff's moves,** with the hit moment set per weapon type |

**Still open:**

| # | Open | Whose | Note |
|---|---|---|---|
| 1 | **How "Mind of" is worked out** (D-241: what a character's skills lean towards) | Crown | Nothing is built for it; lore-adjacent, so no default |
| 2 | **Which six skills per weapon type** | Crown | They are proposed, not picked: Crown has not watched the moves |
| 3 | **Rings: which hand, and which finger is which slot** | Crown | D-242 rules a finger per slot, not the order. Default: the hand that holds no weapon, thumb first in slot order |
| 4 | **A hero body carries its own weapon inside the body.** With no cut-outs for now, what shows when that body is worn with the game's gear? | Crown | Default: the hero's own weapon is hidden if its bones allow it (**editor**); if not, that body is for looking at, not for fighting, until a cut-out is made |
| 5 | **The shield's hold:** Greystone's forearm hold is the only one measured, and his sword hold is not the reference | Editor, then Crown's eye | Looked at in step 1 |
| 6 | **How the skill's multiple sits with tier, infusion and status build-up** (D-243) | To be planned before code | Its own job |

**Still needs the editor or Crown's eyes:** whether Kwang's hold looks right on Manny and on Quinn; where the hand sits on a long shaft (staff, spear, scythe, two-handed hammer and axe); which way the edge faces on axes and curved blades (only the outer box was measured); the two skeleton assets brought to one, or one mirror set-up made for each; scale inside each pack; whether the moves wanted from Kwang, Greystone and Wukong are already in the Manny copies on disk or must be converted from the packs; the hit moments of the moves (not readable through the connection); whether StylizedWeapons can take a tint; whether one apparel piece fits both bodies. Only a sample was measured: 19 of about 110 StylizedWeapons models.

## Clashes found

1. **D-242 calls `OverrideAttack` unused.** It is read in the field and in the fight, and it is on the loadout entry, not on a piece (section 3). An override that travels with a shield needs a field on the piece.
2. **Settled:** D-242's pairs against its own standard-attack line. The off hand's override wins and attacks carry a hand type, so a pair's own attack is the off-hand piece's dual attack.
3. **Settled:** D-238's "spirit worn over the body" is a body since D-241, and an unlocked body keeps its own look (D-242).
4. **D-209 against the pair table:** D-209 allows same-type pairs only, while Terra's axe and shield and the shield rule (B-515) pair across types. Settled in code already: a shield is the one allowed cross-type off hand.
5. **The shortlist against the measurement:** the shortlist says "Quinn shares the skeleton" (§3). Measured, Manny and Quinn are on two skeleton assets with matching bone names and different attach-point turns. The measurement is the newer fact (section 7).

6. **The shortlist against D-242 on the heroes' weapons:** the shortlist lists Kwang and Greystone under greatsword. D-242 takes Kwang for the sword and Greystone for sword and shield (his pack carries a shield on the forearm). D-242 is the newer ruling; section 8 follows it.
7. **Greystone inside D-242:** he is the source of the sword-and-shield skills, but his hold "does not" match the reference. His moves have to be watched on Kwang's hold (step 1) before the six are picked.
8. **D-242 on converting:** it says all three heroes' animations "need retargeting to Manny", while Manny copies of Kwang's, Greystone's and Wukong's animations are already on disk (shortlist §4.1). Whether the wanted moves are among those copies is to be checked before any converting.
9. **D-243 against D-101:** the weapon's part is multiplied by the skill, where D-101 adds it. Deliberate; D-243 says so, and the two games now differ here on purpose.
10. **Hit moments:** D-242 sets a move's hit moment per weapon type; today it is marked once on the animation (`SkillDataBase.h:304`). A build note, not a clash of rulings.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-17 | Created from D-242, the asset shortlist and the weapon code as it stands | dev (docs only) |
| 2026-11-17 | Crown's addition: rule 1 fixes how a weapon model is built (origin at the grip, one length axis, one end); the values wait for `docs/research/weapon-orientation.md` | dev (docs only) |
| 2026-11-17 | Rule 1 filled from the measurement (along Y, tip to +Y, origin at the main hand; shields face X); which packs match and the one-off correction (re-import); one attach point per hand settled; the twelve-type table and steps 0a and 6 added; Manny and Quinn found on two skeleton assets | dev (docs only) |
| 2026-11-17 | Crown, D-242 "Paragon is the reference": rule 1's standard is the Paragon grip, not the StylizedWeapons pattern; the values wait for step 0 (add and measure Kwang, then Sparrow); the StylizedWeapons pattern stays as what the placeholders do today | dev (docs only) |
| 2026-11-17 | Brought in line with D-242's later paragraphs and D-243: the measured holds (Kwang's is the reference), the hero packs read (weapons baked in, no cut-outs), attack hand types and mirroring, the ring glow per finger, six skills per weapon type, shared staff moves with a hit moment per weapon type, the weapon-times-skill rule, the staff / scythe / spear check as step 3; section 11 is now answered and still open | dev (docs only) |
