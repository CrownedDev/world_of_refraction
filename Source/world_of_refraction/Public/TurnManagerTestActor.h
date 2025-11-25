// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TurnManagerTestActor.generated.h"

/**
 * Test actor for TurnManager
 *
 * Place in level and call RunTest from context menu to verify:
 * - Turn debt calculations
 * - Speed ratios (3:1, 2:1, etc.)
 * - Tie-breaker cascade
 * - Speed changes
 * - Death/resurrection
 */
UCLASS()
class WORLD_OF_REFRACTION_API ATurnManagerTestActor : public AActor
{
    GENERATED_BODY()

public:
    ATurnManagerTestActor();

    /** Run full test suite */
    UFUNCTION(CallInEditor, Category = "Testing")
    void RunTest();

    /** Test basic 3v3 combat */
    UFUNCTION(CallInEditor, Category = "Testing")
    void Test_Basic3v3();

    /** Test speed ratio (fast character gets more turns) */
    UFUNCTION(CallInEditor, Category = "Testing")
    void Test_SpeedRatio();

    /** Test tie-breaking cascade */
    UFUNCTION(CallInEditor, Category = "Testing")
    void Test_TieBreaking();

    /** Test speed changes mid-combat */
    UFUNCTION(CallInEditor, Category = "Testing")
    void Test_SpeedChanges();

    /** Test death/resurrection */
    UFUNCTION(CallInEditor, Category = "Testing")
    void Test_DeathResurrection();

protected:
    virtual void BeginPlay() override;

private:
    /** Helper: Create test character with specific stats */
    AActor *CreateTestCharacter(FName Name, int32 Body, int32 Mind, int32 Spirit,
                                int32 TurnSpeed, int32 AttackSpeed);

    /** Helper: Run turns and verify order */
    void SimulateTurns(int32 NumTurns);

    /** Helper: Print test results */
    void PrintTestResult(const FString &TestName, bool bPassed);

    /** Test tracking */
    int32 TestsPassed = 0;
    int32 TestsFailed = 0;
};