// WeaponAttackData.h
// Weapon attack data asset - inherits from BaseAttackData, adds physical damage type

#pragma once

#include "CoreMinimal.h"
#include "BaseAttackData.h"
#include "EPhysicalDamageType.h"
#include "WeaponAttackData.generated.h"

/**
 * Weapon Attack Data Asset
 * Extends BaseAttackData with physical damage type for Generic character infusion
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UWeaponAttackData : public UBaseAttackData
{
    GENERATED_BODY()

public:
    // ==================== PHYSICAL DAMAGE TYPE ====================

    // Physical damage type (Generic characters only - enables status buildup)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Type")
    EPhysicalDamageType PhysicalDamageType = EPhysicalDamageType::Slash;

    // Status buildup per hit (fills status bar for Bleed/Stun/ArmorBreak)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage Type", meta = (ClampMin = "0"))
    int32 StatusBuildup = 10;

    // ==================== UTILITY ====================

    UFUNCTION(BlueprintPure, Category = "WeaponAttack")
    FString GetDamageTypeName() const;
};