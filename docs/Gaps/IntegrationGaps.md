# Integration Gaps — Catalog

**Date:** 2026-05-27
**Branch surveyed:** `main` (immediately post-merge of `feature/crystal-wear-substat-modifier`, commit `2cc8205`)

This catalog enumerates UE-side integration gaps: places where designed intent and current production wiring diverge. Scope: test-scaffolding standing in for real integration, unbound delegates, stubbed implementations, and UI options reachable in design but not in production. **Pure code-quality items, asset/content gaps, and performance concerns are out of scope.**

Methodology: grep sweep across `Source/world_of_refraction/**` for TODO / FIXME / Phase markers; cross-reference all `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*` declarations against `AddDynamic`/`AddUObject` subscribers and `.Broadcast(` sites; inspect `Category="Test"`/`"Debug"` UPROPERTY/UFUNCTION marked as doing real work.

---

## 1. Player input layer — `ACombatPlayerController` is test scaffolding

### 1.1 `ACombatPlayerController` builds player actions from test fields and bypasses orchestrator
- **What:** The combat PC fires actions from designer-assigned test properties straight into `UActionExecutor`, skipping `ACombatOrchestrator::SubmitAction` validation, the `bWaitingForAsyncAction` guard, win-condition checks, and the orchestrator's `OnActionExecuted` broadcast.
- **Where:** `Source/world_of_refraction/Public/CombatPlayerController.h:55-67`; `Source/world_of_refraction/Private/CombatPlayerController.cpp:160-238` (`OnConfirmAction`).
- **Evidence:** `UPROPERTY(... Category = "Test|Spell") TObjectPtr<USpellData> TestSpell;` (h:55), `Category = "Test|Combat") AActor *TestTarget` (h:59), `Category = "Test|Ability") TObjectPtr<UAbilityData> TestAbility` (h:62). `OnConfirmAction` calls `Executor->ExecuteActionAsync(ControlledActor, Action, ...)` directly (cpp:226).
- **Impact:** If `ACombatPlayerController` is the active PC class in production play, **two parallel player paths can submit actions** — the test PC bypasses turn-gating (a confirm press during another actor's turn submits anyway), and the orchestrator never sees the action so `OnActionExecuted` doesn't fire for the menu/UI subscribers. If it's NOT the active PC, then the entire `Test*`/`OnConfirmAction` surface is dead weight masquerading as the player input layer.
- **Priority:** High — confirm in PIE which PC the game mode uses, then either gut the test path or fence it behind a `bIsTestMode` flag.
- **Scope:** Medium — disambiguating, gating/deleting, and wiring a real PC that goes through the orchestrator is a focused session. A full proper PC implementation is Large.

### 1.2 Player defense input is BP-only — `DefenseSystem::SubmitDefenseInput` has no player-side C++ caller
- **What:** The only C++ caller of `SubmitDefenseInput` is the AI defender path; player block/parry/dodge presumably comes from BP (the "E blocks" pattern flagged in a prior session).
- **Where:** `Source/world_of_refraction/Public/DefenseSystem.h:205` (declaration, `BlueprintCallable`); `Source/world_of_refraction/Private/AIDecisionManager.cpp:388` (sole C++ caller).
- **Evidence:** `grep -rn "SubmitDefenseInput" Source/world_of_refraction/Private` returns only `AIDecisionManager.cpp:388`. No call from `CombatPlayerController` or any UI widget.
- **Impact:** Player defense input lives outside C++; gameplay logic is fragmented across the C++/BP boundary. Works today (assumed) but means input rebinding, controller support, or any defense-input change requires BP edits that aren't grep-discoverable.
- **Priority:** Medium — works today (presumed), but fragile.
- **Scope:** Small — move the binding onto whichever PC owns combat input.

---

## 2. Player defense UI — `UDefensePromptWidget` is a Phase 1 stub

### 2.1 DefensePromptWidget is fully stubbed; never instantiated outside test actors
- **What:** Every method body of `UDefensePromptWidget` is a `// TODO Phase 1` comment with no implementation; no production code spawns the widget.
- **Where:** `Source/world_of_refraction/Private/UI/Combat/DefensePromptWidget.cpp:15-62`.
- **Evidence (seven TODOs verbatim):**
  - `:15` `// TODO Phase 1: cache UDefenseSystem subsystem`
  - `:16` `// TODO Phase 1: bind OnDefenseWindowOpened -> HandleDefenseWindowOpened`
  - `:17` `// TODO Phase 1: bind OnDefenseWindowClosed -> HandleDefenseWindowClosed`
  - `:30` `// TODO Phase 1: unbind delegates using Get() not IsValid()`
  - `:56` `// TODO Phase 1: filter by local player controlled actor`
  - `:57` `// TODO Phase 1: ShowPrompt(Defender, WindowDuration, AttackSize)`
  - `:62` `// TODO Phase 1: HidePrompt(Result)`
  - `InitialiseForCombat` only logs `"Initialised (stub)"` (`:20`).
  - `grep` for any caller of `UDefensePromptWidget::InitialiseForCombat` returns zero non-test matches.
- **Impact:** Player has no on-screen defense UI. The defense window opens silently — only AI defenders react (via `AIDecisionManager.cpp:388`); players have no countdown, no button prompts, no result feedback through C++.
- **Priority:** High — pitch-blocker for any demo that includes a player defending.
- **Scope:** Small — implement the seven TODOs, spawn from `BP_CombatOrchestrator` HUD init.

### 2.2 `DefenseSystem::OnDefenseWindowOpened` has zero C++ subscribers
- **What:** Broadcast on every defense window open but nothing in C++ listens.
- **Where:** `Source/world_of_refraction/Private/DefenseSystem.cpp:90` (broadcast); `Source/world_of_refraction/Public/DefenseSystem.h:126` (declaration). No `AddDynamic` site anywhere.
- **Evidence:** `grep -rn "OnDefenseWindowOpened\.AddDynamic" Source/world_of_refraction` returns nothing.
- **Impact:** Same root as 2.1 — the intended consumer is `DefensePromptWidget` which is stubbed.
- **Priority:** High — collapses to the same fix as 2.1.
- **Scope:** N/A (covered by 2.1).

### 2.3 `DefenseSystem::OnDefenseInputReceived`, `OnParryReflect`, `OnDefenseCueTriggered` — broadcast/declared with no subscribers
- **What:** Three more defense-system delegates with zero subscribers.
- **Where:** `Public/DefenseSystem.h:132, 135, 138`; broadcasts at `Private/DefenseSystem.cpp:237` (`OnDefenseInputReceived`), `:172` (`OnParryReflect`). `OnDefenseCueTriggered` is **declared but never broadcast** — pure dead delegate.
- **Evidence:** `grep -rn "OnDefenseInputReceived\|OnParryReflect\|OnDefenseCueTriggered" --include="*.cpp"` shows broadcast-only references; no `AddDynamic`.
- **Impact:** No telemetry/SFX/UI on defense input echo (input feedback), no parry-reflect VFX/SFX trigger, no anticipatory defense-cue (the cue is presumably wind-up audio/anim before the window opens).
- **Priority:** Medium — important for game-feel polish, not gameplay blocking.
- **Scope:** Small per consumer; depends on whether designers want these surfaces.

### 2.4 `UActionExecutor::OnDefenseWindowRequested` — pure dead delegate
✅ **RESOLVED** on `feature/integration-gaps-sweep-1` — type declaration and field deleted from `ActionExecutor.h`; pre-merge grep reconfirmed zero references elsewhere in the module.

- **What:** Declared on ActionExecutor; never broadcast, never subscribed.
- **Where:** `Source/world_of_refraction/Public/ActionExecutor.h:71` (decl), `:334` (field). Zero broadcast sites, zero subscribers.
- **Evidence:** `grep -rn "OnDefenseWindowRequested" Source/world_of_refraction` finds only the two declaration sites.
- **Impact:** Cruft only. The live defense-open broadcast is `DefenseSystem::OnDefenseWindowOpened` (item 2.2).
- **Priority:** Low — delete.
- **Scope:** Small.

---

## 3. Combat lifecycle observability — broadcasts without consumers

### 3.1 `UActionExecutor` events `OnActionStarted` / `OnActionCompleted` / `OnHealingDone` / `OnTargetKilled` have no production C++ subscribers
- **What:** Four major action-event broadcasts go unconsumed. Camera bindings are present-but-commented-out.
- **Where:** Declarations `ActionExecutor.h:53, 56, 62, 65`. Broadcasts: `ActionExecutor.cpp:352, 481` (OnActionStarted), `:408, 1597` (OnActionCompleted), `:2222, 4122` (OnHealingDone), `:1397` (OnTargetKilled).
- **Evidence:** Camera bindings commented out: `CombatCameraManager.cpp:78` `// ActionExecutor->OnActionStarted.AddDynamic(this, &ACombatCameraManager::OnActionStarted);` and `:79` `// ActionExecutor->OnActionCompleted.AddDynamic(this, &ACombatCameraManager::OnActionCompleted);`. Plus `CombatCameraManager.cpp:77` `// TODO: Bind to ActionExecutor events when available`. The handler functions (`CombatCameraManager.cpp:464, 470`) exist as empty/log-only stubs.
- **Impact:** No camera transitions on action start/end (designer intent per the TODO+commented bindings); no kill-feed/log/SFX for kills; no healing-VFX feedback. `OnDamageDealt` IS bound (SkillEffectManager — drives lifesteal/burn/chill/stun), so the gap is specifically about *observability/presentation* events, not gameplay-effect events.
- **Priority:** Medium — gameplay works, but combat feels flat without camera/feedback.
- **Scope:** Medium — uncomment + implement camera handlers; add kill-feed/healing-VFX consumers per design.

### 3.2 `ACombatOrchestrator` events `OnCombatStateChanged` / `OnCombatResultReady` / `OnActorTurnStarted` have no production C++ subscribers
- **What:** Three orchestrator broadcasts whose only C++ consumers are in `Combatorchestratortestactor.cpp`.
- **Where:** `CombatOrchestrator.h:60, 61, 62`. Broadcasts at `CombatOrchestrator.cpp:633` (state), `:284, 538, 610` (result), `:581` (actor turn).
- **Evidence:** All `AddDynamic` to these delegates are in `Combatorchestratortestactor.cpp` (10+ sites). Production C++ has none. `OnActorTurnStarted` is **broadcast but zero subscribers anywhere** — likely redundant with `TurnManager::OnTurnStarted` (which IS bound by Camera, Orchestrator, TurnOrderStripWidget).
- **Impact:** No outcome screen (Victory/Defeat/Draw) wired in C++ — if `BP_CombatOrchestrator` doesn't listen, the player gets no round-end feedback. State-change broadcasts (e.g. transition to `Initializing` for setup overlay) similarly orphaned.
- **Priority:** Medium-High — confirm BP coverage; if absent, ship a result-overlay widget for the pitch.
- **Scope:** Medium for the result UI; Low to delete `OnActorTurnStarted` if redundant.

### 3.3 `UCharacterDataComponent::OnResurrected` — broadcast, no subscribers
- **What:** Revive intercept fires `OnResurrected` but nothing listens.
- **Where:** `CharacterDataComponent.h:17` (decl); broadcasts at `:272, 289`.
- **Evidence:** No `AddDynamic` to `OnResurrected` anywhere. Compare `OnDied` (`:250, 291`) which IS bound by CharacterPanelWidget and WeatherStateManager.
- **Impact:** No resurrection VFX/SFX/log; UI bar refreshes only because the Revive path also broadcasts `OnHPChanged` (`:273, 278`), not because anything binds `OnResurrected`.
- **Priority:** Low — only relevant if revive becomes prominent.
- **Scope:** Small.

---

## 4. BD overflow — visibility broadcasts + self-cost mechanics

### 4.1 `UBrokenDarknessManager::OnTransformed`, `OnOverloadDamage`, `OnStacksChanged` — broadcast, no production C++ subscribers
- **What:** Three BD lifecycle/combat broadcasts have no UI/SFX consumer in C++.
- **Where:** `BrokenDarknessManager.h:21, 24, 26`. Broadcasts at `BrokenDarknessManager.cpp:248` (Transformed), `:605, 623` (Stacks), `:301, 511, 521` (Overload damage).
- **Evidence:** `CharacterPanelWidget` binds `OnEnergyAbsorbed` (`:92`) and `OnOverloadStateChanged` (`:93`) but NOT the three above. `CombatCommandMenuSubsystem` binds `OnAlignmentChanged` (`:478`). No other BD subscribers.
- **Impact:** No on-screen "TRANSFORMED INTO BROKEN DARKNESS" stinger on the runtime-transform path (a major narrative beat fires silently). No stack-count indicator updates on absorption stack changes (the absorbed-element color updates via `OnAlignmentChanged`, but the **stack count** doesn't drive any UI). No "overload aura tick damaged enemy X" feedback during overload.
- **Priority:** Medium — runtime BD transformation is dramatic in design; firing it silently undersells it.
- **Scope:** Small per consumer; depends on designer priority.

### 4.2 BD forbidden-cast self-buildup unwired
- **What:** When BD casts a **forbidden element** (one they're attempting to absorb), they currently take self-DAMAGE but not self-status-BUILDUP. Both halves should fire together — two costs scaled by two different stats.
- **Where:** `UActionExecutor::ProcessForbiddenElementCast` (`ActionExecutor.cpp:3356`) → `UBrokenDarknessManager::ProcessForbiddenCast` (`BrokenDarknessManager.cpp:278+`, self-damage path at `:275, 290-293`). The dead `UActionExecutor::ApplySelfStatusBuildup` helper (`ActionExecutor.cpp:3419-3434`) was the intended apply hook for the missing buildup half.
- **Design (locked):**
  - **Self-damage** scales with **SpellDamage** (existing behaviour via `BrokenDarknessManager::CalculateForbiddenCastDamage` × `ForbiddenCastSelfDamagePercent`).
  - **Self-buildup** scales with **StatusMultiplier** (the missing piece — same stat the offensive buildup pipeline uses).
  - **Element built** = the forbidden element the BD is attempting to absorb (the spell's element on a forbidden cast).
  - Both apply together — two costs, two stats, two channels.
- **Impact:** Forbidden-cast risk is currently HP-only; the design's "status backlash" half doesn't fire. Casting a forbidden Fire spell as a BD should both burn the caster AND build Fire on their status bar (eventually triggering Burn on themselves).
- **Priority:** Medium — BD core risk/reward mechanic; partial implementation undersells the design.
- **Scope:** Small-Medium — one new buildup call inside the existing forbidden-cast path, scaled correctly off StatusMultiplier; route via `UStatusBuildupManager::AddStatusBuildup(Caster, Caster, Amount, ForbiddenElement, None)` so the standard pipeline (resistance, trigger) applies.

### 4.3 BD overload aura per-turn tick — status-buildup + absorption-drain coupling unwired
- **What:** When BD absorbs **past their absorption limit**, they enter overload — an unstable aura of the absorbed element that should, per turn while overloaded, release elemental energy. `OnOverloadDamage` already broadcasts (`BrokenDarknessManager.cpp:301, 511, 521`) so HP-damage may be partially wired; the **status-buildup release + absorption-drain coupling** is the missing core design.
- **Where:** `UBrokenDarknessManager` overload state machine + `OnOverloadDamage` broadcast path; `UStatusBuildupManager::AddStatusBuildup` for the status half. Likely tick site is the existing overload-state per-turn processing (audit before adding to avoid double-wiring HP damage).
- **Design (locked, key insight: status buildup IS released elemental energy — one flow, multiple faces):**
  - **Per turn, the aura releases elemental energy.** The amount released:
    - **Increases** with **StatusMultiplier** (more output → more release)
    - **Decreases** with **Efficiency** (better control → holds it together, less release)
  - That released energy simultaneously:
    - **Drains** the same amount from the BD's **absorption pool** (bleeding back toward limit)
    - Becomes **status buildup on the BD** in the aura's element, **mitigated by Resistance**
  - **Separately**, the aura deals **HP damage** scaled by **SpellDamage** (may already be partially wired via `OnOverloadDamage`).
  - **Element** = the absorbed (overloading) element — the BD's current alignment.
- **Per-turn tick formula:**
  ```
  released = f(StatusMultiplier↑, Efficiency↓)
  absorption_pool -= released
  status_buildup(self, aura_element) += released × (1 - Resistance)
  hp_damage(self) = g(SpellDamage)
  ```
- **Impact:** Overload's risk model is incomplete. Without status-buildup release, overload is purely an HP cost — designers can't tune the "elemental backlash" half. The absorption-drain coupling is what gives overload its self-recovery curve (BD bleeds back toward limit naturally over turns).
- **Priority:** Medium — BD core mechanic; absorption/overload loop is structurally incomplete without it.
- **Scope:** Medium — touches overload state machine + adds the buildup release. **Verify what `OnOverloadDamage` already does before implementing** so the HP-damage half isn't double-wired. The dead `UActionExecutor::ApplySelfStatusBuildup` helper is again the intended apply hook for the buildup half (or a direct `AddStatusBuildup` call from the BD manager — design choice for the implementing session).

### 4.x cross-reference — DON'T DELETE `ApplySelfStatusBuildup` / `ApplySelfDamage` yet

Sweep-4's `7.2` reframe note flagged `UActionExecutor::ApplySelfStatusBuildup` (`:3419-3434`) and `ApplySelfDamage` (`:3402-3417`) as structurally obsolete now that the `StatusIncrease` / `StatusDecrease` effect-type pipeline exists for authored effects. **However, gaps 4.2 and 4.3 are intrinsic BD mechanics (not authored effects)** and these dead helpers are the natural apply hooks for those mechanics. **Leave them in place until 4.2 + 4.3 are implemented (or a final apply path for intrinsic self-cost is chosen during that implementation).** If 4.2/4.3 end up routing through `UStatusBuildupManager::AddStatusBuildup` directly (skipping the helpers), then the helpers can be deleted post-implementation.

---

## 5. Loadout / Item event surface — orphan delegates

### 5.1 `ULoadoutComponent::OnLoadoutChanged`, `OnLoadoutItemUsed`, `OnValidationFailed` — broadcast, no subscribers
- **What:** Three loadout-component delegates with no C++ consumers.
- **Where:** `LoadoutComponent.h:43, 44, 45`. Broadcasts at `LoadoutComponent.cpp:103, 743, 432`.
- **Evidence:** No `AddDynamic` to any of the three anywhere in the module.
- **Impact:** Loadout swap mid-combat triggers no UI refresh in C++ (BP-side might handle, unverifiable). Item-slot decrement on use triggers no inventory-UI update in C++. Validation failures during battle-prep log per error but no UI propagates them (the player can't see *why* their loadout was rejected).
- **Priority:** Medium for `OnValidationFailed` (UX cost when authoring goes wrong); Low for the other two if BP covers them.
- **Scope:** Small per consumer.

### 5.2 `UItemExecutor::OnItemUsed` + `OnGambleResult` — broadcast, no subscribers
- **What:** Two item-execution delegates with no C++ consumers.
- **Where:** `ItemExecutor.h:75, 76`. Broadcasts at `ItemExecutor.cpp:111, 547`.
- **Evidence:** No `AddDynamic` to either.
- **Impact:** No combat-log entry for item use ("Player used Garnet S on Enemy"). No special UI for Amethyst gamble outcomes (the gamble result is computed but never surfaced to a feedback channel).
- **Priority:** Medium for `OnItemUsed` (combat log is standard UX); Medium for `OnGambleResult` (gamble feedback is the whole point of Amethyst).
- **Scope:** Small per consumer.

---

## 6. Movement state machine — declared but never broadcast

### 6.1 `UCombatMovementComponent::OnApproachComplete`, `OnReturnComplete` — declared, never broadcast
✅ **RESOLVED** on `feature/integration-gaps-sweep-1` (option-b cleanup + rename): deleted dead `FOnReturnComplete` type and dead `OnReturnComplete` field; renamed `FOnApproachComplete` → `FOnMovementComplete` so the live working field's type matches its name; stale flow-comment at the old `:46` updated. Pre-edit grep reconfirmed `FOnApproachComplete` / `FOnReturnComplete` appeared only in `CombatMovementComponent.h` — clean rename, no .cpp changes needed.

- **What:** Two of the three declared movement delegates have no broadcast site. Production code uses a different `OnMovementComplete` delegate (which IS broadcast and bound — verified by ActionExecutor bindings at `:1550, 3663`).
- **Where:** `CombatMovementComponent.h:19, 22` (decls). No `OnApproachComplete.Broadcast` or `OnReturnComplete.Broadcast` anywhere.
- **Evidence:** `grep -n "OnApproachComplete\.Broadcast\|OnReturnComplete\.Broadcast" Source/world_of_refraction` returns nothing.
- **Impact:** Cruft only — the working API is `OnMovementComplete`. The two unused delegates suggest an earlier API design that was superseded but not removed.
- **Priority:** Low — cleanup.
- **Scope:** Small — delete.

### 6.2 `UCombatMovementComponent::OnMovementCancelled` — broadcast, no subscribers
- **What:** Cancel event fires but nothing reacts.
- **Where:** `CombatMovementComponent.h:25` (decl); `Private/CombatMovementComponent.cpp:303` (broadcast).
- **Impact:** A cancelled approach (e.g. target dies mid-flight) silently aborts — no "attack interrupted" feedback.
- **Priority:** Low.
- **Scope:** Small.

### 6.3 Approach/return cosmetic VFX TODOs unimplemented
- **What:** Movement component plays no vanish/appear FX during teleport-style movement.
- **Where:** `Private/CombatMovementComponent.cpp:159, 170`.
- **Evidence:** `:159` `// TODO: Play DepartureVFX, DepartureSound, MovementMontage (vanish)`; `:170` `// TODO: Play ArrivalVFX, ArrivalSound, ArrivalMontage (appear)`.
- **Impact:** Teleport-class moves snap instantly with no transition VFX.
- **Priority:** Low — cosmetic.
- **Scope:** Small once assets exist (asset gap, not catalogued here).

---

## 7. Status effect & buildup — stub branches and TODO-gated wiring

### 7.1 `USkillEffectManager` Phase 2 passive-layer effect handlers are switch-stubs
- **What:** A block of effect types has empty handler branches.
- **Where:** `Source/world_of_refraction/Private/SkillEffectManager.cpp:1112`.
- **Evidence:** `// Stub cases for the new passive-layer effect types. Each needs its own [body]`. Followed by no-op branches (verified by file inspection in this session's prior survey).
- **Impact:** Passive-layer buff/debuff effects authored in data may apply but produce no runtime effect; specific subset depends on which `ESkillEffectType` values the stub covers.
- **Priority:** Medium — depends on whether any of the stubbed types are used by shipping crystals/spells.
- **Scope:** Medium — implement each handler.

### 7.2 ActionExecutor status-buildup self-application is a TODO
✅ **RESOLVED** on `feature/integration-gaps-sweep-4` (reframed). The original catalog entry conflated two distinct surfaces. Sweep-4 verification clarified:

- **Self-EFFECT application** (Burn-instance on caster, etc.) was **already supported** via `FSkillEffect::Target = ETargetType::Self` flowing through `UActionExecutor::GetEffectTargets` → `UActionExecutor::ApplySkillEffects` → `StatusMgr->ApplyEffect(User, ...)`. The original 7.2 framing missed `FSkillEffect::Target` (`FSkillEffect.h:64-65`).
- **Status-bar GAUGE manipulation** (caller's intent: "this spell builds N Fire on the target" or "this ability reduces N status on the caster") was the genuine gap. The dead helpers `ApplySelfStatusBuildup` / `ApplySelfDamage` on ActionExecutor + the magic-number TODOs at `:3430` / `:3532` / `:3535` were stubs for this surface but never wired.

Resolution: added two new effect types — **`StatusIncrease`** (debuff, builds the target's gauge) and **`StatusDecrease`** (buff, reduces it) — that flow through the existing effect system. They use `FSkillEffect::Target` for self-vs-other routing, the existing condition framework for triggers, and the resolved cast element (spell's `Element`, or the infused source's element for infused casts) for which status bar to manipulate. Specifically:

- Enum entries appended to `ESkillEffectType.h` under a new `STATUS BAR MANIPULATION` banner (preserves `.uasset` enum-by-value stamping).
- Classification: `StatusDecrease` added to `IsBuff()` in both `FSkillEffect.h` and `ActiveSkillEffect.h`; `StatusIncrease` added to `IsDebuff()` in both.
- New `UStatusBuildupManager::ReduceStatusBuildupByAmount(Target, Amount)` mirrors `AddStatusBuildup`'s shape but subtracts (the existing fraction-based `ReduceStatusBuildup` stays untouched — Quartz items still use it).
- Two new switch cases in `USkillEffectManager::ApplyEffectLogic`: StatusIncrease → `AddStatusBuildup(Source, Actor, Value, Effect.Element, None)`; StatusDecrease → `ReduceStatusBuildupByAmount(Actor, Value)`.
- Element threading: `UActionExecutor::ApplySkillEffects` gains an `ESpellElement ResolvedCastElement = Generic` parameter; the single caller at `:1527` computes it from the Action. Inside the loop, the new effect types use the resolved element while every other effect type keeps the previous `Generic` hardcode — **no behaviour change for DOT / ResistanceBuff / any pre-existing effect**.

The original dead helpers (`ApplySelfStatusBuildup`, `ApplySelfDamage`, magic-number TODOs at `:3430-3535`) remain in `ActionExecutor.cpp` for now. **They are NOT cleanup candidates** — gaps **4.2** (BD forbidden-cast self-buildup) and **4.3** (BD overload aura per-turn tick) are intrinsic BD self-cost mechanics for which these helpers are the natural apply hooks. See the **4.x cross-reference note** in section 4 before considering deletion.

#### Original framing (pre-sweep-4)
> Buildup-on-self code path is unimplemented. TODOs at `ActionExecutor.cpp:3430` `// TODO: Implement status buildup on self`, `:3535` `// TODO: Integrate with SkillEffectManager when API is available`, and the adjacent magic number `:3532` `int32 BaseBuildup = 10 * HitCount`. Framing assumed a new "self-buildup channel" was needed; sweep-4 verification showed the right answer was two effect types that flow through the existing effect pipeline.

### 7.3 `UCombatCameraManager` TODO `// Get target from action and transition to Action camera`
- **What:** Action camera transition not wired.
- **Where:** `Private/CombatCameraManager.cpp:466`.
- **Evidence:** Quoted above; lives inside the `OnActionCompleted` handler which is itself unbound (item 3.1).
- **Impact:** Even if 3.1 is fixed, the action-camera transition is a no-op until this is implemented.
- **Priority:** Pair with 3.1 (Medium).
- **Scope:** Small.

---

## 8. UI lifecycle lives only in BP / test actors

### 8.1 No production C++ spawns HUD widgets — only `AHUDTestActor`
- **What:** `UCharacterPanelWidget`, `UTurnOrderStripWidget`, and (when implemented) `UDefensePromptWidget` are only instantiated by `AHUDTestActor`; production spawning must happen entirely in `BP_CombatOrchestrator`.
- **Where:** `Source/world_of_refraction/Private/Testing/HUDTestActor.cpp:121` (`Strip->InitialiseForCombat()`); `:43-58` (TargetActor wiring). Per `UISystem.md:22-34`, the old `UCombatHUDRoot` was deleted and never replaced in C++.
- **Evidence:** `grep -rn "InitialiseForCombat" Source/world_of_refraction` shows only `HUDTestActor.cpp` calling widget init outside of self-init sites.
- **Impact:** HUD lifecycle errors are unfindable by grep. Adding a new HUD widget (e.g. fixed `DefensePromptWidget`, future result-overlay) requires editing `BP_CombatOrchestrator` rather than the orchestrator's C++. Test-actor-only spawn means HUD reliability depends on whether a designer remembers to place an `AHUDTestActor` in the level, OR on BP wiring that isn't audited here.
- **Priority:** Medium — works today (presumed) but fragile; widening this gap blocks future C++-side HUD work.
- **Scope:** Medium — port spawn logic from `AHUDTestActor` into `ACombatOrchestrator::OnCombatStartedUI` (or its C++ pair).

### 8.2 `UCharacterPanelWidget::OnPanelHovered` / `OnPanelClicked` — broadcast, no C++ subscribers
- **What:** Panel hover/click signals fire but no C++ listens.
- **Where:** `Public/UI/Combat/CharacterPanelWidget.h:20, 21`; broadcasts at `:466, 472, 477`.
- **Impact:** Target-picking via panel hover (a common UX in turn-based combat) must be BP-mediated; no C++ targeting flow uses these signals.
- **Priority:** Low if BP covers; Medium if intended for AI/automated target-selection that hasn't materialized.
- **Scope:** Small if needed.

---

## 9. UI options designed but unreachable in production

### 9.1 Source-cycle UI is now Breakthrough-only; verify intent for other spell submenus
- **What:** This branch surfaced `CycleSource` for Breakthrough spells (Evolution / Innate), but Refractions / ResonateWeapon / ResonateRing remain element-locked. Need design confirmation: should any of these support source cycling later?
- **Where:** `Source/world_of_refraction/Private/UI/Combat/CombatCommandMenuSubsystem.cpp:1241-1247` (the `bShowCycleSource` predicate — currently Attack/Ability/Breakthrough only).
- **Evidence:** The predicate explicitly enumerates the three allowed categories; the inline comment at `:1241` reads `"Cycle Source — Attack / Ability always; Breakthrough spells too (so BD can pick Innate for Darkness-conversion). Other spell submenus stay element-locked."` — this is a design statement, not necessarily a permanent rule.
- **Impact:** If a designer later wants (e.g.) a Refraction spell to be infusable by a ring crystal, the UI silently won't allow it — same shape as the BD-Innate gap fixed this branch.
- **Priority:** Low (verify intent only — may not be a gap).
- **Scope:** Small if extended.

### 9.2 BD command menu shows only Breakthrough — InnateSpells loadout pool is empty
- **What:** BD's saved loadout has no `InnateSpells` (Darkness pool) populated, so the Refractions submenu would have nothing to show even if reachable.
- **Where:** Authoring-side; `Content/Data/Characters/Data/Caster/BrokenDarkness/Inventory/DA_Inventory_BD.uasset` (LFS, not readable here). Code path `FCombatLoadout.cpp:499` populates the runtime pool from `FSavedLoadout::InnateSpells`.
- **Evidence:** This branch's BD wiring relied on Breakthrough as the only BD cast surface; prior-session note from menu testing.
- **Impact:** A pitch demo featuring BD shows only "Breakthrough" in the cast menu — no general Darkness Refractions. Acceptable as a slice if intentional; misleading if designers think a Refractions submenu would appear.
- **Priority:** Medium if BD is in pitch; Low otherwise.
- **Scope:** Small (designer authors at least one Darkness spell into the asset).

---

## 10. Small-but-real TODOs on the live combat path

These are not full gaps but are TODO-marked compromises that may affect demo correctness.

### 10.1 AI spell source defaults to Innate without resolving
✅ **RESOLVED** on `feature/integration-gaps-sweep-2`. New helper `ULoadoutComponent::ResolveSpellSource(USpellData*)` walks the active loadout's spell lists in locked precedence order (**Innate → RingCrystal → WeaponCrystal → Evolution**), first match wins. Semantically mirrors the player path's `MapCategoryToSpellSource` (Refractions→Innate, ResonateRing→RingCrystal, ResonateWeapon→WeaponCrystal, Breakthrough→Evolution); BD per-element pools resolve as Innate (still Refraction-source, just filtered by absorption). Default fallback: Innate with a warning when the spell isn't in any loadout source (data inconsistency).

All 6 AI sites (the survey said 5, actual count is 6) updated:
- `AIDecisionManager.cpp:691` (EstimateSpellDamage probe — modifier walk)
- `AIDecisionManager.cpp:862` (CanAffordSpell probe — cost gate)
- `AIDecisionManager.cpp:1008/:1021` (TrySurvivalBranch heal probe + OutAction — same spell, source resolved once, both use it)
- `AIDecisionManager.cpp:1095/:1107` (TryCleanseBranch cleanse probe + OutAction — same pattern)
- `AIDecisionManager.cpp:1417` (main spell-decision OutAction)

Sites :691 and :862 resolve `ULoadoutComponent` from `Actor->FindComponentByClass<>` (loadout not in scope); the four TrySurvival/TryCleanse/main-decision sites use the in-scope `Loadout` parameter. All sites fall back to Innate if loadout lookup fails, preserving prior behaviour on misconfigured actors.

- **Where:** `Private/AIDecisionManager.cpp:1417` `Action.SpellSource = ESpellSource::Innate; // TODO: Determine actual source`.
- **Impact:** AI casts get the wrong `ESpellSource` for ring/weapon/evolution crystals, meaning the EP-vs-wear split (this branch's wear modifier) doesn't apply correctly to AI casts.
- **Priority:** Medium — directly affects this branch's gameplay model.
- **Scope:** Small.

### 10.2 Attack tier/base size hardcoded
✅ **RESOLVED** on `feature/integration-gaps-sweep-1` — `:317` reads `Attack->BaseEnergyCost` (existing field on `UCastableSkillDataBase`; warns when 0 for an infused attack). `:1025` reads `Attack->BaseSize`, a new field added to `UWeaponAttackData` with default `0.0f` and a runtime warning when unauthored. No fallback constants — warnings surface the authoring gap. Original gap doc said "tier" for :317; verified the call site computes energy cost, not tier (tier already exists on the parent class).

- **Where:** `Private/ActionExecutor.cpp:317` `return 5; // TODO: Get from constants or attack data`. `:1025` `float AttackSize = 1.5f; // TODO: get from attack data`.
- **Impact:** Attack-data-driven sizing/tier ignored; design changes to attack data won't propagate.
- **Priority:** Low.
- **Scope:** Small.

### 10.3 `ESpellSource::Item` case is a stub for an unbuilt spell-scroll feature
- **What:** The `case ESpellSource::Item:` branch in `ProcessPostCastBySource` is a `// TODO: Consume spell item from inventory` stub. No spell anywhere sets `SpellSource = ESpellSource::Item` (zero producers, zero callers), so the case is unreachable today.
- **Where:** `Private/ActionExecutor.cpp:1916-1919`.
- **Evidence:** `grep -n "SpellSource\s*=\s*ESpellSource::Item|SpellSource\s*==\s*ESpellSource::Item" Source/world_of_refraction` returns zero matches. Standard item consumption from loadout `ItemSlots` is wired separately via `UActionExecutor::ExecuteItem` (`:1838-1842`) → `ULoadoutComponent::UseItem(SlotIndex)` (decrement `Slot.Quantity`, broadcast `OnItemUsed`).
- **Impact:** None today (unreachable). If spell-scroll items get designed later, this is the call site that needs wiring. **Standard item consumption from loadout `ItemSlots` works correctly via `ExecuteItem` → `Loadout->UseItem`.**
- **Priority:** Low (demoted from Medium after verification — `ESpellSource::Item` has no producers and the standard item-use path is wired correctly).
- **Scope:** Small (when spell-scroll items are actually built).

### 10.4 Beam DOT damage is placeholder
✅ **RESOLVED** on `feature/integration-gaps-sweep-1` (discrete-tick model). Design call:
- New `USpellData::BeamTickInterval` field (Beam-only, default `0.5f`, `ClampMin = 0.01`).
- Tick count derived at projectile init: `BeamTickCount = max(1, RoundToInt(BeamDuration / BeamTickInterval))`.
- Per-tick damage = `BaseDamage / BeamTickCount`; integer remainder = `BaseDamage % BeamTickCount` distributed across the first `Remainder` ticks so the running total exactly equals `BaseDamage`.
- `ASpellProjectile::TickBeam` accumulates `DeltaTime` toward `BeamTickIntervalSec`; on threshold, broadcasts `OnBeamTick(Target, ThisTickDamage, bTargetInBeam)` and increments the tick index. Beam stops when tick index hits count OR `BeamTimeRemaining` elapses.
- `FOnBeamTick` signature changed: `(AActor* Target, int32 TickDamage, bool bTargetInBeam)` — `DeltaTime` dropped (no per-frame consumer remained).
- Per-frame VFX/debug visualization stays inside `TickBeam` (cosmetic, continuous); the delegate is purely the damage-side surface now.
- TODOs at `ActionExecutor.cpp:2788-2790` removed; handler applies the broadcast `TickDamage` directly.

- **Where:** `Private/ActionExecutor.cpp:2788` `// TODO: Calculate per-tick damage based on beam total damage and duration`; `:2790` `int32 TickDamage = 5; // Placeholder`.
- **Impact:** Beam spells deal flat 5 per tick regardless of design intent.
- **Priority:** Medium if any beam spell ships.
- **Scope:** Small.

### 10.5 Damage calculator status-multiplier modifiers
✅ **RESOLVED** on `feature/integration-gaps-sweep-3`. Verification revealed `UDamageCalculator::CalculateStatusBuildup` (the function containing the TODO) had zero callers — dead code, same pattern as sweep-2's 10.6. Resolution applied as **(a) + (b)** together:

- **(a)** Deleted `UDamageCalculator::CalculateStatusBuildup` (decl + impl, ~30 lines) with a tombstone comment pointing readers to the live path. `GetBDStackStatusMultiplier` is preserved — it's still consumed by the BD damage path.
- **(b)** Added the genuine missing piece — StatusMultiplierBuff/StatusMultiplierDebuff aggregation on the attacker — to `UStatusBuildupManager::AddStatusBuildup` between the character-stat amplification (lines 252-278) and the resistance reduction block. ~6 lines, mirrors the `DamageBuff`/`DamageDebuff` shape at `DamageCalculator.cpp:521-523`. Uses `GetEffectManager()` (the file-local precedent — same call already at `:154, :203, :303, :453`).

Resistance side was NOT added: the live path's `GetTotalElementResistance` (`StatusBuildupManager.cpp:163-184`, called at `:298`) already aggregates `ResistanceBuff`/`ResistanceDebuff` with the element filter the user's spec described. Adding it again at `CalculateStatusBuildup` would have double-applied even if that function had been live.

**Cross-link to gap 7.1:** the queried `StatusMultiplierBuff`/`Debuff` aggregation goes through the same SkillEffectManager handler stubs flagged by 7.1 (`SkillEffectManager.cpp:1051-1056`). `GetTotalStatModifier` sums effect values by type, so the query path is sound — but the values sum to whatever the stubbed handlers populate. If 7.1's handlers remain no-op, this fix queries `0.0f` and is effectively a no-op until 7.1 lands. The wiring is correct and will activate the moment the handlers do real work.

- **Where:** `Private/DamageCalculator.cpp:366` `// TODO: Apply skill effect modifiers — StatusMultiplierBuff / StatusMultiplierDebuff` (incomplete TODO comment).
- **Impact:** Buffs/debuffs that modify status-multiplier don't affect damage.
- **Priority:** Medium.
- **Scope:** Small.

### 10.6 Loadout validation reports errors but doesn't clear bad slots
🔄 **REFRAMED (sweep-2)**. Original framing was "three TODO sites need wiring"; verification on `feature/integration-gaps-sweep-2` revealed the three sites were in **dead code** (`FCombatLoadout::Validate / ValidateGeneric / ValidateCaster / ValidateResonator`, zero callers). Option-(a) cleanup applied: the four dead functions were deleted (decl + impl, plus the in-block `// TODO: Validate against inventory when component exists` comments). `FCombatLoadout::ValidateBDSpellLoadout` stays — it's still shared with `FSavedLoadout::GetValidationErrors`. The real gap surfaces below.

- **What:** Loadout validation in `ULoadoutComponent::GetValidationErrors` reports errors but does not mutate the loadout — bad slots remain populated after validation, so runtime can still hit unowned items mid-combat.
- **Where:** `ULoadoutComponent::GetValidationErrors` (`LoadoutComponent.cpp:440-667`, currently `const`). Comprehensive ownership checks exist (primary weapon, weapon `AssignedAbilities`/`AssignedSpells`, primary ring, primary evolution via `UEvolutionInventoryComponent::HasInstance`, secondary weapon, ring loadout with slot-cost cap, innate spells with element-capability gate) but each finding becomes an error string — no slot is cleared. Called from `ValidateActiveLoadout`/`ValidateLoadout`/`PrepareForBattle`; `PrepareForBattle` is soft-fail (errors broadcast via `OnValidationFailed`, battle still starts).
- **Impact:** Authored loadouts with unowned references pass through to combat. The character will attempt to use the unowned item at runtime — silent failure or potential crash depending on null-handling at the use site. Logged via `OnValidationFailed` broadcast but the slot itself is not cleared.
- **Priority:** Medium — data-integrity, latent runtime bug.
- **Scope:** Medium — contract change (validator becomes non-const, mutation semantics), plus careful audit of every `GetValidationErrors` caller (currently 3 in `LoadoutComponent.cpp` alone) to make sure mutation is safe at each call site.
- **Status:** NOT in this sweep — separate future task.

#### Original framing (sweep-1 catalog entry)
> Three TODOs in `FCombatLoadout.cpp` at lines 59, 97, 149: "Validate against inventory when component exists." Treated as a small fix wiring three inventory cross-checks. Sweep-2 verification: those three TODO sites were inside `FCombatLoadout::ValidateGeneric / ValidateCaster / ValidateResonator` — zero callers anywhere in the module. The dead functions were deleted; the real gap (soft-reject semantics on the live path) was reframed above.

### 10.7 `LoadoutComponent` auto-populate is dumb
- **Where:** `Private/LoadoutComponent.cpp:1037` `// TODO: Implement smarter auto-population`.
- **Impact:** `AutoPopulateLoadout` only assigns the first available weapon and skips items entirely.
- **Priority:** Low — affects authoring UX, not gameplay.
- **Scope:** Medium.

---

## Priority Summary

Sorted by Priority then Scope. "Pitch impact" flag highlights items affecting the demo path.

| # | Gap | Priority | Scope | Pitch? |
|---|---|---|---|---|
| 1.1 | `ACombatPlayerController` test path bypasses orchestrator | High | Medium | YES |
| 2.1 | `DefensePromptWidget` fully stubbed | High | Small | YES |
| 2.2 | `OnDefenseWindowOpened` no subscriber | High | (= 2.1) | YES |
| 3.2 | Orchestrator `OnCombatResultReady` / `OnCombatStateChanged` no production C++ subscriber | Medium-High | Medium | YES (if no BP) |
| 1.2 | Player defense input is BP-only | Medium | Small | — |
| 2.3 | `OnDefenseInputReceived` / `OnParryReflect` / `OnDefenseCueTriggered` no subscribers | Medium | Small | — |
| 3.1 | ActionExecutor `OnActionStarted`/`Completed`/`HealingDone`/`TargetKilled` no consumers | Medium | Medium | — |
| 4.1 | BD `OnTransformed` / `OnOverloadDamage` / `OnStacksChanged` no UI | Medium | Small | YES (if runtime BD transform happens on stage) |
| 4.2 | BD forbidden-cast self-buildup unwired — self-damage exists, self-status-buildup half missing; scales with StatusMultiplier | Medium | Small-Medium | — |
| 4.3 | BD overload aura per-turn tick — status-buildup release + absorption-drain coupling unwired; HP-damage half may be partially wired via `OnOverloadDamage` | Medium | Medium | — |
| 5.1 | LoadoutComponent delegates no subscribers | Medium | Small | — |
| 5.2 | `OnItemUsed` / `OnGambleResult` no subscribers | Medium | Small | — |
| 7.1 | SkillEffectManager Phase 2 passive-layer stubs | Medium | Medium | YES (if affected effects are demo'd) |
| 7.2 | **✅ RESOLVED (sweep-4)** — Status-buildup-on-self TODO — reframed: two new effect types `StatusIncrease`/`StatusDecrease` flow through existing effect system with element from resolved cast source | Medium | Small | — |
| 7.3 | Action-camera transition TODO | Medium | Small | — |
| 8.1 | HUD spawn only via test actor / BP | Medium | Medium | — |
| 9.2 | BD InnateSpells empty | Medium (if BD demoed) | Small (designer fix) | YES (if BD) |
| 10.1 | **✅ RESOLVED (sweep-2)** — AI `SpellSource` defaults to Innate — new `ULoadoutComponent::ResolveSpellSource` helper; 6 AI sites updated; locked precedence Innate→Ring→Weapon→Evolution | Medium | Small | YES (AI casts will mis-charge) |
| 10.4 | **✅ RESOLVED (sweep-1)** — Beam DOT placeholder 5/tick — discrete-tick model; new `USpellData::BeamTickInterval` field; remainder distributed across ticks | Medium | Small | YES (if beams demoed) |
| 10.5 | **✅ RESOLVED (sweep-3)** — DamageCalculator StatusMultiplier modifiers — dead-code deletion + StatusMultiplierBuff/Debuff wired into live StatusBuildupManager path (resistance side already lived there) | Medium | Small | — |
| 10.6 | **🔄 REFRAMED (sweep-2)** — loadout validation reports errors but doesn't clear bad slots — dead struct-side Validate* deleted; live `GetValidationErrors` needs mutation semantics (separate future task) | Medium | Medium | — |
| 2.4 | **✅ RESOLVED (sweep-1)** — `OnDefenseWindowRequested` pure dead | Low | Small | — |
| 3.3 | `OnResurrected` no subscribers | Low | Small | — |
| 6.1 | **✅ RESOLVED (sweep-1)** — Movement Approach/Return delegates never broadcast — dead bits deleted; `FOnApproachComplete` renamed to `FOnMovementComplete` to match the live field | Low | Small | — |
| 6.2 | `OnMovementCancelled` no subscribers | Low | Small | — |
| 6.3 | Teleport vanish/appear VFX TODOs | Low | Small (asset-bound) | — |
| 8.2 | `CharacterPanelWidget` hover/click no C++ subscribers | Low | Small | — |
| 9.1 | Verify cycle-source intent for other submenus | Low (verify) | Small | — |
| 10.2 | **✅ RESOLVED (sweep-1)** — Attack tier/size hardcoded — read from asset; new `UWeaponAttackData::BaseSize` field | Low | Small | — |
| 10.3 | `ESpellSource::Item` stub case (unreachable) | Low | Small | — |
| 10.7 | LoadoutComponent auto-populate dumb | Low | Medium | — |

**Totals:** 31 distinct gaps — **3 High**, **14 Medium / Medium-High**, **14 Low**.
**Pitch-impacting (the subset most likely to bite the demo):** 1.1, 2.1, 2.2, 3.2, 4.1, 7.1, 9.2, 10.1, 10.4.

**Smallest fix-set to unblock a clean pitch demo:** 2.1 (implement DefensePromptWidget Phase 1) + 1.1 (gate or remove `CombatPlayerController` test path) + confirm BP-side coverage of 3.2 (or ship a minimal C++ result-overlay). Three focused fixes, ~one session each.
