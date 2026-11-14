// InviteScreenSpec.cpp
// B-459b: the Play together screen's model. Friends (one invited reads "invited · waiting"; no list says how a friend
// finds the session), each seat's player (you as host, a friend by name, an AI seat, one held), the session row, and an
// invite refused before hosting. The screen itself is the PIE sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "UI/Coop/InviteScreenModel.h"

namespace
{
	constexpr int32 INVITE_TEST_INVITED = 1;
}

BEGIN_DEFINE_SPEC(FInviteScreenSpec, "WoR.UI.InviteScreen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FInviteScreenSpec)

void FInviteScreenSpec::Define()
{
	It("names who plays each seat", [this]()
	{
		TestEqual(TEXT("the host's own seat"), WoRInviteScreen::GetSeatWho(true, false, false, FString(), true).ToString(), FString(TEXT("You (host)")));
		TestEqual(TEXT("solo"), WoRInviteScreen::GetSeatWho(true, false, false, FString(), false).ToString(), FString(TEXT("You")));
		TestEqual(TEXT("an AI seat"), WoRInviteScreen::GetSeatWho(false, true, false, FString(), true).ToString(), FString(TEXT("AI seat")));
		TestTrue(TEXT("a held AI seat"), WoRInviteScreen::GetSeatWho(false, true, true, FString(), true).ToString().Contains(TEXT("held")));
		TestEqual(TEXT("a friend's seat"), WoRInviteScreen::GetSeatWho(false, false, false, TEXT("Kestrel"), true).ToString(), FString(TEXT("Kestrel")));
	});

	It("lists friends, marks the invited, and offers the session row", [this]()
	{
		const FInviteScreenPage Hosting = WoRInviteScreen::BuildPage({TEXT("Kestrel"), TEXT("Dune")}, {INVITE_TEST_INVITED}, true, {});
		if (!TestEqual(TEXT("two friends"), Hosting.Friends.Num(), 2)) return;
		TestFalse(TEXT("Kestrel not invited"), Hosting.Friends[0].bInvited);
		TestTrue(TEXT("Dune invited"), Hosting.Friends[INVITE_TEST_INVITED].bInvited);
		TestTrue(TEXT("and waiting"), Hosting.Friends[INVITE_TEST_INVITED].Status.ToString().Contains(TEXT("waiting")));
		TestEqual(TEXT("hosting: stop"), Hosting.SessionAction.ToString(), FString(TEXT("Stop hosting")));

		const FInviteScreenPage Lan = WoRInviteScreen::BuildPage({}, {}, false, {});
		TestEqual(TEXT("not hosting: host"), Lan.SessionAction.ToString(), FString(TEXT("Host a session")));
		TestTrue(TEXT("no list: how a friend finds the session"), Lan.FriendsNote.ToString().Contains(TEXT("wor.Session.Find")));
	});

	It("shows the local party's seats and refuses an invite before hosting", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("Test world created"), TestWorld.Create())) return;
		UPartySessionSubsystem *PartySession = TestWorld.GetSubsystem<UPartySessionSubsystem>();
		if (!TestNotNull(TEXT("party"), PartySession)) return;
		FPartyMember Leader;
		Leader.PawnClass = ACombatCharacter::StaticClass();
		Leader.CharacterData = FWoRTestWorld::NewCharacterData();
		Leader.CharacterData->Name = TEXT("Sera");
		FPartyMember AI = Leader;
		AI.CharacterData = FWoRTestWorld::NewCharacterData();
		AI.CharacterData->Name = TEXT("Bram");
		AI.bAISeat = true;
		AI.bPlayerControlled = false;
		PartySession->RestoreLocalParty(FText::FromString(TEXT("Invite")), false, {Leader, AI});

		const FInviteScreenPage Page = WoRInviteScreen::BuildPage(TestWorld.GetWorld(), {});
		if (!TestEqual(TEXT("two seats"), Page.Seats.Num(), 2)) return;
		TestEqual(TEXT("the leader is yours"), Page.Seats[0].Who.ToString(), FString(TEXT("You")));
		TestEqual(TEXT("Bram is an AI seat"), Page.Seats[1].Who.ToString(), FString(TEXT("AI seat")));
		TestFalse(TEXT("an invite before hosting is refused"), WoRInviteScreen::Invite(TestWorld.GetWorld(), 0).IsEmpty());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
