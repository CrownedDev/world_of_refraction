// TrialRestSpot.h
// B-187 part 1 (RunStructure §4, D-043, D-097, Q-187): a rest spot on the trial map. Using it rests the party: every
// member back to full HP / EP and the fallen revived (D-043), and it becomes the run's last rest spot (where a wipe
// sends you back, D-155). It opens the placeholder rest menu (URestSpotWidget): inventory, and leaving the trial
// here for a reduced payout ("full rewards only for beating the boss or falling in battle"). Mini-boss rest spots and
// mini-boss revival are B-187b; placing pillar levels here is B-363.
// Debug: wor.Trial.Rest (rests the party anywhere in a run, as the spot "ConsoleRest", and opens the menu).

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialMapFeature.h"
#include "TrialRestSpot.generated.h"

class UWorld;

namespace WoRRestSpot
{
	/** Rests the party at RestSpotId: revive the fallen, every member to full, the run's last rest spot. Returns how
	 *  many members were revived; INDEX_NONE outside a run. */
	WORLD_OF_REFRACTION_API int32 Rest(UWorld *World, FName RestSpotId);

	/** Leaves the run here: the run's end pays the tally at RunRewards::LEAVE_EARLY_SHARE, then the hub. False
	 *  outside a run. */
	WORLD_OF_REFRACTION_API bool LeaveRun(UWorld *World);

	/** B-363 review: the player walks away from the rest spot (its menu or level screen closed): pillar points can't
	 *  be placed or bought, and skins can't be switched, until the next rest. */
	WORLD_OF_REFRACTION_API void StepAway(UWorld *World);

	/** The confirm text for leaving (RunStructure §6). */
	WORLD_OF_REFRACTION_API FText GetLeaveConfirmText();
}

UCLASS()
class WORLD_OF_REFRACTION_API ATrialRestSpot : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	/** Rests the party and opens the rest menu for the pressing player. */
	virtual void Interact(APawn *Pawn) override;

protected:
	virtual FString GetHintText() const override;
};
