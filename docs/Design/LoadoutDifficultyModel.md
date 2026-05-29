# Loadout as Difficulty Selector — Design Note

## Idea
Authored enemy difficulty is expressed by **selecting which loadout** from the enemy character's loadout array, not by separate enemy variants or stat multipliers.

A given enemy template (e.g. "Fire Wizard") has multiple loadouts authored at ascending power tiers:

- **Loadout 0 = Easy** (basic gear, low-tier crystals, fundamental spells)
- **Loadout 1 = Normal** (mid-tier gear)
- **Loadout 2 = Hard** (strong gear, refined crystals, full spell complement)
- **Loadout N = Nightmare** (full kit, rare crystals, peak power)

Encounter difficulty determines which loadout index the enemy is spawned with.

## Why this shape
- Reuses existing loadout infrastructure — no new "difficulty multiplier" system.
- Power difference is **legible** to the player ("oh, they have the Citrine crystal now") rather than opaque stat scaling.
- Each tier is hand-authored, so encounters feel intentional rather than proceduralized.
- One enemy template covers all difficulty levels — less designer work than separate `Fire_Wizard_Easy` / `Fire_Wizard_Hard` templates.

## Scope: AUTHORED ENEMIES ONLY
This model does **not** apply to player-uploaded character-to-enemy content. Character-to-enemy difficulty is determined by other mechanisms (TBD — see gap 10.7 / character-to-enemy design). When a player uploads a character, their explicit loadout(s) are used as-authored; the system does not re-tier their build.

## Open questions (for future decision)
- **Mapping:** is loadout 0 *always* easy, or does the encounter spec name which index to use? (e.g. "this boss uses loadout 2 even on easy")
- **How many tiers?** 2 (easy/hard), 4 (easy/normal/hard/nightmare), or per-enemy-flexible?
- **Cross-difficulty consistency:** if loadout 1 is "normal," do all enemies follow the same power-curve at index 1, or does each enemy define its own tier semantics?
- **Difficulty selection:** global setting? Per-encounter? Adaptive?
- **Authoring tooling:** how does a designer ensure tiers are balanced relative to each other?

## Status
Idea captured 2026-05-29 from session discussion. Not yet specified, not yet implemented. Awaiting further design work.

## Cross-references
- **gap 10.7** (`AutoPopulateLoadout`) — *not* this; auto-populate's home is character-to-enemy, not authored-enemy difficulty.
- **Character-to-enemy system** — distinct from this; uses player-authored loadouts directly.
