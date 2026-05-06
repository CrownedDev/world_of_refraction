# World of Refraction — Claude Code Context

## Project

- **Engine**: Unreal Engine 5.7
- **Language**: C++ primary, Blueprints for UI wiring and designer-tunable values
- **Module**: `world_of_refraction` (single module)
- **Goal**: Production-grade turn-based combat RPG. 9-element magic system, three character classes (Generic / Caster / Resonator).

## Source layout

- `Source/world_of_refraction/Public/` — headers
- `Source/world_of_refraction/Private/` — implementations
- UBT auto-includes subdirectories within the module — `#include` statements don't need updating after file moves *within* the module.

## Working principles
-  **Create a new branch for every new feature or significant refactor** — never build directly on `main` or an unrelated working branch. Use descriptive prefixes (`feature/`, `refactor/`, `chore/`, `fix/`).
- **Production-grade, not over-engineered.** Working game > perfect architecture. Push back on over-engineering. Prefer the simplest working solution.
- **Incremental.** Build new alongside old. Verify behaviour. Wrap old paths during transition. Remove old code only after the new path is proven in PIE.
- **Self-documenting code.** Names carry the meaning. Minimal comments. Use named constants — no magic numbers. Mark unresolved values with `// TODO:`.
- **Strategic logging.** Enough `UE_LOG` to diagnose, not enough to spam Output Log.
- **Don't touch more than 3 files in one change.** Break larger work into steps.
- **Don't break existing functionality.** If a change does, revert and reassess.

## Architecture rules

| Need                                                                                    | Use                                                                        |
| --------------------------------------------------------------------------------------- | -------------------------------------------------------------------------- |
| Immutable design-time data (weapon stats, spell defs, character templates)              | `UPrimaryDataAsset`                                                        |
| Mutable runtime state on an actor (current HP/EP, status effects)                       | `UActorComponent`                                                          |
| Cross-system service with global access (DamageCalculator, TurnManager, ActionExecutor) | `UGameInstanceSubsystem`                                                   |
| Mutable runtime data with no actor lifetime concerns                                    | `USTRUCT(BlueprintType)`                                                   |
| Designer-tunable enums                                                                  | `UENUM(BlueprintType)`                                                     |
| UI screens, HUDs, menus                                                                 | `UUserWidget` (Blueprint authored, C++ base when behaviour is non-trivial) |

**Source-of-truth queries go to the owning object.** Crystal properties are queried from the crystal, not duplicated on the weapon.

## Cross-system communication

- Prefer dynamic multicast delegates (`DECLARE_DYNAMIC_MULTICAST_DELEGATE_*`) over polling or hard references.
- Subsystems: `GetGameInstance()->GetSubsystem<UYourSubsystem>()`. **Never cache subsystem pointers across PIE sessions.**
- Components: `GetOwner()->FindComponentByClass<>()` once, cache, not per-tick.
- **Avoid `Tick`.** Turn-based combat is event-driven.

## Debug tools — required per system

Every new C++ system ships with debug utilities. Pattern:

- `<SystemName>Debug.h/.cpp` pair under `Public/` and `Private/`
- Static functions logging via `UE_LOG` and/or `GEngine->AddOnScreenDebugMessage`
- A `GetXxxString()` returning a formatted `FString` for inspection
- For data assets: `UFUNCTION(CallInEditor, Category="Debug")` Print/Log buttons in Details panel
- For runtime systems: `CompareXxx()` or `PrintXxxState()` for snapshot inspection

**If a system can't be inspected without launching PIE and triggering the exact path, debug tools are missing.**

## UE5 gotchas — don't repeat these

- **TransBuffer crash**: Dynamically recreating widgets *inside* a designer-placed widget causes PIE crashes (`PlayLevel.cpp:546` assertion). Fix: split widget creation (once) from data updates (per turn), or use standalone widgets added directly to viewport.
- **Widget lifecycle**: `OnCombatStartedUI` must fire *after* `TurnManager->InitializeCombat()` so PreviewTurnOrder arrays are populated before UI builds.
- **Blueprint custom events vs functions**: Turn event bindings need red custom event nodes, not purple function nodes. Function nodes can't bind dynamic delegates.
- **UHT enum defaults**: Don't use scope resolution (`ESpellElement::None`) as default in `UFUNCTION` declarations — UHT parsing fails. Use constructor cast: `ESpellElement(0)`.
- **UHT meta tags**: Must use literals, not constants. `meta=(ClampMin=0)` works; `meta=(ClampMin=MIN_VALUE)` does not.
- **`InnateElement` default**: Defaults to `Generic` (not `Fire`) — represents "no innate magical ability". Character class — not `InnateElement` — gates magical capabilities.
- **Animations**: Standard attacks in fixed-position turn-based combat use In-Place anims by default. Root motion is opt-in per montage.
- **Git LFS .uasset files**: Stored as LFS pointers. Cannot be read as text. When Blueprint logic is referenced, ask for a screenshot or text export — don't try to read the .uasset.

## Repo-specific notes

- **LFS coverage is incomplete.** `.gitattributes` only LFS-tracks `*.uasset` and `*.umap`. Other binary types (`.fbx`, `.png`, `.wav`, `.tga`, `.exr`) are not LFS-tracked. Flag this if a change adds binaries of those types.
- **`*.cpp` and `*.h` use the `cpp` diff driver** (not LFS).

## Git discipline

- Commit after every working milestone.
- **Commit before any risky operation** — file moves, rebases, refactors crossing >3 files. A failed rebase has corrupted `.uasset` files in this project before.
- Detailed commit messages.

## Workflow

When making a change:
1. Read the relevant code first. Don't guess.
2. Identify the exact file and line(s) before writing.
3. State the plan before executing for anything beyond a single-file edit.
4. Show changing code with a few lines of context above and below — full file dumps are noise.
5. Specify exact file path and line placement.
6. Flag obsolete code explicitly — but do not remove until the new path is verified in PIE.
7. After the change: confirm it compiles, list TODOs introduced, suggest the next logical step.
8. Don't execute the next step without confirmation.

**Ask before:**
- Editing more than one file
- Running bash commands that mutate state (`git`, file deletions, mass renames)
- Refactors crossing existing system boundaries

**Don't:**
- Agree with an approach that has problems
- Make breaking changes without flagging them
- Remove code prematurely
- Write summary documentation mid-refactor

## Session orientation

At the start of a session, the user will state:
- Current branch
- Last committed milestone
- Today's target system

If they don't, ask — don't guess. Session-specific state is not stored in this file.
## Session state

Per-session state lives in `docs/sessions/YYYY-MM-DD.md`. At session start, the user will reference the latest one or paste its contents. If they don't, ask which session doc to read before assuming current state.

The `_TEMPLATE.md` in that folder is the format. Don't read it as state.
