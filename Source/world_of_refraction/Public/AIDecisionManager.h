// AIDecisionManager.h
// AI decision making for combat turns

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EAIDifficulty.h"
#include "ActionStructs.h"
#include "AIDecisionManager.generated.h"

class ACombatOrchestrator;
class UCharacterDataComponent;
class ULoadoutComponent;

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

    // ==================== QUERY ====================

    /** Get current difficulty from combat */
    UFUNCTION(BlueprintPure, Category = "AI")
    EAIDifficulty GetCurrentDifficulty() const;

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
};