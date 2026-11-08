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
| 2026-11-08 | B-231 part 1 (D-129): actions stay on the server; new `UCombatPresentationComponent` on every `ACombatCharacter` multicasts the montages the server plays (`UActionExecutor::PlayActionMontageOnActor`, the defense montage) and each resolved hit (`OnHitShown` on every machine, for damage numbers). `wor.Net.PrintPresentation`; spec `WoR.Net.Presentation`. Projectiles / VFX: B-231b. | `feature/B-231-actions-server-multicast-cloud` (UNBUILT) |
| 2026-11-08 | B-230b: new replicated `UCombatGridSlotComponent` on every `ACombatCharacter`; `UCombatGridSubsystem` publishes each change onto it and its position reads fall back to it (a client's grid is empty); Move / Swap require a held actor; a client's menu greys the row step from it. `wor.Grid.PrintSlots`; spec `WoR.Net.GridSlot`. | `feature/B-230b-grid-slot-replicated-cloud` (UNBUILT) |
| 2026-11-08 | B-231b part 1: `ASkillProjectile` cosmetic copies (`FCosmeticProjectileLaunch`, `MakeCosmeticLaunch`, `InitializeCosmetic`: no collision, bursts only); `UCombatPresentationComponent::SendProjectile` / `Multicast_LaunchProjectile`, called after each launch. Impact visuals / telegraphs / stance montages / damage-number widget: B-231c. | `feature/B-231b-cosmetic-projectiles-cloud` (UNBUILT) |
| 2026-11-08 | B-231c part 1: `FCosmeticVFX`; `UActionExecutor::SpawnVFXArrayEntry` sends each skill VFX (telegraphs included) through `UCombatPresentationComponent::SendVFX`; clients spawn it (`SpawnLocally`, Unreliable multicast). Left on B-231c: B-369 impact visuals, stance montages, damage-number widget. | `feature/B-231c-client-telegraphs-cloud` (UNBUILT) |
| 2026-11-07 | B-228b: `ClientCombatHUD::Next` (build once on, tear down once off, never on the server); `UTurnStateComponent::OnRep_TurnState` → `UpdateClientCombatHUD`; the component owns the client's `UCombatHUD` (review fix: the orchestrator is not on clients); spec in `WoR.TurnState`. Unbuilt (cloud pre-write). | feature/B-228b-client-combat-hud-cloud |
