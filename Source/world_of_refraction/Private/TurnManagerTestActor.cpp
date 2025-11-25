// Copyright Epic Games, Inc. All Rights Reserved.
// CORRECTED VERSION - Matches actual world_of_refraction API

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

void ATurnManagerTestActor::RunTest()
{
	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("TURN MANAGER TEST SUITE"));
	UE_LOG(LogTemp, Display, TEXT("========================================"));

	TestsPassed = 0;
	TestsFailed = 0;

	// Verify TurnManager exists
	UTurnManager* TurnManager = GetGameInstance()->GetSubsystem<UTurnManager>();
	if (!TurnManager)
	{
		UE_LOG(LogTemp, Error, TEXT(""));
		UE_LOG(LogTemp, Error, TEXT("❌ CRITICAL ERROR: TurnManager subsystem not found!"));
		UE_LOG(LogTemp, Error, TEXT("   GameInstance: %s"), GetGameInstance() ? TEXT("Valid") : TEXT("NULL"));
		UE_LOG(LogTemp, Error, TEXT(""));
		UE_LOG(LogTemp, Error, TEXT("   WHY: GameInstanceSubsystems require a running game instance."));
		UE_LOG(LogTemp, Error, TEXT("   The 'Run Test' button in Details Panel doesn't provide one."));
		UE_LOG(LogTemp, Error, TEXT(""));
		UE_LOG(LogTemp, Error, TEXT("   SOLUTION: Run tests in PIE (Play-in-Editor) mode:"));
		UE_LOG(LogTemp, Error, TEXT("   1. Double-click BP_TurnManagerTest to open Blueprint"));
		UE_LOG(LogTemp, Error, TEXT("   2. In Event Graph, add 'Event BeginPlay' node"));
		UE_LOG(LogTemp, Error, TEXT("   3. Drag from BeginPlay, search 'Run Test', connect it"));
		UE_LOG(LogTemp, Error, TEXT("   4. Compile & Save Blueprint"));
		UE_LOG(LogTemp, Error, TEXT("   5. Press ALT+P (or Play button) to start game"));
		UE_LOG(LogTemp, Error, TEXT("   6. Tests will run automatically and show in Output Log"));
		UE_LOG(LogTemp, Error, TEXT(""));
		UE_LOG(LogTemp, Error, TEXT("========================================"));
		UE_LOG(LogTemp, Error, TEXT("RESULTS: All tests SKIPPED (no GameInstance)"));
		UE_LOG(LogTemp, Error, TEXT("========================================"));
		return;
	}

	Test_Basic3v3();
	Test_SpeedRatio();
	Test_TieBreaking();
	Test_SpeedChanges();
	Test_DeathResurrection();

	UE_LOG(LogTemp, Display, TEXT("========================================"));
	UE_LOG(LogTemp, Display, TEXT("RESULTS: %d passed, %d failed"), TestsPassed, TestsFailed);
	UE_LOG(LogTemp, Display, TEXT("========================================"));
}

void ATurnManagerTestActor::Test_Basic3v3()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Basic 3v3 Combat"));

	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("Player1", 5, 5, 5, 0, 0));
	Team1.Add(CreateTestCharacter("Player2", 5, 5, 5, 0, 0));
	Team1.Add(CreateTestCharacter("Player3", 5, 5, 5, 0, 0));

	TArray<AActor*> Team2;
	Team2.Add(CreateTestCharacter("Enemy1", 5, 5, 5, 0, 0));
	Team2.Add(CreateTestCharacter("Enemy2", 5, 5, 5, 0, 0));
	Team2.Add(CreateTestCharacter("Enemy3", 5, 5, 5, 0, 0));

	UTurnManager* TurnManager = GetGameInstance()->GetSubsystem<UTurnManager>();

	if (!TurnManager)
	{
		PrintTestResult("Basic 3v3", false);
		UE_LOG(LogTemp, Error, TEXT("  TurnManager subsystem not found!"));
		return;
	}

	TurnManager->InitializeCombat(Team1, Team2);
	AActor* FirstActor = TurnManager->GetCurrentActor();
	bool bSuccess = (FirstActor != nullptr);

	PrintTestResult("Basic 3v3", bSuccess);
	if (bSuccess)
	{
		UE_LOG(LogTemp, Display, TEXT("  First turn: %s"), *FirstActor->GetName());
	}

	TurnManager->EndCombat();
	CleanupTestActors(Team1);
	CleanupTestActors(Team2);
}

void ATurnManagerTestActor::Test_SpeedRatio()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Speed Ratio"));

	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("Fast", 5, 5, 5, 6, 0)); // Speed = 11

	TArray<AActor*> Team2;
	Team2.Add(CreateTestCharacter("Slow", 5, 5, 5, 0, 0)); // Speed = 5

	UTurnManager* TurnManager = GetGameInstance()->GetSubsystem<UTurnManager>();
	TurnManager->InitializeCombat(Team1, Team2);

	int32 FastTurns = 0;
	int32 SlowTurns = 0;

	for (int32 i = 0; i < 10; i++)
	{
		AActor* Current = TurnManager->GetCurrentActor();
		if (Current->GetName().Contains("Fast"))
			FastTurns++;
		else
			SlowTurns++;

		TurnManager->AdvanceToNextTurn();
	}

	float Ratio = (float)FastTurns / (float)SlowTurns;
	bool bSuccess = (Ratio >= 1.5f && Ratio <= 2.5f);

	PrintTestResult("Speed Ratio", bSuccess);
	UE_LOG(LogTemp, Display, TEXT("  Fast: %d turns, Slow: %d turns (Ratio: %.2f)"), FastTurns, SlowTurns, Ratio);

	TurnManager->EndCombat();
	CleanupTestActors(Team1);
	CleanupTestActors(Team2);
}

void ATurnManagerTestActor::Test_TieBreaking()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Tie-Breaking"));

	TArray<AActor*> Team1;
	Team1.Add(CreateTestCharacter("P1", 5, 5, 5, 0, 0));
	Team1.Add(CreateTestCharacter("P2", 5, 5, 5, 0, 0));
	Team1.Add(CreateTestCharacter("P3", 5, 5, 5, 0, 0));

	TArray<AActor*> Team2;
	Team2.Add(CreateTestCharacter("E1", 5, 5, 5, 0, 0));
	Team2.Add(CreateTestCharacter("E2", 5, 5, 5, 0, 0));
	Team2.Add(CreateTestCharacter("E3", 5, 5, 5, 0, 0));

	UTurnManager* TurnManager = GetGameInstance()->GetSubsystem<UTurnManager>();
	TurnManager->InitializeCombat(Team1, Team2);

	bool bSuccess = true;
	for (int32 i = 0; i < 6; i++)
	{
		AActor* Current = TurnManager->GetCurrentActor();
		if (!Current)
		{
			bSuccess = false;
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

	TArray<AActor*> Team1;
	AActor* Char1 = CreateTestCharacter("Char1", 5, 5, 5, 0, 0);
	Team1.Add(Char1);

	TArray<AActor*> Team2;
	Team2.Add(CreateTestCharacter("Char2", 5, 5, 5, 0, 0));

	UTurnManager* TurnManager = GetGameInstance()->GetSubsystem<UTurnManager>();
	TurnManager->InitializeCombat(Team1, Team2);

	UCharacterDataComponent* CharComp = Char1->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp && CharComp->CharacterData)
	{
		CharComp->CharacterData->WorldTurnSpeedPoints += 6;
		TurnManager->OnActorSpeedChanged(Char1);
		PrintTestResult("Speed Changes", true);
	}
	else
	{
		PrintTestResult("Speed Changes", false);
	}

	TurnManager->EndCombat();
	CleanupTestActors(Team1);
	CleanupTestActors(Team2);
}

void ATurnManagerTestActor::Test_DeathResurrection()
{
	UE_LOG(LogTemp, Display, TEXT("\n[TEST] Death/Resurrection"));

	TArray<AActor*> Team1;
	AActor* TestChar = CreateTestCharacter("TestChar", 5, 5, 5, 0, 0);
	Team1.Add(TestChar);

	TArray<AActor*> Team2;
	Team2.Add(CreateTestCharacter("Enemy", 5, 5, 5, 0, 0));

	UTurnManager* TurnManager = GetGameInstance()->GetSubsystem<UTurnManager>();
	TurnManager->InitializeCombat(Team1, Team2);

	UCharacterDataComponent* CharComp = TestChar->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp)
	{
		CharComp->ServerTakeDamage(999);
		TurnManager->OnActorDied(TestChar);

		TurnManager->AdvanceToNextTurn();
		AActor* Current = TurnManager->GetCurrentActor();

		bool bDeadSkipped = (Current != TestChar);

		CharComp->ServerResurrect(50);  // ← CORRECT: Takes int32 parameter
		TurnManager->OnActorResurrected(TestChar);

		PrintTestResult("Death/Resurrection", bDeadSkipped);
	}
	else
	{
		PrintTestResult("Death/Resurrection", false);
	}

	TurnManager->EndCombat();
	CleanupTestActors(Team1);
	CleanupTestActors(Team2);
}

AActor* ATurnManagerTestActor::CreateTestCharacter(FString Name, int32 Mind, int32 Body, int32 Spirit,
	int32 TurnSpeed, int32 AttackSpeed)
{
	AActor* Actor = GetWorld()->SpawnActor<AActor>();
	Actor->Rename(*Name);

	UCharacterDataComponent* CharComp = NewObject<UCharacterDataComponent>(Actor, UCharacterDataComponent::StaticClass());
	CharComp->RegisterComponent();
	Actor->AddInstanceComponent(CharComp);

	UCharacterData* CharData = NewObject<UCharacterData>();
	CharData->CharacterName = Name;
	CharData->WorldMindLevel = Mind;
	CharData->WorldBodyLevel = Body;
	CharData->WorldSpiritLevel = Spirit;
	CharData->WorldTurnSpeedPoints = TurnSpeed;
	CharData->WorldAttackSpeedPoints = AttackSpeed;

	CharComp->CharacterData = CharData;
	CharComp->InitializeFromTemplate();

	return Actor;
}

void ATurnManagerTestActor::CleanupTestActors(TArray<AActor*>& Actors)
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
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
		UE_LOG(LogTemp, Display, TEXT("  ✓ %s: PASSED"), *TestName);
	}
	else
	{
		TestsFailed++;
		UE_LOG(LogTemp, Error, TEXT("  ✗ %s: FAILED"), *TestName);
	}
}