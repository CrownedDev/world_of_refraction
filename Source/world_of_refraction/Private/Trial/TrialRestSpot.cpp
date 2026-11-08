// TrialRestSpot.cpp

#include "Trial/TrialRestSpot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Party/PartySessionSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Hub/RestSpotWidget.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "TrialRestSpot"

int32 WoRRestSpot::Rest(UWorld *World, FName RestSpotId)
{
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	// Desktop review: the host rests the party (a client has no run; its press reaches the host in B-187b, D-103).
	if (!Run || !Run->IsRunActive() || !PartySession || World->GetNetMode() == NM_Client)
	{
		return INDEX_NONE;
	}
	const int32 Revived = PartySession->ReviveFallenMembers(); // D-043: the fallen come back here
	PartySession->SetAllMembersFull();
	Run->NoteRestSpotUsed(RestSpotId); // a wipe sends the party back here (D-155)
	if (USaveSubsystem *Save = GI->GetSubsystem<USaveSubsystem>())
	{
		Save->SaveNow(World, true); // desktop review: a rest is a checkpoint (the heal and the rest spot survive a quit)
	}
	UE_LOG(LogWoR, Log, TEXT("[RestSpot] Rested at %s: every member full, %d revived"), *RestSpotId.ToString(), Revived);
	return Revived;
}

bool WoRRestSpot::LeaveRun(UWorld *World)
{
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (!Run || !Run->IsRunActive() || World->GetNetMode() == NM_Client)
	{
		return false;
	}
	Run->LeaveRunEarly();
	return true;
}

FText WoRRestSpot::GetLeaveConfirmText()
{
	return LOCTEXT("LeaveConfirm", "Leave the trial? You keep part of what you earned. Full rewards only for beating the boss or falling in battle.");
}

void ATrialRestSpot::Interact(APawn *Pawn)
{
	if (WoRRestSpot::Rest(GetWorld(), GetFeatureId()) == INDEX_NONE)
	{
		return;
	}
	ShowMessage(LOCTEXT("Rested", "You rest. The party is restored."));
	APlayerController *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC && PC->IsLocalController())
	{
		URestSpotWidget::Open(PC); // TODO: co-op: a client's menu (its press reaches the host's rest, B-187b)
	}
	RefreshHint();
}

FString ATrialRestSpot::GetHintText() const
{
	return LOCTEXT("RestHint", "Press E to rest").ToString();
}

static FAutoConsoleCommandWithWorld GTrialRestCommand(
	TEXT("wor.Trial.Rest"),
	TEXT("B-187: rest the party as at a rest spot (full HP / EP, the fallen revived, the last rest spot noted) and open the rest menu."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const int32 Revived = WoRRestSpot::Rest(World, TEXT("ConsoleRest"));
		if (Revived == INDEX_NONE)
		{
			UE_LOG(LogWoR, Display, TEXT("[RestSpot] wor.Trial.Rest: no run"));
			return;
		}
		URestSpotWidget::Open(World->GetFirstPlayerController());
	}));

#undef LOCTEXT_NAMESPACE
