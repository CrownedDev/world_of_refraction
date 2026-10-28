# Character Forms (Mortal Shell model)

**Status:** draft for discussion (Crown, 2026-10-28). Nothing here is built or in the backlog yet; it replaces the idea of a full character creator (face sliders, hair, body types), which is not planned.

## 1. The idea

- **The player starts as a formless being** with no character design: a translucent, galaxy-touched silhouette on the mannequin (it echoes V, "any and all concepts", and must look intended, never like a missing asset).
- **Characters are unlocked.** Each character is an authored person from Crown's story with a **form** (body, look, materials), **lore**, and **a loadout tied to them** (their gear, skills, evolution).
- **Picking a character gives you their form**, the way a Foundling inhabits a Shell in Mortal Shell. No character picked: formless.
- **Characters are separate from their loadouts** (Crown): you **unlock the character**, and can then **switch** between unlocked characters. The loadout belongs to the character (its starting kit and what you've unlocked for them), but the player plays *as* the character, not just with its gear.

## 2. The editor

- **One in-engine character editor** (Unreal editor tool over data assets: form, materials, loadout, lore ids) used by Crown to author characters players aspire to become.
- The same tool could later let players make their own character, if ever wanted. Not planned.

## 3. What this replaces and saves

- No character creator to build (rigging, clothing fits per body type, saving every option, syncing them in co-op).
- Aspiration is the hook (Mortal Shell's shells, Nightreign's fixed characters): players see a form and want to earn it.

## 4. Ties to what exists

- **Loadouts and the pool** (D-020, D-021): a character's loadout is what you've unlocked for that character; buying it at run start was discussed (D-021 model).
- **Memories and hidden unlocks** (D-100): a natural way to unlock a character: meet its conditions.
- **Blank-slate start / uncovering who you are** (Crown's idea, discussed in chat 2026-10-27; details still Crown's to settle): the formless being is that blank slate; class is picked at the start.
- **V's material** (`VisualDirection.md` §2f): the formless look can be a lighter cousin of V's galaxy material.
- **Co-op**: each player's current form replicates like any character choice.

## 5. Open (to discuss)

- How a character is unlocked (memory conditions, a run outcome, purchase).
- Whether switching characters is hub-only, or also at rest spots.
- What the formless being can do: full play with the starter kit, or limited until a character is picked.
- Whether a character's loadout can be edited freely or stays its own (bound gear, D-033, suggests parts stay the character's).
- Name for the formless state and for "characters" in player-facing text (Crown's).

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-28 | Draft from Crown's Mortal Shell idea | docs/character-forms |
