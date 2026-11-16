# PIE — CS-5: levels earned in a run survive a save and load (audit clash 5)

**Branch:** `fix/CS-5-save-run-levels` (in dev, built and tested on the desktop 2026-11-16).

**What it is:** `docs/Audits/CharacterStateAudit.md` clash 5. The pillar levels a member earns in a run (dropped or bought), how many they bought from the vendor (its rising price), their over-cap points and the levels still waiting to be placed were not in the save. Quitting and resuming a run lost them. They are saved with the run now.

**The audit's trace:** confirmed by a spec that failed first (everything came back 0).

**Already proven by tests:** `WoR.Save.Run` (two Body levels, one bought, a Mind over-cap point and another member's three waiting levels come back after a save and load). Full suite in the session note.

**Not proven:** a real quit and resume in PIE.

**Known limit:** a save made before this change resumes with no run levels (as before); no save is wiped.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | In a trial, win fights until a member has a level to place; buy one Body level at the vendor | The vendor's next price is the second step; the member's Body is one higher | [ ] |
| 2 | Note the member's pillar levels and the waiting count (`wor.WorldStat.Print`) | — | [ ] |
| 3 | Quit to the title and load the save | The run resumes | [ ] |
| 4 | Check the same member (`wor.WorldStat.Print`, or the rest spot's Level up) | The same pillar levels and the same waiting count | [ ] |
| 5 | Look at the vendor's price | Still the second step, not the first | [ ] |
