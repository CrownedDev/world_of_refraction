// MemorySpot.h
// B-209 (D-100 C): a special spot on the trial map. Walking into it unlocks its memory (into the
// run and the journal, MemoryUnlock), once per run. Unseen in game: no mesh, no hint, only the
// line "A memory surfaces." when it fires.

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialMapFeature.h"
#include "MemorySpot.generated.h"

class UMemoryData;

UCLASS()
class WORLD_OF_REFRACTION_API AMemorySpot : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	AMemorySpot();

	/** Also fires on the interact key while standing in it. */
	virtual void Interact(APawn *Pawn) override;

	/** The player reached the spot: unlock the memory unless the run already found it here.
	 *  False when nothing happened. */
	bool Reach();

	UPROPERTY(EditInstanceOnly, Category = "Memory")
	TObjectPtr<UMemoryData> Memory;

protected:
	virtual void BeginPlay() override;
	virtual FString GetHintText() const override { return FString(); }

private:
	UFUNCTION()
	void OnSpotBeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp,
							int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult);

	/** Spent for this run: the trigger stops reacting. */
	void Spend();
};
