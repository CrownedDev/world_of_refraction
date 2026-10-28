# Character Forms (Mortal Shell model)

**Status:** draft for discussion (Crown, 2026-10-28). Nothing here is built or in the backlog yet; it replaces the idea of a full character creator (face sliders, hair, body types), which is not planned.

## 1. The idea (Crown, 2026-10-28)

- **The player starts as a formless being** with no character design: a translucent, galaxy-touched silhouette on the mannequin (it echoes V, "any and all concepts", and must look intended, never like a missing asset).
- **Each authored character has a loadout**: their weapons, rings, skills, evolution (and lore).
- **You build your own loadout.** Every piece you equip that belongs to a character's loadout **fills a meter for that character**; the more of their pieces you use, the faster it fills (it fills through play, not at once).
- **At 100% you unlock the character's skin** (their form). The unlock also **reveals their full loadout**: all their weapons, skills and so on, so you see what else they used.
- **Wearing the skin makes their bound gear work** (D-033's owner bonus): one of the benefits of playing as them.
- **How it fills (Crown, 2026-10-28: A):** **per fight won**, by how many of the character's pieces were equipped in that fight (1 of 6: a trickle; all 6: fast). An **exact piece counts in full**; the **same piece at a different tier counts for less** (`// TODO: tune` the amounts). Monster Hunter's weapon mastery / Hades' affinity: visible progress after every fight.
- **Switching (Crown, 2026-10-28: A):** once unlocked, you can switch skins **in the hub and at rest spots** (where loadouts change); never in a fight.
- No character creator (face sliders, hair, body types) is planned; the skins are the customisation.

## 2. The editor

- **One in-engine character editor** (Unreal editor tool over data assets: form, materials, loadout, lore ids) used by Crown to author characters players aspire to become.
- The same tool could later let players make their own character, if ever wanted. Not planned.

## 3. What this replaces and saves

- No character creator to build (rigging, clothing fits per body type, saving every option, syncing them in co-op).
- Aspiration is the hook (Mortal Shell's shells, Nightreign's fixed characters): players see a form and want to earn it.

## 4. Ties to what exists

- **Loadouts and the pool** (D-020, D-021): the player's own loadout drives the meters; a character's authored loadout is the reference it's matched against.
- **Memories and hidden unlocks** (D-100): the meter is the visible path; hidden conditions could add to it or unlock extra lore for a character.
- **Blank-slate start / uncovering who you are** (Crown's idea, discussed in chat 2026-10-27; details still Crown's to settle): the formless being is that blank slate; class is picked at the start.
- **V's material** (`VisualDirection.md` §2f): the formless look can be a lighter cousin of V's galaxy material.
- **Co-op**: each player's current form replicates like any character choice.

## 5. Open (to discuss)

- What the formless being can do: full play with the starter kit, or limited until a character is picked.
- Whether a skin changes anything beyond looks and bound gear (stats, a signature skill).
- Name for the formless state and for "characters" in player-facing text (Crown's).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-28 | Draft from Crown's Mortal Shell idea | docs/character-forms |
| 2026-10-28 | Switch skins in the hub and at rest spots (Crown) | docs/character-forms-switch |
| 2026-10-28 | Meter fills per fight won by matching pieces, partial credit for other tiers (Crown) | docs/character-forms-fill |
| 2026-10-28 | The loadout-match meter unlocks the skin and reveals the loadout; bound gear works on the skin (Crown) | docs/character-forms-meter |
