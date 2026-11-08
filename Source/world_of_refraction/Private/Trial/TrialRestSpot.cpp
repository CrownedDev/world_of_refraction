// TrialRestSpot.cpp

#include "Trial/TrialRestSpot.h"

#include "Player/WoRPlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Party/PartySessionSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Trial/TrialData.h"
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
	Run->SetAtRestSpot(true); // B-363 (D-156): pillar points are placed and bought here; the skin switch reads it too
	if (USaveSubsystem *Save = GI->GetSubsystem<USaveSubsystem>())
	{
		Save->SaveNow(World, true); // desktop review: a rest is a checkpoint (the heal and the rest spot survive a quit)
	}
	UE_LOG(LogWoR, Log, TEXT("[RestSpot] Rested at %s: every member full, %d revived"), *RestSpotId.ToString(), Revived);
	return Revived;
}

bool WoRRestSpot::CanReviveHere(const UWorld *World)
{
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	return Run && Run->CanReviveEncounter(Run->GetLastRestSpot());
}

bool WoRRestSpot::ReviveMiniBoss(UWorld *World)
{
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	// Desktop review: only when the level can reload, so a revive is never spent for nothing.
	const UTrialData *Trial = Run ? Run->GetActiveTrial() : nullptr;
	if (!Run || World->GetNetMode() == NM_Client || !Trial || Trial->Level.IsNull() || !Run->ReviveEncounter(Run->GetLastRestSpot()))
	{
		return false;
	}
	if (USaveSubsystem *Save = GI->GetSubsystem<USaveSubsystem>())
	{
		Save->SaveNow(World, true); // desktop review: quitting right after a revive can't skip the mini-boss
	}
	// B-187c: the reload puts the party back where it stood to rest (ATrialGameMode::RestartPlayer).
	FTransform Stand;
	if (Run->GetRestStand(Stand))
	{
		Run->SetPendingReturnStand(Stand);
	}
	StepAway(World); // desktop review: the party leaves the spot with the reload
	Run->ReloadTrialLevel();
	return true;
}

void WoRRestSpot::StepAway(UWorld *World)
{
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	if (UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr)
	{
		Run->SetAtRestSpot(false);
	}
	if (UCharacterFormSubsystem *Forms = GI ? GI->GetSubsystem<UCharacterFormSubsystem>() : nullptr)
	{
		Forms->SetAtRestSpot(false);
	}
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
	if (WoRRestSpot::Rest(GetWorld(), GetRestSpotId()) == INDEX_NONE)
	{
		return;
	}
	if (UTrialRunSubsystem *Run = Pawn ? GetTrialRun() : nullptr)
	{
		Run->NoteRestStand(Pawn->GetActorTransform()); // B-187c: a revive brings the party back here
	}
	ShowMessageTo(Pawn, LOCTEXT("Rested", "You rest. The party is restored."));
	APlayerController *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC && PC->IsLocalController())
	{
		URestSpotWidget::Open(PC);
	}
	else if (AWoRPlayerState *Player = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr)
	{
		// B-187d: a client's press (ServerInteractFeature) opens its own menu; B-187e: with the host's revive answer.
		Player->Client_OpenRestMenu(WoRRestSpot::CanReviveHere(GetWorld()), WoRRestSpot::GetNextRevivalNumber(GetWorld()));
	}
	RefreshHint();
}

int32 WoRRestSpot::GetNextRevivalNumber(const UWorld *World)
{
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	return Run ? Run->GetEncounterRevivals(Run->GetLastRestSpot()) + 1 : 0;
}

ATrialRestSpot *ATrialRestSpot::SpawnForMiniBoss(UWorld *World, const FTransform &Where, FName EncounterId)
{
	if (!World || EncounterId.IsNone())
	{
		return nullptr;
	}
	ATrialRestSpot *Spot = World->SpawnActorDeferred<ATrialRestSpot>(ATrialRestSpot::StaticClass(), Where, nullptr, nullptr,
																	 ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Spot)
	{
		return nullptr;
	}
	Spot->MiniBossEncounterId = EncounterId; // before BeginPlay, so the hint and id are right from the start
	Spot->FinishSpawning(Where);
	UE_LOG(LogWoR, Log, TEXT("[RestSpot] A rest spot where mini-boss %s fell"), *EncounterId.ToString());
	return Spot;
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
