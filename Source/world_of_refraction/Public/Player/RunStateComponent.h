// RunStateComponent.h
// B-232 (D-129 two-player slice, Design/CoopStateOwnership.md): the run state every player shares and the side's
// roster, on the GameState. The server copies them from UTrialRunSubsystem and UPartySessionSubsystem (they keep the
// run across level travel; a GameState lives for one level) when the level starts; clients get them by replication.
// Reads go through the static Get* helpers: the GameState's copy when there is one, else the subsystem (standalone,
// headless specs), so every caller has one path.
// Seats (Q-21-audit, D-150): a member a player claimed (AWoRPlayerState::ClaimCharacter) is that player's; an AI seat
// is AI; in multiplayer every slot is one or the other (no unclaimed members, D-150: an unclaimed one falls to the host).
// The B-232 rotation (RotationIndex, NoteTurnTaken, ResetRotation) is OBSOLETE (B-360): kept until Crown's PIE.
// Joining mid-run (D-150): a player asks for a seat (RequestJoin); the host picks the AI seat it replaces (AcceptJoin:
// `wor.Party.AcceptJoin`, or the placeholder `wor.UI.JoinSeat`); that member's gear and pouch go to the host.
// B-232d: the run's found features, memories and unspent levels join it (PullRunProgress on each change).
// B-233b (Q-233b): in a run a new friend is seated only while the party is at a rest spot (UTrialRunSubsystem::
// IsAtRestSpot: rested there, not yet walked away). A friend who leaves (its connection logs out) drops out: its members become AI seats held for it (in a
// fight the host commands them until it ends, then they turn AI); seated again on a held seat, it takes its own member
// back with its gear, anywhere outside a fight.
// Debug: GetDebugString, `wor.Run.PrintState`, `wor.Party.PrintJoinRequests`.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Trial/TrialGate.h"
#include "RunStateComponent.generated.h"

class ACombatCharacter;
class AController;
class AGameStateBase;
class APlayerState;
class UCharacterData;
class UParty;
class UTrialData;
class UMemoryData;

namespace RunJoinRules
{
	/** B-233b (Rules 06 drop-in ruling, Q-233b): may the host seat a joiner now? Outside a run always; in a run only at a
	 *  rest spot, unless the joiner takes back the seat it left (held for it). */
	WORLD_OF_REFRACTION_API bool CanSeatJoiner(bool bInRun, bool bHostAtRestSpot, bool bOwnHeldSeat);
}

/** D-150: a player waiting to join the run with its own character; the host picks the AI seat it takes. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FRunJoinRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Run State")
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Run State")
	TObjectPtr<UCharacterData> Character = nullptr;

	/** Empty: the leader's pawn class. */
	UPROPERTY(BlueprintReadOnly, Category = "Run State")
	TSoftClassPtr<ACombatCharacter> PawnClass;
};

/** B-232d: one character's pillar levels waiting to be placed this run, as replicated. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FRunUnspentLevels
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Run State")
	FName CharacterKey;

	UPROPERTY(BlueprintReadOnly, Category = "Run State")
	int32 Count = 0;
};

/** One member of the side's roster. Who claims it is read from the players' PlayerStates. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FRunRosterEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Run State")
	TObjectPtr<UCharacterData> Character = nullptr;

	/** Set before the fight by the party leader, fixed during it (Q-21-audit): B-346's AI seat (FPartyMember::bAISeat). */
	UPROPERTY(BlueprintReadOnly, Category = "Run State")
	bool bAISeat = false;

	/** B-360b: the party member (FPartyMember::MemberId), so two copies of one character are two entries with their
	 *  own commanders. Invalid on a roster set from characters alone (SetRoster): the character stands for it. */
	UPROPERTY(BlueprintReadOnly, Category = "Run State")
	FGuid MemberId;
};

UCLASS(ClassGroup = (Trial), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API URunStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URunStateComponent();

	/** The GameState's run state in WorldContext's world, or null (no GameState, or not an AWoRGameState). */
	static URunStateComponent *Find(const UObject *WorldContext);

	/** Server: copy the run (trial, seed, variation, difficulty, beaten encounters) and the local party's roster.
	 *  Called from BeginPlay on authority; the rotation starts over. */
	void PullFromSubsystems();

	/** B-232d (server): copies the run's progress that changes mid-level (found features, memories, unspent
	 *  levels); UTrialRunSubsystem calls it on each change. */
	void PullRunProgress();

	/** Server: the roster follows the party (a recruit, an AI-seat change); the rotation is kept. Bound to
	 *  UPartySessionSubsystem::OnPartyChanged on authority. */
	void RefreshRoster();

	/** OBSOLETE (B-360, D-150: no rotation; delete after PIE). Server: the seat rotation started over each fight. */
	void ResetRotation();

	// ---- Reads: the GameState's copy, else the subsystem ----

	static UTrialData *GetActiveTrial(const UObject *WorldContext);
	static int32 GetRunSeed(const UObject *WorldContext);
	static bool IsEncounterBeaten(const UObject *WorldContext, FName EncounterId);
	/** B-232d: a map feature found this run (a chest opened, a memory spot read). */
	static bool HasFoundFeature(const UObject *WorldContext, FName FeatureId);
	/** B-232d: the memories collected this run. */
	static TArray<UMemoryData *> GetRunMemories(const UObject *WorldContext);
	/** B-232d: a character's pillar levels waiting to be placed. */
	static int32 GetUnspentLevels(const UObject *WorldContext, FName CharacterKey);

	/** B-232d: the copy itself (specs, debug); the readers above use it only on a client. */
	const TArray<FName> &GetFoundFeatureIds() const { return FoundFeatureIds; }

	// ---- Seats (Q-21-audit) ----

	/** Server / specs: replace the roster. */
	void SetRoster(const TArray<FRunRosterEntry> &InRoster);
	const TArray<FRunRosterEntry> &GetRoster() const { return Roster; }

	/** The roster entry of Member's character, or null (not on the side's roster: an enemy, a test actor). */
	const FRunRosterEntry *FindRosterEntry(const AActor *Member) const;

	/** The side's player ids, in seat order: ascending player id (the same on every machine; PlayerArray order is not). */
	TArray<int32> GetSidePlayerIds() const;

	/** The player id that commands Member's turn now, or SeatRules::AI_COMMANDER (an AI seat, a side with no
	 *  players, or Member not on the roster). D-150: a claimed member is its player's; an unclaimed one is the host's
	 *  (solo: the one player) — nothing rotates. */
	int32 GetCommander(const AActor *Member) const;

	// ---- Joining mid-run (D-150) ----
	/** Server: Joiner asks for a seat with Character. Refused (false, logged) with no free AI seat (the party is full),
	 *  a character already in the party (B-360b lifts this), or during a fight. One request per player. */
	bool RequestJoin(APlayerState *Joiner, UCharacterData *Character, TSoftClassPtr<ACombatCharacter> PawnClass);
	/** Server, the host's choice: the request of PlayerId takes AI seat ReplaceSlot (its member's gear and pouch go to
	 *  the host), the joiner claims its character, the roster follows. False (logged) when refused. */
	bool AcceptJoin(int32 PlayerId, int32 ReplaceSlot);
	/** Server: drop PlayerId's request. */
	bool RefuseJoin(int32 PlayerId);
	const TArray<FRunJoinRequest> &GetJoinRequests() const { return JoinRequests; }
	/** B-459c (server): a join request came, was answered, or left with its player (also on a refused answer; the
	 *  listeners re-read GetJoinRequests). The host's join request card listens. */
	FSimpleMulticastDelegate OnJoinRequestsChanged;
	/** The party slots an AI seat holds (never the leader's). */
	TArray<int32> GetFreeAISeats() const;

	// ---- B-233b: drop-in at rest spots, a leaving friend ----
	/** Server: may the host seat a new joiner now (RunJoinRules::CanSeatJoiner on this machine's run)? */
	bool CanSeatNewJoinerNow() const;
	/** Server: Leaver left the session: its join request goes, its members become AI seats held for it (in a fight:
	 *  when the fight ends). Called on a remote player's logout. */
	void HandlePlayerLeft(APlayerState *Leaver);
	/** The leaving player's id (unique net id string) a member's AI seat is held for, or empty. */
	FString GetHeldFor(const FGuid &MemberId) const;

	/** Does Sender (its PlayerState's id) command Member now? False off the roster. */
	bool IsCommandedBy(const AActor *Member, const AController *Sender) const;

	/** OBSOLETE (B-360, D-150: no rotation; delete after PIE). Server: Member's turn ended; a turn the rotation
	 *  took moved it to the side's next player. Does nothing now. */
	void NoteTurnTaken(const AActor *Member);

	int32 GetRotationIndex() const { return RotationIndex; }

	FString GetDebugString() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

private:
	UFUNCTION()
	void HandlePartyChanged(UParty *Party);

	/** B-233b: a remote player's logout (FGameModeEvents::GameModeLogoutEvent). */
	void HandleLogout(class AGameModeBase *GameMode, AController *Exiting);
	/** B-233b: a fight with seats waiting to turn AI ended. */
	UFUNCTION()
	void HandleCombatEndedForDrops(int32 FinalTurnCount);
	void ApplyPendingSeatDrops();
	/** Makes Member an AI seat now; false when a fight runs (true when it already left the party). */
	bool DropToAISeat(const FGuid &MemberId);
	FDelegateHandle LogoutHandle;

	/** The player id that claims the entry (B-360b: its member; by character when the roster was set from characters),
	 *  or SeatRules::UNCLAIMED (none, or a player no longer here). */
	int32 GetClaimingPlayerId(const FRunRosterEntry &Entry) const;

	const AGameStateBase *GetOwningGameState() const;

	UPROPERTY(Replicated)
	bool bHasRun = false;

	UPROPERTY(Replicated)
	TObjectPtr<UTrialData> ActiveTrial = nullptr;

	UPROPERTY(Replicated)
	int32 RunSeed = 0;

	UPROPERTY(Replicated)
	int32 VariationIndex = INDEX_NONE;

	UPROPERTY(Replicated)
	ETrialDifficulty Difficulty = TrialGate::DEFAULT_DIFFICULTY;

	UPROPERTY(Replicated)
	TArray<FName> BeatenEncounters;

	UPROPERTY(Replicated)
	TArray<FRunRosterEntry> Roster;

	// B-232d: the run's progress that changes mid-level (PullRunProgress). On a client its arrival refreshes the map
	// features (OnRep_RunProgress), which may have checked before it came.
	UPROPERTY(ReplicatedUsing = OnRep_RunProgress)
	TArray<FName> FoundFeatureIds;

	UPROPERTY(ReplicatedUsing = OnRep_RunProgress)
	TArray<TObjectPtr<UMemoryData>> RunMemories;

	UPROPERTY(ReplicatedUsing = OnRep_RunProgress)
	TArray<FRunUnspentLevels> UnspentLevels;

	UFUNCTION()
	void OnRep_RunProgress();

	/** Server only: who is waiting to join (D-150). */
	UPROPERTY()
	TArray<FRunJoinRequest> JoinRequests;

	/** B-233b, server only: member id -> the player (unique net id string) its AI seat is held for. Per level (a
	 *  GameState lives for one): a level change forgets the holds, the seats stay AI. */
	TMap<FGuid, FString> HeldSeats;
	/** B-233b, server only: members of a player who left mid-fight; they turn AI when the fight ends. */
	TArray<FGuid> PendingSeatDrops;

	/** OBSOLETE (B-360, D-150): the B-232 rotation counter, no longer read. */
	UPROPERTY(Replicated)
	int32 RotationIndex = 0;
};
