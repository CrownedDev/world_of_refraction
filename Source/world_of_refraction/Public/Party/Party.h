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

	/** Sentinel for FPartyMember::CurrentHP/CurrentEP: no carried trial state.
	 *  Distinguishable from a legitimately carried 0 (a downed member, or a
	 *  Broken Darkness member's empty EP pool). */
	constexpr int32 NO_CARRIED_VITALS = -1;
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

	/** Carried vitals across the trial→battle→trial chain. NO_CARRIED_VITALS (-1)
	 *  means no trial state has been carried yet — display falls back to the
	 *  template's CalculateMaxHealth/CalculateMaxEnergy, so a hub member reads
	 *  full. >= 0 is a real carried value from an in-trial battle.
	 *
	 *  The bar always renders for a real member; the sentinel only changes which
	 *  number it shows. Slot vacancy is the display's separate concern, not a
	 *  vitals state. */
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	int32 CurrentHP = PartyConstants::NO_CARRIED_VITALS;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	int32 CurrentEP = PartyConstants::NO_CARRIED_VITALS;

	bool IsValidMember() const { return !PawnClass.IsNull() && CharacterData != nullptr; }

	bool HasCarriedVitals() const { return CurrentHP != PartyConstants::NO_CARRIED_VITALS; }
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
	 *  take TArray<UCharacterData*>. Skips slots whose asset failed to resolve.
	 *
	 *  ⚠️ Includes the dead. The battle path wants GetLivingMemberCharacterData. */
	UFUNCTION(BlueprintPure, Category = "Party")
	TArray<UCharacterData *> GetMemberCharacterData() const;

	/** As GetMemberCharacterData, minus members who died in an earlier battle of
	 *  this trial. This is the roster the Battle Party is built from.
	 *
	 *  Dead is a CARRIED zero — HasCarriedVitals() first, so the NO_CARRIED_VITALS
	 *  sentinel (-1, a fresh trial with nothing banked yet) is never mistaken for a
	 *  corpse. Returns empty on a full party wipe; callers must handle that rather
	 *  than passing it on as a battle roster. */
	UFUNCTION(BlueprintPure, Category = "Party")
	TArray<UCharacterData *> GetLivingMemberCharacterData() const;
};
