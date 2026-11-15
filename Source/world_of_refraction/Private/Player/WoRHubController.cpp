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
	/** B-503m3, B-503m4: the wheel's keys, always made in code (InputMap.md row 18). */
	const FName FIELD_WHEEL_PICK_ACTION_NAME = TEXT("IA_Field_WheelPick");
	const FName FIELD_WHEEL_MOUSE_ACTION_NAME = TEXT("IA_Field_WheelMouse");
	const FName FIELD_WHEEL_ELEMENT_ACTION_NAME = TEXT("IA_Field_WheelElement");
	const FName FIELD_WHEEL_SCROLL_ACTION_NAME = TEXT("IA_Field_WheelScroll");
	const FName FIELD_WHEEL_CONTEXT_NAME = TEXT("IMC_Field_Wheel");
}

void AWoRHubController::BuildFieldWheelKeys()
{
	if (FieldWheelContext)
	{
		return;
	}
	using namespace WoRHubControllerConstants;
	FieldWheelContext = NewObject<UInputMappingContext>(this, FIELD_WHEEL_CONTEXT_NAME);
	// B-503m4 (D-217): the actions eat their keys (the default), so the character stands and the camera holds still.
	FieldWheelPickAction = NewObject<UInputAction>(this, FIELD_WHEEL_PICK_ACTION_NAME);
	FieldWheelPickAction->ValueType = EInputActionValueType::Axis2D;
	FieldWheelContext->MapKey(FieldWheelPickAction, EKeys::Gamepad_Left2D);
	FieldWheelMouseAction = NewObject<UInputAction>(this, FIELD_WHEEL_MOUSE_ACTION_NAME);
	FieldWheelMouseAction->ValueType = EInputActionValueType::Axis2D;
	FieldWheelContext->MapKey(FieldWheelMouseAction, EKeys::Mouse2D);
	FieldWheelElementAction = NewObject<UInputAction>(this, FIELD_WHEEL_ELEMENT_ACTION_NAME);
	FieldWheelElementAction->ValueType = EInputActionValueType::Axis1D;
	FieldWheelContext->MapKey(FieldWheelElementAction, EKeys::Gamepad_DPad_Right);
	FieldWheelContext->MapKey(FieldWheelElementAction, EKeys::D);
	FieldWheelContext->MapKey(FieldWheelElementAction, EKeys::Gamepad_DPad_Left).Modifiers.Add(NewObject<UInputModifierNegate>(FieldWheelContext));
	FieldWheelContext->MapKey(FieldWheelElementAction, EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(FieldWheelContext));
	FieldWheelScrollAction = NewObject<UInputAction>(this, FIELD_WHEEL_SCROLL_ACTION_NAME);
	FieldWheelScrollAction->ValueType = EInputActionValueType::Axis1D;
	FieldWheelContext->MapKey(FieldWheelScrollAction, EKeys::MouseWheelAxis);
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
		// B-503m4 (D-217 "RB, tap and hold"): RB is both: a tap attacks, a hold opens the wheel. LB is off the field keys.
		FieldKeysContext->MapKey(FieldSwitchSourceAction, EKeys::R);
		FieldKeysContext->MapKey(FieldSwitchSourceAction, EKeys::Gamepad_RightShoulder);
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
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(FieldWheelOpenTimer);
	}
	CloseFieldWheel();
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
		BuildFieldWheelKeys();
		EnhancedInput->BindAction(FieldWheelPickAction, ETriggerEvent::Triggered, this, &AWoRHubController::HandleFieldWheelPick);
		EnhancedInput->BindAction(FieldWheelMouseAction, ETriggerEvent::Triggered, this, &AWoRHubController::HandleFieldWheelMouse);
		EnhancedInput->BindAction(FieldWheelElementAction, ETriggerEvent::Started, this, &AWoRHubController::HandleFieldWheelElement);
		EnhancedInput->BindAction(FieldWheelScrollAction, ETriggerEvent::Triggered, this, &AWoRHubController::HandleFieldWheelScroll);
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
		FieldMovement->FieldAttack(/*bHeld=*/false); // B-503m4 (D-217 "Ice"): Ice is the Water entry's option, not a hold
	}
}

void AWoRHubController::HandleFieldAttackCanceled(const FInputActionValue & /*Value*/)
{
	bFieldAttackPressed = false;
}

void AWoRHubController::HandleFieldSwitchSourceStarted(const FInputActionValue & /*Value*/)
{
	bFieldSourcePressed = true;
	// B-503m3 (D-217): held this long, the wheel opens while the key is still down.
	GetWorldTimerManager().SetTimer(FieldWheelOpenTimer, this, &AWoRHubController::OpenFieldWheel,
									static_cast<float>(FieldSkillRules::FIELD_SOURCE_HOLD_SECONDS), /*bLoop=*/false);
}

void AWoRHubController::HandleFieldSwitchSourceCanceled(const FInputActionValue & /*Value*/)
{
	bFieldSourcePressed = false;
	GetWorldTimerManager().ClearTimer(FieldWheelOpenTimer);
	CloseFieldWheel(); // focus loss or a menu took the key: nothing picked
}

void AWoRHubController::HandleFieldSwitchSource(const FInputActionValue & /*Value*/)
{
	if (!bFieldSourcePressed)
	{
		return; // a release with no press of ours (focus loss, a menu took the mapping)
	}
	bFieldSourcePressed = false;
	GetWorldTimerManager().ClearTimer(FieldWheelOpenTimer);
	UFieldMovementComponent *FieldMovement = GetFieldMovement();
	const int32 Picked = FieldWheelPick;
	const bool bFromWheel = IsFieldWheelOpen();
	CloseFieldWheel();
	if (!FieldMovement)
	{
		return;
	}
	// B-503m4 (D-217): letting go of the wheel confirms its pick (an element option too: "Ice", "(R) Fire",
	// "Dark Fire"). A tap changes nothing: RB's tap is the attack (IA_Field_Attack), R's is nothing.
	if (!bFromWheel)
	{
		return;
	}
	const FFieldAttackEntry Entry = FieldMovement->SelectFieldAttack(Picked);
	UE_LOG(LogWoR, Log, TEXT("%s Field attack wheel: picked %d"), WoRHubControllerConstants::LOG_TAG, Picked + 1);
	if (HubHUD)
	{
		HubHUD->ShowFieldAttack(Entry);
	}
}

void AWoRHubController::OpenFieldWheel()
{
	const UFieldMovementComponent *FieldMovement = bFieldSourcePressed ? GetFieldMovement() : nullptr;
	if (!FieldMovement || !HubHUD || !FieldWheelContext)
	{
		return;
	}
	FieldWheelEntries = FieldMovement->GetFieldAttackEntries();
	const TArray<int32> Slices = FieldSkillRules::GetFieldSliceStarts(FieldWheelEntries);
	if (Slices.Num() > HubHUD->GetFieldWheelCapacity())
	{
		FieldWheelEntries.SetNum(Slices[HubHUD->GetFieldWheelCapacity()]); // the stick picks only shown slices
	}
	const int32 Kept = FieldSkillRules::ClampFieldAttackIndex(FieldMovement->GetFieldAttackIndex(), FieldWheelEntries.Num());
	if (!HubHUD->ShowFieldAttackWheel(FieldWheelEntries, Kept))
	{
		FieldWheelEntries.Reset();
		return; // the hub or nothing to pick
	}
	FieldWheelPick = FieldWheelKept = Kept;
	FieldWheelPointer = FVector2D::ZeroVector;
	bFieldAttackPressed = false; // B-503m4: RB held for the wheel doesn't attack on release
	if (UEnhancedInputLocalPlayerSubsystem *Mappings = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Mappings->AddMappingContext(FieldWheelContext, WoRInputPriority::FIELD_WHEEL);
	}
	UE_LOG(LogWoR, Log, TEXT("%s Field attack wheel open: %d entries, on %d (wor.Field.CycleAttack lists them)"), WoRHubControllerConstants::LOG_TAG,
		   FieldWheelEntries.Num(), Kept + 1);
}

void AWoRHubController::CloseFieldWheel()
{
	if (!IsFieldWheelOpen())
	{
		return;
	}
	FieldWheelPick = FieldWheelKept = INDEX_NONE;
	FieldWheelEntries.Reset();
	if (HubHUD)
	{
		HubHUD->HideFieldAttackWheel();
	}
	if (UEnhancedInputLocalPlayerSubsystem *Mappings = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Mappings->RemoveMappingContext(FieldWheelContext);
	}
}

void AWoRHubController::PickFieldWheelSlice(int32 Slice)
{
	const TArray<int32> Slices = FieldSkillRules::GetFieldSliceStarts(FieldWheelEntries);
	if (!Slices.IsValidIndex(Slice) || Slice == FieldSkillRules::GetFieldSliceOf(FieldWheelEntries, FieldWheelPick) || !HubHUD)
	{
		return;
	}
	// Back on the kept entry's slice, the kept option; any other slice opens on its first option.
	FieldWheelPick = Slice == FieldSkillRules::GetFieldSliceOf(FieldWheelEntries, FieldWheelKept) ? FieldWheelKept : Slices[Slice];
	HubHUD->ShowFieldAttackWheel(FieldWheelEntries, FieldWheelPick);
}

void AWoRHubController::StepFieldWheelElement(int32 Delta)
{
	if (!IsFieldWheelOpen() || !HubHUD)
	{
		return;
	}
	FieldWheelPick = FieldSkillRules::StepFieldSliceOption(FieldWheelEntries, FieldWheelPick, Delta);
	HubHUD->ShowFieldAttackWheel(FieldWheelEntries, FieldWheelPick);
}

void AWoRHubController::HandleFieldWheelPick(const FInputActionValue &Value)
{
	if (IsFieldWheelOpen())
	{
		const int32 SliceCount = FieldSkillRules::GetFieldSliceStarts(FieldWheelEntries).Num();
		PickFieldWheelSlice(FieldSkillRules::GetFieldWheelIndexFromStick(Value.Get<FVector2D>(), SliceCount));
	}
}

void AWoRHubController::HandleFieldWheelMouse(const FInputActionValue &Value)
{
	if (IsFieldWheelOpen())
	{
		// The mouse moves a pointer on the wheel (GTA V's PC wheel) that reads as the stick. TODO: PIE: mouse up is up?
		FieldWheelPointer = FieldSkillRules::MoveFieldWheelPointer(FieldWheelPointer, Value.Get<FVector2D>());
		const int32 SliceCount = FieldSkillRules::GetFieldSliceStarts(FieldWheelEntries).Num();
		PickFieldWheelSlice(FieldSkillRules::GetFieldWheelIndexFromStick(FieldWheelPointer, SliceCount));
	}
}

void AWoRHubController::HandleFieldWheelElement(const FInputActionValue &Value)
{
	const float Direction = Value.Get<float>();
	if (Direction != 0.0f)
	{
		StepFieldWheelElement(Direction > 0.0f ? 1 : -1); // D-pad right / D: the next element
	}
}

void AWoRHubController::HandleFieldWheelScroll(const FInputActionValue &Value)
{
	const float Scroll = Value.Get<float>();
	if (Scroll != 0.0f)
	{
		StepFieldWheelElement(Scroll < 0.0f ? 1 : -1); // down (towards you) is the next, as in a list (Q-503m3)
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
