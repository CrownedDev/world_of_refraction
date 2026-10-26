# Combat Camera

## Overview

The combat camera decides what the player sees during a fight (design: `docs/Design/CombatCamera.md`, D-074). Each combatant owns one camera, and every camera follows the same mode rules. A director on the combat orchestrator picks which camera the viewer sees and in which mode:
- **Turn:** the viewer's own character is choosing an action.
- **Not your turn:** anyone else's turn, panned out wide.
- **Action:** a skill's Level Sequence is playing.
- **Static:** camera movement is turned off.

Built in B-156 (code only). The template sequences and the rig tuning are B-157.

## Architecture

| Class | Kind | Role |
|---|---|---|
| `FCombatCameraRules` (`Combat/Camera/CombatCameraTypes.h`) | plain struct, static functions | The rules, with no world:<br>• `SelectMode(isViewers, sequencePlaying, movementOff)`<br>• `TemplateKindFor(skill, actionType)`: spells use Spell, other skills Melee or Ranged by `ExecutionType`<br>• `ComputeView(mode, style, frame)`: camera transform; offsets are named constants marked `TODO: tune` |
| `ECombatCameraMode`, `ETurnViewStyle`, `ECameraTemplateKind` | `UENUM(BlueprintType)` | • Modes: None / Turn / Action / NotYourTurn / Static<br>• Turn-view styles: CentredWide (default) / OverTheShoulder / Tactical<br>• Template kinds: Melee / Ranged / Spell |
| `UCombatCameraComponent` (`Combat/Camera/CombatCameraComponent.h`) | `UActorComponent`, added to each combatant for one fight | Owns one `ACameraActor`. `ShowView(mode, transform)` spawns it on first use and moves it after; `ReleaseCamera()` / `EndPlay` destroy it |
| `UCombatCameraDirector` (`Combat/Camera/CombatCameraDirector.h`) | `UActorComponent`, default subobject `CameraDirector` on `ACombatOrchestrator` | Adds the per-character cameras, listens to combat events, sets the viewer's view target, and plays, binds and stops sequences. Settings: `TurnViewStyle`, `bCameraMovementOff`. `TemplateSequences` holds the template sequence for each skill type |
| `USkillDataBase::CameraSequence` | `TSoftObjectPtr<ULevelSequence>` (D-057) | The skill's own cinematic. Empty means its type's template |
| `UCombatCameraDebug` (`Debug/Combat/CombatCameraDebug.h`) | function library | `GetCameraStateString`, `PrintCameraState`, `FindDirector`; console commands `wor.Camera`, `wor.Camera.Style 0-2` and `wor.Camera.MovementOff 0/1` |

**Frame:** the camera is always framed from the viewer's side: `TowardEnemies` runs from the local party's centre to the opposing party's. So enemy turns never flip the view across the arena.

**Sequence binding:** the tags `Caster` and `Targets` are bound with `ALevelSequenceActor::SetBindingByTag`. A sequence's camera-cut track owns the view while it plays.

## Integration Points

- `ACombatOrchestrator::StartCombat` calls `CameraDirector->BeginCombat(LocalParty, OpposingParty, ActionExecutorRef)` **before** `TurnManager->InitializeCombat`, because turn 1 is dispatched inside that call.
- `ACombatOrchestrator::ReleaseCombatResources` calls `CameraDirector->EndCombat()` on a win, a loss and a forced end. The director's own `EndPlay` also calls it.
- Delegates the director binds (and removes at the end of the fight):
  - `ACombatOrchestrator::OnActorTurnStarted`: show that actor's camera in Turn, NotYourTurn or Static.
  - `UActionExecutor::OnActionStarted`: frame the executor (Execution turns, where a ritual fires, have no turn-start broadcast), then resolve and play the sequence. With no sequence, the turn view stays.
  - The sequence's `OnFinished`: go back to the turn view. The sequence actor is destroyed next tick, not inside its own broadcast.
  - The sequence is also stopped by the next turn start. Not by `OnActionCompleted`: that broadcast arrives after the orchestrator may already have started the next turn and its sequence.
- Viewer: `GetWorld()->GetFirstPlayerController()`, the same one `UCombatHUD` uses. The view target from before the fight is restored at the end.

## Known Limitations / TODOs

- Solo only: the whole local side counts as the viewer's characters. Co-op and PvP come with D-065.
- No template sequences exist yet (B-157), so every action stays on the turn view.
- Offsets and the 0.6 s blend are first guesses (`TODO: tune`, B-157).
- Settings have no menu yet: set them in the Details panel or with the console commands (D-024).
- Sequences load synchronously at action start (`TODO: B-140`).
- A sequence runs until it ends or the next turn starts. Re-check this once real templates exist.
- Choices made for the basic version, for Crown to confirm in PIE (D-078):
  - camera movement off also skips sequences
  - enemy actions play sequences too
  - a death at turn start leaves the view where it was
- No Defence mode: it is to be play-tested first (design §2).
- Arming a delayed skill also broadcasts `OnActionStarted`. Once templates exist, a ritual's cinematic may play at arm time and again when it fires. Check this in B-157.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-26 | Created. B-156: rules, per-character component, director, skill sequence field, `wor.Camera` debug, orchestrator wiring, specs `WoR.Combat.Camera` and `WoR.Combat.Camera.Director` | feature/B-156-combat-camera |
