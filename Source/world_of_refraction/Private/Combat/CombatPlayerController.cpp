// CombatPlayerController.cpp

#include "Combat/CombatPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/EDefenseDirection.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"

namespace CombatMenuKeys
{
	/** Above the editor-authored combat context (priority 0), so a shared key goes to the menu. */
	constexpr int32 CONTEXT_PRIORITY = 1;

	struct FBinding
	{
		FName ActionName;
		EPieMenuCategory Category;
		FKey Keyboard;
		FKey Gamepad;
	};

	/** Q-093: fixed keys for now (D-063); each action name unique so rebinding can come later. */
	TArray<FBinding> GetBindings()
	{
		return {
			{TEXT("IA_Combat_MenuDefend"), EPieMenuCategory::Defend, EKeys::G, EKeys::Gamepad_FaceButton_Top},
			{TEXT("IA_Combat_MenuInfusionSource"), EPieMenuCategory::CycleSource, EKeys::R, EKeys::Gamepad_LeftShoulder},
			{TEXT("IA_Combat_MenuInfusionLevel"), EPieMenuCategory::CycleLevel, EKeys::T, EKeys::Gamepad_RightShoulder},
		};
	}

	FAutoConsoleCommand GPrintMenuKeysCmd(
		TEXT("wor.Input.MenuKeys"),
		TEXT("DEBUG: the combat menu keys (B-093): action, keyboard key, gamepad button."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UE_LOG(LogTemp, Display, TEXT("%s"), *ACombatPlayerController::GetMenuKeysString());
		}));
}

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
		BuildMenuKeys();
		Subsystem->AddMappingContext(MenuKeysContext, CombatMenuKeys::CONTEXT_PRIORITY);
	}
}

void ACombatPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Hygiene: the combat mapping context is added in BeginPlay onto the persistent
	// LocalPlayer subsystem; remove it here so it does not linger when leaving combat
	// (e.g. switching to the hub controller). Removes only its own context — additive.
	if (UEnhancedInputLocalPlayerSubsystem *Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (CombatMappingContext)
		{
			Subsystem->RemoveMappingContext(CombatMappingContext);
		}
		if (MenuKeysContext)
		{
			Subsystem->RemoveMappingContext(MenuKeysContext);
		}
	}

	Super::EndPlay(EndPlayReason);
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

	BuildMenuKeys();
	EnhancedInput->BindAction(DefendAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnDefend);
	EnhancedInput->BindAction(InfusionAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnInfusion);
	EnhancedInput->BindAction(InfusionLevelAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnInfusionLevel);

	UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Defense input bindings complete"));
}

// ========================================
// DEFENSE INPUT HANDLERS
// ========================================

// Defense input resolves the defender from the DefenseSystem (the player's open-window
// character), NOT GetPawn() — the PC possesses nothing in this combat model. Null defender
// (no window open) is a harmless no-op. SINGLE-TARGET seed; multi-target is Stage 6 (§13).
void ACombatPlayerController::OnBlock(const FInputActionValue &Value)
{
	if (UDefenseSystem *DefenseSys = GetDefenseSystem())
	{
		AActor *Defender = DefenseSys->GetActiveDefenderForLocalPlayer();
		UE_LOG(LogTemp, Log, TEXT("[CombatPC] Guard pressed — defender: %s"), *GetNameSafe(Defender));
		if (Defender)
		{
			DefenseSys->SubmitDefenseInput(Defender, EDefenseType::Block, EDefenseDirection::None);
		}
	}
}

void ACombatPlayerController::OnParry(const FInputActionValue &Value)
{
	if (UDefenseSystem *DefenseSys = GetDefenseSystem())
	{
		AActor *Defender = DefenseSys->GetActiveDefenderForLocalPlayer();
		UE_LOG(LogTemp, Log, TEXT("[CombatPC] Parry key pressed (a Guard, D-084) — defender: %s"), *GetNameSafe(Defender));
		if (Defender)
		{
			DefenseSys->SubmitDefenseInput(Defender, EDefenseType::Parry, EDefenseDirection::None);
		}
	}
}

void ACombatPlayerController::OnDodgeLeft(const FInputActionValue &Value)
{
	if (UDefenseSystem *DefenseSys = GetDefenseSystem())
	{
		AActor *Defender = DefenseSys->GetActiveDefenderForLocalPlayer();
		UE_LOG(LogTemp, Log, TEXT("[CombatPC] DodgeLeft pressed — defender: %s"), *GetNameSafe(Defender));
		if (Defender)
		{
			DefenseSys->SubmitDefenseInput(Defender, EDefenseType::Dodge, EDefenseDirection::Left);
		}
	}
}

void ACombatPlayerController::OnDodgeRight(const FInputActionValue &Value)
{
	if (UDefenseSystem *DefenseSys = GetDefenseSystem())
	{
		AActor *Defender = DefenseSys->GetActiveDefenderForLocalPlayer();
		UE_LOG(LogTemp, Log, TEXT("[CombatPC] DodgeRight pressed — defender: %s"), *GetNameSafe(Defender));
		if (Defender)
		{
			DefenseSys->SubmitDefenseInput(Defender, EDefenseType::Dodge, EDefenseDirection::Right);
		}
	}
}

// ========================================
// MENU KEYS (B-093)
// ========================================

void ACombatPlayerController::BuildMenuKeys()
{
	if (MenuKeysContext)
	{
		return;
	}
	MenuKeysContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Combat_MenuKeys"));
	for (const CombatMenuKeys::FBinding &Binding : CombatMenuKeys::GetBindings())
	{
		UInputAction *Action = NewObject<UInputAction>(this, Binding.ActionName);
		Action->ValueType = EInputActionValueType::Boolean;
		// Don't eat the key: the context stays mapped all fight, and a defense key sharing it
		// (priority 0) must still reach the defense window on an enemy's turn.
		Action->bConsumeInput = false;
		MenuKeysContext->MapKey(Action, Binding.Keyboard);
		MenuKeysContext->MapKey(Action, Binding.Gamepad);
		switch (Binding.Category)
		{
		case EPieMenuCategory::Defend:
			DefendAction = Action;
			break;
		case EPieMenuCategory::CycleSource:
			InfusionAction = Action;
			break;
		case EPieMenuCategory::CycleLevel:
			InfusionLevelAction = Action;
			break;
		default:
			checkNoEntry();
			break;
		}
	}
}

void ACombatPlayerController::OnDefend(const FInputActionValue &Value) { PressMenuKey(EPieMenuCategory::Defend); }

void ACombatPlayerController::OnInfusion(const FInputActionValue &Value) { PressMenuKey(EPieMenuCategory::CycleSource); }

void ACombatPlayerController::OnInfusionLevel(const FInputActionValue &Value) { PressMenuKey(EPieMenuCategory::CycleLevel); }

void ACombatPlayerController::PressMenuKey(EPieMenuCategory Category)
{
	if (!IsLocalController())
	{
		return;
	}
	UGameInstance *GI = GetGameInstance();
	if (UCombatCommandMenuSubsystem *Menu = GI ? GI->GetSubsystem<UCombatCommandMenuSubsystem>() : nullptr)
	{
		Menu->PressMenuKey(Category);
	}
}

FString ACombatPlayerController::GetMenuKeysString()
{
	FString Out = TEXT("[CombatPC] Menu keys");
	for (const CombatMenuKeys::FBinding &Binding : CombatMenuKeys::GetBindings())
	{
		Out += FString::Printf(TEXT("\n  %s (%s): %s / %s"), *Binding.ActionName.ToString(),
							   *UEnum::GetValueAsString(Binding.Category), *Binding.Keyboard.GetDisplayName().ToString(),
							   *Binding.Gamepad.GetDisplayName().ToString());
	}
	return Out;
}

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
