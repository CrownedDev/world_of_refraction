# Performance Audit (2026-10-28)

**Status:** proposal from reading the code (no profiling possible in the cloud); findings turned into backlog jobs B-258 onward (Phase 1c). Confirm with the profiling steps on the desktop (D-068 milestone check).

---


Date: 2026-10-28 · Branch read: `dev` · Code: `Source/world_of_refraction` (637 files, ~136k lines)
Method: code reading only (no Unreal here, nothing profiled). Costs are estimates from patterns and call
counts. Every item below is a **proposal** for Crown, not a decision. Duplication (`docs/analysis/DryAudit_2026-10-28.md`)
and data-model simplification are covered by other audits and not repeated.

## The short version

The game is in good shape on the "per frame" side: almost nothing ticks, the UI is event-driven and pools its
widgets, the turn preview is cached. The cost is **per hit and per AI turn**, and it comes from one pattern:
**stats are recomposed from scratch on every read, and each recompose copies the whole active loadout and
(at `wor.Effects.UseGAS 1`) re-syncs the effect mirror.** Because one hit fires several "sync every attribute"
events, and one AI turn runs ~50-100 full damage previews, that pattern multiplies into thousands of heap
allocations in a single frame. On a 16.6 ms budget (D-068) that can show up as a hitch on AI turns and on
multi-target hits, mostly in Development/PIE builds.

Fix order that gives the most for the least risk: **P1 (S) → P2 (M) → P3 (M) → P4 (S, folds into B-140)**.

## Ranked findings (impact × frequency)

### P1 — `FCombatLoadout` copied by value on every stat read · HIGH · per stat read (hundreds per hit) · S
- `GetActiveLoadout()` already returns `const FCombatLoadout &` (`LoadoutComponent.cpp:164-178`), but 22 call
  sites store it by value: `const FCombatLoadout Loadout = GetActiveLoadout();`. The hot ones are in the stat
  getters: `LoadoutComponent.cpp:4129` (`GetActiveStatBonus`), `:4247`, `:4355`, `:4442`, `:4524`, `:4602`, `:4622`
  (`GetActivePrimaryEvolutionAttachment`, whose comment wrongly says "GetActiveLoadout copies"), `:2773`; and
  `CharacterDataComponent.cpp:686` (`BuildPillarLayers`). Others: `SkillEffectManager.cpp:534`,
  `ActionExecutor.cpp:4168, 4211, 4362, 5531`, `CrystalManager.cpp:224, 303`, `DurabilityHeaderWidget.cpp:79, 320`.
- `FCombatLoadout` (`Public/Loadout/FCombatLoadout.h`) holds an `FString`, 6+ `TArray`s (spells, rings, BD pools,
  item slots) and two weapon entries that each carry their own arrays: roughly 10-20 heap allocations per copy.
- A single pillar read (`BuildPillarLayers`, `CharacterDataComponent.cpp:645-740`) makes 3 copies
  (`GetActivePrimaryEvolutionAttachment` + line 686 + `GetActiveStatBonus`). `GetStatLayers`
  (`:1170`) calls `GetActiveStatBonus` again per stat.
- **Fix:** change each to `const FCombatLoadout &` (read-only use). `GetActivePrimaryEvolutionAttachment` can
  return `const FEvolutionAttachment *` or keep by-value but read from the reference.
- **Files (clusters ≤3):** (a) `LoadoutComponent.cpp`; (b) `CharacterDataComponent.cpp`, `SkillEffectManager.cpp`,
  `CrystalManager.cpp`; (c) `ActionExecutor.cpp`, `DurabilityHeaderWidget.cpp`.
- **Risk:** low. Only unsafe where a caller mutates `SavedLoadouts` while holding the reference (check
  `CrystalManager.cpp:303` "AfterLoadout" and `ActionExecutor.cpp:4168`, which is non-const: keep those as copies).

### P2 — "Sync every attribute" fires several times per hit · HIGH · per hit / per HP or EP change · M
- `UWoRAttributeSyncComponent::SyncAttributes` (`WoRAttributeSyncComponent.cpp:204`) recomputes ~45 attributes
  via `ReadSourceValues` (`:141-200`): 16 composed stats, 3 pillars, 14 type resistances (each one
  `GetEffectsByType` ×2 by value + `GetActiveResistanceBonus`, i.e. another loadout copy:
  `ResistanceQuery.cpp:76-103`), crit, defense, turn speed.
- It is bound to **OnHPChanged, OnEPChanged, OnEffectApplied/Removed/StacksChanged, OnStatusBuildupChanged,
  OnLoadoutChanged, OnTurnStarted, speed changed** (`:521-545`, handlers `:568-616`). One hit = target HP change +
  buildup change (+ attacker EP spend) = 2-3 full syncs, each ~50-100 loadout copies after P1 is skipped.
  An AoE or multi-hit skill multiplies this per target per hit; DOT/HOT payouts at turn start add more.
- Estimated: ~0.5-3 ms per hit in Development, more for AoE. Not visible alone, but it stacks with P3 and VFX spawns.
- **Fix (two steps):** (1) vitals-only path: `HandleVitalChanged` writes just Health/MaxHealth/Energy/MaxEnergy
  (no stat recompose); buildup handler writes just Buildup. (2) coalesce the rest: mark dirty and run one
  `SyncAttributes` on the next tick (`GetWorld()->GetTimerManager().SetTimerForNextTick`) or at action end,
  not once per event. Keep a `wor.GAS.SyncStats` debug counter (per fight) in `WoRAttributeSyncComponent` so
  the saving is visible.
- **Files:** `WoRAttributeSyncComponent.h/.cpp` (+ spec). **Risk:** medium: a read between the event and the
  deferred sync must still see fresh values. `ReadComposedOnGAS` already refreshes the one stat it reads first
  (`:103-122`), which covers that case for stats. Check B-134 baseline / `WoR.GAS.*` specs stay green.

### P3 — AI turn runs ~50-100 full damage pipelines in one frame · HIGH · per enemy turn · M
- `SelectBestTarget` (`AIDecisionManager.cpp:715-755`) calls `EstimateBestDamage` for every enemy over every
  spell, ability and weapon attack (`:757-805`); then action scoring repeats it for the best target
  (`:1501-1590`, including `EstimateStatusScore`), and action building repeats it again (`:1685-1790`).
- Each estimate (`EstimateActionDamage`, `:841-899`) = `ComputeActionStatModifiers` + `PreviewActionDamage`
  + `CalculateDamage` + `GetCriticalChance` + `GetCritDamageMultiplier`: dozens of stat reads, each hitting P1 and,
  at `UseGAS 1`, P5. 3 targets × ~12 skills ≈ 36 + 12 + 12 ≈ 60 estimates. Rough guess: 5-20 ms on one frame
  in Development, which is a visible hitch right after the "thinking" delay (`:127-140`).
- **Fix:** memoise within one decision: a `TMap<(Skill, Target), int32>` of estimates cleared at
  `MakeDecision` start, so the three passes share one computation; and skip status scoring for skills with no
  status payload. Later (after P1/P2), consider a per-decision stat snapshot.
- **Files:** `AIDecisionManager.h/.cpp`, plus `AIDecisionManagerDebug` (print hit/miss counts).
- **Risk:** low: the decision reads state but does not change it, so a per-decision cache cannot go stale.

### P4 — `LoadSynchronous` at cast/defense time · MED-HIGH (hitch on first use) · per first cast of each VFX · S-M
- Already tracked as **B-140** (blocked on the B-142 Insights baseline). Sites B-140 lists are in
  `ActionExecutor.cpp` (now `:3420-3422, 3479, 3546, 5043`). **Not on its list yet:**
  `ActionExecutorMontage.cpp:537` (every montage VFX notify, `SpawnVFXArrayEntry`),
  `DefenseSystem.cpp:551, 555` (Perfect Guard flash + sound: a hitch on the exact frame the player is rewarded
  for timing, the worst place for one), and `CombatCameraDirector.cpp:276` (camera sequence, has a B-140 TODO).
- **Fix:** add these four to B-140's preload set (equipped skills' VFX entries + each fighter's `Cosmetics` +
  camera sequences), hold handles in the orchestrator for the fight, release at end.
- **Risk:** low. Memory cost of holding a fight's VFX is small.

### P5 — Effect reads at `UseGAS 1` re-sync the mirror and allocate per call · MED · per stat read · M
- `USkillEffectManager::GetTotalStatModifier` / `...ForElement` (`SkillEffectManager.cpp:2373-2421`) call
  `UWoREffectMirrorComponent::GetGASReadSource` (`WoREffectMirrorComponent.cpp:267-281`), which calls
  `SyncMirror()` **on every read**. `SyncMirror` (`:97-150+`) copies the actor's whole effect list
  (`GetActiveEffects` by value; `FActiveSkillEffect` has 3 `FString`s + a `TArray`) and does O(n²) matching.
  Then `SumWhere` (`WoREffectSpecBuilder.cpp:70-87`) builds a `TFunction` + query and allocates a handle array.
  `GetNetStatModifier` (`:2423`) makes up to 3 such reads per stat.
- **Fix:** a "mirror dirty" flag set by the effect events; `GetGASReadSource` calls `SyncMirror` only when dirty.
  In `SumWhere`, iterate `ASC.GetActiveGameplayEffects()` directly instead of `GetActiveEffects(Query)`.
- **Files:** `WoREffectMirrorComponent.h/.cpp`, `WoREffectSpecBuilder.cpp`. **Risk:** medium (the "read between a
  change and its event" case that comment `:278` guards): every mutation path must set the flag. Do it after
  GAS stage 3 settles, or guard it behind a cvar for an A/B check.

### P6 — Cached equipment/resistance bonus · MED · per stat read · M (after P1)
- `GetActiveStatBonus` (`LoadoutComponent.cpp:4082-4240`) and `GetActiveResistanceBonus` re-sum every equipped
  piece with tier scaling on every read, though gear only changes on loadout/inventory/durability events.
- **Fix:** cache both on `ULoadoutComponent`, invalidated on `OnLoadoutChanged`, inventory change and crystal
  break; add `wor.Loadout.VerifyBonusCache` (computes fresh, logs a mismatch) as the debug tool.
- **Risk:** medium (stale cache if some mutation doesn't broadcast). Only worth doing if Insights still shows
  these after P1-P3.

### P7 — Logging at Log/Display in game paths · MED in Dev/PIE, none in Shipping · per action / per hit · S
- ~706 `UE_LOG(..., Log|Display)` lines outside Debug/Test files. Heaviest: `SkillEffectManager.cpp` (72),
  `ActionExecutor.cpp` (71), `CombatOrchestrator.cpp` (35), `CombatCommandMenuSubsystem.cpp` (33),
  `TurnManager.cpp` (22), `AIDecisionManager.cpp` (22, mostly `LogTemp`). Each Log line formats an `FString` and
  writes the log file. B-141 moved per-hit combat lines; the effect manager, AI and orchestrator remain.
  Examples: `InfusionChargeManager.cpp:191` (level change while charging), `DefenseSystem.cpp:558`,
  `SkillProjectile.cpp:359` (every overlap).
- **Fix:** a B-141 follow-up: per-effect/per-AI-step lines to `Verbose` on `LogWoREffects` / a `LogWoRAI` category.
  Shipping strips logs anyway, so this is about PIE feel and Insights noise. **Risk:** none.

### P8 — Per-frame work (all small) · LOW · per frame
- Only one actor ticks: `ASkillProjectile` (`SkillProjectile.cpp:47, 97`), only while in flight. It calls
  `SetActorLocation` then `SetActorRotation` (`:328-329`) with overlap events on (`:57-58`): two overlap updates per
  frame. Use one `SetActorLocationAndRotation`. Also turn tick off until `Launch`.
- `UCombatAnimInstance::NativeUpdateAnimation` (`CombatAnimInstance.cpp:32-110`) per character per frame:
  `GetSubsystem<UTurnManager>`, `IsArmed()` and `GetCurrentStance()`, which do 3-4 `FindComponentByClass`
  (`LoadoutComponent.cpp:4071-4078, 4057-4068`). ~30 lookups/frame for 8 fighters: microseconds. Proposal: cache
  stance/armed from `OnLoadoutChanged`; low priority.
- `UInfusionChargeManager` runs a 60 Hz looping timer while charging (`InfusionChargeManager.cpp:71-76`) and advances
  by a fixed `1.0f / 60.0f` (`:392`) instead of real elapsed time. Cheap; note for correctness under low fps.
- `ACharacter` ticks its movement component by default on every combat pawn even when standing still. Proposal:
  disable `CharacterMovement` tick on combat pawns while not moving (or set `bCanEverTick` false for enemies
  that never walk). Low.
- UI: no `NativeTick` in C++, widget pools are built once (`CombatCommandMenuWidget.cpp:41`,
  `TurnOrderStripWidget.cpp:107-121`, `CharacterPanelWidget.cpp:469-490`). **Blueprint property bindings can't be
  read from here (.uasset):** Crown should check the WBP_ panels for bound properties (they run every frame).

### P9 — Asset scans and hard references at fight start · LOW-MED · per fight / at load
- `UBossData::FindFor` (`BossData.cpp:71-88`) lists **every** BossData asset and `GetAsset()`s (sync-loads) each
  one, per enemy, at fight start (`BossTriggers.cpp:139`, `FightTriggers.cpp:120`, `MemoryUnlock.cpp:71`). Each
  BossData hard-references its boss's `UCharacterData` and `AddedSkills` (montages, VFX). Same pattern for
  `ReactionTriggers::GatherTriggers` (`ReactionTrigger.cpp:67`, from `BattleGameMode.cpp:273`) and story data.
  Fine with 3 bosses; grows with content. **Fix:** match on the `FAssetData` first (an `AssetRegistrySearchable`
  boss path tag) and load only the match, or build the map once per session. S.
- Existing hard refs (`SpellData.h:63-81` VFX + cast montage, `WeaponData.h:75-110` meshes + montages,
  `SkillDataBase.h:306-321` montages) stay per D-057. They mean that everything in the party's inventory and
  saved loadouts is resident. Fine for now; `memreport` (below) tells when it isn't.
- Battle level load: `BattleGameMode.cpp:68-70, 162`, `CombatHUD.cpp:50-169` sync-load classes. They happen during
  travel, so they're acceptable. Move them to async only if the load shows up on the travel screen.
- Niagara spawns use `ENCPoolMethod::None` (`ActionExecutorMontage.cpp:555`, `ActionExecutor.cpp:3492`). Use
  `AutoRelease` for repeated hit/muzzle effects once B-140 holds the systems. S, low.

### P10 — Build and iteration time · MED for Crown's day-to-day · per compile · S-M
- `ActionExecutor.h` (1257 lines) includes `NiagaraFunctionLibrary.h`, `NiagaraComponent.h`, `SkillProjectile.h`,
  `DefenseSystem.h`, `DamageCalculator.h` and `LoadoutComponent.h` (`ActionExecutor.h:14-31`). 29 files include it.
- `LoadoutComponent.h` (828 lines, 83 includers) includes `CharacterDataComponent.h` (`:25`), which includes
  `Net/UnrealNetwork.h` (`CharacterDataComponent.h:7`, needed only in the .cpp). 102 files include it, so a touch
  to the char component rebuilds a large part of the module. `WoRAttributeSets.h` also includes
  `AbilitySystemComponent.h` in a header.
- **Fix (one header per cluster):** forward-declare and move includes to the .cpp: Niagara + projectile + defense
  out of `ActionExecutor.h`; `CharacterDataComponent.h` out of `LoadoutComponent.h` (it only needs the class
  pointer); `UnrealNetwork.h` into `CharacterDataComponent.cpp`. Then add `IWYUSupport = IWYUSupport.Full` and
  `bUseUnity = false` for one local "adaptive" check build (catches the missing-include and anonymous-namespace
  clashes like the one B-212 hit; there are 202 anonymous namespaces, many with generic constant names).
- **Risk:** low (compile errors only, no behaviour change).

## Networking defaults to settle before B-228..B-235 (listen server, ≤3 players)

What replicates today: the GAS attribute sets: ~45 attributes, all `COND_None` + `REPNOTIFY_Always`
(`WoRAttributeSets.cpp:12`, `WoRAttributeSets.h:42-278`); the ASC in `Mixed` mode on every `ACombatCharacter`
(`CombatCharacter.cpp:39-41`); `UCharacterDataComponent` HP/EP/alive/BD (`CharacterDataComponent.h:128-164`,
B-131 makes it actually replicate); `UCurrencyComponent` (`CurrencyComponent.cpp:42`). No RPCs, no
`NetUpdateFrequency`, no push model, no dormancy yet. Proposed defaults:
1. **ASC mode:** `Mixed` for player-controlled pawns, `Minimal` for AI enemies (the comment at
   `CombatCharacter.cpp:37-38` already plans this). Set it at possession, not in the constructor.
2. **Update rate:** turn-based state changes rarely. Combat pawns: `SetNetUpdateFrequency(10)`,
   `SetMinNetUpdateFrequency(2)`, and `ForceNetUpdate()` after an action resolves. GameState turn component (B-228): same.
3. **Push model:** `NetCore` is already a dependency in Build.cs. Set `bWithPushModel = true` in both Target.cs files and
   `net.IsPushModelEnabled=1` in DefaultEngine.ini, then use
   `DOREPLIFETIME_WITH_PARAMS_FAST` + `MARK_PROPERTY_DIRTY_FROM_NAME` on the new GameState/PlayerState/turn and effect
   components (B-228/B-230). That stops the server comparing ~45 attributes × ~10 actors every net tick. The GAS
   attribute sets can stay as they are.
4. **Attributes:** keep Health/Energy/Buildup (+ Max) `COND_None` (everyone's HUD needs them). Derived stats are
   only for the inspect/stat screen. Consider `COND_OwnerOnly` for them if `stat net` shows them.
5. **Don't replicate the inventory/loadout arrays wholesale** (`InventoryComponent::SavedLoadouts`,
   `PoolSubsystem.h:292-305`). They're big nested arrays. Server-authoritative, with the owning client getting
   the active loadout summary `COND_OwnerOnly`, or a client RPC when the loadout screen opens.
6. **RPC volume:** Server RPCs reliable only for commands (menu pick, guard/dodge, infusion commit, B-229).
   Cosmetics (VFX, damage numbers, camera cues, B-231) go as **unreliable multicast**, one per hit, not per notify.
   Montages through the usual montage replication.
7. **Dormancy:** hub/field actors that rarely change (`AEnemyZone`, `AMerchantInteractable`, `AFieldEnemyTeam`,
   encounter arenas) `DORM_Initial` / `DORM_DormantAll`, `FlushNetDormancy()` on change.
8. **ReplicationGraph / Iris:** not needed for ≤3 players and ~15 actors. Revisit only if `stat net` says so.
9. **Before B-230:** P2 matters more in co-op, because every redundant `SyncAttributes` that changes a value
   also produces replication traffic.

## Profiling steps for Crown (the D-068 milestone check; B-142 is the first one)

Run in a **Development** build (standalone, not PIE) at 1080p on the desktop, on the demo fight
(2 heroes vs 3-4 enemies, with an AoE spell and a boss phase if possible). Save results under `docs/analysis/`.
1. **Baseline frame:** `stat unit` + `stat fps`. Note Frame / Game / Draw / GPU while idle in combat, during a
   player cast, and on an enemy turn. D-068 passes if Frame ≤ 16.6 ms. Look for spikes, not just the average.
2. **Unreal Insights trace (B-142):** launch with
   `-trace=default,cpu,frame,loadtime,assetloadtime,memory,net -statnamedevents`; or in-game
   `Trace.Start default,cpu,loadtime,assetloadtime`. Play one full fight, `Trace.Stop`. In Timing Insights,
   sort the game-thread spikes: expect `AIDecisionManager` (P3), `SyncAttributes` / `GetActiveStatBonus` (P1/P2),
   `LoadSynchronous` / `FlushAsyncLoading` on first casts and Perfect Guard (P4). Adding
   `TRACE_CPUPROFILER_EVENT_SCOPE(WoR_SyncAttributes)` etc. to those five functions first makes this
   conclusive (S, one cluster).
3. **Game thread detail:** `stat game`, `stat anim` (anim update cost per pawn), `stat niagarasystems` on a big cast.
4. **Loads:** `stat levels` + Insights *Asset Loading* track. Also `-LogCmds="LogStreaming Verbose"` once, to
   list every sync load during the fight.
5. **Memory:** `memreport -full` in the hub and again mid-fight; `obj list class=NiagaraSystem`,
   `obj list class=AnimMontage`, `obj list class=SkeletalMesh` to see what the hard refs keep resident;
   `obj gc` then `obj list` again to check per-fight objects are released (no leak per fight).
6. **Logging cost:** compare `stat unit` on an enemy turn with `log LogTemp Warning` / `log LogWoREffects Warning`
   vs default (P7).
7. **Co-op (each B-228..B-235 milestone):** two-player listen-server standalone with `net PktLag=100`:
   `stat net` (Out bytes/sec, actor count, RPCs) and Insights *Networking Insights* (`-trace=net`). Target:
   idle combat ≈ 0 property sends; one action ≈ a handful of RPCs + attribute updates.

## Not issues (checked)
- No polling `GetAllActorsOfClass`. `TActorIterator` is used only in console commands or rare one-offs
  (`EncounterComponent.cpp:117` at encounter trigger; typed iterators in `EnemyZone`, `MerchantInteractable`).
- `GetSubsystem` is a map lookup and is not cached across PIE (good).
- Turn-order preview reads a cached belt (`TurnManager.cpp:553-565`, rebuilt only on change, `:812`).
- No `GEngine->AddOnScreenDebugMessage` in game paths; `UE_LOG ... Verbose` arguments aren't evaluated when off.
- Widgets are pooled and created once per fight (TransBuffer gotcha respected).
