// TrialNPC.h
// B-449 (D-186, Q-449; Crown 2026-11-14): a character met on the trial map. Talking to one opens its placeholder talk
// panel (UTrialNPCWidget) with what it offers. V extracts the party: the run ends at the half share
// (RunRewards::LEAVE_EARLY_SHARE) exactly as the boss rest spot's Extract does (UTrialRunSubsystem::LeaveRunEarly),
// with no charge on top. V's Extract works wherever V stands: it is the second way out, not tied to a beaten boss.
// A placed actor (ATrialMapFeature). Placing rule (Q-449, a level-design rule, not checked in code): at most one V
// per trial map, at a rest spot not tied to a boss.
// B-447 (D-183, Q-447): one of the trial's NPCs sells pillar points each run (WoRTrialNPC::FindPointSeller); its panel
// has "Buy a point". The rest spot's level screen only places points now.
// Debug: wor.Trial.PrintNPCs logs GetTrialNPCsString; wor.Trial.SpawnV puts a V at the player for a PIE check;
// wor.Trial.SpawnNPC <name> puts another character there (two or more NPCs show the rotation).

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialMapFeature.h"
#include "TrialNPC.generated.h"

class UCurrencyComponent;
struct FPartyMember;

UCLASS()
class WORLD_OF_REFRACTION_API ATrialNPC : public ATrialMapFeature
{
	GENERATED_BODY()

public:
	ATrialNPC();

	/** Opens the talk panel for the pressing player (a guest's panel greys Extract: the run is the host's). */
	virtual void Interact(APawn *Pawn) override;
	virtual FInteractPrompt GetPrompt() const override { return FInteractPrompt(EInteractVerb::Talk, DisplayName); }

	const FText &GetDisplayName() const { return DisplayName; }
	bool ExtractsParty() const { return bExtractsParty; }
	/** For a spawned NPC (the debug commands, the specs): who it is and whether it extracts. Placed ones set these
	 *  in the Details panel. */
	void SetIdentity(const FText &InDisplayName, bool bInExtractsParty)
	{
		DisplayName = InDisplayName;
		bExtractsParty = bInExtractsParty;
	}

protected:
	virtual FString GetHintText() const override;

	/** Who this is. The default is V, the only trial NPC Crown has named (Q-449). */
	UPROPERTY(EditInstanceOnly, Category = "Trial NPC")
	FText DisplayName;

	/** V's service: talking offers Extract. Off for any other character. */
	UPROPERTY(EditInstanceOnly, Category = "Trial NPC")
	bool bExtractsParty = true;
};

namespace WoRTrialNPC
{
	/** Why the party can't extract at an NPC now (no run; a client: the run is the host's); empty when it can. */
	WORLD_OF_REFRACTION_API FText GetExtractRefusal(const UWorld *World);

	/** Extracts the party: the run's end pays the tally at RunRewards::LEAVE_EARLY_SHARE, then the hub. False when
	 *  refused (GetExtractRefusal). */
	WORLD_OF_REFRACTION_API bool Extract(UWorld *World);

	// B-447 (D-183, Q-447): one of the trial's NPCs sells pillar points each run. Which one is fixed by the run's seed
	// and the NPCs placed in the level, so it holds for the whole run and changes between runs; nothing marks it, the
	// player finds it by talking. V counts as one of the NPCs.

	/** Which of Count NPCs sells this run (0 with fewer than two). */
	WORLD_OF_REFRACTION_API int32 PickSellerIndex(int32 RunSeed, int32 Count);

	/** The NPC in World that sells pillar points this run; null outside a run or with none placed. */
	WORLD_OF_REFRACTION_API const ATrialNPC *FindPointSeller(const UWorld *World);

	/** Buys Member one pillar point from NPC (WoRLevelDrops::TryBuyPoint: its price and refusals). Empty on success,
	 *  else why not (NPC isn't this run's seller; a client: the run is the host's). */
	WORLD_OF_REFRACTION_API FText TryBuyPointAt(UWorld *World, const ATrialNPC &NPC, const FPartyMember &Member, UCurrencyComponent *Wallet = nullptr);

	/** Every NPC placed in World: its name, what it offers, where it stands. */
	WORLD_OF_REFRACTION_API FString GetTrialNPCsString(const UWorld *World);
}
