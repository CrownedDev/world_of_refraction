// TrialNPC.h
// B-449 (D-186, Q-449; Crown 2026-11-14): a character met on the trial map. Talking to one opens its placeholder talk
// panel (UTrialNPCWidget) with what it offers. V extracts the party: the run ends at the half share
// (RunRewards::LEAVE_EARLY_SHARE) exactly as the boss rest spot's Extract does (UTrialRunSubsystem::LeaveRunEarly),
// with no charge on top. V's Extract works wherever V stands: it is the second way out, not tied to a beaten boss.
// A placed actor (ATrialMapFeature). Placing rule (Q-449, a level-design rule, not checked in code): at most one V
// per trial map, at a rest spot not tied to a boss.
// Debug: wor.Trial.PrintNPCs logs GetTrialNPCsString; wor.Trial.SpawnV puts a V at the player for a PIE check.

#pragma once

#include "CoreMinimal.h"
#include "Trial/TrialMapFeature.h"
#include "TrialNPC.generated.h"

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

	/** Every NPC placed in World: its name, what it offers, where it stands. */
	WORLD_OF_REFRACTION_API FString GetTrialNPCsString(const UWorld *World);
}
