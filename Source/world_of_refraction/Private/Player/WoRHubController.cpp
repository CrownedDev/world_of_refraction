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
#include "InputTriggers.h"
#include "Character/CharacterDataComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/InputPriorities.h"
#include "Player/WoRCameraRules.h"
#include "Player/WoRFrontEndGameMode.h"
#include "UI/Hub/HubHUDWidget.h"
#include "UI/Menus/PauseMenuWidget.h"
#include "Trial/TrialDuplicates.h"
#include "UI/Menus/TitleScreenWidget.h"
#include "UI/WoRKeyHintText.h"
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
	const FName FIELD_WHEEL_PICK_ACTION_NAME = TEXT("IA_Field_StripMove"); // B-514 (D-222): was IA_Field_WheelPick
	const FName FIELD_WHEEL_MOUSE_ACTION_NAME = TEXT("IA_Field_WheelMouse");
	const FName FIELD_WHEEL_ELEMENT_ACTION_NAME = TEXT("IA_Field_StripSource"); // B-514: was IA_Field_WheelElement
	const FName FIELD_WHEEL_SCROLL_ACTION_NAME = TEXT("IA_Field_WheelScroll");
	const FName FIELD_WHEEL_CONFIRM_ACTION_NAME = TEXT("IA_Field_WheelConfirm");
	const FName FIELD_WHEEL_CLOSE_ACTION_NAME = TEXT("IA_Field_WheelClose");
	const FName FIELD_WHEEL_CONTEXT_NAME = TEXT("IMC_Field_Wheel");
	/** B-536: the game menu's second key (InputMap.md, Menus). */
	const FName GAME_MENU_TAB_ACTION_NAME = TEXT("IA_UI_GameMenuTab");
	const FName GAME_MENU_TAB_CONTEXT_NAME = TEXT("IMC_UI_GameMenuTab");
	/** B-503m5: how often the cursor is read while the wheel is up (only then; the field has no tick for it). */
	constexpr float FIELD_WHEEL_HOVER_SECONDS = 1.0f / 30.0f;
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
	// B-514 (D-222): the quick strip. Left / right along the pieces, up / down the highlighted piece's source.
	const auto MapSteps = [this](UInputAction *Action, std::initializer_list<FKey> Next, std::initializer_list<FKey> Back)
	{
		for (const FKey &Key : Next)
		{
			FieldWheelContext->MapKey(Action, Key);
		}
		for (const FKey &Key : Back)
		{
			FieldWheelContext->MapKey(Action, Key).Modifiers.Add(NewObject<UInputModifierNegate>(FieldWheelContext));
		}
	};
	FieldWheelPickAction = NewObject<UInputAction>(this, FIELD_WHEEL_PICK_ACTION_NAME);
	FieldWheelPickAction->ValueType = EInputActionValueType::Axis1D;
	MapSteps(FieldWheelPickAction, {EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right, EKeys::D},
			 {EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left, EKeys::A});
	FieldWheelMouseAction = NewObject<UInputAction>(this, FIELD_WHEEL_MOUSE_ACTION_NAME);
	FieldWheelMouseAction->ValueType = EInputActionValueType::Axis2D;
	FieldWheelContext->MapKey(FieldWheelMouseAction, EKeys::Mouse2D);
	FieldWheelElementAction = NewObject<UInputAction>(this, FIELD_WHEEL_ELEMENT_ACTION_NAME);
	FieldWheelElementAction->ValueType = EInputActionValueType::Axis1D;
	MapSteps(FieldWheelElementAction, {EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down, EKeys::S},
			 {EKeys::Gamepad_DPad_Up, EKeys::Gamepad_LeftStick_Up, EKeys::W});
	FieldWheelScrollAction = NewObject<UInputAction>(this, FIELD_WHEEL_SCROLL_ACTION_NAME);
	FieldWheelScrollAction->ValueType = EInputActionValueType::Axis1D;
	FieldWheelContext->MapKey(FieldWheelScrollAction, EKeys::MouseWheelAxis);
	// B-503m5 (D-218, a toggle): eaten while it is up, so the click doesn't attack, A doesn't jump and R / RB don't
	// reach the field keys. The key that opened it is still down then: Enhanced Input ignores it until let go.
	FieldWheelConfirmAction = NewObject<UInputAction>(this, FIELD_WHEEL_CONFIRM_ACTION_NAME);
	FieldWheelConfirmAction->ValueType = EInputActionValueType::Boolean;
	FieldWheelContext->MapKey(FieldWheelConfirmAction, EKeys::Gamepad_FaceButton_Bottom);
	FieldWheelContext->MapKey(FieldWheelConfirmAction, EKeys::LeftMouseButton);
	FieldWheelContext->MapKey(FieldWheelConfirmAction, EKeys::Enter); // B-539 (D-234): Enter selects the lit piece
	FieldWheelCloseAction = NewObject<UInputAction>(this, FIELD_WHEEL_CLOSE_ACTION_NAME);
	FieldWheelCloseAction->ValueType = EInputActionValueType::Boolean;
	for (const FKey &Key : {EKeys::Gamepad_FaceButton_Right, EKeys::BackSpace, EKeys::Gamepad_RightShoulder, EKeys::R})
	{
		FieldWheelContext->MapKey(FieldWheelCloseAction, Key);
	}
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

void AWoRHubController::BuildGameMenuTabKey()
{
	UEnhancedInputLocalPlayerSubsystem *Mappings =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (GameMenuTabContext || !Mappings)
	{
		return;
	}
	using namespace WoRHubControllerConstants;
	GameMenuTabAction = NewObject<UInputAction>(this, GAME_MENU_TAB_ACTION_NAME);
	GameMenuTabAction->ValueType = EInputActionValueType::Boolean;
	GameMenuTabAction->bConsumeInput = false; // a hold of Tab still reaches the join request card
	GameMenuTabContext = NewObject<UInputMappingContext>(this, GAME_MENU_TAB_CONTEXT_NAME);
	GameMenuTabContext->MapKey(GameMenuTabAction, EKeys::Tab).Triggers.Add(NewObject<UInputTriggerTap>(GameMenuTabContext));
	Mappings->AddMappingContext(GameMenuTabContext, WoRInputPriority::FIELD_KEYS);
}

void AWoRHubController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GameMenuTabContext) // B-536: as the field keys below, none left for the next level's controller
	{
		if (UEnhancedInputLocalPlayerSubsystem *Mappings =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Mappings->RemoveMappingContext(GameMenuTabContext);
		}
		GameMenuTabContext = nullptr;
	}
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

	// D-255 (a): the hub and trial look at most 10 up and 55 down (the engine's +-89.9 put the camera in the floor).
	if (PlayerCameraManager)
	{
		WoRCameraRules::ApplyFieldLimits(*PlayerCameraManager);
	}
	else if (IsLocalController())
	{
		UE_LOG(LogWoR, Warning, TEXT("%s No camera manager at BeginPlay: the D-255 look limits are not set"), WoRHubControllerConstants::LOG_TAG);
	}

	if (!GetPawn())
	{
		DisableInput(this); // B-545: on again in SetPawn
	}

	// B-443: the interact prompt and the money chip, for this machine's player only (B-537: not in the menu level).
	if (IsLocalController() && !WoRFrontEnd::IsFrontEndWorld(GetWorld()))
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

	// B-537 (D-232): the menu level opens the title every time it loads.
	if (IsLocalController() && WoRFrontEnd::IsFrontEndWorld(GetWorld()))
	{
		UTitleScreenWidget::Open(this);
		return;
	}
	// B-570 (TrialPoolDesign §6): a duplicate dropped in the last fight asks its three choices here, back on the map.
	if (IsLocalController())
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { TrialDuplicates::OpenNextPrompt(this); }));
	}
	// B-467, B-535: the game opens on the title, PIE included (the first map load of a session, the default map only).
	// OBSOLETE (B-537): once the menu level is GameDefaultMap this opens nothing (the hub is no longer the default map).
	UTitleScreenWidget::OpenAtLaunch(this);
}

void AWoRHubController::SetPawn(APawn *InPawn)
{
	Super::SetPawn(InPawn);
	if (InPawn)
	{
		EnableInput(this);
	}
	else if (InputEnabled())
	{
		DisableInput(this);
		UE_LOG(LogWoR, Log, TEXT("[WoRHubController] No pawn: the controller's own input is off until the next one"));
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
		EnhancedInput->BindAction(FieldWheelPickAction, ETriggerEvent::Started, this, &AWoRHubController::HandleFieldWheelPick);
		EnhancedInput->BindAction(FieldWheelElementAction, ETriggerEvent::Started, this, &AWoRHubController::HandleFieldWheelElement);
		EnhancedInput->BindAction(FieldWheelScrollAction, ETriggerEvent::Triggered, this, &AWoRHubController::HandleFieldWheelScroll);
		EnhancedInput->BindAction(FieldWheelConfirmAction, ETriggerEvent::Started, this, &AWoRHubController::HandleFieldWheelConfirm);
		EnhancedInput->BindAction(FieldWheelCloseAction, ETriggerEvent::Started, this, &AWoRHubController::HandleFieldWheelClose);
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
	BuildGameMenuTabKey();
	if (GameMenuTabAction)
	{
		EnhancedInput->BindAction(GameMenuTabAction, ETriggerEvent::Triggered, this, &AWoRHubController::HandleToggleMenu); // on the tap's release
	}
}

void AWoRHubController::HandleToggleMenu(const FInputActionValue & /*Value*/)
{
	CloseFieldWheel(); // B-503m5: the wheel stays up after its key is let go; a menu over it closes it, picking nothing
	OpenGameMenu();
}

void AWoRHubController::OpenGameMenu()
{
	// Opening only: with the menu up, game input is off, so the menu's own Back / Menu key closes it.
	if (WoRFrontEnd::IsFrontEndWorld(GetWorld()))
	{
		return; // B-537: the menu level is the title; there is no game to pause
	}
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
	// B-503s2: the local leader's pawn is the party's first member (AWoRGameModeBase's rule, B-529); stamped here too
	// for a map whose game mode doesn't derive from it yet.
	const UPartySessionSubsystem *Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = Session ? Session->GetLocalParty() : nullptr;
	UCharacterDataComponent *CharComp = InPawn ? InPawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (IsLocalController() && Party && Party->Members.Num() > 0 && CharComp && CharComp->CharacterData == Party->Members[0].CharacterData)
	{
		CharComp->StampPartyMember(Party->Members[0].MemberId);
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
	GetWorldTimerManager().ClearTimer(FieldWheelOpenTimer); // focus loss or a menu took the key before the wheel opened
}

void AWoRHubController::HandleFieldSwitchSource(const FInputActionValue & /*Value*/)
{
	// B-503m5 (D-218, a toggle): letting go leaves an open wheel up (its own keys confirm or close it). A tap
	// changes nothing: RB's tap is the attack (IA_Field_Attack), R's is nothing.
	bFieldSourcePressed = false;
	GetWorldTimerManager().ClearTimer(FieldWheelOpenTimer);
}

void AWoRHubController::ConfirmFieldWheel()
{
	UFieldMovementComponent *FieldMovement = GetFieldMovement();
	const int32 Picked = FieldWheelPick;
	CloseFieldWheel();
	if (!FieldMovement || Picked == INDEX_NONE)
	{
		return;
	}
	// An element option too: "Ice", "(R) Fire", "Dark Fire".
	const FFieldAttackEntry Entry = FieldMovement->SelectFieldAttack(Picked);
	UE_LOG(LogWoR, Log, TEXT("%s Field attack wheel: picked %d"), WoRHubControllerConstants::LOG_TAG, Picked + 1);
	if (HubHUD)
	{
		HubHUD->ShowFieldAttack(Entry);
	}
}

void AWoRHubController::HandleFieldWheelConfirm(const FInputActionValue & /*Value*/)
{
	if (!IsFieldWheelOpen() || !HubHUD)
	{
		return;
	}
	if (bFieldWheelCursor && IsInputKeyDown(EKeys::LeftMouseButton)) // Enter takes the lit piece, as A does
	{
		// A click picks the slice under it; a click off the wheel does nothing (the wheel stays up).
		bool bOnWheel = false;
		const int32 Slice = HubHUD->GetFieldWheelSliceUnderCursor(bOnWheel);
		if (!bOnWheel)
		{
			return;
		}
		PickFieldWheelSlice(Slice);
	}
	ConfirmFieldWheel();
}

void AWoRHubController::HandleFieldWheelClose(const FInputActionValue & /*Value*/)
{
	CloseFieldWheel();
}

void AWoRHubController::UpdateFieldWheelHover()
{
	// B-514: only a moving cursor picks, so a resting one doesn't undo A / D or the mouse wheel.
	FVector2f CursorAt;
	if (!IsFieldWheelOpen() || !HubHUD || !GetMousePosition(CursorAt.X, CursorAt.Y) || CursorAt.Equals(FieldWheelCursorAt))
	{
		return;
	}
	FieldWheelCursorAt = CursorAt;
	bool bOnWheel = false;
	PickFieldWheelSlice(HubHUD->GetFieldWheelSliceUnderCursor(bOnWheel));
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
	if (!HubHUD->ShowFieldAttackWheel(FieldWheelEntries, Kept, Kept))
	{
		FieldWheelEntries.Reset();
		return; // nothing to pick
	}
	FieldWheelPick = FieldWheelKept = Kept;
	bFieldAttackPressed = false; // B-503m4: RB held for the wheel doesn't attack on release
	if (UEnhancedInputLocalPlayerSubsystem *Mappings = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Mappings->AddMappingContext(FieldWheelContext, WoRInputPriority::FIELD_WHEEL);
	}
	// B-503m5 (D-218): the character stands and the camera holds while it is up, whatever the pawn's own look and
	// move keys are; on keyboard and mouse the cursor shows and picks.
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
	bFieldWheelCursor = !WoRKeyHints::IsGamepad(HubHUD.Get());
	if (bFieldWheelCursor)
	{
		SetShowMouseCursor(true);
		FInputModeGameAndUI CursorMode;
		CursorMode.SetHideCursorDuringCapture(false);
		CursorMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		SetInputMode(CursorMode);
		GetMousePosition(FieldWheelCursorAt.X, FieldWheelCursorAt.Y); // the cursor picks only once it moves
		GetWorldTimerManager().SetTimer(FieldWheelHoverTimer, this, &AWoRHubController::UpdateFieldWheelHover,
										WoRHubControllerConstants::FIELD_WHEEL_HOVER_SECONDS, /*bLoop=*/true);
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
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	if (bFieldWheelCursor)
	{
		bFieldWheelCursor = false;
		if (GetWorld())
		{
			GetWorldTimerManager().ClearTimer(FieldWheelHoverTimer);
		}
		SetShowMouseCursor(false);
		SetInputMode(FInputModeGameOnly());
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
	HubHUD->ShowFieldAttackWheel(FieldWheelEntries, FieldWheelPick, FieldWheelKept);
}

void AWoRHubController::StepFieldWheelPiece(int32 Delta)
{
	if (IsFieldWheelOpen())
	{
		const int32 PieceCount = FieldSkillRules::GetFieldSliceStarts(FieldWheelEntries).Num();
		const int32 Piece = FieldSkillRules::GetFieldSliceOf(FieldWheelEntries, FieldWheelPick);
		PickFieldWheelSlice(FieldSkillRules::StepFieldWheelIndex(Piece, Delta, PieceCount)); // wraps at the ends
	}
}

void AWoRHubController::StepFieldWheelElement(int32 Delta)
{
	if (!IsFieldWheelOpen() || !HubHUD)
	{
		return;
	}
	FieldWheelPick = FieldSkillRules::StepFieldSliceOption(FieldWheelEntries, FieldWheelPick, Delta);
	HubHUD->ShowFieldAttackWheel(FieldWheelEntries, FieldWheelPick, FieldWheelKept);
}

void AWoRHubController::HandleFieldWheelPick(const FInputActionValue &Value)
{
	const float Direction = Value.Get<float>();
	if (Direction != 0.0f)
	{
		StepFieldWheelPiece(Direction > 0.0f ? 1 : -1); // B-514: right / D, the next piece
	}
}

void AWoRHubController::HandleFieldWheelElement(const FInputActionValue &Value)
{
	const float Direction = Value.Get<float>();
	if (Direction != 0.0f)
	{
		StepFieldWheelElement(Direction > 0.0f ? 1 : -1); // B-514: down / S, the next source, as in a list
	}
}

void AWoRHubController::HandleFieldWheelScroll(const FInputActionValue &Value)
{
	const float Scroll = Value.Get<float>();
	if (Scroll != 0.0f)
	{
		StepFieldWheelPiece(Scroll < 0.0f ? 1 : -1); // B-514 (D-222): along the pieces, down (towards you) the next
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
