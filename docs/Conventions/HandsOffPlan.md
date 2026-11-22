# Hands-off plan

**Status: APPROVED (Crown, 2026-11-22: "Approve and start", D-251). Checkpoint 1: first run FAILED, 2026-11-22 (28 of 38 decided, 30 needed; the text-only tiles were found unprompted). Next: the test tool (B-573), then run again.**

Crown: "I've become involved in the process again due to the detailed aspects of this part of the project, we need to make a plan to remove myself from it."

## The problem, measured on 2026-11-22

| What | Number |
|---|---|
| PIE sheets waiting on Crown / done | 635 / 12 |
| Backlog jobs `REVIEW (in dev)` | 583 of 752 |
| Commits on `dev` not yet on `main` (last updated 2026-10-26) | 8,623 |
| Functional or screenshot tests found in `Source/` | 0 |

Crown is the only tester, so every job ends at Crown. In the weapon and menu work the brain also handed Crown jobs it should have kept: spotting reversed axes and text-only tiles, judging rotations one turn at a time, reporting what a screen looks like.

## Who does what

| Step | Before D-251 | Now |
|---|---|---|
| "Does it work?" | Crown plays the sheet | A QA agent plays it in PIE and attaches a screenshot per check |
| "Does it match the mockup?" | Crown screenshots | A second agent that didn't build it compares screenshot and mockup |
| Positions, grips, rotations | Crown reads off the problem each turn | The brain iterates from front, side, top and back captures; Crown gets the finished sheet |
| Small design choices | Crown is asked | Reference-backed default, listed in the milestone report |
| Closing and reopening Unreal | Crown closes it | The brain does it, whenever Crown isn't using the editor |
| Next piece | Waits for Crown's look | Starts when QA passes |
| Feel, sound, taste | Crown, per piece | Crown, at checkpoints 3, 4, 6 and 7 |
| Lore, money and licences, merges to `main` | Crown | Crown (unchanged) |

Crown's target for the first milestone: three play sessions (10, 5 and 20 minutes) and the final lock, at most 10 questions, no sheets.

## Order of milestones (Crown, 2026-11-22)

1. **Weapons and abilities:** one full battle, tested, refined, locked. No spells.
2. **Spells:** starts only after 1 is locked, with its own quality bar, because spells bring their own problems.

Crown: "we should be able to have one full battle with weapons and abilities which we can test refine etc then we move onto spells".

## Milestone 1: the fixed set

| Fighter | Weapons | Normal attack | Ability 1 | Ability 2 |
|---|---|---|---|---|
| Generic A | Sword and shield | Light Hit | Sniping Flurry (chain) | Covered Cleave (one big hit) |
| Generic B | Two axes | dual-axe attack | Whirling Axe (hits the group) | Shield Splitter (breaks a guard) |

- Both are Generic: only a Generic's weapon entry holds a pair (CL-23b), and the other classes' differences are mostly spells, which are milestone 2.
- Two styles, because they are the two with animation files saved in the project (sword and shield: Light Hit, Attack, Slash, Combo1; dual: six attacks and the whirlwinds).
- Enemies: a mirror of the same two (FeelGuide's Maelle vs Painted Maelle reference; telegraphs belong to the skill).
- Also: Guard, Perfect Guard, Dodge, weapon hit effects and sounds, one potion, one arena.
- Parked, not deleted: spells, adapters, crystals and elements, greatswords, bows, guns, every other ability.
- To confirm at checkpoint 2: both loadouts are legal, each ability does what its name says, and an existing animation fits it. A piece that fails is swapped for the nearest that passes and named in the report.

## Milestone 2: spells (not started, set confirmed when 1 locks)

Bolt (one enemy), Wave (the group), Mend (an ally): Generic spells, so each is built once and takes the caster's element (ED-07). It runs checkpoints 2 to 7 again, with Bolt as the piece judged at checkpoint 4.

## Checkpoints

| # | Claude does | Done when | Crown |
|---|---|---|---|
| 1 | Trial run: plays the two newest menu sheets (38 checks) | 30+ decided with screenshots, and the known text-only tiles found unprompted. On a fail: build the test tool, run again | reads a short result |
| 2 | Prepare: finish D-250, build the two loadouts and the mirror, list animation and sound gaps | both load and are legal; every gap has an owned asset or is on one shopping list | answers only if money is needed |
| 3 | Rough fight: start to finish with the set, unpolished | agent wins and loses it, uses every skill and defence, log clean | **plays 10 min: is this the right fight?** |
| 4 | One ability to final quality (Sniping Flurry): animation, camera, hit, effect, sound, defence timing | agent checks pass from both sides (using it, defending it) | **plays 5 min: is this the quality bar?** |
| 5 | The other three abilities and both attacks copy that bar; entry, victory, return, battle screen | every piece has before / after captures and passes QA | nothing |
| 6 | Full fight, handed over as finished | full QA pass, 60 fps, defaults listed | **plays 20 min: approve, or give notes** |
| 7 | Refine from the notes (two rounds at most), then lock | Crown approves | **approves the lock** |

After the lock: milestone 2 (spells). Further weapon types come later, one at a time, copying the locked fight.

**Claude stops and asks only when:** money or a licence is needed, it is story, it would break a ruling, the same fault survives two fixes, or a checkpoint can't pass. Each checkpoint ends with a report: done, screenshots, decisions taken, problems, next.

**The waiting PIE sheets:** only those touching this fight are played, by the agent, marked "QA passed (agent)" and never "Crown passed". The rest are parked until their feature joins the set.

## Rules changed by D-251

| Rule before | Now |
|---|---|
| Crown sees each animation, spell and UI piece (`VisualGameplayPlan.md` 2, D-248 / D-249 build order, the visual-phase working habit) | Agent QA per piece, Crown at checkpoints 3, 4, 6 and 7 |
| Crown checks the look before code and data are aligned (the four UI steps of 2026-11-15) | The second agent checks the look against the approved mockup |
| Game assets are committed after Crown's PIE pass | Committed to a job branch and merged to `dev` after agent QA |
| Old code is removed after Crown's PIE pass | Unchanged: removed only after Crown's checkpoint 6 session |
| `main` is Crown's | Unchanged |

## The reactive turn-based template: not the base
- Its manual has every skill built by hand in Sequencer (about 10 steps: bind, camera, cuts, damage event, telegraph, QTE). That adds detail work for Crown.
- The user supplies character animations and montages. It is Blueprint only, documents no co-op, and updates by re-migrating its folder.
- Our turn rules, timed defence, saves and tests would all have to be rebuilt inside it.
- It may be bought later as a feel reference. Nothing in this plan depends on it.

## How an agent plays
- Open the editor with `-saveddirsuffix=QA -ModelContextProtocolStartServer`: saves go to `Saved_QA/`, never Crown's. Stopping Play always writes the last-used slot, so never play-test without it.
- Start PIE in its own window (Play > New Editor Window): only that window can be resized.
- Drive the game with the `WoRAgentPlayToolset` tools below (B-573), not the Slate inspector's `PressKey`: that one presses and releases in the same frame, which the game's keys never see. Esc ends PIE.
- The brain checks each verdict against its screenshot. A check only partly seen is not a pass.

### The play tools (`WoRAgentPlayToolset`, editor only)
Ask unreal-mcp to describe the toolset for the exact argument spelling. Mouse points are pixels of `Screenshot`'s picture (the game alone, top left is 0,0). Key names are Unreal's: `Tab`, `SpaceBar`, `Enter`, `Escape`, `W`, `Left`, `Gamepad_FaceButton_Bottom`. Pad buttons work, sticks don't. Every input tool answers with what it sent, the real time in ms and the frame.

| Tool | Worked example | What it does |
|---|---|---|
| `Screenshot` | `Screenshot()` | The game picture, to read and to take mouse points from |
| `TapKey` | `TapKey(Key="Tab")` opens the game menu. `TapKey(Key="SpaceBar", HoldMs=600)` is a hold | Press, then release `HoldMs` later (50 by default), never in the same frame |
| `KeyDown` / `KeyUp` | `KeyDown(Key="W")`, look, `KeyUp(Key="W")` | A key left down across calls |
| `MoveMouse` | `MoveMouse(X=640, Y=360)` | Puts the agent's own pointer on that point of the game and leaves it there (hover). The real pointer is not moved |
| `Click` | `Click(X=1180, Y=64)` clicks the Loadout button. `Button="RightMouseButton"` for the right one | Move there, press, release |
| `Wheel` | `Wheel(X=900, Y=500, Notches=-3)` scrolls the grid down three notches | Move there, turn the wheel |
| `PlaySequence` | Perfect Guard: `Steps=[{AtMs:0, Action:"Screenshot"}, {AtMs:850, Action:"KeyTap", Key:"SpaceBar"}, {AtMs:900, Action:"Screenshot"}, {AtMs:1400, Action:"Screenshot"}]`, `ScreenshotDir="docs/PIE/qa/2026-11-23/guard"` | Inputs and screenshots at set times in one call, each sent on the first frame at or after its time. The game keeps running. PNGs are named `shot_step<step>_<ms>ms.png`; with no folder they go to `Intermediate/AgentPlay`. Actions: `KeyDown`, `KeyUp`, `KeyTap`, `MouseMove`, `MouseDown`, `MouseUp`, `MouseClick`, `MouseWheel`, `Screenshot`. Two minutes at most, one sequence at a time |
| `SetWindowSize` | `SetWindowSize(Width=1280, Height=720)` | The game picture becomes that size (take a new screenshot after it) |
| `RunConsoleCommand` | `RunConsoleCommand(Command="wor.Party.Print")` | Runs it in the game's world and returns what it printed and logged. The console is not opened and the keys stay where they were |
| `GetState` | `GetState()` | PIE running or not, hold taken or released, picture and window size, where keyboard focus and the agent's mouse are, how much of a sequence is left |
| `Release` | `Release()` when a person needs this editor's mouse back | Stops a playing sequence and gives back the hold (below). Stopping PIE does the same |

- **The hold (a person can use the PC while the agent plays):** the first play tool of a PIE session takes it; `Release` or the end of PIE gives it back. While it is taken:
  - The mouse the game sees is the agent's own pointer, which exists only inside this editor. The real pointer is never moved, and moving it does nothing to the game, with Unreal in front or behind another app.
  - The editor's "use less CPU when in background" setting is off, in memory only (never saved), so the game keeps full speed behind another app and timed inputs land on time.
  - In this editor, hovering with the real mouse shows nothing; a real click still lands where it is made (so Stop still works).

- **Timing a defence:** run a `PlaySequence` of screenshots only across the enemy's attack first, read the hit's time off them, then run it again with the key at that time. The answer's `AtMs` says when each input really went in.
- **Keys go nowhere?** `GetState` shows where keyboard focus is. A key step gives focus to the game when it is outside it; a `Click` on the game does the same as a player's click.
- **Debug text over the picture:** `RunConsoleCommand(Command="DisableAllScreenMessages")` hides the cyan debug lines before a screenshot that will be judged.

### Test setup (console commands, hub only)
| Command | Gives |
|---|---|
| `wor.Debug.GiveTestKit` | Everything below, found by kind, so no asset path is needed |
| `wor.Debug.GiveTestKit apparel` | Apparel up to 60 pieces: with the weapons and adapters, a Gear grid past one screen |
| `wor.Debug.GiveTestKit adapters` | Adapters until their room is full |
| `wor.Debug.GiveTestKit evolution` | Two evolution items |
| `wor.Debug.GiveTestKit skill` | Three learned abilities, the longest names first |
| `wor.Debug.GiveTestKit member` | A second party member: the recruitable character in the level, else another character in the played one's body |
| `wor.Debug.GiveTestKit weapons` | `wor.Debug.GiveTestWeapons` (12; the 20 weapon room is unchanged) |

The answer has one line per part and says what was refused. Several parts can be named at once.

## Limits
- An agent can't hear, and judges timing and feel poorly from screenshots. Those stay with Crown's checkpoints.
- The editor connection has no import tool, so asset imports still need Crown until one is built.
- The weekly usage limit has already stopped the Writer once; it sets the pace, so no dates are promised.
