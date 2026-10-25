# UI Screen Inventory — title screen to end-game

Every screen, overlay and HUD element World of Refraction needs, mapped to the systems that already exist in source. Each entry has a build status and an asset list, so assets can be sourced against a known scope.

Status is checked against `main` on 2026-10-25. Screens added on the `refactor/bp-logic-to-cpp` branch (not yet merged) are marked *(branch)*.

**Priority**
- **P0** — needed for the publisher pitch demo: title → hub → one trial fight → results.
- **P1** — needed for a playable loop (progression, gear, saves).
- **P2** — long-term (networking, PvP, live features).

**Status**
- **Exists** — C++ base and WBP in the repo.
- **C++ only** — logic exists, no widget.
- **Partial**
- **None** — nothing yet.

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

| # | Screen | P | Status | What it shows / does | Asset needs |
|---|---|---|---|---|---|
| 1.1 | **Splash / logos** | P0 | None | Studio logo, engine logo, legal | Studio logo, UE logo (per licence), short sting SFX |
| 1.2 | **Title screen** | P0 | None | Game logo, "Press any key", animated backdrop | **Game logo** (wordmark + emblem), key art / animated backdrop (prism scene), title music |
| 1.3 | **Main menu** | P0 | None | Continue / New Game / Load / Settings / Credits / Quit | Menu backdrop (can reuse title), button set |
| 1.4 | **Save slot select** | P1 | None — **no save system exists** (review §4) | Slots with character portrait, level, playtime, location | Slot frame, empty-slot art. *Blocked on Save/Persistence keystone.* |
| 1.5 | **New game / character select** | P1 | None | Choose starting class (Generic / Caster / Resonator) + innate element. Player identity is `DA_Character_Template`. | 3 **class emblems**, class splash art or 3D preview stage, 9 element emblems |
| 1.6 | **Settings** | P0 (minimal) | None | Graphics, audio volumes, controls/rebinding (Enhanced Input), language, accessibility (text size, colour-blind element markers) | Tab icons, slider, toggle and dropdown widgets from the kit |
| 1.7 | **Credits** | P2 | None | Scrolling credits | — |
| 1.8 | **Loading screen** | P0 | None. `OpenLevel` is used for hub ↔ trial ↔ battle with no screen. | Tip text, element lore art, spinner | 3–6 loading illustrations, spinner/prism animation |
| 1.9 | **Confirm / message dialog** | P0 | None | Generic modal: "Quit?", "Not enough Prisms", errors | Heavy frame, 2-button layout |

---

## 2. Hub (out-of-combat)

| # | Screen | P | Status | What it shows / does | Asset needs |
|---|---|---|---|---|---|
| 2.1 | **Hub HUD** | P0 | None. Hints today are debug on-screen text (`TrialDoor.cpp:144`, `MerchantInteractable.cpp:128`). | Currency strip (Prisms, Diamond, Gold), party portraits (up to 3, `Party.h:27`), compass/minimap (optional) | **Currency icons ×5** (Gold, Prisms, Diamond, Gear Essence, Skill Essence) + typed-essence icons per element/pillar (`CurrencyTypes.h`), small portrait frames |
| 2.2 | **Interaction prompt** | P0 | Debug text only. C++ interact exists *(branch: `AExplorationPlayerController`)*. | "[E] Enter Trial: X", "[E] Talk", "[E] Invite / Dismiss" above the target | Key-glyph icons (keyboard + gamepad), small floating frame |
| 2.3 | **Merchant shop** | P0 | **Exists** (`ShopWindowWidget`, `WBP_ShopWindow`, stock/cart rows) | Stock list, detail panel, cart, totals, purchase toast | **Item icons** (see §7). Merchant portrait per merchant. Tier badges F–S. |
| 2.4 | **Party management** | P1 | C++ only (`UPartySessionSubsystem`, `OnPartyChanged`) | View, reorder and dismiss members; leader slot locked | Character **portraits** (13 characters: 9 element lords + Generic, Resonator, BD, Template), role/class emblems |
| 2.5 | **Recruit dialog** | P1 | Walk-up E toggles membership instantly, with no UI | Character card: name, class, element, stats; Invite/Cancel | Character card frame, full-body or bust art per recruitable |
| 2.6 | **Trial select (door)** | P0 | Door → `EnterTrial`, no UI. `UTrialData` is one level per door; multi-level-per-door is banked. | Trial name, element, difficulty, recommended level, rewards preview | **Trial banners** per element (9), difficulty markers |
| 2.7 | **Dialogue / NPC talk** | P2 | None | Speaker portrait + text box + choices | Dialogue box, nameplate, portrait frame |

---

## 3. Gear & progression (the "menu" layer)

This is the **largest gap**. The systems exist in C++ but there is no UI. The equip UI also blocks per-instance rolls from ever reaching combat (`PerInstanceRollSystem.md`; review §5.1 #10, #11).

| # | Screen | P | Status | What it shows / does | Asset needs |
|---|---|---|---|---|---|
| 3.1 | **Pause / game menu** | P0 | None | Resume, Party, Inventory, Loadout, Settings, Quit to title | Tab bar icons |
| 3.2 | **Character sheet** | P1 | C++ only (`CharacterDataComponent`, world stats M/B/S) | Portrait, class, element, HP/EP, world pillars, 12 substats, resistances (`ResistanceSystem.md`) | **Stat icons**: 3 pillars (Mind/Body/Spirit) + ~12 substats (RawDamage, Defense, CritDamage, TurnSpeed, StatusMultiplier, Efficiency, SpellDamage, Resistance, SpellSpeed, ActionSpeed, Luck, Reflex), resistance icons per element + physical |
| 3.3 | **Inventory** | P1 | C++ only (`UInventoryComponent`); no UI | Owned weapons, rings, crystals, evolutions, stones, spells/abilities; filter/sort | **All item icons** (§7), tier frames F–S, rarity/roll indicators |
| 3.4 | **Loadout / equip** | **P0→P1** | C++ only (`ULoadoutComponent`, multiple saved loadouts). **Banked as "Player equip UI — BLOCKED"** | Slots per class:<br>- Generic: primary + secondary weapon (dual toggle)<br>- Caster: weapon + ring + innate spells<br>- Resonator: ring array + switching<br>Plus item slots, loadout create/rename/duplicate. Must forbid the same instance in two slots (TODO.md double-reference hazard). | Slot frames per slot type, empty-slot silhouettes, class-specific layout art |
| 3.5 | **Crystal / attachment** | P1 | C++ only (`InventoryComponent::Attach*` — BP-callable, zero C++ callers) | Attach crystal, evolution or fusion to weapon/ring. Durability and wear display (`CrystalWear.md`). | **Crystal icons 10 gems × 7 tiers** (or 10 + tier frame overlay), **17 stone icons**, fusion indicator, durability bar |
| 3.6 | **Fusion / augment** | P1 | C++ only (`AugmentStoneSystem.md`) | Combine gem + stone, pick BonusStat, preview result | Fusion animation/VFX, combined-icon treatment |
| 3.7 | **Workshop: dismantle / merge / level-up / downgrade** | P1 | C++ only (`UEconomyService`, all BP-callable, zero C++ callers) | Item in, cost/yield preview, confirm | Essence icons, "+1 level" and dismantle VFX, cost lines |
| 3.8 | **Spell & ability book** | P1 | C++ only (`USpellData` / `UAbilityData`) | Learned skills, school, element, tier, cost, scaling grade (`ScalingSystem.md`) | **Skill icons** (§7), school icons, scaling-grade letters (S/A/B…) |
| 3.9 | **World-stat draft (5-pick-3)** | P1 | Event only (`OnWorldStatDraftReady`). Never fired on a win until the Arc A fix. | Pick 3 of 5 world-stat boosts after a win | Pillar icons (reuse 3.2), card frames |
| 3.10 | **Reroll (Pool)** | P2 | Storage only. Economy unbuilt (TODO.md). | Spend charged pool to reroll an instance's stats | Reroll VFX, pool gauge |
| 3.11 | **Codex / element chart** | P0 (pitch) | None | The 9 elements, their status effect on cap (Burn, HealBlock, Stun, …), class differences. **The pitch's teaching screen.** | 9 **element emblems** (hero quality), status-effect icons, short loop VFX per element |

---

## 4. Trial (exploration between fights)

| # | Screen | P | Status | What it shows / does | Asset needs |
|---|---|---|---|---|---|
| 4.1 | **Trial HUD** | P0 | None | Trial name/element, objective, party HP mini-bars, return-door hint | Reuses hub HUD parts |
| 4.2 | **Encounter warning / join window** | P0 | Trigger + join window exist (`UEncounterComponent`), no UI | "Enemy ahead"; countdown for party members to join the arena sphere | Warning banner, countdown ring, arena-bubble VFX (Void bubble exists) |
| 4.3 | **Battle transition** | P0 | `OpenLevel` swap, no UI | Stylised transition into battle (prism shatter / swirl) | Transition material/flipbook + SFX |

---

## 5. Combat (the pitch-critical layer)

| # | Element | P | Status | What it shows / does | Asset needs |
|---|---|---|---|---|---|
| 5.1 | **Character panels** | P0 | **Exists** (`UCharacterPanelWidget`, `WBP_CharacterPanel`). Native effects list, static text and death dim *(branch)*. | HP / EP (or ring/weapon durability / BD absorption) / status-buildup bars, name, class + element, world stats, active effects list | Portrait/thumbnail per character, bar set (§0), element tints, death state |
| 5.2 | **Turn-order strip** | P0 | **Exists** (`UTurnOrderStripWidget` + slot, 1 + 4 preview). Native slot binding *(branch)*. | Current actor + upcoming turns; bonus-turn badge (Emerald) | **Turn portraits** (small, circular or diamond), active highlight, bonus badge, Execution-turn (ritual) marker |
| 5.3 | **Command menu** | P0 | **Exists** (`UCombatCommandMenuSubsystem`, `WBP_CombatMenu`, pooled buttons, durability header) | Attack / Ability / Spell / Item / Defend / Switch Ring / Weapon Toggle; submenus; infusion level cycle | **Command icons** (~8), button frames, infusion-level pips (L0/L1/L2), per-element button tints |
| 5.4 | **Targeting** | P0 | Menu side exists (`OpenTargetSelection`, group targets). In-world highlight unknown (BP). Double-targeting TODO. | Pick enemy/ally/group; show predicted damage | **World target marker** (ground ring / reticle decal), selection outline material, group-target highlight |
| 5.5 | **Skill info / tooltip** | P0 | None | Cost (EP, durability, HP for infusion), element, delivery, scaling, effects | Tooltip frame (§0), cost icons |
| 5.6 | **Defense prompt** | P0 | Stub on main; native prompt *(branch)* | "Block / Parry / Dodge" timing window during enemy attacks; result text | **Input glyphs**, timing ring/bar that shrinks over the window, success/perfect/fail stamps + SFX |
| 5.7 | **Damage / heal numbers** | P0 | **None.** `OnDamageDealt(Attacker, Target, Damage, bCritical)` exists but carries no element (`ActionExecutor.h:82`). | Floating numbers: normal, crit, heal, DOT tick, absorbed, reflected, "Miss/Parried" | Number font (bold, outlined), crit treatment, element-coloured variants |
| 5.8 | **Status-effect icons** | P0 | Text rows only | Icon row per character: buffs, debuffs, DOTs, stacks, turns left | **Status icon set**: ~40 icons covers the visible effect families (stat buff/debuff per substat, 9 element DOT/cap effects, Stun, Silence, SkipTurn, HealBlock, LastStand, Lifesteal, Reflect, Absorb, Charges). 108 effect types exist (`ESkillEffectType`), but most share a family icon + tint. |
| 5.9 | **Status-cap / element proc banner** | P0 (pitch) | None | Big moment when a bar caps: "BURN!", "STUN!", "HEAL BLOCK!" with element flair | 9 element proc banners/VFX — **the most pitch-visible UI in the game** |
| 5.10 | **Broken Darkness gauges** | P0 if BD is in the demo | Partial (panel bar + synthetic stacks row) | Absorption energy, alignment element, stacks, overload state | Overload meter, alignment emblem swap, stack pips |
| 5.11 | **Weather indicator** | P1 | Weather system exists; sky driven by `BP_WeatherController` | Current weather/dominance between leaders | Weather icons |
| 5.12 | **Combat log** | P1 | None (`UE_LOG` only) | Scrollable action log | Log frame, small element icons |
| 5.13 | **Pause (combat)** | P0 | None | Resume / Settings / Flee (Flee action is a stub) | Reuses 3.1 |
| 5.14 | **Results** | P0 | Native results screen *(branch)*. Main exits one tick after combat ends. | Victory / Defeat / Draw, turns, survivors; later XP, loot, WSP | Victory and defeat **title treatments**, results backdrop, stinger music ×2 |
| 5.15 | **Loot / rewards** | P1 | None (no reward source on win) | Items and currencies earned, per-instance roll reveal | Item icons, roll-reveal animation, rarity glow |
| 5.16 | **Defeat / retry** | P1 | None (defeat = return to trial reload) | Retry / Return to hub | — |
| 5.17 | **Tutorial overlays** | P0 (pitch) | None | First-time callouts: turn order, defense timing, element caps, infusion | Callout arrow/frame, highlight mask |

---

## 6. Long-term (P2)

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
| **Skill icons** | per spell/ability | `Content/Data/Pool/...` (12 per weapon-type × damage type, plus spells) | Biggest volume. Consider one per *school × element* first, then bespoke for the demo kit. |

---

## 8. Minimum set for the pitch demo (P0 only)

If budget is tight, this is the order to buy or commission:
1. **UI kit** (§0): fonts, frames, buttons, bars, element palette.
2. **9 element emblems** + **9 status-cap proc banners/VFX** (5.9). These sell the core system.
3. **Character portraits** for the demo party + demo enemy (likely 4–5, not 13).
4. **Status-effect icon set** (~40, family-based).
5. **Damage-number font** and treatments.
6. **Defense prompt** glyphs + timing ring.
7. **Title logo + key art**, loading illustration, victory/defeat treatments.
8. **Command icons**, target marker decal, turn-order frames.

Screens to build for the demo, in dependency order:
1. Kit
2. Combat HUD polish (5.1–5.9)
3. Results (5.14)
4. Title / main menu / settings / loading (1.2–1.8)
5. Hub HUD + prompts (2.1–2.2)
6. Trial select (2.6)
7. Codex (3.11)
8. Tutorial overlays (5.17)

Loadout/equip (3.4) is P0 only if the pitch shows building a character; otherwise it's the first P1.

---

## Open questions (they change asset scope)

1. **Art direction?** Painterly, cel-shaded or clean sci-fi? Pick before buying anything; the kit and icons must match.
2. **Which characters and elements are in the demo fight?** This sets the portrait and VFX count (also review Q4).
3. **Gamepad support for the pitch?** If yes, every screen needs a focus path, plus controller glyphs.
4. **2D portraits or live 3D renders** (render-to-texture) for panels and turn order?
5. **Diegetic or flat HUD?** World-space nameplates and bars over characters, or screen-space panels only, as today?
