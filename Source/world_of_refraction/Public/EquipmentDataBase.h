// EquipmentDataBase.h
// Shared base for UWeaponData and URingData. Owns identity, crystal slot,
// default spells, requirements, stat-bonus roll template, and icon —
// fields and behaviour confirmed identical between weapon and ring assets
// before extraction.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemTier.h"
#include "ESpellElement.h"
#include "WorldStatRequirements.h"
#include "FEquipmentStatBonus.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "EquipmentDataBase.generated.h"

class UItemData;
class USpellData;
class UCharacterData;
class UTexture2D;

/**
 * UEquipmentDataBase
 * Common base for weapon and ring data assets. Subclasses extend with
 * type-specific data (weapon attack, abilities, stance vs. ring mesh,
 * spell locking, etc).
 *
 * NOTE (UE5.7): Details-panel category ordering across base/derived class
 * boundaries is not controllable via `meta = (DisplayAfter = ...)` — that
 * hint only sorts within a single class. Base categories always render
 * before derived categories. Accepted as a cosmetic limitation; fixing it
 * would require an editor-only module with `IDetailCustomization`.
 */
UCLASS(Abstract, BlueprintType)
class WORLD_OF_REFRACTION_API UEquipmentDataBase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ==================== IDENTITY ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FString Name = TEXT("Unnamed Equipment");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    EItemTier Tier = EItemTier::E_Tier;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FString Description = TEXT("");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    UTexture2D *Icon = nullptr;

    // ==================== CRYSTAL ====================

    /** Refined crystal slotted into this equipment — determines element. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crystal")
    UItemData *SlottedCrystal = nullptr;

    /** Default spells for this equipment — copied to inventory entry when obtained.
     *  Lost if crystal is removed; spell vendor reassigns them. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crystal", meta = (TitleProperty = "Name"))
    TArray<USpellData *> DefaultSpells;

    // ==================== INFUSION ====================

    /** When true, this equipment cannot serve as an infusion source and
     *  any action wielded through it is rejected if infusion is requested.
     *  ORed with the action-level immunity at the infusion gate. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion")
    bool bImmuneToInfusion = false;

    /** Status buildup multiplier when this equipment is the infusion source
     *  (higher = faster status). Applied at the infusion DOT gate. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Infusion",
              meta = (EditCondition = "!bImmuneToInfusion", ClampMin = "0.0", ClampMax = "2.0"))
    float InfusionStatusMultiplier = 1.0f;

    // ==================== MASTERY ====================

    /** Roll template for this equipment's per-instance stat bonuses. Copied
     *  into the inventory entry's StatBonus at CreateFromWeapon /
     *  CreateFromRing time. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mastery")
    FEquipmentStatBonus DefaultStatBonus;

    /** When true, the per-instance StatBonus.bLocked is set on creation —
     *  bonuses cannot be re-rolled via SpendPendingPoints. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mastery")
    bool bStatBonusLocked = false;

    // ==================== REQUIREMENTS ====================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
    FWorldStatRequirements Requirements;

    // ==================== CRYSTAL HELPERS ====================

    UFUNCTION(BlueprintPure, Category = "Equipment|Crystal")
    bool HasCrystal() const { return SlottedCrystal != nullptr; }

    UFUNCTION(BlueprintPure, Category = "Equipment|Crystal")
    bool IsEvolved() const;

    UFUNCTION(BlueprintPure, Category = "Equipment|Crystal")
    ESpellElement GetCrystalElement() const;

    // ==================== UTILITY ====================

    UFUNCTION(BlueprintPure, Category = "Equipment")
    bool MeetsRequirements(const UCharacterData *Character) const
    {
        return Requirements.MeetsRequirements(Character);
    }

    UFUNCTION(BlueprintPure, Category = "Equipment")
    FString GetRequirementsSummary(const UCharacterData *Character) const
    {
        return Requirements.GetRequirementsSummary(Character);
    }

    UFUNCTION(BlueprintPure, Category = "Equipment")
    FString GetDisplayName() const { return Name; }

    /** Cap on DefaultSpells, used by IsDataValid warning. Subclasses override. */
    UFUNCTION(BlueprintPure, Category = "Equipment|Spells")
    virtual int32 GetMaxSpells() const { return TNumericLimits<int32>::Max(); }

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override;
#endif
};
