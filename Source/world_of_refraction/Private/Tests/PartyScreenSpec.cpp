// PartyScreenSpec.cpp
// B-458: the party screen's model. Each member's row (an AI shows no level text, D-200), the leader's seat stays
// yours, another member's seat toggles between you and the AI, and X steps through the 3 x 3 start cells. The screen
// itself is the PIE sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Character/EWorldPillar.h"
#include "Trial/LevelDropPlacement.h"
#include "Combat/Grid/CombatGridConstants.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "UI/Menus/LoadoutV2SkillsModel.h"
#include "UI/Party/PartyScreenModel.h"

namespace
{
	constexpr int32 PARTY_SCREEN_TEST_AI_SLOT = 1;
	constexpr int32 PARTY_SCREEN_TEST_LAST_COLUMN = CombatGridConstants::GRID_COLUMNS - 1;
	constexpr int32 PARTY_SCREEN_TEST_SLOTS = 2;
	constexpr int32 PARTY_SCREEN_TEST_CARRIED_HP = 5;

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

	It("gives each seat its HP and EP, full with none carried, and its Mind / Body / Spirit (B-628b)", [this]()
	{
		if (!TestNotNull(TEXT("party session"), PartySession)) return;
		UParty *Party = PartySession->GetLocalParty();
		if (!TestTrue(TEXT("two members"), Party && Party->Members.Num() == PARTY_SCREEN_TEST_SLOTS)) return;
		Party->Members[PARTY_SCREEN_TEST_AI_SLOT].CurrentHP = PARTY_SCREEN_TEST_CARRIED_HP;
		Party->Members[PARTY_SCREEN_TEST_AI_SLOT].CurrentEP = 0;
		const FPartyScreenPage Page = WoRPartyScreen::BuildPage(TestWorld->GetWorld());
		if (!TestEqual(TEXT("two rows"), Page.Members.Num(), PARTY_SCREEN_TEST_SLOTS)) return;
		const FPartyScreenMember &Leader = Page.Members[0];
		const FPartyScreenMember &Carried = Page.Members[PARTY_SCREEN_TEST_AI_SLOT];
		TestTrue(TEXT("the maximums can be read"), Leader.MaxHP > 0 && Leader.MaxEP > 0);
		TestEqual(TEXT("no carried HP: full"), Leader.HP, Leader.MaxHP);
		TestEqual(TEXT("no carried EP: full"), Leader.EP, Leader.MaxEP);
		TestEqual(TEXT("carried HP shows as carried"), Carried.HP, PARTY_SCREEN_TEST_CARRIED_HP);
		TestEqual(TEXT("carried EP shows as carried"), Carried.EP, 0);
		TestEqual(TEXT("Mind is the live level"), Leader.Mind, WoRLevelDrops::GetLiveLevel(TestWorld->GetWorld(), Party->Members[0], EWorldPillar::Mind));
		TestEqual(TEXT("Body is the live level"), Leader.Body, WoRLevelDrops::GetLiveLevel(TestWorld->GetWorld(), Party->Members[0], EWorldPillar::Body));
		TestEqual(TEXT("Spirit is the live level"), Leader.Spirit, WoRLevelDrops::GetLiveLevel(TestWorld->GetWorld(), Party->Members[0], EWorldPillar::Spirit));
		// The test world spawns no member, so there is no loadout to read: the weapon line is empty. With one, it is the first piece.
		const FPartyScreenLoadout LeaderLoadout = WoRPartyScreen::BuildLoadout(TestWorld->GetWorld(), 0);
		TestEqual(TEXT("the weapon is the active loadout's first piece, or nothing"), Leader.Weapon.ToString(),
				  LeaderLoadout.Entries.IsEmpty() ? FString() : LeaderLoadout.Entries[0].Name.ToString());
		const FPartyScreenPage NoWorld = WoRPartyScreen::BuildPage(Party, false);
		TestTrue(TEXT("with no world: the vitals, no pillars or weapon"), NoWorld.Members[0].MaxHP == Leader.MaxHP &&
				 NoWorld.Members[0].Mind == 0 && NoWorld.Members[0].Weapon.IsEmpty());
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
		TestEqual(TEXT("B-628: Details names the row and the place"), Moved.StartValue.ToString(), FString(TEXT("Back Row, top")));
	});

	It("B-628: lists the active loadout's pieces with their slotted skills, then Innate spells", [this]()
	{
		FLoadoutV2WeaponsPage Weapons;
		FLoadoutV2Entry &Evolution = Weapons.Entries.AddDefaulted_GetRef();
		Evolution.Kind = ELoadoutV2EntryKind::Evolution;
		Evolution.Place = ELoadoutV2Place::Seat;
		Evolution.Name = FText::FromString(TEXT("Ironheart"));
		FLoadoutV2Entry &Sword = Weapons.Entries.AddDefaulted_GetRef();
		Sword.Place = ELoadoutV2Place::Primary;
		Sword.Name = FText::FromString(TEXT("Arming Sword"));
		Sword.TypeIcon.Name = FText::FromString(TEXT("Sword"));

		FLoadoutSkillsPage Source;
		FLoadoutSkillTab &School = Source.Tabs.AddDefaulted_GetRef();
		School.List = ELoadoutSkillList::Innate;
		School.Rows.SetNum(PARTY_SCREEN_TEST_SLOTS);
		FLoadoutSkillTab &Blade = Source.Tabs.AddDefaulted_GetRef();
		Blade.List = ELoadoutSkillList::WeaponSkills;
		Blade.Gear.Place = ELoadoutScreenPlace::Primary;
		Blade.Rows.SetNum(PARTY_SCREEN_TEST_SLOTS);
		Blade.Rows[0].Skill = NewObject<UAbilityData>();
		Blade.Rows[0].Name = FText::FromString(TEXT("Force Strike"));
		Blade.Rows[1].Skill = NewObject<UAbilityData>();
		Blade.Rows[1].Name = FText::FromString(TEXT("Rally"));
		Blade.Filled = PARTY_SCREEN_TEST_SLOTS;

		const FPartyScreenLoadout Loadout = WoRPartyScreen::BuildLoadout(FText::FromString(TEXT("Bruiser (1 of 1)")), Weapons,
																		 WoRLoadoutV2Skills::Build(Source, Weapons));
		if (!TestEqual(TEXT("the evolution, the sword, Innate spells"), Loadout.Entries.Num(), 3)) return;
		TestEqual(TEXT("the title"), Loadout.Title.ToString(), FString(TEXT("Bruiser (1 of 1)")));
		TestTrue(TEXT("the evolution has no skill slots"), Loadout.Entries[0].SkillsLine.IsEmpty());
		TestEqual(TEXT("the sword's name"), Loadout.Entries[1].Name.ToString(), FString(TEXT("Arming Sword")));
		TestTrue(TEXT("its type and tier"), Loadout.Entries[1].TypeLine.ToString().StartsWith(TEXT("Sword")));
		TestEqual(TEXT("its slotted skills"), Loadout.Entries[1].SkillsLine.ToString(), FString(TEXT("Force Strike, Rally")));
		TestEqual(TEXT("an empty entry says so"), Loadout.Entries[2].SkillsLine.ToString(), FString(TEXT("No skills slotted")));
		TestTrue(TEXT("no member, no loadout"), WoRPartyScreen::BuildLoadout(TestWorld->GetWorld(), PartyConstants::MAX_PARTY_MEMBERS).Entries.IsEmpty());
	});

	It("B-628: dismisses a member but never the leader", [this]()
	{
		if (!PartySession) return;
		TestFalse(TEXT("the leader is refused, with a reason"), WoRPartyScreen::Dismiss(TestWorld->GetWorld(), PartyConstants::LEADER_SLOT).IsEmpty());
		TestTrue(TEXT("the AI member leaves"), WoRPartyScreen::Dismiss(TestWorld->GetWorld(), PARTY_SCREEN_TEST_AI_SLOT).IsEmpty());
		TestEqual(TEXT("one member left, the leader"), WoRPartyScreen::BuildPage(TestWorld->GetWorld()).Members.Num(), 1);
		TestFalse(TEXT("a slot past the party is refused"), WoRPartyScreen::Dismiss(TestWorld->GetWorld(), PARTY_SCREEN_TEST_AI_SLOT).IsEmpty());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
