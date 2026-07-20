// PartySessionSubsystem.h
// Owns the player-side party across the session (Encounter Composition, Arc 1).
//
// GameInstance-scoped for the same reason UTrialRunSubsystem is: the party must
// survive the hub→trial→battle→trial OpenLevel chain, and nothing actor-resident
// does. Parties are created with the GameInstance as outer so they survive too.
//
// Arc 1 ships a party-of-1 stub: CreateSoloParty seeds the leader's own pawn as
// the sole member. AI companion fill and the invite UI land in Arc 2 — InviteMember
// / DismissMember exist now so ABattleGameMode can be written against the final
// shape rather than a temporary one.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PartySessionSubsystem.generated.h"

class ACombatCharacter;
class APlayerController;
class UCharacterData;
class UParty;

UCLASS(Config = Game)
class WORLD_OF_REFRACTION_API UPartySessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Last-resort pawn class for the solo Trial Party when neither an explicit
	 *  class nor PC0's current pawn can supply one. Config-authored (DefaultGame.ini)
	 *  because a subsystem has no asset for a designer to edit — same pattern as
	 *  UTrialRunSubsystem::HubLevel. */
	UPROPERTY(Config)
	TSoftClassPtr<ACombatCharacter> DefaultSoloPawnClass;

	/** The local player's Trial Party. Pure query — null when none exists yet.
	 *  Use EnsureLocalParty when you need one to exist. */
	UFUNCTION(BlueprintPure, Category = "Party")
	UParty *GetLocalParty() const;

	/** Get-or-create the Trial Party. Pawn class resolves in order: PawnClass →
	 *  PC's current pawn class → DefaultSoloPawnClass. Returns null (with an error)
	 *  if all three fail.
	 *
	 *  ⚠️ Call this where the player possesses their REAL character — the hub or
	 *  trial — not at battle bootstrap. By then AGameModeBase has already spawned
	 *  PC0 a pawn from the BATTLE level's DefaultPawnClass, so the resolution chain
	 *  would capture the stage default instead of the player's character. That is
	 *  invisible today only because every GameMode shares a DefaultPawnClass. */
	UParty *EnsureLocalParty(APlayerController *PC, TSoftClassPtr<ACombatCharacter> PawnClass);

	/** Is this actor's class one the Trial Party rosters?
	 *
	 *  ⚠️ POC limit: membership is CLASS-based, so two members of the same class
	 *  are indistinguishable. Instance identity arrives with the Arc 2 grid work
	 *  (unique row slot per member). */
	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsTrialPartyMember(const AActor *Actor) const;

	/** Create (or reset) the local party with Leader as its owner, seeded with
	 *  LeaderPawnClass as member 0. Idempotent per level: calling it again on an
	 *  existing party rebinds the leader and leaves membership intact, so a hub
	 *  BeginPlay re-entry does not wipe a party assembled earlier in the session. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	UParty *CreateSoloParty(APlayerController *Leader, TSoftClassPtr<ACombatCharacter> LeaderPawnClass);

	/** Append a member. Fails when the party is full, the class is null, or the
	 *  class's CDO carries no CharacterData. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	bool InviteMember(TSoftClassPtr<ACombatCharacter> PawnClass);

	UFUNCTION(BlueprintCallable, Category = "Party")
	bool DismissMember(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Party")
	void SetDisplayName(const FText &NewName);

	/** Re-point Leader after a level transition — the old PlayerController was
	 *  destroyed by OpenLevel, so the weak ref is stale by design. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void RebindLeader(APlayerController *Leader);

private:
	/** Resolve the CharacterData a pawn class ships with, by loading the class and
	 *  reading its CDO's CharacterDataComponent. Synchronous — invites happen at
	 *  hub interaction speed, not in a frame-critical path. */
	static UCharacterData *ResolveCharacterData(const TSoftClassPtr<ACombatCharacter> &PawnClass);

	static FText MakeDefaultDisplayName(const APlayerController *Leader);

	/** HARD refs — the subsystem owns party lifetime. Array (not a single ptr)
	 *  because multiplayer adds remote parties; Arc 1 only ever fills index 0. */
	UPROPERTY()
	TArray<TObjectPtr<UParty>> Parties;
};
