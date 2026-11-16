# PIE — CS-6: after a rest, a member starts at their real full HP (audit clash 6)

**Branch:** `fix/CS-6-full-hp` (in dev, built and tested on the desktop 2026-11-16).

**What it is:** `docs/Audits/CharacterStateAudit.md` clash 6. There were two "full HP" numbers. A rest (and trial entry, a revive, a new recruit) filled a member to the character asset's formula, but a member's real max adds gear, stones and the levels earned in the run. So after a rest a member with +HP gear or Body levels started the next fight below full. Now "full" is stored as "no carried number", and each fighter fills to its own max.

**The audit's trace:** confirmed by a spec that failed first (165 HP against a real max of 188).

**Already proven by tests:** `WoR.Party.FullAfterRest` (after a rest the next pawn starts at its own full HP and EP, Body run levels included; with no rest a wounded member stays wounded). `WoR.Party.FallenMembers` (a revived member is full). Full suite in the session note.

**Not proven:** a real rest spot and the next fight in PIE.

**Known limit (not new):** the party cards (hub, pause menu, the trial's party panel) still show the asset formula's max when a member is full, not the real max with gear and run levels. Only the fight's own bars show the real max. A follow-up.

**Review fix:** the trial's party panel used "no carried number" to mean "in the hub" and would have hidden its HP bars after a rest; it now shows them whenever a run is active.

**One change to watch:** a member who broke into Broken Darkness during the run now starts the fight after a rest with empty EP, as a born Broken Darkness member does (Q-348b: "it seeds exactly as a born-BD does"). Before, a rest gave it full EP.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | In a trial, wear a piece with +Max HP (or level Body at the vendor) | The member's max HP is above the character's base | [ ] |
| 2 | Take damage in a fight, then rest at a rest spot | — | [ ] |
| 3 | Start the next fight | The member's HP bar is full: current HP equals the new max | [ ] |
| 4 | Take damage again and start another fight without resting | The member starts wounded, as the last fight left them | [ ] |
| 5 | Start a new trial | Every member starts at their own full HP | [ ] |
| 6 | Right after a rest (and right after entering a trial), look at the party panel | Each member shows its HP and EP bars, full (not just a name) | [ ] |
