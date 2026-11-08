// ApparelData.h
// B-380 (D-164; the deckbuilder's Apparel): a gear piece with HP and Defence lines and its own crystal socket. It sits
// in the loadout's apparel slots, outside the loadout budget (D-110 unchanged), and is separate from the cosmetic
// outfit pieces (D-111): stats only. Its crystal keys it (its element) and powers no skill. Only BaseStatBonus's
// BonusMaxHP and BonusDefense count (GetStatLines); other lines are ignored (and flagged by PrintApparel).
// Debug: GetApparelString, PrintApparel in the Details panel.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/EquipmentDataBase.h"
#include "ApparelData.generated.h"

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UApparelData : public UEquipmentDataBase
{
	GENERATED_BODY()

public:
	/** BaseStatBonus kept to the two apparel lines: max HP and Defence. */
	FEquipmentStatBonus GetStatLines() const;

	/** True when BaseStatBonus has a line apparel does not use (ignored in play). */
	bool HasUnusedStatLines() const;

	/** Debug: name, tier, HP and Defence, any unused lines. */
	FString GetApparelString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintApparel() const;
};
