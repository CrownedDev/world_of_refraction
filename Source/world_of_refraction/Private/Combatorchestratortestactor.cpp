// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatOrchestratorTestActor.h"
#include "TurnManager.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "Kismet/GameplayStatics.h"

ACombatOrchestratorTestActor::ACombatOrchestratorTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	TestOrchestrator = nullptr;
	TestsPassed = 0;
	TestsFailed = 0;
	TestActorCounter = 0;
	bWaitingForResult = false;
}

void ACombatOrchestratorTestActor::BeginPlay()
{
	Super::BeginPlay();
}

// ========================================
// TEST EXECUTION
// ========================================

void ACombatOrchestratorTestActor::RunAllTests()
{
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("COMBAT ORCHESTRATOR TEST SUITE"));
	UE_LOG(LogTemp, Display, TEXT("========================================"));

	TestsPassed = 0;
	TestsFailed = 0;

	Test_BasicCombatFlow();
	Test_StateTransitions();
	Test_VictoryCondition();
	Test_DefeatCondition();
	Test_ForceEndCombat();

	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("RESULTS: %d passed, %d failed"), TestsPassed, TestsFailed);
	UE_LOG(LogTemp, Display, TEXT("========================================"));
}

void ACombatOrchestratorTestActor::Test_BasicCombatFlow()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Basic Combat Flow"));

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Basic Combat Flow", false);
		return;
	}

	// Configure for fast testing
	Orchestrator->bAutoAdvanceTurns = true;
	Orchestrator->AutoAdvanceDelay = 0.1f;

	// Create teams
	TArray<AActor*> Team0;
	Team0.Add(CreateTestCharacter("Player1", 5, 5, 5, 0, 0));
	Team0.Add(CreateTestCharacter("Player2", 5, 5, 5, 0, 0));

	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("Enemy1", 5, 5, 5, 0, 1));
	Team1.Add(CreateTestCharacter("Enemy2", 5, 5, 5, 0, 1));

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Verify state
	bool bStarted = (Orchestrator->GetCombatState() == ECombatState::InProgress);
	bool bHasCurrentActor = (Orchestrator->GetCurrentActor() != nullptr);
	bool bTeamsStored = (Orchestrator->GetTeam0().Num() == 2 && Orchestrator->GetTeam1().Num() == 2);

	bool bPassed = bStarted && bHasCurrentActor && bTeamsStored;

	if (bPassed)
	{
		UE_LOG(LogTemp, Display, TEXT("    Combat started successfully"));
		UE_LOG(LogTemp, Display, TEXT("    Current actor: %s"), *Orchestrator->GetCurrentActor()->GetName());
		UE_LOG(LogTemp, Display, TEXT("    Teams: %d vs %d"),
			Orchestrator->GetTeam0().Num(), Orchestrator->GetTeam1().Num());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("    Failed: State=%d, HasActor=%d, TeamsStored=%d"),
			(int32)Orchestrator->GetCombatState(), bHasCurrentActor, bTeamsStored);
	}

	PrintTestResult("Basic Combat Flow", bPassed);

	// Force cleanup
	Orchestrator->ForceEndCombat();
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_StateTransitions()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] State Transitions"));

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("State Transitions", false);
		return;
	}

	// Track state transitions
	RecordedStateTransitions.Empty();
	Orchestrator->OnCombatStateChanged.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatStateChanged);

	// Disable auto-advance for manual control
	Orchestrator->bAutoAdvanceTurns = false;

	// Create teams
	TArray<AActor*> Team0;
	Team0.Add(CreateTestCharacter("P1", 5, 5, 5, 0, 0));

	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("E1", 5, 5, 5, 0, 1));

	// Should be Idle initially
	bool bIdleFirst = (Orchestrator->GetCombatState() == ECombatState::Idle);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Should transition through Initializing -> InProgress
	bool bInProgress = (Orchestrator->GetCombatState() == ECombatState::InProgress);

	// Force end
	Orchestrator->ForceEndCombat(ECombatState::Victory);

	// Should be back to Idle
	bool bIdleAfter = (Orchestrator->GetCombatState() == ECombatState::Idle);

	// Check we got expected transitions
	// Expected: Initializing, InProgress, Victory, Idle
	bool bTransitionsValid = RecordedStateTransitions.Num() >= 3;

	UE_LOG(LogTemp, Display, TEXT("    Recorded %d state transitions"), RecordedStateTransitions.Num());
	for (int32 i = 0; i < RecordedStateTransitions.Num(); i++)
	{
		UE_LOG(LogTemp, Display, TEXT("    [%d] -> %d"), i, (int32)RecordedStateTransitions[i]);
	}

	bool bPassed = bIdleFirst && bInProgress && bIdleAfter && bTransitionsValid;
	PrintTestResult("State Transitions", bPassed);

	// Cleanup
	Orchestrator->OnCombatStateChanged.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatStateChanged);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_VictoryCondition()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Victory Condition"));

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Victory Condition", false);
		return;
	}

	Orchestrator->bAutoAdvanceTurns = false;

	// Create teams
	TArray<AActor*> Team0;
	Team0.Add(CreateTestCharacter("Player", 5, 5, 5, 0, 0));

	TArray<AActor*> Team1;
	AActor* Enemy = CreateTestCharacter("Enemy", 5, 5, 5, 0, 1);
	Team1.Add(Enemy);

	// Track result
	LastCombatResult = FCombatResult();
	Orchestrator->OnCombatResultReady.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatResultReady);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Kill all enemies using debug tool
	Orchestrator->DebugKillActor(Enemy);

	// Complete current action to trigger win check
	Orchestrator->OnActionCompleted();

	// Check result
	bool bVictory = (LastCombatResult.FinalState == ECombatState::Victory);
	bool bCorrectSurvivors = (LastCombatResult.Team0Survivors == 1 && LastCombatResult.Team1Survivors == 0);

	UE_LOG(LogTemp, Display, TEXT("    Result: State=%d, Team0=%d alive, Team1=%d alive"),
		(int32)LastCombatResult.FinalState,
		LastCombatResult.Team0Survivors,
		LastCombatResult.Team1Survivors);

	bool bPassed = bVictory && bCorrectSurvivors;
	PrintTestResult("Victory Condition", bPassed);

	// Cleanup
	Orchestrator->OnCombatResultReady.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatResultReady);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_DefeatCondition()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Defeat Condition"));

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Defeat Condition", false);
		return;
	}

	Orchestrator->bAutoAdvanceTurns = false;

	// Create teams
	TArray<AActor*> Team0;
	AActor* Player = CreateTestCharacter("Player", 5, 5, 5, 0, 0);
	Team0.Add(Player);

	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("Enemy", 5, 5, 5, 0, 1));

	// Track result
	LastCombatResult = FCombatResult();
	Orchestrator->OnCombatResultReady.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatResultReady);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Kill all players
	Orchestrator->DebugKillActor(Player);

	// Complete action to trigger win check
	Orchestrator->OnActionCompleted();

	// Check result
	bool bDefeat = (LastCombatResult.FinalState == ECombatState::Defeat);
	bool bCorrectSurvivors = (LastCombatResult.Team0Survivors == 0 && LastCombatResult.Team1Survivors == 1);

	UE_LOG(LogTemp, Display, TEXT("    Result: State=%d, Team0=%d alive, Team1=%d alive"),
		(int32)LastCombatResult.FinalState,
		LastCombatResult.Team0Survivors,
		LastCombatResult.Team1Survivors);

	bool bPassed = bDefeat && bCorrectSurvivors;
	PrintTestResult("Defeat Condition", bPassed);

	// Cleanup
	Orchestrator->OnCombatResultReady.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatResultReady);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_ForceEndCombat()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Force End Combat"));

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Force End Combat", false);
		return;
	}

	Orchestrator->bAutoAdvanceTurns = false;

	// Create teams
	TArray<AActor*> Team0;
	Team0.Add(CreateTestCharacter("P1", 5, 5, 5, 0, 0));

	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("E1", 5, 5, 5, 0, 1));

	// Track result
	LastCombatResult = FCombatResult();
	Orchestrator->OnCombatResultReady.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatResultReady);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);
	bool bWasInProgress = (Orchestrator->GetCombatState() == ECombatState::InProgress);

	// Force end (like fleeing or cutscene)
	Orchestrator->ForceEndCombat(ECombatState::Idle);

	bool bIsIdle = (Orchestrator->GetCombatState() == ECombatState::Idle);
	bool bResultBroadcast = (LastCombatResult.FinalState != ECombatState::Idle ||
		LastCombatResult.TotalTurns > 0);

	UE_LOG(LogTemp, Display, TEXT("    Was in progress: %s"), bWasInProgress ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogTemp, Display, TEXT("    Is now idle: %s"), bIsIdle ? TEXT("Yes") : TEXT("No"));

	bool bPassed = bWasInProgress && bIsIdle;
	PrintTestResult("Force End Combat", bPassed);

	// Cleanup
	Orchestrator->OnCombatResultReady.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestCombatResultReady);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

// ========================================
// TEST HELPERS
// ========================================

ACombatOrchestrator* ACombatOrchestratorTestActor::GetOrCreateOrchestrator()
{
	// Check if we have GameInstance (need to be in PIE)
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatTest] Not in PIE! Press ALT+P to start Play-In-Editor, then run tests."));
		return nullptr;
	}

	// Reuse existing or spawn new
	if (!TestOrchestrator || !IsValid(TestOrchestrator))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		TestOrchestrator = GetWorld()->SpawnActor<ACombatOrchestrator>(ACombatOrchestrator::StaticClass(), SpawnParams);

		if (TestOrchestrator)
		{
			UE_LOG(LogTemp, Log, TEXT("[CombatTest] Spawned test CombatOrchestrator"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CombatTest] Failed to spawn CombatOrchestrator!"));
		}
	}

	return TestOrchestrator;
}

AActor* ACombatOrchestratorTestActor::CreateTestCharacter(const FString& Name, int32 Mind, int32 Body, int32 Spirit, int32 TurnSpeed, int32 TeamIndex)
{
	// Generate unique name to prevent collision with actors from previous tests
	FString UniqueName = FString::Printf(TEXT("%s_%d"), *Name, TestActorCounter++);

	// Spawn empty actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = FName(*UniqueName);
	AActor* TestActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (!TestActor)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatTest] Failed to spawn test actor: %s"), *UniqueName);
		return nullptr;
	}

	// Create CharacterData asset - use display name for logs
	UCharacterData* CharData = NewObject<UCharacterData>(TestActor);
	CharData->CharacterName = Name;  // Display name stays readable
	CharData->WorldMindLevel = Mind;
	CharData->WorldBodyLevel = Body;
	CharData->WorldSpiritLevel = Spirit;
	CharData->WorldTurnSpeedPoints = TurnSpeed;
	CharData->WorldAttackSpeedPoints = 0;

	// Create and setup component (CRITICAL: assign data BEFORE register)
	UCharacterDataComponent* CharComp = NewObject<UCharacterDataComponent>(TestActor);
	CharComp->CharacterData = CharData;
	CharComp->RegisterComponent();
	TestActor->AddOwnedComponent(CharComp);

	return TestActor;
}

void ACombatOrchestratorTestActor::CleanupTestActors(TArray<AActor*>& Actors)
{
	for (AActor* Actor : Actors)
	{
		if (Actor && IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	Actors.Empty();
}

void ACombatOrchestratorTestActor::PrintTestResult(const FString& TestName, bool bPassed)
{
	if (bPassed)
	{
		UE_LOG(LogTemp, Display, TEXT("  [PASS] %s"), *TestName);
		TestsPassed++;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  [FAIL] %s"), *TestName);
		TestsFailed++;
	}
}

// ========================================
// EVENT HANDLERS
// ========================================

void ACombatOrchestratorTestActor::OnTestCombatStateChanged(ECombatState NewState)
{
	RecordedStateTransitions.Add(NewState);
}

void ACombatOrchestratorTestActor::OnTestCombatResultReady(const FCombatResult& Result)
{
	LastCombatResult = Result;
	bWaitingForResult = false;
}