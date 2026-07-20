// Party.h
// UParty — the player-side roster that survives level transitions (Encounter
// Composition, Arc 1 Foundation).
//
// A UObject rather than a USTRUCT so it has real identity: UBattleConfigComponent
// holds a TWeakObjectPtr<UParty> back-reference, which a struct cannot provide, and
// it gives the type somewhere to grow replication later.
//
// ⚠️ OUTER MUST BE THE GAMEINSTANCE. UPartySessionSubsystem creates parties with
// GetGameInstance() as outer. A UObject outered to the world is destroyed by
// OpenLevel — the same trap UTrialRunSubsystem documents for runtime-created
// CharacterData — and the party would silently empty on the first hub→trial swap.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Party.generated.h"

class ACombatCharacter;
class APlayerController;
class UCharacterData;

namespace PartyConstants
{
	/** Active party size. No reserves — a dead member is out until revived. */
	constexpr int32 MAX_PARTY_MEMBERS = 3;
}

/** One party slot. Pawn class is the authored identity; CharacterData is resolved
 *  from that class's CDO at invite time and cached so the battle stash (which
 *  takes UCharacterData*) does not have to re-load the class mid-transition. */
USTRUCT(BlueprintType)
struct FPartyMember
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	TSoftClassPtr<ACombatCharacter> PawnClass;

	/** HARD ref — pins the authored asset across OpenLevel, matching how
	 *  UTrialRunSubsystem pins its pending rosters. */
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	TObjectPtr<UCharacterData> CharacterData = nullptr;

	bool IsValidMember() const { return !PawnClass.IsNull() && CharacterData != nullptr; }
};

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UParty : public UObject
{
	GENERATED_BODY()

public:
	/** Defaults to "<Leader>'s Party"; SetDisplayName overrides. */
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	FText DisplayName;

	/** WEAK: a non-seamless OpenLevel destroys and recreates the PlayerController,
	 *  so a hard ref would dangle after the first transition. Treat as valid only
	 *  within the level that set it — see UPartySessionSubsystem::RebindLeader. */
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	TWeakObjectPtr<APlayerController> Leader;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	TArray<FPartyMember> Members;

	UFUNCTION(BlueprintPure, Category = "Party")
	int32 GetMemberCount() const { return Members.Num(); }

	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsFull() const { return Members.Num() >= PartyConstants::MAX_PARTY_MEMBERS; }

	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsEmpty() const { return Members.Num() == 0; }

	/** Flattened roster in slot order, for the battle stash / orchestrator, which
	 *  take TArray<UCharacterData*>. Skips slots whose asset failed to resolve. */
	UFUNCTION(BlueprintPure, Category = "Party")
	TArray<UCharacterData *> GetMemberCharacterData() const;
};
