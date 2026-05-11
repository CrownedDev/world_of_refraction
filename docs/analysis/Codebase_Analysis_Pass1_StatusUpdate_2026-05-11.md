# Pass 1 Status Update — 2026-05-11

**Reference:** `docs/analysis/Codebase_Analysis_Pass1_Map.md` (2026-05-08)
**Scope:** Verify every Pass 1 inventory item against current code state.
**Major intervening work:** ApplyHit consolidation Phases A–D (`c6131f7..9dd2a2e`), Action Data Parity Commits 1–3 (`b3bce88..e62d3de`), merged to `main` via `5bb3fda` and `c1cae27`.

**Legend:** **Closed** — verified done in code, cited. **Partially closed** — some work landed, residue remains. **Still open** — Pass 1 finding holds, no change. **Invalidated** — the underlying code shape no longer exists; finding is moot. **Worse** — finding compounded.

---

## Overall metric drift

| Metric | Pass 1 (2026-05-08) | Current (2026-05-11) | Δ |
|---|---:|---:|---:|
| `ActionExecutor.cpp` | 4,356 | 4,130 | −226 |
| `CombatOrchestrator.cpp` | 2,527 | 2,414 | −113 |
| `StatusEffectManager.cpp` | 1,741 | 1,762 | +21 |
| `WeaponManager.cpp` | 1,199 | 888 | **−311** |
| `CharacterData.cpp` | 147 | 50 | −97 |

Phase C2 + D drove the WeaponManager and CharacterData drops. `StatusEffectManager` ticked up slightly (no consolidation pass yet).

---

## Bugs surfaced — Tier 4

| # | Original finding | Status | Evidence |
|---|---|---|---|
| 1 | `UCharacterData::CalculateEvolutionCost` Generic-case fallthrough (`CharacterData.cpp:60-63`) | **Invalidated** | Function no longer exists. Grep `CalculateEvolutionCost` across `Source/` returns zero hits. `CharacterData.cpp` is now 50 LOC and contains only editor-validation code. Function was removed in intervening work; the bug went with it. |
| 2 | `FStatusEffect::CreateFromPhysicalDamageType` Slash→Pierce fallthrough (`StatusEffect.h:441-449`) | **Still open** | Bug present verbatim. `StatusEffect.h:441-449` confirmed: `case 0: // Slash → Bleed DOT` sets up the Bleed effect then has no `break;`. Falls into `case 1: // Pierce → Armor Break`. Cases 1 and 2 have proper breaks. Slash physical attacks still always produce Armor Break, not Bleed. |
| 3 | `ULoadoutComponent::GetValidationErrors` orphan brace block (`LoadoutComponent.cpp:492-495`) | **Still open** | Bug present at `LoadoutComponent.cpp:487-495`. After the guarded `if (TotalSlotCost > MaxSlots) { Errors.Add(...) }` block, an unconditional `{ ... }` block immediately follows with a second `Errors.Add(...)` using `RESONATOR_RING_LOADOUT_SLOT_CAPACITY` — fires every Resonator validation regardless of slot usage. |
| 4 | `UItemData` inverted-logic stat-modifier cluster (9 functions) | **Partially closed** | Re-verified function by function. `HasStatModifiers` / `GetStatModifierSummary` / `GetMindModifierPercent` / `GetBodyModifierPercent` / `GetSpiritModifierPercent` / `CalculateModifiedMind/Body/Spirit` all read fields whose `EditCondition` matches the guard — these were misread in Pass 1 and the guards are semantically correct. **However:** `GetEvolutionTypeName` (`ItemData.cpp:669-671`) and `GetEvolutionStatSummary` (`:691-693`) DO have inverted guards — they `return TEXT("N/A")` when `bIsEvolutionCrystal == true`, then read evolution-only fields. Two genuine inverted functions remain; Pass 1's "9-function cluster" claim was over-broad. |

---

## Tier 1 — Big rocks

### Item #1 — ActionExecutor sync/async unification + ApplyCommitCosts decomposition

| Sub-finding | Status | Evidence |
|---|---|---|
| Sync `ExecuteSpell` / `ExecuteAbility` / `ExecuteAttack` exist | **Closed** | Phase D (`9dd2a2e`) deleted all three function bodies and declarations. `Public/ActionExecutor.h` no longer declares them; `Private/ActionExecutor.cpp` no longer defines them. Grep `UActionExecutor::ExecuteSpell\(` / `ExecuteAbility\(` / `ExecuteAttack\(` returns 0 production hits. Async + ApplyHit is the only path. |
| `ExecuteAction` switch routes Spell/Ability/Attack sync | **Closed** | `ExecuteAction` switch (`ActionExecutor.cpp:317`) now collapses Spell / Ability / Attack into a single warning branch (caller bug if reached); Item / Defend remain as legitimate sync paths. |
| `ApplyCommitCosts` 217-line monolith | **Still open** | Function still ~217 lines at `ActionExecutor.cpp:~4158` (now offset shifted but unchanged). Not touched in any session work this period. |
| `ExecuteActionAsync` 151-line orchestration | **Still open** | Function still long; Phase D-prep migrated `OnConfirmAction` to async but didn't decompose this. |
| Dead `GetCurrentExecutionContext` | **Still open** | Grep confirms zero callers. |
| `// Action Execfutor` typo at `ActionExecutor.h:136` | **Still open** | Verified present. |

### Item #2 — DamageCalculator

| Sub-finding | Status | Evidence |
|---|---|---|
| `CalculateSpellDamage` orphan (47 lines, `:145`) | **Partially closed** | Has 2 internal callers in `DamageCalculator.cpp:312, 449` (verified by grep `->CalculateSpellDamage(`, excluding the `UCharacterData::CalculateSpellDamage()` stat function). Not externally orphan; internal-only by current Action async pipeline that bypasses it. Phase E review still needed. |
| `CalculateAbilityDamage` orphan (43 lines, `:193`) | **Still open** | Zero callers. Genuinely orphan. |
| 27 tuning `UPROPERTY` floats on stateless calculator | **Still open** | Architectural question unchanged. |

### Item #5 — CombatOrchestrator

| Sub-finding | Status | Evidence |
|---|---|---|
| `DebugExecuteSyncSpell/Ability/Attack` (3 methods over 100 lines each) | **Closed** | Phase D renamed all three to `DebugExecuteAsyncSpell/Ability/Attack`. Method bodies also shrank ~50%: `DebugExecuteAsyncAttack` is now ~62 LOC (`CombatOrchestrator.cpp:1872`), `DebugExecuteAsyncSpell` ~80 LOC (`:1934`), `DebugExecuteAsyncAbility` ~58 LOC (`:2112`). Bloat reduced from ~314 LOC to ~200 LOC. |
| `SubmitAction` / `SubmitActionAsync` duplication | **Still open** | Not touched. |
| `ApplyBetweenCombatCrystalDestruction` + `ApplyBetweenCombatRepair` parallel structures | **Still open** | Not touched. |
| `OnActionCompleted` 65-line orchestration | **Still open** | Not touched. |

### Item #6 — LoadoutComponent

| Sub-finding | Status | Evidence |
|---|---|---|
| `GetValidationErrors` 208 lines, 7 validation passes | **Still open** | Now 209 lines at `:344`. Unchanged. |
| `CreateAndConfigureLoadout` 131 lines, triple-loop | **Still open** | Now 132 lines. Unchanged. |
| `GetActiveWeapon` class-conditional 5-return | **Still open** | Now 54 lines. Unchanged. |
| Orphan-block duplicate-error bug (Tier 4 #3) | **Still open** | See Bug #3 row above. |

### Item #7 — ItemData

| Sub-finding | Status | Evidence |
|---|---|---|
| 16+ tier switches | **Still open** | Not touched. |
| Inverted-logic stat-modifier cluster (Tier 4 #4) | **Partially closed (revised)** | See Bug #4 row. Most functions are clean; 2 remain inverted. |
| `GenerateDescription` 120 lines | **Still open** | Not touched. |
| `PostEditChangeProperty` 71-line display-mirror refresh | **Still open** | Not touched. |

### Item #8 — WeaponManager + crystal-subscription dedup with RingManager

| Sub-finding | Status | Evidence |
|---|---|---|
| `ExecuteAttackWithInfusion` 120-line method | **Closed** | Deleted in Phase C2 (`e43f49f`). Historical-note comment at `WeaponManager.cpp:389`: *"UWeaponManager::ExecuteAttack and ExecuteAttackWithInfusion deleted in C2."* `WeaponManager.cpp` dropped from 1,199 → 888 LOC. |
| `ApplyWeaponDamage` 83-line method | **Closed** | Deleted in Phase C2. |
| `ApplyWeaponStatusBuildup` | **Closed** | Deleted in Phase C2 (per the same comment block). |
| Crystal-subscription bookkeeping duplicated with RingManager | **Still open** | Not touched. Both managers still maintain their own subscribe/unsubscribe/handle-broken/find-owner ladders. |
| `ConjureWeapon` / `DispelConjuredWeapon` orphans | **Still open** | Not touched; Caster mechanic still unwired. |
| Magic numbers `STATUS_THRESHOLD_BLEED/ARMOR_BREAK/STUN = 100.0f` (3×) | **Still open** | Unchanged. |
| `BreakCalculator::CalculateDurabilityWear*` duplication | **Still open** | Unchanged. |

### Item #31 — StatusEffectManager

| Sub-finding | Status | Evidence |
|---|---|---|
| `ApplyImmediateStatus` (78 lines, `:1577`) and `ApplyTriggeredStatus` (107 lines, `:1656`) mirror switches | **Still open** | Methods unchanged. |
| `ApplyEffectLogic` 95 lines | **Still open** | Unchanged at `:864`. |
| Status-bar block ~350 LOC sub-feature | **Still open** | Unchanged at `:1394+`. |
| `IsImmuneToEffectType` stub still called from `ApplyEffect:49` | **Still open** | Stub present at `:1161`; call site still present. |
| `CleanupInvalidActors` no callers | **Closed** | Function no longer exists; grep returns 0 hits in source. Removed in intervening work. |
| `RemoveAllBuffs` / `RemoveAllDebuffs` / `RemoveAllDOTs` / `RemoveEffectsBySource` no external callers | **Partially closed (revised)** | Pass 1 was wrong on `RemoveAllDebuffs` — `ItemExecutor.cpp` actively calls it (Iolite cleanse). Other three still have no external production callers. |
| `IsStunned` / `IsSilenced` / `IsRooted` stubs | **Still open** | Unchanged. |
| Magic `100.0f` 4× in `IsTriggerConditionMet` | **Still open** | Unchanged. |

### Item #34 — AIDecisionManager

| Sub-finding | Status | Evidence |
|---|---|---|
| `BuildOffensiveAction` 223 lines | **Still open** | Largest method in repo. Unchanged at `:946`. |
| `DecideSpellInfusionLevel` / `DecideAbilityInfusionLevel` ~80% duplication | **Still open** | 91 LOC and 85 LOC respectively. Unchanged. |
| Three difficulty ladders | **Still open** | Unchanged. |
| `FindHealingItem` / `FindCleanseItem` / `FindEnergyItem` triplet | **Still open** | Unchanged. |
| AI ability buildup wiring (now that `AbilityData.StatusBuildup` exists) | **Worse** | Action Data Parity Commit 1 added `UAbilityData::StatusBuildup` UPROPERTY. AIDecisionManager still calls `Ability->CalculateStatusBuildup(...)` (the stat-derived formula at `AbilityData.cpp:119`) and does **not** read the new field. AI value-estimation now diverges from runtime: runtime sees raw-mode and per-asset buildup; AI sees only the stat-derived value. Score-time and runtime are out of sync. |
| Dead helpers `GetCurrentEP/GetMaxEP/GetCharacterData` | **Still open** | Unchanged. |

### Item #35 — UI cluster

| Sub-finding | Status | Evidence |
|---|---|---|
| `CombatCommandMenuSubsystem::HandleSelection` 145 lines | **Still open** | Unchanged. |
| `BuildTargetButtons` (139) and `BuildGroupTargetButtons` (128) byte-identical 71-line infusion-controls blocks | **Still open** | Identical blocks confirmed present at `:949-1019` and `:1080-1149`. |
| Source-label mapping in 4 places | **Still open** | Unchanged. |
| `DurabilityHeaderWidget::RefreshForActor` 135 lines + slot-updater triplet | **Still open** | Unchanged. |
| `FCombatCapabilities::BuildFrom` 170 lines | **Still open** | Unchanged. |
| TransBuffer-crash exposure in `CombatHUDRoot` | **Still open** | Not verified. |
| `DefensePromptWidget` stub | **Still open** | Unchanged. |

---

## Tier 2 — Medium

| # | Finding | Status |
|---|---|---|
| 9 | `DurabilityHeaderWidget` refactor | Still open |
| 10 | `CombatHUDRoot` TransBuffer-risk verification | Still open |
| 11 | `InfusionVFXComponent` source-cycling extraction | Still open |
| 12 | `ItemExecutor` `Execute*Effect` handler pattern + `CanQuartzTransform` rename | Still open |
| 13 | `CombatGridSubsystem` dead-code BP audit | Still open |

---

## Tier 3 — Small wins

| # | Finding | Status | Evidence |
|---|---|---|---|
| 14 | Delete `Private/SpellElement.cpp` (vestigial empty file) | **Closed** | File no longer exists. |
| 15 | Delete `URingData::GetEvolutionMaxSpells` | **Still open** | Not touched. |
| 15 | Delete `UWeaponData::HasIloditeEquipped` | **Closed** | Function no longer exists in source. Grep returns 0 hits across `Source/`. |
| 16 | Rename `Ilodite → Iolite` | **Closed** | Function gone — typo went with it. |
| 17 | Delete `UStatusEffectManager::CleanupInvalidActors`, `IsStunned`, `IsSilenced`, `IsRooted` | **Partially closed** | `CleanupInvalidActors` removed (verified). Three `Is*` stubs still present. |
| 18 | Delete `UWeatherStateManager::GetLeaderElement/GetTeam0Leader/GetTeam1Leader` | **Still open** | Not touched. |
| 19 | Stale comments cleanup (TurnManager:234, AbilityData:128, ActionStatModifiers:11, DurabilityHeaderWidget:72-77, CharacterData:389/448/475/484) | **Partially closed** | `AbilityData.cpp:128` comment updated — now reads *"StatusMultiplier (Mind stat in Phase 1; moves to Spirit in Phase 2b)"* instead of the stale "Effect Damage multiplier (Spirit stat)" version. Other stale-comment sites not verified individually but `TurnManager:234` likely unchanged. |
| 20 | Fix `// Action Execfutor` typo at `ActionExecutor.h:136` | **Still open** | Verified present. |
| 21 | Rename `BonusAttack/BonusMagicPower/BonusMaxMP` in WeaponData | **Still open** | Not verified individually. |
| 22 | Promote `0.7f` element penalty to `InfusionConstants::ELEMENT_PENALTY` | **Closed** | Grep for `0.7f` in `ActionExecutor.cpp` returns **0 hits**. The 3 sites Pass 1 cited (lines 679, 772, 1851) all sat inside the deleted sync paths. |
| 23 | Promote `InfusionCost = 5` (3 sites) | **Still open / Verify** | Grep for `InfusionCost = 5` returned no pattern hits, but the magic value may still appear without that exact phrase. `ExecuteAttackAsync` at `:800` still carries `int32 InfusionCost = 5; // TODO: from constants`. |
| 24 | Consolidate `STATUS_THRESHOLD_*` (3× same constant) | **Still open** | Unchanged. |

---

## Tier 4 — Bugs (summary)

See "Bugs surfaced" table at top. **Still open:** Bug 2 (Slash fallthrough), Bug 3 (orphan brace block). **Invalidated:** Bug 1 (function removed). **Partially closed:** Bug 4 (cluster reduced to 2 inverted functions).

---

## Cross-subsystem duplication candidates (Pass 1's table)

| # | Candidate | Status |
|---|---|---|
| 1 | Crystal subscription bookkeeping (Ring + Weapon Manager mirrors) | Still open |
| 2 | Validation logic split across 3 files (LoadoutComponent + LoadoutData + FCombatLoadout) | Still open |
| 3 | Find character data + null guard pattern | Still open |
| 4 | `switch` on `EItemTier` (7 cases, 16+ instances) | Still open |
| 5 | `switch` on `EAIDifficulty` (3 ladders) | Still open |
| 6 | `switch` on `ESpellElement` for color/tint | Still open |
| 7 | Status-effect removal loop (6+ near-identical) | Still open |
| 8 | Source-label mapping (4 copies) | Still open |
| 9 | Find-crystal-by-type loop (3 copies) | Still open |
| 10 | Team0/Team1 mirror code in WeatherStateManager | Still open |
| 11 | Status-buildup formula table (Immediate vs Triggered) | Still open |
| 12 | `FWeaponInventoryEntry` / `FRingInventoryEntry` mirrors | Still open |
| 13 | `LoadoutComponent` 5 "delegate to inventory entry" methods | Still open |
| 14 | `DurabilityHeaderWidget` slot updaters (3×) | Still open |
| 15 | TransBuffer-mitigated widget spawn (3 sites) | Still open |

None of the cross-subsystem patterns were touched. They remain as Pass 1 left them.

---

## Out-of-scope items observed in Pass 1

| Item | Status |
|---|---|
| Config-asset vs subsystem-field architecture (DamageCalculator / DefenseSystem / WeaponManager tuning UPROPERTYs) | Unchanged |
| `WeaponMeshComponent` polling instead of binding | Unchanged |
| `ConjureWeapon` system unwired | Unchanged |
| `CombatPlayerController` looks like debug controller | Unchanged |
| `DefensePromptWidget` is a stub | Unchanged |
| TransBuffer-crash exposure in `CombatHUDRoot` | Unverified |
| `WeaponData::Bonus*` pre-rename names | Unchanged |
| AI cannot see `ActionMods` in damage previews | **Worse** — see Item #34 row; AI now also misses `bIsRawMode` / per-asset ability buildup |

---

## New items surfaced by this verification pass (not in Pass 1)

These are findings the verification turned up that Pass 1 didn't see — either because the underlying code hadn't shipped or because Pass 1 missed them. **Flag-only; no fixes proposed here.**

1. **`UActionExecutor::RollCriticalHit`** (`ActionExecutor.cpp:1965`) — defined, zero callers. `UDamageCalculator::RollCriticalHit` is the one in use. Orphan flagged in Phase D commit message; preserved for Phase E sweep.
2. **`UActionExecutor::ApplySpellSizeL2Cost`** — declaration in `ActionExecutor.h:491`, no body, no callers. Pre-existing dead declaration that Pass 1 didn't flag.
3. **`FWeaponAttackResult` + `FOnWeaponAttackExecuted` delegate type + `OnWeaponAttackExecuted` field** (`WeaponManager.h:94, 146, 382`) — declared, never broadcast, never bound. Phase C2 flagged for deletion; remains.
4. **`ApplyAbilityEffects` orphan (Phase D side effect)** — `ActionExecutor.cpp:~3574`. Sync `ExecuteAbility` was its only caller. `FinalizeAsyncAction` has no `EActionType::Ability` post-action branch — so `Ability->Effects[]` (drain, conditional `OnHit`/`OnCrit`/`OnKill`) may not process for any async ability action. Either a pre-existing async-path gap (regression risk) or `Ability->Effects[]` is unused production data. Needs PIE test with a populated `Effects[]` to disambiguate.
5. **Raw-mode dead-after-redirect branch in `ExecuteSpellAsync`** — `if (Spell->bIsRawMode) { SpellStatusType = BurstDamage; }` at `ActionExecutor.cpp:619` is overridden to `None` by the immediately-following `ApplyRawModeRedirect` call. Inline comment flags for Phase E.
6. **`AbilityData.cpp:128` stale-comment claim from Pass 1 was already fixed** between the audit and this verification — comment now correctly reads *"Mind stat in Phase 1; moves to Spirit in Phase 2b."* Counts as a small Pass-1 win.
7. **Working-tree drift** — `.claude/settings.local.json` + `Content/Data/Characters/Data/DA_Character_WaterLord.uasset` + `Content/Data/Spells/Fire/Destruction/DA_Spells_Fire_CometStrike.uasset` have been modified-but-unstaged across multiple sessions. Unrelated to any consolidation work but accumulating. Worth a "clean or commit" call.

---

## Summary

**Closed since Pass 1:**
- Tier 1: ActionExecutor sync paths retired (Phase D), WeaponManager attack chain deleted (Phase C2), Orchestrator debug methods migrated to async (Phase D), `0.7f` element penalty constant eliminated.
- Tier 3: `SpellElement.cpp`, `HasIloditeEquipped`, `Ilodite` typo (all 3 went with the function).
- Bug 1: `CalculateEvolutionCost` removed entirely.

**Partially closed:**
- ActionExecutor LOC down 226. ApplyCommitCosts + ExecuteActionAsync still long.
- StatusEffectManager — `CleanupInvalidActors` gone; sub-feature split + duplicate switches untouched.
- Bug 4 — cluster reduced to 2 genuinely-inverted functions.
- Tier 3 stale-comment sweep — at least `AbilityData.cpp:128` confirmed updated.

**Still open (no movement):**
- ApplyCommitCosts decomposition (Tier 1).
- StatusEffectManager mirror-switches + status-bar split (Tier 1).
- AIDecisionManager monolith + duplicates (Tier 1).
- CombatCommandMenuSubsystem infusion-controls dedup (Tier 1).
- LoadoutComponent validator (Tier 1, with persistent orphan-block bug).
- Bugs 2 and 3.
- Most cross-subsystem candidates (15 of 15).

**Worse:**
- AIDecisionManager / `AbilityData.StatusBuildup` gap — AI value-estimation is now further out of sync with runtime than at Pass 1, because runtime gained a new field AI doesn't read.

**Invalidated:**
- Bug 1 (function removed).
- Tier 1 ActionExecutor sync/async duplication (sync path is gone).
- Tier 1 WeaponManager `ExecuteAttackWithInfusion` (deleted in C2).

**New findings (Pass 1 missed):**
- `RollCriticalHit` orphan in ActionExecutor.
- `ApplySpellSizeL2Cost` body-less declaration.
- `FWeaponAttackResult` triad orphan in WeaponManager.
- `ApplyAbilityEffects` orphan side-effect from Phase D — possible production regression depending on whether `Ability->Effects[]` is populated.
- Dead-after-redirect raw-mode branch in `ExecuteSpellAsync`.
- Working-tree drift across 3 files.

The consolidation arc cleared a substantial slice of Tier 1 #1, #5, and #8. The biggest remaining structural items — `ApplyCommitCosts`, AI monolith, validation triplication, mirror-switches in StatusEffectManager — are untouched and still warrant Pass-2 deep-dives.
