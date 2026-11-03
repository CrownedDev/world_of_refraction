// CharacterFormData.h
// B-279 (Design/CharacterForms.md): an authored character the player can unlock as a skin. Its
// character asset's inventory is the reference loadout the loadout-match meter compares against.
// Skins, outfits and the mini-boss use are B-279b to B-279d.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Inventory/ItemTier.h"
#include "CharacterFormData.generated.h"

class UCharacterData;
class USkeletalMesh;
class UMaterialInterface;

/** One piece of a loadout: gear carries its tier; a skill matches by asset alone. */
struct FCharacterFormPiece
{
    const UObject *Asset = nullptr;
    EItemTier Tier = EItemTier::E_Tier;
    bool bHasTier = false;
};

namespace CharacterFormMeter
{
    /** Meter gained by a fight won wearing every reference piece at its exact tier. // TODO: tune */
    constexpr float FULL_SET_FILL_PER_FIGHT = 0.25f;
    /** Credit for a reference gear piece worn at another tier (exact = 1). // TODO: tune */
    constexpr float OTHER_TIER_CREDIT = 0.5f;
    constexpr float FULL = 1.0f;

    /** What one fight won adds: FULL_SET_FILL_PER_FIGHT x the matched share of Reference (each reference
     *  piece counted once). 0 for an empty reference. */
    WORLD_OF_REFRACTION_API float GetFightFill(const TArray<FCharacterFormPiece> &Reference, const TArray<FCharacterFormPiece> &Equipped);
}

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UCharacterFormData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    static const FPrimaryAssetType FormAssetType;

    /** The player-facing name of the form (the character's). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Form")
    FText FormName;

    /** The authored character: its Inventory is the reference loadout (weapons, adapters, evolutions,
     *  spells, abilities) the meter matches against. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Form")
    TObjectPtr<UCharacterData> Character = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Form|Look")
    TSoftObjectPtr<USkeletalMesh> FormMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Form|Look")
    TArray<TSoftObjectPtr<UMaterialInterface>> FormMaterials;

    /** Memory / lore ids tied to the character (D-100). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Form|Lore")
    TArray<FName> LoreIds;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    /** The reference pieces from Character's inventory asset; empty without one. */
    TArray<FCharacterFormPiece> GetReferencePieces() const;

    /** Debug: name, character and reference pieces. */
    FString GetFormString() const;

    UFUNCTION(CallInEditor, Category = "Debug")
    void PrintForm() const;
};
