// FallenMembersSpec.cpp
// B-150 (D-043): a member dead at a fight's end stays in the party, marked fallen, and sits out
// later fights; a rest spot (ReviveFallenMembers) brings the fallen back at full HP.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"

namespace
{
	constexpr int32 FALLEN_TEST_MEMBERS = 3;
	constexpr int32 FALLEN_TEST_SLOT = 1;
	constexpr int32 FALLEN_TEST_WOUNDED_HP = 1;
}

BEGIN_DEFINE_SPEC(FFallenMembersSpec, "WoR.Party.Fallen",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPartySessionSubsystem *PartySession = nullptr;
	UParty *Party = nullptr;
END_DEFINE_SPEC(FFallenMembersSpec)

void FFallenMembersSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		TestNotNull(TEXT("PartySession"), PartySession);
		Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		TestNotNull(TEXT("Party"), Party);
		if (!Party)
		{
			return;
		}
		for (int32 Slot = 0; Slot < FALLEN_TEST_MEMBERS; ++Slot)
		{
			FPartyMember Member;
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = FWoRTestWorld::NewCharacterData();
			Party->Members.Add(Member);
		}
		PartySession->SetAllMembersFull();
		// The fight's end banks a dead member's zero (ABattleGameMode::HandleCombatEnd).
		PartySession->SetMemberVitals(FALLEN_TEST_SLOT, 0, 0);
	});

	AfterEach([this]()
	{
		Party = nullptr;
		PartySession = nullptr;
		TestWorld.Reset();
	});

	It("keeps a member dead at a fight's end in the party, marked fallen", [this]()
	{
		if (!Party) return;
		TestEqual(TEXT("still in the party"), Party->GetMemberCount(), FALLEN_TEST_MEMBERS);
		TestTrue(TEXT("marked fallen"), PartySession->IsMemberFallen(FALLEN_TEST_SLOT));
		TestFalse(TEXT("the others are not"), PartySession->IsMemberFallen(0));
	});

	It("skips a fallen member in the next fight's roster", [this]()
	{
		if (!Party) return;
		const TArray<UCharacterData *> Roster = Party->GetLivingMemberCharacterData();
		TestEqual(TEXT("two fight"), Roster.Num(), FALLEN_TEST_MEMBERS - 1);
		TestFalse(TEXT("not the fallen one"), Roster.Contains(Party->Members[FALLEN_TEST_SLOT].CharacterData.Get()));
	});

	It("does not count a member with no carried vitals as fallen", [this]()
	{
		if (!Party) return;
		PartySession->ClearAllMemberVitals();
		TestFalse(TEXT("no carry is not dead"), PartySession->IsMemberFallen(FALLEN_TEST_SLOT));
	});

	It("brings the fallen back at full HP at a rest spot, leaving the standing as they are", [this]()
	{
		if (!Party) return;
		PartySession->SetMemberVitals(0, FALLEN_TEST_WOUNDED_HP, 0);
		TestEqual(TEXT("one revived"), PartySession->ReviveFallenMembers(), 1);

		const FPartyMember &Revived = Party->Members[FALLEN_TEST_SLOT];
		TestFalse(TEXT("no longer fallen"), Revived.IsFallen());
		TestEqual(TEXT("full HP"), Revived.CurrentHP, Revived.CharacterData->CalculateMaxHealth());
		TestEqual(TEXT("fights again"), Party->GetLivingMemberCharacterData().Num(), FALLEN_TEST_MEMBERS);
		TestEqual(TEXT("a wounded standing member is not healed"), Party->Members[0].CurrentHP, FALLEN_TEST_WOUNDED_HP);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
