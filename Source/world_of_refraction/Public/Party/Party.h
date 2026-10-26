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

#include "Combat/Grid/ECombatRow.h"
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

	/** The leader's slot — always Player-controlled (D-004), never dismissed. */
	constexpr int32 LEADER_SLOT = 0;

	/** Sentinel for FPartyMember::CurrentHP/CurrentEP: no carried trial state.
	 *  Distinguishable from a legitimately carried 0 (a downed member, or a
	 *  Broken Darkness member's empty EP pool). */
	constexpr int32 NO_CARRIED_VITALS = -1;

	/** Sentinel for FPartyMember::PreferredColumn: the member has not been placed
	 *  on the formation grid. An unplaced member falls back to
	 *  UCombatGridSubsystem::AssignTeamWithPreferences' centre-out placement at fight
	 *  start; a placed member starts in its cell (D-042, B-149).
	 *
	 *  Column alone carries the unplaced state — ECombatRow has no "unset" value,
	 *  and adding one would ripple into the row modifier switches in
	 *  FCombatGridPosition. PreferredRow is meaningless while this is set. */
	constexpr int32 UNSET_FORMATION = -1;
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

	/** Authored formation cell. Only meaningful when HasFormation() — see
	 *  UNSET_FORMATION. Written by UPartySessionSubsystem::SetMemberFormation,
	 *  which owns the no-two-members-per-cell invariant; do not assign directly. */
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	ECombatRow PreferredRow = ECombatRow::Middle;

	UPROPERTY(BlueprintReadOnly, Category = "Party")
	int32 PreferredColumn = PartyConstants::UNSET_FORMATION;

	/** D-004: who plays this member in battle — you (the command menu) or the AI.
	 *  The leader (slot 0) is always Player; written by
	 *  UPartySessionSubsystem::SetMemberPlayerControlled, which enforces that. */
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	bool bPlayerControlled = true;

	bool IsValidMember() const { return !PawnClass.IsNull() && CharacterData != nullptr; }

	bool HasCarriedVitals() const { return CurrentHP != PartyConstants::NO_CARRIED_VITALS; }

	bool HasFormation() const { return PreferredColumn != PartyConstants::UNSET_FORMATION; }
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

	/** D-045 Gameplay setting. False (default, "Take over"): when every Player
	 *  member has fallen you take over the AI members still standing. True ("Let AI
	 *  finish"): the AI plays the fight out. Not saved yet.
	 *  TODO(D-024): moves to the CommonUI Settings screen and the settings save. */
	UPROPERTY(BlueprintReadOnly, Category = "Party")
	bool bLetAIFinish = false;

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
