// HubJournalStation.cpp

#include "Hub/HubJournalStation.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/Hub/MemoryJournalWidget.h"
#include "UI/Menus/JournalScreenWidget.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "HubJournalStation"

void AHubJournalStation::Interact(APawn *Pawn)
{
	APlayerController *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	// B-471c (Q-463 point 3): V's lines are heard in the vendors' talk box now, so the station opens the journal screen.
	if (UJournalScreenWidget::Open(PC))
	{
		return;
	}
	// OBSOLETE (B-471c): the placeholder journal, reached only without a root layout; it goes after the PIE pass.
	if (!UMemoryJournalWidget::Open(PC))
	{
		UE_LOG(LogWoR, Warning, TEXT("[HubJournalStation] %s: no player controller to open the journal"), *GetNameSafe(Pawn));
	}
}

FInteractPrompt AHubJournalStation::GetPrompt() const
{
	return FInteractPrompt(EInteractVerb::Open, LOCTEXT("JournalPromptName", "Journal"));
}

FString AHubJournalStation::GetHintText() const
{
	return LOCTEXT("JournalHint", "Press E: journal").ToString();
}

#undef LOCTEXT_NAMESPACE
