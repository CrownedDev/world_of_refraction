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

## 4. BD overflow visibility — broadcasts without UI consumers

### 4.1 `UBrokenDarknessManager::OnTransformed`, `OnOverloadDamage`, `OnStacksChanged` — broadcast, no production C++ subscribers
- **What:** Three BD lifecycle/combat broadcasts have no UI/SFX consumer in C++.
- **Where:** `BrokenDarknessManager.h:21, 24, 26`. Broadcasts at `BrokenDarknessManager.cpp:248` (Transformed), `:605, 623` (Stacks), `:301, 511, 521` (Overload damage).
- **Evidence:** `CharacterPanelWidget` binds `OnEnergyAbsorbed` (`:92`) and `OnOverloadStateChanged` (`:93`) but NOT the three above. `CombatCommandMenuSubsystem` binds `OnAlignmentChanged` (`:478`). No other BD subscribers.
- **Impact:** No on-screen "TRANSFORMED INTO BROKEN DARKNESS" stinger on the runtime-transform path (a major narrative beat fires silently). No stack-count indicator updates on absorption stack changes (the absorbed-element color updates via `OnAlignmentChanged`, but the **stack count** doesn't drive any UI). No "overload aura tick damaged enemy X" feedback during overload.
- **Priority:** Medium — runtime BD transformation is dramatic in design; firing it silently undersells it.
- **Scope:** Small per consumer; depends on designer priority.

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
- **What:** Buildup-on-self code path is unimplemented.
- **Where:** `Private/ActionExecutor.cpp:3430`, `:3535`.
- **Evidence:** `:3430` `// TODO: Implement status buildup on self`. `:3535` `// TODO: Integrate with SkillEffectManager when API is available`. Adjacent `:3532` `int32 BaseBuildup = 10 * HitCount; // TODO: Get from CombatConstants`.
- **Impact:** Self-buildup (e.g. recoil from a forbidden cast that should add to the caster's own status bar) doesn't fire; magic constants used.
- **Priority:** Medium — silent gameplay gap if any spell is designed around it.
- **Scope:** Small.

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
- **Where:** `Private/DamageCalculator.cpp:366` `// TODO: Apply skill effect modifiers — StatusMultiplierBuff / StatusMultiplierDebuff` (incomplete TODO comment).
- **Impact:** Buffs/debuffs that modify status-multiplier don't affect damage.
- **Priority:** Medium.
- **Scope:** Small.

### 10.6 `FCombatLoadout` ownership validation unbuilt (×3)
- **Where:** `Private/FCombatLoadout.cpp:59, 97, 149` — three `// TODO: Validate against inventory when component exists`.
- **Impact:** Loadout validation is structural only; the inventory-ownership cross-check is deferred. Authored saved loadouts can list owned-only items that aren't actually owned and pass validation.
- **Priority:** Medium (data-integrity).
- **Scope:** Small.

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
| 5.1 | LoadoutComponent delegates no subscribers | Medium | Small | — |
| 5.2 | `OnItemUsed` / `OnGambleResult` no subscribers | Medium | Small | — |
| 7.1 | SkillEffectManager Phase 2 passive-layer stubs | Medium | Medium | YES (if affected effects are demo'd) |
| 7.2 | Status-buildup-on-self TODO | Medium | Small | — |
| 7.3 | Action-camera transition TODO | Medium | Small | — |
| 8.1 | HUD spawn only via test actor / BP | Medium | Medium | — |
| 9.2 | BD InnateSpells empty | Medium (if BD demoed) | Small (designer fix) | YES (if BD) |
| 10.1 | AI `SpellSource` defaults to Innate | Medium | Small | YES (AI casts will mis-charge) |
| 10.4 | **✅ RESOLVED (sweep-1)** — Beam DOT placeholder 5/tick — discrete-tick model; new `USpellData::BeamTickInterval` field; remainder distributed across ticks | Medium | Small | YES (if beams demoed) |
| 10.5 | DamageCalculator StatusMultiplier modifiers | Medium | Small | — |
| 10.6 | `FCombatLoadout` ownership validation × 3 | Medium | Small | — |
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

**Totals:** 29 distinct gaps — **3 High**, **12 Medium / Medium-High**, **14 Low**.
**Pitch-impacting (the subset most likely to bite the demo):** 1.1, 2.1, 2.2, 3.2, 4.1, 7.1, 9.2, 10.1, 10.4.

**Smallest fix-set to unblock a clean pitch demo:** 2.1 (implement DefensePromptWidget Phase 1) + 1.1 (gate or remove `CombatPlayerController` test path) + confirm BP-side coverage of 3.2 (or ship a minimal C++ result-overlay). Three focused fixes, ~one session each.
