// CurseData.h
// B-378b (MemoryCatalogue §3.8, D-100; SLF's curses): what a curse is and what it weakens. A curse is held by id
// (UCurseSubsystem, given by a memory's Curse reward whose RewardId is CurseId); this asset gives it a name, a journal
// text (Crown's) and its penalty: stat lines and resistances added to the holder's own party characters while held
// (negative values weaken). Debug: GetCurseString, PrintCurse in the Details panel.
// B-390 (D-169, Crown): a curse has a tier (F to S, like items) and an essence; clearing it at the hub costs that
// essence at the tier's step of the item essence price ladder (EconomyYield::GetTypedEssencePurchaseCostForTier).

#pragma once

#include "CoreMinimal.h"
#include "Currency/CurrencyTypes.h"
#include "Engine/DataAsset.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Inventory/ItemTier.h"
#include "CurseData.generated.h"

/** B-390 (D-169): the essence a curse's clear is paid in, chosen per curse. */
UENUM(BlueprintType)
enum class ECurseClearEssence : uint8
{
	Pillar,
	Elemental
};

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

	/** D-169: sets the clear price only (the penalty is not tier-scaled). TODO(Crown): each curse's tier (lore). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curse|Clear")
	EItemTier Tier = EItemTier::F_Tier;

	/** D-169: the essence the hub's clear is paid in. TODO(Crown): each curse's essence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curse|Clear")
	ECurseClearEssence ClearEssence = ECurseClearEssence::Pillar;

	/** The balance ClearEssence names (Pillar or Elemental Essence). */
	ECurrencyType GetClearCurrency() const;

	/** How much of GetClearCurrency clearing this curse costs (by Tier). */
	int32 GetClearPrice() const;

	/** Every curse asset (the asset registry, sorted by path). */
	static TArray<UCurseData *> GatherAll();

	FString GetCurseString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintCurse() const;
};
