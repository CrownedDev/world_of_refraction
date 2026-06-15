// CombatPlayerController.cpp

#include "Combat/CombatPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/EDefenseDirection.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

void ACombatPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Add the combat input mapping context at priority 0.
	if (UEnhancedInputLocalPlayerSubsystem *Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
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
}

void ACombatPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent *EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	// Defense input — block / parry / dodge, each on press (Started). Routes straight
	// to DefenseSystem::SubmitDefenseInput; no gating here — the subsystem ignores
	// input when no defense window is open, so out-of-window presses are harmless.
	if (BlockAction)
	{
		EnhancedInput->BindAction(BlockAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnBlock);
	}
	if (ParryAction)
	{
		EnhancedInput->BindAction(ParryAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnParry);
	}
	if (DodgeLeftAction)
	{
		EnhancedInput->BindAction(DodgeLeftAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnDodgeLeft);
	}
	if (DodgeRightAction)
	{
		EnhancedInput->BindAction(DodgeRightAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnDodgeRight);
	}

	// TODO: Infusion bindings (commented until the infusion input mechanic is decided):
	// if (InfusionAction)      EnhancedInput->BindAction(InfusionAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnInfusion);
	// if (InfusionLevelAction) EnhancedInput->BindAction(InfusionLevelAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnInfusionLevel);

	UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Defense input bindings complete"));
}

// ========================================
// DEFENSE INPUT HANDLERS
// ========================================

void ACombatPlayerController::OnBlock(const FInputActionValue &Value)
{
	if (UDefenseSystem *DefenseSys = GetDefenseSystem())
	{
		DefenseSys->SubmitDefenseInput(GetPawn(), EDefenseType::Block, EDefenseDirection::None);
	}
}

void ACombatPlayerController::OnParry(const FInputActionValue &Value)
{
	if (UDefenseSystem *DefenseSys = GetDefenseSystem())
	{
		DefenseSys->SubmitDefenseInput(GetPawn(), EDefenseType::Parry, EDefenseDirection::None);
	}
}

void ACombatPlayerController::OnDodgeLeft(const FInputActionValue &Value)
{
	if (UDefenseSystem *DefenseSys = GetDefenseSystem())
	{
		DefenseSys->SubmitDefenseInput(GetPawn(), EDefenseType::Dodge, EDefenseDirection::Left);
	}
}

void ACombatPlayerController::OnDodgeRight(const FInputActionValue &Value)
{
	if (UDefenseSystem *DefenseSys = GetDefenseSystem())
	{
		DefenseSys->SubmitDefenseInput(GetPawn(), EDefenseType::Dodge, EDefenseDirection::Right);
	}
}

// TODO: Infusion handlers — implement when the infusion input mechanic is decided.
// The live command menu drives infusion level via UInfusionVFXComponent::CycleInfusionLevel;
// the controller path may hold-to-charge (UInfusionChargeManager) or cycle. Left unimplemented.
// void ACombatPlayerController::OnInfusion(const FInputActionValue &Value) { }
// void ACombatPlayerController::OnInfusionLevel(const FInputActionValue &Value) { }

// ========================================
// HELPERS
// ========================================

UDefenseSystem *ACombatPlayerController::GetDefenseSystem() const
{
	if (UGameInstance *GI = GetGameInstance())
	{
		return GI->GetSubsystem<UDefenseSystem>();
	}
	return nullptr;
}
