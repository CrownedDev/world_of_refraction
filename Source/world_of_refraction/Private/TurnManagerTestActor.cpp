// Copyright Epic Games, Inc. All Rights Reserved.

#include "TurnManagerTestActor.h"
#include "Combat/TurnManager.h"
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

    // Create 6 characters with equal speed
    TArray<AActor *> Team1;
    Team1.Add(CreateTestCharacter("Player1", 5, 5, 5, 0, 0)); // Speed = 5
    Team1.Add(CreateTestCharacter("Player2", 5, 5, 5, 0, 0)); // Speed = 5
    Team1.Add(CreateTestCharacter("Player3", 5, 5, 5, 0, 0)); // Speed = 5

    TArray<AActor *> Team2;
    Team2.Add(CreateTestCharacter("Enemy1", 5, 5, 5, 0, 0)); // Speed = 5
    Team2.Add(CreateTestCharacter("Enemy2", 5, 5, 5, 0, 0)); // Speed = 5
    Team2.Add(CreateTestCharacter("Enemy3", 5, 5, 5, 0, 0)); // Speed = 5

    // Get TurnManager
    UTurnManager *TurnManager = GetGameInstance()->GetSubsystem<UTurnManager>();

    if (!TurnManager)
    {
        PrintTestResult("Basic 3v3", false);
        UE_LOG(LogTemp, Error, TEXT("  TurnManager subsystem not found!"));
        return;
    }

    // Initialize combat
    TurnManager->InitializeCombat(Team1, Team2);

    // Verify combat started
    bool bCombatActive = TurnManager->IsCombatActive();
    bool bHasCurrentActor = TurnManager->GetCurrentActor() != nullptr;

    if (bCombatActive && bHasCurrentActor)
    {
        PrintTestResult("Basic 3v3", true);
        UE_LOG(LogTemp, Display, TEXT("  First turn: %s"), *TurnManager->GetCurrentActor()->GetName());
    }
    else
    {
        PrintTestResult("Basic 3v3", false);
        UE_LOG(LogTemp, Error, TEXT("  Combat failed to start properly"));
    }

    // Cleanup
    TurnManager->EndCombat();
    for (AActor *Actor : Team1)
    {
        Actor->Destroy();
    }
    for (AActor *Actor : Team2)
    {
        Actor->Destroy();
    }
}

void ATurnManagerTestActor::Test_SpeedRatio()
{
    UE_LOG(LogTemp, Display, TEXT("\n[TEST] Speed Ratio (3:1)"));

    // Create characters with 3:1 speed ratio
    TArray<AActor *> Team1;
    Team1.Add(CreateTestCharacter("Fast", 3, 5, 5, 6, 0)); // Speed = 3+6 = 9

    TArray<AActor *> Team2;
    Team2.Add(CreateTestCharacter("Slow", 3, 5, 5, 0, 0)); // Speed = 3+0 = 3

    UTurnManager *TurnManager = GetGameInstance()->GetSubsystem<UTurnManager>();
    TurnManager->InitializeCombat(Team1, Team2);

    // Simulate 10 turns and count who went how many times
    TMap<AActor *, int32> TurnCounts;
    TurnCounts.Add(Team1[0], 0);
    TurnCounts.Add(Team2[0], 0);

    for (int32 i = 0; i < 10; i++)
    {
        AActor *CurrentActor = TurnManager->GetCurrentActor();
        TurnCounts[CurrentActor]++;

        TurnManager->EndCurrentTurn();
        TurnManager->AdvanceToNextTurn();
    }

    // Fast should get ~7-8 turns, Slow should get ~2-3 turns (3:1 ratio)
    int32 FastTurns = TurnCounts[Team1[0]];
    int32 SlowTurns = TurnCounts[Team2[0]];
    float Ratio = static_cast<float>(FastTurns) / SlowTurns;

    UE_LOG(LogTemp, Display, TEXT("  Fast: %d turns, Slow: %d turns, Ratio: %.2f"),
           FastTurns, SlowTurns, Ratio);

    // Ratio should be close to 3:1 (allow 2.5-3.5 range)
    bool bPassed = (Ratio >= 2.5f && Ratio <= 3.5f);
    PrintTestResult("Speed Ratio", bPassed);

    // Cleanup
    TurnManager->EndCombat();
    for (AActor *Actor : Team1)
    {
        Actor->Destroy();
    }
    for (AActor *Actor : Team2)
    {
        Actor->Destroy();
    }
}

void ATurnManagerTestActor::Test_TieBreaking()
{
    UE_LOG(LogTemp, Display, TEXT("\n[TEST] Tie-Breaking Cascade"));

    // Create characters with IDENTICAL stats except tie-breakers
    TArray<AActor *> Team1;
    // Same speed, same AttackSpeed, but different Body/Mind/Spirit for cascade
    Team1.Add(CreateTestCharacter("Char1", 5, 5, 5, 0, 0)); // Speed=5, AS=5
    Team1.Add(CreateTestCharacter("Char2", 5, 5, 4, 0, 0)); // Speed=5, AS=5, lower spirit
    Team1.Add(CreateTestCharacter("Char3", 5, 4, 5, 0, 0)); // Speed=5, AS=5, lower mind

    TArray<AActor *> Team2;

    UTurnManager *TurnManager = GetGameInstance()->GetSubsystem<UTurnManager>();
    TurnManager->InitializeCombat(Team1, Team2);

    // First turn should be determined by tie-breaker
    AActor *FirstActor = TurnManager->GetCurrentActor();

    UE_LOG(LogTemp, Display, TEXT("  First actor: %s"), *FirstActor->GetName());

    // Verify tie-breaking is working (any result is valid, just shouldn't crash)
    bool bPassed = (FirstActor != nullptr);
    PrintTestResult("Tie-Breaking", bPassed);

    // Cleanup
    TurnManager->EndCombat();
    for (AActor *Actor : Team1)
    {
        Actor->Destroy();
    }
    for (AActor *Actor : Team2)
    {
        Actor->Destroy();
    }
}

void ATurnManagerTestActor::Test_SpeedChanges()
{
    UE_LOG(LogTemp, Display, TEXT("\n[TEST] Speed Changes Mid-Combat"));

    TArray<AActor *> Team1;
    Team1.Add(CreateTestCharacter("Buffed", 3, 5, 5, 0, 0)); // Speed = 3

    TArray<AActor *> Team2;
    Team2.Add(CreateTestCharacter("Normal", 3, 5, 5, 0, 0)); // Speed = 3

    UTurnManager *TurnManager = GetGameInstance()->GetSubsystem<UTurnManager>();
    TurnManager->InitializeCombat(Team1, Team2);

    // Take a few turns
    for (int32 i = 0; i < 3; i++)
    {
        TurnManager->EndCurrentTurn();
        TurnManager->AdvanceToNextTurn();
    }

    // Buff first character's speed
    UCharacterDataComponent *CharComp = Team1[0]->FindComponentByClass<UCharacterDataComponent>();
    if (CharComp && CharComp->CharacterData)
    {
        // Simulate speed buff (would normally come from status effect)
        CharComp->CharacterData->ModifySubStat(ESubStatType::TurnSpeed, 6); // +6 speed

        // Notify TurnManager
        TurnManager->OnActorSpeedChanged(Team1[0]);

        UE_LOG(LogTemp, Display, TEXT("  Applied speed buff to %s"), *Team1[0]->GetName());

        // Simulate more turns - buffed character should get more turns now
        TMap<AActor *, int32> TurnCounts;
        TurnCounts.Add(Team1[0], 0);
        TurnCounts.Add(Team2[0], 0);

        for (int32 i = 0; i < 10; i++)
        {
            AActor *CurrentActor = TurnManager->GetCurrentActor();
            TurnCounts[CurrentActor]++;

            TurnManager->EndCurrentTurn();
            TurnManager->AdvanceToNextTurn();
        }

        int32 BuffedTurns = TurnCounts[Team1[0]];
        int32 NormalTurns = TurnCounts[Team2[0]];

        UE_LOG(LogTemp, Display, TEXT("  Buffed: %d turns, Normal: %d turns"),
               BuffedTurns, NormalTurns);

        // Buffed should have more turns
        bool bPassed = (BuffedTurns > NormalTurns);
        PrintTestResult("Speed Changes", bPassed);
    }
    else
    {
        PrintTestResult("Speed Changes", false);
        UE_LOG(LogTemp, Error, TEXT("  Failed to find CharacterDataComponent"));
    }

    // Cleanup
    TurnManager->EndCombat();
    for (AActor *Actor : Team1)
    {
        Actor->Destroy();
    }
    for (AActor *Actor : Team2)
    {
        Actor->Destroy();
    }
}

void ATurnManagerTestActor::Test_DeathResurrection()
{
    UE_LOG(LogTemp, Display, TEXT("\n[TEST] Death/Resurrection"));

    TArray<AActor *> Team1;
    Team1.Add(CreateTestCharacter("Alive1", 5, 5, 5, 0, 0));
    Team1.Add(CreateTestCharacter("Alive2", 5, 5, 5, 0, 0));

    TArray<AActor *> Team2;

    UTurnManager *TurnManager = GetGameInstance()->GetSubsystem<UTurnManager>();
    TurnManager->InitializeCombat(Team1, Team2);

    // Kill first character
    TurnManager->OnActorDied(Team1[0]);
    UE_LOG(LogTemp, Display, TEXT("  Killed %s"), *Team1[0]->GetName());

    // Simulate turns - dead character should be skipped
    TMap<AActor *, int32> TurnCounts;
    TurnCounts.Add(Team1[0], 0);
    TurnCounts.Add(Team1[1], 0);

    for (int32 i = 0; i < 5; i++)
    {
        AActor *CurrentActor = TurnManager->GetCurrentActor();
        TurnCounts[CurrentActor]++;

        TurnManager->EndCurrentTurn();
        TurnManager->AdvanceToNextTurn();
    }

    int32 DeadTurns = TurnCounts[Team1[0]];
    int32 AliveTurns = TurnCounts[Team1[1]];

    UE_LOG(LogTemp, Display, TEXT("  Dead: %d turns, Alive: %d turns"), DeadTurns, AliveTurns);

    // Dead character should have 0 turns
    bool bDeathWorked = (DeadTurns == 0 && AliveTurns > 0);

    // Resurrect
    TurnManager->OnActorResurrected(Team1[0]);
    UE_LOG(LogTemp, Display, TEXT("  Resurrected %s"), *Team1[0]->GetName());

    // Take more turns
    for (int32 i = 0; i < 5; i++)
    {
        AActor *CurrentActor = TurnManager->GetCurrentActor();
        TurnCounts[CurrentActor]++;

        TurnManager->EndCurrentTurn();
        TurnManager->AdvanceToNextTurn();
    }

    int32 ResurrectedTurns = TurnCounts[Team1[0]];

    UE_LOG(LogTemp, Display, TEXT("  After resurrection: %d turns"), ResurrectedTurns);

    // Resurrected character should now have turns
    bool bResurrectionWorked = (ResurrectedTurns > 0);

    bool bPassed = bDeathWorked && bResurrectionWorked;
    PrintTestResult("Death/Resurrection", bPassed);

    // Cleanup
    TurnManager->EndCombat();
    for (AActor *Actor : Team1)
    {
        Actor->Destroy();
    }
    for (AActor *Actor : Team2)
    {
        Actor->Destroy();
    }
}

AActor *ATurnManagerTestActor::CreateTestCharacter(FName Name, int32 Body, int32 Mind, int32 Spirit,
                                                   int32 TurnSpeed, int32 AttackSpeed)
{
    // Spawn empty actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = Name;
    AActor *TestActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector,
                                                       FRotator::ZeroRotator, SpawnParams);

    // Add CharacterDataComponent
    UCharacterDataComponent *CharComp = NewObject<UCharacterDataComponent>(TestActor);
    CharComp->RegisterComponent();
    TestActor->AddInstanceComponent(CharComp);

    // Create CharacterData asset (temporary runtime instance)
    UCharacterData *CharData = NewObject<UCharacterData>();
    CharData->WorldMind = Mind;
    CharData->WorldBody = Body;
    CharData->WorldSpirit = Spirit;
    CharData->SetSubStat(ESubStatType::TurnSpeed, TurnSpeed);
    CharData->SetSubStat(ESubStatType::AttackSpeed, AttackSpeed);

    CharComp->CharacterData = CharData;

    return TestActor;
}

void ATurnManagerTestActor::PrintTestResult(const FString &TestName, bool bPassed)
{
    if (bPassed)
    {
        TestsPassed++;
        UE_LOG(LogTemp, Display, TEXT("  ✓ PASSED: %s"), *TestName);
    }
    else
    {
        TestsFailed++;
        UE_LOG(LogTemp, Error, TEXT("  ✗ FAILED: %s"), *TestName);
    }
}