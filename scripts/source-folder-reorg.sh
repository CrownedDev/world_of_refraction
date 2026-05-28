#!/usr/bin/env bash
# =============================================================================
# Source Folder Reorganization — Phase 2 execution script (git mv)
# =============================================================================
# Plan of record : docs/Refactor/SourceFolderReorg_Phase2_Plan.md  (commit c760c79)
# Branch         : chore/source-folder-reorg
# Rollback tag   : pre-source-folder-reorg (8f50e28)
#
# SHELL CHOICE: Git Bash (not PowerShell).
#   - `mkdir -p` creates nested dirs idempotently in one token.
#   - One `git mv SRC DST` per line reads cleanly for paper review.
#   - The two casing-fix moves (see below) need NO special Windows handling
#     because each ALSO changes directory; git applies the new leaf-name
#     casing as part of the path change. (A same-directory casing-only rename
#     on Windows would need `git mv -f` or a temp two-step — not the case here.)
#
# USAGE:
#   ./scripts/source-folder-reorg.sh           # run setup + ALL batches
#   ./scripts/source-folder-reorg.sh 1          # run setup + batch 1 only
#   ./scripts/source-folder-reorg.sh 4          # run setup + batch 4 only
#   (setup_dirs always runs first; it is idempotent, safe to repeat.)
#
# INTENDED FLOW: run ONE batch, let Crown regenerate project files + compile,
#   commit that batch, then run the next. Batches are independent move sets.
#
# SAFETY:
#   - UE 5.7 editor MUST be fully closed before running (verified at gen time).
#   - `set -euo pipefail` aborts on the first failed git mv.
#   - All paths are relative to Source/world_of_refraction/ (cd'd below).
#
# NOT IN THIS SCRIPT (post-move, separate step — see TODO at bottom):
#   - The single include edit: Public/Equipment/Crystals/CrystalIdentity.h:18
#     "Durabilityconstants.h" -> "DurabilityConstants.h"
# =============================================================================

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT/Source/world_of_refraction"
echo ">> working dir: $(pwd)"

# -----------------------------------------------------------------------------
# SETUP — create mirrored Public/ + Private/ destination subfolders
# (UBT auto-includes subdirs; #include statements do NOT change. mkdir only —
#  no git mv here. UI/Combat/ and Testing/ already exist, omitted.)
# -----------------------------------------------------------------------------
setup_dirs() {
  echo ">> [setup] creating mirrored destination folders"
  local subdirs=(
    Combat
    Combat/Actions
    Combat/Damage
    Combat/Defense
    Combat/Grid
    Combat/Camera
    Combat/Projectile
    Combat/Mechanics
    Character
    AI
    Skills/Definitions
    Skills/Effects
    Equipment
    Equipment/Weapons
    Equipment/Rings
    Equipment/Durability
    Equipment/Crystals
    Inventory
    Loadout
    Loadout/Entries
    Infusion
  )
  for d in "${subdirs[@]}"; do
    mkdir -p "Public/$d" "Private/$d"
  done
  echo ">> [setup] done (${#subdirs[@]} mirrored pairs = $(( ${#subdirs[@]} * 2 )) dirs)"
}

# =============================================================================
# ========================= BATCH 1 — Combat/ ================================
# =============================================================================
batch1() {
  echo ">> [batch 1] Combat/ (root + 7 subfolders)"

  # === Combat/ (root) ===
  git mv Public/CombatOrchestrator.h            Public/Combat/CombatOrchestrator.h
  git mv Private/CombatOrchestrator.cpp         Private/Combat/CombatOrchestrator.cpp
  git mv Public/CombatOrchestratorTestActor.h   Public/Combat/CombatOrchestratorTestActor.h
  # CASING FIX (move + rename): Combatorchestratortestactor.cpp -> CombatOrchestratorTestActor.cpp
  git mv Private/Combatorchestratortestactor.cpp Private/Combat/CombatOrchestratorTestActor.cpp
  git mv Public/TurnManager.h                   Public/Combat/TurnManager.h
  git mv Private/TurnManager.cpp                Private/Combat/TurnManager.cpp
  git mv Public/TurnManagerTestActor.h          Public/Combat/TurnManagerTestActor.h
  git mv Private/TurnManagerTestActor.cpp       Private/Combat/TurnManagerTestActor.cpp
  git mv Public/CombatConstants.h               Public/Combat/CombatConstants.h
  git mv Private/CombatConstants.cpp            Private/Combat/CombatConstants.cpp
  git mv Public/CombatAnimInstance.h            Public/Combat/CombatAnimInstance.h
  git mv Private/CombatAnimInstance.cpp         Private/Combat/CombatAnimInstance.cpp
  git mv Public/TargetType.h                    Public/Combat/TargetType.h   # HO

  # === Combat/Actions/ ===
  git mv Public/ActionExecutor.h                Public/Combat/Actions/ActionExecutor.h
  git mv Private/ActionExecutor.cpp             Private/Combat/Actions/ActionExecutor.cpp
  git mv Public/ActionStructs.h                 Public/Combat/Actions/ActionStructs.h          # HO
  git mv Public/ActionStatModifiers.h           Public/Combat/Actions/ActionStatModifiers.h    # HO
  git mv Public/ActionUtils.h                   Public/Combat/Actions/ActionUtils.h            # HO
  git mv Public/EActionType.h                   Public/Combat/Actions/EActionType.h            # HO
  git mv Public/EAbilityExecutionType.h         Public/Combat/Actions/EAbilityExecutionType.h  # HO

  # === Combat/Damage/ ===
  git mv Public/DamageCalculator.h              Public/Combat/Damage/DamageCalculator.h
  git mv Private/DamageCalculator.cpp           Private/Combat/Damage/DamageCalculator.cpp
  git mv Public/EPhysicalDamageType.h           Public/Combat/Damage/EPhysicalDamageType.h     # HO

  # === Combat/Defense/ ===
  git mv Public/DefenseSystem.h                 Public/Combat/Defense/DefenseSystem.h
  git mv Private/DefenseSystem.cpp              Private/Combat/Defense/DefenseSystem.cpp
  git mv Public/EDefenseDirection.h             Public/Combat/Defense/EDefenseDirection.h      # HO
  git mv Public/EDefenseType.h                  Public/Combat/Defense/EDefenseType.h           # HO

  # === Combat/Grid/ ===
  git mv Public/CombatGridSubsystem.h           Public/Combat/Grid/CombatGridSubsystem.h
  git mv Private/CombatGridSubsystem.cpp        Private/Combat/Grid/CombatGridSubsystem.cpp
  git mv Public/CombatMovementComponent.h       Public/Combat/Grid/CombatMovementComponent.h
  git mv Private/CombatMovementComponent.cpp    Private/Combat/Grid/CombatMovementComponent.cpp
  git mv Public/CombatGridConstants.h           Public/Combat/Grid/CombatGridConstants.h       # HO
  git mv Public/FCombatGridPosition.h           Public/Combat/Grid/FCombatGridPosition.h       # HO
  git mv Public/ECombatRow.h                    Public/Combat/Grid/ECombatRow.h                # HO
  git mv Public/ECombatMovementType.h           Public/Combat/Grid/ECombatMovementType.h       # HO
  git mv Public/EMovementCategory.h             Public/Combat/Grid/EMovementCategory.h         # HO

  # === Combat/Camera/ ===
  git mv Public/CombatCameraManager.h           Public/Combat/Camera/CombatCameraManager.h
  git mv Private/CombatCameraManager.cpp        Private/Combat/Camera/CombatCameraManager.cpp
  git mv Public/EActionCameraPhase.h            Public/Combat/Camera/EActionCameraPhase.h      # HO
  git mv Public/ECombatCameraState.h            Public/Combat/Camera/ECombatCameraState.h      # HO

  # === Combat/Projectile/ ===
  git mv Public/SpellProjectile.h               Public/Combat/Projectile/SpellProjectile.h
  git mv Private/SpellProjectile.cpp            Private/Combat/Projectile/SpellProjectile.cpp
  git mv Public/SpellProjectileTestActor.h      Public/Combat/Projectile/SpellProjectileTestActor.h
  git mv Private/SpellProjectileTestActor.cpp   Private/Combat/Projectile/SpellProjectileTestActor.cpp

  # === Combat/Mechanics/ ===
  git mv Public/BrokenDarknessManager.h         Public/Combat/Mechanics/BrokenDarknessManager.h
  git mv Private/BrokenDarknessManager.cpp      Private/Combat/Mechanics/BrokenDarknessManager.cpp
  git mv Public/WeatherStateManager.h           Public/Combat/Mechanics/WeatherStateManager.h
  git mv Private/WeatherStateManager.cpp        Private/Combat/Mechanics/WeatherStateManager.cpp
  git mv Public/RealityBoost.h                  Public/Combat/Mechanics/RealityBoost.h         # HO
}

# =============================================================================
# ===================== BATCH 2 — Character/ + AI/ ===========================
# =============================================================================
batch2() {
  echo ">> [batch 2] Character/ + AI/"

  # === Character/ (incl. movement/stance/cosmetic data) ===
  git mv Public/CharacterData.h                 Public/Character/CharacterData.h
  git mv Private/CharacterData.cpp              Private/Character/CharacterData.cpp
  git mv Public/CharacterDataComponent.h        Public/Character/CharacterDataComponent.h
  git mv Private/CharacterDataComponent.cpp     Private/Character/CharacterDataComponent.cpp
  git mv Public/CharacterDataDebug.h            Public/Character/CharacterDataDebug.h
  git mv Private/CharacterDataDebug.cpp         Private/Character/CharacterDataDebug.cpp
  git mv Public/CosmeticsData.h                 Public/Character/CosmeticsData.h
  git mv Private/CosmeticsData.cpp              Private/Character/CosmeticsData.cpp
  git mv Public/MovementData.h                  Public/Character/MovementData.h
  git mv Private/MovementData.cpp               Private/Character/MovementData.cpp
  git mv Public/StanceData.h                    Public/Character/StanceData.h                  # HO
  git mv Public/StanceDataDebug.h               Public/Character/StanceDataDebug.h
  git mv Private/StanceDataDebug.cpp            Private/Character/StanceDataDebug.cpp
  git mv Public/ECharacterClass.h               Public/Character/ECharacterClass.h             # HO
  git mv Public/FPillarWeights.h                Public/Character/FPillarWeights.h              # HO
  git mv Public/StatConstants.h                 Public/Character/StatConstants.h               # HO

  # === AI/ ===
  git mv Public/AIDecisionManager.h             Public/AI/AIDecisionManager.h
  git mv Private/AIDecisionManager.cpp          Private/AI/AIDecisionManager.cpp
  git mv Public/AIDecisionConstants.h           Public/AI/AIDecisionConstants.h                # HO
  git mv Public/EAIDifficulty.h                 Public/AI/EAIDifficulty.h                      # HO
}

# =============================================================================
# ============= BATCH 3 — Skills/ (Definitions + Effects) ====================
# =============================================================================
batch3() {
  echo ">> [batch 3] Skills/Definitions/ + Skills/Effects/"

  # === Skills/Definitions/ ===
  git mv Public/SkillDataBase.h                 Public/Skills/Definitions/SkillDataBase.h
  git mv Private/SkillDataBase.cpp              Private/Skills/Definitions/SkillDataBase.cpp
  git mv Public/CastableSkillDataBase.h         Public/Skills/Definitions/CastableSkillDataBase.h
  git mv Private/CastableSkillDataBase.cpp      Private/Skills/Definitions/CastableSkillDataBase.cpp
  git mv Public/SpellData.h                     Public/Skills/Definitions/SpellData.h
  git mv Private/SpellData.cpp                  Private/Skills/Definitions/SpellData.cpp
  git mv Public/SpellDataDebug.h                Public/Skills/Definitions/SpellDataDebug.h
  git mv Private/SpellDataDebug.cpp             Private/Skills/Definitions/SpellDataDebug.cpp
  git mv Public/AbilityData.h                   Public/Skills/Definitions/AbilityData.h
  git mv Private/AbilityData.cpp                Private/Skills/Definitions/AbilityData.cpp
  git mv Public/AbilityDataDebug.h              Public/Skills/Definitions/AbilityDataDebug.h
  git mv Private/AbilityDataDebug.cpp           Private/Skills/Definitions/AbilityDataDebug.cpp
  git mv Public/WorldStatRequirements.h         Public/Skills/Definitions/WorldStatRequirements.h
  git mv Private/WorldStatRequirements.cpp      Private/Skills/Definitions/WorldStatRequirements.cpp
  git mv Public/SpellSchool.h                   Public/Skills/Definitions/SpellSchool.h        # HO
  git mv Public/ESpellElement.h                 Public/Skills/Definitions/ESpellElement.h      # HO
  git mv Public/ESpellDeliveryType.h            Public/Skills/Definitions/ESpellDeliveryType.h # HO
  git mv Public/ESpellSource.h                  Public/Skills/Definitions/ESpellSource.h       # HO
  git mv Public/EStatScalingType.h              Public/Skills/Definitions/EStatScalingType.h   # HO
  git mv Public/ElementHelpers.h                Public/Skills/Definitions/ElementHelpers.h     # HO

  # === Skills/Effects/ ===
  git mv Public/SkillEffectManager.h            Public/Skills/Effects/SkillEffectManager.h
  git mv Private/SkillEffectManager.cpp         Private/Skills/Effects/SkillEffectManager.cpp
  git mv Public/SkillEffectManagerTestActor.h   Public/Skills/Effects/SkillEffectManagerTestActor.h
  git mv Private/SkillEffectManagerTestActor.cpp Private/Skills/Effects/SkillEffectManagerTestActor.cpp
  git mv Public/StatusBuildupManager.h          Public/Skills/Effects/StatusBuildupManager.h
  git mv Private/StatusBuildupManager.cpp       Private/Skills/Effects/StatusBuildupManager.cpp
  git mv Public/FSkillEffect.h                  Public/Skills/Effects/FSkillEffect.h           # HO
  git mv Public/ActiveSkillEffect.h             Public/Skills/Effects/ActiveSkillEffect.h      # HO
  git mv Public/BarCapTriggerResolver.h         Public/Skills/Effects/BarCapTriggerResolver.h  # HO
  git mv Public/SkillTriggerUtils.h             Public/Skills/Effects/SkillTriggerUtils.h      # HO
  git mv Public/SkillEffectDisplayNames.h       Public/Skills/Effects/SkillEffectDisplayNames.h # HO
  git mv Public/ESkillEffectType.h              Public/Skills/Effects/ESkillEffectType.h       # HO
  git mv Public/ESkillEffectTiming.h            Public/Skills/Effects/ESkillEffectTiming.h     # HO
  git mv Public/ESkillTrigger.h                 Public/Skills/Effects/ESkillTrigger.h          # HO
}

# =============================================================================
# ============ BATCH 4 — Equipment/ (root + 4 subfolders) ====================
# =============================================================================
batch4() {
  echo ">> [batch 4] Equipment/ (root + Weapons + Rings + Durability + Crystals)"

  # === Equipment/ (ROOT — shared base + attachment layer) ===
  git mv Public/EquipmentDataBase.h             Public/Equipment/EquipmentDataBase.h
  git mv Private/EquipmentDataBase.cpp          Private/Equipment/EquipmentDataBase.cpp
  git mv Public/EquipmentBonusGenerator.h       Public/Equipment/EquipmentBonusGenerator.h
  git mv Private/EquipmentBonusGenerator.cpp    Private/Equipment/EquipmentBonusGenerator.cpp
  git mv Public/FEquipmentStatBonus.h           Public/Equipment/FEquipmentStatBonus.h
  git mv Private/FEquipmentStatBonus.cpp        Private/Equipment/FEquipmentStatBonus.cpp
  git mv Public/FRuntimeAttachedItem.h          Public/Equipment/FRuntimeAttachedItem.h
  git mv Private/FRuntimeAttachedItem.cpp       Private/Equipment/FRuntimeAttachedItem.cpp
  git mv Public/IEquipmentGenerator.h           Public/Equipment/IEquipmentGenerator.h         # HO
  git mv Public/FAttachedItem.h                 Public/Equipment/FAttachedItem.h               # HO
  git mv Public/FRefinedAttachment.h            Public/Equipment/FRefinedAttachment.h          # HO
  git mv Public/FEquippedItemSlot.h             Public/Equipment/FEquippedItemSlot.h           # HO
  git mv Public/EAttachedItemKind.h             Public/Equipment/EAttachedItemKind.h           # HO

  # === Equipment/Weapons/ ===
  git mv Public/WeaponManager.h                 Public/Equipment/Weapons/WeaponManager.h
  git mv Private/WeaponManager.cpp              Private/Equipment/Weapons/WeaponManager.cpp
  git mv Public/WeaponData.h                    Public/Equipment/Weapons/WeaponData.h
  git mv Private/WeaponData.cpp                 Private/Equipment/Weapons/WeaponData.cpp
  git mv Public/WeaponDataDebug.h               Public/Equipment/Weapons/WeaponDataDebug.h
  git mv Private/WeaponDataDebug.cpp            Private/Equipment/Weapons/WeaponDataDebug.cpp
  git mv Public/WeaponAttackData.h              Public/Equipment/Weapons/WeaponAttackData.h
  git mv Private/WeaponAttackData.cpp           Private/Equipment/Weapons/WeaponAttackData.cpp
  git mv Public/WeaponAttackDataDebug.h         Public/Equipment/Weapons/WeaponAttackDataDebug.h
  git mv Private/WeaponAttackDataDebug.cpp      Private/Equipment/Weapons/WeaponAttackDataDebug.cpp
  git mv Public/WeaponMeshComponent.h           Public/Equipment/Weapons/WeaponMeshComponent.h
  git mv Private/WeaponMeshComponent.cpp        Private/Equipment/Weapons/WeaponMeshComponent.cpp
  git mv Public/EWeaponType.h                   Public/Equipment/Weapons/EWeaponType.h         # HO
  git mv Public/EWeaponSlotType.h               Public/Equipment/Weapons/EWeaponSlotType.h     # HO
  git mv Public/EWeaponWieldMode.h              Public/Equipment/Weapons/EWeaponWieldMode.h    # HO

  # === Equipment/Rings/ ===
  git mv Public/RingManager.h                   Public/Equipment/Rings/RingManager.h
  git mv Private/RingManager.cpp                Private/Equipment/Rings/RingManager.cpp
  git mv Public/RingData.h                      Public/Equipment/Rings/RingData.h
  git mv Private/RingData.cpp                   Private/Equipment/Rings/RingData.cpp

  # === Equipment/Durability/ ===
  git mv Public/BreakCalculator.h               Public/Equipment/Durability/BreakCalculator.h
  git mv Private/BreakCalculator.cpp            Private/Equipment/Durability/BreakCalculator.cpp
  git mv Public/BreakCalculatorDebug.h          Public/Equipment/Durability/BreakCalculatorDebug.h
  git mv Private/BreakCalculatorDebug.cpp       Private/Equipment/Durability/BreakCalculatorDebug.cpp
  # CASING FIX (move + rename): Durabilityconstants.h -> DurabilityConstants.h
  git mv Public/Durabilityconstants.h           Public/Equipment/Durability/DurabilityConstants.h  # HO

  # === Equipment/Crystals/ ===
  git mv Public/CrystalManager.h                Public/Equipment/Crystals/CrystalManager.h
  git mv Private/CrystalManager.cpp             Private/Equipment/Crystals/CrystalManager.cpp
  git mv Public/CrystalInventoryComponent.h     Public/Equipment/Crystals/CrystalInventoryComponent.h
  git mv Private/CrystalInventoryComponent.cpp  Private/Equipment/Crystals/CrystalInventoryComponent.cpp
  git mv Public/CrystalEffectTable.h            Public/Equipment/Crystals/CrystalEffectTable.h
  git mv Private/CrystalEffectTable.cpp         Private/Equipment/Crystals/CrystalEffectTable.cpp
  git mv Public/EvolutionItemData.h             Public/Equipment/Crystals/EvolutionItemData.h
  git mv Private/EvolutionItemData.cpp          Private/Equipment/Crystals/EvolutionItemData.cpp
  git mv Public/EvolutionInventoryComponent.h   Public/Equipment/Crystals/EvolutionInventoryComponent.h
  git mv Private/EvolutionInventoryComponent.cpp Private/Equipment/Crystals/EvolutionInventoryComponent.cpp
  git mv Public/FEvolutionAttachment.h          Public/Equipment/Crystals/FEvolutionAttachment.h
  git mv Private/FEvolutionAttachment.cpp       Private/Equipment/Crystals/FEvolutionAttachment.cpp
  git mv Public/CrystalDescription.h            Public/Equipment/Crystals/CrystalDescription.h    # HO
  git mv Public/CrystalIdentity.h               Public/Equipment/Crystals/CrystalIdentity.h       # HO (include edit TODO)
  git mv Public/CrystalType.h                   Public/Equipment/Crystals/CrystalType.h           # HO
  git mv Public/CrystalTypeHelpers.h            Public/Equipment/Crystals/CrystalTypeHelpers.h    # HO
  git mv Public/FCrystalId.h                    Public/Equipment/Crystals/FCrystalId.h            # HO
  git mv Public/EEvolutionType.h                Public/Equipment/Crystals/EEvolutionType.h        # HO
  git mv Public/FEvolutionInventoryEntry.h      Public/Equipment/Crystals/FEvolutionInventoryEntry.h # HO
  git mv Public/FBrokenCrystalPayload.h         Public/Equipment/Crystals/FBrokenCrystalPayload.h    # HO
}

# =============================================================================
# ============ BATCH 5 — Inventory/ + Loadout/ (+ Entries) ===================
# =============================================================================
batch5() {
  echo ">> [batch 5] Inventory/ + Loadout/ + Loadout/Entries/"

  # === Inventory/ ===
  git mv Public/InventoryComponent.h            Public/Inventory/InventoryComponent.h
  git mv Private/InventoryComponent.cpp         Private/Inventory/InventoryComponent.cpp
  git mv Public/InventoryData.h                 Public/Inventory/InventoryData.h
  git mv Private/InventoryData.cpp              Private/Inventory/InventoryData.cpp
  git mv Public/InventoryDebug.h                Public/Inventory/InventoryDebug.h
  git mv Private/InventoryDebug.cpp             Private/Inventory/InventoryDebug.cpp
  git mv Public/ItemExecutor.h                  Public/Inventory/ItemExecutor.h
  git mv Private/ItemExecutor.cpp               Private/Inventory/ItemExecutor.cpp
  git mv Public/ItemDataDebug.h                 Public/Inventory/ItemDataDebug.h    # (stale-check follow-up)
  git mv Private/ItemDataDebug.cpp              Private/Inventory/ItemDataDebug.cpp # (stale-check follow-up)
  git mv Public/InventoryConstants.h            Public/Inventory/InventoryConstants.h          # HO
  git mv Public/ItemConstants.h                 Public/Inventory/ItemConstants.h               # HO
  git mv Public/ItemTier.h                      Public/Inventory/ItemTier.h                    # HO
  git mv Public/ItemEffectType.h                Public/Inventory/ItemEffectType.h              # HO

  # === Loadout/ ===
  git mv Public/LoadoutComponent.h              Public/Loadout/LoadoutComponent.h
  git mv Private/LoadoutComponent.cpp           Private/Loadout/LoadoutComponent.cpp
  git mv Public/FCombatLoadout.h                Public/Loadout/FCombatLoadout.h
  git mv Private/FCombatLoadout.cpp             Private/Loadout/FCombatLoadout.cpp
  git mv Public/FSavedLoadout.h                 Public/Loadout/FSavedLoadout.h
  git mv Private/FSavedLoadout.cpp              Private/Loadout/FSavedLoadout.cpp
  git mv Public/LoadoutConstants.h              Public/Loadout/LoadoutConstants.h              # HO

  # === Loadout/Entries/ ===
  git mv Public/FAbilityCollection.h            Public/Loadout/Entries/FAbilityCollection.h
  git mv Private/FAbilityCollection.cpp         Private/Loadout/Entries/FAbilityCollection.cpp
  git mv Public/FSpellCollection.h              Public/Loadout/Entries/FSpellCollection.h
  git mv Private/FSpellCollection.cpp           Private/Loadout/Entries/FSpellCollection.cpp
  git mv Public/FWeaponLoadoutEntry.h           Public/Loadout/Entries/FWeaponLoadoutEntry.h
  git mv Private/FWeaponLoadoutEntry.cpp        Private/Loadout/Entries/FWeaponLoadoutEntry.cpp
  git mv Public/FRingLoadoutEntry.h             Public/Loadout/Entries/FRingLoadoutEntry.h
  git mv Private/FRingLoadoutEntry.cpp          Private/Loadout/Entries/FRingLoadoutEntry.cpp
  git mv Public/FWeaponInventoryEntry.h         Public/Loadout/Entries/FWeaponInventoryEntry.h
  git mv Private/FWeaponInventoryEntry.cpp      Private/Loadout/Entries/FWeaponInventoryEntry.cpp
  git mv Public/FRingInventoryEntry.h           Public/Loadout/Entries/FRingInventoryEntry.h
  git mv Private/FRingInventoryEntry.cpp        Private/Loadout/Entries/FRingInventoryEntry.cpp
  git mv Public/FItemLoadoutSlot.h              Public/Loadout/Entries/FItemLoadoutSlot.h      # HO
}

# =============================================================================
# ======================= BATCH 6 — Infusion/ ================================
# =============================================================================
batch6() {
  echo ">> [batch 6] Infusion/"

  git mv Public/InfusionChargeManager.h         Public/Infusion/InfusionChargeManager.h
  git mv Private/InfusionChargeManager.cpp      Private/Infusion/InfusionChargeManager.cpp
  git mv Public/InfusionCostHelper.h            Public/Infusion/InfusionCostHelper.h
  git mv Private/InfusionCostHelper.cpp         Private/Infusion/InfusionCostHelper.cpp
  git mv Public/InfusionVFXComponent.h          Public/Infusion/InfusionVFXComponent.h
  git mv Private/InfusionVFXComponent.cpp       Private/Infusion/InfusionVFXComponent.cpp
  git mv Public/InfusionDisplayDataDebug.h      Public/Infusion/InfusionDisplayDataDebug.h
  git mv Private/InfusionDisplayDataDebug.cpp   Private/Infusion/InfusionDisplayDataDebug.cpp
  git mv Public/HybridSpellColors.h             Public/Infusion/HybridSpellColors.h
  git mv Private/HybridSpellColors.cpp          Private/Infusion/HybridSpellColors.cpp
  git mv Public/ElementColorDebugComponent.h    Public/Infusion/ElementColorDebugComponent.h
  git mv Private/ElementColorDebugComponent.cpp Private/Infusion/ElementColorDebugComponent.cpp
  git mv Public/InfusionConstants.h             Public/Infusion/InfusionConstants.h            # HO
  git mv Public/InfusionDisplayData.h           Public/Infusion/InfusionDisplayData.h          # HO
  git mv Public/EChargeInfusionType.h           Public/Infusion/EChargeInfusionType.h          # HO
  git mv Public/EInfusionSourceOption.h         Public/Infusion/EInfusionSourceOption.h        # HO
  git mv Public/EInfusionDisplayLocation.h      Public/Infusion/EInfusionDisplayLocation.h     # HO
  git mv Public/ElementColors.h                 Public/Infusion/ElementColors.h                # HO
}

# =============================================================================
# ============ BATCH 7 — UI/ pull-in + Testing/ move =========================
# (UI/Combat/ and Testing/ already exist — no mkdir needed.)
# =============================================================================
batch7() {
  echo ">> [batch 7] UI/Combat/ (CombatActionMenuBase) + Testing/ (CombatPlayerController)"

  # === UI/Combat/ (pull CombatActionMenuBase in from root) ===
  git mv Public/CombatActionMenuBase.h          Public/UI/Combat/CombatActionMenuBase.h
  git mv Private/CombatActionMenuBase.cpp       Private/UI/Combat/CombatActionMenuBase.cpp

  # === Testing/ (CombatPlayerController = test scaffold, gap 1.1) ===
  git mv Public/CombatPlayerController.h         Public/Testing/CombatPlayerController.h
  git mv Private/CombatPlayerController.cpp      Private/Testing/CombatPlayerController.cpp
}

# =============================================================================
# DISPATCH
# =============================================================================
setup_dirs   # always runs first; idempotent
case "${1:-all}" in
  setup) : ;;                                   # dirs only
  1) batch1 ;;
  2) batch2 ;;
  3) batch3 ;;
  4) batch4 ;;
  5) batch5 ;;
  6) batch6 ;;
  7) batch7 ;;
  all) batch1; batch2; batch3; batch4; batch5; batch6; batch7 ;;
  *) echo "ERROR: unknown batch '$1' (use: setup|1..7|all)" >&2; exit 2 ;;
esac
echo ">> done: ${1:-all}"

# =============================================================================
# SANITY COUNTS (verified at generation against the plan classification table)
# -----------------------------------------------------------------------------
#   mkdir (mirrored dir pairs)      : 21 pairs  = 42 dirs
#   git mv total                    : 216  (141 headers + 75 .cpp)
#     batch1 Combat/                : 49  (33 .h + 16 .cpp)
#     batch2 Character/ + AI/       : 20  (13 .h +  7 .cpp)
#     batch3 Skills/                : 34  (24 .h + 10 .cpp)
#     batch4 Equipment/             : 57  (37 .h + 20 .cpp)
#     batch5 Inventory/ + Loadout/  : 34  (20 .h + 14 .cpp)
#     batch6 Infusion/              : 18  (12 .h +  6 .cpp)
#     batch7 UI/ + Testing/         :  4  ( 2 .h +  2 .cpp)
#   casing-fix moves                : 2  (Combatorchestratortestactor.cpp, Durabilityconstants.h)
#
#   Files in plan NOT in script     : 0
#   Files in script NOT in plan     : 0
#   (Reconciliation: plan total 239 files = 216 moved here + 23 already-located,
#    no-move files: 12 .h + 11 .cpp under Public/UI/Combat[/CommandMenu] +
#    Public/Testing/HUDTestActor, and their Private/ pairs.)
#
# TODO (post-move, NOT in this script — separate STEP 5 edit):
#   Public/Equipment/Crystals/CrystalIdentity.h:18
#     #include "Durabilityconstants.h"  ->  #include "DurabilityConstants.h"
#   (The other 5 consumers of that header already use the capital-C spelling.)
# =============================================================================
