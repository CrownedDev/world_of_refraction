# Combat Orchestrator

## Overview

`ACombatOrchestrator` is the top-level coordinator for a combat encounter. It is an
`AActor` placed in the level (the arena's `GetActorLocation()` doubles as the arena
center). It does not implement turn order, damage math, or status logic itself —
instead it wires together the dedicated combat subsystems and drives the encounter
through its lifecycle: setup, per-turn coordination, action submission, win-condition
checks, and post-combat cleanup.

Team convention: Team 0 = players, Team 1 = enemies.

## Architecture

### `ACombatOrchestrator` (AActor)

Responsibilities (per the header doc comment):
- Initialize/end combat via `TurnManager`.
- Listen to turn events and coordinate responses.
- Process status effects at turn boundaries (via `SkillEffectManager`).
- Accept and execute actions from UI/AI (via `ActionExecutor`).
- Check win conditions.

Key internal state fields:
- `ECombatState CombatState` — `Idle`, `Initializing`, `InProgress`, `Victory`,
  `Defeat`, `Draw`.
- `TArray<AActor*> Team0Combatants`, `Team1Combatants` — stored team rosters.
- `AActor* CurrentActor` — actor whose turn is active (mirrors `TurnManager`).
- `int32 CurrentTurnNumber` — mirrors `TurnManager`'s `GlobalTurnCount`.
- `bool bWaitingForAsyncAction` — guards against double-submission while an async
  action (projectile spell, attack-with-movement) is in flight.
- Cached subsystem pointers: `TurnManagerRef`, `SkillEffectManagerRef`,
  `ActionExecutorRef`, `AIDecisionManagerRef`. Cached in `BeginPlay`.
- `ACombatCameraManager* CameraManager` — found lazily via `FindCameraManager()`.
- `FTimerHandle AutoAdvanceTimerHandle` — debug auto-advance timer.

### Supporting types (declared in `CombatOrchestrator.h`)

- `enum class ECombatState : uint8` — the combat lifecycle states.
- `struct FCombatResult` — outcome payload: `FinalState`, `TotalTurns`,
  `Team0Survivors`, `Team1Survivors`, `LastActorStanding`. Built by
  `BuildCombatResult()`.

### Configuration fields (EditAnywhere)

- `bAutoAdvanceTurns` / `AutoAdvanceDelay` — debug fallback that auto-advances turns
  for player actors when no UI is wired.
- `bAutoStartCombat` — auto-starts combat on `BeginPlay` using level actors.
- `CombatDifficulty` (`EAIDifficulty`) — AI difficulty for the encounter.
- `DebugOverrideActor`, `DebugDamageAmount`, `DebugStatusBuildupAmount`,
  `DebugSelectedTargetIndex` — debug knobs.

## How It Works

### Startup (`StartCombat`)

1. Stores `CombatDifficulty`; registers `this` with `AIDecisionManager` and
   `UCombatCommandMenuSubsystem` so they can call back into the orchestrator.
2. If combat is already active, calls `ForceEndCombat()` first.
3. Validates teams are non-empty; stores `Team0Combatants` / `Team1Combatants`;
   resets `CurrentTurnNumber`, `CurrentActor`, `bWaitingForAsyncAction`.
4. `SetCombatState(Initializing)`.
5. `PrepareAllLoadoutsForBattle()` — for each combatant, calls
   `ULoadoutComponent::PrepareForBattle()`, then applies evolution-crystal effects
   (`SkillEffectManager::ApplyEvolutionEffects`) and equipment effects
   (`ApplyEquipmentEffects`).
6. `UCombatGridSubsystem` — `AutoAssignTeam` for both teams, `PlaceAllActors`,
   `UpdateAllActorFacing`, all relative to the arena center.
7. `UWeatherStateManager::InitialiseLeaders()`.
8. `UActionExecutor::SetArenaCenter()`.
9. `BindTurnManagerEvents()` then `TurnManager->InitializeCombat(Team0, Team1)` —
   which fires the first `OnTurnStarted`.
10. Calls the BlueprintImplementableEvent `OnCombatStartedUI()` (HUD creation),
    `SetCombatState(InProgress)`, and `CombatCameraManager::InitializeForCombat()`.

### Per-turn flow

`TurnManager` broadcasts `OnTurnStarted` → `HandleTurnStarted(Actor, TurnNumber)`:
1. Updates `CurrentActor` / `CurrentTurnNumber`.
2. `ProcessStartOfTurnEffects(Actor)` — delegates to
   `SkillEffectManager::ProcessStartOfTurnEffects` and
   `UStatusBuildupManager::ProcessStatusBarDecay`.
3. `ProcessBrokenDarknessOverflow(Actor)` — if the actor's
   `UBrokenDarknessManager` is overloaded, deals aura/self/drain damage to
   combatants within `CalculateAuraRange()`.
4. If the actor died from those effects, calls `OnActionCompleted()` and returns
   (turn skipped).
5. Broadcasts `OnActorTurnStarted`.
6. `RequestActionFromActor(Actor)`:
   - AI-controlled actor (`CharacterData->ShouldUseAI()`): routes to
     `AIDecisionManager->RequestDecision(Actor)`.
   - Player actor: broadcasts `OnActionRequested`. If `bAutoAdvanceTurns` is set,
     starts the auto-advance timer as a debug fallback.

### Action submission

`SubmitAction(FAction)` (synchronous-capable path):
1. Guards: combat in progress, `CurrentActor` set, `ActionExecutor` available, not
   already waiting on an async action.
2. Validates via `ActionExecutor::ValidateAction`.
3. Decides if the action needs async execution:
   - Spell with `Projectile` / `Homing` / `Beam` delivery type.
   - Attack with `AttackData->ApproachData != nullptr`.
   - Ability where `AbilityData->RequiresApproach()` is true.
4. Async path: sets `bWaitingForAsyncAction`, calls
   `ActionExecutor::ExecuteActionAsync` with `HandleAsyncActionCompleted` as the
   completion callback.
5. Sync path: `ActionExecutor::ExecuteAction`, broadcasts `OnActionExecuted`, then
   calls `OnActionCompleted()`.

`SubmitActionAsync(FAction)` — always uses the async `ExecuteActionAsync` path.

`HandleAsyncActionCompleted(FActionResult)` — clears `bWaitingForAsyncAction`,
broadcasts `OnActionExecuted`, calls `OnActionCompleted()`.

### Turn completion (`OnActionCompleted`)

1. Bails if still waiting on an async action or combat not in progress.
2. `ProcessEndOfTurnEffects(CurrentActor)` — delegates to
   `SkillEffectManager::ProcessEndOfTurnEffects` (DOTs, duration ticking,
   expiry).
3. `CheckWinCondition()` — counts living members per team. If a terminal state is
   reached: unbinds turn events, clears all skill effects,
   `ApplyBetweenCombatCrystalDestruction()` then `ApplyBetweenCombatRepair()`,
   `TurnManager->EndCombat()`, sets the win state, builds and broadcasts
   `FCombatResult`, clears teams, returns to `Idle`.
4. Otherwise `TurnManager->AdvanceToNextTurn()`.

### Forced end (`ForceEndCombat`)

Ends combat outside the win-condition path (flee, cutscene interrupt). Ends the
camera, ends weather, clears the auto-advance timer, unbinds turn events, clears
all skill effects, resets every combatant's status bar via `UStatusBuildupManager`,
`ConsumeAllUsedItems()`, clears grid positions, `TurnManager->EndCombat()`, builds
and broadcasts `FCombatResult`, unregisters from `AIDecisionManager` and
`UCombatCommandMenuSubsystem`, and resets to `Idle`.

Note: `ForceEndCombat` deliberately does NOT call `ApplyBetweenCombatRepair()` —
aborted combats do not count as completed battles.

### Crystal durability at combat end

- `ApplyBetweenCombatCrystalDestruction()` (Phase B) — iterates each combatant's
  equipped crystal slots; for any broken crystal (`FRuntimeAttachedItem::IsBroken()`,
  queried via `LoadoutComp->GetCrystalEntryByHolder(Slot.Holder)`) it calls
  `LoadoutComp->ResetCrystalEntryByHolder(Slot.Holder)` to clear the runtime entry.
  Also calls `LoadoutComp->ClearBrokenPrimaryEvolution()` for the case-B standalone
  primary-slot evolution (e.g. Broken Darkness) — `GetEquippedCrystals` doesn't
  surface evolution self-holders, so the loop would miss broken primary evolutions.
  Counted in `CrystalsDestroyed`. The asset is never mutated at runtime. Silent
  (no UI delegate). Runs BEFORE repair so destroyed crystals are not repair candidates.
- `ApplyBetweenCombatRepair()` — adds `DurabilityConstants::REPAIR_PER_BATTLE` to
  every refined, non-immune, non-broken equipped crystal.

### Crystal-wear debug (`CallInEditor`)

Four parameterless buttons under category `Debug|CrystalWear` (Details panel
during PIE). All forward to `UCrystalManager` `WOR_*` methods — the subsystem
isn't in the `FExec` chain so the orchestrator hosts the entry points:

- `DebugCrystalState` → `WOR_CrystalState` — prints the active combatant's
  primary-weapon crystal: type (Refined / Evolution), tier, `bCanBreak`,
  current/max durability.
- `DebugWearTable` → `WOR_WearTable` — substat-modified wear prediction table
  (rows F→S) for the active combatant, worst-case envelope (S-tier, L2, Spell).
  No wear applied.
- `DebugSimCast_S_L2` → `WOR_SimCast(S, 2)` — runs the real `ProcessPostCastWear`
  path once on the active combatant's primary-weapon crystal at worst-case
  parameters. Logs PREDICT and AFTER.
- `DebugSimCast_Matched_L1` — resolves the equipped crystal's own tier from
  `Attachment->Refined.Id.Tier`, then `WOR_SimCast(<crystalTier>, 1)`.
  Isolates infusion-only wear.

Full formula, constants, and `WOR_*` semantics: `CrystalWear.md`.

## Integration Points

### Delegates broadcast

- `FOnCombatStateChanged OnCombatStateChanged` — fired by `SetCombatState` on every
  state transition.
- `FOnCombatResultReady OnCombatResultReady` — fired with an `FCombatResult` when
  combat ends (win path, forced end, or `HandleCombatEnded`).
- `FOnActorTurnStarted OnActorTurnStarted` — fired in `HandleTurnStarted`.
- `FOnActionRequested OnActionRequested` — fired for player actors needing input.
- `FOnActionExecuted OnActionExecuted` — fired after each action (sync and async).
- `OnCombatStartedUI` — a `BlueprintImplementableEvent` (not a delegate), fired
  after `TurnManager::InitializeCombat` so PreviewTurnOrder data is ready for the UI.

### Subsystems / systems it depends on

- `UTurnManager` (GameInstanceSubsystem) — turn order; the orchestrator binds to
  its `OnTurnStarted` / `OnTurnEnded` / `OnCombatEnded`.
- `USkillEffectManager` — start/end-of-turn effect processing, equipment/evolution
  effects, `ClearAllEffects`.
- `UActionExecutor` — action validation and (sync/async) execution; arena center.
- `UAIDecisionManager` — decision-making for AI actors.
- `UStatusBuildupManager` — status bar decay/reset.
- `UCombatGridSubsystem` — grid position assignment and actor placement.
- `UWeatherStateManager` — weather leader init / end.
- `UCombatCommandMenuSubsystem` — player action menu (registered with `this`).
- `ACombatCameraManager` — combat camera; found via `GetAllActorsOfClass`.
- Per-actor components: `UCharacterDataComponent`, `ULoadoutComponent`,
  `UInventoryComponent`, `UBrokenDarknessManager`.

### What depends on it

- `UAIDecisionManager` and `UCombatCommandMenuSubsystem` hold back-references set
  via `SetCombatOrchestrator(this)` and cleared on `EndPlay` / combat end.
- UI/HUD Blueprints listen to the orchestrator's delegates and override
  `OnCombatStartedUI`.

## Known Limitations / TODOs

No `// TODO`, `// FIXME`, or `// HACK` markers were found in either file.

Observations from the code (not explicitly flagged as issues):
- `ForceEndCombat` contains a duplicated `CameraManager->EndCombat()` call and a
  duplicated `AIDecisionManager->ClearCombatOrchestrator()` block (called both
  mid-function and again at the end). Functionally harmless but redundant.
- The header doc comment lists `BattleUIManager` and `AIDecisionManager` under
  "Future integrations"; `AIDecisionManager` is now actually integrated, so that
  comment is partially stale.
- `StartCombat` logs an error if `TurnManager` is missing but continues far enough
  to position the grid before returning — combat is then positioned but turns
  never advance.
- `HandleTurnEnded` is bound but intentionally near-empty; end-of-turn logic lives
  in `OnActionCompleted`.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-05-17 | Initial documentation | docs/architecture-documentation |
| 2026-05-27 | Between-combat destruction sweep extended via `ULoadoutComponent::ClearBrokenPrimaryEvolution` to cover case-B primary-slot evolutions (BD). New `Debug\|CrystalWear` subsection — four `CallInEditor` buttons (`DebugCrystalState`, `DebugWearTable`, `DebugSimCast_S_L2`, `DebugSimCast_Matched_L1`) forwarding to `UCrystalManager` `WOR_*`. | feature/crystal-wear-substat-modifier |
