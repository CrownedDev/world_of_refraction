# Implementation vs Best Practice — comparison 2 (2026-10-29)

**Status:** proposal; extends `BestPractice_2026-10.md` (B-047). Inventory and loadouts are in `InventoryArchitecture_2026-10-29.md`. Some references are from memory (marked).

---


**Status: PROPOSAL.** Read-only analysis of `dev` (pulled 2026-10-29). Nothing here is decided, and nothing in the repo was changed.
**Extends** `docs/analysis/BestPractice_2026-10.md` (B-047). That audit covered data assets, naming, saves, text, input, co-op readiness, tests and performance, mostly against Lyra. Its findings are not repeated here.
**Skipped:** inventory and loadout ownership. These are covered in `docs/analysis/UnifiedLoadout_2026-10-29.md` (D-110, B-301..B-309).
**Paths:** paths are relative to `Source/world_of_refraction/` unless they start with `docs/` or `Config/`.
**Sources:** every outside claim has a URL or is marked *from memory*. Lyra details are from memory of the 5.x sample source, because the source sits behind Epic's GitHub org.

---

## 1. GAS usage

**What we do**
- **Three attribute sets**, all replicated with rep-notifies (`Public/GAS/WoRAttributeSets.h:26`, `:82`, `:212`): Vitals; Stats: pillars, 12 sub-stats and derived values; Resistance: 14 `Resist_*`.
- **The ASC sits on `ACombatCharacter`** and uses Mixed replication (`docs/Design/GASMigration.md` §1).
- **Native GE classes, one per family, with SetByCaller magnitudes** (`Public/GAS/WoRGameplayEffects.h`), so there is no GE asset per effect. Lifetime in turns comes from `UWoRTurnEffectTracker` over `FTurnSpan` (`Public/GAS/WoRTurnEffectTracker.h`).
- **Executions:** Damage, Buildup and Tick (`Public/GAS/WoRDamageExecution.h`, `WoRBuildupExecution.h`, `WoRTickExecution.h`). Each wraps a pure math core (`WoRDamageMath`, `WoRBuildupMath`, `WoRTickMath`).
- **One MMC:** `UWoRMMC_ClampedStatTerm` (`Public/GAS/WoRDerivedStatMagnitudes.h`).
- **One translator:** `FWoREffectSpecBuilder` (`Public/GAS/WoREffectSpecBuilder.h`).
- **A custom context:** `FWoREffectContext` (`Public/GAS/WoREffectContext.h`), allocated by a globals subclass.
- **The enum path and GAS run side by side.** `UWoREffectMirrorComponent` copies each enum effect into a GE. `wor.Effects.UseGAS` (default 0) switches which side is read. A GAS read **resyncs the mirror first**, so every read costs a full sync (`docs/Architecture/GASSystem.md`, "Effect mirror").
- **No abilities yet.** There is no `UGameplayAbility` anywhere in `Source/`. Stage 4 (B-043) is BLOCKED on B-042f.

**What the references do**
- Epic's GAS docs describe attribute sets, GEs, executions and abilities as one system, and recommend GAs + AbilityTasks for actions ([Epic: GAS overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-ability-system-for-unreal-engine)).
- **tranek's GASDocumentation:** Executions run on the server only and can't be predicted. That is fine without prediction, and a turn-based game needs none. SetByCaller is the standard way to pass runtime magnitudes. **Mixed replication expects the ASC's OwnerActor's Owner to be the Controller.** An unpossessed actor in Mixed mode won't get its GEs on the owning client unless `SetOwner(PlayerController)` is called. AI should use Minimal. Source: [tranek/GASDocumentation](https://github.com/tranek/GASDocumentation), sections on Replication Mode and Execution Calculations.
- **Lyra** (*from memory*): the players' ASC is on `ALyraPlayerState`; AI pawns own theirs; attributes are `ULyraHealthSet` / `ULyraCombatSet`, and damage goes through `ULyraDamageExecution`; GEs are Blueprint assets, granted through `ULyraAbilitySet` data assets. Lyra never runs a parallel store.

**Gap in plain words**
1. **Two stores of truth are costly, and they are open-ended.** Every effect lives in the enum manager **and** in a GE. Every GAS read resyncs. The cut-over (B-042yc/yd/ye → B-042f → B-043) is about 6 more jobs away, and each new effect feature currently has to be written twice. No reference runs both stores for long. Epic's guidance assumes GAS *is* the store.
2. **The Mixed-mode owner trap.** One player leads up to three characters, but only one is possessed. In co-op, the other two characters' GEs (and so their status tags and durations) will not replicate to their player in Mixed mode, unless each pawn's owner is set to that player's controller. This is not planned in B-230.
3. **Native family GEs instead of assets.** This differs from Lyra, but fits tranek's guidance on native GEs, and it is fine.

**Recommendations**
- **(M) Set a cut-over line.** From now on, a new effect type or read is written **only** on the GAS side (the enum side is frozen). Make B-042f's "green on both paths" the last dual-path gate. Then flip UseGAS to 1 by default and delete the resync-on-read. Reason: Epic and tranek both model GAS as the single store. The double cost here is measurable: the perf audit's "effect mirror" suspect (B-266). Maps to B-042yc..B-042f. Add the rule to `GASMigration.md`.
- **(S) The co-op owner fix.** When the party spawns, call `SetOwner(OwningPlayerController)` on every player-led pawn, and give AI-only enemies Minimal replication. Reason: tranek's Mixed-mode note. New job, one cluster in B-230's area.
- **Keep:** the ASC on the character. One player leads three characters, and AI companions have no PlayerState. Lyra's PlayerState placement assumes one pawn per player.

---

## 2. Turn system

**What we do**
- `UTurnManager` is a `UGameInstanceSubsystem` (`Public/Combat/TurnManager.h:184`).
- **The scheduler is round-based "debt"** (`:172-180`): each round adds `SpeedRatio = speed / slowest` to `TurnsOwed`; the highest net debt acts next. The one step function `AdvanceSimState` drives both live play and a 16-entry preview "belt" (`docs/Architecture/TurnManager.md`).
- **Turn changes:** `DelayNextTurn(Actor, Places)` (`:284`); pinned bonus turns (`ScheduleBonusTurn`, `:292`); ritual execution turns with frozen spell speed (`ScheduleExecutionTurn`, `:302`); first-strike opening rounds (`InitializeCombat`, `:202`).
- **Duration counting** has one shared rule: `FTurnSpan` (`Public/Skills/Effects/TurnSpan.h:16`).
- **The combat state machine is coarse.** `ECombatState` has only `Idle / Initializing / InProgress / Victory / Defeat / Draw / Fled` (`Public/Combat/CombatOrchestrator.h:30-39`). The turn phases live in bools: `bWaitingForAsyncAction` (`:567`), `bRowMoveUsedThisTurn` (`:331`), and the executor's own flags.

**What the references do**
- **FFX CTB:** Each unit has a tick counter set from Agility. Each action's rank sets the delay to the next turn, and Haste / Slow scale ticks. The visible order shows about 16 upcoming turns and updates live when you hover an action ([FF Wiki: CTB](https://finalfantasy.fandom.com/wiki/Conditional_Turn-Based_Battle)).
- **Octopath Traveler** (*from memory*): order is decided per round from speed, the next round's order is shown, and Break pushes a unit to the end of the next round. Delays work on positions, as ours do.
- **Expedition 33** (*from memory*): a speed-based timeline with a visible order list. Its real-time layer is in the actions (parry and dodge), not in the scheduler.
- **XCOM 2:** Turns are team phases (IGOUGO). Every rule change is a new `XComGameState` in a `History`, with a Context saying what caused it. Visualisers play the history back separately (model / view / controller split). Source: [r/xcom2mods wiki: game states](https://r.datuan.dev/r/xcom2mods/wiki/index/game_states) (community mirror, not official) and [Nexus tutorial](https://forums.nexusmods.com/topic/3820875-tutorial-using-gamestate-components).
- **UE turn-flow patterns:** an explicit phase enum or state machine; `UGameplayTask` / `UBlueprintAsyncActionBase` for waits ([Epic: async actions](https://dev.epicgames.com/documentation/en-us/unreal-engine/blueprint-async-actions-in-unreal-engine) *URL from memory*); StateTree for flows ([Epic: StateTree](https://dev.epicgames.com/documentation/en-us/unreal-engine/state-tree-in-unreal-engine)).

**Gap in plain words**
- **The scheduler is as capable as the references.** It does what Octopath and Expedition 33 do (speed order, delays, bonus turns, preview), and ritual turns go beyond them. The one FFX feature it lacks is **action weight**: a heavy skill that makes your next turn come later. Today, only a separate delay effect can do that.
- **The weak point is the turn flow, not the order.** "Whose turn, which phase" is spread over the orchestrator, the executor and the HUD, as bools and timers. B-228 must replicate it. A phase that is a set of flags can't be replicated as one value, and can't be inspected as one value either.

**Recommendations**
- **(M) An explicit turn phase on the GameState component** that B-228 creates. Phases: `TurnStart → AwaitCommand → Executing → DefenseWindow → Resolving → TurnEnd`. It is one replicated enum plus a changed-phase delegate. The HUD reads it, and the old bools become derived from it. Reason: XCOM's single game-state source and every UE co-op sample replicate one turn-state value (Lyra's `ULyraExperienceManagerComponent` loading phases is the same idea, *from memory*). It also gives `wor.Turn` debugging one line. Fold into B-228, as one extra cluster.
- **(S, only if design wants it) Action weight** as an optional `NextTurnDelay` on the skill asset, using the existing `DelayNextTurn`. Reason: FFX X/X-2 rank-based delay is widely praised for depth; Raise as a `Q-` entry, not a job.
- **Keep:** the debt scheduler, the preview belt and `FTurnSpan`. They are simpler than CTB ticks and already tested.

---

## 3. Action execution

**What we do**
- `UActionExecutor` is a `UGameInstanceSubsystem` (`Public/Combat/Actions/ActionExecutor.h:143`). It is about 5.7k lines in `ActionExecutor.cpp`, plus `ActionExecutorCosts.cpp` (758) and `ActionExecutorMontage.cpp` (640).
- **The async path:** starts at `ExecuteActionAsync` (`ActionExecutor.cpp:814`) runs an explicit montage chain `Ritual → Skill → Return → Done` (`EMontagePhase`, `ActionExecutor.h:62`; `BeginMontageChain`, `ActionExecutorMontage.cpp:115`) notifies drive the impacts (`UCombatNotify`, `Public/Combat/CombatNotify.h:33`; `OnSpellAnimNotify`, `ActionExecutor.cpp:5027`; `ApplyOneImpact`, `:1988`) each impact opens a defense window (`UDefenseSystem`, real time, `Public/Combat/Defense/DefenseSystem.h:139-142`) finalize waits for both the montage and the defenses (`TryFinalizeAsyncAction`, `:2154`) a timeout failsafe lands full damage (`OnAsyncActionTimeout`, `:2392`)
- **The state for the action in flight sits on the subsystem:** `PendingSpellCaster`, `PendingSpellData`, `PendingSpellTargets`, … (`ActionExecutor.h:517-545`); `PendingExecutionActor`, `AsyncTimeoutHandle`, `bAsyncCompletionFired`, `AsyncActionSerial` (`:912-970`); `CurrentExecutionContext` (a `TOptional`); So only **one** action can be in flight.
- **The camera** plan is a `TSoftObjectPtr<ULevelSequence> CameraSequence` per skill (`Public/Skills/Definitions/SkillDataBase.h:140`), bound at runtime. It is presentation only (`docs/Design/CombatCamera.md` §3).

**What the references do**
- **GAS abilities:** each activation is an instanced `UGameplayAbility` that owns its state. `UAbilityTask_PlayMontageAndWait` and `UAbilityTask_WaitGameplayEvent` replace hand-built callbacks. `EndAbility` / `CancelAbility` clean up tasks and timers automatically ([tranek: Ability Tasks](https://github.com/tranek/GASDocumentation#concepts-at); [Epic: Gameplay Abilities](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-gameplay-abilities-in-unreal-engine)).
- **Lyra** (*from memory*): `ULyraGameplayAbility_RangedWeapon` and the melee abilities each use montage and event tasks; impacts come from `AnimNotify` → `SendGameplayEventToActor`; damage is a GE with `ULyraDamageExecution`.
- **XCOM 2** (see §2): an action is resolved into game state first, then visualised. Rules never wait on animation.
- **Expedition 33:** `CombatCamera.md` says skills play as Level Sequences. I could not find a primary Sandfall source. An animator's showcase shows UE5 level sequences of boss attacks ([inkl / animator demos](https://www.inkl.com/news/clair-obscur-expedition-33-animation-demos-showcase-the-making-of-the-game-s-dynamic-combat-sequences)). Treat this as partial support. Its parry and dodge still need frame-timed windows inside the presentation.

**Gap in plain words**
- **The sequence matches GAS's shape.** The design (montage → notify → impact → defense → finalize) is exactly what GA + tasks express. We hand-built it, so lifetime is managed by hand: a serial number against stale callbacks, a timeout, a completion-fired guard, and `DebugForceResetAsync` (`:466`). Each of these exists because there is no owning object.
- **Single-instance state blocks three planned things:** reactions or counters that run *during* another action (the `Interrupt / Reaction` turn types `TurnManager.h:20-23` foresees); co-op, where B-231 keeps the action on the server and D-064 forbids per-fight state on GameInstance subsystems; the balance sim running fights quickly.
- **Pure XCOM-style resolve-then-visualise doesn't fit.** Defense timing is real time in the presentation. Our `FActionDamagePreview` (`ActionExecutor.h:108`) is already the "planned hit" half of that idea.

**Recommendations**
- **(M) Before B-043: move the in-flight state into one object.** Create a `UCombatActionRun` (`UObject`, outer = the orchestrator or GameState) that holds what the `Pending*` fields hold. The subsystem keeps the stateless maths. It is mechanical, but it is the exact field set a `UGameplayAbility` instance will own, so B-043 becomes "re-parent and swap callbacks for tasks". Reason: GA instancing (tranek, Epic). D-064. New jobs: 2-3 clusters, before B-043.
- **(L) B-043 as planned:** one GA per delivery shape, `PlayMontageAndWait`, `WaitGameplayEvent(Event.Combat.Impact)`. Keep the defense window as our own task (`UAbilityTask_WaitDefense`), not inside a montage.
- **Keep:** the Level Sequence is camera only, and the rules stay in C++. That keeps headless tests and the sim possible, which the XCOM principle backs.

---

## 4. AI

**What we do**
- `UAIDecisionManager` is a `UGameInstanceSubsystem` (`Public/AI/AIDecisionManager.h:47`, 2.5k lines in the `.cpp`).
- **The flow:** `RequestDecision` → a thinking delay → `ExecuteDecision` (`:135`) → `BuildAction`.
- **Easy** is random. **Medium+** uses `BuildAction_Smart`: survival → cleanse → offence, in a fixed **call order** (`docs/Architecture/AISystem.md` "Smart decision branches").
- **Offence scores:** targets by kill potential + missing HP + threat (`ScoreTarget`, `:188`); actions by estimated damage + status value, through the **real** damage calculator ("hard wall", `docs/Design/AIArchitecture.md` §4).
- **No BT, Blackboard or perception in combat**, on purpose (`Public/Combat/AI/CombatAIController.h:16`).
- **The dual-utility design** (rank band + score in a policy asset, Kevin Dill) is written in `AIArchitecture.md` §4, §7, but **no backlog job builds it**.
- B-259 (memoise estimates) and B-241 (one infusion decision) are queued.

**What the references do**
- **Dual-utility reasoning** ([Dill, Game AI Pro 2 ch. 3](http://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter03_Dual-Utility_Reasoning.pdf)) and the **Infinite Axis Utility System** ([Dave Mark & Mike Lewis, GDC 2015 "Building a Better Centaur"](https://www.gdcvault.com/play/1021848/Building-a-Better-Centaur-AI)): considerations are response curves multiplied together, authored as data; a "why" dump per candidate.
- **XCOM 2** (*from memory*): enemy behaviour is authored as data-driven behaviour trees of weighted moves in config (`AIBehaviors` in its `.ini` files), and modders tune it without code.
- **Darkest Dungeon** (*from memory*): each monster skill has data-authored target weights (for example, prefer marked or low-HP heroes) and per-skill chance; the AI is simple, and the readability comes from the data. I found no AI talk. The design postmortem is [Tyler Sigman, GDC 2016](https://www.gdcvault.com/play/1023089/) (overview only).
- **UE StateTree** suits a flow with states and utility selection (Epic's StateTree has utility considerations from 5.5, *from memory*), and BTs suit overworld behaviour ([Epic: StateTree](https://dev.epicgames.com/documentation/en-us/unreal-engine/state-tree-in-unreal-engine)).

**Gap in plain words**
- **The scoring is right; the priorities are code.** Changing "cleanse before heal", or making a boss archetype prefer status, means editing call order in C++. Every reference above puts that in data.
- **There is no "why did it pick this" dump per candidate.** `AIArchitecture.md` §8 itself says this is required.
- **Target and action are scored as separate steps.** A cheap AoE across three enemies can't beat a big single hit (`AIArchitecture.md` §8).

**Recommendations**
- **(M) Schedule the designed policy asset** as `UCombatAIPolicyData`, in about 3 clusters:
  1. a band + score candidate list, with today's order as default bands (same choices, pinned by spec);
  2. the policy asset per archetype;
  3. `wor.AI.Why`, which prints rank and score per candidate.
  - Reason: Dill's dual-utility and IAUS are the best-documented game-AI practice, and XCOM and DD both tune AI as data.
  - New jobs. Do them after B-241 / B-259 (shared code).
- **Keep:** **No BT or StateTree in combat.** A single per-turn choice is a utility problem, not a flow problem; BTs earn their keep in the overworld (AIArchitecture §3). **The hard wall.** Estimates go through the real calculator.

---

## 5. Combat orchestration and subsystems

**What we do**
- `ACombatOrchestrator` is a level actor (`Public/Combat/CombatOrchestrator.h:127`).
- **22 `UGameInstanceSubsystem`s.** The ones that hold fight state: TurnManager, ActionExecutor, SkillEffectManager, StatusBuildupManager, DefenseSystem, CombatGridSubsystem, WeatherStateManager, InfusionChargeManager, AIDecisionManager CombatCommandMenuSubsystem: UI state, `Public/UI/Combat/CombatCommandMenuSubsystem.h:47`, 2.1k lines
- **One `UWorldSubsystem`:** `UBattleRandomSubsystem` (`Public/Combat/Mechanics/BattleRandomSubsystem.h:15`).
- D-064 forbids new per-fight state on GameInstance subsystems. `docs/Design/CoopStateOwnership.md` maps the move.
- `AWoRGameState` exists only on the unbuilt branch `feature/B-227-seamless-travel`.

**What the references do**
- **Epic's lifetimes:** GameInstance subsystems live across levels and are never replicated; World subsystems live and die with the world; only actors and components replicate (GameState, PlayerState, their components). Sources: [Epic: Programming Subsystems](https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-subsystems-in-unreal-engine); [Epic: GameMode and GameState](https://dev.epicgames.com/documentation/en-us/unreal-engine/game-mode-and-game-state-in-unreal-engine).
- **Lyra** (*from memory*): match state is in **GameState components** (`ULyraExperienceManagerComponent`, the bot creation component, and so on), added by experience definitions; stateless services are subsystems; `UGameplayMessageSubsystem` handles decoupled events.

**Gap in plain words**
- **Fight state outlives the fight.** A GameInstance subsystem survives level travel, so every system needs a reset. B-246 exists because the three fight-ending paths don't reset the same things. A World subsystem resets itself when the battle level unloads.
- **For co-op, the state must reach clients.** Only GameState and actor components replicate, so the end state is the same as D-064.

**Recommendations**
- **(S) A cheap first step for stateless-but-per-fight services:** move them to `UWorldSubsystem`, following `UBattleRandomSubsystem`. Candidates: DefenseSystem, InfusionChargeManager, WeatherStateManager. The reset bugs go away because the world dies. Reason: Epic's subsystem lifetimes. We already have the precedent in-house. This is one new job per system (each ≤3 files where the `GetGameInstance()->GetSubsystem` call sites are few; count first).
- **(L, already planned)** Replicated state goes to GameState and combatant components (B-228, B-230). Keep Lyra's experiences **out**: one game mode per level is enough here (B-047 already rejected Game Features).

---

## 6. UI

**What we do**
- **The CommonUI base (B-103):** `UWoRRootLayout` with Game / Menu / Modal `UCommonActivatableWidgetStack`s (`Public/UI/WoRRootLayout.h:30-87`; Modal is still optional, with a TODO); `UWoRActivatableWidget`; `UWoRButtonBase : UCommonButtonBase`.
- **Every placeholder and HUD widget is a plain `UUserWidget` added with `AddToViewport`:** 15 call sites, for example `Private/UI/Hub/ItemStorageWidget.cpp:76`, `Private/UI/Trial/TrialGateWidget.cpp:70`, `Private/UI/Combat/CombatHUD.cpp:55-200`, `Private/Trial/BattleGameMode.cpp:617`. Only `UWoRTestScreen` uses the stack.
- **The HUD is a `UObject` (`UCombatHUD`, `Public/UI/Combat/CombatHUD.h:31`).** Command-menu state sits in a GameInstance subsystem.

**What the references do**
- **Lyra** (*from memory*): `UPrimaryGameLayout` with layers named by tag (`UI.Layer.Game / GameMenu / Menu / Modal`); every screen is pushed with `UCommonUIExtensions::PushContentToLayer_ForPlayer`; HUD pieces are slotted through the **UI Extension** subsystem; input mode and focus come from the activatable widget's `GetDesiredInputConfig`.
- **Epic CommonUI docs:** input routing, gamepad focus and back handling only work for widgets inside activatable stacks ([Epic: CommonUI](https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine)).
- **MVVM plugin:** the view binds to a viewmodel, and the game writes the viewmodel ([Epic: UMG Viewmodel](https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-viewmodel)).

**Gap in plain words**
- **The placeholders skip CommonUI routing,** which is the reason we adopted CommonUI. So gamepad or keyboard navigation, back-to-close and input-mode switching don't work on them.
- **The B-105 HUD has a choice to make.** Built on this pattern, it would repeat the problem. It also has to pick between "widgets read subsystems" and a viewmodel the turn state writes.

**Recommendations**
- **(S) Placeholder screens become `UWoRActivatableWidget`s pushed with `PushToLayer(Menu)`.** This is about 7 widgets, two or three per job. Their C++-built content stays. Reason: CommonUI only routes input for widgets in stacks, and Lyra pushes every screen. New jobs; also needed by D-063 / B-128 gamepad work.
- **(M, decide in B-105) A combat HUD viewmodel** (`UMVVMViewModelBase`), fed from the B-228 GameState turn phase and the attribute sets. Reason: Epic's MVVM is the supported path, and it gives co-op clients the same feed. Put a `Q-105` entry in front of Crown. If the plugin feels heavy, a plain C++ "HUD model" UObject with delegates is the light version.
- **Skip:** the UI Extension subsystem. There is one HUD and no Game Features.

---

## 7. Data-driven content

**What we do**
- **18 `UPrimaryDataAsset` types,** among them skills, effects, characters, bosses, encounters, memories, reaction triggers, story lines and resistance profiles.
- **The Asset Manager is registered** for the saved types (`Config/DefaultGame.ini:7-11`).
- **Effects are an enum of 110 types** (`Public/Skills/Effects/ESkillEffectType.h:19`) with payload structs. `docs/analysis/EffectsOverhaul.md` already compares them with the deckbuilder's verb-row model.
- **Conditions are structs:** reaction triggers (`Public/Reaction/ReactionTrigger.h:22`) and memory conditions (`Public/Memory/MemoryData.h:99`, `EMemoryConditionKind`).
- **Tuning:** `UCombatTuningData` holds curve-table ladders as `FScalableFloat`, with C++ fallback (`Public/Combat/Tuning/CombatTuningData.h:20`; `docs/Architecture/CombatTuning.md`). No asset is committed yet. Stat caps stay in C++ (Q-218).
- **No `UDataTable` or Data Registry** anywhere.

**What the references do**
- **Epic:** data tables for row-shaped bulk data; curve tables with `FScalableFloat` for level-scaled numbers (GAS-native); [Data Registries](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-registries-in-unreal-engine) to merge tables from many sources; [data assets](https://dev.epicgames.com/documentation/en-us/unreal-engine/data-assets-in-unreal-engine) for rich objects.
- **Lyra** (*from memory*): pawn data, ability sets and input configs as data assets; experience definitions compose them.
- **The deckbuilder** (`wor-deckbuilder/docs/reference/01-card-model.md`; `04-effects.md` per EffectsOverhaul): one row = verb + target + scope + condition + `Lasts`; a closed verb list (24-28 verbs), not 110 types.

**Gap in plain words**
- **Our asset choice matches Epic's guidance.** Rich objects are assets; numbers are curves.
- **The real gap is the effect vocabulary.** 110 enum types means 110 code paths and a GE family per type. The deckbuilder reaches similar expressiveness with about 28 verbs plus fields. EffectsOverhaul (B-044) already argues this, and it should land **before** GAS stage 4 freezes the family list.
- **Tuning is built but has no asset,** so designers can't use it yet.

**Recommendations**
- **(S) Commit `DA_CombatTuning_Default` and its curve table** (editor step, B-218's PIE). Then move the Q-218 caps when B-042's MMCs need them. Reason: Epic's `FScalableFloat` + curve-table pattern.
- **(L, existing analysis)** Act on EffectsOverhaul's verb-row merge before B-043. Reason: the deckbuilder rule comes first (CLAUDE.md), and GAS families map 1:1 to verbs more cleanly than to 110 types. Maps to the jobs that come out of B-044.
- **Keep:** data assets over data tables for skills and effects. Validation and CallInEditor buttons need UObjects, and a table row can't hold instanced subobjects.

---

## 8. Saves (re-check after B-090 / B-124 / B-208)

**What we do**
- `UWoRSaveGame : ULocalPlayerSaveGame` with version `LATEST = 3`, `MIN_SUPPORTED = 1` (`Public/Save/WoRSaveGame.h:15-17`).
- Explicit plain structs (`Public/Save/WoRSaveTypes.h`), and asset refs as `FPrimaryAssetId` + path (`FSavedAssetRef`).
- **One slot, `WoR_Main`.**
- **Save points:** async save at trial or fight exit; sync save at shutdown (`Private/Save/SaveSubsystem.cpp:293`).
- **The run save** (B-208) resumes at the last fight exit.

**What the references do**
- **Epic:** `ULocalPlayerSaveGame` with `GetLatestDataVersion` / `HandlePostLoad` is the recommended 5.x path ([Epic: Saving and Loading](https://dev.epicgames.com/documentation/en-us/unreal-engine/saving-and-loading-your-game-in-unreal-engine)). We match it.
- **Hades and Slay the Spire** (*from memory*): they checkpoint the run at room or fight boundaries, as we do; Slay the Spire also keeps a `.backUp` copy of the save so a corrupt write can be recovered (its `preferences`/`saves` folders hold `.autosave` + `.backUp` files, *from memory*).
- **Industry practice:** write to a temp file and then rename, and keep the previous good file. Console certification requires surviving power loss during a save (*from memory*).

**Gap in plain words**
- **The save shape is right.**
- **There is no protection against a torn write.** If the game dies mid-save, the one slot can be lost, and D-060's wipe-on-old-version would then wipe silently.
- **B-234 (co-op checkpoint copies) will need the same "last good copy" idea** anyway.

**Recommendations**
- **(S) A rolling backup.** Before each write, copy `WoR_Main` to `WoR_Main_Prev`. On a failed load or a version below the minimum, try the previous copy before wiping, with a warning. Reason: Slay the Spire's backup files and standard cert practice. New job, 2 files (`SaveSubsystem.h/.cpp`) + spec.
- **Keep:** explicit structs, `FPrimaryAssetId` refs, the version hook, and run checkpoints at fight exit.

---

## 9. Testing and tooling

**What we do**
- **About 120 Automation Specs with about 765 `It` cases,** run headless by `Scripts/ue/test.sh`.
- `FWoRTestWorld` gives a real GameInstance and a hand-ticked world (`Private/Tests/WoRTestWorld.h:27`). Real orchestrator fights run headless (`CombatHangFixesSpec`).
- **The `DataValidation` commandlet** runs with a known-error baseline (`Scripts/ue/validation_baseline.txt`, B-119).
- **84 `wor.*` console commands.** `Compare*` parity checks cover tags, attributes and effects.
- **Balance sim is designed,** not built (`docs/Design/BalanceSimulator.md`, B-291..B-293).
- **No CI:** no `.github/workflows`. The Stop hook runs the scripts only when `UE_ROOT` is set (Crown's PC).
- **No Functional Tests yet** (D-066: after B-105).

**What the references do**
- **Epic:** [Automation Spec](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-spec-in-unreal-engine) for unit and feature tests; [Functional Testing](https://dev.epicgames.com/documentation/en-us/unreal-engine/functional-testing-in-unreal-engine) for in-level tests; [Gauntlet](https://dev.epicgames.com/documentation/en-us/unreal-engine/gauntlet-automation-framework-in-unreal-engine) for multi-process sessions, such as client + server and soak tests.
- **Indie UE CI** (*from memory*): a self-hosted runner (GitHub Actions / Jenkins / Horde) on a dev machine, nightly build + tests + cook, and results posted to the repo.
- **Balance sims:** Into the Breach / Slay the Spire style automated play runs are widely credited for tuning (*from memory*); Hearthstone's team used simulators (*from memory*).

**Gap in plain words**
- **The test depth is better than most indie UE projects** and beyond Lyra, which ships few tests.
- **The gap is when tests run.** Cloud sessions can't build. "REVIEW (branch, UNBUILT)" rows pile up in the backlog (B-241, B-242, B-250..B-256 and more) until Crown's PC runs them.
- **Co-op tests will need two processes.** That is Gauntlet, or at least PIE multi-client functional tests, and B-228+ checks are written as manual 2-player PIE.

**Recommendations**
- **(S/M) A self-hosted GitHub Actions runner on Crown's PC.** It runs `build.sh` + `test.sh` on every push to a `fix/ refactor/ feature/` branch and posts the result as a check. This turns "UNBUILT" branches green or red without Crown's time. Reason: standard indie UE CI ([GitHub: self-hosted runners](https://docs.github.com/en/actions/hosting-your-own-runners/managing-self-hosted-runners/about-self-hosted-runners)). New job (`.github/workflows/ue.yml` + docs). Crown installs the runner.
- **(M, with B-228) One PIE multi-client functional test** (listen server + 1 client, `-game` with the net mode set) that checks the turn phase replicates. Gauntlet comes later, only if multi-process soak tests are wanted. Reason: Epic Functional Tests run in PIE net modes (*from memory*).
- **Keep:** the headless specs-first rule (D-066); the `Compare*` parity commands (rarely seen elsewhere, and exactly what a parallel migration needs); the sim plan reusing `FWoRTestWorld` and the seeded battle stream.

---

## 10. Networking plan

**What we do (plan)**
- **D-103:** a listen server, EOS with Steam on top, up to 3 players; joining at rest spots; checkpoint save copies with host hand-over at a rest spot; no mid-fight host migration.
- **The order:** B-227: seamless travel, `AWoRGameState`; B-228: turn state on the GameState; B-229: Server RPCs; B-230: replicated combatant components; B-231: server-side actions, multicast presentation; B-232 .. B-235: run, party, EOS, personal rewards.
- **Code today:** `WoRAuthority::HasAuthority` (`Public/WoRAuthority.h`); attribute sets and the currency / character-data components replicate; no RPCs yet; `OnlineSubsystem` is commented out (`Source/world_of_refraction/world_of_refraction.Build.cs:29`).

**What the references do**
- **Epic:** [seamless travel](https://dev.epicgames.com/documentation/en-us/unreal-engine/travelling-in-multiplayer-in-unreal-engine) with `CopyProperties` on the PlayerState; [Actor role and authority](https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-role-and-remote-role-in-unreal-engine); the [EOS Online Subsystem](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-eos-plugin-in-unreal-engine); [Iris](https://dev.epicgames.com/documentation/en-us/unreal-engine/iris-replication-system-in-unreal-engine), which is opt-in and aimed at large actor counts.
- **Lyra** (*from memory*): a listen server or dedicated server, CommonUser for sessions and EOS, and the ASC replicating effects.
- **Turn-based co-op examples** (*from memory*): Divinity: Original Sin 2 uses a host-authoritative listen server, and players control their own characters within one shared turn order. XCOM 2's multiplayer sends commands, and both machines run the same deterministic rules. Baldur's Gate 3 is the same host model as Divinity.
- **Command vs state replication.** Lockstep (sending commands only) suits deterministic turn games; it is low-bandwidth and needs bit-exact determinism. Replicating server state is what UE gives you.

**Gap in plain words**
- **The plan is the standard UE shape** (host authority, replicated state, Server RPCs for input) and matches Divinity / BG3 co-op. That is the right call: the code isn't bit-deterministic (floats, timers), so lockstep would be risky.
- **Three things are missing from the plan:**
  1. The **Mixed-mode owner** for unpossessed party pawns (§1).
  2. **Defense timing over the network.**
     - The defense window is local real time (`DefenseSystem.h:139-142`).
     - A client's Guard press via a Server RPC arrives about one RTT late, so a 0.3 s window feels shorter.
     - Fighting and rhythm games solve this by judging on the pressing machine and sending the result, with the server sanity-checking it.
     - Expedition 33 is single-player, so it has no lesson to give here.
  3. **Iris isn't needed.** The actor count is tiny.

**Recommendations**
- **(S, into B-229) Client-judged defense.** The defending player's client times the window against the multicast impact cue and sends `ServerSubmitDefense(ImpactId, Result, Offset)`. The server accepts it if the offset is within the window plus a latency allowance. Reason: client-side judgement with server validation is the standard answer for timing-critical input; GAS's own local-predicted activation follows the same idea ([tranek: Prediction](https://github.com/tranek/GASDocumentation#concepts-p)). Raise as Q-229 (it changes how the rule is judged).
- **(S, into B-230)** The owner fix from §1.
- **Skip Iris** (opt-in, beta-era, nothing gained at this scale) and **dedicated servers** (D-103).
- **Keep:** checkpoint copies plus "the fight is exited on host loss". This is simpler than mid-fight migration, which few shipped games attempt.

---

## Ranked: the 10 most valuable changes (value × effort)

| # | Change | Size | Why first | Maps to |
|---|---|---|---|---|
| 1 | Self-hosted CI runner: build + test on every push | S/M | Unblocks about 15 "UNBUILT" branches; every later item gets checked | **New job** (1 cluster: workflow + doc); Crown installs the runner |
| 2 | GAS cut-over line: freeze the enum path, UseGAS=1 default after B-042f, remove resync-on-read | M | Stops paying for every effect twice; perf suspect (B-266) | B-042yc..B-042f; rule into `GASMigration.md` |
| 3 | Explicit turn phase as a replicated enum on the GameState component | M | One value for HUD, co-op, sim and debugging | **Fold into B-228** (+1 cluster) |
| 4 | In-flight action state into `UCombatActionRun` before B-043 | M | Removes the hand-built lifetime guards; prepares GAs; D-064 | **New jobs** (2-3 clusters), before B-043 |
| 5 | Mixed-mode owner = player controller for every player-led pawn; Minimal for AI | S | A silent co-op bug otherwise | **New job** (1 cluster) beside B-230 |
| 6 | Placeholder screens pushed onto the CommonUI stacks | S | Gamepad, back and input mode work on them | **New jobs** (≈3 clusters of 2-3 widgets) |
| 7 | AI dual-utility policy asset + `wor.AI.Why` | M | Designed but never scheduled; makes AI tunable as data | **New jobs** (3 clusters) after B-241 / B-259 |
| 8 | Save rolling backup + "try previous before wipe" | S | Cheap insurance; B-234 needs the same | **New job** (1 cluster) |
| 9 | Client-judged defense with server validation | S | Defense must feel the same online | **Q-229** + inside B-229 |
| 10 | Per-fight stateless services to `UWorldSubsystem` (Defense, Infusion, Weather) | S each | Kills reset-tail bugs (B-246's class) | **New jobs**, 1 per system; count call sites first |

Also worth doing, outside the top 10:
- Commit the tuning asset (B-218 PIE).
- Land the EffectsOverhaul verb-row merge before B-043 (L).
- A combat HUD viewmodel decision in B-105 (Q-105).
- One multi-client functional test with B-228.
- Action weight, as a design `Q-` only.

## What we do better than the references (keep)

- **`FTurnSpan`: one pure, tested turn-counting rule** shared by the bars and the GAS tracker (`TurnSpan.h:16`). GAS has no turn durations; we added the missing piece in the smallest form.
- **Native GE family classes + SetByCaller + one spec builder.** There are no hundreds of GE assets. This is leaner than Lyra's asset-per-effect for a data-heavy RPG.
- **Pure maths cores under the executions** (`WoRDamageMath`, `WoRBuildupMath`, `WoRTickMath`). The executions and the old path share one formula, so parity is provable by spec.
- **`Compare*` parity commands and the dual-path specs.** A migration you can measure, which few projects have.
- **The seeded per-battle random stream** (`UBattleRandomSubsystem`, already a World subsystem). Fights replay from a seed, which enables the balance sim. XCOM and Into the Breach rely on the same property (*from memory*).
- **The AI's "hard wall".** Estimates go through the real damage calculator and submit through the player pipeline, so there is no second rules path.
- **Headless whole-fight tests** (`FWoRTestWorld` + a real orchestrator). Lyra ships far less.
- **Data validation in the test run with a known-debt baseline** (B-119). Epic ships the commandlet; few teams gate on it.
- **The ASC on the character, not on PlayerState.** This is right for "one player, three characters" (§1).
- **Level Sequences are camera only; rules stay in C++** (`CombatCamera.md` §3). This keeps tests and the sim headless.

## Sources not checked live

- I could not open Lyra's source; everything about it is from memory of the UE 5.x sample.
- The XCOM 2 architecture comes from community modding wikis, not Firaxis.
- The Expedition 33 "every skill is a Level Sequence" claim is only partly supported (an animator's demos). I found no Sandfall talk.
- I found no Darkest Dungeon AI talk.
