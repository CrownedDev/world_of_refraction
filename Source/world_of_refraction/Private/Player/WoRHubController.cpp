// WoRHubController.cpp

#include "Player/WoRHubController.h"

#include "Blueprint/UserWidget.h"
#include "Character/CombatCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Field/FieldMovementComponent.h"
#include "Hub/HubInteract.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/InputPriorities.h"
#include "UI/Hub/HubHUDWidget.h"
#include "UI/Menus/PauseMenuWidget.h"
#include "UI/Menus/TitleScreenWidget.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

namespace WoRHubControllerConstants
{
	const TCHAR *const LOG_TAG = TEXT("[WoRHubController]");

	/** Explicit, not defaulted: the existing HUD (party strip, combat panels) is
	 *  added to the viewport from Blueprint graphs at times we do not control, and
	 *  ties at ZOrder 0 resolve by insertion order. Naming the value keeps the
	 *  layering deterministic across level loads. Below the existing HUD for now —
	 *  the strip stays on top until it moves into GameLayer. */
	constexpr int32 ROOT_LAYOUT_ZORDER = -10;

	/** B-443: with the rest of the viewport HUD (the party strip, ZOrder 0), above the root layout; the layout
	 *  hides both while a menu screen is up. */
	constexpr int32 HUB_HUD_ZORDER = 0;

	/** B-503e: the code-made field actions keep the asset names (InputMap.md), so a later asset slots in. */
	const FName FIELD_ATTACK_ACTION_NAME = TEXT("IA_Field_Attack");
	const FName FIELD_SWITCH_SOURCE_ACTION_NAME = TEXT("IA_Field_SwitchSource");
	const FName FIELD_FLY_VERTICAL_ACTION_NAME = TEXT("IA_Field_FlyVertical");
	const FName FIELD_KEYS_CONTEXT_NAME = TEXT("IMC_Field_Keys");
}

void AWoRHubController::BuildCodeFieldActions()
{
	if (FieldKeysContext || (FieldAttackAction && FieldSwitchSourceAction && FieldFlyVerticalAction))
	{
		return;
	}
	UEnhancedInputLocalPlayerSubsystem *Mappings =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	using namespace WoRHubControllerConstants;
	if (!Mappings)
	{
		UE_LOG(LogWoR, Warning, TEXT("%s No Enhanced Input subsystem - field keys unbound"), LOG_TAG);
		return;
	}
	FieldKeysContext = NewObject<UInputMappingContext>(this, FIELD_KEYS_CONTEXT_NAME);
	const auto MakeAction = [this](FName Name, EInputActionValueType ValueType)
	{
		UInputAction *Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = ValueType;
		// Don't eat the key: Space is also Jump, and the BP's own context may map these keys.
		Action->bConsumeInput = false;
		return Action;
	};
	if (!FieldAttackAction)
	{
		FieldAttackAction = MakeAction(FIELD_ATTACK_ACTION_NAME, EInputActionValueType::Boolean);
		FieldKeysContext->MapKey(FieldAttackAction, EKeys::LeftMouseButton);
		FieldKeysContext->MapKey(FieldAttackAction, EKeys::Gamepad_RightShoulder);
	}
	if (!FieldSwitchSourceAction)
	{
		FieldSwitchSourceAction = MakeAction(FIELD_SWITCH_SOURCE_ACTION_NAME, EInputActionValueType::Boolean);
		FieldKeysContext->MapKey(FieldSwitchSourceAction, EKeys::R);
		FieldKeysContext->MapKey(FieldSwitchSourceAction, EKeys::Gamepad_LeftShoulder);
	}
	if (!FieldFlyVerticalAction)
	{
		FieldFlyVerticalAction = MakeAction(FIELD_FLY_VERTICAL_ACTION_NAME, EInputActionValueType::Axis1D);
		FieldKeysContext->MapKey(FieldFlyVerticalAction, EKeys::SpaceBar);
		FieldKeysContext->MapKey(FieldFlyVerticalAction, EKeys::Gamepad_FaceButton_Bottom);
		FieldKeysContext->MapKey(FieldFlyVerticalAction, EKeys::LeftControl)
			.Modifiers.Add(NewObject<UInputModifierNegate>(FieldKeysContext));
		FieldKeysContext->MapKey(FieldFlyVerticalAction, EKeys::Gamepad_RightThumbstick)
			.Modifiers.Add(NewObject<UInputModifierNegate>(FieldKeysContext));
	}
	Mappings->AddMappingContext(FieldKeysContext, WoRInputPriority::FIELD_KEYS);
	UE_LOG(LogWoR, Log, TEXT("%s Field keys made in code (no IA_Field_* assets): %d mappings in %s"), LOG_TAG,
		   FieldKeysContext->GetMappings().Num(), *FIELD_KEYS_CONTEXT_NAME.ToString());
}

void AWoRHubController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The local player outlives this controller: leave no stale field keys for the next level's controller.
	if (FieldKeysContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem *Mappings =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Mappings->RemoveMappingContext(FieldKeysContext);
		}
		FieldKeysContext = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AWoRHubController::BeginPlay()
{
	Super::BeginPlay();

	// B-443: the interact prompt and the money chip, for this machine's player only.
	if (IsLocalController())
	{
		HubHUD = CreateWidget<UHubHUDWidget>(this, UHubHUDWidget::StaticClass());
		if (HubHUD)
		{
			HubHUD->AddToViewport(WoRHubControllerConstants::HUB_HUD_ZORDER);
		}
	}

	// Root layout only. The Blueprint's BeginPlay runs after this and still owns
	// the mapping context and the WBP_PartyDisplay creation. Interact and
	// EnsureLocalParty live here since B-158 (SetupInputComponent / OnPossess).
	if (!RootLayoutClass)
	{
		UE_LOG(LogTemp, Log,
			   TEXT("[WoRHubController] RootLayoutClass unset - no CommonUI root layout created (this is a valid "
					"pre-F2b state)"));
		return;
	}

	RootLayout = CreateWidget<UWoRRootLayout>(this, RootLayoutClass);
	if (!RootLayout)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WoRHubController] CreateWidget failed for %s - no root layout"),
			   *RootLayoutClass->GetName());
		return;
	}

	RootLayout->AddToViewport(WoRHubControllerConstants::ROOT_LAYOUT_ZORDER);

	UE_LOG(LogTemp, Log, TEXT("[WoRHubController] Root layout %s created (mode %s, ZOrder %d)"),
		   *RootLayoutClass->GetName(), *UEnum::GetValueAsString(CurrentMode),
		   WoRHubControllerConstants::ROOT_LAYOUT_ZORDER);

	// B-467: the game opens on the title. Not in the editor's PIE (wor.UI.Title opens it there).
	if (!GIsEditor)
	{
		UTitleScreenWidget::OpenAtLaunch(this);
	}
}

void AWoRHubController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent *EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WoRHubController] InputComponent is not Enhanced Input - interact unbound"));
		return;
	}
	if (!InteractAction)
	{
		InteractAction = DefaultInteractAction.LoadSynchronous();
	}
	if (InteractAction)
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AWoRHubController::HandleInteractPressed);
	}
	else
	{
		UE_LOG(LogWoR, Error,
			   TEXT("[WoRHubController] No interact action: set DefaultInteractAction in Config/DefaultGame.ini "
					"(now '%s') or InteractAction on the controller BP (B-205)"),
			   *DefaultInteractAction.ToString());
	}

	if (!FieldMovementSkillAction && !DefaultFieldMovementSkillAction.IsNull())
	{
		FieldMovementSkillAction = DefaultFieldMovementSkillAction.LoadSynchronous();
	}
	if (FieldMovementSkillAction)
	{
		// The double tap is the asset's own trigger (B-214g), so one Triggered = one use.
		EnhancedInput->BindAction(FieldMovementSkillAction, ETriggerEvent::Triggered, this,
								  &AWoRHubController::HandleFieldMovementSkill);
	}
	else
	{
		UE_LOG(LogWoR, Log,
			   TEXT("[WoRHubController] No field movement skill action yet (B-214g makes IA_Field_MovementSkill); "
					"wor.Field.Activate stands in"));
	}

	// B-214g: the field attack, its source switch, and flying up / down (assets by config soft pointer, as above).
	const auto LoadFieldAction = [](TObjectPtr<UInputAction> &Action, const TSoftObjectPtr<UInputAction> &Default)
	{
		if (!Action && !Default.IsNull())
		{
			Action = Default.LoadSynchronous();
		}
		return Action != nullptr;
	};
	LoadFieldAction(FieldAttackAction, DefaultFieldAttackAction);
	LoadFieldAction(FieldSwitchSourceAction, DefaultFieldSwitchSourceAction);
	LoadFieldAction(FieldFlyVerticalAction, DefaultFieldFlyVerticalAction);
	BuildCodeFieldActions();
	if (FieldAttackAction)
	{
		EnhancedInput->BindAction(FieldAttackAction, ETriggerEvent::Started, this, &AWoRHubController::HandleFieldAttackStarted);
		EnhancedInput->BindAction(FieldAttackAction, ETriggerEvent::Completed, this, &AWoRHubController::HandleFieldAttackCompleted);
		EnhancedInput->BindAction(FieldAttackAction, ETriggerEvent::Canceled, this, &AWoRHubController::HandleFieldAttackCanceled);
	}
	if (FieldSwitchSourceAction)
	{
		EnhancedInput->BindAction(FieldSwitchSourceAction, ETriggerEvent::Started, this, &AWoRHubController::HandleFieldSwitchSourceStarted);
		EnhancedInput->BindAction(FieldSwitchSourceAction, ETriggerEvent::Completed, this, &AWoRHubController::HandleFieldSwitchSource);
		EnhancedInput->BindAction(FieldSwitchSourceAction, ETriggerEvent::Canceled, this, &AWoRHubController::HandleFieldSwitchSourceCanceled);
	}
	if (FieldFlyVerticalAction)
	{
		EnhancedInput->BindAction(FieldFlyVerticalAction, ETriggerEvent::Triggered, this, &AWoRHubController::HandleFieldFlyVertical);
	}

	if (!ToggleMenuAction && !DefaultToggleMenuAction.IsNull())
	{
		ToggleMenuAction = DefaultToggleMenuAction.LoadSynchronous();
	}
	if (ToggleMenuAction)
	{
		EnhancedInput->BindAction(ToggleMenuAction, ETriggerEvent::Started, this, &AWoRHubController::HandleToggleMenu);
	}
	else
	{
		UE_LOG(LogWoR, Warning,
			   TEXT("[WoRHubController] No menu action: set DefaultToggleMenuAction in Config/DefaultGame.ini (now '%s'); "
					"wor.UI.Pause stands in (B-439)"),
			   *DefaultToggleMenuAction.ToString());
	}
}

void AWoRHubController::HandleToggleMenu(const FInputActionValue & /*Value*/)
{
	OpenGameMenu();
}

void AWoRHubController::OpenGameMenu()
{
	// Opening only: with the menu up, game input is off, so the menu's own Back / Menu key closes it.
	UPauseMenuWidget::Open(this);
}

void AWoRHubController::OnPossess(APawn *InPawn)
{
	Super::OnPossess(InPawn);

	if (bEnsureLocalPartyOnPossess)
	{
		UGameInstance *GI = GetGameInstance();
		if (UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr)
		{
			PartySession->EnsureLocalParty(this, LocalPartyPawnClass);
		}
	}
	EnsureFieldMovement(InPawn); // after EnsureLocalParty, so the leader's record is bound
}

void AWoRHubController::EnsureFieldMovement(APawn *InPawn) const
{
	if (!InPawn)
	{
		return;
	}
	UFieldMovementComponent *FieldMovement = InPawn->FindComponentByClass<UFieldMovementComponent>();
	if (!FieldMovement)
	{
		FieldMovement = NewObject<UFieldMovementComponent>(InPawn, TEXT("FieldMovement"));
		InPawn->AddInstanceComponent(FieldMovement);
		FieldMovement->RegisterComponent();
		UE_LOG(LogWoR, Log, TEXT("[WoRHubController] Field movement added to %s (B-214a)"), *InPawn->GetName());
	}
	FieldMovement->RefreshFromRecord();
}

void AWoRHubController::HandleInteractPressed(const FInputActionValue & /*Value*/)
{
	InteractWithNearest();
}

void AWoRHubController::HandleFieldMovementSkill(const FInputActionValue & /*Value*/)
{
	if (UFieldMovementComponent *FieldMovement = GetFieldMovement())
	{
		FieldMovement->ActivateFieldSkill();
	}
}

UFieldMovementComponent *AWoRHubController::GetFieldMovement() const
{
	return GetPawn() ? GetPawn()->FindComponentByClass<UFieldMovementComponent>() : nullptr;
}

void AWoRHubController::HandleFieldAttackStarted(const FInputActionValue & /*Value*/)
{
	FieldAttackPressTime = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	bFieldAttackPressed = true;
}

void AWoRHubController::HandleFieldAttackCompleted(const FInputActionValue & /*Value*/)
{
	if (!bFieldAttackPressed)
	{
		return; // a release with no press of ours (focus loss, a menu took the mapping)
	}
	bFieldAttackPressed = false;
	if (UFieldMovementComponent *FieldMovement = GetFieldMovement())
	{
		const double Pressed = GetWorld() ? GetWorld()->GetRealTimeSeconds() - FieldAttackPressTime : 0.0;
		FieldMovement->FieldAttack(FieldSkillRules::IsFieldAttackHeld(Pressed));
	}
}

void AWoRHubController::HandleFieldAttackCanceled(const FInputActionValue & /*Value*/)
{
	bFieldAttackPressed = false;
}

void AWoRHubController::HandleFieldSwitchSourceStarted(const FInputActionValue & /*Value*/)
{
	FieldSourcePressTime = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	bFieldSourcePressed = true;
}

void AWoRHubController::HandleFieldSwitchSourceCanceled(const FInputActionValue & /*Value*/)
{
	bFieldSourcePressed = false;
}

void AWoRHubController::HandleFieldSwitchSource(const FInputActionValue & /*Value*/)
{
	if (!bFieldSourcePressed)
	{
		return; // a release with no press of ours (focus loss, a menu took the mapping)
	}
	bFieldSourcePressed = false;
	const double Pressed = GetWorld() ? GetWorld()->GetRealTimeSeconds() - FieldSourcePressTime : 0.0;
	if (UFieldMovementComponent *FieldMovement = GetFieldMovement())
	{
		// D-217: a press steps the field attack's source (plain Sword, Fire Sword, ...); a hold the element within it
		// (B-503l, Q-503l: Reality "(R) Fire", Broken Darkness "Dark Fire"; a one-element source stays). Flashed on the strip.
		const FFieldAttackEntry Entry = FieldMovement->CycleFieldAttack(FieldSkillRules::IsFieldSourceHeld(Pressed));
		if (HubHUD)
		{
			HubHUD->ShowFieldAttack(Entry);
		}
	}
}

void AWoRHubController::HandleFieldFlyVertical(const FInputActionValue &Value)
{
	if (UFieldMovementComponent *FieldMovement = GetFieldMovement())
	{
		FieldMovement->AddFlightVerticalInput(Value.Get<float>());
	}
}

void AWoRHubController::InteractWithNearest()
{
	HubInteract::InteractWithNearest(GetPawn(), WoRHubControllerConstants::LOG_TAG);
}

FString AWoRHubController::GetInteractionStateString() const
{
	return HubInteract::GetInteractionStateString(GetPawn(), WoRHubControllerConstants::LOG_TAG);
}

void AWoRHubController::SetHubMode(EHubMode NewMode)
{
	if (CurrentMode == NewMode)
	{
		return;
	}

	CurrentMode = NewMode;
	UE_LOG(LogTemp, Log, TEXT("[WoRHubController] Hub mode -> %s"), *UEnum::GetValueAsString(CurrentMode));
}

UCommonActivatableWidget *AWoRHubController::PushMenu(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (!RootLayout)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WoRHubController] PushMenu with no root layout - is RootLayoutClass set?"));
		return nullptr;
	}

	return RootLayout->PushToMenu(WidgetClass);
}

void AWoRHubController::PopMenu()
{
	if (!RootLayout)
	{
		return;
	}

	RootLayout->PopMenu();
}
