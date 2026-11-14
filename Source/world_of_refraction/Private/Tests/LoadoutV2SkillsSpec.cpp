// LoadoutV2SkillsSpec.cpp
// B-476d (UIScreens.md #2-3 Skills tab): the Skills tab lists the Weapons tab's entries that have skills (not the
// character's evolution), each with its Skills page tab and "filled/total skills", then a Refractor's Innate spells
// (its school tabs, on points); a skill says what it does (name, EP); LB / RB turn Weapons | Skills | Apparel.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Skills/Definitions/AbilityData.h"
#include "UI/Menus/LoadoutV2ScreenWidget.h"
#include "UI/Menus/LoadoutV2SkillsModel.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 V2_SKILLS_WEAPON_ROWS = 3;
	constexpr int32 V2_SKILLS_WEAPON_FILLED = 2;
	constexpr int32 V2_SKILLS_SCHOOL_ROWS = 6;
	constexpr int32 V2_SKILLS_POINTS = 18;
	constexpr int32 V2_SKILLS_MAX_POINTS = 24;
	constexpr int32 V2_SKILLS_EP = 4;

	FLoadoutSkillTab V2SkillsTab(ELoadoutSkillList List, ELoadoutScreenPlace Place, int32 AdapterIndex, int32 Rows, int32 Filled)
	{
		FLoadoutSkillTab Tab;
		Tab.List = List;
		Tab.Gear.Place = Place;
		Tab.Gear.AdapterIndex = AdapterIndex;
		Tab.Rows.SetNum(Rows);
		Tab.Filled = Filled;
		return Tab;
	}

	FLoadoutV2Entry V2SkillsEntry(ELoadoutV2EntryKind Kind, ELoadoutV2Place Place, int32 AdapterIndex, const TCHAR *Name)
	{
		FLoadoutV2Entry Entry;
		Entry.Kind = Kind;
		Entry.Place = Place;
		Entry.AdapterIndex = AdapterIndex;
		Entry.Name = FText::FromString(Name);
		return Entry;
	}
}

BEGIN_DEFINE_SPEC(FLoadoutV2SkillsSpec, "WoR.UI.LoadoutV2Skills", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FLoadoutV2SkillsSpec)

void FLoadoutV2SkillsSpec::Define()
{
	It("lists the entries with skills, each on its tab with filled/total, then Innate spells on points", [this]()
	{
		FLoadoutSkillsPage Source;
		Source.Tabs.Add(V2SkillsTab(ELoadoutSkillList::Innate, ELoadoutScreenPlace::None, INDEX_NONE, V2_SKILLS_SCHOOL_ROWS, 1));
		Source.Tabs.Add(V2SkillsTab(ELoadoutSkillList::WeaponSkills, ELoadoutScreenPlace::Primary, INDEX_NONE, V2_SKILLS_WEAPON_ROWS, V2_SKILLS_WEAPON_FILLED));
		Source.Tabs.Add(V2SkillsTab(ELoadoutSkillList::AdapterSpells, ELoadoutScreenPlace::Adapter, 0, 1, 0));
		Source.Points = V2_SKILLS_POINTS;
		Source.MaxPoints = V2_SKILLS_MAX_POINTS;
		FLoadoutV2WeaponsPage Weapons;
		Weapons.Entries.Add(V2SkillsEntry(ELoadoutV2EntryKind::Evolution, ELoadoutV2Place::Seat, INDEX_NONE, TEXT("Ironheart")));
		Weapons.Entries.Add(V2SkillsEntry(ELoadoutV2EntryKind::Weapon, ELoadoutV2Place::Primary, INDEX_NONE, TEXT("Ashen Blade")));
		Weapons.Entries.Add(V2SkillsEntry(ELoadoutV2EntryKind::Adapter, ELoadoutV2Place::Adapter, 0, TEXT("Ember Ring")));

		const FLoadoutV2SkillsPage Page = WoRLoadoutV2Skills::Build(Source, Weapons);
		if (!TestEqual(TEXT("the blade, the ring, Innate spells (no evolution)"), Page.Entries.Num(), 3))
		{
			return;
		}
		TestTrue(TEXT("the blade on its tab"), Page.Entries[0].Tabs == TArray<int32>({1}));
		TestTrue(TEXT("filled/total in its header"), Page.Entries[0].Header.ToString().Contains(TEXT("2/3 skills")));
		TestTrue(TEXT("the ring on its adapter tab"), Page.Entries[1].Tabs == TArray<int32>({2}));
		TestTrue(TEXT("Innate spells last, on points"), Page.Entries[2].bInnate && Page.Entries[2].Header.ToString().Contains(TEXT("18/24")));
		TestEqual(TEXT("the blade's rows"), WoRLoadoutV2Skills::GetRows(Page, 0).Num(), V2_SKILLS_WEAPON_ROWS);
		TestEqual(TEXT("a school's rows"), WoRLoadoutV2Skills::GetRows(Page, 2).Num(), V2_SKILLS_SCHOOL_ROWS);
		TestTrue(TEXT("a bad entry has none"), WoRLoadoutV2Skills::GetRows(Page, 3).IsEmpty());
	});

	It("says what a skill does: its name, then EP", [this]()
	{
		TestTrue(TEXT("nothing without a skill"), WoRLoadoutV2Skills::BuildSkillDetails(nullptr, false).IsEmpty());
		UAbilityData *Skill = NewObject<UAbilityData>(GetTransientPackage());
		Skill->Name = TEXT("Flame Arc");
		Skill->BaseEnergyCost = V2_SKILLS_EP;
		const TArray<FLoadoutV2DetailLine> Lines = WoRLoadoutV2Skills::BuildSkillDetails(Skill, false);
		if (!TestTrue(TEXT("lines"), Lines.Num() >= 2))
		{
			return;
		}
		TestTrue(TEXT("the name first"), Lines[0].Kind == ELoadoutV2DetailKind::Piece && Lines[0].Label.ToString() == TEXT("Flame Arc"));
		TestTrue(TEXT("then EP"), Lines[1].Value.ToString() == FText::AsNumber(V2_SKILLS_EP).ToString());
	});

	It("turns Weapons, Skills, Apparel on LB / RB (the screen, headless)", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("world"), TestWorld.Create()))
		{
			return;
		}
		ULoadoutV2ScreenWidget *Screen = CreateWidget<ULoadoutV2ScreenWidget>(TestWorld.GetWorld(), ULoadoutV2ScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen"), Screen))
		{
			return;
		}
		Screen->ActivateWidget();
		TestFalse(TEXT("opens on Weapons"), Screen->IsOnSkillsTab());
		Screen->SwitchTab(1);
		TestTrue(TEXT("RB: Skills"), Screen->GetTab() == ELoadoutV2Tab::Skills && Screen->GetMode() == ELoadoutV2Mode::SkillEntries);
		Screen->SwitchTab(1);
		TestTrue(TEXT("RB: Apparel"), Screen->GetTab() == ELoadoutV2Tab::Apparel && Screen->GetMode() == ELoadoutV2Mode::List);
		Screen->SwitchTab(-1);
		Screen->SwitchTab(-1);
		TestTrue(TEXT("LB twice: back to Weapons"), Screen->GetTab() == ELoadoutV2Tab::Weapons && Screen->GetMode() == ELoadoutV2Mode::List);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
