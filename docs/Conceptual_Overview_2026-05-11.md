# World of Refraction — Conceptual Overview

**For:** designers, producers, playtesters, publisher reviewers. Anyone who needs to understand the game without reading code.
**Date:** 2026-05-11
**Status:** in development; this document is a snapshot, not a promise.

---

## What it is

*World of Refraction* is a turn-based combat RPG built around three big ideas:

1. **Defense is real-time inside a turn-based frame.** When an enemy attacks you on its turn, you have a short window to react — block, parry, or dodge — by pressing the right button at the right moment. Combat is paced and tactical like a JRPG, but moment-to-moment defense is reflexive.
2. **Magic is built from nine elements, and the same character can change elements every turn.** A spell isn't a fixed thing; it's an element plus an effect plus your tactical choice of how to power it. The same fireball-shaped spell can come out fire, water, dark, or void depending on what you've equipped.
3. **The line between hero and enemy is intentionally porous.** Enemies you defeat can be recruited; champions you train can later be defeated and converted by an enemy faction. The Lord — the player's central figure — sits at the top of a network of allied and rival champions whose loyalty depends on outcomes.

The combat loop, the character system, and the political layer all reinforce each other: choosing what element to fight with this turn isn't just damage optimization, it's part of what kind of character you're becoming.

---

## How combat works

A fight has two teams. Each team has up to nine combatants arranged on a 3×3 grid — front row, middle, back row. Front row deals and takes a little more damage; back row takes a little less. Where you stand matters.

**Turn order is debt-based.** Everyone earns "turn debt" each round at a rate set by their speed. Whoever is owed the most goes next. A fast character takes more turns per round; a slow character takes fewer but, depending on stats, each turn can land harder.

**On your turn**, you pick an action category: a basic attack, a refraction (magic spell), an ability (special technique), a defend stance, an item, or a switch/flee. After picking the category, you pick what to do specifically (which spell? which ability?), then pick a target.

Most actions then start what we call the **defense moment**. The attack plays its animation toward the target. For a brief window, the defender — whether player-controlled or AI — can press a button at the right time to block (reduces damage), parry (cancels the hit and reflects a fragment), or dodge (avoids the hit entirely). Mistime it and the hit lands clean.

Damage is computed once defense resolves. The hit is applied — health drops, sometimes a status effect lands.

**Status effects** work on a buildup model rather than a simple yes/no. Every attack that carries a status (a fire spell, a bleeding sword swing, a poisonous touch) adds to a status bar on the target. When the bar fills, the actual effect triggers — burning, bleeding, stunned. This means a single hit rarely poisons you; sustained pressure does. The pressure cleared between turns by Iolite items, friendly cleansing magic, or sometimes by a character's own resistance ticking it down.

**Victory** comes when one team has no living combatants. **Defeat** is the inverse. Between matches, equipment durability that broke during the fight is repaired (if it hadn't shattered entirely), used items are consumed from inventory, and characters carry their wounds forward into the next encounter.

The round-by-round feel: you're choosing actions deliberately like a chess move, then watching the action play out cinematically with a short, focused defense moment that snaps you out of the contemplative pace and asks for a reflex.

---

## How characters work

Every character belongs to one of three **classes**. Each class is a different shape of relationship to the game's combat systems.

- **Generic** — the all-rounder. Wields one or two weapons. Has access to all combat options but starts with no innate elemental magic. The "I'll do the thing in front of me" class. Generic characters can infuse their weapon with their innate element if they have one — making physical attacks deal extra elemental damage at the cost of weapon wear.
- **Caster** — magic-first. Casts spells through their innate element and may temporarily "break" into a transformed state where they absorb attacks and turn the absorbed energy back into hybrid spells. Casters have access to a special transformation called *Broken Darkness* that fundamentally changes how their combat reads — they parry incoming damage to feed their power.
- **Resonator** — equipment-driven. Doesn't have a fixed elemental identity; instead, equips a "ring" carrying a refined crystal that determines what element their magic comes from. A Resonator can carry multiple rings and rotate between them mid-combat. Versatile, but their power is bounded by their equipment.

The three classes mean three feels: a Generic feels like a grounded action character, a Caster feels like a transforming arcanist, a Resonator feels like a tactician choosing the right tool for the job.

Every character has stats split across three **pillars**: **Mind** (thinking, magic effectiveness, status power), **Body** (durability, physical output, health), and **Spirit** (speed, ritual, energy). Each pillar holds several sub-stats — Critical Chance, Action Speed, Energy regeneration, Status Multiplier, etc. — and the same total can be allocated very differently across two characters of the same class to make them feel genuinely distinct. A Mind-heavy Caster is a status-effect specialist; a Spirit-heavy Caster casts twice for every once a Mind-heavy Caster does.

**Weapons and abilities** are what the character carries into combat. A weapon has a physical damage type (slash, pierce, impact) that biases what status it tends to build, and may have a slotted crystal that gives it elemental properties. An ability is a special move — a charge attack, a defensive technique, an area sweep — independent of magic.

**Spells (Refractions) and elements** are the centerpiece. There are nine elements:

- Fire, Water, Earth, Air, Lightning, Light, Dark, Void, and Generic (raw force, no element).

A spell is a shape (fireball, beam, area-of-effect, support buff) crossed with an element. The same shape can come out different elements depending on how you've equipped your character. A Caster casts via their innate element. A Resonator casts via whichever ring is active. A Generic doesn't natively cast, but can infuse their attack with their innate element.

**Infusion** is the moment-to-moment tactical choice. When you go to cast a spell or swing a weapon, you can choose to pay extra cost (HP, durability, energy) to either boost the damage (L2 charge) or boost the status buildup (L1 charge). You can also choose what *source* powers the infusion — your own innate element, your active weapon's crystal, your equipped ring, or a special evolution crystal you've slotted. The choice changes what element the action comes out as, what's paid in cost, and what side-effects (durability wear, self-damage, status backlash) get triggered.

This means every turn, even with the same basic action, you're making a real choice: power it normally, or pay tax for charge level 1 (more status), charge level 2 (more damage), and which kind of power it draws from.

---

## How status effects work

Two stats determine how status effects flow:

- **Defense** reduces incoming raw damage. Defending an attack reduces how much HP you lose.
- **Resistance** reduces incoming status buildup. Resisting an attack reduces how fast their status bar fills against you.

These are intentionally different. A heavily armored character takes light damage but can still get statused as fast as anyone. A heavily resistant character takes full damage but rarely gets statused. Build choices matter.

Each character has a status bar per element-tagged effect type. Fire spells fill a "fire status" bar — when it caps, the target burns. Slashing attacks fill a "bleed" bar (in principle — see Honest Limitations). Stuns fill via impact damage. Heals don't fill anything.

When a status bar fills, the actual status fires: damage-over-time burns, defense-reduced armor breaks, speed-reduced slows, energy-drained silences. These have durations; they tick down each turn until they wear off. Crystals (Iolite, specifically) cleanse status when used.

The buildup model means status isn't a coin flip — it's a slow accumulation. Sustained pressure from one element on one target eventually pays off. That changes how teams build: dedicated status applicators ("burn every turn until they catch fire") versus heavy single-target hitters ("just kill them faster than they can status me").

---

## How items and crystals work

The game has a unified **crystal economy**. Crystals come in several flavors:

- **Refined crystals** are the basic combat items — small, single-use, slottable into weapons and rings. Each refined crystal is one of nine element types, but their *gameplay function* is determined by the crystal's "shape": Garnet (damage), Sapphire (heal), Citrine (energy), Emerald (buff stat A), Amber (buff stat B), Opal (buff stat C), Onyx (debuff), Amethyst (gamble — random outcome), Iolite (cleanse), Quartz (absorb and transform). The element of the crystal tints the effect; the shape determines what the effect is.
- **Evolution crystals** are rarer and more powerful — they don't give a one-shot effect like refined crystals. Instead, they permanently modify a weapon or ring while slotted (stat boosts, behaviour changes). Evolution crystals are character-class-specific: Casters use them in their weapons differently than Generics use them in rings.
- **Generic items** are the rest of the economy — consumables, potions, scrolls.

**Weapons hold crystals.** A weapon has slots for refined crystals (giving it elemental properties or buffs) and a separate slot for an evolution crystal (giving it deeper changes). When you cast a spell or swing the weapon, the weapon's slotted crystals may be consumed (refined crystals are single-use); the weapon's durability ticks down. Eventually a weapon or its evolution crystal can break, removing that capability from the character mid-combat.

**Tier** matters. Items come in tiers (S, A, B, C, D, E, F). Higher tier = more potent effect, more cost. Some crystals have **secondary effects** that only fire at higher tiers (an S-tier Garnet doesn't just damage — it also applies a burn).

**Stat traits** are minor passive bonuses some items grant. Equipping a Generic with a particular ring might bump Mind by 5%; equipping a Caster might bump Spirit. Build composition matters.

---

## The big design ideas

What makes this game different from other turn-based RPGs:

**1. Real-time defense in a turn-based frame.** Most turn-based RPGs ask "what do you do?" and then play it back. This one asks "what do you do?" — and then, briefly, "react." The defense moment compresses a fighting-game reflex into a JRPG cadence. It's the game's signature feel.

**2. Element + Shape = Spell.** The nine-element system isn't a damage-type palette; it's a character-identity choice that you can change every turn. The same character can act as different colors of fighter — fire today, void tomorrow — because the spell, the infusion source, and the equipment are all separable. Few RPGs let you shift elemental identity at the action level. We do.

**3. Three-class triangle.** Generic, Caster, Resonator don't sit on a damage/tank/support triangle. They sit on a *relationship-to-power* triangle: physical-only, magical-by-self, magical-by-equipment. Each plays meaningfully differently.

**4. Character-to-enemy conversion is a story mechanic.** Defeated enemies can be recruited. Trained champions can be lost to defeat. The Lord — the player's persistent identity — accumulates and loses champions over the campaign arc, and the political layer is built on these wins and losses. This isn't a minigame; it's where character identity is contested.

**5. Status bars instead of status rolls.** Every status effect builds via a bar that fills with sustained pressure rather than firing on a single roll. This rewards focused team comp ("we are the burn team") over single-shot luck.

**6. World stats.** There's an out-of-combat economy of "world stats" — knowledge, charisma, wealth — that gates which spells, abilities, and items your characters can use. A spell isn't just "do you have the energy?", it's "did your character live the life that taught them this magic?". Builds extend beyond combat.

**7. Champions and the Lord.** The campaign is fractal: in each fight you play a team of characters, but across the campaign each of those characters is a champion in your network, accumulating their own world stats, their own equipment, their own status. The Lord is the master node; champions defend, rise, and sometimes fall.

---

## Honest limitations

This is what currently works, what currently doesn't, and what's in progress. A publisher reading this should not feel surprised by anything when they hit "play."

**What works today (verified in-engine):**
- The combat loop: turn order, action selection, defense windows, damage and buildup application, victory/defeat, between-combat repair.
- Three classes function distinctly: Generic dual-wield, Caster transformation, Resonator ring-switching.
- The nine elements are recognised throughout — element comes through correctly from infusion source to spell tinting to status type.
- Infusion choice end-to-end: you can pay HP / durability / energy at commit time, and the action correctly receives the boost.
- Status effects apply, tick, and clear correctly when their underlying systems are wired (see below).
- Items work: damage, heal, energy, buff, debuff, cleanse, absorb-and-transform.
- A real-time defense moment with block / parry / dodge plays out per attack.

**What's in development (working but partially wired):**
- **Element advantage / weakness.** The 9-element matrix exists in design documents but the runtime damage calculator doesn't yet consult it — fire vs. water currently deals base damage, not the design's intended bonus. This is a known stub in three places (`CharacterData`, `DamageCalculator`, `BrokenDarknessManager`). When it lands, it lands everywhere at once.
- **Element → Status mapping for spells.** The mapping of which element produces which status effect by default is still being finalised. Currently spells use their declared `PrimaryEffect` field or fall back to damage-over-time. The "fire builds Burn, water builds Frostbite, lightning builds Paralyse" matrix is parked pending design lock.
- **Ability buildup.** Spell status buildup works correctly. Weapon-attack status buildup works correctly. Ability status buildup (special moves) is partially wired — the data asset has the field, but the runtime calculator doesn't yet read it in non-raw-mode actions. Abilities still build status via the legacy "constant + stat multiplier" formula. This is a known gap for the next session.
- **AI value-estimation drift.** The AI evaluates damage using a slightly different formula than runtime computes. Specifically, AI doesn't account for per-action stat modifiers (Reality boosts, Evolution buffs, Luck), and AI doesn't read the recently-added raw-mode / ability-buildup fields. The AI plays the right action *most* of the time but underestimates how strong some choices are. Documented; planned cleanup.
- **Conjured weapons.** The Caster's ability to summon temporary weapons that lock their spell-casting is partly implemented at the data level but not wired into the action pipeline yet.
- **Defense prompt UI.** The player gets a defense window, but the on-screen prompt widget is currently a stub. The mechanic works; the visual polish for "press X now!" is in progress.
- **Save / load.** No durable save system yet — inventory state, loadout state, character progression all exist in-memory only.

**What's known broken (small but real, not architectural):**
- Slashing weapon attacks produce Armor Break status instead of Bleed (small switch-statement bug). Pierce and Impact work correctly. Targeted fix.
- A Resonator loadout that exceeds the slot capacity reports the error twice. UI cosmetic; not gameplay-affecting.
- Two helper functions in `UItemData` report "N/A" for evolution crystals when they should report values. Edge case; affects only the editor display of evolution crystal stats, not actual gameplay.

**What's architecturally aspirational (not yet built):**
- Multiplayer beyond the local two-team framing.
- A persistent world map / campaign progression layer connecting individual fights to the Lord narrative.
- Resonator endgame: what happens when all ring crystals shatter. Currently the system auto-switches to the next non-broken ring; full breakdown behaviour is undefined.
- A formal AI hierarchy beyond difficulty levels. Current AI is one engine with tunable accuracy.

**Why these are listed as "in development" not "missing":**
The combat engine — the part the player actually plays — is real, runs, and is being refined session by session. The element-advantage matrix not consulting the damage formula isn't a missing feature; it's a parked design decision waiting on a balancing pass that wants the matrix locked first. The ability-buildup AI gap isn't a bug to file; it's a known seam that lands in the next round of consolidation.

This game has the bones of a real combat system already running. What's left is the design lock on the systems we've intentionally parked, the polish pass on the UI seams, and the campaign / persistence layer that turns a combat engine into a game.

---

## What's exciting

If a publisher (or a designer joining the project, or a playtester) asks "why this game?":

- A turn-based RPG with a fighting-game's defense feel. That blend hasn't been done well at scale. The closest analogs (*Paper Mario*'s timing, *Persona*'s One More) are simpler than what's being built here.
- A nine-element system the player can shift between every turn. Most JRPGs lock element to class, weapon, or character. Letting it shift mid-fight changes how teams are built and how individual fights flow.
- A campaign layer where defeating a character is also a recruiting tool. Most RPGs make NPCs killable or recruitable; this one makes them transferable between factions through combat outcome.
- A status-buildup model that rewards focused pressure over single-shot luck. Comp-driven status play is a fighting-game and MMO mechanic — applying it to JRPG single-player combat is novel.

The systems are connected: elements feed infusion feeds status buildup feeds character identity feeds the politics of the Lord network. The game has a thesis, not just a feature list.

---

*This document describes the game as it exists in code on 2026-05-11. It's not a marketing document; it's a snapshot. Where it says "currently works" it means a developer can launch the editor today, start a fight, and see the system function. Where it says "in development" it means the work is real and ongoing, not promised vapor.*
