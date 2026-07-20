# Combat Character

## Overview

`ACombatCharacter` is the C++ base for every combat-capable character. It exists
for one reason: to **natively create the combat component stack** so those
components exist earlier in the spawn lifecycle than Blueprint-authored ones can.

It holds no logic of its own — no `BeginPlay`, no tick, no state. It is a
constructor and ten component pointers. Behaviour lives in the components.

Chain: `ACharacter` → `ACombatCharacter` → `BP_TestCharacterBase` → the character
and enemy Blueprints (`BP_CombatCharacter_*`, `BP_EnemyBase` →
`BP_Enemy_Generic`).

## Why native components

Blueprint-panel components live in the Blueprint's **SimpleConstructionScript
(SCS)** and are not instantiated until `FinishSpawningActor` runs the construction
script. `ABattleGameMode::SpawnCombatant` deliberately spawns deferred:

```
SpawnActorDeferred  →  assign CharacterData  →  FinishSpawningActor
```

The asset must be assigned in that middle window so
`UCharacterDataComponent::BeginPlay` runs its init cascade against the *right*
asset rather than the Blueprint default. An SCS component does not exist there —
`FindComponentByClass` returns null — which is exactly the bug that aborted every
battle before T-C1a (see the `BattleGameMode` abort path).

Native components also give lifecycle guarantees, typed C++ access without a
lookup, and a Components panel that isn't cluttered with gameplay plumbing.

## Architecture

### Native component stack (constructor order)

| # | Member | Class |
|---|--------|-------|
| 1 | `CharacterDataComponent` | `UCharacterDataComponent` |
| 2 | `WeaponMeshComponent` | `UWeaponMeshComponent` |
| 3 | `CurrencyComponent` | `UCurrencyComponent` |
| 4 | `InventoryComponent` | `UInventoryComponent` |
| 5 | `CrystalInventoryComponent` | `UCrystalInventoryComponent` |
| 6 | `EvolutionInventoryComponent` | `UEvolutionInventoryComponent` |
| 7 | `InfusionVFXComponent` | `UInfusionVFXComponent` |
| 8 | `LoadoutComponent` | `ULoadoutComponent` |
| 9 | `BrokenDarknessComponent` | `UBrokenDarknessManager` |
| 10 | `BattleConfigComponent` | `UBattleConfigComponent` |

All are `UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")`
`TObjectPtr<>` members.

`UBrokenDarknessManager` is present on **every** combat character, inert unless the
owner is BD — all its behaviour gates on `bIsFlipped`, seeded from
`CharacterData->bBrokenDarknessInnate`. A uniform contract was chosen over a
conditionally-present component.

`UBattleConfigComponent` holds runtime battle context (grid position, owning party,
team index, display context) — set by `ABattleGameMode` at spawn time. Unlike the
rest of the stack it has **no cascade dependency**: it reads nothing from its
siblings at init, so it appends last and its ordering is free (Arc 1 of the
Encounter Composition System). See `docs/Architecture/PartySystem.md`.

### Still on the BP SCS (deliberately)

- `hubCamera` (`UCameraComponent`), `hubSpringArm` (`USpringArmComponent`) — engine
  components, hub-navigation concern
- `elementColorDebug` (`UElementColorDebugComponent`) — debug-only affordance

`characterMovement`, `capsuleComponent`, and `mesh` come natively from `ACharacter`.

## ⚠️ Constructor order matters

Component `BeginPlay` runs in **registration order**: native components before SCS
components, and among natives in **constructor-declaration order**.

**`CharacterDataComponent` must stay first.** Its `BeginPlay` runs the init
cascade — inventory init, loadout init, world-stat seed, pool recompute, HP/EP
seed, born-BD flag — that seeds the state every other component reads. Each new
promotion appends *after* the previous one; nothing is inserted above it.

Two concrete hazards this ordering protects, both found during the promotion arc:

- **`ULoadoutComponent::BeginPlay`** calls `EnsureDefaultLoadout()`, which injects a
  `"Default"` loadout when `SavedLoadouts` is empty. If Loadout ran before the
  cascade populated loadouts, it would inject a spurious one built from a stale
  `CharacterClass`.
- **`UBrokenDarknessManager`** read a flag the cascade seeds — see below.

**Lookup itself is order-safe.** Every component (native *and* SCS) is registered
before *any* `BeginPlay` runs, so `FindComponentByClass` succeeds regardless of
order. Only *reads of initialized state* are order-sensitive. That is why
`UInfusionVFXComponent`, whose `BeginPlay` caches CharacterData / Loadout /
WeaponMesh pointers, could be promoted ahead of Loadout safely.

### The `InitializeBornBrokenDarkness()` hook

UE does **not** contractually guarantee component `BeginPlay` order, and promotion
silently changes it. Rather than leave the born-BD flip depending on that,
`UBrokenDarknessManager` exposes an explicit entry point:

```cpp
void UBrokenDarknessManager::InitializeBornBrokenDarkness()
```

Called from `UCharacterDataComponent::BeginPlay`, inside the `if (CharacterData)`
block, immediately after the born-BD flag is seeded — the first moment
`IsBrokenDarkness()` is meaningful. Idempotent (guards on `!bIsFlipped`), so it
composes safely with `TriggerTransformation()`, which handles runtime breaks and
guards the same flag.

The `OnEPChanged` binding **stays** in `UBrokenDarknessManager::BeginPlay`: it needs
only the component to exist, not any seeded state, so it is order-independent —
and keeping it there means an owner with null `CharacterData` (whose cascade never
runs) still gets its overload wiring.

This is the pattern to copy for any future component whose init depends on cascade
state: **an explicit call from the cascade, not an ordering assumption.** The
failure mode it replaces was silent — no error, no log, and only on born-BD
characters.

## AI Possession

`ACombatCharacter` sets, in its constructor:

```cpp
AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
AIControllerClass = ACombatAIController::StaticClass();
```

So **every** combat character auto-spawns an `ACombatAIController` when it finishes
spawning — placed enemies and battle-stage spawns alike. This makes engine
possession the authority on "who is driving this pawn": `IsPlayerControlled()` /
`IsBotControlled()` now answer truthfully, replacing the old asset-level
`bIsAIControlled` flag (see `ECharacterOrigin`, which now carries *identity* while
possession carries *control*).

`ACombatAIController` is a bare, behaviourless `AAIController` subclass — combat
decisions still live in `UAIDecisionManager`, which drives pawns directly and never
consults the controller. Its one job beyond hosting possession:

```cpp
void ACombatAIController::OnUnPossess()   // → Super, then Destroy()
```

**It self-destroys when displaced.** This is the single mechanism that reaps both
orphan sources:

- **Displacement** — `ABattleGameMode` has PC0 possess `LocalParty[0]`;
  `AController::OnPossess` unpossesses the incumbent AI controller first, which then
  reaps itself.
- **Login-time orphan** — `AGameModeBase` spawns PC0 a default pawn from
  `DefaultPawnClass` at login; that pawn auto-possesses an AI controller in
  `PostInitializeComponents` (before PC0 has possessed anything), and PC0 then
  displaces it. This orphan is unreachable from any GameMode hook, so a GameMode-side
  fix could not catch it — the controller reaping itself does.

All four project GameModes use an `ACombatCharacter` as `DefaultPawnClass`, so all
four leaked one controller per level load before this. Handling it on the controller
covers every GameMode, present and future, with no per-site work and no BP
reparenting.

⚠️ Forecloses one thing: an AI controller can no longer outlive its pawn to
re-possess a respawn. Not a current use case (`UAIDecisionManager` never holds a
controller), but a respawn feature would need to revisit it.

## Integration Points

- `ABattleGameMode::SpawnCombatant` — the deferred-spawn path this class exists to
  serve; assigns `CharacterData` between `SpawnActorDeferred` and `FinishSpawning`.
- `UCharacterDataComponent::BeginPlay` — the init cascade; calls into
  `UInventoryComponent`, `ULoadoutComponent`, and `UBrokenDarknessManager`.
- All consumers reach these components via `GetOwner()->FindComponentByClass<>()`,
  which is agnostic to native-vs-SCS origin. **No consumer needed changing when
  these were promoted.**

## Known Limitations / TODOs

- Per-instance component overrides on *placed* pawns were keyed to the old SCS
  nodes and do not survive promotion. Every captured default across all 9 BPs was
  already at its C++ default, so nothing was lost — but placed instances are worth
  re-checking after any future promotion.
- `UWeaponMeshComponent` is the only component in the stack that ticks
  (`bCanEverTick = true`, `TickInterval = 0.1f`), which sits awkwardly against the
  project's event-driven / avoid-Tick rule. Not addressed here.
- `hubCamera` / `hubSpringArm` remain SCS. If hub navigation ever needs the same
  lifecycle guarantees, they are the next candidates.

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-07-20 | Initial documentation. Records the Combat Component C++ Promotion arc: 8 components promoted off the BP SCS onto `ACombatCharacter` (WeaponMesh, Currency, Inventory, CrystalInventory, EvolutionInventory, InfusionVFX, Loadout, BrokenDarkness), joining `CharacterDataComponent` from T-C1a. Adds the `InitializeBornBrokenDarkness()` cascade hook replacing BeginPlay-order dependence. Every captured SCS default across all 9 BPs was already at its C++ default — no re-entry needed. `UCombatMovementComponent`, listed on the roadmap, did not exist (dissolved by the warp-positioning work, `9563ff2d` / `9d064648`); `EvolutionInventory` took its slot. | feature/combat-components-cpp |
| 2026-07-20 | Encounter Composition Arc 1 (Party Foundation, merged `e5908739`): `UBattleConfigComponent` added as the 10th native (runtime battle context, no cascade dependency). New §AI Possession — `AutoPossessAI` + `AIControllerClass = ACombatAIController`, and the `OnUnPossess` self-reap that handles displacement and login-time orphan controllers in one mechanism. `ECharacterOrigin` now carries identity while engine possession carries control, replacing `bIsAIControlled`. | feature/encounter-composition-arc1 |
