# World of Refraction: game rules reference

This is the one place that states **this game's rules**: what the player experiences and what the game enforces. It is shaped like the deckbuilder's `docs/reference/` (an index plus one page per topic). It isn't architecture documentation; for classes, delegates and data flow see `docs/Architecture/`.

**Start here for any rules question.** If this game has no rule and the deckbuilder does, the deckbuilder's rule is the suggested answer in a `D-xxx` entry (CLAUDE.md, "Rules reference").

## How to read a page

Every rule is one row: an ID, the rule in plain words with its real numbers, a status, and where it comes from.

| Status | Meaning |
|---|---|
| **In game** | The shipped code does this. Checked against the C++ when the page was written. |
| **Decided, not built (B-xxx)** | Crown has ruled (a `D-xxx` entry); the backlog job that builds it is named. |
| **Code only (no ruling)** | The code does this, but Crown has never ruled on it. It may change. |
| **Conflict** | Sources disagree. The page's **Conflicts** section says how, and links the `D-xxx` question or the `B-xxx` fix. |

**Sources, in order of authority:** Crown's rulings in `docs/DesignDecisions.md` (cited `D-0xx`); then the code (cited `path:line`, relative to `Source/world_of_refraction/`); then the design and mechanics docs, cited only when a rule exists nowhere else. When a ruling and the code disagree, the ruling is the rule and the code is the conflict.

Each page ends with **Open questions** (unanswered `D-xxx`; the D-entry holds the options and recommendation, the page only the facts), **Conflicts** (each linked to a D or B number) and **Gaps** (things the player needs a rule for that nobody has written).

## Pages

| # | Page | Prefix | What it covers |
|---|---|---|---|
| 01 | [Classes and loadouts](01-classes-and-loadouts.md) | CL | Generic, Refractor (`Caster`) and Resonator; slots and caps; weapons, rings, crystals, stones, fusions, evolutions; Refractor innate spells; pillars and requirements |
| 02 | [Elements and damage](02-elements-and-damage.md) | ED | The nine elements, Generic and None; how a skill's element is chosen; every damage step with its constants; resistance; infusion L0–L2; real-time defense; Broken Darkness and Reality on the damage side |
| 03 | [Status bars](03-status-bars.md) | SB | What fills each bar and how fast; resistance; draining; what each full bar does and for how long; the stun rules |
| 04 | [Effects and turns](04-effects-and-turns.md) | ET | Turn order and bonus/Execution turns; what a turn allows and costs; effect durations, ticking, stacking, identity, cleanse, gear effects and auras; how a fight starts and ends |
| 05 | [Economy and runs](05-economy-and-runs.md) | ER | Currencies and rewards; hub, trial and battle; shops; upgrading, merging, dismantling; wear and repair; drop quality; the shared pool; encounters |
| 06 | [Party](06-party.md) | PA | Building the party; who goes into battle and who controls them; characters found in a trial; HP/EP between fights; death; the combat grid; AI-controlled characters; co-op |

## Status at a glance (2026-10-26)

254 rules across the six pages. The written-but-not-built rulings are mostly in economy (B-073, B-072, B-090, B-091) and status-bar caps (B-062, B-063, B-064a).

Writing the pages turned up 23 new questions for Crown, **D-034 to D-056**, and six fix jobs, **B-112 to B-117**:
- B-112: a full Light bar does nothing.
- B-113: Status Multiplier may count twice.
- B-114: the AI's buildup estimate.
- B-115: durability and absorption carry-over.
- B-116: the run-start loadout purchase.
- B-117: a stale-text sweep.

Six jobs now wait on these questions: B-062, B-063, B-064, B-071, B-073 and B-080.

## Keeping it current

- When a `D-xxx` is answered, update the rows it affects (status and source) in the same change that applies it.
- When a job builds a "Decided, not built" rule, flip it to **In game** and update the citation.
- New topics get a new numbered page and a row above; rule IDs are never reused.
