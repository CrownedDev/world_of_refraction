// EncounterData.h
// B-074: an enemy team authored as data (Encounter Composition Arc 2). A field encounter
// (UEncounterComponent) that names one fights this team, up to ENCOUNTER_MAX_ENEMIES strong,
// instead of only the pawn it sits on. Difficulty and rank stay on the placed encounter.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EncounterData.generated.h"

class UCharacterData;

namespace EncounterConstants
{
	/** docs/Design/Enemies.md §3 (fight size): a team is at most three. */
	constexpr int32 ENCOUNTER_MAX_ENEMIES = 3;
}

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UEncounterData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The enemy team, in battle-line order. Empty entries are skipped; past three are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter")
	TArray<TObjectPtr<UCharacterData>> Enemies;

	/** The team that takes the field: the authored enemies, empty entries skipped, at most three. */
	UFUNCTION(BlueprintPure, Category = "Encounter")
	TArray<UCharacterData *> GetEnemyRoster() const;

	/** The opposing roster for a field encounter: Encounter's team when it has one, else the
	 *  encounter's own pawn (Fallback) alone. Empty only when both are missing. */
	static TArray<UCharacterData *> BuildOpposingRoster(const UEncounterData *Encounter, UCharacterData *Fallback);

	/** One line per enemy, for logs and the Details panel. */
	FString GetEncounterString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintEncounter() const;
};
