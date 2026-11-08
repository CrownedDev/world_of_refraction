// TrialRestSpot.h
// B-187 part 1 (RunStructure §4, D-043, D-097, Q-187): a rest spot on the trial map. Using it rests the party: every
// member back to full HP / EP and the fallen revived (D-043), and it becomes the run's last rest spot (where a wipe
// sends you back, D-155). It opens the placeholder rest menu (URestSpotWidget): inventory, and leaving the trial
// here for a reduced payout ("full rewards only for beating the boss or falling in battle"). B-187b: a beaten
// mini-boss leaves one where it stood, whose menu can revive it one tier stronger (Q-187b). Placing pillar levels
// here is B-363.
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

	/** The confirm text for leaving (RunStructure §6). */
	WORLD_OF_REFRACTION_API FText GetLeaveConfirmText();

	/** B-187b (Q-187b): revives the mini-boss whose rest spot the party last used (the run's last rest spot) and reloads
	 *  the trial level so it stands there again, one tier stronger. Free; at most MiniBossRevival::MAX_REVIVALS times.
	 *  False when that spot's mini-boss can't be revived. */
	WORLD_OF_REFRACTION_API bool ReviveMiniBoss(UWorld *World);

	/** Whether the last used rest spot is a mini-boss's that can be revived (the menu shows Revive). */
	WORLD_OF_REFRACTION_API bool CanReviveHere(const UWorld *World);

	/** B-187e: which revival the next Revive would be at the last used rest spot (1 for the first). */
	WORLD_OF_REFRACTION_API int32 GetNextRevivalNumber(const UWorld *World);

	/** B-187e (review): how near the rest stand a client must be for the host to take its Revive (cm). */
	constexpr float CLIENT_REVIVE_REACH_CM = 600.f;
}

UCLASS()
class WORLD_OF_REFRACTION_API ATrialRestSpot : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	/** Rests the party and opens the rest menu for the pressing player. */
	virtual void Interact(APawn *Pawn) override;

	/** B-187b: a rest spot where the beaten mini-boss EncounterId stood (its id is the encounter's, so the menu can
	 *  revive it). Null when the spawn fails. */
	static ATrialRestSpot *SpawnForMiniBoss(UWorld *World, const FTransform &Where, FName EncounterId);

	/** The id the run records for this spot: a mini-boss's encounter id, else its own feature id. */
	FName GetRestSpotId() const { return MiniBossEncounterId.IsNone() ? GetFeatureId() : MiniBossEncounterId; }

protected:
	virtual FString GetHintText() const override;

private:
	/** B-187b: set when this spot marks a beaten mini-boss (SpawnForMiniBoss). */
	FName MiniBossEncounterId;
};
