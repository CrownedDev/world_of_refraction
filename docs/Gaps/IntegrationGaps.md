# Integration Gaps — Catalog

**Date:** 2026-05-27
**Branch surveyed:** `main` (immediately post-merge of `feature/crystal-wear-substat-modifier`, commit `2cc8205`)

This catalog enumerates UE-side integration gaps: places where designed intent and current production wiring diverge. Scope: test-scaffolding standing in for real integration, unbound delegates, stubbed implementations, and UI options reachable in design but not in production. **Pure code-quality items, asset/content gaps, and performance concerns are out of scope.**

Methodology: grep sweep across `Source/world_of_refraction/**` for TODO / FIXME / Phase markers; cross-reference all `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*` declarations against `AddDynamic`/`AddUObject` subscribers and `.Broadcast(` sites; inspect `Category="Test"`/`"Debug"` UPROPERTY/UFUNCTION marked as doing real work.

---

## 1. Player input layer — `ACombatPlayerController` is test scaffolding

### 1.1 `ACombatPlayerController` builds player actions from test fields and bypasses orchestrator
✅ **RESOLVED** on `feature/realtime-defense` — the test-scaffolding PC was **deleted**: `Public/Testing/CombatPlayerController.h` and `Private/Testing/CombatPlayerController.cpp` no longer exist, and no `TestSpell`/`TestTarget`/`TestAbility` action-submission surface remains in any PC (grep returns zero). The branch added a real `ACombatPlayerController` at `Public/Combat/CombatPlayerController.h` / `Private/Combat/CombatPlayerController.cpp` (commit `b8682afb`) that owns **defense input** (see 1.2); player turn-action submission continues through the command-menu → `ACombatOrchestrator::SubmitAction` path, so the parallel-bypass concern is gone.

**Original gap below for history.**

- **What:** The combat PC fires actions from designer-assigned test properties straight into `UActionExecutor`, skipping `ACombatOrchestrator::SubmitAction` validation, the `bWaitingForAsyncAction` guard, win-condition checks, and the orchestrator's `OnActionExecuted` broadcast.
- **Where:** `Source/world_of_refraction/Public/CombatPlayerController.h:55-67`; `Source/world_of_refraction/Private/CombatPlayerController.cpp:160-238` (`OnConfirmAction`).
- **Evidence:** `UPROPERTY(... Category = "Test|Spell") TObjectPtr<USpellData> TestSpell;` (h:55), `Category = "Test|Combat") AActor *TestTarget` (h:59), `Category = "Test|Ability") TObjectPtr<UAbilityData> TestAbility` (h:62). `OnConfirmAction` calls `Executor->ExecuteActionAsync(ControlledActor, Action, ...)` directly (cpp:226).
- **Impact:** If `ACombatPlayerController` is the active PC class in production play, **two parallel player paths can submit actions** — the test PC bypasses turn-gating (a confirm press during another actor's turn submits anyway), and the orchestrator never sees the action so `OnActionExecuted` doesn't fire for the menu/UI subscribers. If it's NOT the active PC, then the entire `Test*`/`OnConfirmAction` surface is dead weight masquerading as the player input layer.
- **Priority:** High — confirm in PIE which PC the game mode uses, then either gut the test path or fence it behind a `bIsTestMode` flag.
- **Scope:** Medium — disambiguating, gating/deleting, and wiring a real PC that goes through the orchestrator is a focused session. A full proper PC implementation is Large.

### 1.2 Player defense input is BP-only — `DefenseSystem::SubmitDefenseInput` has no player-side C++ caller
✅ **RESOLVED** on `feature/realtime-defense` — `UDefenseSystem::SubmitDefenseInput` now has a player-side C++ caller: `ACombatPlayerController` (`Private/Combat/CombatPlayerController.cpp:86, 99, 112, 125`) submits Block/Parry/Dodge from bound input, resolving the active defender via `GetActiveDefenderForLocalPlayer` (commits `b8682afb`/`b59d6f69`). The declaration now lives at `Public/Combat/Defense/DefenseSystem.h`. Player defense input is no longer BP-only.

**Original gap below for history.**

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
- **Status:** Deferred pending design decisions — priority unchanged (still High, still a pitch-blocker), but implementation is blocked on Crown answering: which defense options are available per attack context (the full set always, or attack-specific subsets); the timing-indicator style (shrinking bar / ring / QTE / something else); the input scheme (keyboard / mouse / controller — couples to the new production PC from gap 1.1); the no-input fallback (auto-block / take the hit / something else); and the visual style plus how the prompt relates to the existing combat HUD. The seven TODOs can't be written truthfully until these are locked, so 2.1 waits on Crown rather than on engineering capacity.

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
🔀 **SPLIT (2026-05-29 triage)** — two halves with different statuses:
- **Camera half → BLOCKED-DESIGN.** Wiring `OnActionStarted`/`OnActionCompleted` to the camera (uncomment the bindings, flesh out the stub handlers) is a small implementation; the *work* is the design question — what should the camera DO when an action fires? Crown deferred pending **camera-feel direction**. Implementation scope is small; design scope (camera language/feel for combat actions) is the unresolved work. Defer until camera direction is decided. Pairs with [7.3].
- **Asset half → BLOCKED-EXTERNAL.** Kill-feed (`OnTargetKilled`) and healing-VFX (`OnHealingDone`) consumers depend on a combat-log surface + VFX/SFX assets that don't exist yet. Same bucket as 5.1/5.2.

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
⛔ **BLOCKED-EXTERNAL (2026-05-29 triage)** — Producer wired correctly; consumer-side is asset-blocked on revive VFX/SFX/UI/combat-log. The revive mechanic exists (S-tier healing item + effects) and `OnResurrected` fires correctly; what's missing is the *feedback*, and every plausible consumer (resurrection VFX, SFX, a UI flash, a combat-log entry) is itself blocked on assets/systems that don't exist. C++ side is complete. Same bucket as 5.1/5.2.

- **What:** Revive intercept fires `OnResurrected` but nothing listens.
- **Where:** `CharacterDataComponent.h:17` (decl); broadcasts at `:272, 289`.
- **Evidence:** No `AddDynamic` to `OnResurrected` anywhere. Compare `OnDied` (`:250, 291`) which IS bound by CharacterPanelWidget and WeatherStateManager.
- **Impact:** No resurrection VFX/SFX/log; UI bar refreshes only because the Revive path also broadcasts `OnHPChanged` (`:273, 278`), not because anything binds `OnResurrected`.
- **Priority:** Low — only relevant if revive becomes prominent.
- **Scope:** Small.

---

## 4. BD overflow — visibility broadcasts + self-cost mechanics

### 4.1 `UBrokenDarknessManager::OnTransformed`, `OnOverloadDamage`, `OnStacksChanged` — broadcast, no production C++ subscribers
✅ **RESOLVED (partial)** on `feature/integration-gaps-sweep-5` — the BD display surfaces flagged here are now wired. Three pieces shipped:

- **Piece 1 — element display:** new `UCharacterDataComponent::GetDisplayElement()` (BlueprintPure) returns `BrokenDarkness` for any IsBrokenDarkness character, else delegates to the existing `UCharacterData::GetElement()` (Caster→Innate; others→Generic). Single source of truth for "what element does this character display as". The energy-bar tint was already BD-gated correctly in `ApplyEnergyBarTint` (`CharacterPanelWidget.cpp:402-426`) — no C++ change needed there. **BP-side follow-up (Crown editor work, out of C++ scope):** wire `WBP_CharacterPanel::ClassElementText` to call `GetDisplayElement()` instead of reading `InnateElement` directly.

- **Piece 2 — stack display (reworked sweep-5):** `CharacterPanelWidget` binds `OnStacksChanged` and `OnAlignmentChanged` (and `OnTransformed`); their handlers route through `RefreshEffectsList`. Stacks render in the **existing effects/debuff panel** as a synthetic `FActiveSkillEffect` row — `EffectType = StatusMultiplierBuff` (truthful: stacks amplify status buildup on matching-element spells via `UDamageCalculator::GetBDStackStatusMultiplier`), `Element = CurrentAlignment`, `bCanStack = true`, `CurrentStacks = GetCurrentStackCount()`, `MaxStacks = GetMaxStacks()`, `bPermanent = true`, `EffectName` = element display name. The existing `SkillEffectBlueprintLibrary` helpers consume it directly: `GetEffectDisplayName` → element name, `GetEffectStackString` → "xN" (stack 1 collapses to no count, which matches the 1× multiplier truthfully), `IsEffectBuff` → true → row tints as a buff. **Single-alignment model** preserved — appended row auto-clears when stacks drop to 0 or alignment switches. **No separate widget**, **no BP changes required**; the standalone `StacksText` was removed.

- **Piece 3 — overload bar text color (sweep-5):** `RefreshEnergyBar` colors `EPText` when BD has `CurrentEP > MaxEP`. Thresholds in `CombatConstants.h` rescaled to fit the real overload window (`MaxEP × OVERLOAD_CAPACITY_FRACTION = 30%`, i.e. ratio capped at 1.30): **101%+ → yellow, 111%+ → orange, 121%+ → red.** Red sits at 1.20 so it's reachable before the 1.30 hard cap. Resets to white when not overloaded. Visual cue for energy-past-cap, since the bar percent is clamped at 1.0.

**Still out of scope (not in this sweep):** `OnTransformed` stinger (UI/SFX, separate concern), `OnOverloadDamage` per-tick feedback (4.3 overload-aura gap covers the gameplay half; UI feedback is a separate consumer task), runtime BD transformation UI beat.

- **What:** Three BD lifecycle/combat broadcasts have no UI/SFX consumer in C++.
- **Where:** `BrokenDarknessManager.h:21, 24, 26`. Broadcasts at `BrokenDarknessManager.cpp:248` (Transformed), `:605, 623` (Stacks), `:301, 511, 521` (Overload damage).
- **Evidence:** `CharacterPanelWidget` binds `OnEnergyAbsorbed` (`:92`) and `OnOverloadStateChanged` (`:93`) but NOT the three above. `CombatCommandMenuSubsystem` binds `OnAlignmentChanged` (`:478`). No other BD subscribers.
- **Impact:** No on-screen "TRANSFORMED INTO BROKEN DARKNESS" stinger on the runtime-transform path (a major narrative beat fires silently). No stack-count indicator updates on absorption stack changes (the absorbed-element color updates via `OnAlignmentChanged`, but the **stack count** doesn't drive any UI). No "overload aura tick damaged enemy X" feedback during overload.
- **Priority:** Medium — runtime BD transformation is dramatic in design; firing it silently undersells it.
- **Scope:** Small per consumer; depends on designer priority.

### 4.2 BD forbidden-cast self-buildup unwired
✅ **RESOLVED** on `feature/bd-forbidden-cast-self-cost` — both halves of the locked design now fire together inside `UBrokenDarknessManager::ProcessForbiddenCast`:

- **Self-DAMAGE corrected to scale off SpellDamage.** `CalculateForbiddenCastDamage` now returns `SpellBaseDamage × ForbiddenCastSelfDamagePercent × GetEvolutionModifiedSpellDamage()`. Matches the spell-damage convention at `DamageCalculator::GetAttackerDamageMultiplier` (`:226-249`), where `GetEvolutionModifiedSpellDamage` is used as a direct multiplier on damage (returns `1.0 + (ModifiedMind × points × SPELL_DAMAGE_PER_POINT)` — an unleveled caster still pays the flat 25%, a stat-invested one scales up). Previous behaviour was a flat % of the spell's authored `BaseDamage`, ignoring the caster's stat.

- **Self-BUILDUP wired** alongside the self-damage. New designer constant `ForbiddenCastSelfBuildupPercent` (default 0.25, parallel to `ForbiddenCastSelfDamagePercent`). `ProcessForbiddenCast` now also calls `UStatusBuildupManager::AddStatusBuildup(Owner, Owner, SpellBaseDamage × ForbiddenCastSelfBuildupPercent, SpellElement, EPhysicalDamageType::None)`. The standard pipeline applies the caster's StatusMultiplier amplification (step 5/5b) and the caster's own Resistance reduction (step 6) automatically — Source and Target are both the BD, so the caster eats their own backlash through the normal damage-symmetry buildup math. Element = the forbidden element being cast (the attempted-absorb element per locked design).

- **Apply hook chosen.** Routed directly to `UStatusBuildupManager` from `BrokenDarknessManager.cpp` — the dead `UActionExecutor::ApplySelfStatusBuildup` helper (`:3458-3473`) was NOT used (its body is a TODO stub against the wrong subsystem). After 4.3 lands with the same pattern, both `ApplySelfStatusBuildup` and `ApplySelfDamage` can be cleaned up. **(done)** — both helpers deleted on `feature/dead-helper-cleanup`; see 4.x cross-reference below.

- **Single debug log** at Warning level summarising both costs ("damage X, base buildup Y").

- **`OnOverloadDamage` broadcast preserved** — still fires the self-damage value for any future VFX/UI consumer; no current subscribers, no behavioural change.

**Original gap below for history.**

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
✅ **RESOLVED** on `feature/bd-overload-aura` (branched off `feature/fix-bd-stack-multiplier` for step 5c availability). The locked design is now wired in `UBrokenDarknessManager::ProcessOverloadTick`:

- **Unified HP-damage path.** Both aura damage (to combatants in range) and self-damage now scale `Base × GetEvolutionModifiedSpellDamage()` — the 4.2 forbidden-cast convention. Previously both scaled by `StatusMultiplierBonus`, which mixed channels (damage and status sharing one stat curve). `ApplyDamageToActor` stays the shared apply primitive. `OnOverloadDamage` broadcast preserved at the same three sites.

- **Coupled energy leak.** `released = BaseEnergyRelease × StatusMultiplierBonus × EfficiencyMult` — single quantity feeding both outflows:
  - **Drain:** `ServerSpendEnergy(RoundToInt(released))`, replacing the prior decoupled `BaseEnergyDrain × (1 - Eff%)` formula (which also had the Efficiency direction inverted relative to its inline comment — fixed in passing).
  - **Self-status:** `AddStatusBuildup(BD, BD, released, GetCurrentAlignment(), None, /*bSkipBaseStatAmp=*/true)` — same `released` number, no recomputation.

- **New `bSkipBaseStatAmp` flag on `AddStatusBuildup`** (default false). Gates ONLY step 5 (the base-stat `1 + (Spirit × points × per-point)` block). Steps 5b (skill-effect StatusMultiplierBuff/Debuff), 5c (BD absorption-stack amplification — restored on `feature/fix-bd-stack-multiplier`), and 6 (target resistance) all still fire on the self-status. Skip is correct because `released` already includes the base-stat StatusMultiplier — re-applying step 5 would double-count.

- **Renames + signature cleanup:**
  - `BaseEnergyDrain` → `BaseEnergyRelease` (semantic shift: now one constant drives both drain and self-status, per locked design).
  - `ProcessOverloadTick(NearbyEnemies, StatusMultiplierBonus, EfficiencyPercent)` → `(..., EfficiencyMult)`. `CombatOrchestrator` drops the prior `× 100 / × 0.01` round-trip and passes the [`1 - EFFICIENCY_MAX`, `1.0`] multiplier directly.

- **Auto-exit preserved.** `ServerSpendEnergy` broadcasts `OnEPChanged`, which `HandleOwnerEnergyChanged` consumes to call `UpdateOverloadState` → `ExitOverload` when `CurrentEP ≤ MaxEP`. The leak provides the self-recovery curve the design specified.

- **Stack multiplier comes along for free.** A Fire-aligned BD at stacks 2/3 overloading on Fire gets the self-status amplified ×2/×4 via step 5c — automatic consequence of routing self-status through `AddStatusBuildup`. No manual stack-handling needed in `ProcessOverloadTick`.

**Original gap below for history.**

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

### 4.x cross-reference — `ApplySelfStatusBuildup` / `ApplySelfDamage` cleanup landed
✅ **CLEANED UP** post-4.2/4.3. Both helpers (`UActionExecutor::ApplySelfStatusBuildup` and `ApplySelfDamage`) deleted on `feature/dead-helper-cleanup` along with their "INFUSION INTERNAL HELPERS" header in `ActionExecutor.h` and the "HELPER IMPLEMENTATIONS" header in `ActionExecutor.cpp`. The 4.2 (`ProcessForbiddenCast`) and 4.3 (`ProcessOverloadTick`) implementations route directly to `UStatusBuildupManager::AddStatusBuildup` from `UBrokenDarknessManager` — intrinsic BD self-cost mechanics live in the manager, not in `ActionExecutor`. Pre-deletion grep confirmed zero live callers (the original "leave in place" gate from sweep-4 expected at-least-one helper to become the apply hook; the implementations chose direct manager calls instead).

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
✅ **RESOLVED (obsolete)** — `UCombatMovementComponent` was **dissolved** (`9563ff2d` / `9d064648`; warp positioning replaced the movement system). `Public/Combat/Grid/CombatMovementComponent.h` / `.cpp` no longer exist (grep returns zero), so `OnMovementCancelled` and its owning component are gone.

- **What:** Cancel event fires but nothing reacts.
- **Where:** `CombatMovementComponent.h:25` (decl); `Private/CombatMovementComponent.cpp:303` (broadcast).
- **Impact:** A cancelled approach (e.g. target dies mid-flight) silently aborts — no "attack interrupted" feedback.
- **Priority:** Low.
- **Scope:** Small.

### 6.3 Approach/return cosmetic VFX TODOs unimplemented
✅ **RESOLVED (obsolete)** — same dissolution: `UCombatMovementComponent` is deleted (`9563ff2d`), so the approach/return cosmetic-VFX TODOs at the old `CombatMovementComponent.cpp:159, 170` no longer exist. Warp-based positioning (`BeginSkillExecution`) replaced the teleport-movement path.

- **What:** Movement component plays no vanish/appear FX during teleport-style movement.
- **Where:** `Private/CombatMovementComponent.cpp:159, 170`.
- **Evidence:** `:159` `// TODO: Play DepartureVFX, DepartureSound, MovementMontage (vanish)`; `:170` `// TODO: Play ArrivalVFX, ArrivalSound, ArrivalMontage (appear)`.
- **Impact:** Teleport-class moves snap instantly with no transition VFX.
- **Priority:** Low — cosmetic.
- **Scope:** Small once assets exist (asset gap, not catalogued here).

---

## 7. Status effect & buildup — stub branches and TODO-gated wiring

### 7.1 `USkillEffectManager` Phase 2 passive-layer effect handlers are switch-stubs
✅ **RESOLVED** (2026-05-30, docs-only — no code change needed). Verified previously and re-confirmed in survey 2026-05-30: SkillEffectManager's passive effect handling works correctly via the query system. Effects enter `ActiveEffects` in `ApplyEffect` (`SkillEffectManager.cpp:134`); `GetTotalStatModifier` (`:1462`) reads them directly by summing `GetStackedValue()` over the matching `EffectType` — it never touches `ApplyEffectLogic`. The "stub" switch bodies in `ApplyEffectLogic` are intentionally log-only for query-driven effect types (stat modifiers `:1117-1128`, defensive passives `:1186-1193`, immunities `:1198-1213`) — that's the correct architecture, not a missing implementation. On-hit triggers (`ApplyBurn/Chill/StunToTarget`) are wired at `OnDamageDealtHandler:1357`. Resource percent/drain cases (`RestoreHPPercent`, `RestoreEnergyPercent`, `DrainHP`, `DrainEnergy`) already have real bodies. The gap's framing as "stubbed handlers blocking aggregation" was incorrect: 10.5's StatusMultiplierBuff/Debuff aggregation already reads live values the moment such an effect is applied — the `0.0f` observation was simply the correct return when no such effect is present, not a dormant-handler symptom.

**No commits to reference** — closed as working-as-designed after survey; no code work performed.

_(Separate, out-of-scope follow-up noted during survey: there is no dedicated `SkillEffectManagerDebug.h/.cpp` pair, unlike most other systems. That's CLAUDE.md debug-tooling compliance, not part of 7.1 — track as its own cleanup task. `SkillEffectManagerTestActor` currently serves as the inspection harness.)_

- **Original framing (incorrect):** A block of effect types has empty handler branches at `SkillEffectManager.cpp:1112`; passive-layer buff/debuff effects authored in data may apply but produce no runtime effect.

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
🎨 **BLOCKED-DESIGN (2026-05-29 triage)** — Implementation scope is small; design scope (camera language/feel for combat actions) is the unresolved work. Defer until camera direction is decided. Moves in lockstep with [3.1]'s camera half.

- **What:** Action camera transition not wired.
- **Where:** `Private/CombatCameraManager.cpp:466`.
- **Evidence:** Quoted above; lives inside the `OnActionCompleted` handler which is itself unbound (item 3.1).
- **Impact:** Even if 3.1 is fixed, the action-camera transition is a no-op until this is implemented.
- **Priority:** Pair with 3.1 (Medium).
- **Scope:** Small.

### 7.4 `USkillEffectManager` has no `SkillEffectManagerDebug` pair — CLAUDE.md tooling gap
- **What:** Every other C++ system ships a `<SystemName>Debug.h/.cpp` pair per the CLAUDE.md "Debug tools — required per system" rule (CharacterDataDebug, WeaponDataDebug, ItemDataDebug, SpellDataDebug, AbilityDataDebug, BreakCalculatorDebug, etc.). `USkillEffectManager` — a runtime subsystem holding per-actor `ActiveEffects` state — has no such pair. There is no `GetActiveEffectsString(Actor)` / `PrintEffectState(Actor)` / `GetXxxString()` snapshot inspector.
- **Where:** `Source/world_of_refraction/Private/Skills/Effects/` and `.../Public/Skills/Effects/` — no `SkillEffectManagerDebug.*` present (confirmed by glob 2026-05-30).
- **Evidence:** `SkillEffectManagerTestActor` currently fills the inspection role, but it's an editor isolation harness (PIE-guarded, manual `CallInEditor` spawn) — not the lightweight static-logging snapshot tool the CLAUDE.md pattern prescribes. Per the rule: *"If a system can't be inspected without launching PIE and triggering the exact path, debug tools are missing."* SkillEffect state today meets that bar.
- **Impact:** Active-effect state (which buffs/debuffs are on an actor, stacks, remaining turns, summed stat modifiers) can't be dumped on demand without the test-actor flow. This bit the 7.1 survey directly — verifying `GetTotalStatModifier` returns live values had to be reasoned from the data flow rather than read off a snapshot. Diagnosing passive-effect issues in PIE is harder than it should be.
- **Priority:** Low — tooling/compliance, not a runtime defect. No gameplay impact.
- **Scope:** Small — a `SkillEffectManagerDebug.h/.cpp` pair: static `GetActiveEffectsString(AActor*)` + `PrintEffectState(AActor*)` logging via `UE_LOG` / `AddOnScreenDebugMessage`, mirroring the existing Debug pairs. Optionally a `CompareEffectState()` snapshot helper for the runtime-system variant of the pattern.
- **Surfaced by:** 7.1 survey (2026-05-30) — flagged out-of-scope there and filed here.

---

## 8. UI lifecycle lives only in BP / test actors

### 8.1 No production C++ spawns HUD widgets — only `AHUDTestActor`
✅ **CLOSED (working as designed)** — BP-via-`OnCombatStartedUI` is the intentional extension point pattern, not a fragile workaround. `ACombatOrchestrator` declares `OnCombatStartedUI` as a `BlueprintImplementableEvent` (CombatOrchestrator.h, *"override in BP to create HUD"*) and calls it at `CombatOrchestrator.cpp:207` after `TurnManager` init (correct widget-lifecycle ordering); `BP_CombatOrchestrator` overrides it to spawn the HUD. The original framing below ("no production C++ spawns HUD widgets") was factually true but mischaracterized the design — BP-side spawn IS the intended architecture. `AHUDTestActor` is unrelated test-only scaffolding (an editor isolation harness, PIE-guarded, manual `CallInEditor` spawn). No port-to-C++ is required. The only residual is auditability (the C++ side can't grep-verify the BP wiring exists) — noted, not actioned.

**Original framing below for history.**

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

- **(a)** Deleted `UDamageCalculator::CalculateStatusBuildup` (decl + impl, ~30 lines) with a tombstone comment pointing readers to the live path. `GetBDStackStatusMultiplier` was preserved at the time — but see the regression note below.
- **(b)** Added the genuine missing piece — StatusMultiplierBuff/StatusMultiplierDebuff aggregation on the attacker — to `UStatusBuildupManager::AddStatusBuildup` between the character-stat amplification (lines 252-278) and the resistance reduction block. ~6 lines, mirrors the `DamageBuff`/`DamageDebuff` shape at `DamageCalculator.cpp:521-523`. Uses `GetEffectManager()` (the file-local precedent — same call already at `:154, :203, :303, :453`).

Resistance side was NOT added: the live path's `GetTotalElementResistance` (`StatusBuildupManager.cpp:163-184`, called at `:298`) already aggregates `ResistanceBuff`/`ResistanceDebuff` with the element filter the user's spec described. Adding it again at `CalculateStatusBuildup` would have double-applied even if that function had been live.

**Cross-link to gap 7.1:** the queried `StatusMultiplierBuff`/`Debuff` aggregation goes through the same SkillEffectManager handler stubs flagged by 7.1 (`SkillEffectManager.cpp:1051-1056`). `GetTotalStatModifier` sums effect values by type, so the query path is sound — but the values sum to whatever the stubbed handlers populate. If 7.1's handlers remain no-op, this fix queries `0.0f` and is effectively a no-op until 7.1 lands. The wiring is correct and will activate the moment the handlers do real work.

#### 10.5.r — BD stack multiplier regression (caused by 10.5 cleanup, fixed)
✅ **RESOLVED** on `feature/fix-bd-stack-multiplier`. Sweep-3's deletion of `CalculateStatusBuildup` left `UDamageCalculator::GetBDStackStatusMultiplier` standing but **with zero live callers** — its only consumer was the deleted function. Net effect: BD absorption stacks (1×/1×/2×/4× at stacks 0-3) became inert against matching-element status buildup. The sweep-3 tombstone comment claiming "the BD damage path still consumes it" was incorrect — that path was inside the deleted function.

- Moved the element-gated accessor onto the manager: `UBrokenDarknessManager::GetElementStackStatusMultiplier(ESpellElement Element) const` (`BrokenDarknessManager.h/.cpp`). Returns `1.0` when not transformed or element doesn't match `CurrentAlignmentElement`; otherwise returns `GetStackStatusMultiplier()`. Same logic as the old `DamageCalculator::GetBDStackStatusMultiplier`, just lifted to where it naturally lives (no `DamageCalculator` state was being used).
- Wired into `UStatusBuildupManager::AddStatusBuildup` as **step 5c** (between the StatusMultiplierBuff/Debuff aggregation and the target-resistance reduction): `Amount *= Source->FindComponentByClass<UBrokenDarknessManager>()->GetElementStackStatusMultiplier(Element)`. Safe to call unconditionally — non-BD sources return `1.0` via the manager-component lookup failing, and matching-element/transformed gating lives inside the accessor.
- Deleted `UDamageCalculator::GetBDStackStatusMultiplier` (decl + impl). Updated the sweep-3 tombstone in `DamageCalculator.cpp:347-360` to capture the regression resolution. Updated the descriptive comment at `CharacterPanelWidget.cpp:367` to point at the new accessor location.
- **Side benefit (4.3 enabler):** any `AddStatusBuildup` call from an overloaded BD targeting themselves in the current alignment now picks up stack amplification automatically — gap 4.3's "released energy × stacks" question is resolved by this regression fix without any code in `ProcessOverloadTick` needing to know about stacks.

**PIE-verify:** A Fire-aligned BD at 2-3 stacks casting an offensive Fire spell should build target Fire status noticeably faster than at 0-1 stacks. Pre-fix: no amplification at all.

- **Where:** `Private/DamageCalculator.cpp:366` `// TODO: Apply skill effect modifiers — StatusMultiplierBuff / StatusMultiplierDebuff` (incomplete TODO comment).
- **Impact:** Buffs/debuffs that modify status-multiplier don't affect damage.
- **Priority:** Medium.
- **Scope:** Small.

### 10.6 Loadout validation reports errors but doesn't clear bad slots
✅ **RESOLVED (2026-05-30, scope rung C — full detection + clear).** The validator now detects ownership / element-coherence (weapon-crystal + ring-crystal) / weapon-type-ability / cap-violation invalidities, and `ULoadoutComponent::ClearInvalidSlots` auto-clears them at `PrepareForBattle` (after the `OnValidationFailed` broadcast, before combat starts) so combat always begins with a valid loadout. The structured path is `CollectInvalidSlotFindings() → TArray<FInvalidSlotFinding>` (each carries `ELoadoutSlotType` + slot/sub index + `bClearable`), the action-mappable counterpart the build-prep notes called for; BD-pool and guard findings stay non-clearable, item-slot ownership stays exempt (self-owning transfer model). A `DebugClearAndReportValidation` CallInEditor helper exercises the arc without staging PIE. Resolved in commits `31ba784`, `1df0f43`, `9bec6de`, `7bae871`, `1496169` (on `main`). Build-prep history below retained. See [`docs/Architecture/LoadoutSystem.md`](../Architecture/LoadoutSystem.md) §Validation.

🔄 **REFRAMED (sweep-2)**. Original framing was "three TODO sites need wiring"; verification on `feature/integration-gaps-sweep-2` revealed the three sites were in **dead code** (`FCombatLoadout::Validate / ValidateGeneric / ValidateCaster / ValidateResonator`, zero callers). Option-(a) cleanup applied: the four dead functions were deleted (decl + impl, plus the in-block `// TODO: Validate against inventory when component exists` comments). `FCombatLoadout::ValidateBDSpellLoadout` stays — it's still shared with `FSavedLoadout::GetValidationErrors`. The real gap surfaces below.

🎯 **RESCOPED → BLOCKED-DESIGN (2026-05-29 build-prep survey).** What looked like a SMALL "clear what the validator already flags" task widened once Crown clarified what *invalid* means. The "detect → don't clear" framing is true **only for ownership-mismatch**; Crown's actual concern is **element-source mismatch** and **weapon-type/ability mismatch**, neither of which the validator detects today. So this is a *build-detection* task, not a *clear-existing-findings* task — and its shape depends on design answers Crown hasn't given. (The morning's DOABLE-NOW assessment assumed ownership was the only invalidity; the build-prep survey overturned that. Earlier sweep-1 framing remains at the bottom for history.)

- **What:** Two distinct concerns, only one of which is detected today:
  - *(detected)* **Ownership mismatch** — `GetValidationErrors` flags loadout slots referencing items the player doesn't own, but is `const` and never clears them, so bad refs survive to combat.
  - *(NOT detected — Crown's real concern)* **Element-source mismatch** (a slot's element-source assumption is broken by current equipment — e.g. a WeaponCrystal-sourced spell when the equipped weapon has no crystal, or a wrong-element crystal) and **weapon-type/ability mismatch** (an `AssignedAbility` whose `RequiredWeaponType` doesn't match the equipped weapon). The validator checks neither.
- **Where:** `ULoadoutComponent::GetValidationErrors` (`LoadoutComponent.cpp:440-667`, `const`). Its checks are **ownership + four structural gates** (evolution-spell count cap, resonator ring slot-cost cap, innate-spell class-element *castability* via `IsElementCastable`, BD pool rules) + duplicate-item-type. Crucially: the ability check is **ownership-only** (`HasAbility` → "not learned") — it never compares `Ability->RequiredWeaponType`. Element-source coherence is checked **nowhere** at battle-prep; `ResolveSpellSource` (`:874-952`) resolves at runtime and **silently defaults to `Innate`** on no match rather than failing.
- **Impact:** Loadouts whose abilities/spells are *owned but incoherent* (wrong weapon type, missing/wrong-element crystal source) pass validation clean and reach combat. The mismatch surfaces only at cast time — wrong element resolved, or an ability that can't fire — with no battle-prep signal. Ownership-mismatch additionally survives un-cleared (the original residue).
- **Priority:** Medium — data-integrity, latent runtime correctness, now with a wider surface than first catalogued.

#### Implementation notes (2026-05-29 build-prep survey)
- **Error list is not action-mappable.** `GetValidationErrors` returns `TArray<FString>` (human-readable), with no slot index or enum payload. Any clearing/detection logic **must re-derive per-slot validity independently** — it cannot parse the error strings back to specific slots.
- **Scope ladder (Crown picks the rung):**
  - **(A) Ownership-only** — the catalog's original framing. `ClearInvalidSlots` re-derives ownership per slot and nulls. Detection already exists. **SMALL (~30 min).**
  - **(B) + weapon-type/ability mismatch** — detection logic already exists but is **orphaned**: `FWeaponLoadoutEntry::ValidateAbilities` (`FWeaponLoadoutEntry.cpp:141-182`) checks `RequiredWeaponType` match + dual-wield gate, but returns a bare `bool` and is **not called by the live validator**. Needs wiring + refactor to a per-ability finding. **SMALL-MEDIUM.**
  - **(C) + element-source mismatch** — **no existing detection**; must be built from the available primitives (`HasCrystal`, `GetCrystalElement`, `ResolveSpellSource`). **MEDIUM.**
- **Open design question (gates B/C):** is `ResolveSpellSource`'s silent `Innate` fallback **graceful degradation (allowed)** or **invalid (rejected)**? This single decision determines whether element-source mismatch is even a validation *failure* — and therefore whether (C) is in scope at all.
- **Status:** ⛔ **BLOCKED-DESIGN** — waiting on Crown for (1) scope rung A/B/C, and (2) the Innate-fallback intent. Implementation can't proceed truthfully until both are answered. *(Mechanics already confirmed: inventory-removal paths don't clear loadout refs, and there are no runtime equip setters, so battle-prep remains the correct chokepoint whichever rung is chosen.)*

#### Original framing (sweep-1 catalog entry)
> Three TODOs in `FCombatLoadout.cpp` at lines 59, 97, 149: "Validate against inventory when component exists." Treated as a small fix wiring three inventory cross-checks. Sweep-2 verification: those three TODO sites were inside `FCombatLoadout::ValidateGeneric / ValidateCaster / ValidateResonator` — zero callers anywhere in the module. The dead functions were deleted; the real gap (soft-reject semantics on the live path) was reframed above.

### 10.7 `LoadoutComponent` auto-populate is dumb
🎯 **BLOCKED-DESIGN (2026-05-29 triage)** — Likely tied to the character-to-enemy loadout source decision. Design question: when a player uploads a character without explicit loadouts, where does the loadout come from? Direction A (max power), B (thematic), C (constrained random), or hybrid? What "smart" means depends entirely on what auto-populate is *for*, and that isn't decided. **NOT** related to the loadout-as-difficulty model — authored enemies use hand-authored loadouts (see [`docs/Design/LoadoutDifficultyModel.md`](../Design/LoadoutDifficultyModel.md)).

- **Where:** `Private/LoadoutComponent.cpp:1037` `// TODO: Implement smarter auto-population`.
- **Impact:** `AutoPopulateLoadout` only assigns the first available weapon and skips items entirely.
- **Priority:** Low — affects authoring UX, not gameplay.
- **Scope:** Medium.

---

## Priority Summary

Sorted by Priority then Scope. "Pitch impact" flag highlights items affecting the demo path.

| # | Gap | Priority | Scope | Pitch? |
|---|---|---|---|---|
| 1.1 | **✅ RESOLVED (realtime-defense)** — test PC deleted (`Testing/CombatPlayerController` gone); real `Combat/CombatPlayerController` added; actions route through orchestrator | High | Medium | YES |
| 2.1 | `DefensePromptWidget` fully stubbed | High | Small | YES |
| 2.2 | `OnDefenseWindowOpened` no subscriber | High | (= 2.1) | YES |
| 3.2 | Orchestrator `OnCombatResultReady` / `OnCombatStateChanged` no production C++ subscriber | Medium-High | Medium | YES (if no BP) |
| 1.2 | **✅ RESOLVED (realtime-defense)** — `SubmitDefenseInput` called from `Combat/CombatPlayerController.cpp:86-125` via `GetActiveDefenderForLocalPlayer` | Medium | Small | — |
| 2.3 | `OnDefenseInputReceived` / `OnParryReflect` / `OnDefenseCueTriggered` no subscribers | Medium | Small | — |
| 3.1 | **🔀 SPLIT (2026-05-29)** — camera half → BLOCKED-DESIGN (camera-feel direction); kill-feed/healing-VFX half → BLOCKED-EXTERNAL (combat-log + VFX) | Medium | Small (camera) | — |
| 4.1 | **✅ RESOLVED partial (sweep-5)** — BD UI display: `GetDisplayElement` helper + stack-text wiring (single-alignment "Fire x3") + overload text color (yellow/orange/red); `OnTransformed` stinger still pending | Medium | Small | YES (if runtime BD transform happens on stage) |
| 4.2 | **✅ RESOLVED (bd-forbidden-cast-self-cost)** — forbidden-cast self-buildup wired in `ProcessForbiddenCast`; self-damage corrected to scale off `GetEvolutionModifiedSpellDamage` | Medium | Small-Medium | — |
| 4.3 | **✅ RESOLVED (bd-overload-aura)** — coupled `released` quantity drains absorption + becomes self-status; HP-damage unified on `GetEvolutionModifiedSpellDamage`; `bSkipBaseStatAmp` flag on `AddStatusBuildup` prevents stat double-count | Medium | Medium | — |
| 5.1 | LoadoutComponent delegates no subscribers | Medium | Small | — |
| 5.2 | `OnItemUsed` / `OnGambleResult` no subscribers | Medium | Small | — |
| 7.1 | **✅ RESOLVED (2026-05-30, docs-only)** — passive handling works as designed: effects enter `ActiveEffects` in `ApplyEffect`, `GetTotalStatModifier` reads them directly; log-only `ApplyEffectLogic` bodies are correct for query-driven types; on-hit triggers wired at `OnDamageDealtHandler:1357`. "Stubbed handlers blocking aggregation" framing was incorrect | Medium | — | — |
| 7.2 | **✅ RESOLVED (sweep-4)** — Status-buildup-on-self TODO — reframed: two new effect types `StatusIncrease`/`StatusDecrease` flow through existing effect system with element from resolved cast source | Medium | Small | — |
| 7.3 | **🎨 BLOCKED-DESIGN (2026-05-29)** — Action-camera transition; impl small, camera-feel direction is the work; pairs with 3.1 camera half | Medium | Small | — |
| 7.4 | **🛠️ DOABLE-NOW (2026-05-30)** — no `SkillEffectManagerDebug` pair; CLAUDE.md "debug tools per system" compliance gap. Surfaced by 7.1 survey | Low | Small | — |
| 8.1 | **✅ CLOSED (working as designed)** — BP-via-`OnCombatStartedUI` is the intentional extension-point pattern; `AHUDTestActor` is unrelated test scaffolding; no C++ port needed | — | — | — |
| 9.2 | BD InnateSpells empty | Medium (if BD demoed) | Small (designer fix) | YES (if BD) |
| 10.1 | **✅ RESOLVED (sweep-2)** — AI `SpellSource` defaults to Innate — new `ULoadoutComponent::ResolveSpellSource` helper; 6 AI sites updated; locked precedence Innate→Ring→Weapon→Evolution | Medium | Small | YES (AI casts will mis-charge) |
| 10.4 | **✅ RESOLVED (sweep-1)** — Beam DOT placeholder 5/tick — discrete-tick model; new `USpellData::BeamTickInterval` field; remainder distributed across ticks | Medium | Small | YES (if beams demoed) |
| 10.5 | **✅ RESOLVED (sweep-3)** — DamageCalculator StatusMultiplier modifiers — dead-code deletion + StatusMultiplierBuff/Debuff wired into live StatusBuildupManager path (resistance side already lived there) | Medium | Small | — |
| 10.6 | **✅ RESOLVED (2026-05-30, rung C)** — validator now detects ownership / element-coherence / weapon-type-ability / cap-violation; `ClearInvalidSlots` auto-clears at `PrepareForBattle`. Commits `31ba784`/`1df0f43`/`9bec6de`/`7bae871`/`1496169` | Medium | C: Medium | — |
| 2.4 | **✅ RESOLVED (sweep-1)** — `OnDefenseWindowRequested` pure dead | Low | Small | — |
| 3.3 | **⛔ BLOCKED-EXTERNAL (2026-05-29)** — `OnResurrected` producer wired; consumer-side asset-blocked (revive VFX/SFX/UI/combat-log) | Low | Small | — |
| 6.1 | **✅ RESOLVED (sweep-1)** — Movement Approach/Return delegates never broadcast — dead bits deleted; `FOnApproachComplete` renamed to `FOnMovementComplete` to match the live field | Low | Small | — |
| 6.2 | **✅ RESOLVED (obsolete)** — `UCombatMovementComponent` dissolved (`9563ff2d`); `OnMovementCancelled` gone | Low | Small | — |
| 6.3 | **✅ RESOLVED (obsolete)** — `UCombatMovementComponent` dissolved (`9563ff2d`); teleport-VFX TODOs gone | Low | Small (asset-bound) | — |
| 8.2 | `CharacterPanelWidget` hover/click no C++ subscribers | Low | Small | — |
| 9.1 | Verify cycle-source intent for other submenus | Low (verify) | Small | — |
| 10.2 | **✅ RESOLVED (sweep-1)** — Attack tier/size hardcoded — read from asset; new `UWeaponAttackData::BaseSize` field | Low | Small | — |
| 10.3 | `ESpellSource::Item` stub case (unreachable) | Low | Small | — |
| 10.7 | **🎯 BLOCKED-DESIGN (2026-05-29)** — auto-populate's home is the character-to-enemy loadout-source decision (max-power / thematic / constrained-random?); not authored-enemy difficulty | Low | Medium | — |

**Totals:** 32 distinct gaps — **3 High**, **14 Medium / Medium-High**, **15 Low**.
**Pitch-impacting (the subset most likely to bite the demo):** 1.1, 2.1, 2.2, 3.2, 4.1, 7.1, 9.2, 10.1, 10.4.

**Smallest fix-set to unblock a clean pitch demo:** 2.1 (implement DefensePromptWidget Phase 1 — Stage 5 of the realtime-defense arc) + confirm BP-side coverage of 3.2 (or ship a minimal C++ result-overlay). *(1.1 — the test-PC bypass — is now ✅ RESOLVED on `feature/realtime-defense`.)*
