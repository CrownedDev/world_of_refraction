// AIDecisionManager.h
// AI decision making for combat turns

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EAIDifficulty.h"
#include "ActionStructs.h"
#include "EDefenseType.h"
#include "AIDecisionManager.generated.h"

class ACombatOrchestrator;
class UCharacterDataComponent;
class ULoadoutComponent;
class UDefenseSystem;

/**
 * Handles AI decision making during combat
 * Routes turn decisions through standard action pipeline
 */
UCLASS()
class WORLD_OF_REFRACTION_API UAIDecisionManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;
    virtual void Deinitialize() override;

    // ==================== COMBAT REGISTRATION ====================

    /** Set the active combat orchestrator */
    UFUNCTION(BlueprintCallable, Category = "AI")
    void SetCombatOrchestrator(ACombatOrchestrator *Orchestrator);

    /** Clear combat reference */
    UFUNCTION(BlueprintCallable, Category = "AI")
    void ClearCombatOrchestrator();

    // ==================== DECISION MAKING ====================

    /**
     * Request AI decision for an actor's turn
     * Applies thinking delay based on difficulty, then submits action
     */
    UFUNCTION(BlueprintCallable, Category = "AI")
    void RequestDecision(AActor *AIActor);

    // ==================== DEFENSE DECISIONS ====================

    /**
     * Schedule defense decision for AI defender
     * Called by DefenseSystem when window opens
     */
    UFUNCTION(BlueprintCallable, Category = "AI|Defense")
    void ScheduleDefenseDecision(AActor *Defender, float AttackSize, int32 BaseDamage, float WindowDuration);

private:
    // ==================== INTERNAL ====================

    /** Active combat reference */
    UPROPERTY()
    ACombatOrchestrator *CurrentCombat = nullptr;

    /** Timer for thinking delay */
    FTimerHandle ThinkingTimerHandle;

    /** Actor waiting for decision */
    UPROPERTY()
    AActor *PendingActor = nullptr;

    // ==================== DECISION LOGIC ====================

    /** Called after thinking delay - makes and submits decision */
    void ExecuteDecision();

    /** Build action for AI actor */
    FAction BuildAction(AActor *AIActor);

    /** Pick a random action type based on available options */
    EActionType ChooseActionType(AActor *AIActor, ULoadoutComponent *Loadout);

    /** Get thinking delay range for difficulty */
    void GetThinkingDelayRange(EAIDifficulty Difficulty, float &OutMin, float &OutMax) const;

    /** Calculate random thinking delay */
    float CalculateThinkingDelay(EAIDifficulty Difficulty) const;

    // ==================== DEFENSE DECISIONS ====================

    /** Defense system reference */
    UPROPERTY()
    UDefenseSystem *DefenseSystemRef = nullptr;

    /** Active defense timers per actor */
    TMap<AActor *, FTimerHandle> DefenseTimerHandles;

    // ==================== DEFENSE LOGIC ====================

    /** Choose defense type based on attack and difficulty */
    EDefenseType ChooseDefenseType(AActor *Defender, float AttackSize, EAIDifficulty Difficulty);

    /** Get defense attempt chance for difficulty */
    float GetDefenseAttemptChance(EAIDifficulty Difficulty) const;

    /** Get defense timing accuracy for difficulty */
    float GetDefenseAccuracy(EAIDifficulty Difficulty) const;

    /** Calculate reaction delay for defense */
    float CalculateDefenseReactionDelay(EAIDifficulty Difficulty, float WindowDuration) const;

    // ==================== QUERY ====================

    /** Get current difficulty from combat */
    UFUNCTION(BlueprintPure, Category = "AI")
    EAIDifficulty GetCurrentDifficulty() const;
};