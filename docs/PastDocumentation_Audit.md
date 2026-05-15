# Past Documentation Audit
Date: 2026-05-14
Branch: feature/skill-effect-phase-b

## Summary
Reviewed 47 docs in `docs/PastDocumentation/` (plus 4 analysis docs and the conceptual overview) spanning January–May 2026. Most session summaries were retrospective and self-contained; outstanding items cluster around eight themes: AI extension, spell-architecture refactor, element systems (advantage/status-mapping), defense UI BP, weather variants, dead-code cleanup, animation/VFX, and three known bugs the analysis pass surfaced. The single biggest live arc is the **Spell Architecture refactor** (designed April 2026, never executed) — the spell-on-crystal data model is still in the codebase.

## Files Reviewed

### January 2026
- `January2026/12012026/CameraSystem_Design_Document.md` — combat camera design (Home/Character/Selection/Action states)

### April 2026
- `Project_Roadmap_CombatSystem.md` — combat-system roadmap (priorities 1–3 + long term)
- `CombatCameraSystem_Documentation.md` — completed camera report + action-phase next steps
- `Session_CombatHUD_WeatherDesign_April2026.md` — combat debug HUD + weather design kickoff
- `Session_HUDFixes_WeatherDesign_April17_2026.md` — HUD bar fixes + full weather design
- `WeatherSystem_Design_April2026.md` — weather data assets + monetisation spec
- `WeatherSystem_Restructure_April2026.md` — per-character `EquippedWeatherVariant` restructure
- `Session_WeatherComplete_AnimationPlan_April21_2026.md` — weather completion + animation plan
- `Session_22042026_WeaponCombatAnimations.md` — CombatMaster animation integration
- `Session_23042026_CombatUI (1).md` — `WBP_CombatActionMenu` Blueprint construction
- `session_gc_crash_actor_owned_ui_april2026.md` — GC crash diagnosis + actor-owned widget plan
- `session_actor_owned_combat_ui_april2026.md` — actor-owned UI completed implementation
- `SpellArchitecture_Refactor_Plan.md` — remove spells from crystals → weapon/ring (DEFERRED)
- `SpellArchitecture_ClaudeCode_Prompt.md` — companion execution prompt for refactor
- `Session_29042026_ItemsSubmenu_TargetPicker.md` — items submenu + target picker
- `Session_29042026_Part2_MenuLifecycle_HUDDiagnosis.md` — Phase 0/1 menu lifecycle + HUD bug
- `HUD_Migration_Steps.md` — C++ migration steps for `WBP_CombatHUD`
- `2026-04-29-postmortem-menu-cpp-migration.md` — failed C++ menu binding migration postmortem
- `Workstream_A1_Capabilities_Audit.md` — 4 gaps in `FCombatCapabilities::BuildFrom`
- `New_Session_Kickoff_Prompt.md` — kickoff template
- `Session_30042026_Part2_CommandMenuB0B2.md` — menu rebuild Workstreams A1/B0/B1/B2
- `Session_30042026_Part3_MenuRebuildComplete_BUsingPrimaryDebt.md` — menu rebuild + `bUsingPrimary` debt logged
- `Session_30042026_Part3_MenuRebuildComplete_BUsingPrimaryDebt (3).md` — duplicate of above
- `Infusion_Menu_Integration_Design_Pin.md` — pinned infusion menu design (deferred)
- `Session_01052026_HUD_Architecture_Migration.md` — HUD → `UCombatHUDRoot` migration
- `MetaHuman_Consideration_WoR.docx` — (binary, not read)
- `CombatActionMenu_Architecture.docx` — (binary, not read)
- `CombatHUDWidget.cpp` / `.h` — (source files; skipped)

### May 2026
- `Infusion_Design_Decisions_Locked.md` — 12 locked design decisions
- `Infusion_Cleanup_Migration_Audit.md` — operational cleanup checklist
- `Session_05052026_Durability_Cost_Phase4d_Complete.md` — Phase 4d crystal wear shipped
- `Durability_Refactor_Implementation_Reconciliation.md` — bridge doc reconciling design vs reality
- `NextSession_Pre_Submenu_Backend.md` — pre-submenu backend closure plan
- `Session_05052026_Phase4f_CategoryB_Complete.md` — Phase 4f + Category B cleanup
- `Design_Decisions_05052026.md` — 3 parked design conversations
- `Session_06052026_PickerInfusion_Complete (1).md` — picker-embedded infusion controls
- `Session_07052026_DamageApplication_BacklashUI.md` — damage pipeline lazy-bind + backlash UI
- `Session_07052026_DamageApplication_BacklashUI_bShowPrimaryRename.md` — adds `bShowPrimary` rename
- `Session_07052026_Reality_Element_FirstClass.md` — Reality element + animation interrupt fix
- `Handoff_Durability_Header_2026_05_08.md` — durability header arc
- `Resource_Model_Redesign_OpenQuestions.md` — resource model parked + closed
- `Handoff_Item32_Resonator_Dormant_EP_2026_05_08.md` — Item 32 shipped
- `PerAction_Stat_Modifiers_Locked.md` — `FActionStatModifiers` generalisation
- `Handoff_PerActionAccumulator_SubstatLayout_LuckArc_2026_05_08.md` — Luck arc handoff
- `Project_Backlog_2026_05_08.md` — master backlog (May 8)

### Future Work
- `Futurework/Luck_Consumers_Design.md` — Luck consumer designs (5 consumers)

### Analysis (`docs/analysis/` + root)
- `Conceptual_Overview_2026-05-11.md` — designer/publisher overview + honest limitations
- `analysis/Codebase_Analysis_Pass1_Map.md` — structural smell map
- `analysis/Codebase_Analysis_Pass1_StatusUpdate_2026-05-11.md` — Pass 1 verification update
- `analysis/Codebase_Technical_Reference_2026-05-11.md` — system-by-system reference
- `analysis/Codebase_Analysis_Pass2_ApplyConsolidation.md` — Pass 2 read-only audit for `ApplyHit`

---

## Outstanding Items — Still Relevant

### Spell Architecture Refactor (remove spells from crystals)
- **Source doc**: `April2026/SpellArchitecture_Refactor_Plan.md` + `SpellArchitecture_ClaudeCode_Prompt.md` (entire docs)
- **Original goal**: Move spell storage off `UItemData` (crystals) onto weapon/ring loadout entries; add `RequiredEvolutionCrystal` on `SpellData`; restructure `LoadoutData` slots (`PrimaryWeaponSpells`, `PrimaryRingSpells`, `EvolutionSpells`, `SecondaryWeaponSpells`, `ResonatorRingSpells`).
- **Current state**: `RequiredEvolutionCrystal` exists on `SpellData.h`. `FRingLoadoutEntry` still exposes `GetLockedSpellCount()`/`GetLockedSpells()` and delegates to `FRingInventoryEntry`. Spell-on-crystal data still in place.
- **Gap**: Full restructure — `ItemData` spell fields removal, `FCrystalInventoryEntry` simplification, new `LoadoutData` layout, `FCombatLoadout::CreateFromAsset` update, AI/inventory consumers audit.
- **Suggested next step**: Confirm intent before reopening — high-blast-radius refactor; needs paired loadout-asset migration.

### Element Advantage / Resistance System (9-element matrix)
- **Source doc**: `Conceptual_Overview_2026-05-11.md` "Honest limitations"; `analysis/Codebase_Technical_Reference_2026-05-11.md` DamageCalculator section
- **Original goal**: 9-element weakness/resistance matrix consulted by `DamageCalculator::CalculateDamage`.
- **Current state**: `UDamageCalculator::IsWeakTo()` and `ResistsElement()` return `false` unconditionally (`DamageCalculator.cpp:470-480`). Stubs in `CharacterData`, `DamageCalculator`, `BrokenDarknessManager`.
- **Gap**: Matrix authoring + lookup wiring.
- **Suggested next step**: Author matrix (likely a data table or constexpr array indexed by `ESpellElement`), wire in `DamageCalculator::ApplyElementMultiplier`.

### Element → Status Mapping (spell-hit status)
- **Source doc**: `May2026/Design_Decisions_05052026.md` (Element-to-Status mapping); `Conceptual_Overview_2026-05-11.md`
- **Original goal**: `EStatusType GetPrimaryStatusForElement(ESpellElement)` helper; call from `ActionExecutor::ApplyCommitCosts` Evolution case, `ApplyAbilityInfusionStatus`, `DamageCalculator` spell-hit buildup. `ESkillEffectType::Silenced` (Darkness) added.
- **Current state**: `ESkillEffectType::Silenced` already exists (`ESkillEffectType.h:127`). `GetPrimaryStatusForElement` does not exist (grep zero hits).
- **Gap**: Helper + 3 call sites.
- **Suggested next step**: Add helper in `ESkillEffectType.h` (or new `StatusElementMapping.h`); wire call sites; decide Lightning vs Wind for Stunned.

### AIDecisionManager Audit + Extend
- **Source doc**: `May2026/Project_Backlog_2026_05_08.md` (🟢 priority); `Handoff_*_2026_05_08.md` recurring
- **Original goal**: Audit `BuildAction_Smart`, `TrySurvivalBranch`, `TryCleanseBranch`, `BuildOffensiveAction`; close AI value-estimation drift (AI doesn't see `ActionMods`, raw-mode, per-asset `StatusBuildup`).
- **Current state**: `BuildAction` + difficulty branches implemented (`AIDecisionManager.cpp:143+`). Easy/Medium/Hard tiers route through random / `BuildAction_Smart` / extended logic. Drift gaps unfixed: `DamageCalculator::CalculateAttackDamage`/`CalculateSpellDamage` AI wrappers omit `ActionMods`; AI still calls `Ability->CalculateStatusBuildup()` instead of `UAbilityData::StatusBuildup` UPROPERTY.
- **Gap**: Preview-path parity with execution path; status buildup field switch; ability-effect awareness.
- **Suggested next step**: Plumb `FActionStatModifiers` into AI preview wrappers; switch ability buildup callsite to UPROPERTY field.

### Per-Hit Dodge (Luck consumer)
- **Source doc**: `Futurework/Luck_Consumers_Design.md`; `May2026/Project_Backlog_2026_05_08.md` (🔵)
- **Original goal**: Pre-defense per-hit Luck dodge roll at `ActionExecutor::ApplyDamage`; reuse `bWasDodged`; scale status buildup through `ProcessMultiHit`; mirror on `WeaponManager::ApplyWeaponDamage` sync path.
- **Current state**: Not implemented (Luck constants would live in `CombatConstants.h`, no `LUCK_DODGE` references found).
- **Gap**: Roll site + delegate/result flag + sync-path TODO.
- **Suggested next step**: Implement in `ApplyHit` (unified hit applicator — landed in Phase A), avoiding sync-path duplication.

### Phase 6 — Evolution Backlash
- **Source doc**: `May2026/Durability_Refactor_Implementation_Reconciliation.md` Phase 6; `NextSession_Pre_Submenu_Backend.md` Task 1
- **Original goal**: Self-damage HP cost + self-status buildup on Evolution infusion L1/L2 (5%/10% HP, 15%/25% buildup); element immunity gate.
- **Current state**: `EVOLUTION_L1_HP_COST_PERCENT = 0.05f` and `EVOLUTION_L2_HP_COST_PERCENT = 0.10f` constants exist in `InfusionConstants.h:40-43`. Status-buildup constants and `ApplyCommitCosts` Evolution case wiring need verification.
- **Gap**: Confirm `ApplyCommitCosts` Evolution branch fully wired and immunity check in place.
- **Suggested next step**: Read `ApplyCommitCosts` Evolution case end-to-end; add missing constants if absent.

### `WBP_DefensePrompt` Blueprint Side
- **Source doc**: `April2026/Session_01052026_HUD_Architecture_Migration.md`
- **Original goal**: BP implementation of defense prompt countdown bar + button prompts.
- **Current state**: C++ base `UDefensePromptWidget` exists with `InitialiseForCombat`/`TeardownPrompt`/`ShowPrompt`/`HidePrompt` BlueprintImplementableEvents (`UI/Combat/DefensePromptWidget.h`).
- **Gap**: BP `WBP_DefensePrompt` subclass with visuals (cannot verify from C++ side).
- **Suggested next step**: Confirm with user whether `WBP_DefensePrompt.uasset` exists; if not, BP authoring task.

### Six Sword Abilities (4 remaining)
- **Source doc**: `April2026/Session_22042026_WeaponCombatAnimations.md`; recurring through April backlog
- **Original goal**: Quick Strike + Heavy Strike + 4 more sword abilities (montages + WeaponData wiring).
- **Current state**: Quick Strike + Heavy Strike shipped (per April 22 doc). No grep evidence of 4 additional.
- **Gap**: Design + author 4 ability data assets + montages.
- **Suggested next step**: Confirm naming/scope with user before authoring.

### Unarmed Animation Pipeline
- **Source doc**: `April2026/Session_22042026_WeaponCombatAnimations.md`; `session_actor_owned_combat_ui_april2026.md`
- **Original goal**: `AM_Idle_Unarmed`, `AM_Attack_Unarmed`, `DA_Stance_Unarmed`.
- **Current state**: Not verified — `.uasset` files.
- **Gap**: Asset authoring.
- **Suggested next step**: Verify with user; this is asset work, not C++.

### `bUsingPrimary` Execution-Side Fix for Caster/Resonator
- **Source doc**: `April2026/Session_30042026_Part3_MenuRebuildComplete_BUsingPrimaryDebt.md`; `Session_07052026_DamageApplication_BacklashUI_bShowPrimaryRename.md`
- **Original goal**: `WeaponManager::InitializeWeaponState`/`GetActiveAttack`/`GetActiveAbilities` and `ActionExecutor::ExecuteAttackAsync` gate on `bUsingPrimary` and fail when false for Caster/Resonator.
- **Current state**: April 7 rename to `bShowPrimary` (step 1) is in flight — codebase has both `bShowPrimary` and `bUsingPrimary` (32 occurrences across 7 files). Step 2 (semantic redefine in `LoadoutComponent::GetActiveWeapon`, `IsArmed`, `WeaponManager::InitializeWeaponState`, `LoadoutComponent::GetCurrentStance`, `FCombatCapabilities::BuildFrom`) not verified complete.
- **Gap**: Audit current state of step-2 semantic change; gated on stance-toggle UI design.
- **Suggested next step**: Check `git log` for `bShowPrimary` commits since 2026-05-07; confirm if step 2 landed before planning further work.

### Phase B — Crystal Destruction at Combat End
- **Source doc**: `May2026/NextSession_Pre_Submenu_Backend.md` Task 3; `Durability_Refactor_Implementation_Reconciliation.md`
- **Original goal**: `ApplyBetweenCombatCrystalDestruction` + `OnCrystalDestroyedAtCombatEnd` delegate; clear `AttachedCrystal.Crystal` on inventory entry.
- **Current state**: `ApplyBetweenCombatCrystalDestruction` referenced in `CombatOrchestrator.h/.cpp` (grep). Function may be stubbed; inventory-entry clearing path needs verification.
- **Gap**: Verify implementation completeness against design.
- **Suggested next step**: Read `CombatOrchestrator::ApplyBetweenCombatCrystalDestruction` body.

### Animation / VFX / Damage Pass (`Damage: 0` bug + Lord Enot Big Pack wiring)
- **Source doc**: `May2026/Session_07052026_DamageApplication_BacklashUI.md`; `Session_07052026_Reality_Element_FirstClass.md`
- **Original goal**: Resolve spell `Damage: 0, Targets: 1` log + wire Lord Enot Big Pack VFX + animation `Interrupted: Yes` on Attack/Ability returns.
- **Current state**: Tangled into one workstream by 7 May; not yet executed.
- **Gap**: Three concerns — damage finalisation, animation interrupt, VFX routing.
- **Suggested next step**: Pick one concern (likely damage finalisation as backend-only) first.

### Defense Window Team-Wide Redesign (pinned)
- **Source doc**: `May2026/Session_07052026_DamageApplication_BacklashUI.md`
- **Original goal**: Defense window applies to entire defending team, not just primary target.
- **Current state**: Design pin only; no implementation.
- **Gap**: Full design + implementation.
- **Suggested next step**: Out of scope until current defense UI BP authored.

### Phase 5 — HP-Kill Confirmation Modal
- **Source doc**: `May2026/Durability_Refactor_Implementation_Reconciliation.md` Phase 5
- **Original goal**: Confirmation modal at commit time when HP-cost would kill the caster.
- **Current state**: Blocked on menu work (per Phase 4f doc).
- **Gap**: Modal UI + commit hook.
- **Suggested next step**: Resume after menu polish.

### Phase 7 — Source Auto-Deactivate on Unavailability
- **Source doc**: `May2026/Durability_Refactor_Implementation_Reconciliation.md` Phase 7
- **Original goal**: Auto-set infusion source/level to L0 when source becomes unavailable mid-combat; grey out Infuse button.
- **Current state**: Blocked on menu (per Phase 4f doc).
- **Gap**: Source-availability watcher + button state.
- **Suggested next step**: Resume after menu polish.

### Iolite L2 Implementation
- **Source doc**: `May2026/Session_06052026_PickerInfusion_Complete (1).md` (design lock); `Design_Decisions_05052026.md`
- **Original goal**: Extend `FDamageCalculationInput.bIoliteL2Boost`; +5% to 5 sub-stats in `CalculateDamage`.
- **Current state**: `ApplyIoliteL2StatBuff` not present in grep (likely never written). `ApplyIoliteStatBuff` orphan was flagged for deletion.
- **Gap**: Full feature implementation per locked design.
- **Suggested next step**: Add `bIoliteL2Boost` field + apply in `CalculateDamage` per design doc.

### Element Backlash for Evolution L1/L2 — Self-Status Wiring
- **Source doc**: `May2026/Durability_Refactor_Implementation_Reconciliation.md` Phase 6; `Infusion_Design_Decisions_Locked.md` Section 10
- **Original goal**: Evolution self-status using element-to-status mapping (depends on `GetPrimaryStatusForElement`).
- **Current state**: Constants exist; mapping helper missing; `ApplyAbilityInfusionStatus` body exists at `ActionExecutor.cpp:3448` but stub-status integration unverified.
- **Gap**: Depends on element-to-status mapping (item above) — same suggested fix.
- **Suggested next step**: Bundle with element-to-status mapping work.

### Diagnostic Log Cleanup
- **Source doc**: Multiple April/May sessions
- **Original goal**: Remove leftover diagnostic logs.
- **Current state**: Several unconfirmed: `BindDefenseSystemEvents`/`Bound to DefenseSystem events`/`OnDefenseWindowClosed CALLBACK FIRED` warnings; `CacheAvailableSources` log in `InfusionVFXComponent`.
- **Gap**: Greppable cleanup pass.
- **Suggested next step**: Grep for `Warning, TEXT(` in suspect files; demote or remove.

### Reality Tests A, B1, B3, Ring-Symmetric
- **Source doc**: `May2026/Handoff_Durability_Header_2026_05_08.md`; `Session_07052026_Reality_Element_FirstClass.md`
- **Original goal**: PIE verify Reality element first-class wiring scenarios.
- **Gap**: Manual PIE verification, not code.
- **Suggested next step**: User-side PIE pass.

### Reality "Any Spell" Wiring
- **Source doc**: `May2026/Session_07052026_Reality_Element_FirstClass.md`
- **Original goal**: `ValidateAction` lets Casters cast any element under Reality, but menu may filter by element first — audit needed.
- **Gap**: Menu-side filter audit (`FCombatCapabilities::BuildFrom`?).
- **Suggested next step**: Trace from button-build to `ValidateAction`.

### `WBP_TurnOrderSlot` Preview Range Expansion
- **Source doc**: `April2026/Session_01052026_HUD_Architecture_Migration.md`
- **Original goal**: Multi-slot preview (currently single-slot only); property location/name in `UTurnOrderStripWidget` Class Defaults unverified.
- **Current state**: `TurnOrderStripWidget` + `TurnOrderSlotWidget` exist; preview-range setting needs Blueprint verification.
- **Gap**: Class Defaults audit.
- **Suggested next step**: Open `WBP_CombatHUD`/`TurnOrderStripWidget` and confirm previewable property.

### Weather: Reality Variant + Earnable/Premium Variants + Mini-Game UI
- **Source doc**: `April2026/WeatherSystem_Restructure_April2026.md`; `Session_WeatherComplete_AnimationPlan_April21_2026.md`
- **Original goal**: `DA_Weather_WOR_Reality`; Earnable/Premium variants; Max rank 7/7/7 tiebreak mini-game UI; level-default restore on combat end.
- **Current state**: `WeatherStateManager` exists; `EquippedWeatherVariant` referenced in `CharacterData.h` and `WeatherStateManager.cpp`. Reality DA + variants are asset work.
- **Gap**: DA authoring + restore logic verification.
- **Suggested next step**: User-side asset task; verify `RestoreLevelWeather()` exists in code.

### `WBP_CharacterPanel` Status Bar Theming
- **Source doc**: `April2026/Session_CombatHUD_WeatherDesign_April2026.md`
- **Original goal**: StatusBar dynamic colour by incoming status element; EP colour by class (Generic brown, Resonator ring-break indicator); class/element display for Generic/Resonator.
- **Gap**: Blueprint theming.
- **Suggested next step**: Out-of-scope for C++; flag for designer pass.

### Combat-End Menu Cleanup
- **Source doc**: `April2026/Session_30042026_Part3_MenuRebuildComplete_BUsingPrimaryDebt.md`
- **Original goal**: `RemoveFromParent` on `OnCombatEnded`; menu lingers in viewport after combat.
- **Gap**: Subscribe to `OnCombatEnded` and call cleanup.
- **Suggested next step**: One-line BP/CPP wire.

### Asset-Side Crystal Subscription Migration
- **Source doc**: `May2026/Session_05052026_Phase4f_CategoryB_Complete.md`; `Session_07052026_Reality_Element_FirstClass.md`
- **Original goal**: Migrate `WeaponManager::ProcessPostCastWear`/`SubscribeToActorWeaponCrystals`/`FindWeaponOwnerOfCrystal` and `RingManager` equivalents from `Ring->SlottedCrystal` to runtime `FRingInventoryEntry::AttachedCrystal`/`FWeaponInventoryEntry::AttachedCrystal`.
- **Current state**: `ProcessPostCastWear` already consolidated into `UCrystalManager` (`CrystalManager.cpp:27`). Legacy paths in `RingManager`/`WeaponManager` may still exist.
- **Gap**: Confirm legacy paths removed.
- **Suggested next step**: Grep for `SlottedCrystal` and verify no live callers.

### DRY + Dead Code Cleanup Arc
- **Source doc**: `May2026/Project_Backlog_2026_05_08.md` (🟡); `analysis/Codebase_Analysis_Pass1_StatusUpdate_2026-05-11.md`
- **Original goal**: Multi-session cleanup. Confirmed-still-open at 2026-05-11: Bug 2 (Slash fall-through — *appears fixed in current code, breaks present at each case; verify*), Bug 3 (orphan brace in `LoadoutComponent::GetValidationErrors` — *still present at `:485-494`*), Bug 4 partial (`GetEvolutionTypeName`/`GetEvolutionStatSummary` inverted-guard — *still present at `ItemData.cpp:686-693`*), `ApplyCommitCosts` 217-line decomp, `ExecuteActionAsync` 151-line decomp, dead `GetCurrentExecutionContext` (`ActionExecutor.cpp:1644`), dead `RollCriticalHit` (`DamageCalculator.cpp:349`, zero callers), `IsStunned`/`IsSilenced`/`IsImmuneToEffectType` stubs (`SkillEffectManager.h:361-373`), 27 tuning UPROPERTYs on DamageCalculator, `STATUS_THRESHOLD_*` magic, magic `100.0f` 4×, `BreakCalculator::CalculateDurabilityWear*` duplication, `BuildOffensiveAction` 223 lines, `DecideSpellInfusionLevel`/`DecideAbilityInfusionLevel` 80% duplication, `BuildTargetButtons`/`BuildGroupTargetButtons` 71-line dup, `DurabilityHeaderWidget::RefreshForActor` 135 lines, `FCombatCapabilities::BuildFrom` 170 lines, TransBuffer in `CombatHUDRoot` unverified.
- **New items from Pass 1 status update** (still open): `UActionExecutor::ApplySpellSizeL2Cost` declared `ActionExecutor.h:491` with no body and no callers; `FWeaponAttackResult` + `FOnWeaponAttackExecuted` declared `WeaponManager.h:94/146/382` never broadcast or bound; `ApplyAbilityEffects` orphan side-effect (`ActionExecutor.cpp:~3574`); raw-mode dead-after-redirect branch at `ExecuteSpellAsync:619`.
- **Confirmed CLOSED** (verified via grep): `ESpellSource::Ring`→`RingCrystal` rename (`ESpellSource.h:20`); `bCanBeInfused` orphan EditCondition (no hits); `ConjureWeapon`/`DispelConjuredWeapon` orphans (no hits); `CalculateEvolutionCost` (no hits); `HasIloditeEquipped`/`Ilodite`→`Iolite` rename (no `Ilodite` hits); `// Action Execfutor` typo (no hits); `Phase A` ApplyHit unification (`ActionExecutor.cpp:1890`+).
- **Suggested next step**: Pick highest-leverage subset (Bug 3 + Bug 4 + dead-method deletes are quick wins; decomp work is per-system).

### Source Folder Reorganisation (`chore/source-folder-reorg`)
- **Source doc**: `May2026/Project_Backlog_2026_05_08.md` (🔵)
- **Gap**: Branch exists in name only (per backlog).
- **Suggested next step**: Plan reorganisation map before branching.

### MetaHuman Character Pipeline
- **Source doc**: Multiple April + May docs (recurring future item)
- **Gap**: Asset/integration pipeline evaluation.
- **Suggested next step**: User-side decision; `MetaHuman_Consideration_WoR.docx` is presumably the design.

### Adapter Weapons / Resonance Mode (Citrine/Amethyst)
- **Source doc**: Multiple docs; `May2026/Project_Backlog_2026_05_08.md` (🔵)
- **Gap**: Whole-system design + implementation.
- **Suggested next step**: Out of immediate scope.

### Traits System (Split Personality, conditional/triggered effects)
- **Source doc**: Multiple docs; backlog 🔵
- **Gap**: Whole-system.
- **Suggested next step**: Out of immediate scope.

### World Stat Progression / Drop Chance / Drop Quality (Luck)
- **Source doc**: `Futurework/Luck_Consumers_Design.md`; `Session_06052026_PickerInfusion_Complete (1).md`
- **Gap**: Blocked on loot system; 21-point progression hierarchy un-implemented.
- **Suggested next step**: Out of immediate scope.

### `FCombatCapabilities` Gap 2 (Spurious Switch Weapon on Evolution)
- **Source doc**: `April2026/Workstream_A1_Capabilities_Audit.md`
- **Original goal**: Tighten `bCanSwitchWeapon` check in `BuildFrom` for Evolution primary (rows G10, G11).
- **Current state**: One-line fix still flagged unapplied at last mention.
- **Gap**: Verify `BuildFrom` against spec.
- **Suggested next step**: Read `BuildFrom`'s `bCanSwitchWeapon` branch and compare against Gap-2 expected behaviour.

### Phase D Ability Effects Async Path
- **Source doc**: `analysis/Codebase_Analysis_Pass1_StatusUpdate_2026-05-11.md` (new finding)
- **Original goal**: `ApplyAbilityEffects` at `ActionExecutor.cpp:~3574` may be orphaned; `Ability->Effects[]` may not fire for async ability action.
- **Gap**: PIE test + wire post-action branch in `FinalizeAsyncAction`.
- **Suggested next step**: Read `FinalizeAsyncAction` ability post-action branch and confirm `Effects[]` is iterated.

### Buildup Currently Ignores Defense Outcome
- **Source doc**: `analysis/Codebase_Analysis_Pass2_ApplyConsolidation.md` Section 4.5
- **Original goal**: Block should reduce status buildup, dodge should cancel; currently applied pre-window, never reduced.
- **Gap**: Phase C1 migration (buildup through `ApplyHit`).
- **Suggested next step**: Behaviour-change item — needs user sign-off before commit.

### `UDefenseSystem::CalculateDefenseResult` Tuning UPROPERTYs Ignored
- **Source doc**: `analysis/Codebase_Analysis_Pass2_ApplyConsolidation.md` Separate Concerns
- **Original goal**: Use `BlockReduction`/`ParryReduction`/`ParryReflect` UPROPERTYs instead of inline literals at lines 340/346/347.
- **Gap**: Replace literals with UPROPERTY reads.
- **Suggested next step**: 3-line fix; verify no PIE regression.

### Status Buildup Through Async Attack Path
- **Source doc**: `analysis/Codebase_Analysis_Pass2_ApplyConsolidation.md`
- **Original goal**: Async attack path drops weapon physical-status buildup (`ExecuteAttackAsync` never calls `ApplyWeaponStatusBuildup`).
- **Gap**: Phase C3 — `ExecuteAttackAsync` gains `ApplyHit`.
- **Suggested next step**: Bundled with Phase B/C migration work.

### Items Pass (Item 29 — end-to-end)
- **Source doc**: `May2026/Handoff_Durability_Header_2026_05_08.md` carry-forward
- **Original goal**: End-to-end items pass — use, target, consume, effects.
- **Gap**: Status unverified.
- **Suggested next step**: Confirm with user.

### Stance-Toggle UI Design
- **Source doc**: `April2026/Session_30042026_Part3_*`; `Session_01052026_HUD_Architecture_Migration.md`
- **Original goal**: Mid-combat armed/unarmed stance toggle UI; unblocks `bUsingPrimary` execution-side fix.
- **Gap**: Design + implementation.
- **Suggested next step**: Out of immediate scope (gating dependency, not urgent).

---

## Superseded

- **Resource Model Redesign open questions** — superseded by durability-header arc + `Evolution_Stat_Buff_Locked.md` (closed 2026-05-08). Per `Resource_Model_Redesign_OpenQuestions.md` and `Handoff_Item32_Resonator_Dormant_EP_2026_05_08.md`.
- **`Evolution_Stat_Buff_Locked.md`** — superseded by `PerAction_Stat_Modifiers_Locked.md` (`FActionStatModifiers` generalises Reality/Evolution); file already removed from disk.
- **Old `WBP_ActionMenuHUD` standalone widget** — removed from backlog per `Project_Backlog_2026_05_08.md`.
- **`bUsingPrimary` April-patch in `FCombatLoadout::CreateFromAsset`** — superseded by May 7 `bShowPrimary` semantic redefine (step 2 may still be in progress).
- **April `WBP_CombatActionMenu` BP** — superseded by C++ `UCombatCommandMenuSubsystem` + `UCombatHUDRoot` rebuild (April 30 / May 1).
- **HUD `WBP_CombatHUD` BP turn-order logic** — superseded by C++ `TurnOrderStripWidget` (`UI/Combat/TurnOrderStripWidget.cpp`).
- **`UWeaponManager::ProcessPostCastWear` + `URingManager::ProcessPostCastWear`** — superseded by `UCrystalManager::ProcessPostCastWear` (`CrystalManager.cpp:27`).
- **`StatusEffect.h`** — superseded by `ActiveSkillEffect.h` + `SkillEffectManager.h` (rename complete).
- **April 17 HUD migration steps (`HUD_Migration_Steps.md`)** — superseded by May 1 `UCombatHUDRoot` migration.
- **Easy Radial Wheel Menu plugin uninstall (B5)** — likely superseded by full C++ menu rebuild; verify with `git status` for plugin files.

## Abandoned

- **Element Mode (Physical/Elemental) system** — removed from backlog 2026-05-08 (`Project_Backlog_2026_05_08.md`).
- **Custom spell wear +2 durability** — DROPPED per `Durability_Refactor_Implementation_Reconciliation.md`; default-vs-assigned spell distinction not worth complexity.
- **April `bUsingPrimary` execution-side patch** — explicit decision to defer until stance-toggle UI exists (`Session_30042026_Part3_*`); not abandoned outright but deprioritised indefinitely.
- **Container naming `PlayerTeamContainer`/`EnemyTeamContainer`** — rename to `Team0Panel`/`Team1Panel` deferred until PvP work begins (per `Session_01052026_HUD_Architecture_Migration.md`).

## Needs Verification

- **`ApplyBetweenCombatCrystalDestruction` body completeness** — symbol referenced in `CombatOrchestrator`; needs read to confirm inventory clearing path.
- **`FCombatCapabilities::BuildFrom` Gap 2** — verify `bCanSwitchWeapon` tightened.
- **`bShowPrimary` step 2 semantic redefine** — verify `WeaponManager::InitializeWeaponState` + `LoadoutComponent::GetActiveWeapon`/`IsArmed` semantics match May 7 plan.
- **`WBP_DefensePrompt` Blueprint asset existence** — C++ base ready; BP-side unknown.
- **`WBP_TurnOrderSlot` preview range property** — Class Defaults audit pending.
- **Lord Enot Big Pack VFX wiring** — not visible from C++ grep (likely asset-side).
- **`WeatherStateManager::RestoreLevelWeather()` and combat-end restore** — combat-end behaviour vs design.
- **Lingering `ESpellSource::Ring` displayname change** — enum renamed but display name still `"Ring"`; confirm intent.
- **AI ability buildup wiring** (Pass 1 status flagged Worse) — does AI now call `Ability->StatusBuildup` UPROPERTY or still `CalculateStatusBuildup()`?
- **Raw-mode dead branch at `ExecuteSpellAsync:619`** — confirm `ApplyRawModeRedirect` overrides as analysis claims.
- **Bug 2 Slash fall-through** — current `ActiveSkillEffect.h:515-523` has `break` after Slash; confirm no other switch with fall-through.
- **Items Pass (Item 29)** — completion status unclear from docs.

---

## Recommended Priority Order

1. **Three quick-win bugs from Pass 1 Status Update (Bug 3 + Bug 4)** — orphan brace in `LoadoutComponent::GetValidationErrors` (`:485-494`) duplicates Resonator slot-capacity error; `GetEvolutionTypeName`/`GetEvolutionStatSummary` inverted guards at `ItemData.cpp:686-693`. Single-file, single-PR, low risk; restore behavioural correctness flagged in user-facing conceptual overview.

2. **Element → Status mapping helper (`GetPrimaryStatusForElement`)** — unblocks two downstream items (Evolution backlash self-status, ability infusion status). Small surface area: one header + 2–3 call sites.

3. **Dead-symbol deletes** — `ApplySpellSizeL2Cost` declaration (no body), `RollCriticalHit` (zero callers), `GetCurrentExecutionContext` (zero callers), `IsStunned`/`IsSilenced`/`IsRooted` stubs if confirmed unwired, `FWeaponAttackResult`/`FOnWeaponAttackExecuted` if never broadcast. Lowers context noise for future grep work.

4. **`UDefenseSystem::CalculateDefenseResult` UPROPERTY honouring** — 3-line fix; restores designer tuning that's silently broken.

5. **AIDecisionManager preview-path parity** — `FActionStatModifiers` into `CalculateAttackDamage`/`CalculateSpellDamage` AI wrappers; switch ability buildup callsite to UPROPERTY field. Fixes AI value-estimation drift now that per-action mods are first-class.

6. **Iolite L2 implementation** — design locked; small surface (one field on `FDamageCalculationInput`, one branch in `CalculateDamage`).

7. **`FCombatCapabilities` Gap 2 fix** — one-line `bCanSwitchWeapon` tighten.

8. **Combat-end menu cleanup** — `RemoveFromParent` on `OnCombatEnded`; cosmetic but user-visible.

9. **Phase 6 Evolution backlash wiring verification** — read `ApplyCommitCosts` Evolution case; complete if stubbed.

10. **Spell Architecture refactor (large arc)** — high-blast-radius; needs paired loadout-asset migration; should only happen after demo-relevant items above are clear.

11. **Per-hit dodge (Luck)** — best done after `ApplyHit` is the single hit entrypoint (Phase A landed); avoid touching the legacy sync path.

12. **Defense window team-wide redesign / Phase 5 HP-kill modal / Phase 7 source auto-deactivate** — UI-blocked; resume after menu polish settles.

13. **Long-tail post-demo** — Element advantage matrix, MetaHuman pipeline, Adapter weapons, Traits, World Stat Progression, Drop chance/quality, multiplayer. Defer until publisher-pitch / demo polish phase.
