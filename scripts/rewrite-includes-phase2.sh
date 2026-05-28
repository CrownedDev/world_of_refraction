#!/usr/bin/env bash
# =============================================================================
# Phase 2 Include Rewriter — Option B / Approach 3 (modern UE5 conventions)
# =============================================================================
# Companion to scripts/source-folder-reorg.sh. After the headers MOVE into
# subfolders, this rewrites every `#include "Name.h"` of a moved header into
# its new subdirectory-qualified form `#include "Dest/Sub/Name.h"`.
#
# WHY: Targets use BuildSettingsVersion.V6 + IncludeOrderVersion.Unreal5_7,
#   which default bLegacyPublicIncludePaths = FALSE. UBT therefore does NOT
#   auto-add Public/ subdirectories to the include search path, so bare
#   `#include "Name.h"` stops resolving once Name.h leaves Public/ root.
#   This script makes every include path-explicit (the modern convention).
#
# Plan of record : docs/Refactor/SourceFolderReorg_Phase2_Plan.md (commit c760c79)
# Branch         : chore/source-folder-reorg
# SHELL          : Git Bash (GNU sed -E -i), consistent with source-folder-reorg.sh
#
# -----------------------------------------------------------------------------
# RUN ORDER (each step is a SEPARATE, reviewed action — this file is step 2):
#   1. scripts/source-folder-reorg.sh all     # move all 216 files
#   2. scripts/rewrite-includes-phase2.sh      # THIS script — rewrite includes
#   3. (no separate CrystalIdentity edit needed — see SPECIAL CASE 1 below;
#       this script's dual-key Durability rule already fixes that line)
#   4. Crown regenerates project files + builds
#   5. Iterate on any residual failures
# -----------------------------------------------------------------------------
#
# PRECISION / SAFETY:
#   - Matches ONLY quote-style `#include[ws]*"Name.h"`, anchored on the opening
#     quote. The opening-quote anchor means a partial name cannot match
#     ("TargetType.h" never matches "PartyTargetType.h" — different char before
#     the name) AND already-rewritten includes are skipped automatically
#     ("Combat/TargetType.h" has '/' not '"' before the name, so the pattern
#     does not match). => the script is naturally IDEMPOTENT; re-running is a
#     no-op. (This is a stronger guarantee than a manual "contains '/'" check.)
#   - Escaped dots: the '.' in "Name.h" is matched literally (\.), so it cannot
#     wildcard-match other chars.
#   - `.generated.h` is never touched: after the opening quote the next chars
#     are "Name.generated.h", which does not contain the literal "Name.h\"".
#   - Angle-bracket includes are NOT rewritten, EXCEPT the single known case
#     <CombatConstants.h> (SPECIAL CASE 2).
#   - set -euo pipefail. NO git operations here (moves + commits are separate).
#
# SPECIAL CASES:
#   1. DurabilityConstants (dual-key + casing fix). The file is Durabilityconstants.h
#      on disk (lowercase 'c'); it moves to Equipment/Durability/DurabilityConstants.h
#      (capital 'C'). Consumers use BOTH spellings:
#         "DurabilityConstants.h"  (5 consumers, already capital)
#         "Durabilityconstants.h"  (1 consumer: CrystalIdentity.h:18, lowercase)
#      BOTH are rewritten to "Equipment/Durability/DurabilityConstants.h".
#      => This absorbs the previously-separate CrystalIdentity.h:18 edit.
#   2. CombatConstants angle-bracket. The quote form is handled by the generic
#      map; additionally rewrite the one `<CombatConstants.h>` -> `<Combat/CombatConstants.h>`.
#
# -----------------------------------------------------------------------------
# MAPPING TABLE (141 moving headers — old name -> new path):
#   AbilityData.h                    -> Skills/Definitions/AbilityData.h
#   AbilityDataDebug.h               -> Skills/Definitions/AbilityDataDebug.h
#   ActionExecutor.h                 -> Combat/Actions/ActionExecutor.h
#   ActionStatModifiers.h            -> Combat/Actions/ActionStatModifiers.h
#   ActionStructs.h                  -> Combat/Actions/ActionStructs.h
#   ActionUtils.h                    -> Combat/Actions/ActionUtils.h
#   ActiveSkillEffect.h              -> Skills/Effects/ActiveSkillEffect.h
#   AIDecisionConstants.h            -> AI/AIDecisionConstants.h
#   AIDecisionManager.h              -> AI/AIDecisionManager.h
#   BarCapTriggerResolver.h          -> Skills/Effects/BarCapTriggerResolver.h
#   BreakCalculator.h                -> Equipment/Durability/BreakCalculator.h
#   BreakCalculatorDebug.h           -> Equipment/Durability/BreakCalculatorDebug.h
#   BrokenDarknessManager.h          -> Combat/Mechanics/BrokenDarknessManager.h
#   CastableSkillDataBase.h          -> Skills/Definitions/CastableSkillDataBase.h
#   CharacterData.h                  -> Character/CharacterData.h
#   CharacterDataComponent.h         -> Character/CharacterDataComponent.h
#   CharacterDataDebug.h             -> Character/CharacterDataDebug.h
#   CombatActionMenuBase.h           -> UI/Combat/CombatActionMenuBase.h
#   CombatAnimInstance.h             -> Combat/CombatAnimInstance.h
#   CombatCameraManager.h            -> Combat/Camera/CombatCameraManager.h
#   CombatConstants.h                -> Combat/CombatConstants.h   [+ angle-bracket SPECIAL CASE 2]
#   CombatGridConstants.h            -> Combat/Grid/CombatGridConstants.h
#   CombatGridSubsystem.h            -> Combat/Grid/CombatGridSubsystem.h
#   CombatMovementComponent.h        -> Combat/Grid/CombatMovementComponent.h
#   CombatOrchestrator.h             -> Combat/CombatOrchestrator.h
#   CombatOrchestratorTestActor.h    -> Combat/CombatOrchestratorTestActor.h
#   CombatPlayerController.h         -> Testing/CombatPlayerController.h
#   CosmeticsData.h                  -> Character/CosmeticsData.h
#   CrystalDescription.h             -> Equipment/Crystals/CrystalDescription.h
#   CrystalEffectTable.h             -> Equipment/Crystals/CrystalEffectTable.h
#   CrystalIdentity.h                -> Equipment/Crystals/CrystalIdentity.h
#   CrystalInventoryComponent.h      -> Equipment/Crystals/CrystalInventoryComponent.h
#   CrystalManager.h                 -> Equipment/Crystals/CrystalManager.h
#   CrystalType.h                    -> Equipment/Crystals/CrystalType.h
#   CrystalTypeHelpers.h             -> Equipment/Crystals/CrystalTypeHelpers.h
#   DamageCalculator.h               -> Combat/Damage/DamageCalculator.h
#   DefenseSystem.h                  -> Combat/Defense/DefenseSystem.h
#   Durabilityconstants.h / DurabilityConstants.h -> Equipment/Durability/DurabilityConstants.h  [SPECIAL CASE 1: dual-key + casing]
#   EAbilityExecutionType.h          -> Combat/Actions/EAbilityExecutionType.h
#   EActionCameraPhase.h             -> Combat/Camera/EActionCameraPhase.h
#   EActionType.h                    -> Combat/Actions/EActionType.h
#   EAIDifficulty.h                  -> AI/EAIDifficulty.h
#   EAttachedItemKind.h              -> Equipment/EAttachedItemKind.h
#   ECharacterClass.h                -> Character/ECharacterClass.h
#   EChargeInfusionType.h            -> Infusion/EChargeInfusionType.h
#   ECombatCameraState.h             -> Combat/Camera/ECombatCameraState.h
#   ECombatMovementType.h            -> Combat/Grid/ECombatMovementType.h
#   ECombatRow.h                     -> Combat/Grid/ECombatRow.h
#   EDefenseDirection.h              -> Combat/Defense/EDefenseDirection.h
#   EDefenseType.h                   -> Combat/Defense/EDefenseType.h
#   EEvolutionType.h                 -> Equipment/Crystals/EEvolutionType.h
#   EInfusionDisplayLocation.h       -> Infusion/EInfusionDisplayLocation.h
#   EInfusionSourceOption.h          -> Infusion/EInfusionSourceOption.h
#   ElementColorDebugComponent.h     -> Infusion/ElementColorDebugComponent.h
#   ElementColors.h                  -> Infusion/ElementColors.h
#   ElementHelpers.h                 -> Skills/Definitions/ElementHelpers.h
#   EMovementCategory.h              -> Combat/Grid/EMovementCategory.h          [survey: 0 includers]
#   EPhysicalDamageType.h            -> Combat/Damage/EPhysicalDamageType.h
#   EquipmentBonusGenerator.h        -> Equipment/EquipmentBonusGenerator.h
#   EquipmentDataBase.h              -> Equipment/EquipmentDataBase.h
#   ESkillEffectTiming.h             -> Skills/Effects/ESkillEffectTiming.h
#   ESkillEffectType.h               -> Skills/Effects/ESkillEffectType.h
#   ESkillTrigger.h                  -> Skills/Effects/ESkillTrigger.h
#   ESpellDeliveryType.h             -> Skills/Definitions/ESpellDeliveryType.h
#   ESpellElement.h                  -> Skills/Definitions/ESpellElement.h
#   ESpellSource.h                   -> Skills/Definitions/ESpellSource.h
#   EStatScalingType.h               -> Skills/Definitions/EStatScalingType.h    [survey: 0 includers]
#   EvolutionInventoryComponent.h    -> Equipment/Crystals/EvolutionInventoryComponent.h
#   EvolutionItemData.h              -> Equipment/Crystals/EvolutionItemData.h
#   EWeaponSlotType.h                -> Equipment/Weapons/EWeaponSlotType.h
#   EWeaponType.h                    -> Equipment/Weapons/EWeaponType.h
#   EWeaponWieldMode.h               -> Equipment/Weapons/EWeaponWieldMode.h
#   FAbilityCollection.h             -> Loadout/Entries/FAbilityCollection.h
#   FAttachedItem.h                  -> Equipment/FAttachedItem.h
#   FBrokenCrystalPayload.h          -> Equipment/Crystals/FBrokenCrystalPayload.h
#   FCombatGridPosition.h            -> Combat/Grid/FCombatGridPosition.h
#   FCombatLoadout.h                 -> Loadout/FCombatLoadout.h
#   FCrystalId.h                     -> Equipment/Crystals/FCrystalId.h
#   FEquipmentStatBonus.h            -> Equipment/FEquipmentStatBonus.h
#   FEquippedItemSlot.h              -> Equipment/FEquippedItemSlot.h
#   FEvolutionAttachment.h           -> Equipment/Crystals/FEvolutionAttachment.h
#   FEvolutionInventoryEntry.h       -> Equipment/Crystals/FEvolutionInventoryEntry.h
#   FItemLoadoutSlot.h               -> Loadout/Entries/FItemLoadoutSlot.h
#   FPillarWeights.h                 -> Character/FPillarWeights.h
#   FRefinedAttachment.h             -> Equipment/FRefinedAttachment.h
#   FRingInventoryEntry.h            -> Loadout/Entries/FRingInventoryEntry.h
#   FRingLoadoutEntry.h              -> Loadout/Entries/FRingLoadoutEntry.h
#   FRuntimeAttachedItem.h           -> Equipment/FRuntimeAttachedItem.h
#   FSavedLoadout.h                  -> Loadout/FSavedLoadout.h
#   FSkillEffect.h                   -> Skills/Effects/FSkillEffect.h
#   FSpellCollection.h               -> Loadout/Entries/FSpellCollection.h
#   FWeaponInventoryEntry.h          -> Loadout/Entries/FWeaponInventoryEntry.h
#   FWeaponLoadoutEntry.h            -> Loadout/Entries/FWeaponLoadoutEntry.h
#   HybridSpellColors.h              -> Infusion/HybridSpellColors.h
#   IEquipmentGenerator.h            -> Equipment/IEquipmentGenerator.h
#   InfusionChargeManager.h          -> Infusion/InfusionChargeManager.h
#   InfusionConstants.h              -> Infusion/InfusionConstants.h
#   InfusionCostHelper.h             -> Infusion/InfusionCostHelper.h
#   InfusionDisplayData.h            -> Infusion/InfusionDisplayData.h
#   InfusionDisplayDataDebug.h       -> Infusion/InfusionDisplayDataDebug.h
#   InfusionVFXComponent.h           -> Infusion/InfusionVFXComponent.h
#   InventoryComponent.h             -> Inventory/InventoryComponent.h
#   InventoryConstants.h             -> Inventory/InventoryConstants.h
#   InventoryData.h                  -> Inventory/InventoryData.h
#   InventoryDebug.h                 -> Inventory/InventoryDebug.h
#   ItemConstants.h                  -> Inventory/ItemConstants.h
#   ItemDataDebug.h                  -> Inventory/ItemDataDebug.h
#   ItemEffectType.h                 -> Inventory/ItemEffectType.h
#   ItemExecutor.h                   -> Inventory/ItemExecutor.h
#   ItemTier.h                       -> Inventory/ItemTier.h
#   LoadoutComponent.h               -> Loadout/LoadoutComponent.h
#   LoadoutConstants.h               -> Loadout/LoadoutConstants.h
#   MovementData.h                   -> Character/MovementData.h
#   RealityBoost.h                   -> Combat/Mechanics/RealityBoost.h
#   RingData.h                       -> Equipment/Rings/RingData.h
#   RingManager.h                    -> Equipment/Rings/RingManager.h
#   SkillDataBase.h                  -> Skills/Definitions/SkillDataBase.h
#   SkillEffectDisplayNames.h        -> Skills/Effects/SkillEffectDisplayNames.h
#   SkillEffectManager.h             -> Skills/Effects/SkillEffectManager.h
#   SkillEffectManagerTestActor.h    -> Skills/Effects/SkillEffectManagerTestActor.h
#   SkillTriggerUtils.h              -> Skills/Effects/SkillTriggerUtils.h
#   SpellData.h                      -> Skills/Definitions/SpellData.h
#   SpellDataDebug.h                 -> Skills/Definitions/SpellDataDebug.h
#   SpellProjectile.h                -> Combat/Projectile/SpellProjectile.h
#   SpellProjectileTestActor.h       -> Combat/Projectile/SpellProjectileTestActor.h
#   SpellSchool.h                    -> Skills/Definitions/SpellSchool.h
#   StanceData.h                     -> Character/StanceData.h
#   StanceDataDebug.h                -> Character/StanceDataDebug.h
#   StatConstants.h                  -> Character/StatConstants.h
#   StatusBuildupManager.h           -> Skills/Effects/StatusBuildupManager.h
#   TargetType.h                     -> Combat/TargetType.h
#   TurnManager.h                    -> Combat/TurnManager.h
#   TurnManagerTestActor.h           -> Combat/TurnManagerTestActor.h
#   WeaponAttackData.h               -> Equipment/Weapons/WeaponAttackData.h
#   WeaponAttackDataDebug.h          -> Equipment/Weapons/WeaponAttackDataDebug.h
#   WeaponData.h                     -> Equipment/Weapons/WeaponData.h
#   WeaponDataDebug.h                -> Equipment/Weapons/WeaponDataDebug.h
#   WeaponManager.h                  -> Equipment/Weapons/WeaponManager.h
#   WeaponMeshComponent.h            -> Equipment/Weapons/WeaponMeshComponent.h
#   WeatherStateManager.h            -> Combat/Mechanics/WeatherStateManager.h
#   WorldStatRequirements.h          -> Skills/Definitions/WorldStatRequirements.h
# -----------------------------------------------------------------------------
# =============================================================================

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
SRC="$REPO_ROOT/Source/world_of_refraction"
cd "$REPO_ROOT"
echo ">> include rewriter — scanning $SRC"

# Generic mapping: "OldName.h|DestSubdir"  (140 entries; Durabilityconstants
# handled separately in SPECIAL CASE 1 because of the dual-key + casing fix).
MAP=(
  "AbilityData.h|Skills/Definitions"
  "AbilityDataDebug.h|Skills/Definitions"
  "ActionExecutor.h|Combat/Actions"
  "ActionStatModifiers.h|Combat/Actions"
  "ActionStructs.h|Combat/Actions"
  "ActionUtils.h|Combat/Actions"
  "ActiveSkillEffect.h|Skills/Effects"
  "AIDecisionConstants.h|AI"
  "AIDecisionManager.h|AI"
  "BarCapTriggerResolver.h|Skills/Effects"
  "BreakCalculator.h|Equipment/Durability"
  "BreakCalculatorDebug.h|Equipment/Durability"
  "BrokenDarknessManager.h|Combat/Mechanics"
  "CastableSkillDataBase.h|Skills/Definitions"
  "CharacterData.h|Character"
  "CharacterDataComponent.h|Character"
  "CharacterDataDebug.h|Character"
  "CombatActionMenuBase.h|UI/Combat"
  "CombatAnimInstance.h|Combat"
  "CombatCameraManager.h|Combat/Camera"
  "CombatConstants.h|Combat"
  "CombatGridConstants.h|Combat/Grid"
  "CombatGridSubsystem.h|Combat/Grid"
  "CombatMovementComponent.h|Combat/Grid"
  "CombatOrchestrator.h|Combat"
  "CombatOrchestratorTestActor.h|Combat"
  "CombatPlayerController.h|Testing"
  "CosmeticsData.h|Character"
  "CrystalDescription.h|Equipment/Crystals"
  "CrystalEffectTable.h|Equipment/Crystals"
  "CrystalIdentity.h|Equipment/Crystals"
  "CrystalInventoryComponent.h|Equipment/Crystals"
  "CrystalManager.h|Equipment/Crystals"
  "CrystalType.h|Equipment/Crystals"
  "CrystalTypeHelpers.h|Equipment/Crystals"
  "DamageCalculator.h|Combat/Damage"
  "DefenseSystem.h|Combat/Defense"
  "EAbilityExecutionType.h|Combat/Actions"
  "EActionCameraPhase.h|Combat/Camera"
  "EActionType.h|Combat/Actions"
  "EAIDifficulty.h|AI"
  "EAttachedItemKind.h|Equipment"
  "ECharacterClass.h|Character"
  "EChargeInfusionType.h|Infusion"
  "ECombatCameraState.h|Combat/Camera"
  "ECombatMovementType.h|Combat/Grid"
  "ECombatRow.h|Combat/Grid"
  "EDefenseDirection.h|Combat/Defense"
  "EDefenseType.h|Combat/Defense"
  "EEvolutionType.h|Equipment/Crystals"
  "EInfusionDisplayLocation.h|Infusion"
  "EInfusionSourceOption.h|Infusion"
  "ElementColorDebugComponent.h|Infusion"
  "ElementColors.h|Infusion"
  "ElementHelpers.h|Skills/Definitions"
  "EMovementCategory.h|Combat/Grid"
  "EPhysicalDamageType.h|Combat/Damage"
  "EquipmentBonusGenerator.h|Equipment"
  "EquipmentDataBase.h|Equipment"
  "ESkillEffectTiming.h|Skills/Effects"
  "ESkillEffectType.h|Skills/Effects"
  "ESkillTrigger.h|Skills/Effects"
  "ESpellDeliveryType.h|Skills/Definitions"
  "ESpellElement.h|Skills/Definitions"
  "ESpellSource.h|Skills/Definitions"
  "EStatScalingType.h|Skills/Definitions"
  "EvolutionInventoryComponent.h|Equipment/Crystals"
  "EvolutionItemData.h|Equipment/Crystals"
  "EWeaponSlotType.h|Equipment/Weapons"
  "EWeaponType.h|Equipment/Weapons"
  "EWeaponWieldMode.h|Equipment/Weapons"
  "FAbilityCollection.h|Loadout/Entries"
  "FAttachedItem.h|Equipment"
  "FBrokenCrystalPayload.h|Equipment/Crystals"
  "FCombatGridPosition.h|Combat/Grid"
  "FCombatLoadout.h|Loadout"
  "FCrystalId.h|Equipment/Crystals"
  "FEquipmentStatBonus.h|Equipment"
  "FEquippedItemSlot.h|Equipment"
  "FEvolutionAttachment.h|Equipment/Crystals"
  "FEvolutionInventoryEntry.h|Equipment/Crystals"
  "FItemLoadoutSlot.h|Loadout/Entries"
  "FPillarWeights.h|Character"
  "FRefinedAttachment.h|Equipment"
  "FRingInventoryEntry.h|Loadout/Entries"
  "FRingLoadoutEntry.h|Loadout/Entries"
  "FRuntimeAttachedItem.h|Equipment"
  "FSavedLoadout.h|Loadout"
  "FSkillEffect.h|Skills/Effects"
  "FSpellCollection.h|Loadout/Entries"
  "FWeaponInventoryEntry.h|Loadout/Entries"
  "FWeaponLoadoutEntry.h|Loadout/Entries"
  "HybridSpellColors.h|Infusion"
  "IEquipmentGenerator.h|Equipment"
  "InfusionChargeManager.h|Infusion"
  "InfusionConstants.h|Infusion"
  "InfusionCostHelper.h|Infusion"
  "InfusionDisplayData.h|Infusion"
  "InfusionDisplayDataDebug.h|Infusion"
  "InfusionVFXComponent.h|Infusion"
  "InventoryComponent.h|Inventory"
  "InventoryConstants.h|Inventory"
  "InventoryData.h|Inventory"
  "InventoryDebug.h|Inventory"
  "ItemConstants.h|Inventory"
  "ItemDataDebug.h|Inventory"
  "ItemEffectType.h|Inventory"
  "ItemExecutor.h|Inventory"
  "ItemTier.h|Inventory"
  "LoadoutComponent.h|Loadout"
  "LoadoutConstants.h|Loadout"
  "MovementData.h|Character"
  "RealityBoost.h|Combat/Mechanics"
  "RingData.h|Equipment/Rings"
  "RingManager.h|Equipment/Rings"
  "SkillDataBase.h|Skills/Definitions"
  "SkillEffectDisplayNames.h|Skills/Effects"
  "SkillEffectManager.h|Skills/Effects"
  "SkillEffectManagerTestActor.h|Skills/Effects"
  "SkillTriggerUtils.h|Skills/Effects"
  "SpellData.h|Skills/Definitions"
  "SpellDataDebug.h|Skills/Definitions"
  "SpellProjectile.h|Combat/Projectile"
  "SpellProjectileTestActor.h|Combat/Projectile"
  "SpellSchool.h|Skills/Definitions"
  "StanceData.h|Character"
  "StanceDataDebug.h|Character"
  "StatConstants.h|Character"
  "StatusBuildupManager.h|Skills/Effects"
  "TargetType.h|Combat"
  "TurnManager.h|Combat"
  "TurnManagerTestActor.h|Combat"
  "WeaponAttackData.h|Equipment/Weapons"
  "WeaponAttackDataDebug.h|Equipment/Weapons"
  "WeaponData.h|Equipment/Weapons"
  "WeaponDataDebug.h|Equipment/Weapons"
  "WeaponManager.h|Equipment/Weapons"
  "WeaponMeshComponent.h|Equipment/Weapons"
  "WeatherStateManager.h|Combat/Mechanics"
  "WorldStatRequirements.h|Skills/Definitions"
)

# --- accounting -------------------------------------------------------------
declare -A TOUCHED          # set of files that received >=1 edit
TOTAL_LINES=0               # include lines modified
ZERO_HEADERS=()             # mappings that matched nothing

# regex-escape the literal dots in a header name
esc_dots() { printf '%s' "${1//./\\.}"; }

# rewrite_quote OLDNAME NEWPATH  — quote-form includes, idempotent, .generated.h-safe
rewrite_quote() {
  local old="$1" new="$2"
  local e; e="$(esc_dots "$old")"
  local pat="#include[[:space:]]*\"${e}\""
  local files n
  files="$(grep -rlE "$pat" "$SRC" --include='*.h' --include='*.cpp' 2>/dev/null || true)"
  n="$(grep -rhE "$pat" "$SRC" --include='*.h' --include='*.cpp' 2>/dev/null | wc -l | tr -d ' ' || true)"
  if [ "${n:-0}" -eq 0 ]; then ZERO_HEADERS+=("$old"); return 0; fi
  TOTAL_LINES=$((TOTAL_LINES + n))
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    TOUCHED["$f"]=1
    sed -E -i "s|(#include[[:space:]]*\")${e}\"|\1${new}\"|g" "$f"
  done <<< "$files"
  printf '   %-34s -> %-46s (%s lines)\n' "$old" "$new" "$n"
}

# rewrite_angle OLDNAME NEWPATH  — angle-bracket form (only CombatConstants)
rewrite_angle() {
  local old="$1" new="$2"
  local e; e="$(esc_dots "$old")"
  local pat="#include[[:space:]]*<${e}>"
  local files n
  files="$(grep -rlE "$pat" "$SRC" --include='*.h' --include='*.cpp' 2>/dev/null || true)"
  n="$(grep -rhE "$pat" "$SRC" --include='*.h' --include='*.cpp' 2>/dev/null | wc -l | tr -d ' ' || true)"
  if [ "${n:-0}" -eq 0 ]; then return 0; fi
  TOTAL_LINES=$((TOTAL_LINES + n))
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    TOUCHED["$f"]=1
    sed -E -i "s|(#include[[:space:]]*<)${e}>|\1${new}>|g" "$f"
  done <<< "$files"
  printf '   %-34s -> %-46s (%s lines, angle)\n' "<$old>" "<$new>" "$n"
}

echo ">> applying ${#MAP[@]} generic mappings (quote form) ..."
for entry in "${MAP[@]}"; do
  old="${entry%%|*}"
  dest="${entry#*|}"
  rewrite_quote "$old" "${dest}/${old}"
done

echo ">> SPECIAL CASE 1: DurabilityConstants (dual-key + casing) ..."
rewrite_quote "DurabilityConstants.h" "Equipment/Durability/DurabilityConstants.h"   # 5 capital consumers
rewrite_quote "Durabilityconstants.h" "Equipment/Durability/DurabilityConstants.h"   # 1 lowercase (CrystalIdentity.h)

echo ">> SPECIAL CASE 2: CombatConstants angle-bracket ..."
rewrite_angle "CombatConstants.h" "Combat/CombatConstants.h"

# --- report -----------------------------------------------------------------
echo ""
echo "============================================================"
echo " REWRITE COMPLETE"
echo "   distinct files edited      : ${#TOUCHED[@]}"
echo "   include lines modified     : ${TOTAL_LINES}"
echo "   mappings that matched zero : ${#ZERO_HEADERS[@]}"
if [ "${#ZERO_HEADERS[@]}" -gt 0 ]; then
  printf '       %s\n' "${ZERO_HEADERS[@]}"
fi
echo "   (first-run expectation: ~170 files, ~798 lines, 2 zero-match"
echo "    headers = EMovementCategory.h, EStatScalingType.h. On a 2nd run"
echo "    every mapping matches zero — that is the idempotent no-op signal.)"
echo "============================================================"

# =============================================================================
# PRE-EXECUTION PREDICTION (measured read-only against current tree):
#   mapping entries (headers)    : 141  (140 generic + Durabilityconstants special)
#   special-case entries         : 2    (Durability dual-key + casing; CombatConstants angle)
#   predicted distinct files     : 170
#   predicted include lines      : ~798 (797 quote + 1 angle)
#   predicted zero-match headers : 2    (EMovementCategory.h, EStatScalingType.h)
# Run AFTER scripts/source-folder-reorg.sh. No separate CrystalIdentity edit needed.
# =============================================================================
