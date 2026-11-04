// PartySeatsSpec.cpp
// B-346 (Q-21-audit, the deckbuilder's seat rule): who commands a party member's turn (SeatRules).
// Solo, the one player commands every member that isn't an AI seat; an AI seat is AI on every turn;
// in co-op a claimed member is its player's and an unclaimed member's turns rotate through the side's
// players in seat order. Headless: the co-op seats themselves come with B-232.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Party/SeatRules.h"

namespace
{
	constexpr int32 SEATS_TEST_SOLO_PLAYER = 7;
	constexpr int32 SEATS_TEST_PLAYER_A = 1;
	constexpr int32 SEATS_TEST_PLAYER_B = 2;
	constexpr int32 SEATS_TEST_GONE_PLAYER = 9;
	constexpr int32 SEATS_TEST_TURNS = 6;
}

BEGIN_DEFINE_SPEC(FPartySeatsSpec, "WoR.Party.Seats",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FPartySeatsSpec)

void FPartySeatsSpec::Define()
{
	It("solo: the player commands every member on every turn", [this]()
	{
		const TArray<int32> Solo = {SEATS_TEST_SOLO_PLAYER};
		for (int32 Turn = 0; Turn < SEATS_TEST_TURNS; ++Turn)
		{
			TestEqual(FString::Printf(TEXT("turn %d, unclaimed member"), Turn),
					  SeatRules::GetCommander(false, SeatRules::UNCLAIMED, Solo, Turn), SEATS_TEST_SOLO_PLAYER);
		}
	});

	It("an AI seat is AI on every turn", [this]()
	{
		const TArray<int32> Players = {SEATS_TEST_PLAYER_A, SEATS_TEST_PLAYER_B};
		for (int32 Turn = 0; Turn < SEATS_TEST_TURNS; ++Turn)
		{
			TestEqual(FString::Printf(TEXT("turn %d"), Turn), SeatRules::GetCommander(true, SEATS_TEST_PLAYER_A, Players, Turn),
					  SeatRules::AI_COMMANDER);
		}
		TestFalse(TEXT("an AI seat takes no rotated turns"), SeatRules::UsesRotation(true, SeatRules::UNCLAIMED));
	});

	It("co-op: an unclaimed member's turns rotate through the side's players in seat order", [this]()
	{
		const TArray<int32> Players = {SEATS_TEST_PLAYER_A, SEATS_TEST_PLAYER_B};
		TestTrue(TEXT("unclaimed uses the rotation"), SeatRules::UsesRotation(false, SeatRules::UNCLAIMED));
		int32 Rotation = 0;
		for (int32 Turn = 0; Turn < SEATS_TEST_TURNS; ++Turn)
		{
			const int32 Expected = Turn % 2 == 0 ? SEATS_TEST_PLAYER_A : SEATS_TEST_PLAYER_B;
			TestEqual(FString::Printf(TEXT("rotated turn %d"), Turn),
					  SeatRules::GetCommander(false, SeatRules::UNCLAIMED, Players, Rotation), Expected);
			++Rotation;
		}
	});

	It("co-op: a claimed member is its player's; a claim by someone gone falls back to the rotation", [this]()
	{
		const TArray<int32> Players = {SEATS_TEST_PLAYER_A, SEATS_TEST_PLAYER_B};
		TestFalse(TEXT("a claimed member takes no rotated turns"), SeatRules::UsesRotation(false, SEATS_TEST_PLAYER_B));
		TestEqual(TEXT("claimed by B"), SeatRules::GetCommander(false, SEATS_TEST_PLAYER_B, Players, 0), SEATS_TEST_PLAYER_B);
		TestEqual(TEXT("claimed by a player who left"), SeatRules::GetCommander(false, SEATS_TEST_GONE_PLAYER, Players, 1),
				  SEATS_TEST_PLAYER_B);
		TestEqual(TEXT("a side with no players is AI"), SeatRules::GetCommander(false, SeatRules::UNCLAIMED, TArray<int32>(), 0),
				  SeatRules::AI_COMMANDER);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
