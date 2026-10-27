# World of Refraction — Claude Code Context

## Project

- **Engine**: Unreal Engine 5.8
- **Language**: C++ primary, Blueprints for UI wiring and designer-tunable values
- **Module**: `world_of_refraction` (single module)
- **Goal**: Production-grade turn-based combat RPG. 9-element magic system, three character classes (Generic / Caster / Resonator).

## Source layout

- `Source/world_of_refraction/Public/` — headers
- `Source/world_of_refraction/Private/` — implementations
- UBT auto-includes subdirectories within the module — `#include` statements don't need updating after file moves *within* the module.

## Working principles
-  **ALWAY Create a new branch for every new session — never build directly on `main` or an unrelated working branch. Use descriptive prefixes (`feature/`, `refactor/`, `chore/`, `fix/`).**
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

**Standing technical rules (Crown, 2026-10-26, D-057 to D-068):**
- **Per-fight state** (anything that lives for one battle) goes on the battle's actors, GameState or PlayerState, not in a new field on a `UGameInstanceSubsystem` (D-064). Existing subsystem state moves with the co-op plumbing, which starts now (D-103 changes D-065: no longer waits for B-105).
- **Heavy visuals and audio** (meshes, textures, sounds, Niagara) in a *new* data-asset field use `TSoftObjectPtr`; existing hard references stay (D-057).
- **Player-facing text** is `FText` (`LOCTEXT` / `FText::Format`), never a built `FString` (D-061). Shared game words (element, status, trigger names, button verbs) live in C++ `LOCTEXT` name tables (D-062).
- **Saves** use explicit save structs, not `SaveGame` property tags (D-059). Until the first public build, a save below the minimum version is wiped with a warning (D-060).
- **Input:** fixed keys for now; keep every input action name unique so rebinding can come later (D-063). **Every input added or changed gets its row in `docs/Design/InputMap.md` in the same change** (Crown, 2026-10-27); new keys follow its standard-practice defaults.
- **Content naming:** new assets follow `docs/Conventions/ContentNaming.md` (B-123); existing names stay (D-058).
- **Tests:** headless specs until the new HUD (B-105), then functional level tests (D-066). A legacy `*TestActor` is retired only after a matching spec lands and Crown's PIE pass (D-067).
- **Frame budget:** 60 fps at 1080p on Crown's PC, checked once per milestone (D-068).

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
- **`InnateElement` default**: Defaults to `Generic` (not `Fire`). Since GenericSpellInherit, `Generic` means **"inherit"** (a Generic spell takes the caster's element) and `None` means non-elemental (`ESpellElement.h:28-31`). Character class — not `InnateElement` — gates magical capabilities.
- **Animations**: Standard attacks in fixed-position turn-based combat use In-Place anims by default. Root motion is opt-in per montage.
- **Git LFS .uasset files**: Stored as LFS pointers. Cannot be read as text. When Blueprint logic is referenced, ask for a screenshot or text export — don't try to read the .uasset.

## Rules reference — this game

This game's rules live in `docs/Rules/` (start at `docs/Rules/00-index.md`): one page per topic, each rule cited to its ruling (`D-xxx` / `Q-xxx`) or code (`file:line`). Read the relevant page before a rules question; when you apply a decision or build a rule, update its row there.

## Rules reference — the deckbuilder (Crown, 2026-10-26)

The deckbuilder (`CrownedDev/wor-deckbuilder`, a local clone on Crown's PC) is the **simplest level of this same game**. Its rules are the first reference for game rules.
- When this game has no rule for something and the deckbuilder does, **suggest the deckbuilder's rule** as the recommendation in a `Q-<job>` / `D-xxx` entry, citing the page. Start at its `docs/reference/00-index.md`; read only the page the index points to.
- Never copy a rule in without Crown's answer. Where the two games already differ, flag it as a decision; don't "fix" either one.
- The deckbuilder's names are C#/card names (`Affinity`, "Refraction", "Refractor"); translate them to this codebase's names when citing.

## Recommendations — back them with what worked (Crown, 2026-10-26)

When a `Q-<job>` or `D-xxx` entry recommends an answer, **base the recommendation on proven, well-received examples from other games** (what players liked and what worked), named in the entry with a source where one can be found (e.g. "Expedition 33's parry gives AP; widely praised"). Prefer this over reasoning from scratch. **Exception: questions about this game's lore and story** are Crown's alone; give options, no outside recommendation. The deckbuilder rule above still comes first where it has a rule.

**Approve by default, Crown vetoes (Crown, 2026-10-26):** the game's foundation is decided; what's left is building it out. So for **non-lore** questions, **go with the reference-backed answer**: record it as answered ("approved by the reference-backed default"), build with it, and **tell Crown what was decided** in plain words; Crown only replies to change one. Judge against `docs/Design/DesignPillars.md` (what kind of game Crown wants). In Backlog mode, build with that reference-backed answer (as `Q-<job>` entries already "ship with A") and list it in the hand-off for Crown to agree or change. **Stop and ask instead** when the answer would be an extreme deviation from a ruling, or it's lore/story.

## Repo-specific notes

- **LFS coverage** (2026-10-25): `.gitattributes` LFS-tracks `*.uasset`, `*.umap` and source art/audio/fonts (`png jpg jpeg tga psd exr fbx ttf otf wav ogg mp3`). Only originals of assets the game actually uses are committed (under `SourceArt/`); full vendor packs stay out of git (`docs/Design/UI_DeckbuilderReuse.md` §4).
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
8. Don't execute the next step without confirmation — **except in Backlog mode** (below).

**Ask before:**
- Editing more than one file
- Running bash commands that mutate state (`git`, file deletions, mass renames)
- Refactors crossing existing system boundaries

**Don't:**
- Agree with an approach that has problems
- Make breaking changes without flagging them
- Remove code prematurely
- Write summary documentation mid-refactor

## Architectural Documentation

Maintain living system documents in `docs/Architecture/`. Each document covers one system and is updated as the system changes — not just at session end.

### Document structure (per system):
- Overview — what it does and why
- Architecture — key classes, responsibilities, fields
- Integration Points — delegates, subsystem connections
- Known Limitations / TODOs
- Changelog — date, change description, branch

### When to update:
- Any new system → create `docs/Architecture/<SystemName>.md`
- Any significant change to an existing system → update the relevant doc and add a changelog entry
- Session end → verify docs reflect current state

### Existing documents:
- `docs/Architecture/AISystem.md`
- `docs/Architecture/AugmentStoneSystem.md`
- `docs/Architecture/AutomationTests.md`
- `docs/Architecture/BattleRandom.md`
- `docs/Architecture/BossPhases.md`
- `docs/Architecture/BrokenDarkness.md`
- `docs/Architecture/CharacterDataSystem.md`
- `docs/Architecture/CombatCamera.md`
- `docs/Architecture/CombatCharacter.md`
- `docs/Architecture/CombatOrchestrator.md`
- `docs/Architecture/CrystalWear.md`
- `docs/Architecture/CurrencySystem.md`
- `docs/Architecture/DamageCalculator.md`
- `docs/Architecture/DuplicationAudit.md`
- `docs/Architecture/EconomySystem.md`
- `docs/Architecture/InfusionSystem.md`
- `docs/Architecture/ItemSystem.md`
- `docs/Architecture/LoadoutSystem.md`
- `docs/Architecture/Logging.md`
- `docs/Architecture/MemorySystem.md`
- `docs/Architecture/MerchantShopSystem.md`
- `docs/Architecture/PartySystem.md`
- `docs/Architecture/PerInstanceRollSystem.md`
- `docs/Architecture/ReactionTriggers.md`
- `docs/Architecture/ResistanceSystem.md`
- `docs/Architecture/ScalingSystem.md`
- `docs/Architecture/SkillEffectSystem.md`
- `docs/Architecture/StatComposition.md`
- `docs/Architecture/StatusBuildupSystem.md`
- `docs/Architecture/TierOnInstance.md`
- `docs/Architecture/TurnManager.md`
- `docs/Architecture/UISystem.md`
- `docs/Architecture/WeaponSystem.md`
- `docs/Architecture/WeatherSystem.md`

Add more as systems are documented.

Create or update these documents as part of completing tasks, not as a separate afterthought.

## Backlog mode (autonomous work)

The project is worked from one queue, `docs/Backlog.md`, with open questions in `docs/DesignDecisions.md`. Start it with the `work-backlog` skill. In Backlog mode the "Ask before" rules above are replaced by these:

1. **One job at a time**, top-down: the first `TODO` whose dependencies are `DONE` or `REVIEW (in dev)` and whose decisions aren't `OPEN`.
2. **Own branch per job** (`fix/`, `refactor/`, `feature/`, `chore/` + `B-xxx`), from up-to-date **`dev`**. Commit after each cluster (≤3 files). **Never touch `main`** (no commits, merges or pushes): Crown merges `dev → main` after play-testing.
3. **Design questions: reference-backed default, or block.** For a **non-lore** question that fits the design pillars, record a `Q-<job>` entry answered with the reference-backed default ("Recommendations" above), build with it, list it in the hand-off, and keep going. **Only for lore/story, or an answer that would break a ruling or a pillar,** block instead:
   - add or link a **`Q-<job>`** entry in `DesignDecisions.md` (the job's own number: a question from B-155 is Q-155, a second Q-155b; `D-xxx` is for Crown's chat decisions)
   - mark the job `BLOCKED (Q-<job>)`
   - commit that doc change
   - **move on to the next job**
4. **Playable in PIE when it needs UI (Crown, 2026-10-26).** If a job adds something the player does through UI (adding a character to the party, interacting, choosing, buying…) and the real screen doesn't exist yet, ship a **plain placeholder UI** with it: simple buttons and text, built in C++ (no `.uasset` needed), so Crown can do it in PIE. The PIE checklist says how to open it. It's replaced by the real CommonUI screen later (D-024); log it in the job's hand-off.
5. **Prove it before handing it over.** The build must pass and the tests must pass (`Scripts/ue/build.sh`, `Scripts/ue/test.sh`; the Stop hook enforces this when `UE_ROOT` is set). Then run the `wor-reviewer` agent on the diff and fix what it finds.
6. **Hand-off** (only after build + tests + `wor-reviewer` pass):
   - set the job to `REVIEW (in dev)` in `Backlog.md`
   - add a changelog line to the relevant `docs/Architecture/` doc
   - write `docs/PIE/B-xxx.md` in the format of `docs/PIE/B-001.md` (numbered checks: do this / you should see / result box) and add its row to **Pending** in `docs/PIE/README.md`
   - append a plain-words entry to `docs/sessions/<today>.md`: what changed, why, how it was verified, what Crown should check
   - push the job branch, merge it into `dev` with `git merge --no-ff -m "<real message>"` (never `--no-edit`), push `dev`
7. **Stop** after Crown's requested number of jobs, or when only blocked jobs remain. Summarise: done, blocked (with Q-/D- ids), next.

Still always: no Blueprint/`.uasset` edits outside the editor connection, no deleting code before it's verified in PIE, no history rewriting, and the Architecture rules above.

## Session orientation

At the start of a session, the user will state:
- Current branch
- Last committed milestone
- Today's target system

If they don't, ask — don't guess. Session-specific state is not stored in this file.
## Session state

Per-session state lives in `docs/sessions/YYYY-MM-DD.md`. At session start, the user will reference the latest one or paste its contents. If they don't, ask which session doc to read before assuming current state.

The `_TEMPLATE.md` in that folder is the format. Don't read it as state.
