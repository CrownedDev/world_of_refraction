# Enemies and Bosses

**Status:** decided by Crown, 2026-10-26 (`DesignDecisions.md` D-069), on top of the existing enemy design in `Resources_Design.md` §7.9, `AIArchitecture.md` and `EncounterCompositionSystem.md`.
Companion to `RunStructure.md`. Values marked *tune* are placeholders: named constants with `// TODO: tune` in code.

> **Naming (Crown, 2026-10-26):** "lord" / "Elemental Lord" in this doc is a **placeholder**. The playable characters are **not lords**; each will have a **real character name and lore**. Read "lord" as "the character tied to that element". (V is the one true Void Lord: `DesignDecisions.md` D-079.)

## 1. What you fight

| Kind | Who | Role |
|---|---|---|
| **Mobs** | **V**, every one of them (concept: `Characters/V_turnaround.png`) | The normal fights on the map |
| **Creatures** | Elemental creatures | Variety alongside V |
| **Mini-bosses** | A mix chosen per trial: **elite V**, **story characters** tied to the lords, **big elemental creatures** | Guard fragments; beating one creates a rest spot |
| **Bosses** | The trial lord's mirror, or the gate variation's figure (a younger self, someone related) | End of the trial |

**V:**
- 7 ft, 160 kg; black suit; a faceless head and hands with a **galaxy pattern** on a violet body base; a gold tie with a star; dark smoke wisps rising off the body.
- Built on **Manny and Quinn** (UE5's mannequins): their skeleton, rig and animations, with V's look on top.
- **One body for all mobs**, so variety comes from **gear and class** (the same inventory and loadout system as players) and **element tinting**, not new models.

## 2. On the map

- Enemies are **visible**, standing guard or patrolling. You can approach or avoid them.
- **Touching one starts the fight.**
- **First strike, both ways:** hit an enemy with a field ability before it notices you and you start with an advantage (e.g. act first, or it starts damaged, *tune*); get caught from behind and the enemy gets it. This is a real-time field ability (`GASMigration.md`).

## 3. Fight size

- Enemies roam in **teams of up to three**; touching one pulls in its team. Fights are at most **3 v 3**.
- **Several enemy teams caught in one fight:** the opponents are **the strongest member of each team**. "Strongest" uses one measure, proposed as the existing caliber (`GetTotalPool()`); confirm when building.

## 4. How strong

- Set by the **difficulty picked at the door** and **the zone on the map**: nearer the boss is tougher, and you can wander into a hard zone early.
- **No scaling to the party**, so getting stronger shows.
- Gear: normal enemies roll from their zone's item pool (source tags, `Resources_Design.md` §7.9, with floors becoming zones); bosses and special enemies are hand-built. Your drop rolls from the same pool, so harder zones drop better loot.

## 5. Bosses

- **Phases:** at set HP points the boss changes (new skills, element or stance shift, a line of dialogue).
- **Memories** you collected can alter phases (skip one, add a harder one), authored per memory (`RunStructure.md` §5).
- **Optional revival:** a beaten boss can be revived stronger for better gear (`RunStructure.md` §4).
- **Open:** how phases work in detail, to discuss with Crown. Placeholder only until then.

## 6. Open questions

- Boss phases in detail (§5).
- What the creatures are (types, looks).
- The first-strike advantage's exact form.
- More than three enemy teams in one fight.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | Created from D-069 | docs/enemies |
