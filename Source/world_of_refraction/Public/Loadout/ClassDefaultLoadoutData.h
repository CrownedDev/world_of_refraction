// ClassDefaultLoadoutData.h
// D-221 stage 1c (B-511): the kit every trial starts on, per class, so every run has the same starting point. One asset,
// DA_Loadout_ClassDefaults, found by UTrialRunSubsystem's config path (DefaultGame.ini). Each character's kit is minted
// into the pool once as its own (UPoolSubsystem::MintClassDefaultKit). Plan: docs/Design/LoadoutRestructurePlan.md §3.

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "Engine/DataAsset.h"
#include "ClassDefaultLoadoutData.generated.h"

class UAbilityData;
class UApparelData;
class UEquipmentDataBase;
class UWeaponData;

/** One kit piece: a weapon or an adapter, at its authored tier (the asset's). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FClassDefaultPiece
{
	GENERATED_BODY()

	/** A weapon (UWeaponData) or an adapter (URingData). Empty: this place stays empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TObjectPtr<UEquipmentDataBase> Piece = nullptr;

	/** Optional: the second weapon of a same-type pair (D-209). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TObjectPtr<UWeaponData> OffHand = nullptr;

	/** A weapon's pick per skill slot (empty: the weapon's built-ins). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TArray<TObjectPtr<UAbilityData>> SkillPicks;
};

/** One class's kit. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FClassDefaultLoadout
{
	GENERATED_BODY()

	// shortcut: in the WornGearLayout places (0 the primary, 1 the Generic's second weapon, 2+ adapters) until stage 2
	// drops the fixed places; leave a place's Piece empty to skip it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TArray<FClassDefaultPiece> Pieces;

	/** The place of the piece in hand at the start (the ACTIVE entry). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit", meta = (ClampMin = 0))
	int32 ActiveIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TArray<TObjectPtr<UApparelData>> Apparel;
};

/** A character's minted kit: the owned GUIDs, place for place with FClassDefaultLoadout (invalid for an empty place). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FClassDefaultKitIDs
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Kit")
	TArray<FGuid> Pieces;

	UPROPERTY(BlueprintReadOnly, Category = "Kit")
	TArray<FGuid> OffHands;

	UPROPERTY(BlueprintReadOnly, Category = "Kit")
	TArray<FGuid> Apparel;

	bool Contains(const FGuid &GearID) const
	{
		return GearID.IsValid() && (Pieces.Contains(GearID) || OffHands.Contains(GearID) || Apparel.Contains(GearID));
	}
};

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UClassDefaultLoadoutData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** TODO: an F-tier kit per class (Generic, Refractor, Resonator), picked from the existing F-tier gear. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kit")
	TMap<ECharacterClass, FClassDefaultLoadout> Defaults;

	/** Each class's kit, one line per piece. */
	FString GetDefaultsString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintDefaults() const;
};
