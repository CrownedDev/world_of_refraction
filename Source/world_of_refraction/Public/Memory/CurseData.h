// CurseData.h
// B-378b (MemoryCatalogue §3.8, D-100; SLF's curses): what a curse is and what it weakens. A curse is held by id
// (UCurseSubsystem, given by a memory's Curse reward whose RewardId is CurseId); this asset gives it a name, a journal
// text (Crown's) and its penalty: stat lines and resistances added to the holder's own party characters while held
// (negative values weaken). Debug: GetCurseString, PrintCurse in the Details panel.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "CurseData.generated.h"

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UCurseData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The id a memory's Curse / RemoveCurse reward names (FMemoryReward::RewardId). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curse")
	FName CurseId;

	/** TODO(Crown): the curse's name and journal text (lore). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curse")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curse", meta = (MultiLine = true))
	FText Description;

	/** Added to each of the holder's own characters' gear stats while held (negative lines weaken; not tier-scaled). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curse")
	FEquipmentStatBonus StatPenalty;

	/** Added to their resistances while held (negative: weaker to that element). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curse")
	FResistanceBonus ResistancePenalty;

	/** Every curse asset (the asset registry, sorted by path). */
	static TArray<UCurseData *> GatherAll();

	FString GetCurseString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintCurse() const;
};
