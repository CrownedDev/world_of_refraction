# Game input across level changes

## Overview
Game input is given back at the start of every level change (B-578).

A menu that takes the keys sets `FInputModeUIOnly`. That tells the game viewport to ignore game input (`UGameViewportClient::SetIgnoreInput(true)`) and to stop capturing the mouse. The viewport outlives a level change; the menu and its controller do not, and nothing in the engine clears the flag on a level change (it is only ever set by an input mode). So a menu still up when the level changes left the next level with no move, no Tab and no click, until some other menu was opened and closed.

The one screen that did this is a fight's results screen (`ABattleGameMode::ShowResults`): its Continue button *is* the level change, on every way out of a fight (win, loss, flee, and the loss that ends the run and goes to the hub). Every other menu-only screen sets `FInputModeGameOnly` when it closes, before any travel it starts.

## Architecture
| File | What it holds |
|---|---|
| `Player/WoRGameInputSubsystem` | `UWoRGameInputSubsystem` (game-instance subsystem). Listens to `FCoreUObjectDelegates::PreLoadMapWithContext` (OpenLevel) and `FWorldDelegates::OnSeamlessTravelStart` (seamless server travel). `RestoreGameInput(WorldContext)`: for its own game instance's context, sets `FInputModeGameOnly` on every local player's controller (focus and the mouse back to the game, as closing a menu does) and `ApplyGameOnly` on the viewport. `ApplyGameOnly(Viewport)`: the viewport half of that mode (ignore-input off, capture permanently, lock on capture), which needs no player or viewport widget. |
| `Debug/Player/GameInputDebug` | `WoRGameInputDebug::GetStateString`, console `wor.Input.PrintState`: whether the viewport ignores game input, mouse capture and lock, the cursor, keyboard focus. Not in a shipping build. |

**Why at the start of the level change, not on arrival:** a screen the next level opens on arrival (the title, a join seat, a duplicate prompt) sets its own mode from the new controller's `BeginPlay` or later. That is after the level change started, so it is never undone. A reset on arrival could run after such a screen opened.

**Why a game-instance subsystem:** the rule covers every level change, whoever starts it (`UTrialRunSubsystem::TravelTo`, the title, hosting or joining a session, a debug command), and holds no state.

## Integration Points
- `FCoreUObjectDelegates::PreLoadMapWithContext`, `FWorldDelegates::OnSeamlessTravelStart`.
- `APlayerController::SetInputMode`, `UGameViewportClient::SetIgnoreInput / SetMouseCaptureMode / SetMouseLockMode`.
- `ABattleGameMode::ShowResults` is the screen that relies on it.

## Known Limitations / TODOs
- A level change always ends in game input. A screen that should stay up across one has to open again on arrival.
- The spec proves the viewport half (headless: no viewport widget, no local player). Focus and the real level change are in `docs/PIE/B-578.md`.
- Why this was not seen by hand before agent play is not known. The cause above is read from engine source and matches what the agent saw; the hand case was not reproduced.

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-22 | B-578: new. `UWoRGameInputSubsystem`, `wor.Input.PrintState`, spec `WoR.Input.LevelChange` (2). Fixes no input in the trial after a fight's results screen. | `fix/B-578-input-after-fight` |
