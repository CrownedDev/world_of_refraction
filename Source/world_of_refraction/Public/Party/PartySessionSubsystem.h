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

#include "Combat/Grid/ECombatRow.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PartySessionSubsystem.generated.h"

class ACombatCharacter;
class APlayerController;
class UCharacterData;
class UParty;
struct FPartyMember;

/** Broadcasts on any composition change: initial creation, invite success,
 *  dismiss success. Consumers should re-read UParty state on fire — the delegate
 *  carries the party pointer for convenience. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyChanged, UParty *, Party);

UCLASS(Config = Game)
class WORLD_OF_REFRACTION_API UPartySessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "Party")
	FOnPartyChanged OnPartyChanged;

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
	 *  if all three fail. BP callers may leave PawnClass unconnected — a null soft
	 *  ref falls through to the same chain.
	 *
	 *  ⚠️ Call this where the player possesses their REAL character — the hub or
	 *  trial — not at battle bootstrap. By then AGameModeBase has already spawned
	 *  PC0 a pawn from the BATTLE level's DefaultPawnClass, so the resolution chain
	 *  would capture the stage default instead of the player's character. That is
	 *  invisible today only because every GameMode shares a DefaultPawnClass. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	UParty *EnsureLocalParty(APlayerController *PC, TSoftClassPtr<ACombatCharacter> PawnClass);

	/** Is this actor one the Trial Party rosters? Compares the actor's
	 *  UCharacterDataComponent->CharacterData against the roster — semantic
	 *  character identity, not pawn class. An actor with no component, or none
	 *  with CharacterData, is not a member.
	 *
	 *  ⚠️ Identity limit: CharacterData is the identity, so two members sharing
	 *  one CharacterData asset are indistinguishable. Per-instance / per-user
	 *  identity (PvP, duplicate-character parties) is a separate arc. */
	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsTrialPartyMember(const AActor *Actor) const;

	/** Returns the slot index of the first member with the given CharacterData
	 *  asset, or INDEX_NONE if not present. CharacterData is WoR's semantic
	 *  character identity (name, stats, class, element). PvP will eventually need
	 *  per-instance/per-user identity — that's a separate arc. */
	UFUNCTION(BlueprintPure, Category = "Party")
	int32 GetMemberSlotByData(UCharacterData *CharacterData) const;

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

	// ==================== FORMATION ====================
	// Authored grid placement, written in the hub and read at battle spawn. This
	// subsystem — not the menu — owns the invariant that no two PLACED members
	// share a cell, so any UI driving it gets the guarantee for free.

	/** Place a member on the formation grid. Column must be in
	 *  [0, GRID_COLUMNS); this is a placement API, not an unplace one — the only
	 *  route back to unplaced is being swapped with an unplaced member.
	 *
	 *  SWAP: when another member already holds (Row, Column), that member takes
	 *  this one's PREVIOUS cell — including UNSET_FORMATION when this member was
	 *  unplaced. A swap is a transposition of two cells, so it can neither
	 *  duplicate a cell nor orphan one.
	 *
	 *  Broadcasts OnPartyChanged on a real change. A no-op (member already in the
	 *  target cell) does not broadcast. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void SetMemberFormation(int32 SlotIndex, ECombatRow Row, int32 Column);

	/** Read a member's authored cell. False when the slot is invalid or the member
	 *  is unplaced, in which case the out params are untouched and the caller
	 *  should fall back to the centre-out auto-assignment. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	bool GetMemberFormation(int32 SlotIndex, ECombatRow &OutRow, int32 &OutColumn) const;

	// ==================== CONTROL (D-004 / D-045) ====================

	/** Player or AI for one member in battle. The leader (slot 0) is always Player,
	 *  so a request to hand it to the AI is refused (false). Broadcasts OnPartyChanged
	 *  on a real change. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	bool SetMemberPlayerControlled(int32 SlotIndex, bool bPlayerControlled);

	/** The member's battle control setting; Player for an invalid slot. */
	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsMemberPlayerControlled(int32 SlotIndex) const;

	/** D-045 setting: true lets the AI finish a fight once every Player member is down. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void SetLetAIFinish(bool bLetAIFinish);

	UFUNCTION(BlueprintPure, Category = "Party")
	bool GetLetAIFinish() const;

	/** Re-point Leader after a level transition — the old PlayerController was
	 *  destroyed by OpenLevel, so the weak ref is stale by design. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void RebindLeader(APlayerController *Leader);

	// ==================== CARRIED VITALS ====================
	// Trial-scoped HP/EP that survives the trial→battle→trial OpenLevel chain.
	// Deliberately NOT serialized: the state dies with the GameInstance, which is
	// the correct lifetime for a trial run.

	/** Write both carried vitals onto a slot. No-op (with a warning) on an invalid
	 *  slot. Values are stored verbatim — clamping is the caller's business. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void SetMemberVitals(int32 SlotIndex, int32 HP, int32 EP);

	/** Read a slot's carried vitals. Returns false when the slot is invalid or
	 *  carries no trial state, in which case the caller falls back to the
	 *  template's CalculateMaxHealth/CalculateMaxEnergy. Out params are untouched
	 *  on failure. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	bool GetMemberVitals(int32 SlotIndex, int32 &OutHP, int32 &OutEP) const;

	/** Trial entry: seed every slot to its computed maxima. Broken-Darkness-innate
	 *  members start with an empty EP pool. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void SetAllMembersFull();

	/** Hub return: drop all carried state back to the sentinel, so display falls
	 *  back to template maxima again. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void ClearAllMemberVitals();

	/** D-043 (B-150): a rest spot brings every fallen member back at full HP/EP (Broken Darkness
	 *  members at empty EP, as at trial entry). Standing members are untouched. Returns how many
	 *  came back. Console: wor.Party.ReviveFallen. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	int32 ReviveFallenMembers();

	/** D-043: the slot's member fell in an earlier fight and sits out the next ones. */
	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsMemberFallen(int32 SlotIndex) const;

	/** Multi-line snapshot of the party and its per-slot carry state — inspectable
	 *  without launching PIE. */
	UFUNCTION(BlueprintPure, Category = "Debug")
	FString GetPartyStateString() const;

	UFUNCTION(BlueprintCallable, Category = "Debug")
	void LogPartyState() const;

private:
	/** A member's carried vitals at its computed maxima (Broken Darkness innate: empty EP). */
	static void SetMemberVitalsFull(FPartyMember &Member);

	/** Internal add path — does the work but does NOT broadcast. Callers that
	 *  compose multiple adds into a single logical operation (e.g. CreateSoloParty)
	 *  call this directly, then broadcast once. External callers use InviteMember. */
	bool InviteMemberInternal(TSoftClassPtr<ACombatCharacter> PawnClass);

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
