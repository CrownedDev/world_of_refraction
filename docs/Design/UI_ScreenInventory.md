# UI Screen Inventory — title screen to end-game

Every screen, overlay and HUD element World of Refraction needs, mapped to the systems that already exist in source. Each entry has a build status and an asset list, so assets can be sourced against a known scope.

Status is checked against `main` on 2026-10-25. Screens added on the `refactor/bp-logic-to-cpp` branch (not yet merged) are marked *(branch)*.

**Status**
- **Exists** — C++ base and WBP in the repo.
- **C++ only** — logic exists, no widget.
- **Partial**
- **None** — nothing yet.

---

## Screen list (canonical)

The full list, in the order a player meets them. The sections below give each item's current status and asset needs.

**Front end**
1. Splash / logos
2. Title screen
3. Main menu
4. Save slot select
5. New game / character select (class + element)
6. Settings (graphics, audio, controls, accessibility)
7. Loading screen
8. Confirm / message dialog
9. Credits

**Hub**
10. Hub HUD (currencies, party portraits)
11. Interaction prompt ("[E] Enter / Talk / Invite")
12. Merchant shop
13. Party management
14. Recruit dialog / character card
15. Trial select (door)
16. NPC dialogue box

**Menus (gear & progression)**
17. Pause / game menu
18. Character sheet (stats, pillars, resistances)
19. Inventory
20. Loadout / equip
21. Crystal attachment
22. Fusion / augment stones
23. Workshop (dismantle / merge / level-up / downgrade)
24. Spell & ability book
25. World-stat draft (pick 3 of 5)
26. Reroll
27. Codex / element chart

**Trial**
28. Trial HUD
29. Encounter warning / join countdown
30. Battle transition effect

**Combat**
31. Character panels (HP / EP / status bars)
32. Turn-order strip
33. Command menu + submenus
34. Target selection / in-world marker
35. Skill tooltip
36. Defense prompt (block / parry / dodge timing)
37. Damage / heal numbers
38. Status-effect icons
39. Element proc banner ("BURN!", "STUN!")
40. Broken Darkness gauges
41. Weather indicator
42. Combat log
43. Combat pause
44. Results (victory / defeat)
45. Loot / rewards
46. Retry / return
47. Tutorial callouts

**Online / later**
48. Lobby / co-op invite
49. Matchmaking / PvP challenge
50. Leaderboards
51. Store / cosmetics
52. Achievements

**Shared parts every screen uses**
- Fonts
- Panel frames
- Buttons
- Bars
- Tooltips
- Scrollbars / list rows
- Cursor
- Icons: elements, classes, portraits, weapons, gems, stones, stats, status effects, currencies, commands, input glyphs
- UI sounds

---

## 0. Global UI kit (needed by everything — source this first)

One coherent kit. Every screen below is built from these parts, so buy or commission them as a set.

| Part | Notes |
|---|---|
| **Fonts** | 1 display/title face (stylised, fits "refraction"/prismatic) + 1 readable body face with a bold weight. Must cover digits/symbols for damage numbers. Check the licence allows game embedding. |
| **Panel frames** | 9-slice frames in 3 weights: heavy (modal windows), medium (side panels, character cards), light (tooltips, rows). Plus a dimmed full-screen backdrop. |
| **Buttons** | Primary / secondary / icon-only / tab. States: normal, hover, pressed, disabled, selected. Prefer 9-slice. |
| **Bars** | HP, EP, status buildup, durability, BD absorption. Each needs frame + fill + "damage ghost" fill. Fills should take a tint, because the code tints EP and status bars per element (`CharacterPanelWidget.cpp:282-314`). |
| **Scroll / list** | Scrollbar, list-row backgrounds (alternating + selected), dividers. |
| **Tooltip** | Small frame + pointer, with header/body/stat-line styles. |
| **Cursor / focus** | Mouse cursor and a controller focus highlight (for gamepad support later). |
| **Element palette** | One canonical colour per element. **Lock it before buying icons.** The code already defines element colours (`Infusion/ElementColors.h`, `HybridSpellColors.h`); match the art to them, or update the code to match the art. |
| **UI SFX** | Hover, click, confirm, cancel, error, open/close panel, tab switch, purchase, level-up, turn-start ping, crit, defense success, defense fail. Roughly 15–20 sounds. |
| **Transitions** | Fade-to-black, wipe or prism-shatter transition material for level loads and battle entry. |

---

## 1. Boot & front-end

| # | Screen | Status | What it shows / does | Asset needs |
|---|---|---|---|---|
| 1.1 | **Splash / logos** | None | Studio logo, engine logo, legal | Studio logo, UE logo (per licence), short sting SFX |
| 1.2 | **Title screen** | None | Game logo, "Press any key", animated backdrop | **Game logo** (wordmark + emblem), key art / animated backdrop (prism scene), title music |
| 1.3 | **Main menu** | None | Continue / New Game / Load / Settings / Credits / Quit | Menu backdrop (can reuse title), button set |
| 1.4 | **Save slot select** | None — **no save system exists** (review §4) | Slots with character portrait, level, playtime, location | Slot frame, empty-slot art. *Blocked on Save/Persistence keystone.* |
| 1.5 | **New game / character select** | None | Choose starting class (Generic / Caster / Resonator) + innate element. Player identity is `DA_Character_Template`. | 3 **class emblems**, class splash art or 3D preview stage, 9 element emblems |
| 1.6 | **Settings** | None | Graphics, audio volumes, controls/rebinding (Enhanced Input), language, accessibility (text size, colour-blind element markers) | Tab icons, slider, toggle and dropdown widgets from the kit |
| 1.7 | **Credits** | None | Scrolling credits | — |
| 1.8 | **Loading screen** | None. `OpenLevel` is used for hub ↔ trial ↔ battle with no screen. | Tip text, element lore art, spinner | 3–6 loading illustrations, spinner/prism animation |
| 1.9 | **Confirm / message dialog** | None | Generic modal: "Quit?", "Not enough Prisms", errors | Heavy frame, 2-button layout |

---

## 2. Hub (out-of-combat)

| # | Screen | Status | What it shows / does | Asset needs |
|---|---|---|---|---|
| 2.1 | **Hub HUD** | None. Hints today are debug on-screen text (`TrialDoor.cpp:144`, `MerchantInteractable.cpp:128`). | Currency strip (Prisms, Diamond, Gold), party portraits (up to 3, `Party.h:27`), compass/minimap (optional) | **Currency icons ×5** (Gold, Prisms, Diamond, Gear Essence, Skill Essence) + typed-essence icons per element/pillar (`CurrencyTypes.h`), small portrait frames |
| 2.2 | **Interaction prompt** | Debug text only. C++ interact exists *(branch: `AExplorationPlayerController`)*. | "[E] Enter Trial: X", "[E] Talk", "[E] Invite / Dismiss" above the target | Key-glyph icons (keyboard + gamepad), small floating frame |
| 2.3 | **Merchant shop** | **Exists** (`ShopWindowWidget`, `WBP_ShopWindow`, stock/cart rows) | Stock list, detail panel, cart, totals, purchase toast | **Item icons** (see §7). Merchant portrait per merchant. Tier badges F–S. |
| 2.4 | **Party management** | C++ only (`UPartySessionSubsystem`, `OnPartyChanged`) | View, reorder and dismiss members; leader slot locked | Character **portraits** (13 characters: 9 element lords + Generic, Resonator, BD, Template), role/class emblems |
| 2.5 | **Recruit dialog** | Walk-up E toggles membership instantly, with no UI | Character card: name, class, element, stats; Invite/Cancel | Character card frame, full-body or bust art per recruitable |
| 2.6 | **Trial select (door)** | Door → `EnterTrial`, no UI. `UTrialData` is one level per door; multi-level-per-door is banked. | Trial name, element, difficulty, recommended level, rewards preview | **Trial banners** per element (9), difficulty markers |
| 2.7 | **Dialogue / NPC talk** | None | Speaker portrait + text box + choices | Dialogue box, nameplate, portrait frame |

---

## 3. Gear & progression (the "menu" layer)

This is the **largest gap**. The systems exist in C++ but there is no UI. The equip UI also blocks per-instance rolls from ever reaching combat (`PerInstanceRollSystem.md`; review §5.1 #10, #11).

| # | Screen | Status | What it shows / does | Asset needs |
|---|---|---|---|---|
| 3.1 | **Pause / game menu** | None | Resume, Party, Inventory, Loadout, Settings, Quit to title | Tab bar icons |
| 3.2 | **Character sheet** | C++ only (`CharacterDataComponent`, world stats M/B/S) | Portrait, class, element, HP/EP, world pillars, 12 substats, resistances (`ResistanceSystem.md`) | **Stat icons**: 3 pillars (Mind/Body/Spirit) + ~12 substats (RawDamage, Defense, CritDamage, TurnSpeed, StatusMultiplier, Efficiency, SpellDamage, Resistance, SpellSpeed, ActionSpeed, Luck, Reflex), resistance icons per element + physical |
| 3.3 | **Inventory** | C++ only (`UInventoryComponent`); no UI | Owned weapons, rings, crystals, evolutions, stones, spells/abilities; filter/sort | **All item icons** (§7), tier frames F–S, rarity/roll indicators |
| 3.4 | **Loadout / equip** | C++ only (`ULoadoutComponent`, multiple saved loadouts). **Banked as "Player equip UI — BLOCKED"** | Slots per class:<br>- Generic: primary + secondary weapon (dual toggle)<br>- Caster: weapon + ring + innate spells<br>- Resonator: ring array + switching<br>Plus item slots, loadout create/rename/duplicate. Must forbid the same instance in two slots (TODO.md double-reference hazard). | Slot frames per slot type, empty-slot silhouettes, class-specific layout art |
| 3.5 | **Crystal / attachment** | C++ only (`InventoryComponent::Attach*` — BP-callable, zero C++ callers) | Attach crystal, evolution or fusion to weapon/ring. Durability and wear display (`CrystalWear.md`). | **Crystal icons 10 gems × 7 tiers** (or 10 + tier frame overlay), **17 stone icons**, fusion indicator, durability bar |
| 3.6 | **Fusion / augment** | C++ only (`AugmentStoneSystem.md`) | Combine gem + stone, pick BonusStat, preview result | Fusion animation/VFX, combined-icon treatment |
| 3.7 | **Workshop: dismantle / merge / level-up / downgrade** | C++ only (`UEconomyService`, all BP-callable, zero C++ callers) | Item in, cost/yield preview, confirm | Essence icons, "+1 level" and dismantle VFX, cost lines |
| 3.8 | **Spell & ability book** | C++ only (`USpellData` / `UAbilityData`) | Learned skills, school, element, tier, cost, scaling grade (`ScalingSystem.md`) | **Skill icons** (§7), school icons, scaling-grade letters (S/A/B…) |
| 3.9 | **World-stat draft (5-pick-3)** | Event only (`OnWorldStatDraftReady`). Never fired on a win until the Arc A fix. | Pick 3 of 5 world-stat boosts after a win | Pillar icons (reuse 3.2), card frames |
| 3.10 | **Reroll (Pool)** | Storage only. Economy unbuilt (TODO.md). | Spend charged pool to reroll an instance's stats | Reroll VFX, pool gauge |
| 3.11 | **Codex / element chart** | None | The 9 elements, their status effect on cap (Burn, HealBlock, Stun, …), class differences.  | 9 **element emblems** (hero quality), status-effect icons, short loop VFX per element |

---

## 4. Trial (exploration between fights)

| # | Screen | Status | What it shows / does | Asset needs |
|---|---|---|---|---|
| 4.1 | **Trial HUD** | None | Trial name/element, objective, party HP mini-bars, return-door hint | Reuses hub HUD parts |
| 4.2 | **Encounter warning / join window** | Trigger + join window exist (`UEncounterComponent`), no UI | "Enemy ahead"; countdown for party members to join the arena sphere | Warning banner, countdown ring, arena-bubble VFX (Void bubble exists) |
| 4.3 | **Battle transition** | `OpenLevel` swap, no UI | Stylised transition into battle (prism shatter / swirl) | Transition material/flipbook + SFX |

---

## 5. Combat

| # | Element | Status | What it shows / does | Asset needs |
|---|---|---|---|---|
| 5.1 | **Character panels** | **Exists** (`UCharacterPanelWidget`, `WBP_CharacterPanel`). Native effects list, static text and death dim *(branch)*. | HP / EP (or ring/weapon durability / BD absorption) / status-buildup bars, name, class + element, world stats, active effects list | Portrait/thumbnail per character, bar set (§0), element tints, death state |
| 5.2 | **Turn-order strip** | **Exists** (`UTurnOrderStripWidget` + slot, 1 + 4 preview). Native slot binding *(branch)*. | Current actor + upcoming turns; bonus-turn badge (Emerald) | **Turn portraits** (small, circular or diamond), active highlight, bonus badge, Execution-turn (ritual) marker |
| 5.3 | **Command menu** | **Exists** (`UCombatCommandMenuSubsystem`, `WBP_CombatMenu`, pooled buttons, durability header) | Attack / Ability / Spell / Item / Defend / Switch Ring / Weapon Toggle; submenus; infusion level cycle | **Command icons** (~8), button frames, infusion-level pips (L0/L1/L2), per-element button tints |
| 5.4 | **Targeting** | Menu side exists (`OpenTargetSelection`, group targets). In-world highlight unknown (BP). Double-targeting TODO. | Pick enemy/ally/group; show predicted damage | **World target marker** (ground ring / reticle decal), selection outline material, group-target highlight |
| 5.5 | **Skill info / tooltip** | None | Cost (EP, durability, HP for infusion), element, delivery, scaling, effects | Tooltip frame (§0), cost icons |
| 5.6 | **Defense prompt** | Stub on main; native prompt *(branch)* | "Block / Parry / Dodge" timing window during enemy attacks; result text | **Input glyphs**, timing ring/bar that shrinks over the window, success/perfect/fail stamps + SFX |
| 5.7 | **Damage / heal numbers** | **None.** `OnDamageDealt(Attacker, Target, Damage, bCritical)` exists but carries no element (`ActionExecutor.h:82`). | Floating numbers: normal, crit, heal, DOT tick, absorbed, reflected, "Miss/Parried" | Number font (bold, outlined), crit treatment, element-coloured variants |
| 5.8 | **Status-effect icons** | Text rows only | Icon row per character: buffs, debuffs, DOTs, stacks, turns left | **Status icon set**: ~40 icons covers the visible effect families (stat buff/debuff per substat, 9 element DOT/cap effects, Stun, Silence, SkipTurn, HealBlock, LastStand, Lifesteal, Reflect, Absorb, Charges). 108 effect types exist (`ESkillEffectType`), but most share a family icon + tint. |
| 5.9 | **Status-cap / element proc banner** | None | Big moment when a bar caps: "BURN!", "STUN!", "HEAL BLOCK!" with element flair | 9 element proc banners/VFX |
| 5.10 | **Broken Darkness gauges** | Partial (panel bar + synthetic stacks row) | Absorption energy, alignment element, stacks, overload state | Overload meter, alignment emblem swap, stack pips |
| 5.11 | **Weather indicator** | Weather system exists; sky driven by `BP_WeatherController` | Current weather/dominance between leaders | Weather icons |
| 5.12 | **Combat log** | None (`UE_LOG` only) | Scrollable action log | Log frame, small element icons |
| 5.13 | **Pause (combat)** | None | Resume / Settings / Flee (Flee action is a stub) | Reuses 3.1 |
| 5.14 | **Results** | Native results screen *(branch)*. Main exits one tick after combat ends. | Victory / Defeat / Draw, turns, survivors; later XP, loot, WSP | Victory and defeat **title treatments**, results backdrop, stinger music ×2 |
| 5.15 | **Loot / rewards** | None (no reward source on win) | Items and currencies earned, per-instance roll reveal | Item icons, roll-reveal animation, rarity glow |
| 5.16 | **Defeat / retry** | None (defeat = return to trial reload) | Retry / Return to hub | — |
| 5.17 | **Tutorial overlays** | None | First-time callouts: turn order, defense timing, element caps, infusion | Callout arrow/frame, highlight mask |

---

## 6. Online & later

| Screen | Notes |
|---|---|
| Online lobby / co-op party invite | Needs networking (review §4: replication ~1/5) |
| Matchmaking / PvP Lord-vs-Contender challenge | Roadmap "Networked multiplayer + PvP" |
| Leaderboards / seasons | Live-service layer |
| Store / cosmetics | `UCosmeticsData` exists (`DA_Cosmetics_Default`) |
| Account / Prisms & Diamond wallet | Account-scope currency routing is TODO (`CurrencyComponent.h:21-23`) |
| Achievements, photo mode, accessibility suite | Polish |

---

## 7. Icon & art shopping list (consolidated)

Counts are taken from the source enums, so they are the real scope.

| Set | Count | Source of truth | Tip |
|---|---|---|---|
| **Element emblems** | 9 + Broken Darkness (+ Generic/None neutral) | `ESpellElement.h` (Fire, Water, Earth, Wind, Light, Darkness, Lightning, Void, Reality) | Hero quality: used on menus, codex, banners. Also make small 32px glyph versions. |
| **Class emblems** | 3 | `ECharacterClass` (Generic, Caster, Resonator) | |
| **Character portraits** | 13 (bust + small round) | `DA_Character_*` (9 element lords, Generic, Resonator, BD lord, Template/player) | Panel thumb, turn-strip thumb, party card, recruit card |
| **Weapon type icons** | 11 | `EWeaponType` (Sword, Greatsword, Spear, Staff, Dagger, Axe, Hammer, Bow, Gauntlets, Scythe, Gun) | Plus per-asset icons later. ~301 pool assets have no icon (Roadmap "Icons pass"); start with one per type. |
| **Ring icons** | 9 element rings + generic | `DA_Ring_*` | Base ring + gem-colour swap |
| **Crystal (gem) icons** | 10 gems | `ECrystalType` (Garnet, Sapphire, Citrine, Emerald, Amber, Opal, Onyx, Amethyst, Iolite, Quartz) | One icon per gem + **7 tier frames/glows** (F → S) applied on top, not 70 icons |
| **Stone icons** | 17 | `ECrystalType` *Stone entries | Shared stone silhouette + stat glyph = cheap and readable |
| **Evolution icons** | per evolution asset | `UEvolutionItemData` | |
| **Stat icons** | 3 pillars + 12 substats + resistance per element + physical | `StoneTargetStat`, `EWorldPillar` | |
| **Status-effect icons** | ~40 families | `ESkillEffectType` (108 values) | Buff = up-arrow frame, debuff = down-arrow frame, DOT = drip frame; family glyph inside |
| **Command icons** | ~8 | Command menu | Attack, Ability, Spell, Item, Defend, Switch, Infuse, Back |
| **Currency icons** | 5 + typed essences | `ECurrencyType`, `EEssenceType` | |
| **Tier badges** | 7 | `EItemTier` (F–S) | Letter + colour ramp |
| **Input glyphs** | KB/M + Xbox + PS sets | Enhanced Input | Buy a standard glyph pack |
| **Skill icons** | per spell/ability | `Content/Data/Pool/...` (12 per weapon-type × damage type, plus spells) | Biggest volume. Consider one per *school × element* first, then bespoke per skill. |

---

## Decisions & open questions (they change asset scope)

| # | Question | Answer (2026-10-25) |
|---|---|---|
| 1 | **Art direction?** Painterly, cel-shaded or clean sci-fi? The kit and icons must match. | **Provisional: Clair Obscur: Expedition 33 layout and restraint, crystal-themed.** See *Art direction reference* below. Mockups to follow. |
| 2 | **Gamepad support?** | **Yes.** Every screen needs a controller focus path + focus highlight; buy controller glyphs (Xbox + PlayStation) alongside keyboard/mouse. |
| 3 | **2D portraits or live 3D renders** for panels, turn order and party cards? | **Either.** Undecided; source assets that work with both (portrait frames that accept a texture or a render target). |
| 4 | **Floating or flat HUD?** World-space nameplates and bars over characters, or screen-space panels? | **Provisional: hybrid (Expedition 33 / Destiny).** Party and turn order flat at the screen edges; enemy name + bars near the enemy; bosses get a flat top banner; damage numbers and proc banners in-world. Flat panels remain the current build. |

---

## Art direction reference (provisional, 2026-10-25)

Reference: **Clair Obscur: Expedition 33** combat HUD, re-themed around crystals and refraction. To be validated with mockups before buying the kit.

### Layout (from the Expedition 33 reference)

| Element | Placement | World of Refraction mapping |
|---|---|---|
| Turn order | Vertical portrait strip, **left edge** | `UTurnOrderStripWidget`, turned vertical. Bonus-turn (Emerald) and ritual (Execution-turn) badges on portraits. |
| Enemy info | Name + HP **near the enemy** (top-centre / over target) | Floating enemy plate: name, HP, status-buildup bar tinted to the pending element, small status icons |
| Party | Compact portrait cards, **bottom-right** | Slimmed `UCharacterPanelWidget`: portrait, HP, EP/durability, status bar, effect icons |
| Commands | **Face-button diamond** (right side) + skill list panel | Command menu as a gamepad diamond (Attack / Skill / Item / Defend); skill list shows EP cost, element, infusion level |
| Input glyphs | On every action | Required anyway (gamepad = yes) |
| World | Kept clear; HUD only at edges + on the target | Same |

### Crystal theme

- **Frames:** thin metallic linework (silver / pale gold) around **faceted glass panels**. Subtle refraction or rainbow edge on highlights. Restrained, not neon.
- **Portraits:** **gem-cut frames** (hexagon / elongated diamond) for turn order and party cards.
- **Element colour:** each frame's inner glow takes the character's element colour (the code already tints bars per element: `Infusion/ElementColors.h`).
- **Bars:** fills read as light inside a crystal tube; the damage ghost shows as cracks.
- **Moments:** status cap ("BURN!"), defense success, crit → **shard burst / prism flash**. Turn change refracts across the strip.
- **Fonts:** elegant serif or engraved display face for names and titles; clean sans for numbers.

### Asset search terms

"fantasy crystal UI kit", "glass / gem RPG HUD", "faceted UI frames", "hexagon portrait frame", "prismatic VFX pack", "shard burst Niagara", "ornate RPG UI (Expedition 33 style)".

### Next step

Mockups of the combat HUD (and title / main menu) in this direction before committing to a kit.
