// MemoryFragment.h
// B-190 (RunStructure §5, Q-190): a fragment on the trial map. Picked up with the interact key, it
// adds its memory to the run and to the journal, and is gone for the rest of the run. No hint:
// the pickup says only that a fragment was found, not what it does.

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialMapFeature.h"
#include "MemoryFragment.generated.h"

class UMemoryData;

UCLASS()
class WORLD_OF_REFRACTION_API AMemoryFragment : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	AMemoryFragment();

	virtual void Interact(APawn *Pawn) override;

	UMemoryData *GetMemory() const { return Memory; }

protected:
	virtual void BeginPlay() override;
	virtual FString GetHintText() const override;

	UPROPERTY(EditInstanceOnly, Category = "Memory")
	TObjectPtr<UMemoryData> Memory;

private:
	/** Gone from the map: hidden and out of reach of the interact key. */
	void RemoveFromMap();
};
