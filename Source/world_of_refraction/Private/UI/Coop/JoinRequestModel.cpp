// JoinRequestModel.cpp

#include "UI/Coop/JoinRequestModel.h"

#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/RunStateComponent.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRJoinRequestCard"

namespace
{
	UPartySessionSubsystem *PartySessionOf(UWorld *World)
	{
		UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	}

	/** The server's run component, or null (a client, or no run here). */
	URunStateComponent *HostRunOf(UWorld *World)
	{
		URunStateComponent *Run = URunStateComponent::Find(World);
		return Run && Run->GetOwner() && Run->GetOwner()->HasAuthority() ? Run : nullptr;
	}

	FString FindPlayerName(UWorld *World, int32 PlayerId)
	{
		const AGameStateBase *GameState = World ? World->GetGameState() : nullptr;
		for (const APlayerState *Player : GameState ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
		{
			if (Player && Player->GetPlayerId() == PlayerId)
			{
				return Player->GetPlayerName();
			}
		}
		return FString();
	}

	/** "Wind Caster" for a Caster, else the class alone. */
	FText GetCharacterKind(const UCharacterData &Character)
	{
		const FText ClassName = WoRGameNames::GetCharacterClassName(Character.CharacterClass);
		return Character.HasInnateElement()
				   ? FText::Format(LOCTEXT("ElementClass", "{0} {1}"), WoRGameNames::GetElementName(Character.GetElement()), ClassName)
				   : ClassName;
	}
}

FText WoRJoinRequestCard::GetSeatStatus(bool bHeld)
{
	return bHeld ? LOCTEXT("HeldSeat", "AI seat · held · give") : LOCTEXT("FreeSeat", "AI seat · give");
}

FJoinRequestCard WoRJoinRequestCard::BuildCard(int32 PlayerId, const FString &PlayerName, const UCharacterData *Character,
											   TArray<FJoinRequestSeatRow> Seats, int32 MoreWaiting, bool bSeatOpen, bool bInFight,
											   bool bFocused)
{
	FJoinRequestCard Card;
	Card.bShown = true;
	Card.PlayerId = PlayerId;
	const FText Player = PlayerName.IsEmpty() ? FText::Format(LOCTEXT("PlayerById", "Player {0}"), PlayerId) : FText::FromString(PlayerName);
	const FText CharacterName = Character ? FText::FromString(Character->Name) : LOCTEXT("UnknownCharacter", "their character");
	Card.Title = FText::Format(LOCTEXT("Title", "{0} wants to join"), Player);
	Card.WithLine = Character ? FText::Format(LOCTEXT("WithLine", "with {0} · {1}"), CharacterName, GetCharacterKind(*Character))
							  : FText::Format(LOCTEXT("WithLineBare", "with {0}"), CharacterName);
	Card.Seats = MoveTemp(Seats);
	Card.ReplaceLine = FText::Format(LOCTEXT("ReplaceLine", "{0} replaces the seat's character for this run."), CharacterName);

	TArray<FText> Notes;
	if (bInFight)
	{
		Notes.Add(LOCTEXT("InFight", "Seats are fixed during a fight: answer when it ends."));
	}
	else if (!bSeatOpen)
	{
		// B-233b (Q-233b): a friend who left takes its own seat back anywhere; a new one waits for a rest spot.
		Notes.Add(LOCTEXT("SeatClosed", "In a run, new friends are seated at a rest spot."));
	}
	if (MoreWaiting > 0)
	{
		Notes.Add(FText::Format(LOCTEXT("MoreWaiting", "{0} more waiting."), MoreWaiting));
	}
	Card.Note = FText::Join(FText::FromString(TEXT("  ")), Notes);
	Card.Hints = bFocused ? LOCTEXT("HintsFocused", "{Confirm}  Give this seat    {Toggle}  Refuse    {Back}  Close")
						  : LOCTEXT("HintsIdle", "Hold {ViewButton} to answer  ·  then  {Confirm}  Give this seat    {Toggle}  Refuse");
	return Card;
}

FJoinRequestCard WoRJoinRequestCard::BuildCard(UWorld *World, bool bFocused)
{
	const URunStateComponent *Run = HostRunOf(World);
	if (!Run || Run->GetJoinRequests().IsEmpty())
	{
		return FJoinRequestCard();
	}
	const UPartySessionSubsystem *PartySession = PartySessionOf(World);
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	TArray<FJoinRequestSeatRow> Seats;
	for (const int32 Slot : Run->GetFreeAISeats())
	{
		const FPartyMember *Member = Party && Party->Members.IsValidIndex(Slot) ? &Party->Members[Slot] : nullptr;
		FJoinRequestSeatRow &Row = Seats.AddDefaulted_GetRef();
		Row.Slot = Slot;
		Row.Name = Member && Member->CharacterData ? FText::FromString(Member->CharacterData->Name)
												   : FText::Format(LOCTEXT("SlotName", "Slot {0}"), Slot);
		Row.Status = GetSeatStatus(Member && !Run->GetHeldFor(Member->MemberId).IsEmpty());
	}
	const FRunJoinRequest &Request = Run->GetJoinRequests()[0];
	return BuildCard(Request.PlayerId, FindPlayerName(World, Request.PlayerId), Request.Character, MoveTemp(Seats),
					 Run->GetJoinRequests().Num() - 1, Run->CanSeatNewJoinerNow(), PartySession && PartySession->IsFightRunning(),
					 bFocused);
}

FText WoRJoinRequestCard::Give(UWorld *World, int32 PlayerId, int32 Slot)
{
	URunStateComponent *Run = HostRunOf(World);
	if (!Run)
	{
		return LOCTEXT("NotHost", "Only the host answers a join request.");
	}
	const UPartySessionSubsystem *PartySession = PartySessionOf(World);
	if (PartySession && PartySession->IsFightRunning())
	{
		return LOCTEXT("GiveInFight", "Seats are fixed during a fight: answer when it ends.");
	}
	if (Run->AcceptJoin(PlayerId, Slot))
	{
		return FText::GetEmpty();
	}
	return Run->CanSeatNewJoinerNow() ? LOCTEXT("GiveRefused", "That seat can't be given now.")
									  : LOCTEXT("GiveAtRestSpot", "In a run, new friends are seated at a rest spot.");
}

FText WoRJoinRequestCard::Refuse(UWorld *World, int32 PlayerId)
{
	URunStateComponent *Run = HostRunOf(World);
	if (!Run)
	{
		return LOCTEXT("RefuseNotHost", "Only the host answers a join request.");
	}
	return Run->RefuseJoin(PlayerId) ? FText::GetEmpty() : LOCTEXT("RefuseGone", "That request is gone.");
}

FString WoRJoinRequestCard::GetCardString(UWorld *World)
{
	const FJoinRequestCard Card = BuildCard(World, false);
	if (!Card.bShown)
	{
		return TEXT("[JoinRequestCard] hidden (no join request, or not the host)");
	}
	FString Out = FString::Printf(TEXT("[JoinRequestCard] %s (player %d)\n  %s"), *Card.Title.ToString(), Card.PlayerId, *Card.WithLine.ToString());
	for (const FJoinRequestSeatRow &Seat : Card.Seats)
	{
		Out += FString::Printf(TEXT("\n  slot %d  %s  %s"), Seat.Slot, *Seat.Name.ToString(), *Seat.Status.ToString());
	}
	Out += TEXT("\n  ") + Card.ReplaceLine.ToString();
	if (!Card.Note.IsEmpty())
	{
		Out += TEXT("\n  ") + Card.Note.ToString();
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GJoinRequestCardPrintCommand(
	TEXT("wor.UI.PrintJoinRequest"),
	TEXT("B-459c: log what the host's join request card shows (who asks, with which character, the AI seats)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoRUI, Log, TEXT("%s"), *WoRJoinRequestCard::GetCardString(World));
	}));
#endif

#undef LOCTEXT_NAMESPACE
