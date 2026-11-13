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
class AWoRPlayerState;
struct FPartyMember;

namespace WoRRestSpot
{
	/** Rests at RestSpotId: the presser's characters back to full (the fallen revived), the run's last rest spot. Returns
	 *  how many were revived; INDEX_NONE outside a run. Q-187d (Crown, 2026-11-08): only the characters Presser
	 *  commands rest (RestsMember); a null Presser (console, specs) rests the whole party. */
	WORLD_OF_REFRACTION_API int32 Rest(UWorld *World, FName RestSpotId, const AWoRPlayerState *Presser = nullptr);

	/** Q-187d: whether Presser's press rests Member: Presser claims it (by member id or character), or Presser is the host
	 *  and no player in Players claims it (unclaimed members and AI seats are the host's; solo, that is the whole party). */
	WORLD_OF_REFRACTION_API bool RestsMember(const FPartyMember &Member, const AWoRPlayerState *Presser, bool bPresserIsHost,
											 const TArray<const AWoRPlayerState *> &Players);

	/** Leaves the run here: the run's end pays the tally at RunRewards::LEAVE_EARLY_SHARE, then the hub. False
	 *  outside a run. */
	WORLD_OF_REFRACTION_API bool LeaveRun(UWorld *World);

	/** B-363 review: the player walks away from the rest spot (its menu or level screen closed): pillar points can't
	 *  be placed or bought, and skins can't be switched, until the next rest. */
	WORLD_OF_REFRACTION_API void StepAway(UWorld *World);

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
	virtual FInteractPrompt GetPrompt() const override { return FInteractPrompt(EInteractVerb::Rest); }

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
