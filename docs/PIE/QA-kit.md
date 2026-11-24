# PIE — QA kit: `wor.Debug.QAKit` (QA setup)

**Branch:** `chore/qa-kit` (built 2026-11-24).
**Already proven by tests:** `WoR.Debug.QAKit` (4 tests: which ability shows an HP cost on a weapon (it needs a SkillMontage), which Caster joins, a brand-new character's learn, the no-player message) and the full suite.
**Setup:** the QA save only (it changes the loaded game, and the next save keeps it). Load it in the hub, not in a run. Open the console with `` ` ``.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | `wor.Debug.QAKit skill` | Cyan line `skill: <ability> put in slot N of <weapon>` on the active loadout's primary (the Arming Sword: a skill with a montage, such as QuickStrike), maybe after `primary: ... put in place of ...` or `in hand: ...`, `(learned ... first)` or `, replacing ...`; or `already in a slot`. The last line is `fight loadout: <name> (n of m), in hand <weapon>, primary <weapon>; <weapon> slots [0] <skill> ...` | [ ] |
| 1b | A brand-new game (new QA save, a Generic): `wor.Debug.QAKit skill` in the hub | As check 1, with `(learned <ability> first)`. If it still fails, the line ends `fitting: <ability> (on member yes/no, kept yes/no, needs ...)`: send that line back | [ ] |
| 2 | Open the loadout, the primary's skill list | The skill from check 1 sits in the slot the last line names | [ ] |
| 3 | Start a fight | You hold the weapon the last line named, and Skills is not greyed | [ ] |
| 3b | Infuse that weapon to Raw level II, hover the skill | B-592b's HP cost ghost on your HP bar | [ ] |
| 4 | Back in the hub: `wor.Debug.QAKit caster` | `caster: <name> (<element>) joined, as your seat` (or `already in the party`, or `the party is full`, or no Caster asset: use `wor.Debug.TestGear refractor`) | [ ] |
| 5 | Start a fight and reach the Caster's turn | You pick its actions (not the AI); its element selector colour shows | [ ] |
| 6 | `wor.Debug.QAKit caster darkness` (a party with room) | A Darkness Caster joins, or the hint to use `wor.Debug.TestGear bd` (the played character becomes a Darkness Caster for this PIE session) | [ ] |
| 7 | `wor.Debug.QAKit caster skill` | Both parts run (`skill` is not read as an element) | [ ] |
| 8 | `wor.Debug.QAKit caster ice` | `caster: 'ice' is not an element ...`, nothing changed | [ ] |
| 9 | In a run: `wor.Debug.QAKit` | `a run is open: run it in the hub`, nothing changed | [ ] |
