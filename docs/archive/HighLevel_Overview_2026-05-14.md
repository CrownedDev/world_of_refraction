# World of Refraction — High-Level Overview
Last updated: 2026-05-14

## What It Is

*World of Refraction* is a third-person turn-based combat RPG. Fights take place between two teams of up to nine characters each, arranged on a 3×3 grid. The campaign frame is single-player; the combat frame is JRPG-paced with a real-time defense moment layered inside each enemy turn.

## The Pitch

You command a Lord — a persistent identity at the centre of a network of allied and rival champions. Every fight you pick a team, choose a class for each member, and equip them with weapons, rings, and crystals that determine how they fight. Combat is deliberate: pick the action you want, then watch it play out. But when an enemy attacks you, you get a brief window to react — block, parry, or dodge by pressing the right button at the right moment. The same character can change elemental identity every turn by choosing what powers each action, so two turns in a row never have to look the same. Champions you defeat in the world can be recruited; champions you train can later be lost. Each fight changes who you are and who follows you.

## Player Fantasy

You are the strategist and the swordsman at the same time. You decide what the team does, then you react in the moment to what comes back at you. The character you build today is the character you bring into a defense window five minutes from now — and whether that character lives or dies depends on whether you read the timing right. The game asks you to think like a coach and play like a fighter.

## Setting and Tone

A semi-realistic world rendered in the visual register of *Clair Obscur: Expedition 33* — painterly lighting, grounded human characters, dramatic weather. Combat encounters are arena-staged with cinematic camera framing rather than open-world traversal. The tone is measured and serious, not satirical. Weather is a leader-driven force that visibly changes the arena: a Caster leader's element tints the sky and shifts the stat balance of every combatant on their side.

## The Three Classes

- **Generic** — the all-rounder. Wields one or two physical weapons. No innate magic. Most durable, most direct.
- **Caster** (display name: **Refractor**) — magic-first. Casts through one innate element. Can transform into a temporary "Broken Darkness" state that absorbs incoming damage and turns it back into hybrid spells.
- **Resonator** — equipment-driven. No innate element. Equips rings, each carrying a crystal of one element. Switches between rings mid-combat to switch elemental identity.

## The Nine Elements

The element system is the centerpiece of build expression. A spell is a shape (fireball, beam, area, support) crossed with an element. The same shape can come out as different elements depending on which character is using it and which crystal or ring powers it. The nine elements:

**Fire, Water, Earth, Wind, Lightning, Light, Darkness, Void, Reality.**

(Plus *Generic* — raw, elementless force — and *Broken Darkness*, the Caster's transformed state. These exist in the runtime enum but sit outside the nine.)

## What Makes It Distinct

- **Real-time defense inside a turn-based frame.** You take turns like a JRPG, but every incoming hit asks for a button-press reflex. Block / parry / dodge windows turn the defender into an active participant on the attacker's turn.
- **Element-at-the-action-level.** Most RPGs lock element to class or weapon. Here, the same character can shift between elements every turn by choosing what powers each cast — your innate element, a slotted crystal, a ring, or a special evolution crystal.
- **Status as buildup, not as a roll.** Status effects don't fire on a single coin-flip. Every relevant hit fills a buildup bar; sustained focused pressure pays off, single-shot luck doesn't. Team composition matters because of this.
- **The Lord-and-champions political layer.** Defeated enemies can be recruited; trained allies can be lost to defeat. The persistent identity at the centre of the campaign accumulates and loses champions over time, and the political layer is woven through combat outcomes.
- **A 3×3 grid that matters.** Position controls how much damage you give and take. Front-row hits land harder both ways; back row is a calculated safety.

## Current State (Honest)

The combat engine is real and runs. A player can launch the editor today, start a fight, and play a turn end-to-end: pick an action, choose a target, watch the animation, react in the defense window, see the hit land, watch the status bar fill, and eventually trigger the resulting effect. Three classes function distinctly. Items, weather, and broken-darkness transformations work. Crystal durability is honoured and crystals can break. Between combats, broken equipment is repaired or destroyed.

What's still being built out: the campaign and persistence layer (no save system yet), in-game character creation (characters are currently authored as designer-side data assets), the element advantage matrix (the design exists but the runtime damage calculator doesn't consult it yet), and several polish-pass items on the UI side. The combat-loop bones are here; the surrounding game systems are being layered on top session by session.

## Audience and Comparables

The intended player is someone who likes systems-driven turn-based combat — *Persona*, *Octopath Traveler*, *Sea of Stars*, *Clair Obscur: Expedition 33* — but who also wants more reflex texture than a typical JRPG offers. The defense-window mechanic is closest to *Clair Obscur*'s parry timing and *Paper Mario*'s action commands, but applied to a deeper build-and-element system than either. The character-conversion layer reaches toward what *Fire Emblem* does with recruitable enemies and what *Suikoden* does with army-scale rosters, but routed through individual combat outcomes rather than menu negotiation.

## Status Toward Publisher Pitch

The combat engine is demo-ready in shape but not yet in polish. Before a publisher meeting is worth taking, three things need to land: (1) the element advantage matrix wired into the damage formula, so element matchups read as designed; (2) the defense prompt UI completed (the mechanic works but the on-screen "press X now!" widget is a stub); (3) a small set of representative encounters built end-to-end with finished animations, VFX, and a clear win/lose flow. Character creation, save/load, and the campaign layer do not need to be built for a pitch — they need to be articulable and visibly designed. The thesis of the game — turn-based combat with a fighting-game's defense feel and an element system the player can shift between every turn — is already playable in the editor and forms the demonstration target.
