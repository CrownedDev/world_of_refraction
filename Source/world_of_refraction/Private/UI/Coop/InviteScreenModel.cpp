// InviteScreenModel.cpp

#include "UI/Coop/InviteScreenModel.h"

#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Online/WoRSessionSubsystem.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/RunStateComponent.h"
#include "Player/WoRPlayerState.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRInviteScreen"

namespace
{
	UWoRSessionSubsystem *SessionsOf(UWorld *World)
	{
		UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr;
	}

	/** The name of the remote player who claims Member, or empty (nobody, or this machine's own player). */
	FString FindClaimantName(UWorld *World, const FPartyMember &Member)
	{
		const AGameStateBase *GameState = World ? World->GetGameState() : nullptr;
		for (const APlayerState *Player : GameState ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
		{
			const AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player);
			const APlayerController *PC = WoRPlayer ? Cast<APlayerController>(WoRPlayer->GetOwner()) : nullptr;
			if (WoRPlayer && !(PC && PC->IsLocalController()) &&
				(WoRPlayer->ClaimsMember(Member.MemberId) || WoRPlayer->ClaimsCharacter(Member.CharacterData)))
			{
				return WoRPlayer->GetPlayerName();
			}
		}
		return FString();
	}
}

FText WoRInviteScreen::GetSeatWho(bool bLeader, bool bAISeat, bool bHeld, const FString &ClaimantName, bool bHosting)
{
	if (bLeader)
	{
		return bHosting ? LOCTEXT("YouHost", "You (host)") : LOCTEXT("You", "You");
	}
	if (bAISeat)
	{
		return bHeld ? LOCTEXT("HeldSeat", "AI seat · held") : LOCTEXT("AISeat", "AI seat");
	}
	return ClaimantName.IsEmpty() ? LOCTEXT("You", "You") : FText::FromString(ClaimantName);
}

FInviteScreenPage WoRInviteScreen::BuildPage(const TArray<FString> &FriendNames, const TSet<int32> &Invited, bool bHosting,
											 TArray<FInviteSeatRow> Seats)
{
	FInviteScreenPage Page;
	Page.Title = LOCTEXT("Title", "Play together");
	Page.bHosting = bHosting;
	Page.Subtitle = bHosting ? LOCTEXT("Hosting", "You host a session: friends can join")
							 : LOCTEXT("NotHosting", "No session: host one so friends can join");
	for (int32 Index = 0; Index < FriendNames.Num(); ++Index)
	{
		FInviteFriendRow &Row = Page.Friends.AddDefaulted_GetRef();
		Row.Name = FText::FromString(FriendNames[Index]);
		Row.bInvited = Invited.Contains(Index);
		Row.Status = Row.bInvited ? LOCTEXT("Invited", "invited · waiting") : LOCTEXT("InviteVerb", "invite");
	}
	const FText HowSeating = LOCTEXT("HowSeating", "A friend brings their own character and takes an AI seat.\nIn a run, new friends are seated at a rest spot.");
	Page.FriendsNote = FriendNames.IsEmpty()
						   ? FText::Format(LOCTEXT("NoFriends", "No friends list here (a local network game): a friend on your network finds your session with wor.Session.Find.\n{0}"), HowSeating)
						   : HowSeating;
	Page.Seats = MoveTemp(Seats);
	Page.SessionAction = bHosting ? LOCTEXT("StopHosting", "Stop hosting") : LOCTEXT("Host", "Host a session");
	return Page;
}

FInviteScreenPage WoRInviteScreen::BuildPage(UWorld *World, const TSet<int32> &Invited)
{
	const UWoRSessionSubsystem *Sessions = SessionsOf(World);
	const bool bHosting = Sessions && Sessions->IsHosting();
	TArray<FInviteSeatRow> Seats;
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	const URunStateComponent *Run = World ? URunStateComponent::Find(World) : nullptr;
	for (int32 SlotIndex = 0; Party && SlotIndex < Party->Members.Num(); ++SlotIndex)
	{
		const FPartyMember &Member = Party->Members[SlotIndex];
		const bool bLeader = SlotIndex == PartyConstants::LEADER_SLOT;
		const bool bAISeat = !bLeader && Member.bAISeat;
		const bool bHeld = bAISeat && Run && !Run->GetHeldFor(Member.MemberId).IsEmpty();
		const FString Claimant = bLeader || bAISeat ? FString() : FindClaimantName(World, Member);
		FInviteSeatRow &Seat = Seats.AddDefaulted_GetRef();
		Seat.Name = Member.CharacterData ? FText::FromString(Member.CharacterData->Name) : LOCTEXT("NoData", "<no data>");
		Seat.Who = GetSeatWho(bLeader, bAISeat, bHeld, Claimant, bHosting);
		Seat.bYours = !bAISeat && Claimant.IsEmpty();
	}
	return BuildPage(Sessions ? Sessions->GetFriendNames() : TArray<FString>(), Invited, bHosting, MoveTemp(Seats));
}

FText WoRInviteScreen::Invite(UWorld *World, int32 FriendIndex)
{
	UWoRSessionSubsystem *Sessions = SessionsOf(World);
	if (!Sessions)
	{
		return LOCTEXT("NoSessions", "No session service here.");
	}
	if (!Sessions->IsHosting())
	{
		return LOCTEXT("HostFirst", "Host a session first.");
	}
	return Sessions->InviteFriend(FriendIndex) ? FText::GetEmpty() : LOCTEXT("InviteFailed", "The invite couldn't be sent.");
}

FText WoRInviteScreen::ToggleHosting(UWorld *World)
{
	UWoRSessionSubsystem *Sessions = SessionsOf(World);
	if (!Sessions)
	{
		return LOCTEXT("NoSessions", "No session service here.");
	}
	if (Sessions->IsHosting())
	{
		Sessions->LeaveSession();
		return FText::GetEmpty();
	}
	// On success the level reopens as a listen server and takes this screen with it.
	return Sessions->HostSession() ? FText::GetEmpty() : LOCTEXT("HostFailed", "A session couldn't be hosted now.");
}

void WoRInviteScreen::ReadFriends(UWorld *World)
{
	if (UWoRSessionSubsystem *Sessions = SessionsOf(World))
	{
		Sessions->ReadFriends();
	}
}

FString WoRInviteScreen::GetInviteScreenString(UWorld *World)
{
	const FInviteScreenPage Page = BuildPage(World, TSet<int32>());
	FString Out = FString::Printf(TEXT("[InviteScreen] %s: %s"), *Page.Title.ToString(), *Page.Subtitle.ToString());
	for (const FInviteFriendRow &Friend : Page.Friends)
	{
		Out += FString::Printf(TEXT("\n  friend %s (%s)"), *Friend.Name.ToString(), *Friend.Status.ToString());
	}
	for (const FInviteSeatRow &Seat : Page.Seats)
	{
		Out += FString::Printf(TEXT("\n  seat %s: %s"), *Seat.Name.ToString(), *Seat.Who.ToString());
	}
	Out += TEXT("\n  ") + Page.SessionAction.ToString();
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GInviteScreenPrintCommand(
	TEXT("wor.UI.PrintInvite"),
	TEXT("B-459b: log what the Play together screen shows (friends, seats, the session row)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoRUI, Log, TEXT("%s"), *WoRInviteScreen::GetInviteScreenString(World));
	}));
#endif

#undef LOCTEXT_NAMESPACE
