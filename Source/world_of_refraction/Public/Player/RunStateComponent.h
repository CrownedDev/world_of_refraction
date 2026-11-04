// RunStateComponent.h
// B-232 (D-129 two-player slice, Design/CoopStateOwnership.md): the run state every player shares and the side's
// roster, on the GameState. The server copies them from UTrialRunSubsystem and UPartySessionSubsystem (they keep the
// run across level travel; a GameState lives for one level) when the level starts; clients get them by replication.
// Reads go through the static Get* helpers: the GameState's copy when there is one, else the subsystem (standalone,
// headless specs), so every caller has one path.
// Seats (Q-21-audit, SeatRules): a member a player claimed (AWoRPlayerState::ClaimCharacter) is that player's; an
// unclaimed member's turns rotate through the side's players; an AI seat is AI. The rotation counter lives here.
// Debug: GetDebugString, `wor.Run.PrintState`.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Trial/TrialGate.h"
#include "RunStateComponent.generated.h"

class AController;
class AGameStateBase;
class UCharacterData;
class UParty;
class UTrialData;

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

	/** Server: the roster follows the party (a recruit, an AI-seat change); the rotation is kept. Bound to
	 *  UPartySessionSubsystem::OnPartyChanged on authority. */
	void RefreshRoster();

	/** Server: the seat rotation starts over (each fight, from the orchestrator's StartCombat). */
	void ResetRotation();

	// ---- Reads: the GameState's copy, else the subsystem ----

	static UTrialData *GetActiveTrial(const UObject *WorldContext);
	static int32 GetRunSeed(const UObject *WorldContext);
	static bool IsEncounterBeaten(const UObject *WorldContext, FName EncounterId);

	// ---- Seats (Q-21-audit) ----

	/** Server / specs: replace the roster. */
	void SetRoster(const TArray<FRunRosterEntry> &InRoster);
	const TArray<FRunRosterEntry> &GetRoster() const { return Roster; }

	/** The roster entry of Member's character, or null (not on the side's roster: an enemy, a test actor). */
	const FRunRosterEntry *FindRosterEntry(const AActor *Member) const;

	/** The side's player ids, in seat order: ascending player id (the same on every machine; PlayerArray order is not). */
	TArray<int32> GetSidePlayerIds() const;

	/** The player id that commands Member's turn now, or SeatRules::AI_COMMANDER (an AI seat, a side with no
	 *  players, or Member not on the roster). */
	int32 GetCommander(const AActor *Member) const;

	/** Does Sender (its PlayerState's id) command Member now? False off the roster. */
	bool IsCommandedBy(const AActor *Member, const AController *Sender) const;

	/** Server: Member's turn ended; a turn taken by the rotation moves it to the side's next player. */
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

	/** The player id that claims Character, or SeatRules::UNCLAIMED (none, or a player no longer here). */
	int32 GetClaimingPlayerId(const UCharacterData *Character) const;

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

	/** How many rotated turns the side has taken this fight (SeatRules::GetCommander); ResetRotation at each
	 *  StartCombat, and at the pull. */
	UPROPERTY(Replicated)
	int32 RotationIndex = 0;
};
