// JoinRequestCardSpec.cpp
// B-459c: what the host's join request card shows and the signal it follows. The card names who asks and with which
// character, lists the AI seats (held ones marked), and says why a seat can't be given now (a fight, a closed seat) and
// how many more wait; the run signals each request, answer and refusal; giving a seat seats the joiner. The corner card
// and the hold-to-answer key are the PIE sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/RunStateComponent.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "UI/Coop/JoinRequestModel.h"

namespace
{
	constexpr int32 JOIN_CARD_TEST_HOST_ID = 1;
	constexpr int32 JOIN_CARD_TEST_JOINER_ID = 2;
	constexpr int32 JOIN_CARD_TEST_AI_SLOT = 1;
	constexpr int32 JOIN_CARD_TEST_TWO_MORE = 2;

	FPartyMember JoinCardMember(UCharacterData *Character, bool bAISeat)
	{
		FPartyMember Member;
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Character;
		Member.bAISeat = bAISeat;
		Member.bPlayerControlled = !bAISeat;
		return Member;
	}

	TArray<FJoinRequestSeatRow> OneSeat()
	{
		FJoinRequestSeatRow Seat;
		Seat.Slot = JOIN_CARD_TEST_AI_SLOT;
		Seat.Name = FText::FromString(TEXT("Garrick"));
		Seat.Status = WoRJoinRequestCard::GetSeatStatus(false);
		return {Seat};
	}
}

BEGIN_DEFINE_SPEC(FJoinRequestCardSpec, "WoR.UI.JoinRequestCard", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	AWoRPlayerState *SpawnPlayer(UWorld *World, AWoRGameState *GameState, int32 PlayerId, const FString &Name)
	{
		AWoRPlayerState *Player = World->SpawnActor<AWoRPlayerState>();
		APlayerController *PC = World->SpawnActor<APlayerController>();
		if (!Player || !PC)
		{
			return nullptr;
		}
		if (APlayerState *AutoState = PC->PlayerState; AutoState && AutoState != Player)
		{
			GameState->RemovePlayerState(AutoState);
			AutoState->Destroy();
		}
		Player->SetPlayerId(PlayerId);
		Player->SetPlayerName(Name);
		PC->PlayerState = Player;
		GameState->AddPlayerState(Player);
		return Player;
	}
END_DEFINE_SPEC(FJoinRequestCardSpec)

void FJoinRequestCardSpec::Define()
{
	It("names who asks and with which character, and says why a seat can't be given now", [this]()
	{
		UCharacterData *Vael = FWoRTestWorld::NewCharacterData();
		Vael->Name = TEXT("Vael");
		const FJoinRequestCard Card = WoRJoinRequestCard::BuildCard(JOIN_CARD_TEST_JOINER_ID, TEXT("Kestrel"), Vael, OneSeat(), 0,
																	/*bSeatOpen=*/true, /*bInFight=*/false, /*bFocused=*/false);
		TestTrue(TEXT("shown"), Card.bShown);
		TestEqual(TEXT("who asks"), Card.Title.ToString(), FString(TEXT("Kestrel wants to join")));
		TestTrue(TEXT("with which character"), Card.WithLine.ToString().StartsWith(TEXT("with Vael · ")));
		TestEqual(TEXT("the seat replaced"), Card.ReplaceLine.ToString(), FString(TEXT("Vael replaces the seat's character for this run.")));
		TestTrue(TEXT("nothing in the way"), Card.Note.IsEmpty());
		TestTrue(TEXT("not answering: says how to"), Card.Hints.ToString().StartsWith(TEXT("Hold View")));

		const FJoinRequestCard Answering = WoRJoinRequestCard::BuildCard(JOIN_CARD_TEST_JOINER_ID, FString(), Vael, OneSeat(),
																		 JOIN_CARD_TEST_TWO_MORE, false, false, true);
		TestTrue(TEXT("no name: by id"), Answering.Title.ToString().Contains(TEXT("2")));
		TestTrue(TEXT("a closed seat says so"), Answering.Note.ToString().Contains(TEXT("rest spot")));
		TestTrue(TEXT("and how many more wait"), Answering.Note.ToString().Contains(TEXT("2 more waiting")));
		TestTrue(TEXT("answering: the keys"), Answering.Hints.ToString().StartsWith(TEXT("A")));

		const FJoinRequestCard InFight = WoRJoinRequestCard::BuildCard(JOIN_CARD_TEST_JOINER_ID, TEXT("Kestrel"), Vael, OneSeat(), 0, false, true, false);
		TestTrue(TEXT("a fight says to wait (not the rest spot)"), InFight.Note.ToString().Contains(TEXT("fight")) && !InFight.Note.ToString().Contains(TEXT("rest spot")));
		TestNotEqual(TEXT("a held seat is marked"), WoRJoinRequestCard::GetSeatStatus(true).ToString(), WoRJoinRequestCard::GetSeatStatus(false).ToString());
	});

	It("signals each request and answer, and shows the first request with the party's AI seats", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("Test world created"), TestWorld.Create())) return;
		UWorld *World = TestWorld.GetWorld();
		AWoRGameState *GameState = World->SpawnActor<AWoRGameState>();
		if (!TestNotNull(TEXT("GameState"), GameState)) return;
		World->SetGameState(GameState);
		URunStateComponent *Run = GameState->GetRunState();
		UPartySessionSubsystem *PartySession = TestWorld.GetSubsystem<UPartySessionSubsystem>();
		SpawnPlayer(World, GameState, JOIN_CARD_TEST_HOST_ID, TEXT("Host"));
		AWoRPlayerState *Joiner = SpawnPlayer(World, GameState, JOIN_CARD_TEST_JOINER_ID, TEXT("Kestrel"));
		if (!TestNotNull(TEXT("run"), Run) || !TestNotNull(TEXT("party session"), PartySession) || !TestNotNull(TEXT("joiner"), Joiner)) return;
		UCharacterData *Garrick = FWoRTestWorld::NewCharacterData();
		Garrick->Name = TEXT("Garrick");
		UCharacterData *Vael = FWoRTestWorld::NewCharacterData();
		Vael->Name = TEXT("Vael");
		PartySession->RestoreLocalParty(FText::FromString(TEXT("Coop")), false,
										{JoinCardMember(FWoRTestWorld::NewCharacterData(), false), JoinCardMember(Garrick, true)});
		Run->RefreshRoster();

		int32 Signals = 0;
		const FDelegateHandle Handle = Run->OnJoinRequestsChanged.AddLambda([&Signals]() { ++Signals; });
		TestFalse(TEXT("no request: hidden"), WoRJoinRequestCard::BuildCard(World, false).bShown);

		TestTrue(TEXT("Kestrel asks"), Run->RequestJoin(Joiner, Vael, TSoftClassPtr<ACombatCharacter>()));
		TestEqual(TEXT("the request signals"), Signals, 1);
		const FJoinRequestCard Card = WoRJoinRequestCard::BuildCard(World, false);
		TestTrue(TEXT("shown"), Card.bShown);
		TestEqual(TEXT("by name"), Card.Title.ToString(), FString(TEXT("Kestrel wants to join")));
		if (TestEqual(TEXT("one AI seat"), Card.Seats.Num(), 1))
		{
			TestEqual(TEXT("its slot"), Card.Seats[0].Slot, JOIN_CARD_TEST_AI_SLOT);
			TestEqual(TEXT("its character"), Card.Seats[0].Name.ToString(), FString(TEXT("Garrick")));
		}

		TestTrue(TEXT("refused"), WoRJoinRequestCard::Refuse(World, Card.PlayerId).IsEmpty());
		TestEqual(TEXT("the refusal signals"), Signals, 2);
		TestFalse(TEXT("then hidden"), WoRJoinRequestCard::BuildCard(World, false).bShown);
		TestFalse(TEXT("refusing again: a reason"), WoRJoinRequestCard::Refuse(World, Card.PlayerId).IsEmpty());

		TestTrue(TEXT("Kestrel asks again"), Run->RequestJoin(Joiner, Vael, TSoftClassPtr<ACombatCharacter>()));
		TestTrue(TEXT("given the seat (no run: anywhere)"), WoRJoinRequestCard::Give(World, JOIN_CARD_TEST_JOINER_ID, JOIN_CARD_TEST_AI_SLOT).IsEmpty());
		const UParty *Party = PartySession->GetLocalParty();
		TestTrue(TEXT("Vael has the seat"), Party && Party->Members.IsValidIndex(JOIN_CARD_TEST_AI_SLOT) &&
												Party->Members[JOIN_CARD_TEST_AI_SLOT].CharacterData == Vael);
		TestFalse(TEXT("answered: hidden"), WoRJoinRequestCard::BuildCard(World, false).bShown);
		Run->OnJoinRequestsChanged.Remove(Handle);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
