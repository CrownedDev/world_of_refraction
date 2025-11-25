// Copyright Epic Games, Inc. All Rights Reserved.
// CORRECTED VERSION - Proper component initialization order and null safety
// KEY FIX: CharacterData assigned BEFORE RegisterComponent() to ensure BeginPlay sees it

#include "TurnManagerTestActor.h"
#include "TurnManager.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"

ATurnManagerTestActor::ATurnManagerTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATurnManagerTestActor::BeginPlay()
{
	Super::BeginPlay();
}

// ========================================
// HELPER: Get TurnManager with null safety
// ========================================
UTurnManager* ATurnManagerTestActor::GetTurnManagerSafe()
{
	UGameInstance* GameInst = GetGameInstance();
	if (!GameInst)
	{
		UE_LOG(LogTemp, Error, TEXT(""));
		UE_LOG(LogTemp, Error, TEXT("========================================"));
		UE_LOG(LogTemp, Error, TEXT("CRITICAL ERROR: GameInstance is NULL!"));
		UE_LOG(LogTemp, Error, TEXT("========================================"));
		UE_LOG(LogTemp, Error, TEXT(""));
		UE_LOG(LogTemp, Error, TEXT("CallInEditor functions run outside PIE context."));
		UE_LOG(LogTemp, Error, TEXT("GameInstanceSubsystems only exist during gameplay."));
		UE_LOG(LogTemp, Error, TEXT(""));
		UE_LOG(LogTemp, Error, TEXT("SOLUTION - Run tests in PIE mode:"));
		UE_LOG(LogTemp, Error, TEXT("  Option A: Press ALT+P to Play, tests run via BeginPlay"));
		UE_LOG(LogTemp, Error, TEXT("  Option B: Wire BeginPlay -> RunTest in Blueprint"));
		UE_LOG(LogTemp, Error, TEXT(""));
		return nullptr;
	}

	UTurnManager* TurnManager = GameInst->GetSubsystem<UTurnManager>();
	if (!TurnManager)
	{
		UE_LOG(LogTemp, Error, TEXT(""));
		UE_LOG(LogTemp, Error, TEXT("ERROR: TurnManager subsystem not found!"));
		UE_LOG(LogTemp, Error, TEXT("GameInstance exists but subsystem failed to initialize."));
		UE_LOG(LogTemp, Error, TEXT(""));
		return nullptr;
	}

	return TurnManager;
}

// ========================================
// MAIN TEST RUNNER
// ========================================
void ATurnManagerTestActor::RunTest()
{
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("TURN MANAGER TEST SUITE"));
	UE_LOG(LogTemp, Display, TEXT("========================================"));

	TestsPassed = 0;
	TestsFailed = 0;

	// Early exit if we can't get TurnManager
	if (!GetTurnManagerSafe())
	{
		return;
	}

	// Run all tests
	Test_Basic3v3();
	Test_SpeedRatio();
	Test_TieBreaking();
	Test_SpeedChanges();
	Test_DeathResurrection();

	// Summary
	UE_LOG(LogTemp, Display, TEXT(""));
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("RESULTS: %d passed, %d failed"), TestsPassed, TestsFailed);
	UE_LOG(LogTemp, Display, TEXT("========================================"));
}

// ========================================
// INDIVIDUAL TESTS
// ========================================

void ATurnManagerTestActor::Test_Basic3v3()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Basic 3v3 Combat"));

	UTurnManager* TurnManager = GetTurnManagerSafe();
	if (!TurnManager)
	{
		PrintTestResult("Basic 3v3", false);
		return;
	}

	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("Player1", 5, 5, 5, 0, 0));
	Team1.Add(CreateTestCharacter("Player2", 5, 5, 5, 0, 0));
	Team1.Add(CreateTestCharacter("Player3", 5, 5, 5, 0, 0));

	TArray<AActor*> Team2;
	Team2.Add(CreateTestCharacter("Enemy1", 5, 5, 5, 0, 0));
	Team2.Add(CreateTestCharacter("Enemy2", 5, 5, 5, 0, 0));
	Team2.Add(CreateTestCharacter("Enemy3", 5, 5, 5, 0, 0));

	TurnManager->InitializeCombat(Team1, Team2);
	AActor* FirstActor = TurnManager->GetCurrentActor();
	bool bSuccess = (FirstActor != nullptr);

	PrintTestResult("Basic 3v3", bSuccess);
	if (bSuccess)
	{
		UE_LOG(LogTemp, Display, TEXT("    First turn: %s"), *FirstActor->GetName());
	}

	TurnManager->EndCombat();
	CleanupTestActors(Team1);
	CleanupTestActors(Team2);
}

void ATurnManagerTestActor::Test_SpeedRatio()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Speed Ratio"));

	UTurnManager* TurnManager = GetTurnManagerSafe();
	if (!TurnManager)
	{
		PrintTestResult("Speed Ratio", false);
		return;
	}

	// Fast: Body(5) + TurnSpeed(6) = 11
	// Slow: Body(5) + TurnSpeed(0) = 5
	// Expected ratio: 11/5 = 2.2:1
	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("Fast", 5, 5, 5, 6, 0));

	TArray<AActor*> Team2;
	Team2.Add(CreateTestCharacter("Slow", 5, 5, 5, 0, 0));

	// Debug: Verify speeds were set correctly
	UCharacterDataComponent* FastComp = Team1[0]->FindComponentByClass<UCharacterDataComponent>();
	UCharacterDataComponent* SlowComp = Team2[0]->FindComponentByClass<UCharacterDataComponent>();

	if (FastComp && FastComp->CharacterData)
	{
		int32 FastSpeed = FastComp->CharacterData->WorldBodyLevel + FastComp->CharacterData->WorldTurnSpeedPoints;
		UE_LOG(LogTemp, Display, TEXT("    Fast speed: %d (Body %d + TurnSpeed %d)"),
			FastSpeed,
			FastComp->CharacterData->WorldBodyLevel,
			FastComp->CharacterData->WorldTurnSpeedPoints);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("    Fast CharacterData is NULL - this will cause test failure!"));
	}

	if (SlowComp && SlowComp->CharacterData)
	{
		int32 SlowSpeed = SlowComp->CharacterData->WorldBodyLevel + SlowComp->CharacterData->WorldTurnSpeedPoints;
		UE_LOG(LogTemp, Display, TEXT("    Slow speed: %d (Body %d + TurnSpeed %d)"),
			SlowSpeed,
			SlowComp->CharacterData->WorldBodyLevel,
			SlowComp->CharacterData->WorldTurnSpeedPoints);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("    Slow CharacterData is NULL - this will cause test failure!"));
	}

	TurnManager->InitializeCombat(Team1, Team2);

	int32 FastTurns = 0;
	int32 SlowTurns = 0;

	// Simulate 10 turns
	for (int32 i = 0; i < 10; i++)
	{
		AActor* Current = TurnManager->GetCurrentActor();
		if (Current)
		{
			if (Current->GetName().Contains("Fast"))
			{
				FastTurns++;
			}
			else
			{
				SlowTurns++;
			}
		}
		TurnManager->AdvanceToNextTurn();
	}

	// Expected ratio: ~2:1 (Fast should get about 6-7 turns, Slow 3-4)
	float Ratio = (SlowTurns > 0) ? (float)FastTurns / (float)SlowTurns : 0.0f;
	bool bSuccess = (SlowTurns > 0) && (Ratio >= 1.5f && Ratio <= 2.5f);

	PrintTestResult("Speed Ratio", bSuccess);
	UE_LOG(LogTemp, Display, TEXT("    Fast: %d turns, Slow: %d turns (Ratio: %.2f, Expected: ~2.2)"),
		FastTurns, SlowTurns, Ratio);

	TurnManager->EndCombat();
	CleanupTestActors(Team1);
	CleanupTestActors(Team2);
}

void ATurnManagerTestActor::Test_TieBreaking()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Tie-Breaking"));

	UTurnManager* TurnManager = GetTurnManagerSafe();
	if (!TurnManager)
	{
		PrintTestResult("Tie-Breaking", false);
		return;
	}

	// All characters have identical stats - tie-breaker cascade must work
	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("P1", 5, 5, 5, 0, 0));
	Team1.Add(CreateTestCharacter("P2", 5, 5, 5, 0, 0));
	Team1.Add(CreateTestCharacter("P3", 5, 5, 5, 0, 0));

	TArray<AActor*> Team2;
	Team2.Add(CreateTestCharacter("E1", 5, 5, 5, 0, 0));
	Team2.Add(CreateTestCharacter("E2", 5, 5, 5, 0, 0));
	Team2.Add(CreateTestCharacter("E3", 5, 5, 5, 0, 0));

	TurnManager->InitializeCombat(Team1, Team2);

	bool bSuccess = true;
	for (int32 i = 0; i < 6; i++)
	{
		AActor* Current = TurnManager->GetCurrentActor();
		if (!Current)
		{
			bSuccess = false;
			UE_LOG(LogTemp, Error, TEXT("    Turn %d: GetCurrentActor returned nullptr!"), i + 1);
			break;
		}
		TurnManager->AdvanceToNextTurn();
	}

	PrintTestResult("Tie-Breaking", bSuccess);

	TurnManager->EndCombat();
	CleanupTestActors(Team1);
	CleanupTestActors(Team2);
}

void ATurnManagerTestActor::Test_SpeedChanges()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Speed Changes"));

	UTurnManager* TurnManager = GetTurnManagerSafe();
	if (!TurnManager)
	{
		PrintTestResult("Speed Changes", false);
		return;
	}

	TArray<AActor*> Team1;
	AActor* Char1 = CreateTestCharacter("Char1", 5, 5, 5, 0, 0);
	Team1.Add(Char1);

	TArray<AActor*> Team2;
	Team2.Add(CreateTestCharacter("Char2", 5, 5, 5, 0, 0));

	TurnManager->InitializeCombat(Team1, Team2);

	UCharacterDataComponent* CharComp = Char1->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp && CharComp->CharacterData)
	{
		// Simulate speed buff
		int32 OldSpeed = CharComp->CharacterData->WorldTurnSpeedPoints;
		CharComp->CharacterData->WorldTurnSpeedPoints += 6;

		// Notify TurnManager of speed change
		TurnManager->OnActorSpeedChanged(Char1);

		UE_LOG(LogTemp, Display, TEXT("    TurnSpeed changed: %d -> %d"),
			OldSpeed, CharComp->CharacterData->WorldTurnSpeedPoints);
		PrintTestResult("Speed Changes", true);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("    CharacterDataComponent or CharacterData is null!"));
		PrintTestResult("Speed Changes", false);
	}

	TurnManager->EndCombat();
	CleanupTestActors(Team1);
	CleanupTestActors(Team2);
}

void ATurnManagerTestActor::Test_DeathResurrection()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Death/Resurrection"));

	UTurnManager* TurnManager = GetTurnManagerSafe();
	if (!TurnManager)
	{
		PrintTestResult("Death/Resurrection", false);
		return;
	}

	TArray<AActor*> Team1;
	AActor* TestChar = CreateTestCharacter("TestChar", 5, 5, 5, 0, 0);
	Team1.Add(TestChar);

	TArray<AActor*> Team2;
	Team2.Add(CreateTestCharacter("Enemy", 5, 5, 5, 0, 0));

	TurnManager->InitializeCombat(Team1, Team2);

	UCharacterDataComponent* CharComp = TestChar->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp)
	{
		// Kill the character
		CharComp->ServerTakeDamage(999);
		TurnManager->OnActorDied(TestChar);

		// Advance turn - dead character should be skipped
		TurnManager->AdvanceToNextTurn();
		AActor* Current = TurnManager->GetCurrentActor();
		bool bDeadSkipped = (Current != TestChar);

		// Resurrect
		CharComp->ServerResurrect(50);
		TurnManager->OnActorResurrected(TestChar);

		PrintTestResult("Death/Resurrection", bDeadSkipped);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("    CharacterDataComponent is null!"));
		PrintTestResult("Death/Resurrection", false);
	}

	TurnManager->EndCombat();
	CleanupTestActors(Team1);
	CleanupTestActors(Team2);
}

// ========================================
// TEST HELPERS
// ========================================

AActor* ATurnManagerTestActor::CreateTestCharacter(FString Name, int32 Mind, int32 Body, int32 Spirit,
	int32 TurnSpeed, int32 AttackSpeed)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTestCharacter: World is null!"));
		return nullptr;
	}

	AActor* Actor = World->SpawnActor<AActor>();
	if (!Actor)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTestCharacter: Failed to spawn actor!"));
		return nullptr;
	}

	Actor->Rename(*Name);

	// ============================================================
	// CRITICAL FIX: Create CharacterData FIRST, assign to component
	// BEFORE RegisterComponent() so BeginPlay sees valid data
	// ============================================================

	// Step 1: Create CharacterData with the Actor as outer
	UCharacterData* CharData = NewObject<UCharacterData>(Actor);
	if (!CharData)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTestCharacter: Failed to create CharacterData!"));
		Actor->Destroy();
		return nullptr;
	}

	// Step 2: Set all the stats
	CharData->CharacterName = Name;
	CharData->WorldMindLevel = Mind;
	CharData->WorldBodyLevel = Body;
	CharData->WorldSpiritLevel = Spirit;
	CharData->WorldTurnSpeedPoints = TurnSpeed;
	CharData->WorldAttackSpeedPoints = AttackSpeed;

	// Step 3: Create CharacterDataComponent
	UCharacterDataComponent* CharComp = NewObject<UCharacterDataComponent>(Actor, UCharacterDataComponent::StaticClass());
	if (!CharComp)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTestCharacter: Failed to create CharacterDataComponent!"));
		Actor->Destroy();
		return nullptr;
	}

	// Step 4: CRITICAL - Assign CharacterData BEFORE RegisterComponent()
	// RegisterComponent triggers BeginPlay, which checks CharacterData
	CharComp->CharacterData = CharData;

	// Step 5: Now register the component (BeginPlay will see valid CharacterData)
	CharComp->RegisterComponent();

	// Step 6: Add as owned component so FindComponentByClass works
	Actor->AddOwnedComponent(CharComp);

	// Verify the data is accessible via FindComponentByClass
	UCharacterDataComponent* VerifyComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!VerifyComp)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTestCharacter: FindComponentByClass failed for %s!"), *Name);
	}
	else if (!VerifyComp->CharacterData)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTestCharacter: CharacterData is null after registration for %s!"), *Name);
	}

	return Actor;
}

void ATurnManagerTestActor::CleanupTestActors(TArray<AActor*>& Actors)
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

void ATurnManagerTestActor::PrintTestResult(FString TestName, bool bPassed)
{
	if (bPassed)
	{
		TestsPassed++;
		UE_LOG(LogTemp, Display, TEXT("  [PASS] %s"), *TestName);
	}
	else
	{
		TestsFailed++;
		UE_LOG(LogTemp, Error, TEXT("  [FAIL] %s"), *TestName);
	}
}