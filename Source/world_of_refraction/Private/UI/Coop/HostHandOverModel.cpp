// HostHandOverModel.cpp

#include "UI/Coop/HostHandOverModel.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Online/HostSuccession.h"
#include "Party/PartySessionSubsystem.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRHostHandOverScreens"

namespace
{
	UWoRHostHandOverSubsystem *HandOverOf(UWorld *World)
	{
		UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UWoRHostHandOverSubsystem>() : nullptr;
	}

	/** The pips lit per step: the host left (1), the old session left (2), the new host up (3); 4 is rejoined, when
	 *  the card closes. */
	constexpr int32 PIPS_HOST_LEFT = 1;
	constexpr int32 PIPS_SESSION_LEFT = 2;
	constexpr int32 PIPS_NEW_HOST_UP = 3;
}

FHostLostCard WoRHostHandOverScreens::BuildCard(EHostHandOverStep Step, bool bPlanned, bool bThisMachineHosts, int32 RejoinTries)
{
	FHostLostCard Card;
	Card.bShown = Step != EHostHandOverStep::None;
	if (!Card.bShown)
	{
		return Card;
	}
	Card.Title = bPlanned ? LOCTEXT("PlannedTitle", "Handing over host") : LOCTEXT("LostTitle", "The host left");
	Card.HostLine = bThisMachineHosts ? LOCTEXT("YouHost", "You are taking over as host.")
									  : LOCTEXT("NextHosts", "The next player is taking over as host.");
	Card.KeptLine = LOCTEXT("Kept", "Your run, gear and progress are kept.");
	switch (Step)
	{
	case EHostHandOverStep::TakeOverPending:
		Card.LitPips = PIPS_HOST_LEFT;
		Card.StatusLine = LOCTEXT("TakeOverPending", "Leaving the old session…");
		break;
	case EHostHandOverStep::Hosting:
		Card.LitPips = PIPS_SESSION_LEFT;
		Card.StatusLine = LOCTEXT("Hosting", "Reopening the trial for everyone…");
		break;
	case EHostHandOverStep::Rejoining:
		// The first search means the old session is gone; later ones wait on the new host.
		Card.LitPips = RejoinTries > 1 ? PIPS_NEW_HOST_UP : RejoinTries > 0 ? PIPS_SESSION_LEFT : PIPS_HOST_LEFT;
		Card.StatusLine = LOCTEXT("Rejoining", "Looking for the new host's session…");
		Card.bCanStop = true;
		break;
	case EHostHandOverStep::GaveUp:
	default:
		Card.bGaveUp = true;
		Card.StatusLine = LOCTEXT("GaveUp", "The new host couldn't be reached.");
		Card.KeptLine = LOCTEXT("KeptGaveUp", "Your run waits at its last checkpoint.");
		Card.bCanStop = true;
		break;
	}
	return Card;
}

FHostLostCard WoRHostHandOverScreens::BuildCard(UWorld *World)
{
	const UWoRHostHandOverSubsystem *HandOver = HandOverOf(World);
	if (!HandOver)
	{
		return FHostLostCard();
	}
	const bool bThisMachineHosts = !HandOver->GetNextHost().IsEmpty() && HandOver->GetNextHost() == HandOver->GetLocalPlayerId();
	return BuildCard(HandOver->GetStep(), HandOver->IsPlanned(), bThisMachineHosts, HandOver->GetRejoinTries());
}

FHandOverConfirm WoRHostHandOverScreens::BuildConfirm(ENetMode NetMode, int32 PlayersKnown, const FString &NextHostName)
{
	FHandOverConfirm Confirm;
	Confirm.NextHost = NextHostName.IsEmpty() ? LOCTEXT("NextPlayer", "the next player") : FText::FromString(NextHostName);
	if (NetMode != NM_ListenServer)
	{
		Confirm.Refusal = LOCTEXT("NotHost", "Only the host can hand over.");
	}
	else if (!WoRHostSuccession::CanHandOverFrom(NetMode, PlayersKnown))
	{
		Confirm.Refusal = LOCTEXT("Alone", "Nobody else is here to take over.");
	}
	Confirm.bCanHandOver = Confirm.Refusal.IsEmpty();
	return Confirm;
}

FHandOverConfirm WoRHostHandOverScreens::BuildConfirm(UWorld *World)
{
	const UWoRHostHandOverSubsystem *HandOver = HandOverOf(World);
	if (!World || !HandOver)
	{
		return BuildConfirm(NM_Standalone, 0, FString());
	}
	const FString NextId = WoRHostSuccession::PickNextHost(HandOver->GetKnownOrder(), HandOver->GetLocalPlayerId());
	FString NextName;
	const AGameStateBase *GameState = World->GetGameState();
	for (const APlayerState *Player : GameState ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
	{
		if (Player && !NextId.IsEmpty() && Player->GetUniqueId().ToString() == NextId)
		{
			NextName = Player->GetPlayerName();
		}
	}
	FHandOverConfirm Confirm = BuildConfirm(World->GetNetMode(), HandOver->GetKnownOrder().Num(), NextName);
	const UPartySessionSubsystem *PartySession = World->GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
	if (Confirm.bCanHandOver && PartySession && PartySession->IsFightRunning())
	{
		Confirm.Refusal = LOCTEXT("InFight", "Not during a fight.");
		Confirm.bCanHandOver = false;
	}
	return Confirm;
}

FText WoRHostHandOverScreens::HandOver(UWorld *World)
{
	const FHandOverConfirm Confirm = BuildConfirm(World);
	if (!Confirm.bCanHandOver)
	{
		return Confirm.Refusal;
	}
	UWoRHostHandOverSubsystem *HandOver = HandOverOf(World);
	return HandOver && HandOver->BeginPlannedHandOver(World) ? FText::GetEmpty()
															 : LOCTEXT("HandOverFailed", "The hand-over couldn't start (the save failed).");
}

FString WoRHostHandOverScreens::GetCardString(UWorld *World)
{
	const FHostLostCard Card = BuildCard(World);
	if (!Card.bShown)
	{
		return TEXT("[HostLostCard] hidden (no hand-over)");
	}
	return FString::Printf(TEXT("[HostLostCard] %s | %s | %s (%d of %d pips) | %s%s"), *Card.Title.ToString(), *Card.HostLine.ToString(),
						   *Card.StatusLine.ToString(), Card.LitPips, CARD_PIPS, *Card.KeptLine.ToString(), Card.bCanStop ? TEXT(" | B Stop waiting") : TEXT(""));
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GHandOverCardPrintCommand(
	TEXT("wor.Coop.PrintHandOverCard"),
	TEXT("B-459a: log what the host-lost card and the hand-over confirm show now."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const FHandOverConfirm Confirm = WoRHostHandOverScreens::BuildConfirm(World);
		UE_LOG(LogWoRUI, Log, TEXT("%s\n[HandOverConfirm] next host %s; %s"), *WoRHostHandOverScreens::GetCardString(World),
			   *Confirm.NextHost.ToString(), Confirm.bCanHandOver ? TEXT("can hand over") : *Confirm.Refusal.ToString());
	}));
#endif

#undef LOCTEXT_NAMESPACE
