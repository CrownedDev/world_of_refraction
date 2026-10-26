# PIE — B-196 (spell cap): the loadout check flags spells over the holder's cap

**Branch:** `fix/B-196-loadout-spell-cap` (in `dev`). (The backlog has two B-196 rows; this is the saved-loadout spell cap one.)

**Already proven:** the editor builds and every test passes. `WoR.Loadout.Rules`, "the spell cap (B-196)" (3 cases), checks:
- a weapon at its spell cap has no finding;
- each assigned spell over the cap gets one clearable finding, on a weapon and on a primary ring;
- a loadout spell that only replaces one of the item's own spells is not flagged (the fight keeps it); the item's own extra spells get one non-clearable line.

**What changed:** a saved loadout whose weapon or ring holds more spells than the slots its gem, evolution or tier allows (Q-147) is now flagged. Before, the extra spells were dropped silently in a fight.

**Setup:**
- A Saved Loadout asset (Generic) with a primary weapon that has an F-tier evolution attached.
- Give it more weapon spells than an F evolution holds (for example 3; F holds 1).
- Output Log open.

| # | Do this | You should see | Result |
|---|---|---|---|
| 0 | Save the Saved Loadout asset (or run Data Validation on it) | A validation error per extra spell: "Spell '<name>' is over the holder's spell cap (1)" | [ ] |
| 1 | Remove spells until there is 1, and validate again | No spell cap error | [ ] |
| 2 | Start a fight with the over-cap loadout | The invalid spells are cleared at load (as other invalid loadout parts are); the fight lists only what the evolution holds | [ ] |
| 3 | Note: `GetCompareRulesString` (loadout rules debug) | Over-cap loadouts now show as a new finding against the legacy check; expected | [ ] |
