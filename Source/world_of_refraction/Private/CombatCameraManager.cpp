// CombatCameraManager.cpp

#include "CombatCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "CombatOrchestrator.h"
#include "TurnManager.h"

ACombatCameraManager::ACombatCameraManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    // Defaults
    Team0HomeCamera = nullptr;
    Team1HomeCamera = nullptr;
    DynamicCamera = nullptr;
    CombatOrchestrator = nullptr;
    CurrentFocusActor = nullptr;
    CurrentAttacker = nullptr;
    CurrentTarget = nullptr;
}

void ACombatCameraManager::BeginPlay()
{
    Super::BeginPlay();

    // Validate placed cameras
    if (!Team0HomeCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCameraManager] Team0HomeCamera not assigned!"));
    }
    if (!Team1HomeCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCameraManager] Team1HomeCamera not assigned!"));
    }
}

void ACombatCameraManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // During Approach phase, continuously update camera to follow movement
    if (CurrentState == ECombatCameraState::Action &&
        CurrentActionPhase == EActionCameraPhase::Approach &&
        CurrentAttacker && CurrentTarget)
    {
        UpdateActionCameraFollow(CurrentAttacker, CurrentTarget);
    }
}

// ==================== COMBAT INTEGRATION ====================

void ACombatCameraManager::InitializeForCombat(ACombatOrchestrator *Orchestrator)
{
    CombatOrchestrator = Orchestrator;

    if (!CombatOrchestrator)
    {
        UE_LOG(LogTemp, Error, TEXT("[CombatCameraManager] InitializeForCombat called with null Orchestrator"));
        return;
    }

    // Bind to TurnManager events
    UGameInstance *GI = GetGameInstance();
    if (GI)
    {
        UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
        if (TurnMgr)
        {
            TurnMgr->OnTurnStarted.AddDynamic(this, &ACombatCameraManager::OnTurnStarted);
        }
    }

    // TODO: Bind to ActionExecutor events when available
    // ActionExecutor->OnActionStarted.AddDynamic(this, &ACombatCameraManager::OnActionStarted);
    // ActionExecutor->OnActionCompleted.AddDynamic(this, &ACombatCameraManager::OnActionCompleted);

    // Enable tick for movement following
    SetActorTickEnabled(true);

    // Start at Home camera
    TransitionToHome();

    UE_LOG(LogTemp, Log, TEXT("[CombatCameraManager] Initialized for combat"));
}

void ACombatCameraManager::EndCombat()
{
    // Unbind events
    UGameInstance *GI = GetGameInstance();
    if (GI)
    {
        UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
        if (TurnMgr)
        {
            TurnMgr->OnTurnStarted.RemoveDynamic(this, &ACombatCameraManager::OnTurnStarted);
        }
    }

    SetActorTickEnabled(false);

    CurrentState = ECombatCameraState::None;
    CurrentActionPhase = EActionCameraPhase::None;
    CombatOrchestrator = nullptr;
    CurrentFocusActor = nullptr;
    CurrentAttacker = nullptr;
    CurrentTarget = nullptr;

    UE_LOG(LogTemp, Log, TEXT("[CombatCameraManager] Combat ended"));
}

// ==================== STATE TRANSITIONS ====================

void ACombatCameraManager::TransitionToHome()
{
    ACameraActor *HomeCamera = GetLocalPlayerHomeCamera();
    if (!HomeCamera)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCameraManager] No Home camera available"));
        return;
    }

    float BlendTime = (CurrentState == ECombatCameraState::Action)
                          ? ActionToHomeBlendTime
                          : HomeToCharacterBlendTime;

    BlendToCamera(HomeCamera, BlendTime);

    CurrentState = ECombatCameraState::Home;
    CurrentActionPhase = EActionCameraPhase::None;
    CurrentFocusActor = nullptr;

    UE_LOG(LogTemp, Log, TEXT("[CombatCameraManager] Transitioned to Home"));
}

void ACombatCameraManager::TransitionToCharacter(AActor *Character)
{
    if (!Character)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCameraManager] TransitionToCharacter called with null Character"));
        return;
    }

    ACameraActor *Camera = GetOrCreateDynamicCamera();
    if (!Camera)
    {
        return;
    }

    // Position camera for character close-up
    FTransform CameraTransform = CalculateCharacterCameraTransform(Character);
    Camera->SetActorTransform(CameraTransform);

    BlendToCamera(Camera, HomeToCharacterBlendTime);

    CurrentState = ECombatCameraState::Character;
    CurrentActionPhase = EActionCameraPhase::None;
    CurrentFocusActor = Character;

    UE_LOG(LogTemp, Log, TEXT("[CombatCameraManager] Transitioned to Character: %s"), *Character->GetName());
}

void ACombatCameraManager::TransitionToSelection(AActor *Target)
{
    if (!Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCameraManager] TransitionToSelection called with null Target"));
        return;
    }

    ACameraActor *Camera = GetOrCreateDynamicCamera();
    if (!Camera)
    {
        return;
    }

    // Position camera for target view
    FTransform CameraTransform = CalculateSelectionCameraTransform(Target);
    Camera->SetActorTransform(CameraTransform);

    BlendToCamera(Camera, CharacterToSelectionBlendTime);

    CurrentState = ECombatCameraState::Selection;
    CurrentActionPhase = EActionCameraPhase::None;
    CurrentFocusActor = Target;

    UE_LOG(LogTemp, Log, TEXT("[CombatCameraManager] Transitioned to Selection: %s"), *Target->GetName());
}

void ACombatCameraManager::TransitionToAction(AActor *Attacker, AActor *Target)
{
    if (!Attacker || !Target)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCameraManager] TransitionToAction called with null actors"));
        return;
    }

    ACameraActor *Camera = GetOrCreateDynamicCamera();
    if (!Camera)
    {
        return;
    }

    CurrentAttacker = Attacker;
    CurrentTarget = Target;

    // Position camera to frame both actors
    FTransform CameraTransform = CalculateActionCameraTransform(Attacker, Target);
    Camera->SetActorTransform(CameraTransform);

    BlendToCamera(Camera, SelectionToActionBlendTime);

    CurrentState = ECombatCameraState::Action;
    CurrentActionPhase = EActionCameraPhase::Execution; // Default to execution framing

    UE_LOG(LogTemp, Log, TEXT("[CombatCameraManager] Transitioned to Action: %s -> %s"),
           *Attacker->GetName(), *Target->GetName());
}

// ==================== ACTION PHASE CONTROL ====================

void ACombatCameraManager::SetActionPhase(EActionCameraPhase Phase)
{
    if (CurrentState != ECombatCameraState::Action)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCameraManager] SetActionPhase called but not in Action state"));
        return;
    }

    EActionCameraPhase PreviousPhase = CurrentActionPhase;
    CurrentActionPhase = Phase;

    // Adjust camera based on phase
    ACameraActor *Camera = DynamicCamera;
    if (!Camera || !CurrentAttacker || !CurrentTarget)
    {
        return;
    }

    switch (Phase)
    {
    case EActionCameraPhase::Approach:
        // Enable tick to follow movement
        SetActorTickEnabled(true);
        break;

    case EActionCameraPhase::Cast:
        // Focus on caster
        {
            FTransform CasterTransform = CalculateCharacterCameraTransform(CurrentAttacker);
            Camera->SetActorTransform(CasterTransform);
        }
        break;

    case EActionCameraPhase::Execution:
    case EActionCameraPhase::Defense:
    case EActionCameraPhase::Result:
        // Wide shot framing both actors
        {
            FTransform ActionTransform = CalculateActionCameraTransform(CurrentAttacker, CurrentTarget);
            Camera->SetActorTransform(ActionTransform);
        }
        break;

    default:
        break;
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatCameraManager] Action phase: %d -> %d"),
           (int32)PreviousPhase, (int32)Phase);
}

void ACombatCameraManager::UpdateActionCameraFollow(AActor *MovingActor, AActor *Target)
{
    if (!DynamicCamera || !MovingActor || !Target)
    {
        return;
    }

    // Smoothly update camera to keep both actors in frame
    FTransform DesiredTransform = CalculateActionCameraTransform(MovingActor, Target);

    // Interpolate for smooth following
    FVector CurrentLocation = DynamicCamera->GetActorLocation();
    FVector DesiredLocation = DesiredTransform.GetLocation();
    FVector NewLocation = FMath::VInterpTo(CurrentLocation, DesiredLocation, GetWorld()->GetDeltaSeconds(), 5.0f);

    FRotator CurrentRotation = DynamicCamera->GetActorRotation();
    FRotator DesiredRotation = DesiredTransform.GetRotation().Rotator();
    FRotator NewRotation = FMath::RInterpTo(CurrentRotation, DesiredRotation, GetWorld()->GetDeltaSeconds(), 5.0f);

    DynamicCamera->SetActorLocationAndRotation(NewLocation, NewRotation);
}

// ==================== CAMERA POSITIONING ====================

FTransform ACombatCameraManager::CalculateCharacterCameraTransform(AActor *Character) const
{
    if (!Character)
    {
        return FTransform::Identity;
    }

    FVector CharacterLocation = Character->GetActorLocation();
    FVector CharacterForward = Character->GetActorForwardVector();

    // Position camera in front of character, slightly to the side
    FVector CameraLocation = CharacterLocation - CharacterForward * CharacterCameraDistance + FVector(0, 0, CharacterCameraHeightOffset);

    // Look at character's head area
    FVector LookAtPoint = CharacterLocation + FVector(0, 0, 80.0f);
    FRotator CameraRotation = (LookAtPoint - CameraLocation).Rotation();

    return FTransform(CameraRotation, CameraLocation);
}

FTransform ACombatCameraManager::CalculateSelectionCameraTransform(AActor *Target) const
{
    if (!Target)
    {
        return FTransform::Identity;
    }

    // Similar to character camera but framing the target
    FVector TargetLocation = Target->GetActorLocation();
    FVector TargetForward = Target->GetActorForwardVector();

    FVector CameraLocation = TargetLocation - TargetForward * CharacterCameraDistance + FVector(0, 0, CharacterCameraHeightOffset);

    FVector LookAtPoint = TargetLocation + FVector(0, 0, 80.0f);
    FRotator CameraRotation = (LookAtPoint - CameraLocation).Rotation();

    return FTransform(CameraRotation, CameraLocation);
}

FTransform ACombatCameraManager::CalculateActionCameraTransform(AActor *Attacker, AActor *Target) const
{
    if (!Attacker || !Target)
    {
        return FTransform::Identity;
    }

    FVector AttackerLocation = Attacker->GetActorLocation();
    FVector TargetLocation = Target->GetActorLocation();

    // Midpoint between actors
    FVector Midpoint = (AttackerLocation + TargetLocation) / 2.0f;

    // Distance between actors
    float ActorDistance = FVector::Dist(AttackerLocation, TargetLocation);

    // Camera pulls back based on distance
    float PullbackDistance = FMath::Max(ActorDistance * ActionCameraDistanceMultiplier, 400.0f);

    // Direction perpendicular to the line between actors (side view)
    FVector LineDirection = (TargetLocation - AttackerLocation).GetSafeNormal();
    FVector SideDirection = FVector::CrossProduct(LineDirection, FVector::UpVector).GetSafeNormal();

    // Camera position: to the side and above
    FVector CameraLocation = Midpoint + SideDirection * PullbackDistance + FVector(0, 0, ActionCameraHeightOffset);

    // Look at midpoint
    FRotator CameraRotation = (Midpoint - CameraLocation).Rotation();

    return FTransform(CameraRotation, CameraLocation);
}

// ==================== INTERNAL ====================

ACameraActor *ACombatCameraManager::GetOrCreateDynamicCamera()
{
    if (DynamicCamera)
    {
        return DynamicCamera;
    }

    // Spawn a camera actor for dynamic positioning
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    DynamicCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SpawnParams);

    if (DynamicCamera)
    {
        // Configure camera settings
        UCameraComponent *CameraComp = DynamicCamera->GetCameraComponent();
        if (CameraComp)
        {
            CameraComp->SetFieldOfView(70.0f);
        }

        UE_LOG(LogTemp, Log, TEXT("[CombatCameraManager] Created dynamic camera"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[CombatCameraManager] Failed to spawn dynamic camera"));
    }

    return DynamicCamera;
}

void ACombatCameraManager::BlendToCamera(ACameraActor *Camera, float BlendTime)
{
    if (!Camera)
    {
        return;
    }

    APlayerController *PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC)
    {
        PC->SetViewTargetWithBlend(Camera, BlendTime, EViewTargetBlendFunction::VTBlend_EaseInOut);
    }
}

ACameraActor *ACombatCameraManager::GetLocalPlayerHomeCamera() const
{
    return (LocalPlayerTeam == 0) ? Team0HomeCamera : Team1HomeCamera;
}

// ==================== EVENT HANDLERS ====================

void ACombatCameraManager::OnTurnStarted(AActor *Actor, int32 TurnNumber)
{
    if (!Actor)
    {
        return;
    }

    // Check if this is the local player's team
    // For now, assume Team 0 is player
    UGameInstance *GI = GetGameInstance();
    if (!GI)
        return;

    UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
    if (!TurnMgr)
        return;

    int32 ActorTeam = TurnMgr->GetActorTeam(Actor);

    if (ActorTeam == LocalPlayerTeam)
    {
        // Your team's turn - show Character camera
        TransitionToCharacter(Actor);
    }
    else
    {
        // Enemy turn - stay on Home (or already on Home)
        if (CurrentState != ECombatCameraState::Home)
        {
            TransitionToHome();
        }
    }
}

void ACombatCameraManager::OnActionStarted(AActor *Executor, const FAction &Action)
{
    // TODO: Get target from action and transition to Action camera
    // TransitionToAction(Executor, Target);
}

void ACombatCameraManager::OnActionCompleted(const FActionResult &Result)
{
    // Return to Home after action
    TransitionToHome();
}

void ACombatCameraManager::OnMovementComplete()
{
    // Approach phase done, switch to Execution
    if (CurrentState == ECombatCameraState::Action &&
        CurrentActionPhase == EActionCameraPhase::Approach)
    {
        SetActionPhase(EActionCameraPhase::Execution);
    }
}

// ==================== DEBUG ====================

void ACombatCameraManager::DebugPrintCameraState()
{
    FString StateName;
    switch (CurrentState)
    {
    case ECombatCameraState::None:
        StateName = TEXT("None");
        break;
    case ECombatCameraState::Home:
        StateName = TEXT("Home");
        break;
    case ECombatCameraState::Character:
        StateName = TEXT("Character");
        break;
    case ECombatCameraState::Selection:
        StateName = TEXT("Selection");
        break;
    case ECombatCameraState::Action:
        StateName = TEXT("Action");
        break;
    }

    FString PhaseName;
    switch (CurrentActionPhase)
    {
    case EActionCameraPhase::None:
        PhaseName = TEXT("None");
        break;
    case EActionCameraPhase::Approach:
        PhaseName = TEXT("Approach");
        break;
    case EActionCameraPhase::Cast:
        PhaseName = TEXT("Cast");
        break;
    case EActionCameraPhase::Execution:
        PhaseName = TEXT("Execution");
        break;
    case EActionCameraPhase::Defense:
        PhaseName = TEXT("Defense");
        break;
    case EActionCameraPhase::Result:
        PhaseName = TEXT("Result");
        break;
    }

    UE_LOG(LogTemp, Display, TEXT("[CombatCameraManager] State: %s | Phase: %s"), *StateName, *PhaseName);
    UE_LOG(LogTemp, Display, TEXT("  Team0Home: %s"), Team0HomeCamera ? *Team0HomeCamera->GetName() : TEXT("NULL"));
    UE_LOG(LogTemp, Display, TEXT("  Team1Home: %s"), Team1HomeCamera ? *Team1HomeCamera->GetName() : TEXT("NULL"));
    UE_LOG(LogTemp, Display, TEXT("  DynamicCamera: %s"), DynamicCamera ? TEXT("Created") : TEXT("NULL"));
    UE_LOG(LogTemp, Display, TEXT("  LocalPlayerTeam: %d"), LocalPlayerTeam);
}

void ACombatCameraManager::DebugTransitionToHome()
{
    TransitionToHome();
}

void ACombatCameraManager::DebugTransitionToAction()
{
    // Find any two combatants for testing
    UGameInstance *GI = GetGameInstance();
    if (!GI)
        return;

    UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
    if (!TurnMgr)
        return;

    TArray<AActor *> Team0 = TurnMgr->GetTeamMembers(0);
    TArray<AActor *> Team1 = TurnMgr->GetTeamMembers(1);

    if (Team0.Num() > 0 && Team1.Num() > 0)
    {
        TransitionToAction(Team0[0], Team1[0]);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCameraManager] Debug: No combatants found for test"));
    }
}

void ACombatCameraManager::DebugTransitionToCharacter()
{
    UGameInstance *GI = GetGameInstance();
    if (!GI)
        return;

    UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
    if (!TurnMgr)
        return;

    AActor *CurrentActor = TurnMgr->GetCurrentActor();
    if (CurrentActor)
    {
        TransitionToCharacter(CurrentActor);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCameraManager] No current actor"));
    }
}

void ACombatCameraManager::DebugCycleSelectionTarget()
{
    UGameInstance *GI = GetGameInstance();
    if (!GI)
        return;

    UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
    if (!TurnMgr)
        return;

    TArray<AActor *> Enemies = TurnMgr->GetTeamMembers(1);
    if (Enemies.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatCameraManager] No enemies found"));
        return;
    }

    static int32 CurrentIndex = -1;
    CurrentIndex = (CurrentIndex + 1) % Enemies.Num();

    TransitionToSelection(Enemies[CurrentIndex]);

    UE_LOG(LogTemp, Log, TEXT("[CombatCameraManager] Selection target: %s (%d/%d)"),
           *Enemies[CurrentIndex]->GetName(), CurrentIndex + 1, Enemies.Num());
}