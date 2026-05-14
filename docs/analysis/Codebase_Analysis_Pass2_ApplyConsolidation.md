# Pass 2 — `ApplyHit` Consolidation Audit

Read-only audit of every site in `Source/world_of_refraction/` that applies damage or status buildup to a target. The goal is a single applicator:

```cpp
FCombatHitResult ApplyHit(AActor* Attacker, AActor* Target, const FActionHitInput& Input);
```

Orchestrators (`ExecuteAttack*`, `ExecuteAbility*`, `ExecuteSpell*`, `ExecuteAttackWithInfusion`) stay separate. The applicator is what unifies.

Pass 1 reference: `docs/analysis/Codebase_Analysis_Pass1_Map.md`. All caller claims here verified by Grep — Pass 1 contained at least one wrong-callers claim (`IsStunned`/`IsSilenced` are live at `ActionExecutor.cpp:208,218`).

The brief's note on sync vs async is honored: sync paths (`ExecuteSpell` line 912, `ExecuteAbility` line 1553, `ExecuteAttack` line 1774) are flagged as legacy/test, not dwelt on. Async (`ExecuteSpellAsync` 516, `ExecuteAbilityAsync` 637, `ExecuteAttackAsync` 744) is treated as production.

---

## Section 1 — Damage applicator inventory

A "damage applicator" here = the final step that mutates target HP via `UCharacterDataComponent::ServerTakeDamage` (the only HP-decrement primitive — see `CharacterDataComponent.cpp:106`). Excludes: regen, healing, self-damage costs, DoT ticks (excluded per brief).

### 1.1 `UActionExecutor::ApplyDamage`
- **Signature**: `FCombatHitResult ApplyDamage(AActor* Attacker, AActor* Target, int32 BaseDamage, bool bIsElemental, ESpellElement Element, bool bCanCrit)` — `ActionExecutor.h:263`, body at `ActionExecutor.cpp:1980`.
- **Inline overload**: same with defaults (no element, can crit) — `ActionExecutor.h:272`.
- **Inputs**: parameters above; reads `CurrentExecutionContext->ActionMods` (`ActionExecutor.cpp:2015-2018`); reads `UDamageCalculator` subsystem; reads `UCharacterDataComponent::CurrentHP`.
- **Outputs**: `FCombatHitResult` (`Target`, `DamageDealt`, `bWasCritical`, `bTargetDied`); calls `TargetComp->ServerTakeDamage(CalcResult.FinalDamage)` (`ActionExecutor.cpp:2026`); broadcasts `OnDamageDealt` (`ActionExecutor.cpp:2044`).
- **Defense pipeline**: BYPASSES the windowed defense flow — caller is expected to either (a) be downstream of `ApplyDamageAfterDefense` already, or (b) deliberately ignore defenses (instant spell, beam tick, fallback). It does run through `UDamageCalculator::CalculateDamage`, which subtracts flat defense and resistance internally (`DamageCalculator.cpp:106-121`).
- **Callers (verified by Grep on `Source/world_of_refraction/`)**:
  - `ProcessMultiHit` at `ActionExecutor.cpp:2243` (loop: per-hit).
  - `ResolveInstantSpell` at `ActionExecutor.cpp:2620` — instant spells, no defense window.
  - `SpawnAOEEffect` fallback at `ActionExecutor.cpp:2576` — only when `DefenseSystem` unavailable.
  - `OnProjectileImpact` fallback at `ActionExecutor.cpp:2674` — when `DefenseSystem` unavailable.
  - `OnBeamTick` at `ActionExecutor.cpp:2707` — beam DoT ticks (continuous, no window).

### 1.2 `UActionExecutor::ApplyDamageAfterDefense`
- **Signature**: `void ApplyDamageAfterDefense(AActor* Attacker, AActor* Target, const FPendingDefenseContext& Context, const FDefenseResult& DefenseResult)` — `ActionExecutor.h:466`, body at `ActionExecutor.cpp:1177`.
- **Inputs**: pending context (`HitCount`, `bIsElemental`, `Element`, `bCanCrit`, `BaseDamage`); `FDefenseResult.FinalDamage` (already reduced by Block/Parry); `CurrentExecutionContext->PartialResult`.
- **Outputs**: side-effects via `ProcessMultiHit` → `ApplyDamage` → `ServerTakeDamage`; mutates `PartialResult.TotalDamageDealt`, `DamagePerTarget`, `AffectedTargets`, `bCausedDeath`; broadcasts `OnTargetKilled` (`ActionExecutor.cpp:1222`).
- **Defense pipeline**: This is THE consumer of the defense outcome. Dodge → 0 damage (line 1190-1194). Otherwise routes pre-reduced damage through `ProcessMultiHit`.
- **Callers**: `OnDefenseWindowClosed` at `ActionExecutor.cpp:1130`; timeout failsafe at `ActionExecutor.cpp:1425`.

### 1.3 `UActionExecutor::ProcessMultiHit`
- **Signature**: `int32 ProcessMultiHit(AActor* Attacker, AActor* Target, int32 DamagePerHit, int32 HitCount, bool bIsElemental, ESpellElement Element, bool bCanCrit, FActionResult& OutResult)` — `ActionExecutor.h:485`, body at `ActionExecutor.cpp:2228`.
- **Inputs**: per-hit params; `OutResult` accumulator.
- **Outputs**: returns total damage; mutates `OutResult.bWasCritical`; loops `ApplyDamage` per hit; breaks if target died.
- **Defense pipeline**: caller-determined. Reachable from both `ApplyDamageAfterDefense` (post-defense) and the no-defense `OpenDefenseWindowsForTargets` fallback at `ActionExecutor.cpp:1065`.
- **Callers**: `ApplyDamageAfterDefense` (1202), defense-window fallback (1065), legacy `ExecuteSpell` (981), legacy `ExecuteAbility` (1637), legacy `ExecuteAttack` (1871).

### 1.4 `UWeaponManager::ApplyWeaponDamage`
- **Signature**: `int32 ApplyWeaponDamage(AActor* Attacker, AActor* Target, int32 BaseDamage, bool bIsElemental, ESpellElement Element, bool bCanCrit, FWeaponAttackResult& OutResult)` — body at `WeaponManager.cpp:719`. Header is private.
- **Inputs**: as above; queries `UCharacterData::CalculateFlatDefense/CalculateResistance`; queries `UStatusEffectManager::GetTotalStatModifier` for DefenseBuff/Debuff and CritChanceBuff/Debuff.
- **Outputs**: returns final damage; calls `TargetComp->ServerTakeDamage(FinalDamage)` (`WeaponManager.cpp:783`); mutates `OutResult.bWasCritical`.
- **Defense pipeline**: BYPASSES windowed defense. Computes its OWN flat defense + resistance + crit roll inline (`WeaponManager.cpp:730-777`). This is the duplicate of `UDamageCalculator::CalculateDamage`, hand-rolled inside WeaponManager.
- **Callers**: `WeaponManager::ExecuteAttackWithInfusion` per-target loop at `WeaponManager.cpp:470`. That orchestrator is reached from sync `UActionExecutor::ExecuteAttack` at line 1790 when `Attack==nullptr` — i.e., the "use the equipped weapon" entry point. `ExecuteAttackAsync` does NOT route through WeaponManager.

### 1.5 `UItemExecutor::ExecuteDamageEffect`
- **Signature**: `void ExecuteDamageEffect(AActor* User, AActor* Target, UItemData* Item, FItemUseResult& OutResult)` — `ItemExecutor.cpp:143`.
- **Inputs**: `Item->GetDamageValue()`; `Target` HP via `UCharacterDataComponent`.
- **Outputs**: `TargetComp->ServerTakeDamage(FMath::RoundToInt(Damage))` (`ItemExecutor.cpp:156`); accumulates into `OutResult.DamageDealt`; may schedule a DOT via `ApplySecondaryEffect` (line 163, S-tier Garnet burn — burn DoT ticks excluded by brief).
- **Defense pipeline**: BYPASSES — items are unavoidable per current design.
- **Callers**: `UItemExecutor::UseItem` flow (item executor pipeline, not in scope to map exhaustively). Reached from `UActionExecutor::ExecuteItem` via `ItemExec->UseItem` (`ActionExecutor.cpp:1747`).

### 1.6 `UDefenseSystem::ApplyReflectedDamage`
- **Signature**: `void ApplyReflectedDamage(AActor* Attacker, int32 Damage)` — `DefenseSystem.cpp:461`.
- **Inputs**: `Damage` (already computed inside `CalculateDefenseResult` for parry, line 347).
- **Outputs**: `Comp->ServerTakeDamage(Damage)` on the original attacker (`DefenseSystem.cpp:471`); broadcasts `OnParryReflect` (line 174).
- **Defense pipeline**: this IS the parry-reflect leg. The reflect target (the original attacker) does not get a defense window opened on the reflect — it eats it raw.
- **Callers**: `UDefenseSystem::CloseDefenseWindow` at `DefenseSystem.cpp:173`.

### 1.7 `UBrokenDarknessManager::ApplyDamageToActor`
- **Signature**: `void ApplyDamageToActor(AActor* Target, float Damage)` — `BrokenDarknessManager.cpp:497`, declared `BrokenDarknessManager.h:266`.
- **Inputs**: `Damage` float.
- **Outputs**: `CharComp->ServerTakeDamage(FMath::RoundToInt(Damage))` (line 507).
- **Defense pipeline**: BYPASSES — this is overload aura / forbidden-cast backlash. Conceptually adjacent to DoT (excluded class).
- **Callers (verified)**:
  - `ProcessForbiddenCast` at `BrokenDarknessManager.cpp:273` — self-damage on forbidden element cast.
  - `ProcessOverloadTick` at lines 469 (aura on enemy) and 479 (self).
- **Note**: the enemy-aura call at line 469 *is* technically combat damage to a target, but it's a per-tick effect from a status (overload), so it sits in the same bucket as DoT and is excluded per brief. The self-damage calls (273, 479) are excluded as self-damage costs.

### 1.8 `UStatusEffectManager::ApplyEffectLogic` (DOT case)
- `StatusEffectManager.cpp:881`: `CharComp->ServerTakeDamage(FMath::RoundToInt(Value))` for `EStatusType::DOT`.
- **EXCLUDED per brief** — DoT ticks. Listed for completeness so it is not mistaken for a missing applicator.

### 1.9 `UStatusEffectManager::ApplyTriggeredStatus` (BurstDamage)
- `StatusEffectManager.cpp:1701`: `TargetComp->ServerTakeDamage(BurstDamage)` when status bar fills with `EStatusType::BurstDamage` (raw-mode spell).
- This is a damage application but it fires from inside the buildup pipeline, not from an orchestrator. Treat as a **buildup-trigger consequence**, not a separately-callable damage applicator. Listed here so it isn't double-counted in Section 2.

### 1.10 Debug / test sites (intentionally bypass everything)
- `ActionExecutor::ApplySelfDamage` `:3146` — infusion HP cost. Excluded (self-damage cost).
- `ActionExecutor::ApplyHPCostInternal` `:4247` — HP cost via `ServerTakeDamage`. Excluded (self-damage cost).
- `ItemExecutor::ExecuteEnergyRestoreEffect` `:208` — Citrine self-damage. Excluded.
- `CombatOrchestrator::DebugDamageTeam0/1` `:1304,1313`, `DebugKillActor` `:1359` — debug only.
- `TurnManagerTestActor.cpp:322`, `Testing/HUDTestActor.cpp:149` — test actors.
- `StatusEffectManagerTestActor.cpp:565` — test.

**Damage applicator total in scope: 6** — `ApplyDamage`, `ApplyDamageAfterDefense`, `ProcessMultiHit`, `ApplyWeaponDamage`, `ExecuteDamageEffect`, `ApplyReflectedDamage`. (`ApplyDamageToActor` overload-aura excluded as DoT-tier; bursts/DOTs excluded; debug excluded.)

---

## Section 2 — Status buildup applicator inventory

Every site that pushes value into the unified status bar (`UStatusEffectManager::AddStatusBuildup`, `StatusEffectManager.cpp:1417`) or applies a discrete status effect `FStatusEffect` directly during a hit. Excludes status removal, post-buildup triggered effects, and self-status from infusion costs (per brief).

### 2.1 `UStatusEffectManager::AddStatusBuildup` (the primitive)
- **Signature**: `bool AddStatusBuildup(AActor* Source, AActor* Target, float Amount, EStatusType StatusType, ESpellElement Element)` — `StatusEffectManager.h:375`, body at `StatusEffectManager.cpp:1417`.
- **Inputs**: parameters; reads `Target`'s `CharacterData->CalculateResistance()` (line 1431) — buildup amount is reduced by resistance HERE, before the bar moves.
- **Outputs**: returns `true` if threshold crossed; broadcasts `OnStatusBuildupChanged` (1445); on threshold calls `TriggerStatusEffect` then `ResetStatusBar` (1457-1458).
- **Defense pipeline**: NONE — buildup never reads the defense outcome. Reduces only by `Resistance` stat (passive). Does not get reduced by Block/Parry/Dodge.
- **Callers** (verified by Grep):
  - `UWeaponManager::ApplyWeaponStatusBuildup` `WeaponManager.cpp:924`
  - `UActionExecutor::ApplySpellStatusBuildup` `ActionExecutor.cpp:3691,3734` (raw-mode and elemental branches)
  - `CombatOrchestrator::DebugApplyStatusBuildup` `CombatOrchestrator.cpp:1342,1345` (debug)
  - `Testing/HUDTestActor.cpp:208` (test)

### 2.2 `UActionExecutor::ApplySpellStatusBuildup`
- **Signature**: `void ApplySpellStatusBuildup(AActor* Caster, AActor* Target, USpellData* Spell, int32 InfusionLevel)` — `ActionExecutor.h:649`, body at `ActionExecutor.cpp:3666`.
- **Inputs**: `Spell->bIsRawMode`, `Spell->StatusBuildup`, `Spell->PrimaryEffect`, `Spell->Element`, `InfusionLevel`.
- **Outputs**: calls `StatusManager->AddStatusBuildup` (raw-mode path 3691, elemental path 3734); on elemental, also calls `StatusManager->ApplyImmediateStatus` (line 3750) — applies a weak on-hit status effect. L1 infusion = +50% buildup (3686, 3727).
- **Defense pipeline**: NONE. Called BEFORE defense windows open (`ActionExecutor.cpp:614-617`, in `ExecuteSpellAsync`). Buildup applies even if the attack is later dodged.
- **Callers**: `ExecuteSpellAsync` at `ActionExecutor.cpp:616`; legacy sync `ExecuteSpell` at `:1005`.

### 2.3 `UWeaponManager::ApplyWeaponStatusBuildup`
- **Signature**: `void ApplyWeaponStatusBuildup(AActor* Attacker, AActor* Target, UWeaponData* Weapon, UWeaponAttackData* Attack, int32 InfusionLevel)` — `WeaponManager.h:417` (private), body at `WeaponManager.cpp:877`.
- **Inputs**: `Attack->PhysicalDamageType` (Slash/Pierce → DOT bleed; Impact → DefenseDebuff = armor break), `Attack->StatusBuildup`, `Weapon`, weapon's infusion-active state from `WeaponStates` (line 910), attacker's `InnateElement` if infusion active.
- **Outputs**: calls `StatusManager->AddStatusBuildup` (line 924) and `StatusManager->ApplyImmediateStatus` (line 940). L1 infusion = +50% buildup.
- **Defense pipeline**: NONE. Called from `ExecuteAttackWithInfusion` AFTER `ApplyWeaponDamage` per target (`WeaponManager.cpp:495`), so it follows the damage but does not consume any defense outcome. There is no defense window in WeaponManager's path at all.
- **Callers**: `WeaponManager::ExecuteAttackWithInfusion` at `:495`.

### 2.4 `UActionExecutor::ApplyAbilityInfusionStatus` (STUB)
- **Signature**: `void ApplyAbilityInfusionStatus(AActor* User, const TArray<AActor*>& Targets, EInfusionSourceOption Source, int32 HitCount, float StatusMultiplier)` — `ActionExecutor.h:533`, body at `ActionExecutor.cpp:3251`.
- **State per comments at 3266 (`// Physical source - TODO: Integrate with WeaponManager when API is available`) and 3279 (`// TODO: Integrate with StatusEffectManager when API is available`)**: this function CURRENTLY ONLY LOGS. It does NOT call `AddStatusBuildup`, does not mutate any state. Confirmed by Grep — no `StatusManager` field is touched in the body.
- **Inputs**: source option, hit count, multiplier.
- **Outputs**: `UE_LOG` only.
- **Defense pipeline**: N/A — no-op.
- **Callers**: `ExecuteAbility` (sync) at `:1631`; `ExecuteAbilityAsync` at `:720`.
- **Significance for consolidation**: a stub that already has the right shape (User → Targets → magnitude+source) for the unified `ApplyHit` to absorb, with the actual implementation owed.

### 2.5 `UActionExecutor::ApplyStatusEffects` (helper wrapper around `ApplyEffect`)
- **Signature**: `void ApplyStatusEffects(AActor* Source, AActor* Target, EStatusType PrimaryEffect, float PrimaryValue, int32 PrimaryDuration, EStatusType SecondaryEffect, float SecondaryValue, int32 SecondaryDuration, ESpellElement Element)` — `ActionExecutor.h:473`, body at `ActionExecutor.cpp:2184`.
- **Inputs**: spell's `Primary/SecondaryEffect`, magnitudes, durations, element.
- **Outputs**: builds two `FStatusEffect`s and calls `StatusManager->ApplyEffect` (lines 2210, 2224). NOT buildup — direct effect application that bypasses the status bar.
- **Defense pipeline**: NONE. Called post-finalization in `FinalizeAsyncAction` at `ActionExecutor.cpp:1303`; legacy sync path at `:1014`.
- **Callers**: `FinalizeAsyncAction` (1303), legacy `ExecuteSpell` (1014).
- **Note**: this is the second buildup-adjacent path orchestrators use and is the source of the orchestrator-side duplication called out in the brief — alongside the bar buildup, spells also push direct Primary/Secondary effects.

### 2.6 Direct `StatusEffectManager::ApplyEffect` calls from orchestrator surface
Verified callers across `Source/world_of_refraction/`:
- `ActionExecutor::ExecuteDefend` `:1929` — applies `DefenseBuff` to the defender. Self-buff, not a target hit. EXCLUDE.
- `ActionExecutor::ApplyAbilityEffects` `:3985` — applies status effects from `Ability->Effects` array. Reached from sync `ExecuteAbility:1659` only; **NOT** wired into `ExecuteAbilityAsync`. Confirmed by Grep — `ApplyAbilityEffects` has exactly one caller, line 1659. This is a **legacy-path-only buildup applicator** that the async path silently drops. Flagged as a separate concern.
- `WeaponManager::TriggerPhysicalStatus` `:712` — applies bleed/armor-break/stun. Currently **dead code**: only caller would have been from a buildup-bar trigger but `WeaponManager::ApplyWeaponStatusBuildup` routes through the unified `StatusEffectManager` bar instead. Grep finds no in-source caller of `TriggerPhysicalStatus`. Listed as separate concern.
- `StatusEffectManager::ApplyImmediateStatus` `:1605`, `ApplyTriggeredStatus` `:1714` — internal to status pipeline, called from the buildup bar's threshold logic, not from orchestrators directly.
- `ItemExecutor::*` — many calls (`:242, :270, :298, :336, :393, :460, :519, :572`). Items are out-of-scope for the orchestrator → applicator unification (locked design covers Attack / Ability / Spell / WithInfusion).

**Buildup applicator total in scope: 4** — `AddStatusBuildup` (primitive), `ApplySpellStatusBuildup`, `ApplyWeaponStatusBuildup`, `ApplyAbilityInfusionStatus` (stub). `ApplyStatusEffects` and `ApplyAbilityEffects` are direct-effect helpers, not bar-buildup, but live in the same orchestrator path and are noted because Section 6 needs to address them.

---

## Section 3 — Orchestrator → applicator mapping

Async paths are production. Sync paths are legacy/test. Both listed because the consolidation must not break either until the legacy path is retired.

### 3.1 `UActionExecutor::ExecuteSpellAsync` — `ActionExecutor.cpp:516`
- **Damage**: open windows via `OpenDefenseWindowsForTargets` (line 620). Damage applies later in `ApplyDamageAfterDefense` → `ProcessMultiHit` → `ApplyDamage` → `ServerTakeDamage`.
- **Buildup**: `ApplySpellStatusBuildup` per target at `:616`, BEFORE windows open.
- **Direct-effect**: `ApplyStatusEffects` later in `FinalizeAsyncAction` `:1303`.
- **Atomicity**: ❌ NOT atomic. Buildup at line 616. Damage at the much-later defense callback. Direct status effects in finalization. **Three separate passes per spell.**
- **Multi-applicator flag**: YES — this is the canonical case the consolidation targets.

### 3.2 `UActionExecutor::ExecuteAbilityAsync` — `ActionExecutor.cpp:637`
- **Damage**: `OpenDefenseWindowsForTargets` (line 728) → eventual `ApplyDamageAfterDefense` → `ProcessMultiHit` → `ApplyDamage`.
- **Buildup**: `ApplyAbilityInfusionStatus` at `:720` (stub — see 2.4) — only when `bIsInfused && StatusMultiplier > 0`.
- **Direct-effect**: NONE. `ApplyAbilityEffects` is **NOT called** from the async path. Sync `ExecuteAbility:1659` is the only caller — async users lose `Ability->Effects`. Separate concern.
- **Atomicity**: ❌ NOT atomic. Buildup stub call, then defense-window open, then async damage. (Stub is currently a no-op so functionally only damage runs.)
- **Multi-applicator flag**: YES (would be once the stub does work).

### 3.3 `UActionExecutor::ExecuteAttackAsync` — `ActionExecutor.cpp:744`
- **Damage**: `OpenDefenseWindowsForTargets` (line 817) → defense → `ProcessMultiHit` → `ApplyDamage`.
- **Buildup**: NONE in the async path. Confirmed by Grep — no `Apply*StatusBuildup` call between line 744 and 831. **The async attack path leaks weapon physical-status buildup.** Separate concern.
- **Direct-effect**: NONE.
- **Atomicity**: damage-only path, atomic per target.
- **Multi-applicator flag**: NO — but this is because of a *missing* applicator, not by design.

### 3.4 `UWeaponManager::ExecuteAttackWithInfusion` — `WeaponManager.cpp:396`
- **Damage**: `ApplyWeaponDamage` per hit per target at `:470` — its own defense calc inline.
- **Buildup**: `ApplyWeaponStatusBuildup` per target at `:495`.
- **Atomicity**: ✅ atomic per target — damage loop completes, then buildup, then next target.
- **Multi-applicator flag**: YES — calls both damage (`ApplyWeaponDamage`) and buildup (`ApplyWeaponStatusBuildup`) sequentially per target. Reached from sync `ExecuteAttack:1790` (delegation when `Attack==nullptr`).

### 3.5 Sync `UActionExecutor::ExecuteSpell` — `ActionExecutor.cpp:912` (legacy/test)
- **Damage**: per target via `ProcessMultiHit:981` → `ApplyDamage`.
- **Buildup**: `ApplySpellStatusBuildup` per target at `:1005` (separate loop after damage loop).
- **Direct-effect**: `ApplyStatusEffects` per target at `:1014` (third loop).
- **Atomicity**: ❌ NOT atomic — three sequential passes over the same `ValidTargets` array.
- **Multi-applicator flag**: YES.

### 3.6 Sync `UActionExecutor::ExecuteAbility` — `ActionExecutor.cpp:1553` (legacy/test)
- **Buildup**: `ApplyAbilityInfusionStatus:1631` (stub).
- **Damage**: `ProcessMultiHit:1637` per target.
- **Direct-effect**: `ApplyAbilityEffects:1659`.
- **Atomicity**: ❌. Three sequential passes (one of which is a no-op stub).
- **Multi-applicator flag**: YES.

### 3.7 Sync `UActionExecutor::ExecuteAttack` — `ActionExecutor.cpp:1774` (legacy/test)
- If `Attack==nullptr`: delegates to `WeaponManager::ExecuteAttackWithInfusion` at `:1790`. See 3.4.
- Else: `ProcessMultiHit:1871` per target. **No buildup call in this branch** — same async-attack leak as 3.3.
- **Multi-applicator flag**: NO in the explicit-attack branch (damage-only); YES via WeaponManager delegation.

### 3.8 Summary table

| Orchestrator                              | Damage call                                                           | Buildup call                                                     | Atomic per target? | Multi-applicator?      |
| ----------------------------------------- | --------------------------------------------------------------------- | ---------------------------------------------------------------- | ------------------ | ---------------------- |
| `ExecuteSpellAsync` :516                  | `OpenDefenseWindowsForTargets` :620 → `ApplyDamageAfterDefense` :1130 | `ApplySpellStatusBuildup` :616                                   | ❌ separate phases  | ✅                      |
| `ExecuteAbilityAsync` :637                | `OpenDefenseWindowsForTargets` :728                                   | `ApplyAbilityInfusionStatus` :720 (stub)                         | ❌                  | ✅ (latent)             |
| `ExecuteAttackAsync` :744                 | `OpenDefenseWindowsForTargets` :817                                   | **MISSING**                                                      | n/a                | ❌ (leak)               |
| `ExecuteAttackWithInfusion` (Weapon) :396 | `ApplyWeaponDamage` :470                                              | `ApplyWeaponStatusBuildup` :495                                  | ✅                  | ✅                      |
| Sync `ExecuteSpell` :912                  | `ProcessMultiHit` :981                                                | `ApplySpellStatusBuildup` :1005 + `ApplyStatusEffects` :1014     | ❌ three passes     | ✅                      |
| Sync `ExecuteAbility` :1553               | `ProcessMultiHit` :1637                                               | `ApplyAbilityInfusionStatus` :1631 + `ApplyAbilityEffects` :1659 | ❌                  | ✅                      |
| Sync `ExecuteAttack` :1774                | `ProcessMultiHit` :1871 (or WeaponMgr delegation)                     | none in direct branch                                            | n/a                | ❌ direct / ✅ delegated |

---

## Section 4 — Defense pipeline integration

### 4.1 Window opening
The async paths all open windows via `UActionExecutor::OpenDefenseWindowsForTargets` (`ActionExecutor.cpp:1039`), which calls `UDefenseSystem::OpenDefenseWindow` per target (line 1094). Sync paths open NO windows — `OnDefenseWindowRequested` broadcast at `:977` (sync `ExecuteSpell`) is dead-letter (no DefenseSystem subscriber listens to that delegate; DefenseSystem subscribes to its own `OpenDefenseWindow` API, not the executor's broadcast).

### 4.2 Window resolution
`UDefenseSystem::CloseDefenseWindow` builds an `FDefenseResult` via the static `CalculateDefenseResult` (`DefenseSystem.cpp:315`). Reductions:

| Defense                  | Damage outcome                          | Reflect                   |
| ------------------------ | --------------------------------------- | ------------------------- |
| None / missed timing     | `FinalDamage = BaseDamage`              | 0                         |
| Block (success)          | `FinalDamage = round(BaseDamage * 0.5)` | 0                         |
| Parry (success)          | `FinalDamage = round(BaseDamage * 0.3)` | `round(BaseDamage * 0.3)` |
| Dodge (size < threshold) | `FinalDamage = 0`                       | 0                         |
| Dodge (too large)        | `FinalDamage = BaseDamage` (fail)       | 0                         |

These hard-coded multipliers contradict the editable `BlockReduction = 0.5f / ParryReduction = 0.7f / ParryReflect = 0.3f` UPROPERTYs in `DefenseSystem.h:299-308`. Separate concern.

### 4.3 Reflect leg
Parry reflect: `CloseDefenseWindow` calls `ApplyReflectedDamage(Attacker, ReflectedDamage)` at `DefenseSystem.cpp:173`. The reflect target (original attacker) does NOT get its own defense window opened — it eats the reflect raw via `ServerTakeDamage:471`.

### 4.4 Damage consumer
`UActionExecutor::OnDefenseWindowClosed` (`:1107`) is the only `UFUNCTION` subscriber to `OnDefenseWindowClosed`. It calls `ApplyDamageAfterDefense` (`:1130`), which on dodge-success returns 0 damage (line 1190-1194), otherwise feeds `DefenseResult.FinalDamage` into `ProcessMultiHit`.

### 4.5 Buildup vs defense — **CRITICAL ANSWER**

**In the current code, buildup is NOT reduced when the attack is blocked, parried, or dodged.**

Evidence:
1. In `ExecuteSpellAsync`, `ApplySpellStatusBuildup` is called at line 616, BEFORE `OpenDefenseWindowsForTargets` at line 620. There is no later step that subtracts buildup based on `FDefenseResult`.
2. `AddStatusBuildup` (`StatusEffectManager.cpp:1417`) reduces only by `Resistance` (line 1431) — a passive stat, not a defense outcome.
3. `OnDefenseWindowClosed` and `ApplyDamageAfterDefense` never touch the status bar.
4. On a successful dodge, the spell deals 0 damage but the buildup applied at line 616 stays. A target who dodges three poison spells in a row still procs DOT.

The locked design says block → buildup reduced by Resistance, parry → reduced significantly more, dodge → cancels both. **Implementing this is part of the consolidation work**, not a faithful preservation of current behavior.

For weapon attacks the answer is the same but for a different reason: `ApplyWeaponStatusBuildup` is called from `ExecuteAttackWithInfusion:495` which has no defense window at all.

### 4.6 Defense-skip applicators
- `ResolveInstantSpell:2580` — instant spells, by design no window.
- `OnBeamTick:2694` — continuous DoT, no per-tick window.
- `BrokenDarknessManager::ApplyDamageToActor:497` — overload aura / forbidden-cast self-damage.
- `StatusEffectManager::ApplyEffectLogic:881` (DOT) and `ApplyTriggeredStatus:1701` (BurstDamage) — post-buildup consequences.
- `DefenseSystem::ApplyReflectedDamage:471` — parry reflect (no second window).
- `ItemExecutor::ExecuteDamageEffect:156` — items unavoidable.

---

## Section 5 — Per-site quirks to preserve

| Applicator                          | Quirks unification must preserve                                                                                                                                                                                                                                                                                                                                                                                                                  |
| ----------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `ApplyDamage` (1980)                | Reads `CurrentExecutionContext->ActionMods` for damage scaling (Reality + Evolution + future stat sources). Routes through `UDamageCalculator` so grid-position modifiers, BD stack mults, status-effect damage modifiers, and element interactions all apply. Broadcasts `OnDamageDealt` for floating-numbers UI. Crit-hit detection is per-call.                                                                                                |
| `ApplyDamageAfterDefense` (1177)    | Treats Dodge specially (0 damage short-circuit). Splits `DefenseResult.FinalDamage` over `Context.HitCount` for multi-hit defense reduction. Updates `PartialResult.TotalDamageDealt / DamagePerTarget / AffectedTargets / bCausedDeath`. Broadcasts `OnTargetKilled`. Builds an `FCombatHitResult` locally with `bWasBlocked / bWasParried / bWasDodged` flags but currently DOES NOT store them in `PartialResult` — TODO comment at line 1234. |
| `ProcessMultiHit` (2228)            | Per-hit independent crit roll. Early-out on target death (line 2254). Aggregates `bWasCritical` into `OutResult`.                                                                                                                                                                                                                                                                                                                                 |
| `ApplyWeaponDamage` (719)           | Hand-rolled defense math (flat defense + resistance + crit) duplicating `UDamageCalculator`. Reads status-effect modifiers for DefenseBuff/Debuff and CritChanceBuff/Debuff. Caps resistance at 0.8. Min-damage clamp of 1. Used only when sync `ExecuteAttack` runs without explicit `Attack`.                                                                                                                                                   |
| `ApplyReflectedDamage` (461)        | Logs to `[DefenseSystem]` channel. No defense calc on reflect.                                                                                                                                                                                                                                                                                                                                                                                    |
| `ExecuteDamageEffect` (143)         | Calls `Item->GetDamageValue()`. May chain `ApplySecondaryEffect` for S-tier burns. Logs to `[ItemExecutor]`.                                                                                                                                                                                                                                                                                                                                      |
| `AddStatusBuildup` (1417)           | Reduces by `Resistance` stat. Updates `StatusBarStates[Target]` (per-target FStatusBarState with PendingStatus/Element/LastSource/TurnsSinceLastHit). Broadcasts `OnStatusBuildupChanged`. Threshold check fires `TriggerStatusEffect` then `ResetStatusBar`.                                                                                                                                                                                     |
| `ApplySpellStatusBuildup` (3666)    | Branches on `Spell->bIsRawMode` (raw → BurstDamage type) vs elemental (PrimaryEffect → DOT fallback). L1 infusion = +50%. Calls `ApplyImmediateStatus` for non-burst elementals.                                                                                                                                                                                                                                                                  |
| `ApplyWeaponStatusBuildup` (877)    | PhysicalDamageType → status type (Slash/Pierce → DOT, Impact → DefenseDebuff). L1 infusion = +50%. Element resolves from attacker's `InnateElement` if `WeaponState.bInfusionActive`. Calls `ApplyImmediateStatus` for non-None types.                                                                                                                                                                                                            |
| `ApplyAbilityInfusionStatus` (3251) | Stub. Reads source element via `GetElementForSourceOption`. Hardcoded base `10 * HitCount` and TODO for CombatConstants source. Unification can absorb this and finally implement it.                                                                                                                                                                                                                                                             |
| `ApplyStatusEffects` (2184)         | Builds two ad-hoc `FStatusEffect`s with `FMath::Rand()` IDs. Element propagates. Source string `"Action"`.                                                                                                                                                                                                                                                                                                                                        |
| `ApplyAbilityEffects` (3865)        | Walks `Ability->Effects` array, condition-gated by `EPassiveTrigger` (Always/OnHit/OnCrit/OnKill — verified at lines 3900-3920). Drain effects (HealthRestore/EnergyRestore) heal/restore *attacker*, not target — see migration risk. Currently sync-only path.                                                                                                                                                                                  |

Cross-cutting:
- All damage paths broadcast `OnDamageDealt` (1980 path) or `OnHealingDone` (drain path). `ApplyHit` must replicate.
- Multi-hit semantics differ: `ProcessMultiHit` does N separate crit rolls; `ApplyWeaponDamage` (called inside a per-hit loop in `ExecuteAttackWithInfusion:468`) also rolls per-hit.
- `OnTargetKilled` only fires from `ApplyDamageAfterDefense:1222` and the legacy paths. `ApplyDamage` itself sets `bTargetDied` but does not broadcast — orchestrator does.

---

## Section 6 — Consolidation verdict per site

| #    | Function                                 | file:line                       | Verdict                     | Notes                                                                                                                                                                                                                                                                                                        |
| ---- | ---------------------------------------- | ------------------------------- | --------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 1.1  | `ApplyDamage` (UFUNCTION)                | `ActionExecutor.cpp:1980`       | **WRAPPER**                 | Keep as a thin BP-callable wrapper that builds an `FActionHitInput` (no buildup, full crit) and delegates to `ApplyHit`. Preserves UFUNCTION/BP exposure for any blueprints already binding it.                                                                                                              |
| 1.1b | `ApplyDamage` inline overload            | `ActionExecutor.h:272`          | **COLLAPSE**                | Trivial pass-through; folds into wrapper above.                                                                                                                                                                                                                                                              |
| 1.2  | `ApplyDamageAfterDefense`                | `ActionExecutor.cpp:1177`       | **COLLAPSE WITH MIGRATION** | Logic becomes the post-defense branch of `ApplyHit` (Dodge zeroes out, otherwise consume `DefenseResult.FinalDamage`). Caller (`OnDefenseWindowClosed`) needs to populate `FActionHitInput` from the pending context + defense result.                                                                       |
| 1.3  | `ProcessMultiHit`                        | `ActionExecutor.cpp:2228`       | **COLLAPSE WITH MIGRATION** | Multi-hit becomes an internal loop in `ApplyHit` (N calls of the unified core, each with its own crit roll), or stays as an orchestrator-side helper that calls `ApplyHit` per hit. Either way the per-hit `ApplyDamage` call is replaced.                                                                   |
| 1.4  | `ApplyWeaponDamage`                      | `WeaponManager.cpp:719`         | **COLLAPSE WITH MIGRATION** | Drop hand-rolled defense math — `UDamageCalculator` already does the same thing better. WeaponManager either calls `ApplyHit` directly or `ExecuteAttackWithInfusion` is rewritten to fan out via `ApplyHit`.                                                                                                |
| 1.5  | `ExecuteDamageEffect`                    | `ItemExecutor.cpp:143`          | **KEEP SEPARATE**           | Items are out-of-scope per locked design (orchestrators listed are Attack/Ability/Spell/WithInfusion). Item path has different responsibility (reads `UItemData`, has Quartz absorption hook, no defense window by design). Could WRAPPER later if items get folded in, but not in this consolidation.       |
| 1.6  | `ApplyReflectedDamage`                   | `DefenseSystem.cpp:461`         | **KEEP SEPARATE**           | Reflect is a defense leg, not a hit application by an attacker action. Lives at a different layer (DefenseSystem reflects back). Could be migrated to call `ApplyHit` with a synthesised `FActionHitInput` later, but doing so would couple DefenseSystem to ActionExecutor unnecessarily. Cleaner to leave. |
| 1.7  | `ApplyDamageToActor` (BD)                | `BrokenDarknessManager.cpp:497` | **KEEP SEPARATE**           | Self-damage / aura tick. Different responsibility (DoT-class), excluded by brief from the unification scope.                                                                                                                                                                                                 |
| 2.1  | `AddStatusBuildup` (primitive)           | `StatusEffectManager.cpp:1417`  | **KEEP SEPARATE**           | This is the bar primitive. `ApplyHit` should call it. Don't collapse — it's the implementation, not a duplicate.                                                                                                                                                                                             |
| 2.2  | `ApplySpellStatusBuildup`                | `ActionExecutor.cpp:3666`       | **COLLAPSE WITH MIGRATION** | Logic (raw vs elemental, L1 buff, default-to-DOT, ApplyImmediateStatus on-hit) becomes the elemental-buildup branch of `ApplyHit`. `ExecuteSpellAsync` populates `FActionHitInput.StatusBuildup / StatusToBuild / Element` from `USpellData`.                                                                |
| 2.3  | `ApplyWeaponStatusBuildup`               | `WeaponManager.cpp:877`         | **COLLAPSE WITH MIGRATION** | Logic (PhysicalDamageType → status type, L1 buff, ApplyImmediateStatus) becomes the physical-buildup branch of `ApplyHit`. `ExecuteAttackAsync` (currently leaks) and `ExecuteAttackWithInfusion` populate `FActionHitInput` accordingly.                                                                    |
| 2.4  | `ApplyAbilityInfusionStatus`             | `ActionExecutor.cpp:3251`       | **COLLAPSE WITH MIGRATION** | Stub — replace its callers to populate `FActionHitInput.bIsElemental`/`StatusBuildup`/`StatusToBuild` based on source. Implementation becomes part of the unified buildup branch.                                                                                                                            |
| 2.5  | `ApplyStatusEffects` (Primary/Secondary) | `ActionExecutor.cpp:2184`       | **KEEP SEPARATE**           | Direct-effect application, not bar buildup — different shape (`PrimaryValue/Duration` etc.). The locked `FActionHitInput` does not carry these fields. Either leave as-is and orchestrator continues to call it post-`ApplyHit`, or extend the design later. NOT in current scope.                           |
| 2.6  | `ApplyAbilityEffects`                    | `ActionExecutor.cpp:3865`       | **KEEP SEPARATE**           | Walks `Ability->Effects` with passive-trigger conditions (Always/OnHit/OnCrit/OnKill) and drain semantics. Orthogonal to `FActionHitInput`. Same caveat as 2.5. Re-wiring it into the async path is a separate concern (currently legacy-only).                                                              |

Helper-level: `OpenDefenseWindowsForTargets` `:1039`, `OnDefenseWindowClosed` `:1107`, `OpenDefenseWindow` (DefenseSystem) — all **KEEP SEPARATE**. They are defense plumbing, not applicators.

---

## Section 7 — Migration risks and gotchas

1. **Drain-effect semantics inverted.** `UActionExecutor::ApplyAbilityEffects:3941-3970` heals or restores energy on the ATTACKER (the user) using `Result.TotalDamageDealt * Effect.DrainPercent`. If `ApplyHit` merges with anything on this path it must NOT treat the user as the target. The drain runs after damage at the orchestrator level — `ApplyHit` returning `FCombatHitResult.DamageDealt` lets the orchestrator compute drain externally. Risk: if drain is hoisted into `ApplyHit`, it crosses the attacker/target boundary `FActionHitInput` does not model.

2. **Parry reflect is hidden recursion.** Parry reflects damage to the original attacker via `DefenseSystem::ApplyReflectedDamage:471`. If the attacker's defense state is itself in a window… verify it isn't. (Currently the code does not open a window on reflect — eats raw.) When `ApplyHit` is in place, document explicitly that reflect bypasses `ApplyHit` (or migrate it deliberately).

3. **Multi-hit damage division loses precision twice.** `ExecuteSpellAsync:606` does `int32 DamagePerHit = FinalDamage / FMath::Max(1, Spell->HitCount)`, then `ApplyDamageAfterDefense:1200` does the same again on `DefenseResult.FinalDamage / Context.HitCount`. With non-divisible totals you lose 0..N-1 damage per defense outcome. Not a regression to introduce — but `ApplyHit` consolidating means one place to fix.

4. **Async attack path leaks weapon physical-status buildup.** `ExecuteAttackAsync:744-831` does NOT call `ApplyWeaponStatusBuildup`. Sync `ExecuteAttack` via `WeaponManager::ExecuteAttackWithInfusion` does. Consolidation will close this gap, which IS a behaviour change — flag for QA.

5. **Async ability path leaks `Ability->Effects`.** `ExecuteAbilityAsync:637-742` does NOT call `ApplyAbilityEffects`. Sync `ExecuteAbility:1659` does. Same shape as #4 — consolidation may close this gap, also a behaviour change.

6. **`bIsElemental` decided differently per orchestrator.**
   - Async ability: `bIsElemental = bIsInfused` (`ActionExecutor.cpp:677`)
   - Async attack: `bIsInfused = (SelectedSource != None)` (`:770`)
   - Async spell: `bIsElemental = true` always (`:627`)
   - Sync ability: `bIsElementInfused = (SelectedSource != None)` (`:1584`)
  Whoever populates `FActionHitInput.bIsElemental` must follow these rules per orchestrator.

7. **`CurrentExecutionContext->ActionMods` is a hidden dependency.** `ApplyDamage:2015-2018` reads `ActionMods` off the context. If `ApplyHit` is called outside an action context (e.g. parry reflect, instant spell, beam tick paths today), `ActionMods` is default-constructed. Locked design moves `ActionMods` into `FActionHitInput` — explicit is better — but every existing site that relies on the implicit context needs to populate the field at the orchestrator boundary.

8. **`OnDamageDealt` / `OnTargetKilled` broadcasts.** `ApplyDamage` broadcasts `OnDamageDealt` per hit; `ApplyDamageAfterDefense` broadcasts `OnTargetKilled` once when the post-multi-hit total kills. Multi-hit: floating-number UI gets N events but death gets 1. `ApplyHit` must keep that pattern or UI duplicates / misses kill notifications.

9. **Legacy `OnDefenseWindowRequested` delegate.** `UActionExecutor.h:344` defines it; `ExecuteSpell:977` broadcasts it. No active subscriber found by Grep on `Source/world_of_refraction/`. Dead code, but BP code may bind to it — confirm before removing.

10. **`PartialResult` is mutated from many sites.** Async paths thread results through `CurrentExecutionContext->PartialResult`. `ApplyHit` returning a value is fine, but the orchestrator must accumulate into `PartialResult` (`TotalDamageDealt`, `DamagePerTarget`, `AffectedTargets`, `bCausedDeath`, `bWasCritical`, `StatusEffectsApplied`). Easy to miss a field — `ApplyDamageAfterDefense:1228-1235` already builds an `FCombatHitResult` locally then DOESN'T store its `bWasBlocked/Parried/Dodged` flags (TODO comment at line 1234). The locked `FCombatHitResult` exposes them — consolidation should actually save them this time.

11. **Test actors call `ServerTakeDamage` and `AddStatusBuildup` directly.** `Testing/HUDTestActor.cpp:149,208`, `TurnManagerTestActor.cpp:322`, `StatusEffectManagerTestActor.cpp` extensively, `CombatOrchestrator.cpp:1304-1359` debug menu. These are valid bypass paths and should NOT be migrated. Document that test/debug entry points may continue to call the primitives directly.

---

## Section 8 — Implementation phasing

Each phase ends with a PIE-runnable build. Touched-file count per phase respects the project's "≤3 files per change" rule from `CLAUDE.md`.

### Phase A — Introduce types
- New header for `FActionHitInput` and `FCombatHitResult` (move/extend the existing `FCombatHitResult` definition, currently used in `ApplyDamage` return).
- New file: `ApplyHit.cpp` / declaration on `UActionExecutor` (or a freestanding helper subsystem). Implements core damage + buildup + element + crit + dodge-cancel logic by calling existing primitives (`UDamageCalculator::CalculateDamage`, `UCharacterDataComponent::ServerTakeDamage`, `UStatusEffectManager::AddStatusBuildup`, `UStatusEffectManager::ApplyImmediateStatus`).
- No call sites migrated. Existing applicators remain authoritative.
- **Risk**: zero behaviour change. Verify project compiles.

### Phase B — Migrate `ApplyDamageAfterDefense` (the production damage chokepoint)
- Replace the `ProcessMultiHit` call at `ActionExecutor.cpp:1202` with a per-hit `ApplyHit` loop using `FActionHitInput` populated from `Context` + `DefenseResult`.
- `ApplyHit` for damage-only path can pass `StatusBuildup = 0`, `StatusToBuild = None`. Buildup unchanged at this phase.
- Files touched: `ActionExecutor.cpp`, possibly `ActionExecutor.h`. Stays under the 3-file budget.
- **PIE check**: spell + ability + attack on a defender mid-combat. Damage numbers, crit, kill events, multi-hit totals all match Phase-A behaviour.

### Phase C — Migrate buildup paths
- C1: `ExecuteSpellAsync:614-617` calls `ApplyHit` with both damage AND buildup populated. **NOTE**: the buildup-pre-defense ordering changes — buildup is now subject to `FActionHitInput` and the eventual defense outcome. THIS IS A BEHAVIOUR CHANGE per the locked design (block reduces buildup, dodge cancels both). Surface it as a PR-scope note.
- C2: `WeaponManager::ExecuteAttackWithInfusion` migrates `ApplyWeaponDamage`+`ApplyWeaponStatusBuildup` into one `ApplyHit` per target. Hand-rolled defense math at `WeaponManager.cpp:730-777` deletes — `ApplyHit` routes through `UDamageCalculator`. Behaviour: weapon damage now picks up grid modifiers + BD stack mults that hand-rolled path missed. Document.
- C3: `ExecuteAttackAsync` GAINS a `ApplyHit` call with weapon-status `FActionHitInput` populated. Closes the leak from risk #4.
- One file per sub-phase. Each independently PIE-verifiable.

### Phase D — Retire legacy sync paths
- Once async paths are unified, sync `ExecuteSpell/Ability/Attack` can be rewritten as thin wrappers that build an `FAction` and call `ExecuteAction`. The duplicated `ProcessMultiHit:981, 1637, 1871` call sites disappear.
- Remove `ProcessMultiHit` if no remaining caller. Verify by Grep.

### Phase E — Stub fill-in and dead-code removal
- Implement `ApplyAbilityInfusionStatus`'s now-trivial body (or delete it — its work is in `ApplyHit`).
- Remove `WeaponManager::ApplyWeaponDamage` and `WeaponManager::ApplyWeaponStatusBuildup` if no remaining callers.
- Remove sync ExecuteSpell's `ApplySpellStatusBuildup` if no remaining callers.
- Final Grep verification.

---

## Separate concerns surfaced (not in scope but flagged)

1. **`UDefenseSystem::CalculateDefenseResult` ignores its own UPROPERTYs** — lines 340/346/347 use literal `0.5/0.3` instead of `BlockReduction/ParryReduction/ParryReflect`. Designer-tuning is broken.
2. **`OnDefenseWindowRequested` delegate has no in-source subscriber** — verified by Grep.
3. **`UWeaponManager::TriggerPhysicalStatus:665-717` is dead** — no in-source callers. The bleed/armor-break/stun branching is unreachable.
4. **Async ability path drops `Ability->Effects`** — `ApplyAbilityEffects:3865` only called from sync path.
5. **Async attack path drops weapon physical-status buildup** — `ExecuteAttackAsync` never calls `ApplyWeaponStatusBuildup`.
6. **Buildup currently ignores defense outcome entirely** — see Section 4.5; design says block reduces, dodge cancels, but code applies buildup pre-window and never reduces.
7. **`ApplyDamageAfterDefense` drops `bWasBlocked/Parried/Dodged` info on the floor** — `ActionExecutor.cpp:1228-1235`, marked TODO in source. The locked `FCombatHitResult` carries those fields; consolidation can finally store them.
8. **Multi-hit integer division applied twice** — see risk #3.
9. **Two parallel "calculate defense" implementations** — `UDamageCalculator::CalculateDamage` and the inline math in `UWeaponManager::ApplyWeaponDamage:730-777`. Consolidation kills the latter.
