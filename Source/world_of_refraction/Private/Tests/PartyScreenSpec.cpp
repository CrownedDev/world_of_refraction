// PartyScreenSpec.cpp
// B-458: the party screen's model. Each member's row (an AI shows no level text, D-200), the leader's seat stays
// yours, another member's seat toggles between you and the AI, and X steps through the 3 x 3 start cells. The screen
// itself is the PIE sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Combat/Grid/CombatGridConstants.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "UI/Party/PartyScreenModel.h"

namespace
{
	constexpr int32 PARTY_SCREEN_TEST_AI_SLOT = 1;
	constexpr int32 PARTY_SCREEN_TEST_LAST_COLUMN = CombatGridConstants::GRID_COLUMNS - 1;

	FPartyMember PartyScreenMember(const TCHAR *Name, bool bAISeat)
	{
		FPartyMember Member;
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = FWoRTestWorld::NewCharacterData();
		Member.CharacterData->Name = Name;
		Member.bAISeat = bAISeat;
		Member.bPlayerControlled = !bAISeat;
		return Member;
	}
}

BEGIN_DEFINE_SPEC(FPartyScreenSpec, "WoR.UI.PartyScreen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPartySessionSubsystem *PartySession = nullptr;
END_DEFINE_SPEC(FPartyScreenSpec)

void FPartyScreenSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		if (PartySession)
		{
			PartySession->RestoreLocalParty(FText::FromString(TEXT("Screen")), false,
											{PartyScreenMember(TEXT("Sera"), false), PartyScreenMember(TEXT("Bram"), true)});
		}
	});

	AfterEach([this]()
	{
		PartySession = nullptr;
		TestWorld.Reset();
	});

	It("shows each member and who plays it; an AI has no level text (D-200)", [this]()
	{
		const FPartyScreenPage Page = WoRPartyScreen::BuildPage(TestWorld->GetWorld());
		if (!TestEqual(TEXT("two members"), Page.Members.Num(), 2)) return;
		TestEqual(TEXT("the leader's name"), Page.Members[0].Name.ToString(), FString(TEXT("Sera")));
		TestEqual(TEXT("the leader is yours"), Page.Members[0].PlayedBy.ToString(), FString(TEXT("You (leader)")));
		TestEqual(TEXT("the AI reads AI, nothing more"), Page.Members[1].PlayedBy.ToString(), FString(TEXT("AI")));
		TestTrue(TEXT("a field skill line"), Page.Members[1].FieldSkillLine.ToString().StartsWith(TEXT("Field skill: ")));
		TestFalse(TEXT("no session"), Page.InviteStatus.IsEmpty());
		TestEqual(TEXT("an empty page with no party"), WoRPartyScreen::BuildPage(nullptr, false).Members.Num(), 0);
	});

	It("keeps the leader's seat yours and toggles another member's seat", [this]()
	{
		if (!PartySession) return;
		TestFalse(TEXT("the leader is refused, with a reason"), WoRPartyScreen::ToggleSeat(TestWorld->GetWorld(), PartyConstants::LEADER_SLOT).IsEmpty());
		TestFalse(TEXT("the leader still yours"), PartySession->IsMemberAISeat(PartyConstants::LEADER_SLOT));
		TestTrue(TEXT("the AI seat changes"), WoRPartyScreen::ToggleSeat(TestWorld->GetWorld(), PARTY_SCREEN_TEST_AI_SLOT).IsEmpty());
		TestFalse(TEXT("now yours"), PartySession->IsMemberAISeat(PARTY_SCREEN_TEST_AI_SLOT));
		TestEqual(TEXT("and reads You"), WoRPartyScreen::BuildPage(TestWorld->GetWorld()).Members[PARTY_SCREEN_TEST_AI_SLOT].PlayedBy.ToString(),
				  FString(TEXT("You")));
	});

	It("steps through the 3 x 3 start cells, an unplaced member first to Back", [this]()
	{
		ECombatRow Row = ECombatRow::Middle;
		int32 Column = INDEX_NONE;
		WoRPartyScreen::GetNextCell(false, ECombatRow::Front, PARTY_SCREEN_TEST_LAST_COLUMN, Row, Column);
		TestTrue(TEXT("unplaced starts at Back, 0"), Row == ECombatRow::Back && Column == 0);
		WoRPartyScreen::GetNextCell(true, ECombatRow::Back, PARTY_SCREEN_TEST_LAST_COLUMN, Row, Column);
		TestTrue(TEXT("past a row's last column, the next row"), Row == ECombatRow::Middle && Column == 0);
		WoRPartyScreen::GetNextCell(true, ECombatRow::Front, PARTY_SCREEN_TEST_LAST_COLUMN, Row, Column);
		TestTrue(TEXT("the last cell wraps"), Row == ECombatRow::Back && Column == 0);

		if (!PartySession) return;
		WoRPartyScreen::MoveOnGrid(TestWorld->GetWorld(), PARTY_SCREEN_TEST_AI_SLOT);
		const FPartyScreenMember Moved = WoRPartyScreen::BuildPage(TestWorld->GetWorld()).Members[PARTY_SCREEN_TEST_AI_SLOT];
		TestTrue(TEXT("placed at Back, 0"), Moved.bHasCell && Moved.Row == ECombatRow::Back && Moved.Column == 0);
		TestTrue(TEXT("its start line names the row"), Moved.StartLine.ToString().Contains(TEXT("Back")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
