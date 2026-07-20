# Party / Match Setup — Planned System

**Status:** Planned. Committed by Crown. To be built after the reactive-defense per-hit work (Stage 3+)
is stable. Documented now to capture the design.

## Why
Every real mode — solo, co-op, PvP — loads the player's party dynamically. Players bring characters
THEY built; those can't be pre-placed in a level. Level-placed + Team-tagged characters (the current
setup) are a DEMO/TEST convenience only. The real game spawns parties from saved data and assigns them
to teams + grid positions at match start.

This is one cohesive system — party loading, team assignment, and grid placement are not separate
concerns; they're all "set up the match from party data."

## What it does
1. **Party data** — a saved party: which characters + their builds. (Initially a default/hardcoded
   party asset until player party-building exists.)
2. **Load** — read the party data at match start.
3. **Spawn** — create character actors from the party data (replaces level-placing).
4. **Assign parties** — `LocalParty` (the local player's side) / `OpposingParty` (whatever opposes
   them); co-op = two players' characters in the same `LocalParty` vs an enemy `OpposingParty`.
   The naming is perspective-based, so under PvP each client sees itself as `LocalParty`.
5. **Assign grid positions** — place each character on a grid cell (layer/row/column).
6. **Hand to the orchestrator** — give ACombatOrchestrator the spawned/assigned/positioned characters —
   the SAME list its debug path collects from level tags (`GetAllActorsWithTag` `LocalParty` /
   `OpposingParty`; note the T-C1 production path already spawns from a stashed roster instead). The
   orchestrator is spawn-agnostic (takes a list of actors), so this slots in WITHOUT a combat rewrite.

## Where it lives
Match-setup is session-setup → GameMode (or a match-setup subsystem the GameMode owns) territory.
Likely the combat GameMode coordinates: load party → spawn → assign teams/grid → hand to orchestrator.

## Open design questions (resolve when building)
- **Party source (initial):** where does the party come from before player party-building exists? A
  default party asset? The current level-placed characters promoted to a "default party"? (Need
  something to spawn until party-building is built.)
- **Co-op model:** two players sharing one `LocalParty` vs an enemy `OpposingParty`? Other configurations?
- **Grid assignment:** fixed slots per party position? Player-chosen? Auto-assigned?
- **Enemies:** enemy parties loaded/spawned from an encounter definition, or designer-placed for now?
- **Player↔character link:** in co-op/PvP, how does each player's defense input map to THEIR party's
  characters? (Connects to the defender-lookup work — GetActiveDefenderForLocalPlayer currently
  returns the single non-AI open-window defender; multi-player/multi-character evolves this.)

## Relationship to current work
- The orchestrator finds characters by reference (a list) — placed or spawned, it just gets the list.
  This is the "architectural invariant for future extensibility" already preserved. Party-setup feeds
  the orchestrator the same way level tags do now.
- Connects to the deferred multi-target defense (AOE hitting multiple player characters — Stage 6) and
  the defender lookup (single → multi character/player).

## Sequencing
Build AFTER the reactive-defense per-hit foundation (Stages 3-6) is stable. It's a substantial system
(party data + spawn + assign + setup) and shouldn't interrupt the in-flight combat work.
