// PrebuiltFindData.h
// B-522b (asset overhaul §3 "Found pieces", Crown Q7): a find that comes built. A plain weapon or ring in a loot pool
// arrives empty (B-521); this entry names the piece and what is already in its socket, a gem or an evolution.
// TrialLoot::GatherPool lists it beside the plain assets, by its own SourceTags.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "GameplayTagContainer.h"
#include "PrebuiltFindData.generated.h"

class UEquipmentDataBase;
class UEvolutionItemData;

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UPrebuiltFindData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** A weapon (UWeaponData) or an adapter (URingData), given new and empty, then its socket filled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Find")
	TObjectPtr<UEquipmentDataBase> Piece = nullptr;

	/** The loot pools this find rolls in (a chest's or zone's pool tag), like a plain asset's SourceTags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Find")
	FGameplayTagContainer SourceTags;

	/** A gem in the socket, at full durability. Type None: no gem. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Find")
	FCrystalId Crystal;

	/** An evolution in the socket: minted as the finder's own instance and attached, so it can be taken off and used
	 *  elsewhere. Used when Crystal is None. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Find")
	TObjectPtr<UEvolutionItemData> Evolution = nullptr;
};
