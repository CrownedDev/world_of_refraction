// PrismDropPickup.h
// B-362c (D-160 as changed): the run's dropped Prisms lie here, where the party stood on the trial map when the fight
// they lost started (UTrialRunSubsystem::GetDropSpot; the trial's start when that fight had no map spot). Walking
// into it, or the interact key, puts the whole drop back in the wallet (WoRDefeat::PickUpDrop). The run places it
// each time the trial level loads while a drop exists (PlaceForRun); only one ever exists. Placeholder look: a small
// cube. TODO(B-235 co-op): each player's own pickup. Debug: wor.Trial.PrintLosses.

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialMapFeature.h"
#include "PrismDropPickup.generated.h"

class UTrialRunSubsystem;

UCLASS()
class WORLD_OF_REFRACTION_API APrismDropPickup : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	APrismDropPickup();

	virtual void Interact(APawn *Pawn) override;
	virtual FInteractPrompt GetPrompt() const override;

	/** Takes Pawn's player's own drop into their wallet (B-235; null: the local player's, as before) and removes the
	 *  pickup once no player's drop is left. False when there was nothing of theirs to take. */
	bool PickUp(APawn *Pawn = nullptr);

	/** Removes any pickup already in World, then places one for Run's drop (null when there is none). */
	static APrismDropPickup *PlaceForRun(UWorld *World, UTrialRunSubsystem &Run);

	/** How many pickups lie in World (tests, debug). */
	static int32 CountIn(const UWorld *World);

protected:
	virtual void BeginPlay() override;
	virtual FString GetHintText() const override;

private:
	UFUNCTION()
	void OnPickupBeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp,
							  int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult);

	UFUNCTION()
	void OnPickupEndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp,
							int32 OtherBodyIndex);

	/** A pawn that arrived standing on it (the level's spawn) takes it only after stepping out and back, or with the
	 *  interact key: coming back to the trial must not grab it by itself. */
	TSet<TWeakObjectPtr<APawn>> ArrivedInside;

	double PlacedTime = 0.0;
};
