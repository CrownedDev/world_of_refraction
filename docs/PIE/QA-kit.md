# PIE — QA kit: `wor.Debug.QAKit` (QA setup)

**Branch:** `chore/qa-kit` (built 2026-11-24).
**Already proven by tests:** `WoR.Debug.QAKit` (3 tests: which ability shows an HP cost on a weapon, which Caster joins, the no-player message) and the full suite.
**Setup:** the QA save only (it changes the loaded game, and the next save keeps it). Load it in the hub, not in a run. Open the console with `` ` ``.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | `wor.Debug.QAKit skill` | Cyan line `skill: <ability> put in slot N of <weapon>` (maybe `(learned ... first)` or `, replacing ...`), or `already in a slot` | [ ] |
| 2 | Open the loadout, the weapon in hand's skill list | The skill from check 1 sits in that slot | [ ] |
| 3 | Infuse that weapon to Raw level II, start a fight, hover the skill | B-592b's HP cost ghost on your HP bar | [ ] |
| 4 | Back in the hub: `wor.Debug.QAKit caster` | `caster: <name> (<element>) joined, as your seat` (or `already in the party`, or `the party is full`, or no Caster asset: use `wor.Debug.TestGear refractor`) | [ ] |
| 5 | Start a fight and reach the Caster's turn | You pick its actions (not the AI); its element selector colour shows | [ ] |
| 6 | `wor.Debug.QAKit caster darkness` (a party with room) | A Darkness Caster joins, or the hint to use `wor.Debug.TestGear bd` (the played character becomes a Darkness Caster for this PIE session) | [ ] |
| 7 | `wor.Debug.QAKit caster skill` | Both parts run (`skill` is not read as an element) | [ ] |
| 8 | `wor.Debug.QAKit caster ice` | `caster: 'ice' is not an element ...`, nothing changed | [ ] |
| 9 | In a run: `wor.Debug.QAKit` | `a run is open: run it in the hub`, nothing changed | [ ] |
