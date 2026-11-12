// WoRHubController.cpp

#include "Player/WoRHubController.h"

#include "Blueprint/UserWidget.h"
#include "Character/CombatCharacter.h"
#include "EnhancedInputComponent.h"
#include "Engine/GameInstance.h"
#include "Field/FieldMovementComponent.h"
#include "Hub/HubInteract.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Party/PartySessionSubsystem.h"
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
}

void AWoRHubController::BeginPlay()
{
	Super::BeginPlay();

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
	UFieldMovementComponent *FieldMovement = GetPawn() ? GetPawn()->FindComponentByClass<UFieldMovementComponent>() : nullptr;
	if (FieldMovement)
	{
		FieldMovement->ActivateFieldSkill();
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
