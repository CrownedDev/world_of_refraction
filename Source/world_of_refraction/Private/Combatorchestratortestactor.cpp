// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatOrchestratorTestActor.h"
#include "TurnManager.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "ActionStructs.h"
#include "BaseAttackData.h"
#include "Kismet/GameplayStatics.h"

ACombatOrchestratorTestActor::ACombatOrchestratorTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	TestOrchestrator = nullptr;
	TestsPassed = 0;
	TestsFailed = 0;
	TestActorCounter = 0;
	bWaitingForResult = false;
	bActionExecuted = false;
}

void ACombatOrchestratorTestActor::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoRunTests)
	{
		RunAllTests();
	}
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
	Test_ActionExecutorIntegration();
	Test_RealAttackExecution();

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

void ACombatOrchestratorTestActor::Test_ActionExecutorIntegration()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] ActionExecutor Integration"));

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("ActionExecutor Integration", false);
		return;
	}

	// Disable auto-advance - we'll submit actions manually
	Orchestrator->bAutoAdvanceTurns = false;

	// Track action execution
	bActionExecuted = false;
	LastActionResult = FActionResult();
	Orchestrator->OnActionExecuted.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);

	// Create teams
	TArray<AActor*> Team0;
	AActor* Player = CreateTestCharacter("ActionTestPlayer", 5, 5, 5, 0, 0);
	Team0.Add(Player);

	TArray<AActor*> Team1;
	AActor* Enemy = CreateTestCharacter("ActionTestEnemy", 5, 5, 5, 0, 1);
	Team1.Add(Enemy);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Verify we're in progress and have a current actor
	bool bInProgress = (Orchestrator->GetCombatState() == ECombatState::InProgress);
	bool bHasActor = (Orchestrator->GetCurrentActor() != nullptr);

	if (!bInProgress || !bHasActor)
	{
		UE_LOG(LogTemp, Error, TEXT("    Failed to start combat for action test"));
		PrintTestResult("ActionExecutor Integration", false);
		Orchestrator->ForceEndCombat();
		Orchestrator->OnActionExecuted.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);
		CleanupTestActors(Team0);
		CleanupTestActors(Team1);
		return;
	}

	// Test 1: Validate action (should pass for Defend)
	FAction DefendAction;
	DefendAction.ActionType = EActionType::Defend;

	FActionValidationResult Validation = Orchestrator->ValidateAction(DefendAction);
	bool bValidationPassed = Validation.bIsValid;

	UE_LOG(LogTemp, Display, TEXT("    Defend action validation: %s"),
		bValidationPassed ? TEXT("VALID") : *Validation.ErrorMessage);

	// Test 2: Submit action
	bool bSubmitSuccess = Orchestrator->SubmitAction(DefendAction);

	UE_LOG(LogTemp, Display, TEXT("    Submit action result: %s"),
		bSubmitSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));

	// Test 3: Check that OnActionExecuted was broadcast
	UE_LOG(LogTemp, Display, TEXT("    OnActionExecuted fired: %s"),
		bActionExecuted ? TEXT("YES") : TEXT("NO"));

	// Test 4: Check action result
	bool bResultValid = LastActionResult.bSuccess && LastActionResult.ActionType == EActionType::Defend;
	UE_LOG(LogTemp, Display, TEXT("    Action result valid: %s (Type=%d, Success=%d)"),
		bResultValid ? TEXT("YES") : TEXT("NO"),
		(int32)LastActionResult.ActionType,
		LastActionResult.bSuccess);

	// Test 5: Check that turn advanced (or combat ended if someone died)
	// After SubmitAction, OnActionCompleted is called which advances the turn
	bool bTurnAdvanced = (Orchestrator->GetCurrentTurnNumber() > 1) ||
		(Orchestrator->GetCombatState() != ECombatState::InProgress);

	UE_LOG(LogTemp, Display, TEXT("    Turn advanced: %s (Turn=%d, State=%d)"),
		bTurnAdvanced ? TEXT("YES") : TEXT("NO"),
		Orchestrator->GetCurrentTurnNumber(),
		(int32)Orchestrator->GetCombatState());

	bool bPassed = bValidationPassed && bSubmitSuccess && bActionExecuted && bResultValid;
	PrintTestResult("ActionExecutor Integration", bPassed);

	// Cleanup
	Orchestrator->ForceEndCombat();
	Orchestrator->OnActionExecuted.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);
	CleanupTestActors(Team0);
	CleanupTestActors(Team1);
}

void ACombatOrchestratorTestActor::Test_RealAttackExecution()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Real Attack Execution (DA_Attack_Bolt)"));

	// Load the attack data asset
	UBaseAttackData* BoltAttack = LoadObject<UBaseAttackData>(nullptr,
		TEXT("/Game/Data/Attacks/DA_Attack_Bolt.DA_Attack_Bolt"));

	if (!BoltAttack)
	{
		UE_LOG(LogTemp, Error, TEXT("    Failed to load DA_Attack_Bolt - check asset path"));
		PrintTestResult("Real Attack Execution", false);
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("    Loaded: %s (Hits: %d)"), *BoltAttack->AttackName, BoltAttack->HitCount);

	ACombatOrchestrator* Orchestrator = GetOrCreateOrchestrator();
	if (!Orchestrator)
	{
		PrintTestResult("Real Attack Execution", false);
		return;
	}

	// Disable auto-advance
	Orchestrator->bAutoAdvanceTurns = false;

	// Track action execution
	bActionExecuted = false;
	LastActionResult = FActionResult();
	Orchestrator->OnActionExecuted.AddDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);

	// Create attacker with good Body stat for damage
	TArray<AActor*> Team0;
	AActor* Attacker = CreateTestCharacter("BoltAttacker", 3, 6, 3, 0, 0);
	Team0.Add(Attacker);

	// Create target with moderate defense
	TArray<AActor*> Team1;
	AActor* Target = CreateTestCharacter("BoltTarget", 3, 4, 3, 0, 1);
	Team1.Add(Target);

	// Get target's initial HP
	UCharacterDataComponent* TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
	int32 InitialHP = TargetComp ? TargetComp->CurrentHP : 0;

	UE_LOG(LogTemp, Display, TEXT("    Target initial HP: %d"), InitialHP);

	// Start combat
	Orchestrator->StartCombat(Team0, Team1);

	// Verify combat started
	if (Orchestrator->GetCombatState() != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Error, TEXT("    Combat failed to start"));
		PrintTestResult("Real Attack Execution", false);
		Orchestrator->ForceEndCombat();
		Orchestrator->OnActionExecuted.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);
		CleanupTestActors(Team0);
		CleanupTestActors(Team1);
		return;
	}

	// Create attack action
	FAction AttackAction;
	AttackAction.ActionType = EActionType::Attack;
	AttackAction.AttackData = BoltAttack;
	AttackAction.Targets.Add(Target);

	// Validate action
	FActionValidationResult Validation = Orchestrator->ValidateAction(AttackAction);
	UE_LOG(LogTemp, Display, TEXT("    Attack validation: %s"),
		Validation.bIsValid ? TEXT("VALID") : *Validation.ErrorMessage);

	// Submit attack
	bool bSubmitSuccess = Orchestrator->SubmitAction(AttackAction);
	UE_LOG(LogTemp, Display, TEXT("    Submit result: %s"), bSubmitSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));

	// Check damage was dealt
	int32 FinalHP = TargetComp ? TargetComp->CurrentHP : 0;
	int32 DamageDealt = InitialHP - FinalHP;

	UE_LOG(LogTemp, Display, TEXT("    Target final HP: %d (took %d damage)"), FinalHP, DamageDealt);
	UE_LOG(LogTemp, Display, TEXT("    ActionResult.TotalDamageDealt: %d"), LastActionResult.TotalDamageDealt);
	UE_LOG(LogTemp, Display, TEXT("    ActionResult.bWasCritical: %s"), LastActionResult.bWasCritical ? TEXT("YES") : TEXT("NO"));

	// Verify results
	bool bDamageDealt = (DamageDealt > 0);
	bool bResultMatchesHP = (LastActionResult.TotalDamageDealt == DamageDealt);
	bool bActionFired = bActionExecuted;
	bool bTargetInAffected = LastActionResult.AffectedTargets.Contains(Target);

	UE_LOG(LogTemp, Display, TEXT("    Damage dealt: %s"), bDamageDealt ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("    Result matches HP change: %s"), bResultMatchesHP ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("    Target in affected list: %s"), bTargetInAffected ? TEXT("YES") : TEXT("NO"));

	bool bPassed = bSubmitSuccess && bDamageDealt && bResultMatchesHP && bActionFired && bTargetInAffected;
	PrintTestResult("Real Attack Execution", bPassed);

	// Cleanup
	Orchestrator->ForceEndCombat();
	Orchestrator->OnActionExecuted.RemoveDynamic(this, &ACombatOrchestratorTestActor::OnTestActionExecuted);
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

void ACombatOrchestratorTestActor::OnTestActionExecuted(AActor* Actor, const FActionResult& Result)
{
	LastActionResult = Result;
	bActionExecuted = true;
	UE_LOG(LogTemp, Log, TEXT("[CombatTest] OnActionExecuted: Actor=%s, Success=%d, Type=%d"),
		Actor ? *Actor->GetName() : TEXT("null"),
		Result.bSuccess,
		(int32)Result.ActionType);
}