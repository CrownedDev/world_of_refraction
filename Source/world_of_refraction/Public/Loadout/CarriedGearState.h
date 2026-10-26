// CarriedGearState.h
// B-115 (D-013, D-093): the crystal state a party member carries from one fight to the next in a
// run. Pawns are respawned from authored data every fight, so without this the wear, the +10
// between-fight repair and a destroyed gem never reached the next fight.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Crystals/FEvolutionAttachment.h"
#include "CarriedGearState.generated.h"

/** One weapon or ring's attachment as the fight left it (empty when destroyed). */
USTRUCT()
struct WORLD_OF_REFRACTION_API FCarriedAttachment
{
	GENERATED_BODY()

	/** The weapon or ring asset the attachment sits on (ULoadoutComponent::FindAttachedItemByHolder). */
	UPROPERTY()
	TObjectPtr<UObject> Holder = nullptr;

	UPROPERTY()
	FRuntimeAttachedItem Attached;
};

USTRUCT()
struct WORLD_OF_REFRACTION_API FCarriedGearState
{
	GENERATED_BODY()

	/** False = nothing carried yet: the member starts from its authored gear. */
	UPROPERTY()
	bool bCarried = false;

	UPROPERTY()
	TArray<FCarriedAttachment> Attachments;

	/** The standalone primary-slot evolution (Broken Darkness), when the loadout has one. */
	UPROPERTY()
	bool bHasPrimaryEvolution = false;

	UPROPERTY()
	FEvolutionAttachment PrimaryEvolution;
};
