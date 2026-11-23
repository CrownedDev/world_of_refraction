// TrialRestSpot.h
// B-187 part 1 (RunStructure §4, D-043, D-097, Q-187): a rest spot on the trial map. Using it rests the party: every
// member back to full HP / EP and the fallen revived (D-043), and it becomes the run's last rest spot (where a wipe
// sends you back, D-155). It opens the placeholder rest menu (URestSpotWidget): inventory, and leaving the trial
// here for a reduced payout ("full rewards only for beating the boss or falling in battle"). B-187b: a beaten
// mini-boss leaves one where it stood, whose menu can revive it one tier stronger (Q-187b). Placing pillar levels
// here is B-363.
// B-502 (D-214, Q-502): a placed rest spot is also a service (ERestService): an anvil is the Blacksmith, a ritual
// tree the Spiritualist, a crystal pedestal the Jeweler, a shrine sells pillar points (one of the placed shrines each run, the same one for the
// whole run), V extracts the party.
// Resting and the service open from the same press: the rest menu carries the service. A rest spot a beaten
// mini-boss left is unchanged and has no service.
// Debug: wor.Trial.Rest (rests the party anywhere in a run, as the spot "ConsoleRest", and opens the menu);
// wor.Trial.PrintRestSpots; wor.Trial.SetRestService <none|anvil|tree|pedestal|shrine|v> (the nearest rest spot, for a PIE check
// before the services are set in the maps).

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialMapFeature.h"
#include "TrialRestSpot.generated.h"

class ATrialRestSpot;
class UCurrencyComponent;
class UMerchantData;
class UWorld;
class AWoRPlayerState;
struct FPartyMember;

/** B-502 (D-214): what a placed rest point also is. */
UENUM(BlueprintType)
enum class ERestService : uint8
{
	None UMETA(DisplayName = "None (rest only)"),
	Blacksmith UMETA(DisplayName = "Blacksmith (an anvil)"),
	PillarShrine UMETA(DisplayName = "Pillar shrine (one of the shrines sells points each run)"),
	Extract UMETA(DisplayName = "Extract (V, or V's mirror)"),
	Spiritualist UMETA(DisplayName = "Spiritualist (a ritual tree)"),
	Jeweler UMETA(DisplayName = "Jeweler (a crystal pedestal)")
};

enum class EMerchantType : uint8;

namespace WoRRestSpot
{
	// B-502 (D-214): the services. "Here" is the placed rest spot the host is resting at now (FindRestingSpot).

	/** The placed rest spot the party last rested at; null outside a run, or when that was a mini-boss's spot that is
	 *  gone or the console's rest. It stays set after the party walks on (a wipe returns there, D-155). */
	WORLD_OF_REFRACTION_API const ATrialRestSpot *FindLastRestSpot(const UWorld *World);

	/** The placed rest spot the host is resting at now: the last one rested at, while that rest lasts (its menu or
	 *  level screen is open: UTrialRunSubsystem::IsAtRestSpot) and the host's pawn still stands in it. Null once the
	 *  host steps away, and while only a guest rested somewhere else. A service is offered only here. */
	WORLD_OF_REFRACTION_API const ATrialRestSpot *FindRestingSpot(const UWorld *World);

	/** The vendor a service is (an anvil: the Blacksmith; a ritual tree: the Spiritualist; a crystal pedestal: the
	 *  Jeweler). False for a service
	 *  with no shop of its own. */
	WORLD_OF_REFRACTION_API bool GetServiceVendor(ERestService Service, EMerchantType &OutVendor);

	/** Which of Count shrines sells pillar points this run (0 with fewer than two). The seed alone decides. */
	WORLD_OF_REFRACTION_API int32 PickShrineIndex(int32 RunSeed, int32 Count);

	/** The shrine in World that sells pillar points this run; null outside a run or with none placed. A guest finds
	 *  the same one as the host (the run's seed is on the GameState). */
	WORLD_OF_REFRACTION_API const ATrialRestSpot *FindAwakeShrine(const UWorld *World);

	/** The host is resting at this run's shrine (FindRestingSpot). */
	WORLD_OF_REFRACTION_API bool SellsPointsHere(const UWorld *World);

	/** Buys Member one pillar point here (WoRLevelDrops::TryBuyPoint: its price and refusals). Empty on success, else
	 *  why not (not at this run's shrine; a client: the run is the host's). */
	WORLD_OF_REFRACTION_API FText TryBuyPointHere(UWorld *World, const FPartyMember &Member, UCurrencyComponent *Wallet = nullptr);

	/** Every rest spot placed in World: its service, where it stands, which shrine sells this run. */
	WORLD_OF_REFRACTION_API FString GetRestSpotsString(const UWorld *World);

	/** Rests at RestSpotId: the presser's characters back to full (the fallen revived), the run's last rest spot. Returns
	 *  how many were revived; INDEX_NONE outside a run. Q-187d (Crown, 2026-11-08): only the characters Presser
	 *  commands rest (RestsMember); a null Presser (console, specs) rests the whole party. */
	WORLD_OF_REFRACTION_API int32 Rest(UWorld *World, FName RestSpotId, const AWoRPlayerState *Presser = nullptr);

	/** Q-187d: whether Presser's press rests Member: Presser claims it (by member id or character), or Presser is the host
	 *  and no player in Players claims it (unclaimed members and AI seats are the host's; solo, that is the whole party). */
	WORLD_OF_REFRACTION_API bool RestsMember(const FPartyMember &Member, const AWoRPlayerState *Presser, bool bPresserIsHost,
											 const TArray<const AWoRPlayerState *> &Players);

	/** B-448 (D-186): the party may extract here: the run's last rest spot is one a beaten boss left (ER-09c), or
	 *  (B-502, D-214) the host is resting at V's rest point (FindRestingSpot). Any other rest spot has no Extract. */
	WORLD_OF_REFRACTION_API bool CanExtractHere(const UWorld *World);

	/** Extracts here: the run's end pays the tally at RunRewards::LEAVE_EARLY_SHARE, then the hub. False outside a run
	 *  (a client has none: the run lives on the host), on a client, or (B-448, D-186) where CanExtractHere is false.
	 *  A mini-boss rest spot's id is its encounter id (ATrialRestSpot::GetRestSpotId), which this relies on. */
	WORLD_OF_REFRACTION_API bool LeaveRun(UWorld *World);

	/** B-363 review: the player walks away from the rest spot (its menu or level screen closed): pillar points can't
	 *  be placed, and skins can't be switched, until the next rest. */
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

	/** B-589 (Q-450 = B, D-187): who a Revive brings back, by name, never its rank: the last used rest spot's opponent
	 *  (ATrialRestSpot::GetOpponentName), or "opponent" when it has no name. The lines read "the {name}". */
	WORLD_OF_REFRACTION_API FText GetReviveOpponentName(const UWorld *World);

	/** B-589: the rest menu's revive row, "Revive the Ash Warden (1 of 3)". */
	WORLD_OF_REFRACTION_API FText MakeReviveRowText(const FText &OpponentName, int32 RevivalNumber);

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
	/** "Rest", or "Rest · Anvil" at a service. */
	virtual FInteractPrompt GetPrompt() const override;

	/** B-502: this spot's service. A mini-boss's rest spot has none, whatever was set. */
	ERestService GetService() const { return MiniBossEncounterId.IsNone() ? Service : ERestService::None; }
	/** For a spawned spot (the debug command, the specs). Placed ones set it in the Details panel. */
	void SetService(ERestService InService);
	/** The stock of a vendor rest point's Buy tab; null when none is set. */
	UMerchantData *GetServiceMerchant() const { return ServiceMerchant; }

	/** B-187b: a rest spot where the beaten mini-boss EncounterId stood (its id is the encounter's, so the menu can
	 *  revive it). B-589 (Q-450): OpponentName is its leader's name, which the revive row and vote say. Null when the
	 *  spawn fails. */
	static ATrialRestSpot *SpawnForMiniBoss(UWorld *World, const FTransform &Where, FName EncounterId,
											const FText &OpponentName = FText::GetEmpty());

	/** B-589: the beaten mini-boss's name on its rest spot (SpawnForMiniBoss); empty on any other spot. */
	const FText &GetOpponentName() const { return OpponentName; }

	/** The id the run records for this spot: a mini-boss's encounter id, else its own feature id. */
	FName GetRestSpotId() const { return MiniBossEncounterId.IsNone() ? GetFeatureId() : MiniBossEncounterId; }

protected:
	virtual FString GetHintText() const override;

	/** B-502 (D-214): what this rest point also is. Ignored on a mini-boss's rest spot. */
	UPROPERTY(EditInstanceOnly, Category = "Rest Spot")
	ERestService Service = ERestService::None;

	/** A vendor rest point's stock (the Blacksmith's, the Spiritualist's or the Jeweler's merchant asset). In a
	 *  trial its Buy tab shows the run's rolled part of that list (ER-18). */
	UPROPERTY(EditInstanceOnly, Category = "Rest Spot", meta = (EditCondition = "Service == ERestService::Blacksmith || Service == ERestService::Spiritualist || Service == ERestService::Jeweler"))
	TObjectPtr<UMerchantData> ServiceMerchant;

private:
	/** B-187b: set when this spot marks a beaten mini-boss (SpawnForMiniBoss). */
	FName MiniBossEncounterId;

	/** B-589: SpawnForMiniBoss's OpponentName. */
	FText OpponentName;
};
