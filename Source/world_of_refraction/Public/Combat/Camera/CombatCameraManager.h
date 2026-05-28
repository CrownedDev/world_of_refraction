// CombatCameraManager.h
// Manages all camera transitions during combat

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Camera/ECombatCameraState.h"
#include "Combat/Camera/EActionCameraPhase.h"
#include "CombatCameraManager.generated.h"

class ACameraActor;
class ACombatOrchestrator;

/**
 * Combat Camera Manager
 *
 * Placed in level to manage camera transitions during combat.
 * References placed Home cameras, dynamically positions others.
 *
 * Setup:
 * 1. Place this actor in level
 * 2. Place two CameraActors for Home cameras
 * 3. Assign Home camera references in Details panel
 * 4. CombatOrchestrator will find and use this manager
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API ACombatCameraManager : public AActor
{
    GENERATED_BODY()

public:
    ACombatCameraManager();

    // ==================== PLACED CAMERA REFERENCES ====================

    /** Home camera for Team 0 (placed in level) */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Cameras|Home")
    ACameraActor *Team0HomeCamera;

    /** Home camera for Team 1 (placed in level) */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Cameras|Home")
    ACameraActor *Team1HomeCamera;

    // ==================== BLEND SETTINGS ====================

    /** Blend time for Home <-> Character transitions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cameras|Blend")
    float HomeToCharacterBlendTime = 0.5f;

    /** Blend time for Character <-> Selection transitions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cameras|Blend")
    float CharacterToSelectionBlendTime = 0.3f;

    /** Blend time for Selection <-> Action transitions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cameras|Blend")
    float SelectionToActionBlendTime = 0.4f;

    /** Blend time for Action <-> Home transitions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cameras|Blend")
    float ActionToHomeBlendTime = 0.6f;

    // ==================== CAMERA POSITIONING ====================

    /** Distance multiplier for Action camera pullback */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cameras|Positioning")
    float ActionCameraDistanceMultiplier = 1.2f;

    /** Height offset for Action camera */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cameras|Positioning")
    float ActionCameraHeightOffset = 150.0f;

    /** Distance for Character camera close-up */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cameras|Positioning")
    float CharacterCameraDistance = 300.0f;

    /** Height offset for Character camera */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cameras|Positioning")
    float CharacterCameraHeightOffset = 50.0f;

    // ==================== STATE ====================

    /** Current camera state */
    UPROPERTY(BlueprintReadOnly, Category = "Cameras|State")
    ECombatCameraState CurrentState = ECombatCameraState::None;

    /** Current action phase (only valid when in Action state) */
    UPROPERTY(BlueprintReadOnly, Category = "Cameras|State")
    EActionCameraPhase CurrentActionPhase = EActionCameraPhase::None;

    /** Which team the local player is on (for Home camera selection) */
    UPROPERTY(BlueprintReadWrite, Category = "Cameras|State")
    int32 LocalPlayerTeam = 0;

    /** Set which team the local player controls */
    void SetLocalPlayerTeam(int32 TeamIndex) { LocalPlayerTeam = TeamIndex; }

    // ==================== COMBAT INTEGRATION ====================

    /** Initialize camera system for combat */
    UFUNCTION(BlueprintCallable, Category = "Combat Camera")
    void InitializeForCombat(ACombatOrchestrator *Orchestrator);

    /** End camera system, return to default */
    UFUNCTION(BlueprintCallable, Category = "Combat Camera")
    void EndCombat();

    // ==================== STATE TRANSITIONS ====================

    /** Transition to Home camera */
    UFUNCTION(BlueprintCallable, Category = "Combat Camera")
    void TransitionToHome();

    /** Transition to Character camera for turn start */
    UFUNCTION(BlueprintCallable, Category = "Combat Camera")
    void TransitionToCharacter(AActor *Character);

    /** Transition to Selection camera */
    UFUNCTION(BlueprintCallable, Category = "Combat Camera")
    void TransitionToSelection(AActor *Target);

    /** Transition to Action camera */
    UFUNCTION(BlueprintCallable, Category = "Combat Camera")
    void TransitionToAction(AActor *Attacker, AActor *Target);

    // ==================== ACTION PHASE CONTROL ====================

    /** Set action camera phase (adjusts framing) */
    UFUNCTION(BlueprintCallable, Category = "Combat Camera")
    void SetActionPhase(EActionCameraPhase Phase);

    /** Update action camera to follow movement (call during Approach phase) */
    UFUNCTION(BlueprintCallable, Category = "Combat Camera")
    void UpdateActionCameraFollow(AActor *MovingActor, AActor *Target);

    // ==================== QUERIES ====================

    /** Get current camera state */
    UFUNCTION(BlueprintPure, Category = "Combat Camera")
    ECombatCameraState GetCurrentState() const { return CurrentState; }

    /** Get current action phase */
    UFUNCTION(BlueprintPure, Category = "Combat Camera")
    EActionCameraPhase GetCurrentActionPhase() const { return CurrentActionPhase; }

    /** Is the camera system active? */
    UFUNCTION(BlueprintPure, Category = "Combat Camera")
    bool IsActive() const { return CurrentState != ECombatCameraState::None; }

    // ==================== DEBUG ====================

    UFUNCTION(CallInEditor, Category = "Debug")
    void DebugPrintCameraState();

    UFUNCTION(CallInEditor, Category = "Debug")
    void DebugTransitionToHome();

    UFUNCTION(CallInEditor, Category = "Debug")
    void DebugTransitionToAction();

    /** Test Character camera on current turn actor */
    UFUNCTION(CallInEditor, Category = "Debug")
    void DebugTransitionToCharacter();

    /** Cycle through enemies with Selection camera */
    UFUNCTION(CallInEditor, Category = "Debug")
    void DebugCycleSelectionTarget();

    /** Transition to Team 0 Home camera */
    UFUNCTION(CallInEditor, Category = "Debug")
    void DebugTransitionToTeam0Home();

    /** Transition to Team 1 Home camera */
    UFUNCTION(CallInEditor, Category = "Debug")
    void DebugTransitionToTeam1Home();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    // ==================== INTERNAL ====================

    /** Dynamic camera used for Character/Selection/Action */
    UPROPERTY()
    ACameraActor *DynamicCamera;

    /** Reference to combat orchestrator */
    UPROPERTY()
    ACombatOrchestrator *CombatOrchestrator;

    /** Current actor being framed (Character/Selection states) */
    UPROPERTY()
    AActor *CurrentFocusActor;

    /** Current attacker (Action state) */
    UPROPERTY()
    AActor *CurrentAttacker;

    /** Current target (Action state) */
    UPROPERTY()
    AActor *CurrentTarget;

    /** Spawn or get the dynamic camera */
    ACameraActor *GetOrCreateDynamicCamera();

    /** Calculate camera transform for Character state */
    FTransform CalculateCharacterCameraTransform(AActor *Character) const;

    /** Calculate camera transform for Selection state */
    FTransform CalculateSelectionCameraTransform(AActor *Target) const;

    /** Calculate camera transform for Action state */
    FTransform CalculateActionCameraTransform(AActor *Attacker, AActor *Target) const;

    /** Blend to a camera with specified time */
    void BlendToCamera(ACameraActor *Camera, float BlendTime);

    /** Get the appropriate Home camera for local player */
    ACameraActor *GetLocalPlayerHomeCamera() const;

    // ==================== EVENT HANDLERS ====================

    /** Called when turn starts */
    UFUNCTION()
    void OnTurnStarted(AActor *Actor, int32 TurnNumber);

    /** Called when action starts executing */
    UFUNCTION()
    void OnActionStarted(AActor *Executor, const FAction &Action);

    /** Called when action completes */
    UFUNCTION()
    void OnActionCompleted(const FActionResult &Result);

    /** Called when movement phase completes */
    UFUNCTION()
    void OnMovementComplete();
};
