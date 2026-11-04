// RunStateSpec.cpp
// B-232 (D-129 two-player slice, Q-21-audit): the run state lives on the GameState (replicated; read from there, else
// from UTrialRunSubsystem offline); a player commands only its own characters; an unclaimed member's turns rotate
// through the side's players; an AI seat is nobody's.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/CombatPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/RunStateComponent.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr int32 RUN_STATE_SEED = 7331;
	constexpr int32 RUN_STATE_FIRST_PLAYER_ID = 1;
	constexpr int32 RUN_STATE_SECOND_PLAYER_ID = 2;

	FRunRosterEntry RunStateMember(UCharacterData *Character, bool bAISeat)
	{
		FRunRosterEntry Entry;
		Entry.Character = Character;
		Entry.bAISeat = bAISeat;
		return Entry;
	}
}

BEGIN_DEFINE_SPEC(FRunStateSpec, "WoR.Net.RunState", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AWoRGameState *GameState = nullptr;
	URunStateComponent *Run = nullptr;
	APlayerController *FirstPC = nullptr;
	APlayerController *SecondPC = nullptr;
	AWoRPlayerState *FirstPlayer = nullptr;
	AWoRPlayerState *SecondPlayer = nullptr;

	APlayerController *SpawnPlayer(int32 PlayerId, AWoRPlayerState *&OutPlayer)
	{
		UWorld *World = TestWorld->GetWorld();
		OutPlayer = World->SpawnActor<AWoRPlayerState>();
		APlayerController *PC = World->SpawnActor<APlayerController>();
		if (!OutPlayer || !PC)
		{
			return nullptr;
		}
		// The controller spawned a plain PlayerState of its own and registered it with the GameState; it is not a seat.
		if (APlayerState *AutoState = PC->PlayerState; AutoState && AutoState != OutPlayer)
		{
			GameState->RemovePlayerState(AutoState);
			AutoState->Destroy();
		}
		OutPlayer->SetPlayerId(PlayerId);
		PC->PlayerState = OutPlayer;
		GameState->AddPlayerState(OutPlayer);
		return PC;
	}

	/** The PC whose player is first in seat order (the rotation's turn 0). */
	APlayerController *SeatOrderPC(int32 Seat) const
	{
		const TArray<int32> Side = Run->GetSidePlayerIds();
		const int32 Id = Side.IsValidIndex(Seat) ? Side[Seat] : INDEX_NONE;
		return Id == FirstPlayer->GetPlayerId() ? FirstPC : Id == SecondPlayer->GetPlayerId() ? SecondPC : nullptr;
	}
END_DEFINE_SPEC(FRunStateSpec)

void FRunStateSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		UWorld *World = TestWorld->GetWorld();
		GameState = World ? World->SpawnActor<AWoRGameState>() : nullptr;
		if (!TestNotNull(TEXT("GameState"), GameState)) return;
		World->SetGameState(GameState);
		Run = GameState->GetRunState();
		FirstPC = SpawnPlayer(RUN_STATE_FIRST_PLAYER_ID, FirstPlayer);
		SecondPC = SpawnPlayer(RUN_STATE_SECOND_PLAYER_ID, SecondPlayer);
	});

	AfterEach([this]()
	{
		GameState = nullptr;
		Run = nullptr;
		FirstPC = SecondPC = nullptr;
		FirstPlayer = SecondPlayer = nullptr;
		TestWorld.Reset();
	});

	It("keeps the run on the GameState, replicated, and reads it from there (else from the subsystem)", [this]()
	{
		if (!TestNotNull(TEXT("Run state"), Run)) return;
		UTrialRunSubsystem *TrialRun = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("Trial run"), TrialRun)) return;
		UTrialData *Trial = NewObject<UTrialData>(GetTransientPackage());
		TrialRun->RestoreRun(Trial, RUN_STATE_SEED, INDEX_NONE, ETrialDifficulty::Medium, {TEXT("Beaten")}, {}, {});
		Run->PullFromSubsystems();
		TestEqual(TEXT("seed read from the GameState"), URunStateComponent::GetRunSeed(TestWorld->GetWorld()), RUN_STATE_SEED);
		TestTrue(TEXT("trial read from the GameState"), URunStateComponent::GetActiveTrial(TestWorld->GetWorld()) == Trial);
		TestTrue(TEXT("beaten encounter read from the GameState"), URunStateComponent::IsEncounterBeaten(TestWorld->GetWorld(), TEXT("Beaten")));
		TestTrue(TEXT("the component replicates"), Run->GetIsReplicated());
		for (const TCHAR *Name : {TEXT("RunSeed"), TEXT("ActiveTrial"), TEXT("BeatenEncounters"), TEXT("Roster"), TEXT("RotationIndex")})
		{
			const FProperty *Property = FindFProperty<FProperty>(URunStateComponent::StaticClass(), Name);
			TestTrue(FString::Printf(TEXT("%s replicates"), Name), Property && Property->HasAnyPropertyFlags(CPF_Net));
		}

		FWoRTestWorld Offline;
		if (!TestTrue(TEXT("Offline world"), Offline.Create())) return;
		Offline.GetSubsystem<UTrialRunSubsystem>()->RestoreRun(Trial, RUN_STATE_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
		TestEqual(TEXT("no GameState: the subsystem answers"), URunStateComponent::GetRunSeed(Offline.GetWorld()), RUN_STATE_SEED);
	});

	It("lets a player command only its own character", [this]()
	{
		if (!Run || !FirstPC || !SecondPC) return;
		UCharacterData *FirstData = FWoRTestWorld::NewCharacterData();
		UCharacterData *SecondData = FWoRTestWorld::NewCharacterData();
		AActor *FirstHero = TestWorld->SpawnCombatant(FirstData);
		AActor *SecondHero = TestWorld->SpawnCombatant(SecondData);
		Run->SetRoster({RunStateMember(FirstData, false), RunStateMember(SecondData, false)});
		FirstPlayer->ClaimCharacter(FirstData);
		SecondPlayer->ClaimCharacter(SecondData);
		TestTrue(TEXT("first commands its own"), ACombatPlayerController::ControlsActor(FirstPC, FirstHero));
		TestFalse(TEXT("first doesn't command the second's"), ACombatPlayerController::ControlsActor(FirstPC, SecondHero));
		TestTrue(TEXT("second commands its own"), ACombatPlayerController::ControlsActor(SecondPC, SecondHero));
		TestFalse(TEXT("second doesn't command the first's"), ACombatPlayerController::ControlsActor(SecondPC, FirstHero));
	});

	It("rotates an unclaimed member's turns through the side's players; an AI seat is nobody's", [this]()
	{
		if (!Run || !FirstPC || !SecondPC) return;
		UCharacterData *ClaimedData = FWoRTestWorld::NewCharacterData();
		UCharacterData *RecruitData = FWoRTestWorld::NewCharacterData();
		UCharacterData *AIData = FWoRTestWorld::NewCharacterData();
		AActor *Claimed = TestWorld->SpawnCombatant(ClaimedData);
		AActor *Recruit = TestWorld->SpawnCombatant(RecruitData);
		AActor *AISeat = TestWorld->SpawnCombatant(AIData);
		Run->SetRoster({RunStateMember(ClaimedData, false), RunStateMember(RecruitData, false), RunStateMember(AIData, true)});
		FirstPlayer->ClaimCharacter(ClaimedData);

		TestEqual(TEXT("two seats on the side"), Run->GetSidePlayerIds().Num(), 2);
		APlayerController *SeatZero = SeatOrderPC(0);
		APlayerController *SeatOne = SeatOrderPC(1);
		if (!TestNotNull(TEXT("seat 0"), SeatZero) || !TestNotNull(TEXT("seat 1"), SeatOne)) return;
		TestTrue(TEXT("turn 1: the first seat commands the unclaimed member"), ACombatPlayerController::ControlsActor(SeatZero, Recruit));
		TestFalse(TEXT("turn 1: not the second seat"), ACombatPlayerController::ControlsActor(SeatOne, Recruit));

		Run->NoteTurnTaken(Claimed);
		TestEqual(TEXT("a claimed member's turn doesn't move the rotation"), Run->GetRotationIndex(), 0);
		Run->NoteTurnTaken(Recruit);
		TestTrue(TEXT("turn 2: the second seat commands it"), ACombatPlayerController::ControlsActor(SeatOne, Recruit));
		TestFalse(TEXT("turn 2: not the first seat"), ACombatPlayerController::ControlsActor(SeatZero, Recruit));

		TestFalse(TEXT("an AI seat: not the first player"), ACombatPlayerController::ControlsActor(FirstPC, AISeat));
		TestFalse(TEXT("an AI seat: not the second player"), ACombatPlayerController::ControlsActor(SecondPC, AISeat));

		Run->ResetRotation();
		TestEqual(TEXT("a new fight starts the rotation over (review)"), Run->GetRotationIndex(), 0);
	});

	It("refuses a second claim on a character and releases one (review)", [this]()
	{
		if (!Run || !FirstPlayer || !SecondPlayer) return;
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		FirstPlayer->ClaimCharacter(Data);
		SecondPlayer->ClaimCharacter(Data);
		TestTrue(TEXT("the first keeps it"), FirstPlayer->ClaimsCharacter(Data));
		TestFalse(TEXT("the second is refused"), SecondPlayer->ClaimsCharacter(Data));
		FirstPlayer->ReleaseCharacter(Data);
		TestFalse(TEXT("released"), FirstPlayer->ClaimsCharacter(Data));
		SecondPlayer->ClaimCharacter(Data);
		TestTrue(TEXT("free to claim now"), SecondPlayer->ClaimsCharacter(Data));
		TestTrue(TEXT("seats are in ascending player id"), Run->GetSidePlayerIds() == TArray<int32>({RUN_STATE_FIRST_PLAYER_ID, RUN_STATE_SECOND_PLAYER_ID}));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
