# PIE — B-197 (AI broken ring): the AI casts a broken ring's F-tier spells

**Branch:** `fix/B-197-ai-broken-ring` (in `dev`). (The backlog has two B-197 rows; this is the AI broken ring one.)

**Already proven:** the editor builds and every test passes. `WoR.Combat.BrokenSources` (2 new cases) checks:
- an AI whose ring crystal broke can pick that ring's F-tier spell, and never its greyed higher-tier ones;
- a broken weapon crystal also greys out the loadout's own spell overrides on that weapon.

**What changed:**
- D-083 keeps a broken ring's F-tier spells castable. The player could cast them, but the AI never did: it now can.
- With a broken weapon crystal, spells a saved loadout put on that weapon are now greyed too (before, only the weapon's own spells were).

- The AI casts that F-tier spell from the ring (free, like the player), not as a paid innate cast.

**Not changed (still open):** a broken *secondary* weapon is not checked yet; only the active weapon is (new job B-204).

**Setup:**
- An enemy with a ring in its primary ring slot. The ring has an F-tier spell and a higher-tier spell. Its crystal has low durability so it breaks in the fight.
- Output Log open.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | Fight until the enemy's ring crystal breaks, then let it take more turns | It may cast the ring's F-tier spell (plain, no infusion); it never tries the higher-tier one; no "[AIDecisionManager]" Warning about a refused action | [ ] |
| 2 | As the player, with your own ring broken, open your spells | The F-tier ring spell is castable, the others are greyed with the reason (as before) | [ ] |
| 3 | Give your weapon a spell through a Saved Loadout, break the weapon crystal (`wor.BreakCrystal`) | That spell is greyed with the broken-crystal reason | [ ] |
