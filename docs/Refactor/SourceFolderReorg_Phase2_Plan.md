# Source Folder Reorganization — Phase 2 Plan

**Branch:** `chore/source-folder-reorg`
**Rollback tag:** `pre-source-folder-reorg` → `8f50e28` (= main HEAD at branch point)
**Date:** 2026-05-28
**Survey reference:** Phase-1 survey + mini-survey (this session). Pass-1 structural map: `docs/analysis/Codebase_Analysis_Pass1_Map.md`.

> Goal: introduce subfolders under `Source/world_of_refraction/Public/` and `Private/` so the directory structure communicates system boundaries. **Single module — UBT auto-includes subdirectories, so `#include` statements do NOT change for moved files.** Moves are filesystem-only (`git mv`). The only edits are casing fixes that alter include strings (see §4).

---

## 1. Pre-flight (must hold before STEP 2 moves run)

- [ ] **UE 5.7 editor fully closed** (not just the level) — open editor during moves risks silent `.uasset` corruption. **Crown confirms.**
- [x] On `main`, working tree clean (verified `8f50e28`).
- [x] Rollback tag `pre-source-folder-reorg` created at `8f50e28`.
- [x] Branch `chore/source-folder-reorg` created + active.
- [x] This plan committed to the branch (preserved even if moves fail).

---

## 2. Decisions (locked) — basis labelled

| # | Decision | Resolution | Basis |
|---|---|---|---|
| Q1 | `Crystal*InventoryComponent` / `Evolution*InventoryComponent` home | → **`Crystals/`** (crystal-specific stores) | Phase-1 recommendation (default) |
| Q1b | `FWeaponInventoryEntry` / `FRingInventoryEntry` home | → **`Loadout/Entries/`** (kept beside loadout-entry twins) | Phase-1 recommendation (default) |
| Q2 | `EquipmentDataBase` + attachment-layer companions | → **`Gear/Equipment/`** (it is the shared base of `UWeaponData` + `URingData`, NOT crystal-specific) | **Mini-survey confirmed** |
| Q3 | Presentation-data cluster (`MovementData`, `StanceData`, `CosmeticsData`) | **Distributed** per-system (no `Presentation/` catch-all): `MovementData`→`Combat/Grid/`, `StanceData(+Debug)`→`Skills/Definitions/`, `CosmeticsData`→`Character/` | Phase-1 recommendation (default) |
| Q4 | `CombatPlayerController` | → **`Testing/`** (test scaffold; `Test*` UPROPERTYs + `DebugPrintState`; IntegrationGaps gap 1.1 "test scaffolding bypassing orchestrator" confirmed) | **Mini-survey confirmed** |
| Q5 | Folding aggressiveness | **Keep named subfolders** even when <5 files (`Camera/`, `Defense/`, `Projectile/`, `Damage/`, `Rings/`) — each is a clear subsystem boundary | Phase-1 recommendation (default) |
| Q6 | Casing fixes in scope | **Yes** — see §4 | **Mini-survey confirmed direction** |

> ⚠️ **Defaults (Q1, Q1b, Q3, Q5)** were not explicitly confirmed by Crown in-conversation; they adopt the Phase-1 recommendation. Crown may override any at the STEP-2 gate before moves run.

---

## 3. Locked folder structure

Mirror under both `Public/` and `Private/`. Depth limit: 2 levels under `Public/`, except the pre-existing UI tree (`UI/Combat/CommandMenu/`, 3-deep) which is **left as-is**.

```
Public/  (and mirrored Private/)
├── Combat/            Turn-based combat loop + per-combat subsystems
│   ├── Actions/       Action execution + action data structs/enums
│   ├── Damage/        Damage calculation
│   ├── Defense/       Defense windows (parry/block)
│   ├── Grid/          Grid placement + combat movement (+ MovementData)
│   ├── Camera/        Combat camera state machine
│   ├── Projectile/    Spell projectile actor
│   └── Mechanics/     Broken Darkness, Weather, Reality
├── Character/         Stats, class, data component (+ CosmeticsData)
├── AI/                Enemy decision-making
├── Skills/
│   ├── Definitions/   Spell/ability/school data assets (+ StanceData)
│   └── Effects/       Runtime effect application, triggers, buildup
├── Gear/
│   ├── Weapons/       Weapon manager, data, attacks, mesh
│   ├── Rings/         Ring manager + data
│   ├── Durability/    Break calc + durability constants
│   └── Equipment/     EquipmentDataBase + attachment/runtime-item layer
├── Crystals/          Crystals, evolutions, attachments, crystal stores
├── Inventory/         General inventory storage + consumable-item execution
├── Loadout/
│   └── Entries/       F* loadout/inventory entry structs
├── Infusion/          Infusion charge/cost/VFX + element color tables
├── UI/                UMG widgets (existing UI/Combat[/CommandMenu] tree, unchanged)
│   └── Combat/ …
└── Testing/           Cross-cutting test actors + CombatPlayerController
```

---

## 4. Casing fixes + the one include edit

The durability-constants file is `Durabilityconstants.h` (lowercase 'c') on disk AND in git. But **5 of 6 consumers already include it as `"DurabilityConstants.h"` (capital C)** — they only resolve on Windows' case-insensitive FS and would break on a case-sensitive cooker. So the rename to capital C is a **correctness fix**, not cosmetic.

- **Rename:** `Durabilityconstants.h` → **`DurabilityConstants.h`** (capital C) — do via `git mv` to the new `Gear/Durability/` location with corrected casing in one step.
  - On case-insensitive Windows, a casing-only `git mv` may need the two-step dance (`git mv X tmp && git mv tmp Xcorrect`) — but since this file is ALSO changing directory, the single `git mv Public/Durabilityconstants.h Public/Gear/Durability/DurabilityConstants.h` changes the path enough that casing updates cleanly.
  - **One include edit required:** `Public/CrystalIdentity.h:18` — `#include "Durabilityconstants.h"` → `#include "DurabilityConstants.h"`. (The other 5 consumers are already capital-C and become correct automatically.)
- **Casing fix (no include impact):** `Private/Combatorchestratortestactor.cpp` → `CombatOrchestratorTestActor.cpp` — implementation file, no `#include` references it by name; safe to correct during its move to `Combat/`.

No `.Build.cs` changes needed — verified no `PublicIncludePaths`/`PrivateIncludePaths` entries anywhere in `Source/`.

---

## 5. Flagged for follow-up (NOT this branch's work)

- **`ItemDataDebug.h/.cpp`** — debugs the renamed-away `ItemData` (now `EvolutionItemData`, commit `6ea8cd7`). May be stale/dead. Provisionally classified to `Inventory/`. **Do a dead-code check in a later pass** — do not let placement bless it as live.
- Crystals ↔ Inventory ↔ Loadout ↔ Gear boundary (Q1/Q1b) is the one genuinely tangled domain; if PIE/use reveals a cleaner split later, revisit.

---

## 6. Blueprint-safety assumption

UE resolves `UCLASS`/asset references by reflection name, not source file path. Moving `.h/.cpp` does **not** rebind Blueprint references. **Post-move verification:** one PIE smoke-test after the moves compile, to confirm no BP lost a C++ parent class.

---

## 7. Execution checklist (STEP 2 onward)

- [ ] STEP 2 — generate the `git mv` script from the §3 mapping (grouped, commit-safe batches). **Review before running.**
- [ ] STEP 3 — create the mirrored empty subfolder skeleton under `Public/` and `Private/` (UBT ignores empty dirs; `.gitkeep` not needed since moves populate them).
- [ ] STEP 4 — run `git mv` batches; commit per logical domain group (so a bad batch reverts cleanly).
- [ ] STEP 5 — apply the §4 casing rename + the single `CrystalIdentity.h:18` include edit.
- [ ] STEP 6 — Crown opens UE 5.7, regenerates project files (Rider/UE), builds.
- [ ] STEP 7 — PIE smoke-test (combat starts, BP parents intact). 
- [ ] STEP 8 — update `docs/Architecture/*` "Source layout" references if any cite old flat paths; update `CLAUDE.md` Source layout note.
- [ ] On success: merge to `main`, keep `pre-source-folder-reorg` tag until a few sessions pass.
- [ ] On failure: `git checkout main && git reset --hard pre-source-folder-reorg` is the rollback (branch is disposable).

---

## 8. Full file → destination mapping

Grouped by destination. **HO** = header-only (no Private/ mirror file). Every `.h` and paired `.cpp` listed.

### Combat/
| File(s) | Dest |
|---|---|
| CombatOrchestrator.h/.cpp | Combat/ |
| CombatOrchestratorTestActor.h / Combatorchestratortestactor.cpp → CombatOrchestratorTestActor.cpp | Combat/ (casing fix) |
| TurnManager.h/.cpp | Combat/ |
| TurnManagerTestActor.h/.cpp | Combat/ |
| CombatConstants.h/.cpp | Combat/ |
| CombatAnimInstance.h/.cpp | Combat/ |
| TargetType.h (HO) | Combat/ |
| ActionExecutor.h/.cpp | Combat/Actions/ |
| ActionStructs.h, ActionStatModifiers.h, ActionUtils.h, EActionType.h, EAbilityExecutionType.h (all HO) | Combat/Actions/ |
| DamageCalculator.h/.cpp; EPhysicalDamageType.h (HO) | Combat/Damage/ |
| DefenseSystem.h/.cpp; EDefenseDirection.h, EDefenseType.h (HO) | Combat/Defense/ |
| CombatGridSubsystem.h/.cpp; CombatMovementComponent.h/.cpp; MovementData.h/.cpp; CombatGridConstants.h, FCombatGridPosition.h, ECombatRow.h, ECombatMovementType.h, EMovementCategory.h (HO) | Combat/Grid/ |
| CombatCameraManager.h/.cpp; EActionCameraPhase.h, ECombatCameraState.h (HO) | Combat/Camera/ |
| SpellProjectile.h/.cpp; SpellProjectileTestActor.h/.cpp | Combat/Projectile/ |
| BrokenDarknessManager.h/.cpp; WeatherStateManager.h/.cpp; RealityBoost.h, FBrokenCrystalPayload.h (HO) | Combat/Mechanics/ |

### Character/
| File(s) | Dest |
|---|---|
| CharacterData.h/.cpp; CharacterDataComponent.h/.cpp; CharacterDataDebug.h/.cpp; CosmeticsData.h/.cpp | Character/ |
| ECharacterClass.h, FPillarWeights.h, StatConstants.h (HO) | Character/ |

### AI/
| File(s) | Dest |
|---|---|
| AIDecisionManager.h/.cpp; AIDecisionConstants.h, EAIDifficulty.h (HO) | AI/ |

### Skills/Definitions/
| File(s) | Dest |
|---|---|
| SkillDataBase.h/.cpp; CastableSkillDataBase.h/.cpp; SpellData.h/.cpp; SpellDataDebug.h/.cpp; AbilityData.h/.cpp; AbilityDataDebug.h/.cpp; WorldStatRequirements.h/.cpp; StanceData.h(HO); StanceDataDebug.h/.cpp | Skills/Definitions/ |
| SpellSchool.h, ESpellElement.h, ESpellDeliveryType.h, ESpellSource.h, EStatScalingType.h, ElementHelpers.h (all HO) | Skills/Definitions/ |

### Skills/Effects/
| File(s) | Dest |
|---|---|
| SkillEffectManager.h/.cpp; SkillEffectManagerTestActor.h/.cpp; StatusBuildupManager.h/.cpp | Skills/Effects/ |
| FSkillEffect.h, ActiveSkillEffect.h, BarCapTriggerResolver.h, SkillTriggerUtils.h, SkillEffectDisplayNames.h, ESkillEffectType.h, ESkillEffectTiming.h, ESkillTrigger.h (all HO) | Skills/Effects/ |

### Gear/
| File(s) | Dest |
|---|---|
| WeaponManager.h/.cpp; WeaponData.h/.cpp; WeaponDataDebug.h/.cpp; WeaponAttackData.h/.cpp; WeaponAttackDataDebug.h/.cpp; WeaponMeshComponent.h/.cpp; EWeaponType.h, EWeaponSlotType.h, EWeaponWieldMode.h (HO) | Gear/Weapons/ |
| RingManager.h/.cpp; RingData.h/.cpp | Gear/Rings/ |
| BreakCalculator.h/.cpp; BreakCalculatorDebug.h/.cpp; Durabilityconstants.h → DurabilityConstants.h (HO, casing fix) | Gear/Durability/ |
| EquipmentDataBase.h/.cpp; EquipmentBonusGenerator.h/.cpp; FEquipmentStatBonus.h/.cpp; FRuntimeAttachedItem.h/.cpp; IEquipmentGenerator.h, FAttachedItem.h, FRefinedAttachment.h, FEquippedItemSlot.h, EAttachedItemKind.h (HO) | Gear/Equipment/ |

### Crystals/
| File(s) | Dest |
|---|---|
| CrystalManager.h/.cpp; CrystalInventoryComponent.h/.cpp; CrystalEffectTable.h/.cpp; EvolutionItemData.h/.cpp; EvolutionInventoryComponent.h/.cpp; FEvolutionAttachment.h/.cpp | Crystals/ |
| CrystalDescription.h, CrystalIdentity.h, CrystalType.h, CrystalTypeHelpers.h, FCrystalId.h, EEvolutionType.h, FEvolutionInventoryEntry.h (all HO) | Crystals/ |

> Note: `CrystalIdentity.h:18` include edit applies here (§4).

### Inventory/
| File(s) | Dest |
|---|---|
| InventoryComponent.h/.cpp; InventoryData.h/.cpp; InventoryDebug.h/.cpp; ItemExecutor.h/.cpp; ItemDataDebug.h/.cpp (⚠ stale-check) | Inventory/ |
| InventoryConstants.h, ItemConstants.h, ItemTier.h, ItemEffectType.h (HO) | Inventory/ |

### Loadout/
| File(s) | Dest |
|---|---|
| LoadoutComponent.h/.cpp; FCombatLoadout.h/.cpp; FSavedLoadout.h/.cpp; LoadoutConstants.h (HO) | Loadout/ |
| FAbilityCollection.h/.cpp; FSpellCollection.h/.cpp; FWeaponLoadoutEntry.h/.cpp; FRingLoadoutEntry.h/.cpp; FWeaponInventoryEntry.h/.cpp; FRingInventoryEntry.h/.cpp; FItemLoadoutSlot.h (HO) | Loadout/Entries/ |

### Infusion/
| File(s) | Dest |
|---|---|
| InfusionChargeManager.h/.cpp; InfusionCostHelper.h/.cpp; InfusionVFXComponent.h/.cpp; InfusionDisplayDataDebug.h/.cpp; HybridSpellColors.h/.cpp; ElementColorDebugComponent.h/.cpp | Infusion/ |
| InfusionConstants.h, InfusionDisplayData.h, EChargeInfusionType.h, EInfusionSourceOption.h, EInfusionDisplayLocation.h, ElementColors.h (all HO) | Infusion/ |

### UI/ (existing tree — only CombatActionMenuBase moves in)
| File(s) | Dest |
|---|---|
| CombatActionMenuBase.h/.cpp (currently at root) | UI/Combat/ |
| (all other UI/Combat[/CommandMenu] files already correctly nested — no move) | — |

### Testing/
| File(s) | Dest |
|---|---|
| CombatPlayerController.h/.cpp | Testing/ |
| HUDTestActor.h/.cpp (already in Testing/) | — (no move) |

---

*This plan is the execution reference for STEP 2+. No source files have been moved as of this commit.*
