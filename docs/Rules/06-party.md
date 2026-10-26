# Party

This page covers who fights on your side: building the party in the hub, its size and leader, how members get into a battle and who controls them, characters found during a trial, HP/EP carrying between fights, death and revival, the 3x3 combat grid and its rows, co-op players, and how AI-controlled characters (allies and enemies) play and defend. What a character can equip and its class rules are in 01-classes-and-loadouts.md; damage maths in 02-elements-and-damage.md; status bars in 03-status-bars.md; turn order and effects in 04-effects-and-turns.md; runs, rewards and win/lose routing in 05-economy-and-runs.md.

Code paths are relative to `Source/world_of_refraction/`. The deckbuilder is 1v1 and has no party, grid or ally rules, so it offers no reference for this page.

## Building the party

| # | Rule | Status | Source |
|---|---|---|---|
| PA-01 | The party holds at most **3** members (`MAX_PARTY_MEMBERS = 3`). No reserves or bench. An invite to a full party is refused. | In game | `Party/Party.h:28`; `Party/PartySessionSubsystem.cpp:153-158` |
| PA-02 | You start as a party of one: your own character is slot 0, the **leader**. The party is created when the hub loads (or, failing that, at the first encounter), and returning to the hub keeps the party you built. | In game | `Party/PartySessionSubsystem.cpp:25-58`, `:60-94`; `Combat/Encounter/EncounterComponent.cpp:104-109` |
| PA-03 | **Recruiting:** walk up to a recruitable character and press Interact to invite them; press Interact again on a member to dismiss them. Only a player-controlled pawn triggers it, within 200 units (`DEFAULT_INTERACTION_RADIUS`) and in front of the camera. | In game | `Party/PartyInteractionComponent.cpp:17`, `:79-115`, `:170-190`; `docs/Architecture/PartySystem.md` (Party Assembly POC) |
| PA-04 | The leader (slot 0) can't be dismissed. | In game | `Party/PartySessionSubsystem.cpp:205-213` |
| PA-05 | The same character can't be in the party twice (identity is the character's data asset). | In game | `Party/PartySessionSubsystem.cpp:174-193` |
| PA-06 | You build the team in the hub before a run, from the same character templates you can also find in trials. | Decided, partly built (hub recruiting works; trial finding is B-076) | `D-003`; `D-026` |
| PA-07 | Each member can be given a preferred formation cell (row + column). Moving a member onto an occupied cell swaps the two. | Code only (no ruling); not used in battle, see Cp-2 | `Party/PartySessionSubsystem.cpp:241-299`; `Party/Party.h:75-82` |

## Going into battle

| # | Rule | Status | Source |
|---|---|---|---|
| PA-08 | When an encounter starts, the battle side is taken **from the party list**, not from who is standing near the enemy: every living member fights. Today an encounter is your party against **one** enemy (the encounter's owner). | In game | `D-003` (A); `Combat/Encounter/EncounterComponent.cpp:210-260` |
| PA-09 | Each member spawns as **their own pawn** (`FPartyMember.PawnClass`), next to the leader. | Decided, not built (B-071): today every member spawns as `DefaultPlayerPawnClass` | `D-003`; `Trial/BattleGameMode.cpp:129-137` |
| PA-10 | A Generic or Resonator with no gear is left out of the fight; the fight starts with the others. A Caster ("Refractor") can enter gearless (innate). | Decided, not built (B-071) | `D-016` notes; Backlog B-071 |
| PA-11 | The **leader is always player-controlled.** Each other member has a Player/AI setting, chosen on the hub party screen (default Player). A Player member uses the normal command menu on its turn; an AI member uses the AI. | Decided, not built (B-071) | `D-004` |
| PA-12 | Today only the first spawned member (the leader) is player-controlled; **every other member plays on AI** automatically. | In game (to be replaced by PA-11) | `Trial/BattleGameMode.cpp:163-176`; `Combat/CombatOrchestrator.cpp:1028`, `:1225-1236` |
| PA-13 | You can switch a member between you and the AI **mid-fight, on that member's turn** (a command-menu entry). | Decided, not built (B-071) | `D-026` (2) |
| PA-14 | Defending: you press defence inputs only for player-controlled characters; an AI-controlled ally defends itself the same way an AI enemy does (PA-33..PA-36). | In game | `Combat/Actions/ActionExecutor.cpp:1909-1920`; `Combat/Defense/DefenseSystem.cpp:250-275` |

## Characters found during a trial

| # | Rule | Status | Source |
|---|---|---|---|
| PA-15 | You can find characters during a trial and add them to the party (same recruit flow as the hub, same character templates). | Decided, not built (B-076) | `D-003`; `D-026` |
| PA-16 | A character found in a trial is in the party **for that run only** and leaves when the run ends (no roster or save change). | Decided, not built (B-076) | `D-026` (1) B |
| PA-17 | Found characters arrive **ungeared**. You equip them from the run inventory, which the whole party shares; gear stays in the run inventory when unequipped or when the character leaves. | Decided, not built (B-076) | `D-026` (1); `D-020` |

## HP and EP between fights

| # | Rule | Status | Source |
|---|---|---|---|
| PA-18 | Entering a trial sets every member to full HP and full EP (a Broken Darkness-innate member starts at 0 EP). | In game | `Trial/TrialRunSubsystem.cpp:37-41`; `Party/PartySessionSubsystem.cpp:370-395` |
| PA-19 | Within a trial, each member's HP and EP at the end of a fight carry into the next fight. There is no healing between fights. | In game | `D-013`; `Trial/BattleGameMode.cpp:260-298`; `Character/CharacterDataComponent.cpp:73-90` |
| PA-20 | A carried value is clamped to the member's current maximum when the next fight starts (e.g. after a gear change lowered max HP). | In game | `Character/CharacterDataComponent.cpp:82-85` |
| PA-21 | Returning to the hub wipes the carried values, so the next trial starts fresh. | In game | `Trial/TrialRunSubsystem.cpp:59-64`; `Party/PartySessionSubsystem.cpp:397-412` |
| PA-22 | Status bars do not carry: they start empty every fight. | In game | `D-013` (details in 03-status-bars.md) |

## Death and fallen members

| # | Rule | Status | Source |
|---|---|---|---|
| PA-23 | A character dies when HP reaches 0, unless it holds a Last Stand ward: then one charge is used and it stays alive at the ward's percent of max HP (default `LAST_STAND_HP_PERCENT = 50`; a 0 value falls back to 30%). | In game | `Character/CharacterDataComponent.cpp:410-447`; `Inventory/ItemConstants.h:20-26` |
| PA-24 | In a fight, a dead ally can be revived with Sapphire (defy-death): it comes back at **30%** of max HP (`REVIVE_HP_PERCENT = 0.3`). On a living ally Sapphire grants a Last Stand ward instead. | In game | `Inventory/ItemExecutor.cpp:290-345`; `Inventory/ItemConstants.h:20` |
| PA-25 | Dead characters take no turns and can't be targeted as living allies or enemies. | In game | `Combat/TurnManager.cpp:627`; `Combat/CombatOrchestrator.cpp:1238-1268` |
| PA-26 | A fight ends when one side has no living members: your side alive only = Victory; the enemy side alive only = Defeat; both wiped = Draw. | In game | `Combat/CombatOrchestrator.cpp:1065-1083` |
| PA-27 | A member who is dead when a fight ends stays dead for the rest of the trial: they are left out of every later encounter. No between-fight revive exists. | In game (see Cp-3) | `Party/Party.cpp:21-44`; `Combat/Encounter/EncounterComponent.cpp:225-228` |
| PA-28 | If the whole party is dead, the next encounter does not start (it re-arms). Per Crown, losing a fight ends the run and sends you to the hub. | Code is a stopgap; the ruling is Decided, not built (B-072) | `Combat/Encounter/EncounterComponent.cpp:236-251`; `D-002` |

## The combat grid

| # | Rule | Status | Source |
|---|---|---|---|
| PA-29 | Each side stands on its own **3x3 grid**: rows Back / Middle / Front, 3 columns. At most 3 characters a side (`MAX_TEAM_SIZE = 3`). | In game | `Combat/Grid/CombatGridConstants.h` (`GRID_ROWS`, `GRID_COLUMNS`, `MAX_TEAM_SIZE`) |
| PA-30 | Row gives a passive modifier. **Front:** damage dealt x1.05, defence x0.95. **Middle:** none. **Back:** damage dealt x0.95, defence x1.05. Damage taken is divided by the defence modifier (Back takes about 4.8% less, Front about 5.3% more). | In game (but see PA-31) | `Combat/Grid/CombatGridConstants.h` (`FRONT_ROW_*`, `BACK_ROW_*`); `Combat/Damage/DamageCalculator.cpp:134-140`, `:266-273` |
| PA-31 | At fight start both sides are placed in the **Middle** row, centre column first (leader centre, then left, then right). So today nobody starts in Front or Back and row modifiers never apply. | In game | `Combat/CombatOrchestrator.cpp:214-215`; `Combat/Grid/CombatGridSubsystem.cpp:279-318` |
| PA-32 | Moving between rows: the grid can move a character one row (push back / pull forward, same column) or swap two characters, refused if the cell is taken. No player action or skill uses it yet. | Code only (no ruling); no C++ caller (Blueprint use unverified) | `Combat/Grid/CombatGridSubsystem.cpp:636-797`; `Combat/Grid/CombatGridSubsystem.h:227-256` |

## AI-controlled characters

| # | Rule | Status | Source |
|---|---|---|---|
| PA-33 | Each encounter has one AI difficulty (**Easy / Medium / Hard / Expert**, default Medium), set per encounter. It applies to **every** AI-controlled character in that fight, enemies and AI allies alike. | In game (see Cp-4) | `AI/EAIDifficulty.h`; `Combat/Encounter/EncounterComponent.h:72-74`; `AI/AIDecisionManager.cpp:193-199` |
| PA-34 | Thinking time before acting: Easy 2.0-3.5 s, Medium 1.0-2.0 s, Hard 0.5-1.0 s, Expert 0.2-0.3 s (`*_THINK_MIN/MAX`). | In game | `AI/AIDecisionConstants.h:34-44`; `AI/AIDecisionManager.cpp:323-357` |
| PA-35 | Choice of action: Easy picks a random action and a random living enemy; Medium and up choose by score (kill chance, missing HP, threat, status buildup), heal below 25% HP (Hard/Expert below 40%), and ward themselves with Last Stand at 45% HP or less. | In game | `AI/AIDecisionManager.cpp:226-260`; `AI/AIDecisionConstants.h:10-30`, `:85-98`; `docs/Architecture/AISystem.md` (decision order) |
| PA-36 | Defending: for each incoming hit the AI rolls to try a defence: Easy 40%, Medium 65%, Hard 85%, Expert 95% (`*_DEFENSE_ATTEMPT`). A hit that would kill is always met with a Dodge. Otherwise Easy blocks; Medium blocks or dodges (50/50); Hard/Expert dodge. Timing accuracy also scales (`*_DELTA_BAND_MULT` 3.0 / 1.6 / 0.9 / 0.35, +/-25% jitter). | In game | `AI/AIDecisionConstants.h:46-78`; `AI/AIDecisionManager.cpp:359-411`, `:488-530` |

## Co-op players

| # | Rule | Status | Source |
|---|---|---|---|
| PA-37 | **Human players** (co-op) join a fight through the sphere: when a fight is triggered, an arena sphere opens for `JoinWindowSeconds = 2.0` s (radius 800, shrinking by 100 per joiner down to 400); player characters who enter it are gathered. | Decided; the sphere runs today but its joiners are not used (see Cp-5) | `D-003` (B, for players); `Combat/Encounter/EncounterComponent.h:52-64`; `Combat/Encounter/EncounterComponent.cpp:139-208` |
| PA-38 | Only player-side characters can trigger or join an encounter; an enemy walking into the sphere is ignored. A player-side character not in your party only watches. | In game | `Combat/Encounter/EncounterComponent.cpp:77-88`, `:118-123`, `:187-195` |
| PA-39 | Co-op revive on defeat is not decided. | Open (later, with co-op) | `D-002` notes |

## Open questions
- Answered 2026-10-26: D-042 (start in the chosen cell, B-149), D-043 (revive in a fight or at a rest spot, B-150; revive spells are D-071, OPEN), D-044 (AI allies at a fixed good level, B-071), D-045 (you take over the AI members by default, a setting lets the AI finish, B-071).
- D-002 notes: co-op revive on defeat is not decided yet ("later, with co-op").
- D-026 notes: how a hub-built team's own gear works at run start isn't covered (D-021 covers only the leader). Raise a new D-entry if B-076 needs it.
- D-020 notes: when two members' presets share one item, the suggested clash rule (flag it on the party screen, you pick) is not ruled.

## Conflicts
- **Cp-1: PartySystem.md says "no mid-run recruitment".** `docs/Architecture/PartySystem.md:14-15` says the party is "locked at trial entry... No mid-run recruitment" and that the non-leader slots are "AI companions" (`:16-17`). D-003/D-026 say you can find characters during a trial (run-only) and D-004 says members are Player-controlled by default. Doc fix, no ruling: the rulings are newer and explicit (**B-117**). The code doesn't stop recruiting in a trial; recruitables are only placed in the hub.
- **Cp-2: Formation is stored but never used.** `Party/Party.h:35-42` says an unplaced member "falls back to AutoAssignTeam's centre-out table at spawn", implying a placed member keeps its cell. But `Combat/CombatOrchestrator.cpp:214-215` always auto-assigns both sides to the Middle row, and nothing reads `PreferredRow/PreferredColumn` (`Combat/Grid/CombatGridSubsystem.cpp:292-295` TODO). Question, options and recommendation: **D-042**. **Ruled (D-042): A**, members start in their chosen cell, auto-assign only when unset or clashing (B-149).
- **Cp-3: "Out until revived", but no way to revive after a fight.** `docs/Architecture/PartySystem.md:18-19` and `Party/Party.h:27` say a dead member is "out until revived". Sapphire only revives inside a fight, and dead members aren't spawned into later fights (PA-27), so in practice a death lasts the whole run. Question, options and recommendation: **D-043**. **Ruled (D-043): B**, revive in a fight by spells or items, or at a rest spot with a full heal; otherwise a fallen member sits out (B-150; revive spells D-071).
- **Cp-4: AI allies play at the enemy's difficulty.** One per-encounter difficulty drives every AI character (`AI/AIDecisionManager.cpp:193-199`), so on an Easy encounter your AI allies also act at random and block only 40% of hits. Question, options and recommendation: **D-044**. **Ruled (D-044): A now**, AI allies play at a fixed good level; tactics per ally later (B-071).
- **Cp-5: Sphere joiners don't reach the fight.** The `EncounterComponent.h:3-8` header says players entering the sphere "join LocalParty", and D-003 says co-op players join via the sphere. But `OnJoinWindowExpired` builds the roster from the party list only (`Combat/Encounter/EncounterComponent.cpp:212-228`) and ignores joiners, and the join gate rejects anyone not in your own party (`:187-195`), which would also reject another human player's characters. Not a ruling conflict (co-op isn't built and D-003 says keep the sphere), but the header comment is wrong today. No question for Crown yet: fix the comment with B-071 (**B-117**), and settle how co-op players' parties pass the gate when co-op is built.
- **Cp-6: A dead leader.** D-004 says the leader is always Player-controlled. Today the player possesses whichever living member spawns first (`Trial/BattleGameMode.cpp:129-170`, after `GetLivingMemberCharacterData` drops the dead), so if the leader died in an earlier fight, the next member becomes the player's character. If the leader dies mid-fight, the rest keep playing on AI. Question, options and recommendation: **D-045**. **Ruled (D-045): B by default**, you take over the remaining AI members; a Gameplay setting lets the AI finish instead (B-071).

## Gaps
- No rule for how many characters you can find in one trial, or what happens when you find one with a full party (PA-01 refuses the invite today). **Ruled (Crown, 2026-10-26, rules review): B, swap.** With a full party you can send a member away **for the rest of the run** and the found character joins; a hub member sent away returns to the roster after the run. Offered when you find them and at rest spots. How many characters a trial holds: content, per trial.
- No rule for leaving a found character behind mid-run (dismissing them) or swapping them for a hub member. **Ruled (Crown, 2026-10-26, rules review): A, dismiss at rest spots.** The character leaves for the rest of the run; their gear returns to the run inventory. **Co-op:** other players can be **invited from rest spots mid-run** (drop-in), built with co-op (scheduled after B-090/B-105, D-065).
- No player action to change row in combat, and no skill that pushes or pulls (PA-32); row bonuses have no source today.
- Party size vs grid: the grid holds 9 cells, but sides are capped at 3; no rule says whether bigger enemy groups (more than 3) are ever allowed (`AutoAssignTeam` silently drops extras, `Combat/Grid/CombatGridSubsystem.cpp:286-306`).
- No rule for a Draw (both sides wiped at once): today it routes like any other fight end.
- Party HP is not saved to disk (B-090); quitting mid-trial loses the carried state.
