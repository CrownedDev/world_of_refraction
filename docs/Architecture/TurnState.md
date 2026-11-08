# Turn State (co-op)

## Overview
The fight's turn state, kept on the GameState so every player in a co-op game reads the same thing (B-228, `docs/Design/CoopStateOwnership.md`). `UTurnManager` is a GameInstance subsystem, so a client's copy is empty; the server copies its state into a replicated component.

## Architecture
- `UTurnStateComponent` (`Public/Player/TurnStateComponent.h`) is a default subobject of `AWoRGameState`. It holds:
  - replicated: `bCombatActive`, `CurrentActor`, `TurnNumber` (`UTurnManager::GetTurnCount`), `bCurrentTurnIsBonus`
  - `Upcoming`: the next 8 turns (`FTurnStateSlot`)
  - `Teams`: each combatant's team (`FTurnStateTeam`)
- `Find(WorldContext)` returns the component, or null if the world has no `AWoRGameState`.
- On the server, BeginPlay binds to `OnTurnStarted`, `OnSpeedChanged` and `OnCombatEnded`. `RefreshFromTurnManager` copies the state. `HandleCombatEnded` clears it, because the TurnManager broadcasts before it resets itself.
- On clients, one OnRep broadcasts `OnTurnStateChanged`. The server broadcasts it after each refresh.

## Integration Points
- `UTurnOrderStripWidget` reads the component when there is one, and falls back to `UTurnManager` otherwise.
- `CombatCommandMenuSubsystem` (`TeamOf` / `MembersOf`) does the same for team tints and target lists.
- Debug: `GetDebugString`, `wor.Turn.PrintState`; `wor.GameState.Print` includes it.

## Known Limitations / TODOs
- B-228b: a client builds its combat HUD when the replicated state first shows combat on (`UpdateClientCombatHUD`, which owns the client's `UCombatHUD`: the orchestrator is server-only; built once both sides have replicated), team 0 as its side; status bars, effects and the grid on a client wait on B-230. The six-field OnRep is still not folded into one snapshot (a client may redraw several times per turn).
- `TURN_STATE_PREVIEW_COUNT` is 8. A strip with `PreviewCount` above 8 shows empty extra slots.
- At combat end the host's strip empties, because the state clears. The old path never redrew at the end.

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-03 | B-228: created. Unbuilt (cloud). | feature/B-228-turn-state-gamestate |
| 2026-11-07 | B-228b: `ClientCombatHUD::Next` (build once on, tear down once off, never on the server); `UTurnStateComponent::OnRep_TurnState` → `UpdateClientCombatHUD`; the component owns the client's `UCombatHUD` (review fix: the orchestrator is not on clients); spec in `WoR.TurnState`. Unbuilt (cloud pre-write). | feature/B-228b-client-combat-hud-cloud |
| 2026-11-08 | B-232b part 1: `CommandMenuRules::OpensHere`; the host's menu opens only for members it commands; a client opens its own member's menu from `OnRep_TurnState` (`UpdateClientCommandMenu`, once per turn, closed when the turn moves on). The rest of B-232b is B-232c. Unbuilt (cloud pre-write), on B-228b. | feature/B-232b-menu-on-commander-cloud |
