// CombatPlayerController.cpp

#include "Testing/CombatPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Infusion/InfusionChargeManager.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Combat/Actions/ActionStructs.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ACombatPlayerController::ACombatPlayerController()
{
}

void ACombatPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Add Input Mapping Context
    if (UEnhancedInputLocalPlayerSubsystem *Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (CombatMappingContext)
        {
            Subsystem->AddMappingContext(CombatMappingContext, 0);
            UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Added Combat Mapping Context"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatPlayerController] CombatMappingContext not assigned!"));
        }
    }

    DebugPrintState();
}

void ACombatPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent *EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
    {
        // Charge Infusion - Started, Triggered (held), Completed (released)
        if (ChargeInfusionAction)
        {
            EnhancedInput->BindAction(ChargeInfusionAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnChargeStarted);
            EnhancedInput->BindAction(ChargeInfusionAction, ETriggerEvent::Triggered, this, &ACombatPlayerController::OnChargeTriggered);
            EnhancedInput->BindAction(ChargeInfusionAction, ETriggerEvent::Completed, this, &ACombatPlayerController::OnChargeCompleted);
        }

        // Cycle Source
        if (CycleSourceAction)
        {
            EnhancedInput->BindAction(CycleSourceAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnCycleSource);
        }

        // Confirm Action
        if (ConfirmActionAction)
        {
            EnhancedInput->BindAction(ConfirmActionAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnConfirmAction);
        }

        // Cancel Action
        if (CancelActionAction)
        {
            EnhancedInput->BindAction(CancelActionAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnCancelAction);
        }

        UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Input bindings complete"));
    }
}

// ========================================
// INPUT HANDLERS
// ========================================

void ACombatPlayerController::OnChargeStarted(const FInputActionValue &Value)
{
    AActor *ControlledActor = GetControlledActor();
    if (!ControlledActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatPlayerController] No controlled actor!"));
        return;
    }

    UInfusionChargeManager *ChargeManager = GetChargeManager();
    if (!ChargeManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatPlayerController] No ChargeManager!"));
        return;
    }

    // Start charging
    bool bSuccess = ChargeManager->BeginCharge(ControlledActor, TestChargeType, CurrentSource);

    if (bSuccess)
    {
        bIsCharging = true;
        UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Charge STARTED - Type: %s, Source: %s"),
               *UEnum::GetValueAsString(TestChargeType),
               *UEnum::GetValueAsString(CurrentSource));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatPlayerController] Failed to start charge"));
    }
}

void ACombatPlayerController::OnChargeTriggered(const FInputActionValue &Value)
{
    // This is called every frame while held
    // InfusionChargeManager handles timing internally via timer
    // We could add visual feedback here later
}

void ACombatPlayerController::OnChargeCompleted(const FInputActionValue &Value)
{
    if (!bIsCharging)
    {
        return;
    }

    UInfusionChargeManager *ChargeManager = GetChargeManager();
    if (!ChargeManager)
    {
        return;
    }

    // Complete the charge and get final level
    int32 FinalLevel = ChargeManager->CompleteCharge();
    StoredChargeLevel = FinalLevel;
    bIsCharging = false;

    UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Charge COMPLETED - Final Level: L%d"), FinalLevel);

    DebugPrintState();
}

void ACombatPlayerController::OnCycleSource(const FInputActionValue &Value)
{
    // Don't allow source change while charging
    if (bIsCharging)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatPlayerController] Cannot change source while charging!"));
        return;
    }

    CurrentSource = GetNextSource(CurrentSource);

    UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Source changed to: %s"),
           *UEnum::GetValueAsString(CurrentSource));

    DebugPrintState();
}

void ACombatPlayerController::OnConfirmAction(const FInputActionValue &Value)
{
    AActor *ControlledActor = GetControlledActor();
    if (!ControlledActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatPlayerController] No controlled actor!"));
        return;
    }

    UActionExecutor *Executor = GetActionExecutor();
    if (!Executor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatPlayerController] No ActionExecutor!"));
        return;
    }

    UInfusionChargeManager *ChargeManager = GetChargeManager();
    int32 ChargeLevel = ChargeManager ? ChargeManager->GetCurrentChargeLevel() : 0;

    // Build action based on TestChargeType
    FAction Action;
    if (TestChargeType == EChargeInfusionType::Spell && TestSpell)
    {
        Action.ActionType = EActionType::Spell;
        Action.SpellData = TestSpell;
        Action.SpellInfusionLevel = StoredChargeLevel;
        Action.SelectedSource = CurrentSource;

        // Add target
        if (TestTarget)
        {
            Action.Targets.Add(TestTarget);
        }
        else
        {
            // Fallback: target self for testing VFX
            Action.Targets.Add(GetPawn());
            UE_LOG(LogTemp, Warning, TEXT("[CombatPlayerController] No TestTarget set - targeting self for VFX test"));
        }

        UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Executing SPELL: %s (L%d, Source: %s)"),
               *TestSpell->GetName(), StoredChargeLevel, *UEnum::GetValueAsString(CurrentSource));
    }
    else if (TestChargeType == EChargeInfusionType::Ability && TestAbility)
    {
        Action.ActionType = EActionType::Ability;
        Action.AbilityData = TestAbility;
        Action.AbilityInfusionLevel = StoredChargeLevel;
        Action.SelectedSource = CurrentSource;
        Action.Targets.Add(ControlledActor); // Target self for testing

        UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Executing ABILITY: %s (L%d, Source: %s)"),
               *TestAbility->GetName(), StoredChargeLevel, *UEnum::GetValueAsString(CurrentSource));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatPlayerController] No test spell/ability assigned for type %s"),
               *UEnum::GetValueAsString(TestChargeType));
        return;
    }

    // Execute! Async path (defense windows, multi-hit, status buildup, ApplyHit).
    // Fire-and-forget — the result is logged via the completion lambda for
    // parity with the pre-D-prep sync-result log; no controller state depends
    // on the action having completed. DebugPrintState() and the StoredChargeLevel
    // reset below describe controller input state, not action outcome.
    Executor->ExecuteActionAsync(ControlledActor, Action,
        FOnActionComplete::CreateLambda([](const FActionResult &Result)
        {
            UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Action Result: %s - Damage: %d, Energy: %d"),
                   Result.bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
                   Result.TotalDamageDealt,
                   Result.EnergySpent);
        }));

    DebugPrintState();

    StoredChargeLevel = 0;
}

void ACombatPlayerController::OnCancelAction(const FInputActionValue &Value)
{
    if (!bIsCharging)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Nothing to cancel"));
        return;
    }

    UInfusionChargeManager *ChargeManager = GetChargeManager();
    if (ChargeManager)
    {
        ChargeManager->CancelCharge();
        bIsCharging = false;

        UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Charge CANCELLED (HP cost not refunded)"));
    }

    DebugPrintState();
}

// ========================================
// HELPERS
// ========================================

AActor *ACombatPlayerController::GetControlledActor() const
{
    return GetPawn();
}

UInfusionChargeManager *ACombatPlayerController::GetChargeManager() const
{
    UGameInstance *GI = GetGameInstance();
    if (GI)
    {
        return GI->GetSubsystem<UInfusionChargeManager>();
    }
    return nullptr;
}

UActionExecutor *ACombatPlayerController::GetActionExecutor() const
{
    UGameInstance *GI = GetGameInstance();
    if (GI)
    {
        return GI->GetSubsystem<UActionExecutor>();
    }
    return nullptr;
}

EInfusionSourceOption ACombatPlayerController::GetNextSource(EInfusionSourceOption Current) const
{
    switch (Current)
    {
    case EInfusionSourceOption::None:
        return EInfusionSourceOption::Innate;
    case EInfusionSourceOption::Innate:
        return EInfusionSourceOption::ActiveRing;
    case EInfusionSourceOption::ActiveRing:
        return EInfusionSourceOption::WeaponCrystal;
    case EInfusionSourceOption::WeaponCrystal:
        return EInfusionSourceOption::Evolution;
    case EInfusionSourceOption::Evolution:
        return EInfusionSourceOption::None;
    default:
        return EInfusionSourceOption::None;
    }
}

void ACombatPlayerController::DebugPrintState() const
{
    if (GEngine)
    {
        FString StateStr = FString::Printf(
            TEXT("=== COMBAT STATE ===\nCharge Type: %s\nSource: %s\nCharging: %s"),
            *UEnum::GetValueAsString(TestChargeType),
            *UEnum::GetValueAsString(CurrentSource),
            bIsCharging ? TEXT("YES") : TEXT("NO"));

        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, StateStr);
    }
}