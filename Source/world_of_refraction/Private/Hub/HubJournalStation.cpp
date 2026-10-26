// HubJournalStation.cpp

#include "Hub/HubJournalStation.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/Hub/MemoryJournalWidget.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "HubJournalStation"

void AHubJournalStation::Interact(APawn *Pawn)
{
	APlayerController *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!UMemoryJournalWidget::Open(PC))
	{
		UE_LOG(LogWoR, Warning, TEXT("[HubJournalStation] %s: no player controller to open the journal"), *GetNameSafe(Pawn));
	}
}

FString AHubJournalStation::GetHintText() const
{
	return LOCTEXT("JournalHint", "Press E: memory journal, V, conversations").ToString();
}

#undef LOCTEXT_NAMESPACE
