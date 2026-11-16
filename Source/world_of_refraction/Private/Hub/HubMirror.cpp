// HubMirror.cpp

#include "Hub/HubMirror.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/Hub/FormsScreenWidget.h"
#include "UI/Hub/HubHUDModel.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "HubMirror"

void AHubMirror::Interact(APawn *Pawn)
{
	APlayerController *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	// D-234: only rows that work show, and Spirit is the only one built. TODO: D-221 stage 3 adds the Presets row
	// here (and Cosmetics once that screen exists); with two rows or more, the mirror opens a row menu instead.
	if (!UFormsScreenWidget::Open(PC))
	{
		UE_LOG(LogWoR, Warning, TEXT("[HubMirror] %s: no player controller to open the Spirit screen"), *GetNameSafe(Pawn));
	}
}

FInteractPrompt AHubMirror::GetPrompt() const
{
	return FInteractPrompt(EInteractVerb::Open, LOCTEXT("MirrorPromptName", "Mirror"));
}

FString AHubMirror::GetHintText() const
{
	return WoRHubHUD::FormatPromptLine(GetPrompt()).ToString(); // "E  Open · Mirror"
}

#undef LOCTEXT_NAMESPACE
