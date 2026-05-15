# World of Refraction — Conceptual Overview
Last updated: 2026-05-14

This is the design bible. It explains how every system works in player and designer terms — no engineering details. Where a section's design is locked in a dedicated document, the section names that document and explains the rules in plain English without restating decisions.

---

## 1. Game Overview

*World of Refraction* is a single-player turn-based combat RPG. Two teams of up to nine characters each face off on a 3×3 grid arena. Combat is deliberate — players choose actions on their turn — but defense is real-time, with a brief reflex window when an enemy attacks. Across the campaign, players control a Lord at the centre of a network of allied and rival champions; combat outcomes change who follows whom. The core loop is *pick → react → resolve → recruit-or-lose.*

---

## 2. Combat — Core Loop

A fight has two teams. Each team has up to nine combatants on a 3×3 grid: front row, middle, back row. Front row deals and takes a little more damage; back row takes a little less. Position matters.

Turn order is **debt-based**. Each round, every combatant accrues "turn debt" at a rate set by their effective speed (a Spirit-derived stat modified by buffs, status, and weather). Whoever is owed the most goes next. Fast characters take more turns per round; slow characters take fewer but each turn can land harder.

On your turn, the player picks an action category:
- **Attack** (basic weapon strike)
- **Refraction** (a spell — the in-world term for magic)
- **Ability** (a special weapon-driven technique)
- **Resonate Weapon / Resonate Ring / Breakthrough** (the three flavours of crystal-channelled spell, surfaced when their source is available)
- **Defend** (improves defense for one round)
- **Item** (consume a crystal-item)
- **Switch Weapon / Switch Ring** (mid-combat loadout shift)
- **Flee**

After picking the category, you pick the specific entry (which spell? which ability? which item?), then pick a target or targets. Some actions then ask one more question — see Infusion (§8). Once you commit, the action plays out cinematically: the attacker approaches if the action is melee, the action montage plays, and the resolution lands.

**Victory** is when one team has no living combatants. **Defeat** is the mirror. Between matches, equipment durability that was worn during the fight is gradually repaired; crystals that broke entirely are removed from their slots; used consumable item slots tick down their remaining uses.

The round-by-round feel: a chess-paced choice that triggers a short cinematic with one moment of reflex tension when the defense window opens.

---

## 3. Combat — Real-Time Defense

When an attack heads toward a defender, the game opens a **defense window** — a short timed reflex moment lasting a fraction of a second. During that window, the defender (player or AI) can choose to **block**, **parry**, or **dodge**. Outcomes:

- **Block** — reduces incoming damage by ~50%. The easier success.
- **Parry** — cancels most of the incoming damage and reflects a fragment back at the attacker. Tighter timing window.
- **Dodge** — avoids the hit entirely. Only succeeds when the incoming attack's "size" is below a threshold; large attacks can't be dodged, only blocked or parried. A small, fast slash is dodgeable; a heavy spell is not.

Mistime the input or do nothing, and the hit lands clean.

The defense moment doesn't pause the turn — it interrupts the player's deliberative pace and asks for a reflex. It's the game's signature feel: turn-based contemplation broken once per incoming attack by a fighting-game beat.

A **Caster** in their Broken Darkness state has a special relationship to defense: successful parries and blocks feed absorption energy that can later be spent on hybrid spells (§10).

---

## 4. The Three Classes

Each character belongs to one of three classes, set at character creation. The class is the character's relationship-to-power, not a damage role.

### Generic

The grounded fighter. Wields one or two physical weapons. No innate elemental magic, but can equip rings and crystals to access magic externally. Most durable, most direct. Generic characters can dual-wield — primary weapon plus a secondary — and switch between them mid-combat. They get the deepest ability roster (special weapon-driven moves). Their elemental expression comes through *infusion* of a weapon, a ring, or an evolution crystal; the character themselves carries no element.

### Caster

The arcanist. Locked to one **innate element** at creation — Fire, Water, Earth, Wind, Light, Darkness, Lightning, Void, or Reality. Carries one weapon. Comes with a set of innate **refractions** (spells) tied to their element. Casters have access to **Broken Darkness**, a transformation in which they parry and absorb incoming damage and turn that energy back into hybrid spells (§10). The display name shown to players is **Refractor**.

### Resonator

The tactician. No innate element — element comes entirely from the rings they equip. Carries up to six rings (fewer when an Evolution slot is occupied), each holding a crystal of one element. The active ring determines what element the Resonator's magic comes from this turn; they can switch rings mid-combat at the cost of a turn action. Most flexible elemental identity, most fragile setup — a Resonator whose rings all break is left without spells.

---

## 5. The Nine Elements

The element system is the centerpiece of how a character expresses identity in combat. A spell or attack carries an element; that element decides what status it tends to build on the target, what visuals it uses, and (per design) how it interacts with the defender's own elemental nature. There are nine elements (a tenth, *Generic*, is "elementless raw force"; an eleventh, *Broken Darkness*, is a Caster transformation state).

The advantage / weakness matrix between elements is part of the design but is not yet wired into the damage formula — element matchups currently deal base damage. *(Designed, not yet implemented.)*

Each element tends to inflict a signature status when it caps a target's buildup bar. The mapping below is the current design; some pairings (Wind vs. Lightning for Stun) are still being finalised.

### Fire
Aggressive, sustained pressure. Tends to inflict **damage-over-time** (Burn). Identity: spreading harm that punishes hesitation.

### Water
Suppression. Tends to inflict **Heal Block** — preventing the target from recovering HP through any source. Identity: control through denial.

### Earth
Reduction. Tends to inflict **Defense Debuff** — the target takes more damage from everything after. Identity: setup for follow-ups.

### Wind
Tempo disruption. Tends to inflict **Skip Turn** — the target loses their next action. Identity: stealing initiative.

### Lightning
Shock. Tends to inflict **Stun** — the target can only Attack or Defend, blocking spells, abilities, and items. Identity: the cutoff of complexity.

### Light
Precision suppression. Tends to inflict **Crit Chance Debuff** — the target's critical hits dry up. Identity: punishing finesse builds.

### Darkness
Silence. Tends to inflict **Silenced** — the target cannot pay EP costs, blocking spells, abilities, and infusion charges. Identity: cutting off magic at its source.

### Void
Loss of self. Tends to inflict **Random Skill** — the target's next action is chosen for them, used on a random enemy from their own pool. Identity: chaos.

### Reality
The ninth element, structurally different from the other eight. Reality doesn't inflict a debuff like the others — it inflicts **Burst Damage** (an immediate damage spike). Reality is also the only element with a unique stat-modifier behaviour at every layer: an innate-Reality character, a slotted Reality crystal, and a Reality infusion all stack additively on the same action. Identity: amplification of self rather than imposition on the enemy.

---

## 6. Crystals and Catalysts

Crystals are the in-world physical embodiment of magic. They come in three flavours:

- **Refined crystals** are the basic unit. Each is one of the nine elements and one of ten "shapes" (Garnet=damage, Sapphire=heal, Citrine=energy, Emerald/Amber/Opal=stat buffs, Onyx=debuff, Amethyst=gamble, Iolite=cleanse, Quartz=absorb-and-transform). The shape defines what the crystal does; the element tints the effect (a Fire Garnet damages and tends to apply Burn; a Water Garnet damages and tends to apply Heal Block). Refined crystals are durable — they tick down with use rather than firing once and breaking, and they can be slotted into a ring, a weapon, or an evolution slot.
- **Evolution crystals** are rarer. They don't fire effects; they live in a special slot and permanently modify the equipped weapon or ring while slotted, granting authored stat bonuses, passive effects, and unlocking the **Evolution** infusion source. Evolution crystals are immune to breakage — durability is irrelevant for them. They're class-shaped: Casters and Generics use them in weapons, Resonators use them in their rings.
- **Item crystals** are consumable single-use crystals usable through the **Item** action.

**Weapons** and **rings** are "shells" — they carry a slotted crystal and provide stat bonuses, an animation set, and an attack profile. The element comes from the crystal, not the shell. When a weapon swings without a slotted crystal, the attack is purely physical; with a refined crystal slotted, the weapon can deliver elemental hits and trigger elemental effects. A ring without a slotted crystal is inert.

**Durability** is the lifespan of a refined crystal. Each crystal has a max durability set by its tier (F=30, E=40, D=50, C=60, B=70, A=80, S=100). Each "overwork" event — a tier-mismatched cast, an infusion, etc. — wears the crystal. At zero, the crystal shatters. Between combats, crystals that didn't shatter recover gradually (+10 per fight). Crystals that fully shattered during the fight are destroyed at combat end.

**Tier** runs F → E → D → C → B → A → S. Higher tier = more potent effect, more durability, more cost to wield. Some refined crystals have **S-tier secondary effects** that only fire when the crystal is S-tier (an S-tier Garnet doesn't just damage; it also applies a burn).

---

## 7. Spells, Abilities, and Attacks

Three distinct ways to act:

- **Attacks** are weapon swings. Every weapon has an associated attack — a physical strike with a hit count, an animation, and an approach. Generic and Caster characters have attacks tied to their equipped weapon; Resonators attack with whatever weapon their active ring substitutes in (or a default if none). Attacks are physical by default and free of EP cost.
- **Abilities** are special techniques granted by weapons — a charge strike, an area sweep, a defensive parry-counter. Abilities cost EP and are bound to a weapon (the weapon teaches the technique). Generic characters use abilities heavily; Casters use them as supplements to their magic; Resonators use them when their ring's associated weapon allows.
- **Spells (refractions)** are elemental projections. They cost EP and come from one of four sources: a Caster's innate element (innate refractions), a weapon's slotted crystal (Resonate Weapon), a ring's slotted crystal (Resonate Ring), or an Evolution slot's crystal (Breakthrough refractions). Each spell has a delivery type — Projectile, Homing, Beam, or Instant — and authored hit counts.

A player acquires spells, abilities, and weapons over the campaign. A character's combat options on any given turn are a function of their current class, loadout, EP, and any active status effects.

---

## 8. The Infusion System

(See `docs/PastDocumentation/May2026/Infusion_Design_Decisions_Locked.md` for the locked design.)

Infusion is the moment-to-moment tactical choice that gives every action two extra dimensions: a **source** (what powers the boost) and a **level** (how much it's boosted). Together, these turn the same spell or ability into three different versions.

- **L0** — no infusion. The action fires at its base. No extra cost.
- **L1** — light infusion. Boosts **status buildup** by 25%, costs an authored fraction of HP (Innate/Evolution) or wears the crystal (Ring/Weapon).
- **L2** — heavy infusion. Boosts **damage** by 30%, increases spell size to 2.0×, costs more HP or wears more durability.

The **source** decides where the cost comes from and what element the action takes on:

- **None** — no infusion source selected. L1/L2 cannot be applied.
- **Innate** — pay HP from the caster's own element-aligned essence. Element of the action becomes the caster's innate element.
- **Active Ring / Primary Ring** — channel through the equipped ring's crystal. Wears the crystal's durability. Element becomes the ring's element.
- **Weapon Crystal** — channel through the weapon's slotted crystal. Wears the crystal. Element becomes the weapon crystal's element.
- **Evolution** — channel through the Evolution-slot crystal. Pays HP **and** applies an element-flavoured status to the caster as backlash (§9).

All costs are **reserved as preview during selection and applied only at commit**. The player can cycle freely through sources and levels — the HP bar shows a faded "future cost" segment, the status bar shows a faded "future build" segment, the crystal durability bar shows a faded "future wear" segment — and nothing is actually deducted until they confirm the action. This makes experimentation free.

### Worked example

A Caster has 100/100 HP and a Topaz (Wind) Evolution crystal slotted. They want to cast a Fire spell heavily. They open the infusion submenu, cycle the level to L2, and pick **Evolution Fire** as the source. The HP bar shows a faded -10 (10% of max HP for Evolution L2); the self-Burn status bar shows a faded +25 (Evolution L2 status backlash); the crystal-durability bar doesn't change (Evolution crystals are immune to wear). The player confirms the Fire spell. HP drops to 90, self-Burn build rises to 25, the spell fires with L2 multipliers and Fire element. The spell hits the target with extra damage and extra Fire buildup. The caster doesn't burn themselves yet — but three more L2 Evolution Fire casts will fill that bar and they will catch fire.

### Action-level adaptation

For attacks and abilities, both **source** and **level** can be cycled — infusing an attack changes both its element and its boost level. For spells, only the **level** can be cycled — the spell's element is intrinsic to the spell, not changed by infusion. Spell L1 is +30% energy cost / 1.5× size / normal damage / +25% status buildup; spell L2 is +60% energy cost / 2.0× size / +30% damage / +25% status buildup / +source-specific cost.

If a player's chosen source becomes unavailable mid-combat (the crystal breaks, for example), infusion auto-deactivates to L0 and the Infuse button greys until a source returns. *(Designed; auto-deactivate not yet implemented.)*

---

## 9. Evolution and Reality

Reality is the ninth element and structurally unique. Evolution is a special infusion source. Both stack their bonuses on the action via a per-action stat-modifier accumulator (see `docs/PastDocumentation/May2026/PerAction_Stat_Modifiers_Locked.md`).

### Evolution

When a character equips an **Evolution crystal** in the Evolution slot of a weapon or ring, that crystal's authored stat modifiers contribute to every action the character takes — fully if the crystal is slotted as primary, fully if infused at L2, and halved if infused at L1. The stats can cover any of the nine sub-stats (or three pillar percentages, decomposed into their constituent sub-stats). Evolution is the way a player makes a single character substantially more dangerous in a specific direction.

Evolution infusion carries a **backlash**: the caster takes HP damage (5% at L1, 10% at L2) and accrues self-status of the Evolution crystal's element (15% at L1, 25% at L2). At max buildup, the caster's own element-status fires on them — a Fire Evolution caster burns themselves; a Water Evolution caster heal-blocks themselves. The backlash respects element immunity: a Fire-immune character with a Fire Evolution crystal takes no self-status. *(HP cost wired; self-status mapping not yet implemented.)*

### Reality

Reality is the only element that amplifies the caster instead of imposing on the target. A character with the Reality innate element, a Reality crystal slotted, or a Reality infusion gains a flat percentage bonus to all nine sub-stats for that action:

- **Innate Reality** (Reality Refractor): +10% all sub-stats.
- **Reality Evolution crystal slotted as primary**: +5% all sub-stats, plus the crystal's authored stats.
- **L1 Reality infusion**: +2.5% all sub-stats.
- **L2 Reality infusion**: +5% all sub-stats.

These sources stack additively. A Reality Refractor with a Reality Evolution crystal slotted and a separate Reality crystal infused at L2 stacks +10% + +5% + +5% = +20% across the entire stat sheet for that action (plus the slotted crystal's authored bonuses). Detection key is the element, not the class — a Generic character with a Reality Evolution crystal gets the slotted +5%.

### Worked example

A Reality Refractor with 50 base Spell Damage casts a Reality refraction at L1. Reality innate (+10%) + Reality L1 infusion (+2.5%) = +12.5% — the spell calculates damage at 50 × 1.125 = 56.25 (the same buff applies to crit, status build, etc.). The spell hits with a Burst Damage status payload on the target's bar.

---

## 10. Broken Darkness

Broken Darkness is a Caster-exclusive transformation state and the eleventh entry in the element enum (alongside the nine elements and Generic). It changes how the Caster reads in combat.

When a Caster successfully parries or blocks an incoming hit, they absorb a fraction of that hit's energy and record its element. This absorbed energy accrues over time. When the Caster transforms into Broken Darkness, they can spend that absorbed energy on **hybrid spells** — spells that draw on the elements they parried, not just their own innate element. A Fire Caster who has been parrying Water and Lightning attacks for the last two turns can, in Broken Darkness, cast a hybrid Water-and-Fire spell that wouldn't otherwise be available to them.

Broken Darkness has its own rules:
- It has **forbidden elements** (Light and Void) that, if used while in BD, cause 25% self-damage as the system rejects the misalignment.
- It accumulates **overload stacks** — the longer the Caster stays in BD, the more amplified their statuses become (1×, 1×, 2×, 4× multipliers). Pushed too far, BD ticks self-damage from overload.
- It exits the state when the Caster runs out of absorption energy or chooses to revert.

Broken Darkness is a high-risk, high-expressive-range state. It rewards Casters who play the defense window aggressively (parrying instead of blocking, surviving instead of out-DPSing) and turns survival into power.

---

## 11. Status Effects and Buildup

Status effects in *World of Refraction* are not coin-flips. Every hit that carries a status doesn't decide on landing whether to apply the status — it adds to a **buildup bar** on the target. When the bar fills, the actual status fires.

This is the buildup model:
- Each character has a single buildup bar at any moment. The bar tracks the most recent incoming element or physical type.
- Each relevant hit adds an amount of buildup based on the action's status power (`StatusBuildup` field on the data asset, modified by attacker stats and any infusion bonus).
- At 100%, the bar resolves: the status corresponding to the most recent element fires on the target.
- The bar then resets for the next round of pressure.

**Two stats govern this**:
- **Defense** reduces incoming raw HP damage.
- **Resistance** reduces incoming status buildup. A heavily resistant character takes full damage but rarely gets statused. A heavily armoured character takes light damage but builds status at full rate. Players build around the trade-off.

The status that fires depends on the most recent element / physical type:
- **Fire / Slash (default)** → DOT (Burn / Bleed)
- **Water** → Heal Block
- **Earth / Pierce** → Defense Debuff (Armor Break)
- **Wind** → Skip Turn
- **Lightning / Impact** → Stun
- **Light** → Crit Chance Debuff
- **Darkness** → Silenced
- **Void** → Random Skill
- **Reality** → Burst Damage

Statuses **tick down** each turn. Cleansing items (Iolite) and friendly cleansing spells remove statuses immediately. Immunities — granted by certain crystal effects, certain abilities, certain item effects — block status buildup of a matching element or trigger type entirely.

The buildup model rewards focused team composition: a team designed around piling Fire pressure onto one target will pay off even if no single hit triggers Burn, because the next hit will. A team designed around hitting different elements over time will rarely status anything because the bar keeps resetting to a new element.

---

## 12. Weather

Weather is a battlefield-level effect that visibly tints the arena and modifies stats. Each team has a **weather leader** — the highest-`(WorldMind + WorldBody + WorldSpirit)`-summed Caster on the team. If the leader is a Caster, their **equipped weather variant** determines the side's weather. If the leader is a Generic or Resonator, that side has no weather contribution.

When a leader dies or is replaced, weather re-resolves to the next eligible Caster.

Weather is **per-character per-element variant** — a Caster owns weather variants for their innate element (e.g., a Fire Caster carries one or more Fire weather variants, each with different stat-modification profiles). The leader's currently-equipped variant is what shows on the battlefield.

Combat-end behaviour: weather state clears, but the level's default sky restoration is not yet automatic *(Designed, not yet implemented)*. Some weather variants (Earnable, Premium, Reality variants) are designed as authored content but not yet built.

---

## 13. Stats and Modifiers

(See `docs/PastDocumentation/May2026/PerAction_Stat_Modifiers_Locked.md` for the per-action stat accumulator.)

Every character has stats distributed across three **pillars**:
- **Mind** — Efficiency, Effect Damage, Crit Chance, Spell Speed
- **Body** — Defense, Movement Speed, Raw Damage
- **Spirit** — Resistance, Turn Speed, Max Energy, Max Health

Each pillar holds several sub-stats. The same total point allocation can be distributed very differently across two characters of the same class to make them feel distinct. A Mind-heavy Caster is a status specialist; a Spirit-heavy Caster outpaces a Mind-heavy one in turn frequency at the cost of per-cast power.

**Luck** is a separate sub-stat that gates several consumers:
- **Crit bonus** — implemented. Luck adds a linear bonus to crit chance on top of stat-derived crit.
- **Crystal break skip** — implemented. Luck rolls before crystal wear; on success, the wear tick is skipped, effectively extending crystal lifespan.
- **Per-hit dodge** — *(Designed, not yet implemented.)* Luck would let an attack be dodged before defense window opens.
- **Drop chance / drop quality** — *(Designed, not yet implemented.)* Luck would bias post-combat loot rolls toward more drops and higher-tier drops.

**Per-action modifiers** layer on top of base stats: Reality boost (§9), Evolution authored stats (§9), and any future per-action source contribute to a single accumulator that scales the relevant sub-stats for that action only. Truly passive stat modification (always-on buffs from slotted equipment) is a separate layer applied at the character level.

**World Stats** (Mind / Body / Spirit world levels) gate which spells, abilities, and items a character can use. A spell isn't just "do you have the energy?" — it's "did the character live the life that taught them this magic?". This makes the campaign progression layer matter for combat capability, not just narrative.

---

## 14. Durability and Wear

Durability is the lifespan of a refined crystal — "HP for crystals." Each crystal starts at the max durability for its tier (F=30 up to S=100). Each "overwork" event removes durability:

- **Tier-mismatched action** — using a crystal at an action tier higher than the crystal's: +3 per tier above.
- **Infusion** — L1 ability/attack = 4, L2 = 8. L1 spell = 6, L2 spell = 12. Spells wear crystals more than abilities/attacks because spells "create from nothing" through the crystal's channel.
- **Custom spell** — +2 (constant defined; not currently exercised).

Wear stacks: a 2-tier-mismatched L2 spell on a custom spell wears (6 mismatch) + (12 L2 spell) + (2 custom) = 20 durability.

At zero, the crystal **shatters** — it's destroyed at combat end and removed from the player's inventory.

If shattered crystals are slotted in a ring, the ring becomes inert (still equipped, no spells). If shattered crystals are slotted in a weapon, the weapon still works as a physical weapon (loses crystal-specific effects).

**Auto-repair between combats**: +10 durability per battle completed for any crystal that didn't shatter. Items at low durability take multiple battles to fully recover.

**Evolution crystals never break** — durability is irrelevant for them.

---

## 15. AI and Difficulty

AI characters take turns through the same pipeline as players — same actions, same costs, same defense windows. They differ in *how* they pick:

- **Easy** — random action, random target. Tunable accuracy on defense windows. Plays sloppy.
- **Medium** — uses a smart action builder. Tries survival actions first (heal if low HP, cleanse if statused), then offensive actions. Considers target threat, picks valuable status applications, makes basic infusion decisions.
- **Hard** — same logic as Medium but with deeper status-bar awareness (knowing when a single hit would trigger a target's bar), better infusion-level decisions, and higher defense-window reaction rates.

"Smart" AI means: the AI looks at every possible action across its loadout, scores each option (estimated damage, status value, survival value), and picks the best. It can decide *when to infuse* — investing HP and durability for a bigger payoff — and *which source to infuse from*. The AI's evaluation of damage drifts slightly from the runtime calculation (it omits some per-action modifiers); the drift is documented and being closed.

Defense windows for AI: when an attacker targets an AI character, the AI schedules its own defense decision on a short delay, simulating the same reflex moment the player has.

---

## 16. The Player-to-Enemy Feature

The campaign frame is built on the principle that **defeated characters can change sides**. The player's central identity is the **Lord** — a persistent figure who accumulates and loses **champions** over time. A champion is any character in the Lord's network, characters who have their own loadouts, world stats, equipment, durability state, and combat record.

When the player wins a fight against an enemy team, certain champions on the losing side can be recruited into the Lord's network. When the player loses, certain of their own champions can be lost — captured, recruited by the rival faction, or killed outright.

The same character can therefore appear, over a campaign arc, as: a generic enemy in one fight, a recruited ally in the next, a rival lieutenant after switching sides, or a memorial portrait after being killed. The line between hero and enemy is intentionally porous; the political layer of the campaign is built on win/loss outcomes.

This isn't a minigame or a side system — it's the campaign's core engine. *(The feature is designed at the systems level; campaign-side implementation — recruitment UI, rival factions, persistence — is unbuilt. The combat side that makes the feature meaningful is built.)*

---

## 17. Progression Systems

Progression spans three layers.

**Character stats** — distributed across Mind / Body / Spirit pillars at creation, then refined through play. Stat respec rules and progression curves are designed but the leveling loop is not yet implemented.

**World Stats** — Mind / Body / Spirit *world levels* (separate from combat sub-stats) gate which spells, abilities, and items a character can equip. World Stats grow through narrative and out-of-combat events rather than from combat XP. *(Designed; the 21-point progression hierarchy described in `Luck_Consumers_Design.md` is gated on data side; combat-side gating uses the existing World Stats data layer.)*

**Equipment progression** — crystals are tiered F → S; players acquire them through play. Refined crystals can be slotted, swapped, and infused. Evolution crystals are rarer and represent committed character archetypes. *(Drop tables and post-combat loot distribution are designed but not yet built.)*

**Champion roster progression** — the Lord's network of champions grows and shrinks based on combat outcomes (§16). Each champion accumulates their own combat record, equipment, and World Stats independent of the others.

The progression bones are present in data; the campaign-side experience that makes progression *visible* to the player (level-up screens, world-stat events, loot drops, champion-recruitment UI) is unbuilt.

---

## 18. Systems on the Horizon

Designed but not yet implemented:

**Traits system** — passive character modifiers beyond active status effects. Examples include *Split Personality* (the character behaves differently round-to-round) and *conditional / triggered effects* (a passive that fires when HP drops below a threshold, or after three consecutive parries). Traits would layer on top of the existing skill-effect system as a separate tier of always-on or conditionally-active behaviour.

**Adapter weapons / Resonance Mode** — *Citrine* and *Amethyst* crystals would unlock a mid-combat weapon-shift mode allowing a Caster or Resonator to swap their weapon's behaviour without burning a full Switch action. The system would deepen the moment-to-moment expressivity of the combat loop.

**In-game character creation** — a campaign-level character creator letting the player build new champions with chosen class, innate element, stat allocations, and starting loadouts. Currently, characters are authored as designer-side data assets.

**MetaHuman pipeline** — visual upgrade path using Epic's MetaHuman framework for character rendering. Design seed lives in `docs/PastDocumentation/April2026/MetaHuman_Consideration_WoR.docx`.

**Element advantage / weakness matrix** (active in design, inactive in runtime) — the cross-element interaction table that turns "Fire vs Water" from base damage into a meaningful matchup.

**Element-to-self-status mapping helper** — the central function deciding "Fire infusion's self-backlash is a Burn" etc. Needed before Evolution backlash self-status (§9) is wired.

**Save / load** — disk persistence for inventory, loadouts, world stats, champion roster.

**Persistent world map / campaign layer** — the narrative graph connecting individual combats. The Lord-and-champions feature (§16) lives entirely in this layer.

**Multiplayer** — beyond the local two-team framing.

---

## 19. Design Pillars

The decisions that everything else gets tested against:

- **Defense is real-time, not deterministic.** The fighting-game beat inside the JRPG cadence is the signature feel. Anything that smooths it away breaks the game's identity.
- **Element is a character-identity choice, not a damage-type palette.** Letting the player shift element at the action level is what distinguishes the combat system. Locking element to class or weapon would flatten the game.
- **Status pressure rewards focus.** Buildup, not coin-flips. Sustained focused offense pays off; scattered offense doesn't. Team composition should matter because of this.
- **Cost-at-commit, free experimentation pre-commit.** Players should be able to cycle infusion options, hover over choices, and explore consequences without paying for the exploration. Tactical preview is part of the game's surface.
- **Production-grade, not perfect.** Working game over perfect architecture. The combat engine ships incrementally; design locks happen when they're ready, not when they're complete.
- **The line between hero and enemy is porous.** Recruitment, defeat, and faction-switching are how the campaign expresses itself — combat outcomes change the cast list.
