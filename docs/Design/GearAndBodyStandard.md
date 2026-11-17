# Gear and body standard: the house rulebook

**Status:** for Crown to read (2026-11-17). Nothing is built from it until Crown has read it. It follows D-242 (placeholders first, templates, pairs, the override), D-241 / D-234 / D-238 (bodies and colour), D-209 / D-211 / D-215 / D-224 (pairs, the standard attack, skills), D-223 (the clean gear set), D-058 and `docs/Conventions/ContentNaming.md`. The facts about the packs are from `docs/research/asset-shortlist.md`.
**Marks:** *Ruled* = Crown's words in a D-entry. *Default* = proposed here; the ones that need Crown are in section 11. **(editor)** = only opening the asset in the editor can confirm it; the packs were read by file name only.

## 1. The idea in five lines

1. Every weapon type has **one template**: its grip, stance, standard attack and a pool of moves. Every weapon of that type uses it.
2. **Any single piece can override** part of the template, alone or in a pair.
3. A pair with a real animation set gets a **named pair template**; every other pair swings the main hand and carries the off hand.
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

**Order, most specific first (default):** the off-hand piece's override, then the main-hand piece's override, then the pair's named template, then the main hand's type template.

**How a shield brings a sword-and-shield attack (ruled):** "we should use the main hand, unless the shield has an override; that way we can add sword and shield attacks". The shield piece carries a paired attack; with a sword in the main hand that attack replaces the sword's plain swing. A shield with no override just rides along.

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

**How many a hero's set could give:** the heroes hold 11 to 24 basic attack clips and 1 to 27 heavier ones (shortlist §5.1), but many are variants of one move. A fair guess is **one standard attack and four to eight distinct skills per hero (editor)**. The project already has 224 skill assets for sword, sword and shield, greatsword, axe, two axes and gauntlets (`ContentNaming.md:46`).

**The first set (default): one standard attack and three skills per template**: one quick, one heavy, one that stands out. Three is what a D piece can hold and what an enemy fills by default (D-225), so every type is playable at once; more come later from the same clips.

## 6. The standards

| # | Topic | The rule | Why |
|---|---|---|---|
| 1 | **How the model is built** | Every weapon model is made the same way: its origin at the grip, lying lengthways along one fixed axis with the tip or blade towards one fixed end, the edge towards another. The values are set from the measurement (below). The right hand is the default; left-hand pieces (shield, bow) say so | Crown: "that way we can make any other assets follow the same pattern and we won't have to play around with positioning" |
| 2 | **Put away** | Hidden, as today (`WeaponMeshComponent.cpp:172-182`). The back and hip points are named on the body now, unused | Nothing to align today; showing a weapon on the back later needs no new body work |
| 3 | **Scale** | Real size in centimetres, checked against Manny's hand; a length range per type | Packs differ (**editor**); one check on import catches a giant dagger |
| 4 | **Attach-point names** | One fixed list on every body: right hand, left hand, shield (forearm), ring, back, hip, arrow, effect point. Default: **one per hand**, with the type's grip on the template, not one per weapon type as today | Eight of twelve types have no point today; a new body then needs 8 points, not two per weapon type |
| 5 | **Model kind** | Static by default; animated only when the piece itself moves (a bow string, a chain). The gear code holds both (`WeaponData.h:123-126`) | Static is cheaper and simpler; StylizedWeapons ships everything animated, so most of its pieces can be simplified |
| 6 | **Colour** | One material per kind (weapon, apparel, crystal, body) with named settings: `Tint`, glow strength, tier glow. The element's colour comes from the element table. Never a copy per colour | Bodies already work this way (`Private/Character/Forms/SpiritLook.cpp:37-38,68-72`). StylizedWeapons does not: its colours are separate copies with no tint setting (`docs/Architecture/WeaponSystem.md`, `UWeaponData`), so it needs one new material (**editor**) |
| 7 | **Animation in place** | In place by default; root motion only where a move is marked for it (CLAUDE.md). Keep only the in-place copy | Fights are fixed-position. CombatMaster's root-motion-only sets (DSAS V3, Twin Axe) wait for an in-place pass |
| 8 | **Naming** | New assets follow `ContentNaming.md`: `SM_` / `SK_` models with the type in the name (`SM_Sword_1H_...`), `AS_` clips, `AM_` montages, skills as `DA_<Family>_<Attack or Ability>_<Name>`. Pack assets keep their names (D-058) | A picker can filter by type; new files sit beside their siblings |
| 9 | **Icons** | One style, one size, clear background; the frame is drawn by the screen, not baked in. An adapter's icon is the frame with its crystal's icon inside (D-229) | Three styles are in use today; one icon plus a tint beats one per element per tier |
| 10 | **Heavy files** | New model, texture, sound and effect fields are loaded on demand (D-057) | Standing rule |

### Rule 1 in full: how a weapon model is built (Crown, 2026-11-17)

The standard fixes how the model itself is made, not only how it is attached. A model built to it sits right in the hand with no turn and no offset.

| Part of the rule | Value |
|---|---|
| **Origin** | At the grip, where the hand closes. The exact spot along the grip (its middle, or just under the guard): **to be set from the measurement** |
| **Length axis** | The model lies lengthways along one axis: **to be set from the measurement** |
| **Which end** | The tip, blade, head or barrel points to the positive end of that axis, or the negative: **to be set from the measurement** |
| **Edge or face** | The cutting edge, a bow's string side, a shield's face and a gun's top point along a second axis: **to be set from the measurement** |
| **Two-handed pieces** | The origin is at the hand that leads (default: the rear hand, which is the right); the other hand follows the animation |
| **Shields** | The origin is at the hand grip behind the face, or at the forearm strap: **to be set from the measurement** |
| **A model that does not match** | It is fixed once, in the model itself on import, not by a turn stored on the piece. Today's per-piece turn (`WeaponData.h:130,149`) stays only as the override for a piece that must sit differently |

**Where the values come from:** an editor worker is measuring the owned weapon models and the hand attach points on Manny and Quinn, and will write `docs/research/weapon-orientation.md`. That file did not exist when this was written (2026-11-17), so no axis or origin is stated here. The rule takes the **most common pattern among the owned models**, so the fewest have to be changed, and cites that file.

**The hand's attach point is the other half:** it must face the way the rule expects, the same on every body, or a correctly built model still sits wrong (rule 4).

**No Paragon weapon models are assumed.** Only Paragon animations are on disk as far as is known (shortlist §4; the editor worker is confirming). The models come from StylizedWeapons and the grey base-mesh pack (section 8).

## 7. Bodies

**Ruled:** Body is how a character looks ("Body of Orthos", D-241). Male Nobody and Female Nobody are Manny and Quinn (D-234). A new character's body takes the colour of its element or class, and the chest light too (D-238).

**Today:** the two bodies are fixed paths in code, and the colour is set through one setting called `Tint` plus three chest-light settings (`SpiritLook.cpp:32-38,68-72`).

**A new body must have:**

| Need | Rule |
|---|---|
| **Skeleton** | The UE5 Manny skeleton, which Quinn shares. Every animation the game owns plays on it |
| **Attach points** | The full list in section 6, rule 4, with the same names |
| **Colour settings** | `Tint` and the chest light, if the body is to show its element. A body with neither keeps its own look (section 11, question 6) |
| **Proportions** | Close enough to Manny that attack reach and hand positions hold (**editor**) |
| **Apparel** | Either it wears Manny's or Quinn's apparel, or apparel is hidden on it |

**Converting a body on its own skeleton** (every Paragon hero is one; none is on disk yet; shortlist §4.3):
1. Move the model onto Manny's skeleton, once, in the editor.
2. Add the standard attach points.
3. Decide its apparel: hidden, or a fitted version.
4. Check its reach against the fight's spacing.
5. Check whether its materials can take `Tint` (**editor**).

UE4-era packs (MageAnimationSet) go through one shared converter, set up once.

## 8. The twelve weapon types

| Type | Model | Animations | Pair set | Missing |
|---|---|---|---|---|
| Sword | StylizedWeapons | CombatMaster sword; Paragon Feng Mao | Two swords; with shield | Nothing |
| Greatsword | StylizedWeapons | CombatMaster; Paragon Greystone, Kwang | — | Nothing |
| Spear | StylizedWeapons polearms; grey spears | Paragon Wukong's staff clips as a stand-in (**editor**) | — | **Spear animations** |
| Staff | StylizedWeapons | Paragon Wukong; MageAnimationSet for casting | — | The UE4 conversion; an attach point |
| Dagger | StylizedWeapons | Paragon Kallari, Countess | Two daggers | Nothing (shares the sword's point today) |
| Axe | StylizedWeapons | CombatMaster axe; Paragon Terra | With shield; two axes by fallback | In-place two-axe clips |
| Hammer | StylizedWeapons | Paragon Grux, Narbash | Two hammers | A two-handed hammer; an attach point |
| Bow | StylizedWeapons (animated, own draw clips) | Paragon Sparrow | — | Left-hand grip, arrow point |
| Gauntlets | StylizedWeapons fist weapons | Paragon Crunch, Khaimera; CombatMaster | Gauntlets | An attach point |
| Scythe | **Grey model** only | Paragon Serath, a weak lead (**editor**) | — | **Scythe animations**; a painted model |
| Gun | **Grey model** only | Paragon Revenant, Murdock, Wraith | Two guns | A painted model; an attach point |
| Shield | StylizedWeapons | Paragon Steel, Terra; CombatMaster DSAS | With sword; with axe | Nothing (off hand only, B-515) |

## 9. A checklist for any new asset

1. **Name** follows `ContentNaming.md`, with the weapon type in it.
2. **Scale** is real size; it passes the length range for its type.
3. **Built to rule 1:** its origin is at the grip and it lies along the house axis, tip to the house end; held, it needs no turn and no offset.
4. **Kind:** static, unless the piece itself moves.
5. **Colour:** it uses the kind's one material; the element tint works; no copy per colour.
6. **Type:** its weapon type is set, so the template gives it a stance and an attack with nothing else filled in.
7. **Animations** play on the Manny skeleton, in place, with their hit moments marked.
8. **A body** carries every attach point and, if it shows an element, the colour settings.
9. **An icon** in the house style.
10. **Git:** only the assets the game uses are committed; the rest of the pack stays out (CLAUDE.md).
11. **Licence** read at its source before release (Paragon: no Paragon name or logo).

## 10. The order to put the placeholders in

The shortlist's order, with one step added in front. Each step in Crown's order: apply, Crown looks, align code and data, test the real path.

| # | Step | Crown sees | The problem it is meant to find |
|---|---|---|---|
| 0 | **The template itself,** on the sword and on sword and shield, from what is already in the game (added here; D-242's "look into first") | Nothing new: the same sword, now with no attack or stance on the piece | Whether the default-then-override order holds in the field and the fight; whether the shield's override fits |
| 1 | **Bow** (StylizedWeapons bow, Sparrow's clips) | A bow drawn and fired | An animated weapon in the left hand; the bow's own clip in time with the body's; the arrow point; reach in a fixed-position fight |
| 2 | **Ring and crystal** together | A ring on the hand in its element's colour | A ring attach point on both bodies; one model tinted nine ways; whether a ring can be seen at the fight's camera at all |
| 3 | **Apparel on both bodies** | One piece on Male and Female Nobody | One piece or one per body; clipping; tint; how weapons and rings sit over sleeves |
| 4 | **A Paragon body** with all of the above worn | A different body holding the same gear | The attach points on a new skeleton; apparel on a body it was not made for; saving the chosen body |
| 5 | **Staff** through the UE4 conversion, whenever | A staff cast | The one shared converter, so every UE4-era pack becomes usable |
| — | **Icons,** any time | Every screen filled | The size and frame each screen expects |

## 11. Open questions for Crown

| # | Question | Default, and why |
|---|---|---|
| 1 | **Both pieces of a pair carry an override: which wins?** | **The off hand's.** Crown's words: the main hand, "unless the shield has an override" |
| 2 | **Does a named pair template bring its own standard attack** (a two-sword swing), or is a pair's attack always the main hand's unless a piece overrides? | **The pair template may bring one.** Elden Ring: two weapons of one type switch to a paired moveset on their own, and it is well liked |
| 3 | **Does every shield bring the paired attack,** or only shields given one? | **Every shield, through the shield type's template,** for the main hands that have clips (sword, axe). Monster Hunter: sword and shield is one moveset whatever the shield |
| 4 | **Attach points: one per hand, or one per weapon type as today?** | **One per hand, the grip on the template.** Epic's Lyra sample holds an attach point and an offset on each item, so a new body needs few points |
| 5 | **Rings in a fight: a model, or a glow?** | **A glow on the hand in the element's colour,** with the model kept for the menu. Elden Ring and Dark Souls draw no rings at all |
| 6 | **An unlocked body ("Body of Orthos"): does it show the element's colour?** D-238 gives colour to the base body "until a spirit is worn over it" (now: until another body is worn, D-241) | **No: it keeps its own look.** Destiny 2: armour keeps its look and the element shows in what you cast |
| 7 | **Put away: hidden, or shown on the back?** | **Hidden, as today.** Genshin Impact hides a weapon when it is put away; it avoids clipping with every outfit |
| 8 | **The first set: one attack and three skills per template?** | **Yes.** Hades: each weapon starts with a small, distinct set and grows from there |
| 9 | **Spear and scythe have no real animations.** Stand-ins, or leave them out of the pools? | **Spear on Wukong's staff clips; scythe left out until its clips are checked.** No game to cite: it follows "placeholders first" (D-242) and buys nothing |

**Only the editor can confirm:** the grip, centre and facing of every pack model; the attach points the packs' demo bodies use; scale inside each pack; whether each Paragon clip fits its label (Serath, Wukong as a spear, Yin); how many distinct moves a hero really gives; whether StylizedWeapons can take a tint; whether one apparel piece fits both bodies; whether a Paragon body's materials can take `Tint`.

## Clashes found

1. **D-242 calls `OverrideAttack` unused.** It is read in the field and in the fight, and it is on the loadout entry, not on a piece (section 3). An override that travels with a shield needs a field on the piece.
2. **D-242's pairs against its own standard-attack line:** sword and shield "gets a named pair template", yet a pair's standard attack "is the main hand's unless the off-hand piece carries an override". Which one supplies the sword-and-shield attack is not said. Questions 2 and 3.
3. **D-238's wording against D-241:** D-238 still says a "spirit" is worn over the body. Since D-241 that is a body. Not a live clash; D-238 reads through D-241. Question 6 is the part it leaves open.
4. **D-209 against the pair table:** D-209 allows same-type pairs only, while Terra's axe and shield and the shield rule (B-515) pair across types. Settled in code already: a shield is the one allowed cross-type off hand.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-17 | Created from D-242, the asset shortlist and the weapon code as it stands | dev (docs only) |
| 2026-11-17 | Crown's addition: rule 1 fixes how a weapon model is built (origin at the grip, one length axis, one end); the values wait for `docs/research/weapon-orientation.md` | dev (docs only) |
