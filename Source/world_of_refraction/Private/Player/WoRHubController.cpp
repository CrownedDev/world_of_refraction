// WoRHubController.cpp

#include "Player/WoRHubController.h"

#include "Blueprint/UserWidget.h"
#include "UI/WoRRootLayout.h"

namespace WoRHubControllerConstants
{
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

	// Root layout ONLY. The Blueprint's BeginPlay runs after this and still owns
	// the mapping context, EnsureLocalParty, and the WBP_PartyDisplay creation —
	// none of it is duplicated or replaced here.
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
