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
#include "AI/EAIDifficulty.h"
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h" // ENetMode (B-232e)
#include "Subsystems/GameInstanceSubsystem.h"
#include "PartySessionSubsystem.generated.h"

class ACombatCharacter;
class APlayerController;
class UCharacterData;
class UCharacterDataComponent;
class UParty;
struct FPartyMember;
struct FCarriedGearState;
struct FCarriedAbsorptionState;
enum class EGuardStyle : uint8;
enum class EParryElementPriority : uint8;

/** Broadcasts on any composition change: initial creation, invite success,
 *  dismiss success. Consumers should re-read UParty state on fire — the delegate
 *  carries the party pointer for convenience. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyChanged, UParty *, Party);

namespace PartySeats
{
	/** B-232e (D-150 #3): a recruit found in a run joins as an AI seat in a networked session (listen / dedicated
	 *  server: every slot is a player's character or an AI seat); solo (standalone) you command it, as before. */
	WORLD_OF_REFRACTION_API bool RecruitIsAISeat(ENetMode NetMode);
}

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

	/** B-360b: the slot of the Occurrence-th member (0 = first) with that asset, or INDEX_NONE: two copies of one
	 *  character stand in the battle party as two equal entries, and the n-th entry is the n-th copy. bLivingOnly skips
	 *  fallen members (D-043), the way UParty::GetLivingMemberCharacterData built that list. */
	int32 GetNthMemberSlotByData(const UCharacterData *CharacterData, int32 Occurrence, bool bLivingOnly = false) const;

	/** B-360b: the slot of the member with MemberId, or INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "Party")
	int32 GetMemberSlotById(const FGuid &MemberId) const;

	/** B-360b: the slot a pawn stands for: its component's PartyMemberId when set (a battle pawn; a copy of a character
	 *  shared with another member resolves to its own slot; INDEX_NONE when that member is gone, never another copy's
	 *  slot), else the first member with its CharacterData (a hub pawn). */
	int32 GetMemberSlotOf(const UCharacterDataComponent *Comp) const;

	/** Create (or reset) the local party with Leader as its owner, seeded with
	 *  LeaderPawnClass as member 0. Idempotent per level: calling it again on an
	 *  existing party rebinds the leader and leaves membership intact, so a hub
	 *  BeginPlay re-entry does not wipe a party assembled earlier in the session. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	UParty *CreateSoloParty(APlayerController *Leader, TSoftClassPtr<ACombatCharacter> LeaderPawnClass);

	/** B-090: rebuild the local party from a save, before any level has made one. No leader
	 *  yet: the first CreateSoloParty / EnsureLocalParty rebinds it. Null (and no change) when a
	 *  local party already exists or Members is empty. */
	UParty *RestoreLocalParty(const FText &DisplayName, bool bLetAIFinish, const TArray<FPartyMember> &Members);

	/** B-234b: drops the local party (a new host takes the old host's party from its checkpoint copy, then
	 *  RestoreLocalParty builds it). Broadcasts OnPartyChanged with null. */
	void ForgetLocalParty();

	/** Append a member. Fails when the party is full, the class is null, or the
	 *  class's CDO carries no CharacterData. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	bool InviteMember(TSoftClassPtr<ACombatCharacter> PawnClass);

	/** B-075: append a member as Data, the character a placed pawn actually carries (it may
	 *  override its class default). Same checks as InviteMember. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	bool InviteMemberWithData(TSoftClassPtr<ACombatCharacter> PawnClass, UCharacterData *Data);

	UFUNCTION(BlueprintCallable, Category = "Party")
	bool DismissMember(int32 SlotIndex);

	/** B-076 (D-026, Q-076): a character found during a trial joins for this run only, ungeared,
	 *  at full HP/EP. Console: wor.Party.RecruitForRun <CharacterData path>. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	bool InviteRunMember(TSoftClassPtr<ACombatCharacter> PawnClass, UCharacterData *Data);

	/** B-076: the run ended; every run-only member leaves. Returns how many left. B-361 (D-152): its skills go to
	 *  the host's character (UPoolSubsystem::HandOverSkills); its pickups and pouch were the host's all along. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	int32 DismissRunMembers();

	/** B-361 (D-152, Q-361): the host's own character's pool key: the first member who isn't a run-only recruit (the
	 *  party leader in solo play, the hosting player's character in co-op). None without a party. */
	FName GetHostKey() const;
	/** B-361 (D-152): whose pouch Key's member carries: a run-only recruit's is the host's, anyone else's its own. */
	FName GetPouchKey(FName Key) const;

	/** B-076: the shared-armoury weapon and ring a run-only member uses (an invalid ID: none).
	 *  Applies the next time the member spawns. False for a member who isn't run-only. */
	bool SetRunMemberGear(int32 SlotIndex, const FGuid &Weapon, const FGuid &Ring);

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

	// ==================== SEATS (B-346, Q-21-audit) ====================

	/** Make a member an AI seat (or a player's again) before a fight. Refused (false) for the leader,
	 *  an invalid slot, or while a fight runs: the setting is fixed for the fight. Mirrors the
	 *  OBSOLETE bPlayerControlled. Broadcasts OnPartyChanged on a real change. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	bool SetMemberAISeat(int32 SlotIndex, bool bAISeat);

	/** The AI seat's difficulty (default Medium). Refused while a fight runs or for an invalid slot. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	bool SetMemberAISeatDifficulty(int32 SlotIndex, EAIDifficulty Difficulty);

	/** B-348b: stores a member's head-start allocation (UCharacterDataComponent::SetHeadStartAllocation validates the
	 *  budget and writes through). False for an invalid slot. */
	bool SetMemberHeadStart(int32 SlotIndex, int32 Mind, int32 Body, int32 Spirit);

	/** B-348c: banks a member's Broken Darkness state and the run's strain (at a fight's end). False for an invalid
	 *  slot. */
	bool SetMemberBrokenDarkness(int32 SlotIndex, bool bBrokenDarkness, float RunStrain);

	/** B-392 (D-170): stores a member's chosen guard and the weapon it put away (ULoadoutComponent::SetGuardStyle
	 *  validates and writes through). False for an invalid slot. */
	bool SetMemberGuard(int32 SlotIndex, EGuardStyle Style, const FGuid &GuardWeaponId);

	/** B-393 (D-170): stores which element colours a member's landed-parry effect (ULoadoutComponent::
	 *  SetParryElementPriority writes through). False for an invalid slot. */
	bool SetMemberParryPriority(int32 SlotIndex, EParryElementPriority Priority);

	/** True when the member is an AI seat; false for an invalid slot. */
	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsMemberAISeat(int32 SlotIndex) const;

	/** D-150 (B-360): a joining player's Character takes the AI seat in SlotIndex. That member's gear and pouch go
	 *  to the leader (the host's run). Refused (false, logged): not an AI seat, the leader's slot, or during a fight.
	 *  A character already in the party joins as a separate copy (B-360b). PawnClass empty = the leader's. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	bool ReplaceAISeatWithPlayer(int32 SlotIndex, TSoftClassPtr<ACombatCharacter> PawnClass, UCharacterData *Character);

	/** A fight is running in this world (seats are fixed during one, Q-21-audit). Public since B-360: the run state's join request reads it. */
	bool IsFightRunning() const;

	/** OBSOLETE (B-346): forwards to SetMemberAISeat(SlotIndex, !bPlayerControlled).
	 *  Player or AI for one member in battle. The leader (slot 0) is always Player,
	 *  so a request to hand it to the AI is refused (false). Broadcasts OnPartyChanged
	 *  on a real change. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	bool SetMemberPlayerControlled(int32 SlotIndex, bool bPlayerControlled);

	/** OBSOLETE (B-346): !IsMemberAISeat. The member's battle control setting; Player for an invalid slot. */
	UFUNCTION(BlueprintPure, Category = "Party")
	bool IsMemberPlayerControlled(int32 SlotIndex) const;

	/** OBSOLETE (B-346): nothing reads it; an AI seat stays AI all fight (Q-21-audit).
	 *  D-045 setting: true lets the AI finish a fight once every Player member is down. */
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
	// Saved with the run at each save point during a trial (B-208, USaveSubsystem::CaptureParty).

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

	/** B-115: bank a slot's crystal and absorption state at a fight's end. No-op on an invalid slot. */
	void SetMemberCarriedState(int32 SlotIndex, const FCarriedGearState &Gear, const FCarriedAbsorptionState &Absorption);

	/** B-115: the slot's carried crystal and absorption state, or null on an invalid slot. Each
	 *  part's bCarried says whether anything was banked. */
	const FPartyMember *GetMemberCarriedState(int32 SlotIndex) const;

	/** Trial entry: seed every slot to its computed maxima. Broken-Darkness-innate
	 *  members start with an empty EP pool. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void SetAllMembersFull();

	/** Hub return: drop all carried state (vitals, crystals, absorption) back to the sentinel, so
	 *  display falls back to template maxima again. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	void ClearAllMemberVitals();

	/** D-043 (B-150): a rest spot brings every fallen member back at full HP/EP (Broken Darkness
	 *  members at empty EP, as at trial entry). Standing members are untouched. Returns how many
	 *  came back. Console: wor.Party.ReviveFallen. */
	UFUNCTION(BlueprintCallable, Category = "Party")
	int32 ReviveFallenMembers();

	/** B-187d (Q-187d changed by Crown, 2026-11-08): a rest for the members IsRested picks: each comes back to full
	 *  (the fallen revived, as ReviveFallenMembers), its carried gear and absorption cleared (as SetAllMembersFull).
	 *  Others are untouched. Returns how many of them were fallen. */
	int32 RestMembersWhere(TFunctionRef<bool(const FPartyMember &)> IsRested);

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
	bool InviteMemberInternal(TSoftClassPtr<ACombatCharacter> PawnClass, UCharacterData *DataOverride = nullptr);

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
