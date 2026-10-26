# Classes and loadouts

This page states what each class is, what a character can equip, and what that gear lets it do in a fight: the three classes, loadout slots and caps (the Resonator budget, item slots), weapons, rings, the attachments (crystals, augment stones, fusions, evolutions) as far as they decide what a character can use, the Refractor's innate spell pool, and how world-pillar requirements affect using a skill. Damage numbers, elements and resistances are in 02-elements-and-damage.md; status bars in 03-status-bars.md; what effects do and how turns and costs run in 04-effects-and-turns.md; Gold, Prisms, upgrades and runs in 05-economy-and-runs.md; party size and control in 06-party.md. Code paths are relative to `Source/world_of_refraction/`.

## Classes

| # | Rule | Status | Source |
|---|---|---|---|
| CL-01 | Every character has one of three classes, set on its character data: **Generic**, **Refractor** (the code calls it `Caster`) and **Resonator**. | In game | `Public/Character/ECharacterClass.h:32-37`; `Public/Character/CharacterData.h:123` |
| CL-02 | A class is made by its slots, not by a skill gate: what a character can use comes from what its slots hold. Any class with a weapon gets that weapon's abilities; spells come from rings, evolutions and a Refractor's innate pool. The old gate helpers (`CanUseAbilities` = Generic only, `CanUseSpells` = not Generic) are not read by combat. | In game (helper cleanup B-097) | `D-027`; `Private/UI/Combat/FCombatCapabilities.cpp:49-59`; `Public/Character/ECharacterClass.h:45-55`; `Public/Character/CharacterData.h:261-264` |
| CL-03 | **Generic**: a primary slot (weapon, ring or evolution) plus a secondary slot that holds a weapon or nothing. No innate element. Can cast spells only through a ring or evolution it wears. | In game | `D-027`; `D-007`; `Private/Loadout/FSavedLoadout.cpp:65-86` |
| CL-04 | **Refractor**: a primary slot (weapon, ring or evolution) plus an innate spell pool. Only a Refractor has an innate element, chosen on its character data. | In game | `D-027`; `Public/Character/CharacterData.h:125-128`; `Public/Character/ECharacterClass.h:57-60` |
| CL-05 | **Resonator**: a primary slot (weapon or evolution, **never a ring**) plus a ring loadout. It casts the spells of its **active** ring. | In game | `D-027`; `Private/Loadout/FSavedLoadout.cpp:40-44`; `Private/UI/Combat/FCombatCapabilities.cpp:162-191` |
| CL-06 | A Resonator's EP only fills while it has something to spend it on: an active weapon or an evolution in the primary slot. Without one, EP gain is blocked and the EP bar is hidden. | Kept by Crown (2026-10-26, rules review) | `Private/Character/CharacterDataComponent.cpp:356-366`, `:384-393`; `Private/UI/Combat/CharacterPanelWidget.cpp:785-793` |
| CL-07 | A Generic or Resonator with no gear can't enter a fight: it is left out and the fight starts with the others. A Refractor can fight with no gear (innate spells). Today an empty primary slot is still valid. | Decided, not built (B-084) | `D-016`; `Private/Loadout/FSavedLoadout.cpp:58-60` |
| CL-08 | Two Refractor forms change spell access: a **Reality** Refractor may cast any element; a **Broken Darkness** Refractor casts only its single active pool (plus any element an equipped crystal channels), with a doubled spell budget (CL-35). | In game | `Private/Combat/Mechanics/BrokenDarknessManager.cpp:950-970`; `Public/Skills/Definitions/ElementHelpers.h:37-42`; `docs/Mechanics/Archetypes/Reality.md`, `BrokenDarkness.md` |

## Loadout slots and caps

| # | Rule | Status | Source |
|---|---|---|---|
| CL-09 | A character keeps saved loadouts (gear presets) and fights with the active one. All characters draw from **one shared pool** per player; an item can sit in several presets but only one character wears it at a time. Today loadouts hold copies, and each pawn gets a copy of the whole pool. | Decided, not built (B-091, B-085) | `D-020`; `Private/Inventory/InventoryComponent.cpp:1446-1503` |
| CL-10 | **Resonator budget**: primary cost + ring costs must be **≤ 5** (`LOADOUT_TOTAL_BUDGET`). Bare weapon 1 (`WEAPON_BASE_SLOT_COST`); weapon with anything attached 2 (`WEAPON_CRYSTAL_SLOT_COST`); weapon with an evolution 3 (`WEAPON_EVOLUTION_SLOT_COST`); evolution primary 2 (`EVOLUTION_PRIMARY_SLOT_COST`); ring 1 (`RING_BASE_SLOT_COST`); evolved ring 2 (`RING_EVOLUTION_SLOT_COST`). | In game | `D-010`; `D-027`; `Public/Loadout/LoadoutConstants.h:31,34`; `Public/Inventory/InventoryConstants.h:53-65`; `Private/Loadout/LoadoutComponent.cpp:606-656`; `Private/Loadout/FSavedLoadout.cpp:132-171` |
| CL-11 | There is no ring-count cap and no same-element cap: any mix of ring elements, the budget is the only limit. (The unread `MAX_RESONATOR_RINGS` / `MAX_EVOLVED_RESONATOR_RINGS` were removed in B-117.) | In game | `D-010`; `Private/Loadout/LoadoutComponent.cpp:585-604` (budget) |
| CL-12 | The budget is a Resonator rule only. A Generic's and a Refractor's slots are fixed (CL-03, CL-04) and no budget is checked for them. | In game | `D-027`; `Private/Loadout/LoadoutComponent.cpp:585-604`, `:658` |
| CL-13 | A loadout carries up to **6** item slots (`MAX_ITEM_LOADOUT_SLOTS`), each up to **3** of one item (`MAX_QUANTITY_PER_ITEM_SLOT`), no two slots of the same item type. | In game | `Public/Inventory/InventoryConstants.h:103,106`; `Private/Loadout/FSavedLoadout.cpp:199-225`; `Private/Loadout/LoadoutComponent.cpp:757` |
| CL-14 | At run start you **buy** your loadout from your pool with a starting Gold budget (no random draft). Slot limits apply: 1 weapon / 2 rings / 1 evolution early, growing with account perks. | Decided, not built (B-116) | `D-021`; `docs/Design/Resources_Design.md` §8 (design only) |
| CL-15 | You can own at most **5** evolutions at once (`MAX_EVOLUTION_ITEMS`); slotted ones still count. | In game | `Public/Inventory/InventoryConstants.h:43`; `Private/Equipment/Crystals/EvolutionInventoryComponent.cpp:41` |

## Weapons

| # | Rule | Status | Source |
|---|---|---|---|
| CL-16 | The basic **Attack** is the active weapon's attack (a per-loadout override wins). No active weapon means no basic attack. | In game | `Private/Loadout/LoadoutComponent.cpp:2568-2588`; `Private/UI/Combat/FCombatCapabilities.cpp:51-56` |
| CL-17 | The active weapon: a Generic with two weapons uses the one shown; a Generic with a ring or evolution primary uses its secondary weapon; a Refractor or Resonator uses its primary weapon, and has **none** when the primary is a ring or an evolution. | In game | `Private/Loadout/LoadoutComponent.cpp:2658-2708`, `:2383-2425` |
| CL-18 | **Switch Weapon** is offered only to a Generic with a weapon in both slots. It is a free menu toggle (no turn, no EP) that rebuilds the menu for the new weapon. Not allowed while stunned. | In game | `D-023`; `Private/UI/Combat/FCombatCapabilities.cpp:210-214`; `Private/UI/Combat/CombatCommandMenuSubsystem.cpp:906-920` |
| CL-19 | After a weapon or ring swap, gear **stat bonuses** follow the new gear, but gear **effects** stay as they were at fight start. Crown ruled that effects follow the swap. | Decided, not built (B-081) | `D-008`; `Private/Loadout/LoadoutComponent.cpp:2341-2381` |
| CL-20 | Abilities come from the active weapon, for every class: its preset abilities plus the ones you assign. A weapon holds abilities by **its tier**: F 1, E 2, D 3, C 4, B 5, A 6, S 6 (`ATTACHMENT_SLOTS`). Locked presets (conjured weapons) fill slots and can't be removed. | In game | `Private/UI/Combat/FCombatCapabilities.cpp:58-59`; `Private/Loadout/Entries/FWeaponLoadoutEntry.cpp:31-51`; `Public/Equipment/Crystals/AugmentStoneConstants.h:35` |
| CL-21 | **Weapons never give spells.** A crystal on a weapon gives elemental (infused) abilities instead. Today a weapon crystal still offers its spells (Resonate Weapon), and they cost no EP. | Decided, not built (B-098) | `D-027`; `D-007`; `Private/UI/Combat/FCombatCapabilities.cpp:61-69`; `Private/Combat/Actions/ActionExecutor.cpp:345-352` |
| CL-22 | A Generic with an evolution primary **keeps its secondary weapon** and that weapon's attack and abilities. The active-weapon path does this; `HasSecondaryEquipment` still says it has no secondary. | Decided, partly built (B-099) | `D-027`; `Private/Loadout/LoadoutComponent.cpp:2671-2683` vs `:2469-2473` |
| CL-23 | Wield mode (Single / Dual / Off-Hand Shield) only sets how the weapon is shown. The player doesn't choose it, and it changes no rule. | Kept by Crown (2026-10-26, rules review): damage is set per skill, so wield mode stays visual | `Public/Equipment/Weapons/EWeaponWieldMode.h:28-33` |

## Rings

| # | Rule | Status | Source |
|---|---|---|---|
| CL-24 | A ring's element is its attached crystal's element. With no crystal, or a broken one, the ring has no element and gives no spells. | In game | `Private/Loadout/Entries/FRingInventoryEntry.cpp:17-24`; `Private/UI/Combat/FCombatCapabilities.cpp:145-148`, `:178-181` |
| CL-25 | A ring holds spells by its tier (same F1…S6 curve). A gem crystal on it sets the count by **the crystal's** tier instead, and an evolution on it by **the evolution's** tier (Q-147; same on a weapon, `CrystalEffectTable::ResolveSpellSlotCap`). Spells on a ring must be its element or a Generic spell (which takes the ring's element). A loadout with more spells than the holder's count is flagged by the loadout check, one finding per spell the fight would drop (B-196). | In game | `Private/Loadout/Entries/FRingLoadoutEntry.cpp:26-33`, `:116-139`; `Public/Skills/Definitions/ElementHelpers.h:37-42` |
| CL-26 | Ring use by class: a Resonator wears a ring loadout and casts the active ring, switching with **Switch Ring** (free menu toggle, needs 2+ rings, cycles in order). A Generic or Refractor can wear one ring, in the primary slot. | In game | `Private/UI/Combat/FCombatCapabilities.cpp:131-191`; `Private/UI/Combat/CombatCommandMenuSubsystem.cpp:923-947` |
| CL-27 | Ring spells cost **no EP**; they wear the ring's crystal instead. A non-evolved ring can break. | In game | `Private/Combat/Actions/ActionExecutor.cpp:345-352`; `Private/UI/Combat/FCombatCapabilities.cpp:149`, `:182` |
| CL-28 | A ring accepts a crystal, an evolution, or an **elemental fusion** (stat stone + crystal). It never accepts a bare augment stone or a two-stone fusion. | In game | `D-009`; `Private/Inventory/InventoryComponent.cpp:664-668`, `:879-882`; `Private/Equipment/Rings/RingData.cpp:26-46` |
| CL-29 | On a ring, the stat-stone half of an elemental fusion gives its stat bonus to the wearer (today stone bonuses are read from the active weapon only). | Decided, not built (B-082) | `D-009` |

## Attachments: crystals, stones, fusions, evolutions

| # | Rule | Status | Source |
|---|---|---|---|
| CL-30 | A weapon or ring has **one** attachment slot, holding a crystal, an augment stone, a fusion or an evolution. | In game | `Public/Loadout/Entries/FWeaponInventoryEntry.h:129-138`; `Private/UI/Combat/FCombatCapabilities.cpp:71-75` |
| CL-31 | Augment stones go on weapons only. An Ability Stone adds ability slots by its tier (1…6, `MAX_AUGMENTSTONE_ABILITIES = 6`), shown as extra abilities; a stat stone gives its stat bonus while attached. | In game | `Private/UI/Combat/FCombatCapabilities.cpp:74-75`; `Public/Loadout/LoadoutConstants.h:10`; `docs/Mechanics/Items/AugmentStones.md` |
| CL-32 | A fusion is two halves with at least one stat stone: stat stone + crystal, + stat stone, or + Ability Stone. Two crystals, two Ability Stones, or Ability Stone + crystal can't fuse; an evolution never fuses. | In game | `Public/Equipment/Crystals/CrystalTypeHelpers.h:87-110` |
| CL-33 | An evolution in the primary slot gives its spells (the Breakthrough menu), holds spells by its tier, and changes the wearer's pillars. Taking it out of the primary slot **destroys it for essence**; taking one off a weapon or ring returns it with 10% wear (if that breaks it, it returns broken: Q-159, evolutions are never lost to wear). | In game (spell cap: Conflict, see below) | `Private/UI/Combat/FCombatCapabilities.cpp:116-127`; `Private/Currency/EconomyService.cpp:408-445`; `Private/Inventory/InventoryComponent.cpp:959-968` |

## Refractor innate spells

| # | Rule | Status | Source |
|---|---|---|---|
| CL-34 | Each innate spell costs its tier: F 1, E 2, D 3, C 4, B 5, A 6, S 7 (`SPELL_SLOT_COST`). The total must be **≤ 24** (`INNATE_SPELL_BUDGET`), with at most **6 per school** (`MAX_EQUIPPED_SLOT_POOL`). Each pillar at Mind 4 / Body 7 / Spirit 5 or more cuts every cost by 1 (`SPELL_DISCOUNT_*_THRESHOLD`), never below 1 (`SPELL_SLOT_COST_FLOOR`). | In game | `D-027`; `Public/Loadout/SpellPoolConstants.h:23-48`; `Private/Loadout/LoadoutComponent.cpp:658-750` |
| CL-35 | A Broken Darkness Refractor's budget is **48** (`BD_SPELL_BUDGET`), shared across its Darkness pool and every absorbed element pool. | In game | `Public/Loadout/SpellPoolConstants.h:28-31`; `Private/Loadout/LoadoutComponent.cpp:670-683` |
| CL-36 | A Refractor's innate spells, and the spells it casts, must be its own element, a Generic spell, or an element an equipped crystal channels (Reality and Broken Darkness as in CL-08). Generic and Resonator have no such element check. | In game | `Private/Combat/Actions/ActionExecutor.cpp:238-258`; `Private/Combat/Mechanics/BrokenDarknessManager.cpp:936-970` |

## Pillars and requirements

| # | Rule | Status | Source |
|---|---|---|---|
| CL-37 | The world pillars Mind, Body and Spirit are authored **0–7** on the character (`MAX_WORLD_STAT_LEVEL`), 3 substat points per level (`POINTS_PER_WORLD_STAT_LEVEL`). In a run the live value adds the head start (up to 4 across the three) and bought points, clamped at **20** (`LIVE_MAX`, a placeholder tuning value). | In game | `Public/Character/StatConstants.h:17-18`; `Public/Character/CharacterDataComponent.h:22-32`; `Private/Character/CharacterDataComponent.cpp:243-245` |
| CL-38 | Skills carry a Mind/Body/Spirit requirement (0–7 each). Falling short **never blocks** equipping or using a skill. | In game (Conflict, see below) | `Public/Skills/Definitions/WorldStatRequirements.h:24-34`; `Private/Combat/Actions/ActionExecutor.cpp:226-236`, `:281-286` |
| CL-39 | **Requirement gap**, per pillar: gap = required − yours, clamped to ±5. ×1.40 at −5, ×1.30 −4, ×1.20 −3, ×1.13 −2, ×1.06 −1, ×1.00 at 0, ×0.90 +1, ×0.78 +2, ×0.64 +3, ×0.50 +4, ×0.32 at +5. It scales that pillar's substats for that action: Mind (Efficiency, SpellDamage, CritDamage, SpellSpeed), Body (Defense, ActionSpeed, RawDamage, Reflex), Spirit (Resistance, StatusMultiplier, Luck). Basic attacks too. How this feeds damage: 02. | In game | `Public/Combat/Damage/TierGapConstants.h:125-150`; `Private/Combat/Actions/ActionExecutor.cpp:6846-6860`; `Public/Combat/Actions/ActionStatModifiers.h:131-148` |
| CL-41 | Bound gear: an item can belong to a lord. Anyone can wear it; only that lord gets its bound bonus (effects, working like the item's own: at combat start or armed with their trigger). Everyone else sees no line for it. | In game (shown in no screen yet) | `B-192`, `D-033`, `Q-192`; `Equipment/EquipmentDataBase.cpp` (`IsBoundBonusActive`); `Loadout/LoadoutComponent.cpp` (`GetActiveEffectsGathered`) |
| CL-40 | A weapon's own requirement matters only to Broken Darkness strain (a BD infusing an ability with its innate Darkness over the weapon's requirement). It changes nothing else. | Changed by Crown (D-082, B-162): under the weapon's requirement, the weapon gives less than its full output | `Private/Combat/Actions/ActionExecutor.cpp:4566-4602` |

## Open questions

- Answered 2026-10-26: D-039 (by tier), D-040 (always on), D-041 (a scale, not a gate). See the conflicts below.

These points sit inside answered entries but were left unruled:
- D-020 (ANSWERED, part unruled): the clash rule when two presets want the same item (suggested: the party screen flags it and you pick the wearer).
- D-026 (ANSWERED, part unruled): how a hub-built team's own gear works at run start (D-021 covers only the leader's loadout).
- D-027 (ANSWERED, part unruled): class passives like the deckbuilder's (see Gaps).
- D-007 (ANSWERED, part unruled): the deckbuilder's extra costs for elemental Generic abilities (1 Action to switch, +1 Energy).

## Conflicts

- **CL-C1: How many spells an evolution holds.** The runtime check caps evolution spells by the evolution's tier, F 1…S 6 (`Private/Loadout/LoadoutComponent.cpp:575-581`), as `docs/Mechanics/Gear/EquipmentSlots.md` says. The saved-loadout check uses a flat 6 (`MAX_SPELL_SLOTS`, `Private/Loadout/FSavedLoadout.cpp:190-195`), and `docs/Mechanics/Items/EvolutionCrystals.md` says "up to ~4". Question, options and recommendation: **D-039**. **Ruled (D-039): A**, by tier (1–6, `SlotsForContainerTier`); the saved-loadout check and the doc are fixed in B-147 (done: the live check already capped by tier; test added, docs fixed).
- **CL-C2: Does a Refractor's innate always work?** Here the innate pool works whatever the Refractor wears (`Private/UI/Combat/FCombatCapabilities.cpp:80-112`). In the deckbuilder a Refractor's innate element works only while it has at least one empty gear slot (`wor-deckbuilder/docs/reference/01-card-model.md`, "A Caster's innate element"). Question, options and recommendation: **D-040**. Flagged only because the games differ. **Ruled (D-040): A**, always on; nothing to build.
- **CL-C3: Are skill requirements a gate or only a scale?** `docs/Mechanics/Character/Stats.md` says pillar levels "gate skill tiers", and `ValidateAction` keeps a commented-out "Requirements not met" refusal (`Private/Combat/Actions/ActionExecutor.cpp:233-236`). The shipped rule (`docs/Mechanics/Scaling/RequirementGap.md`, PIE-verified) and the code never block: they scale (CL-38, CL-39). Question, options and recommendation: **D-041**. **Ruled (D-041): A**, usable but weaker; B-147 deleted the commented-out refusal and fixed `Stats.md` (done).
- **CL-C4: Stale text the rulings already settle (no question, needs a doc/comment sweep).** → **Fixed in B-117.**
  - `Public/Character/ECharacterClass.h:9-29` says "2 physical weapons", "1 weapon" and "6 slots, up to 3 same element". `GetClassDescription` (`:91-104`, unused) says Generic has "No spells". Both disagree with D-010 and D-027 (B-083).
  - `docs/Mechanics/Character/Classes.md` and `Archetypes/Generic.md` say Generic is the only class with weapon abilities and that a weapon crystal lends it spells. This disagrees with D-027 and D-007.
  - `docs/Mechanics/Items/FusionStones.md` says rings reject every fusion today. The code accepts elemental fusions (CL-28, D-009).
  - `docs/Mechanics/Gear/Loadout.md` says an evolution primary costs 3. `EVOLUTION_PRIMARY_SLOT_COST` is 2; 3 is a weapon carrying an evolution (CL-10).
  - `docs/Mechanics/README.md:60` calls the loadout "active gear (cap 5)" as if every class had it. The budget is Resonator-only (CL-12).
  - `docs/Mechanics/Scaling/RequirementGap.md` lists StatusMultiplier under Mind. The code puts it under Spirit (`ActionStatModifiers.h:145`), as `Stats.md` does.
  - `Private/Combat/Actions/ActionExecutor.cpp:6849-6851` says the old √-deficit penalty is "still LIVE", and `WorldStatRequirements.h:69` says the penalty is "capped at 60%". The penalty is no longer applied anywhere (`Private/Combat/Damage/DamageCalculator.cpp:321`).
  - `Public/Loadout/Entries/FRingInventoryEntry.h:155-157` says a crystal-less ring returns `Generic`. The code returns `None` (`FRingInventoryEntry.cpp:23`).

## Gaps

- No basic attack without a weapon: a Refractor with a ring or evolution primary, or a Resonator with an evolution primary, has no Attack (CL-16, CL-17). Stunned, it can only Defend (D-001/D-023). No rule says whether an unarmed attack should exist. **Ruled (Crown, 2026-10-26, rules review): keep it. No weapon, no basic attack: a character without a weapon has traded it for literal powers, so they cast.** The logic behind it should be tidied (one clear check for "has a basic attack"), with no change in behaviour.
- No class passives. The deckbuilder gives Generic +1 Physical Resistance, Resonator +1 Resistance per keyed ring element, and Refractor +3 Amp (a flat damage boost) and +1 Resistance to its innate (`wor-deckbuilder/docs/reference/01-card-model.md`, "Class passives"). D-027 left these unruled. **Ruled (Crown, 2026-10-26, rules review): no class passives; the classes are already unique through their slots and rules.**
- No rule ties an ability to a weapon type. Abilities are assigned to whatever weapon holds them. The deckbuilder lists the weapons each ability needs, e.g. "2 Swords" (`01-card-model.md`, "An Ability's RequiredWeapons"). **Outdated (2026-10-26):** abilities carry `RequiredWeaponType`, and the one loadout rule set (B-028) enforces it, with tests.
- No rule for a skill whose source goes away mid-fight (a ring breaks, a crystal breaks). The deckbuilder lets such a skill stay placed but refuses its use (`01-card-model.md`, "A skill can be placed while its need is unmet"). **Ruled: D-083** (greyed out with the reason; broken ring crystal → F-tier spells only; broken weapon crystal → no infusing or elemental abilities). Built in B-163 (`ULoadoutComponent::GetBrokenSourceReason`, `SourceBreakRules.h`); infusing through a broken ring: **Q-163 = B**, it is no infusion source (fix/Q-163-broken-ring-infusion). **Q-163b = B** (approved by default): those F-tier spells cast plain: no infusion through any source, and a Generic one is non-elemental. The AI casts them too (B-197, `UAIDecisionManager::GetUsableSpells`).
- Generic's secondary slot can't hold a ring (`HasRingInSecondary` always returns false, `Private/Loadout/LoadoutComponent.cpp:2483-2511`). Nothing says whether it should. **Ruled (Crown, 2026-10-26, rules review): weapon only. A ring goes only in the primary slot, as for everyone else.** The always-false `HasRingInSecondary` can be retired once checked unused.
- No equip, socket or loadout screen exists: every rule above can only be exercised through data-configured loadouts (`docs/Mechanics/Gear/Loadout.md`). Covered by the UI rebuild (D-024) and the placeholder-UI rule.
