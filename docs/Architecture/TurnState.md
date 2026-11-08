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
- Only the server builds the combat HUD (`CombatOrchestrator` `OnCombatStartedUI`). The client HUD is B-228b.
- `TURN_STATE_PREVIEW_COUNT` is 8. A strip with `PreviewCount` above 8 shows empty extra slots.
- At combat end the host's strip empties, because the state clears. The old path never redrew at the end.

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-03 | B-228: created. Unbuilt (cloud). | feature/B-228-turn-state-gamestate |
| 2026-11-08 | B-231 part 1 (D-129): actions stay on the server; new `UCombatPresentationComponent` on every `ACombatCharacter` multicasts the montages the server plays (`UActionExecutor::PlayActionMontageOnActor`, the defense montage) and each resolved hit (`OnHitShown` on every machine, for damage numbers). `wor.Net.PrintPresentation`; spec `WoR.Net.Presentation`. Projectiles / VFX: B-231b. | `feature/B-231-actions-server-multicast-cloud` (UNBUILT) |
