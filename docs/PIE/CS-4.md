# PIE — CS-4: a crystal swapped between fights stays swapped (audit clash 4)

**Branch:** `fix/CS-4-carried-socket` (in dev, built and tested on the desktop 2026-11-16).

**What it is:** `docs/Audits/CharacterStateAudit.md` clash 4. After a fight, the party keeps a copy of each member's crystals (their wear, a gem broken in the fight). The next fight used to put that copy back over the gear, so a gem you swapped on the trial map between fights came back. Now a socket change updates that copy, so the next fight starts with what you socketed.

**The audit's trace:** confirmed by a spec that failed before the fix. One detail was wrong: the gem you take out is destroyed (Crown's rule, CL-28b), not put back in the pouch. So the bug brought the old gem back from nowhere and lost the new one; nothing doubled in the pouch.

**Already proven by tests:** `WoR.Party.CarriedSocket` (a gem swapped between fights is the one in the sword at the next fight, on the owned piece and the worn one; neither gem is in the pouch; with no swap the last fight's wear still carries). `WoR.Party.CarriedState` and `WoR.Party.RunWearByGuid` still pass. Full suite in the session note.

**Not proven:** the trial map's Equipment screen and a real second fight in PIE.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | In a trial, socket a Garnet in your weapon, then win a fight | The fight shows the Garnet's element | [ ] |
| 2 | On the map, open the Equipment screen: take the Garnet out, put a Sapphire in | The Garnet is gone (not in the pouch), the Sapphire is in the weapon | [ ] |
| 3 | Walk into the next fight without resting | The weapon has the Sapphire, not the Garnet | [ ] |
| 4 | Win that fight and look at the weapon on the map | Still the Sapphire, with this fight's wear on it | [ ] |
| 5 | Fight again without changing anything | The Sapphire starts the fight at the wear the last fight left | [ ] |
