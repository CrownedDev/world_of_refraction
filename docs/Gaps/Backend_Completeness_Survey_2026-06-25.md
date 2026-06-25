# Backend Completeness Survey — `world_of_refraction` module

**Date:** 2026-06-25
**Branch:** feature/worldstats-storage
**Scope:** Read-only survey of `Source/world_of_refraction/Public/` + `Private/`. No source modified.
**Rule applied:** Where code and design docs disagree, the code is trusted and the drift is flagged.

> Caveat carried through this report: all dynamic-multicast delegates are `BlueprintAssignable`. A "no C++ listener" finding does **not** prove the delegate is dead — it may be bound in a Blueprint (`.uasset`, LFS, not inspectable). These are flagged as *no C++ listener*, not *dead*.

---

## SECTION 1 — System Inventory

### 1a. GameInstanceSubsystems (no `UWorldSubsystem` subclasses exist in the module)

| Subsystem | Decl (file:line) | Responsibility | Registered / used | Caching |
|---|---|---|---|---|
| `UPoolSubsystem` | Pool/PoolSubsystem.h:55 | Object/actor + crystal/stone count pooling | `GetSubsystem<UPoolSubsystem>` per-use | OK |
| `UCombatCommandMenuSubsystem` | UI/Combat/CombatCommandMenuSubsystem.h:47 | Drives combat command-menu UI flow | CombatCommandMenuWidget.cpp:80 (weak-cached in widget) | OK |
| `UAIDecisionManager` | AI/AIDecisionManager.h:46 | AI turn decision logic | self + combat callers | **CACHES** `UDefenseSystem* DefenseSystemRef` (h:127, set .cpp:53, re-set :456) |
| `UTurnManager` | Combat/TurnManager.h:166 | Turn order / turn lifecycle | TurnOrderStripWidget.cpp:35 (weak); AIDecisionManager.cpp:1831 | OK |
| `UDamageCalculator` | Combat/Damage/DamageCalculator.h:167 | Damage computation pipeline | AIDecisionManager.cpp:640,726 | **CACHES** `mutable USkillEffectManager* CachedSkillEffectManager` (h:281, set .cpp:565) + `mutable UCombatGridSubsystem* CachedCombatGridSubsystem` (h:284, set .cpp:722) |
| `UDefenseSystem` | Combat/Defense/DefenseSystem.h:235 | Defense / block / mitigation resolution | AIDecisionManager.cpp:53 | OK (within itself) |
| `UCombatGridSubsystem` | Combat/Grid/CombatGridSubsystem.h:21 | Spatial combat grid / positioning | DamageCalculator.cpp:722 | OK |
| `UActionExecutor` | Combat/Actions/ActionExecutor.h:120 | Executes combat actions/skills | CombatOrchestrator.cpp (per-use); AIDecisionManager.cpp:45 | OK |
| `UWeatherStateManager` | Combat/Mechanics/WeatherStateManager.h:30 | Weather/element field state | self | OK |
| `UItemExecutor` | Inventory/ItemExecutor.h:95 | Executes item-use actions | self | OK |
| `UInfusionChargeManager` | Infusion/InfusionChargeManager.h:111 | Tracks infusion charges | `GetSubsystem<>` per-use | OK |
| `UStatusBuildupManager` | Skills/Effects/StatusBuildupManager.h:53 | Status-effect buildup accrual | AIDecisionManager.cpp:1929,1949 | OK |
| `UEconomyService` | Currency/EconomyService.h:29 | Currency/crystal economy ops over components | EconomyService.cpp:47,48,360 | OK |
| `UCrystalManager` | Equipment/Crystals/CrystalManager.h:27 | Crystal equip/management service | self | OK |
| `USkillEffectManager` | Skills/Effects/SkillEffectManager.h:68 | Applies/tracks skill effects | CharacterDataComponent.cpp:642,1230; DamageCalculator.cpp:565 | OK in itself (but cached by DamageCalculator + CharacterDataComponent) |
| `UWeaponManager` | Equipment/Weapons/WeaponManager.h:28 | Weapon equip/lookup service | `GetSubsystem<>` | OK |
| `URingManager` | Equipment/Rings/RingManager.h:26 | Ring equip/management service | self | OK |

### 1b. Manager-style `UActorComponent`s

| Component | Decl (file:line) | Responsibility | Instantiated / found | Caching |
|---|---|---|---|---|
| `UCharacterDataComponent` | Character/CharacterDataComponent.h:96 | Runtime HP/EP/stats + world-stat ops | `NewObject` in test actors (TurnManagerTestActor.cpp:393, CombatOrchestratorTestActor.cpp:594, SkillEffectManagerTestActor.cpp:903); `FindComponentByClass` TurnManagerTestActor.cpp:149 | **CACHES** `mutable USkillEffectManager* CachedSkillEffectManager` (h:616, set .cpp:1230) |
| `UInventoryComponent` | Inventory/InventoryComponent.h:53 | Per-actor item inventory | `FindComponentByClass` CharacterDataComponent.cpp:42 (BP-added) | OK |
| `ULoadoutComponent` | Loadout/LoadoutComponent.h:149 | Per-actor equipped loadout | `FindComponentByClass` CharacterDataComponent.cpp:43,512 (BP-added) | OK |
| `UCurrencyComponent` | Currency/CurrencyComponent.h:40 | Per-actor currency balance | `FindComponentByClass` EconomyService.cpp:48 (BP-added) | OK |
| `UCrystalInventoryComponent` | Equipment/Crystals/CrystalInventoryComponent.h:35 | Per-actor crystal inventory | `FindComponentByClass` EconomyService.cpp:47 (BP-added) | OK |
| `UEvolutionInventoryComponent` | Equipment/Crystals/EvolutionInventoryComponent.h:24 | Per-actor evolution-item inventory | `FindComponentByClass` EconomyService.cpp:360 (BP-added) | OK |
| `UBrokenDarknessManager` | Combat/Mechanics/BrokenDarknessManager.h:53 | Broken-Darkness mechanic state | `FindComponentByClass` FCombatCapabilities.cpp:90 (BP-added) | OK (finds orchestrator per-use) |
| `UInfusionVFXComponent` | Infusion/InfusionVFXComponent.h:26 | Drives infusion VFX | `FindComponentByClass` CombatCommandMenuSubsystem.cpp:1824 (BP-added) | OK |
| `UWeaponMeshComponent` | Equipment/Weapons/WeaponMeshComponent.h:15 | Spawns/attaches weapon meshes | **ORPHANED in C++** — referenced only in own .cpp (BP-added only) | OK |
| `UElementColorDebugComponent` | Infusion/ElementColorDebugComponent.h:28 | Debug element-color tint | **ORPHANED in C++** — own .cpp only (debug) | OK |

### 1c. Placed-actor coordinators

| Actor | Decl (file:line) | Responsibility | Instantiated / found |
|---|---|---|---|
| `ACombatOrchestrator` | Combat/CombatOrchestrator.h:122 | Top-level combat flow coordinator | Level-placed; test spawn CombatOrchestratorTestActor.cpp:553; runtime `GetActorOfClass` BrokenDarknessManager.cpp:1162 |
| `ACombatCameraManager` | Combat/Camera/CombatCameraManager.h:28 | Combat camera transitions | Level-placed; `GetAllActorsOfClass` CombatOrchestrator.cpp:2716 |
| `ASkillProjectile` | Combat/Projectile/SkillProjectile.h:56 | Combat projectile (VFX + hit) | `SpawnActor` ActionExecutor.cpp:3402 |
| `ATurnManagerTestActor` | Combat/TurnManagerTestActor.h:21 | Test harness | Level-placed test |
| `ASkillEffectManagerTestActor` | Skills/Effects/SkillEffectManagerTestActor.h:27 | Test harness | Level-placed test |
| `AHUDTestActor` | Testing/HUDTestActor.h:32 | HUD/UI test harness | Level-placed test |
| `ASkillProjectileTestActor` | Combat/Projectile/SkillProjectileTestActor.h:18 | Projectile test harness | Level-placed test |
| `ACombatOrchestratorTestActor` | Combat/CombatOrchestratorTestActor.h:15 | Orchestrator test harness | Level-placed test |

### Section 1 Flags
- **Subsystem-pointer caching (violates "never cache subsystem pointers across PIE"):** `AIDecisionManager::DefenseSystemRef` (h:127), `DamageCalculator::CachedSkillEffectManager` (h:281) + `CachedCombatGridSubsystem` (h:284), `CharacterDataComponent::CachedSkillEffectManager` (h:616). Lesser concern (TWeakObjectPtr, PIE-safe by design): TurnOrderStripWidget::CachedTurnManager (h:62), CombatCommandMenuWidget::CachedSubsystem (h:71), DefensePromptWidget::CachedDefenseSystem (h:54).
- **Orphaned in C++ (BP-added only, no C++ consumer):** `UWeaponMeshComponent`, `UElementColorDebugComponent`.
- **Instantiation pattern:** no `CreateDefaultSubobject<>` for any manager component — all gameplay components are Blueprint-added and resolved via `FindComponentByClass`. Only `UCharacterDataComponent` is `NewObject`-instantiated in C++, and only in test actors.

---

## SECTION 2 — Wiring Audit

### Full delegate table

| Delegate (instance) | Owner | Declared (file:line) | Broadcast (file:line) | C++ Listener (file:line) |
|---|---|---|---|---|
| `OnHPChanged` | UCharacterDataComponent | h:38 / inst :227 | .cpp:249,260,270,279,383,415,420 | CharacterPanelWidget.cpp:73; WeatherStateManager.cpp:137 |
| `OnEPChanged` | UCharacterDataComponent | h:39 / inst :230 | .cpp:250,289,315,341,353,425,473,481,492 | CharacterPanelWidget.cpp:74; BrokenDarknessManager.cpp:117 |
| `OnDied` | UCharacterDataComponent | h:40 / inst :233 | .cpp:392,433 | CharacterPanelWidget.cpp:75; TurnManager.cpp:107; CombatOrchestrator.cpp:187; WeatherStateManager.cpp:139,141 |
| `OnResurrected` | UCharacterDataComponent | h:41 / inst :236 | .cpp:414,431 | TurnManager.cpp:108 |
| `OnPanelHovered` | UCharacterPanelWidget | h:20 / inst :59 | .cpp:570,576 | **No C++ listener** |
| `OnPanelClicked` | UCharacterPanelWidget | h:21 / inst :62 | .cpp:581 | **No C++ listener** |
| `OnCommandMenuReady` | UCombatCommandMenuSubsystem | h:28 / inst :89 | .cpp (25 sites, 184…1856) | CombatCommandMenuWidget.cpp:77 |
| `OnCommandMenuClosed` | UCombatCommandMenuSubsystem | h:31 / inst :92 | .cpp:427 | CombatCommandMenuWidget.cpp:78 |
| `OnCurrencyChanged` | UCurrencyComponent | h:37 / inst :107 | .cpp:187 | **No C++ listener** |
| `OnItemUsed` | UItemExecutor | h:75 / inst :125 | .cpp:172 | **No C++ listener** |
| `OnGambleResult` | UItemExecutor | h:76 / inst :128 | .cpp:1008 | **No C++ listener** |
| `OnChargeStarted` | UInfusionChargeManager | h:81 / inst :227 | .cpp:80 | **No C++ listener** |
| `OnChargeLevelChanged` | UInfusionChargeManager | h:82 / inst :231 | .cpp:179 | InfusionVFXComponent.cpp:59 |
| `OnChargeComplete` | UInfusionChargeManager | h:83 / inst :235 | .cpp:115 | InfusionVFXComponent.cpp:60 |
| `OnChargeCancelled` | UInfusionChargeManager | h:84 / inst :239 | .cpp:149 | InfusionVFXComponent.cpp:61 |
| `OnLoadoutChanged` | ULoadoutComponent | h:46 / inst :658 | .cpp:107 | **No C++ listener** |
| `OnItemUsed` (Loadout) | ULoadoutComponent | h:47 / inst :662 | .cpp:1410 | **No C++ listener** |
| `OnValidationFailed` | ULoadoutComponent | h:48 / inst :666 | .cpp:462 | **No C++ listener** |
| `OnInventoryChanged` | UInventoryComponent | h:43 / inst :386 | .cpp:177 | **No C++ listener** |
| `OnActionStarted` | UActionExecutor | h:76 / inst :358 | .cpp:628,697,834 | **No C++ listener** (CombatCameraManager.cpp:78 COMMENTED OUT) |
| `OnActionCompleted` | UActionExecutor | h:79 / inst :365 | .cpp:752,2344 | **No C++ listener** (CombatCameraManager.cpp:79 COMMENTED OUT) |
| `OnAsyncActionCompleted` | UActionExecutor | inst :382 | .cpp:2343 | **No C++ listener** |
| `OnDamageDealt` | UActionExecutor | h:82 / inst :368 | .cpp:2963,3069 | SkillEffectManager.cpp:45 |
| `OnHealingDone` | UActionExecutor | h:85 / inst :371 | .cpp:6194 | **No C++ listener** |
| `OnTargetKilled` | UActionExecutor | h:88 / inst :374 | .cpp:2048 | **No C++ listener** |
| `OnActionDeferredArmed` | UActionExecutor | h:92 / inst :362 | .cpp:635 | CombatOrchestrator.cpp:86 |
| `OnImpactFrame` | UActionExecutor | h:97 / inst :378 | .cpp:5634 | **No C++ listener** |
| `AsyncActionCallback` (single-cast `FOnActionComplete`) | UActionExecutor | h:100 / inst :841 | Execute .cpp:5502 | CombatOrchestrator.cpp:451,514,1906,1976,2059,2134,2192,2270,2332,2406,2458,2510,2578 (handler `HandleAsyncActionCompleted` :533) |
| `OnTurnStarted` | UTurnManager | h:151 / inst :275 | .cpp:201 | CombatOrchestrator.cpp:954; TurnOrderStripWidget.cpp:48; CombatCameraManager.cpp:73 |
| `OnCombatEnded` | UTurnManager | h:152 / inst :278 | .cpp:128 | CombatOrchestrator.cpp:955 |
| `OnSpeedChanged` | UTurnManager | h:153 / inst :281 | .cpp:444 | **No C++ listener** |
| `OnStatusBuildupChanged` | UStatusBuildupManager | h:23 / inst :183 | .cpp:451,503,526,552,589 | CharacterPanelWidget.cpp:86 |
| `OnEffectApplied` | USkillEffectManager | h:22 / inst :362 | .cpp:126,257 | CharacterPanelWidget.cpp:80; CharacterDataComponent.cpp:86 |
| `OnEffectRemoved` | USkillEffectManager | h:25 / inst :366 | .cpp:612…1177 (12 sites) | CharacterPanelWidget.cpp:81; CharacterDataComponent.cpp:87 |
| `OnEffectTriggered` | USkillEffectManager | h:28 / inst :370 | .cpp:1072 | **No C++ listener** |
| `OnEffectStacksChanged` | USkillEffectManager | h:31 / inst :374 | .cpp:173 | **No C++ listener** |
| `OnEffectDurationChanged` | USkillEffectManager | h:34 / inst :378 | .cpp:216,1106 | CharacterPanelWidget.cpp:82 |
| `OnCombatStateChanged` | ACombatOrchestrator | h:91 / inst :221 | .cpp:946 | CombatOrchestratorTestActor.cpp:138 (test only) |
| `OnCombatResultReady` | ACombatOrchestrator | h:92 / inst :224 | .cpp:324,598,772 | CombatOrchestratorTestActor.cpp:207,259,310 (test only) |
| `OnActorTurnStarted` | ACombatOrchestrator | h:93 / inst :232 | .cpp:657 | **No C++ listener** |
| `OnActionRequested` | ACombatOrchestrator | h:94 / inst :236 | .cpp:1033 | CombatCommandMenuSubsystem.cpp:89 |
| `OnActionExecuted` | ACombatOrchestrator | h:95 / inst :240 | .cpp:467,544 | CombatCommandMenuSubsystem.cpp:90; CombatOrchestratorTestActor.cpp:352 |
| `OnWorldStatDraftReady` | ACombatOrchestrator | h:100 / inst :229 | .cpp:778 | **No C++ listener** |
| `OnActionMontageEnded` | UCombatAnimInstance | h:62 / inst :65 | .cpp:232 | ActionExecutor.cpp:5078 |
| `OnActionNotify` | UCombatAnimInstance | h:68 / inst :71 | .cpp:30 | ActionExecutor.cpp:5241 |
| `OnCombatNotify` | UCombatAnimInstance | h:76 / inst :79 | CombatNotify.cpp:55 | ActionExecutor.cpp:5341 |
| `OnSkillImpact` | ASkillProjectile | h:25 / inst :91 | .cpp:395 | ActionExecutor.cpp:3441; SkillProjectileTestActor.cpp:75,147 |
| `OnSkillDodged` | ASkillProjectile | h:34 / inst :95 | .cpp:387 | ActionExecutor.cpp:3442; SkillProjectileTestActor.cpp:76,148 |
| `OnTransformed` | UBrokenDarknessManager | h:35 / inst :340 | .cpp:380 | CharacterPanelWidget.cpp:97 |
| `OnReverted` | UBrokenDarknessManager | inst :346 | .cpp:415 | **No C++ listener** |
| `OnEnergyAbsorbed` | UBrokenDarknessManager | h:36 / inst :349 | .cpp:1013 | CharacterPanelWidget.cpp:93 |
| `OnOverloadStateChanged` | UBrokenDarknessManager | h:37 / inst :352 | .cpp:668,688 | CharacterPanelWidget.cpp:94 |
| `OnStacksChanged` | UBrokenDarknessManager | h:38 / inst :355 | .cpp:866,884 | CharacterPanelWidget.cpp:95 |
| `OnAlignmentChanged` | UBrokenDarknessManager | h:39 / inst :358 | .cpp:554,883 | CharacterPanelWidget.cpp:96; CombatCommandMenuSubsystem.cpp:517 |
| `OnOverloadDamage` | UBrokenDarknessManager | h:40 / inst :361 | .cpp:508,716,726 | **No C++ listener** |
| `OnWeatherChanged` | UWeatherStateManager | h:24 / inst :53 | .cpp:161 | **No C++ listener** |
| `OnDefenseWindowOpened` | UDefenseSystem | h:189 / inst :393 | .cpp:97 | **No C++ listener** |
| `OnDefenseWindowClosed` | UDefenseSystem | h:192 / inst :396 | .cpp:159 | ActionExecutor.cpp:2608 |
| `OnDefenseInputReceived` | UDefenseSystem | h:195 / inst :399 | .cpp:228 | **No C++ listener** |
| `OnDefenseCueTriggered` | UDefenseSystem | h:198 / inst :402 | **NEVER BROADCAST** | **No C++ listener** |
| `OnParryReflect` | UDefenseSystem | h:201 / inst :405 | .cpp:154,391 | **No C++ listener** |
| `OnDefenseResolved` | UDefenseSystem | h:207 / inst :408 | ActionExecutor.cpp:1939 (cross-broadcast) | SkillEffectManager.cpp:56 |
| `OnDefensePerfect` | UDefenseSystem | h:210 / inst :411 | ActionExecutor.cpp:1942 (cross-broadcast) | **No C++ listener** |
| `OnCrystalBroken` | UCrystalManager | h:92 / inst :103 | .cpp:179 | DurabilityHeaderWidget.cpp:293; RingManager.cpp:23 |
| `OnCrystalDurabilityChanged` | UCrystalManager | h:105 / inst :115 | .cpp:156 | DurabilityHeaderWidget.cpp:292 |
| `FOnPieMenuButtonSelected` (type only) | — | PieMenuButtonData.h:259 | **NEVER (no instance)** | **NONE** |
| `FOnPieMenuStateChanged` (type only) | — | PieMenuButtonData.h:264 | **NEVER (no instance)** | **NONE** |

### Flag 1 — DECLARED BUT NEVER BROADCAST
- `OnDefenseCueTriggered` (DefenseSystem.h:198 / inst :402) — never broadcast, no listener.
- `FOnPieMenuButtonSelected` (PieMenuButtonData.h:259) — type declared, no instance, never broadcast.
- `FOnPieMenuStateChanged` (PieMenuButtonData.h:264) — type declared, no instance, never broadcast.
- **`TurnManager::OnTurnEnded` — DENIED / does not exist.** No `OnTurnEnded` delegate anywhere. TurnManager declares only `OnTurnStarted` (h:151), `OnCombatEnded` (h:152), `OnSpeedChanged` (h:153) — all three are broadcast (.cpp:201, 128, 444).

### Flag 2 — BOUND TO DEAD/EMPTY HANDLER
- **None found.** Every `AddDynamic`/`BindUObject`/`CreateUObject` handler inspected contains real logic (TurnManager::OnActorDied .cpp:450 + OnActorResurrected :457; CombatOrchestrator::OnCombatantDied :811, HandleTurnStarted :618, HandleActionDeferredArmed :662, HandleCombatEnded :749; CombatCommandMenuSubsystem handlers :113/:136/:531; TurnOrderStripWidget::HandleTurnStarted :101; CharacterPanelWidget::HandleBDAlignmentChanged :243; CombatCameraManager::OnTurnStarted :427; SkillEffectManager handlers :1647/:1718; CharacterDataComponent::HandlePoolEffectChanged :112).
- **An empty `HandleTurnEnded` — DENIED / does not exist.** No handler of that name exists.

### Flag 3 — BROADCAST WITH ZERO C++ LISTENERS
(All `BlueprintAssignable` — may be BP-bound.) `OnPanelHovered`, `OnPanelClicked`, `OnCurrencyChanged`, `OnItemUsed` (ItemExecutor), `OnGambleResult`, `OnChargeStarted`, `OnLoadoutChanged`, `OnItemUsed` (Loadout), `OnValidationFailed`, `OnInventoryChanged`, `OnActionStarted`, `OnActionCompleted`, `OnAsyncActionCompleted`, `OnHealingDone`, `OnTargetKilled`, `OnImpactFrame`, `OnSpeedChanged`, `OnEffectTriggered`, `OnEffectStacksChanged`, `OnActorTurnStarted`, `OnWorldStatDraftReady`, `OnReverted`, `OnOverloadDamage`, `OnWeatherChanged`, `OnDefenseWindowOpened`, `OnDefenseInputReceived`, `OnParryReflect`, `OnDefensePerfect`. (See table for broadcast file:line of each.)

### Section 2 notable wiring observations
- **Camera is unwired in C++:** the only bindings of `OnActionStarted`/`OnActionCompleted` are **commented out** at CombatCameraManager.cpp:78–79. Camera transitions to Action phase are not driven by action events (corroborated by Section 3 TODOs CombatCameraManager.cpp:77,466).
- **`OnWorldStatDraftReady`** (broadcast CombatOrchestrator.cpp:778) has no C++ listener — the 5-pick-3 world-stat draft UI is not wired (see Section 3 `DebugApplyPendingWorldStats`).
- **Cross-class broadcast pattern:** `UDefenseSystem::OnDefenseResolved`/`OnDefensePerfect` are broadcast externally by `UActionExecutor` (ActionExecutor.cpp:1939,1942), not by their owner.
- **Production-listener-only-in-tests:** `OnCombatStateChanged`, `OnCombatResultReady` bound only in CombatOrchestratorTestActor.

---

## SECTION 3 — Stubs, TODOs, Unfinished Paths

`ensureMsgf` / `checkNoEntry` / `unimplemented()` stub guards: **zero matches** module-wide. No `FIXME`/`HACK` markers — all are `TODO`/`placeholder`/`deprecated`.

### Fully-stubbed end-to-end paths
- **`DefensePromptWidget` (entire widget)** — DefensePromptWidget.cpp:7–63. `InitialiseForCombat` only sets `bBound=true` + logs "stub"; `HandleDefenseWindowOpened` (54–58) and `HandleDefenseWindowClosed` (60–63) are empty (comment-only "TODO Phase 1"). No delegate binding, no UI shown.
- **CombatPlayerController infusion input** — CombatPlayerController.cpp:130–134 + h:48–70. `OnInfusion`/`OnInfusionLevel` commented out; bindings stubbed (.cpp:64). Header: "INFUSION INPUT — PLACEHOLDERS (not implemented)".
- **`ESkillEffectType::RandomSkill`** — SkillEffectManager.cpp:1405–1408 logs "implementation pending" and breaks.

### TODOs in live systems (deferred behavior)
- ActionExecutor.cpp:2799–2801 — `ESpellSource::Item` spell items **not consumed** (TODO).
- ActionExecutor.cpp:2795–2796 — `ESpellSource::Evolution` post-cast effects empty (TODO Phase 6).
- ActionExecutor.cpp:1280 — defense window duration hardcoded `0.3f` (TODO from spell data).
- ActionExecutor.cpp:460 — Item channel tier deferred.
- ActionExecutor.cpp:1477, 3221, 3994, 5286, 5879, 6013, 6750 — ability override; SpellSpeed/ActionSpeed COMBAT effect not wired; hand-socket location; no authored world VFX point; ability ally-selection; Crystal `StatBonus` persistent equip bonuses not applied.
- ActionExecutor.h:242–243 — WeaponCrystal (Phase 4d), Evolution (Phase 6) deferred.
- AIDecisionManager.cpp:803, 857 — charge `StatusMultiplier` omitted from buildup scoring; .cpp:1958 — BuildupAmount uses raw attacker value.
- LoadoutComponent.cpp:1867 — auto-population only assigns weapon[0] (TODO smarter).
- ItemExecutor.cpp:738 — S-rank stat reveal deferred to UI pass.
- SkillEffectManager.cpp:2417 — DOT %-of-MaxHP interpretation deferred.
- **Shop-roll quality placeholder (recurring):** hardcoded `C_Quality` at EconomyService.cpp:655–659, FRingInventoryEntry.cpp:54, FWeaponInventoryEntry.cpp:47.
- CombatCameraManager.cpp:77, 466 — bind to ActionExecutor events / Action-camera transition (TODO).
- CombatCommandMenuSubsystem.cpp:1100 — Double-target picker treated as Single (TODO).
- FWeaponLoadoutEntry.cpp:27 — partial ability-locking not in WeaponData.
- CurrencyComponent.h:21–23,132,136 — Prisms/Diamond account-scope routing pending PlayerState.
- CombatConstants.h:319 — LUCK_DODGE_MAX / LUCK_DROP_CHANCE_MAX / LUCK_DROP_QUALITY_MAX unresolved.

### Empty/inert bodies (resolve, but do nothing)
- DefensePromptWidget.cpp:54–58 / 60–63 (empty).
- CombatPlayerController.cpp:133–134 (commented out).
- SkillEffectManager.cpp:1416–1430 — 10 passive-layer effect types fall through to one `UE_LOG(Verbose)` + break. **By design** (queried via `GetTotalStatModifier`, apply-tick is intentionally no-op).

### Functions that early-return without doing their stated job
- `ACombatOrchestrator::DebugApplyPendingWorldStats` — CombatOrchestrator.cpp:837–902 — "DEBUG-ONLY placeholder" dumping pending pool into Mind; real 5-pick-3 draft (`OnWorldStatDraftReady`) deferred.
- `UActionExecutor::ExecuteAction` (sync) for Spell/Ability — ActionExecutor.cpp:736–743 — returns `false` with "must use ExecuteActionAsync" (intentional Phase D guard).

### Duplicate / parallel entry points
| Pair | Locations | Live? |
|---|---|---|
| `ExecuteAction` (sync) vs `ExecuteActionAsync` | h:152/155; cpp:760/~700 | **Async is canonical.** Sync handles only Item/Defend; Spell/Ability/Attack hard-fail (cpp:736–743). All combat callers use async (CombatOrchestrator.cpp:450,513,1905…2577). Sync called once for instant-action branch (CombatOrchestrator.cpp:457). |
| `SubmitAction` vs `SubmitActionAsync` | h:156/163; cpp:359/475 | **Both live.** `SubmitAction` = AI/test/deferred path (AIDecisionManager.cpp:144; CombatOrchestrator.cpp:741,1843; 8× test). `SubmitActionAsync` = player-UI path (CombatCommandMenuSubsystem.cpp:1808 only). `SubmitAction` branches to async internally when `bRequiresAsync` (cpp:442–453). |
| `AbilityData` deprecated damage forwarders | AbilityData.h:77–86 (`DeprecatedFunction`) | Retained BP wrappers around `CalculateDamage`. |

### Drift flag (doc/comment vs code)
- `FFusionAttachment.h:7` + AugmentStoneConstants.h:39 call fusion durability "stubbed", **but** `ItemIdentity::GetFusionBonusDurability` (ItemIdentity.h:170–175) is fully implemented (`((TVA+TVB)*2)+1`). Comments are **stale**; path is live.

---

## SECTION 4 — Multiplayer Readiness Gap Audit

### 4.1 RPC layer — **ABSENT (confirmed)**
Grep for `UFUNCTION(...Server|Client|NetMulticast|Reliable|Unreliable...)` = **0 matches**. Functions named `Server*` (e.g. `UCharacterDataComponent::ServerTakeDamage`) are **naming convention only** — plain in-process C++ methods, not RPCs. No client→server command transport exists.

### 4.2 Authority gating — **PARTIAL**
**Gated** (check `HasServerAuthority()`/`HasAuthority()`):
- CharacterDataComponent: `ServerTakeDamage` (.cpp:254), `ServerHeal` (:264), `ServerSetHP` (:273), `ServerSpendEnergy` (:283), `ServerGainEnergy` (:292), `ServerSetEP` (:318), `ServerGainBrokenDarknessEnergy` (:344), `ServerResurrect` (:407), `ServerSetBrokenDarkness` (:458), `ResetToMax` (:239); helper `HasServerAuthority` :396.
- CurrencyComponent: `Add` (:65), `Spend` (:110); helper :231.
- EconomyService: 20 ops gated via `!Owner->HasAuthority()` early-out (.cpp:38,100,253,305,353,411,451,493,536,628,770,808,844,884,922,957,994,1030,1066,1102).
- InventoryComponent: 8 attach/detach ops gated (.cpp:505,562,617,658,820,866,917,996).

**UNGATED state mutators (flagged):**
- `CharacterDataComponent::AddEarnedWorldStat` (.cpp:200–213) — world-stat pool, no auth check; called from CombatOrchestrator.cpp:847.
- `CharacterDataComponent::SetHeadStartAllocation` (:215–230) — persistent SaveGame field, ungated.
- `CharacterDataComponent::IncrementPurchaseCount` (:232–235) — vendor ramp, ungated.
- `CharacterDataComponent::SeedWorldStats` (:182) / `ResetRunWorldStats` (:194) — ungated.
- `CrystalInventoryComponent::AddCount` / `RemoveCount` / `ClearAll` (h:63,69,93) — **no authority check anywhere in the component.**

### 4.3 Replication setup — **PARTIAL**
| Component | SetIsReplicatedByDefault | GetLifetimeReplicatedProps |
|---|---|---|
| `UCurrencyComponent` | **YES** (.cpp:41) | .cpp:49–59 |
| `UCharacterDataComponent` | **NO** (ctor .cpp:18–27 never calls it) | declares props .cpp:124–132 — **but replication DORMANT/inert because component not flagged replicated** |
| `UInventoryComponent` | **NO** | none |
| `UCrystalInventoryComponent` | **NO** | none |

Replicated UPROPERTYs: CurrencyComponent Gold (h:129, OnRep), Prisms/Diamond/GearEssence/SkillEssence (h:133,137,141,145), EssenceWallet FastArray (h:150) — DOREPLIFETIME .cpp:53–58. CharacterDataComponent CurrentHP/CurrentEP/bIsAlive (h:125,128,137) + bIsBrokenDarkness (h:155) declare ReplicatedUsing/DOREPLIFETIME .cpp:128–131 — **inert** (component not replicated).

SaveGame-tagged (intent only, no save system): CharacterDataComponent bIsBrokenDarkness (h:155), HeadStartMind/Body/Spirit (h:519/522/525); `LiveWorld*` + `WorldStatPurchaseCount` explicitly NOT SaveGame (h:507–514,531). CurrencyComponent Prisms/Diamond/GearEssence/SkillEssence/EssenceWallet (Gold NOT, CurrencyTypes.h:25). PoolSubsystem (h:175–228, dormant). CrystalInventoryComponent Crystals/Stones (h:50,55). Plus InventoryData, FEvolution*, FCrystalAttachment, FRuntimeAttachedItem, FFusionAttachment, FSavedLoadout, FAbilityCollection, FSpellCollection, FRing/FWeaponInventoryEntry.

### 4.4 Replication-hostile data shapes (`TMap` count pools — need FastArray)
| TMap | File:line | Replicated? |
|---|---|---|
| `CrystalInventoryComponent::Crystals` | h:51 | No |
| `CrystalInventoryComponent::Stones` | h:56 | No |
| `InventoryData::CrystalStock` | InventoryData.h:76 | No (asset) |
| `InventoryData::ItemCrystals`/`RefinedCrystals` (deprecated) | InventoryData.h:82,85 | No |
| `PoolSubsystem::Crystals` / `Stones` | PoolSubsystem.h:196,199 | No (subsystem) |

The code already knows this: CurrencyTypes.h:3–5 chose FastArray over `TMap<Enum,int32>` deliberately; InventoryComponent.cpp:1498 notes crystal-pool TMap parity. **World-stats are scalar int32 fields, not a TMap** (CharacterDataComponent.h:508–532) — no TMap hazard, but unreplicated. Runtime `TMap<AActor*,…>` (SkillEffectManager.h:419,429; StatusBuildupManager.h:201; DefenseSystem.h:459,462; CombatGridSubsystem.h:261; ActionStructs.h:266,430,492; CombatOrchestrator.h:490) are inherently non-replicable and live in subsystems.

### 4.5 Subsystem replication problem
**17 `UGameInstanceSubsystem`s** hold combat/economy logic+state that cannot replicate (each lives per-client independently). For server-authoritative combat, the authoritative ones must move to a replicated actor / GameState: TurnManager (h:166), ActionExecutor (h:120), DamageCalculator (h:167), DefenseSystem (h:235), AIDecisionManager (h:46), WeatherStateManager (h:30), CombatGridSubsystem (h:21), StatusBuildupManager (h:53), SkillEffectManager (h:68), InfusionChargeManager (h:111), PoolSubsystem (h:55), EconomyService (h:29), ItemExecutor (h:95), CrystalManager (h:27), WeaponManager (h:28), RingManager (h:26). CombatCommandMenuSubsystem (h:47) is local UI (likely client-only — fine).

### 4.6 Persistence — **ABSENT (confirmed)**
Grep for `USaveGame`/`SaveGameToSlot`/`CreateSaveGameObject` = **0 matches**. No `USaveGame` subclass, no save/load. All SaveGame tags dormant — confirmed in-code: CharacterDataComponent.h:152 ("no save system exists yet"), PoolSubsystem.h:23 ("Disk-save is a LATER phase. SaveGame tags here are DORMANT").

---

## SECTION 5 — Summary Scorecard

### ✅ COMPLETE & WIRED (done and connected in C++)
- **TurnManager** turn lifecycle — OnTurnStarted/OnCombatEnded broadcast and consumed by orchestrator + UI (TurnManager.cpp:201,128; CombatOrchestrator.cpp:954,955).
- **Async action pipeline** — ExecuteActionAsync ↔ orchestrator via single-cast `AsyncActionCallback` (13 call sites → HandleAsyncActionCompleted, CombatOrchestrator.cpp:533).
- **CharacterDataComponent HP/EP/death** — fully broadcast, consumed by panel/turn/orchestrator/weather.
- **SkillEffectManager** effect lifecycle — applied/removed/duration wired to panel + CharacterDataComponent.
- **StatusBuildupManager** → CharacterPanelWidget.
- **BrokenDarknessManager** — alignment/stacks/overload/energy all consumed by CharacterPanelWidget (+ menu subsystem for alignment).
- **Anim/notify chain** — CombatAnimInstance montage/notify events consumed by ActionExecutor (5078,5241,5341).
- **SkillProjectile** impact/dodge → ActionExecutor + tests.
- **CrystalManager** durability/break → DurabilityHeaderWidget + RingManager.
- **Command-menu flow** — OnActionRequested/OnActionExecuted ↔ CombatCommandMenuSubsystem; menu ready/closed ↔ widget.
- **CurrencyComponent** — the one fully replication-ready component (replicated + OnReps + FastArray wallet + authority-gated).

### ⚠️ BUILT BUT NOT WIRED (exists, nothing drives/listens in C++)
- **DefensePromptWidget** — entire widget stubbed; never binds DefenseSystem windows (DefensePromptWidget.cpp:7–63).
- **Combat camera Action-phase transitions** — bindings commented out (CombatCameraManager.cpp:78–79); TODOs at :77,466.
- **World-stat draft UI** — `OnWorldStatDraftReady` broadcast (CombatOrchestrator.cpp:778) with no listener; only `DebugApplyPendingWorldStats` placeholder exists.
- **CombatPlayerController infusion input** — handlers commented out (.cpp:130–134).
- **`ESkillEffectType::RandomSkill`** — implementation pending (SkillEffectManager.cpp:1405).
- **Spell-item consumption** — not implemented (ActionExecutor.cpp:2799).
- **Many broadcast-with-no-C++-listener delegates** (may be BP-bound): OnCurrencyChanged, OnInventoryChanged, OnLoadoutChanged, OnItemUsed×2, OnGambleResult, OnWeatherChanged, OnSpeedChanged, OnTargetKilled, OnHealingDone, OnImpactFrame, OnActionStarted/Completed, OnEffectTriggered/StacksChanged, OnParryReflect, OnDefenseWindowOpened, OnDefenseInputReceived, OnDefensePerfect, OnOverloadDamage, OnReverted, OnActorTurnStarted (see Section 2 Flag 3).
- **`OnDefenseCueTriggered`** — declared, never broadcast (DefenseSystem.h:198).
- **Orphaned components** — UWeaponMeshComponent, UElementColorDebugComponent (C++-unreferenced).

### ❌ MISSING FOR MULTIPLAYER (ordered: authority → replication → RPC transport → persistence)
1. **Authority model** — combat/economy logic lives in 17 GameInstanceSubsystems that cannot replicate and have no server/client split. Server-authoritative combat requires relocating authoritative systems (TurnManager, ActionExecutor, DamageCalculator, DefenseSystem, SkillEffectManager, StatusBuildupManager, AIDecisionManager, EconomyService, PoolSubsystem) onto a replicated actor / GameState. Also close ungated mutators: AddEarnedWorldStat, SetHeadStartAllocation, IncrementPurchaseCount, SeedWorldStats/ResetRunWorldStats, CrystalInventoryComponent AddCount/RemoveCount/ClearAll.
2. **Replication enablement** — `UCharacterDataComponent` declares replicated props + OnReps but **never calls SetIsReplicatedByDefault** (ctor .cpp:18–27) → HP/EP/BD replication is inert; fix the flag. `UInventoryComponent` and `UCrystalInventoryComponent` have no replication at all. (CurrencyComponent is the template to follow.)
3. **Replication-hostile data shapes** — convert count `TMap<Enum,int32>` pools to FastArraySerializer: CrystalInventoryComponent Crystals/Stones (h:51,56), InventoryData CrystalStock (h:76), PoolSubsystem Crystals/Stones (h:196,199). Pattern already exists in CurrencyTypes EssenceWallet.
4. **RPC transport** — zero `UFUNCTION(Server/Client/NetMulticast)` exist. Every client→server intent (submit action, spend currency, use item, end turn) needs a real Server RPC layer; current `Server*` names are in-process calls.
5. **Persistence** — no `USaveGame` subclass and no save/load anywhere. All SaveGame-tagged fields are dormant intent; a save system must be built to back them.

---

*End of survey. No fixes proposed — awaiting review.*
