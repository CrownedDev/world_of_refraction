// CharacterSheetSpec.cpp
// B-442 (D-181): the character sheet's model. The pillars carry their sub-stats in the grouping of CL-39, each
// with a value; resistance lists All, Physical and Spell and then the nine elements, and All is the Defence line
// (D-168); the two number formats; every sub-stat has a name. The screen is the PIE sheet (docs/PIE/B-442.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "UI/Menus/CharacterSheetModel.h"
#include "UI/Menus/CharacterSheetWidget.h"
#include "WoRGameNames.h"
#include "WoRTestWorld.h"

namespace
{
	constexpr int32 SHEET_TEST_PILLARS = 3;
	constexpr int32 SHEET_TEST_MIND_STATS = 4;
	constexpr int32 SHEET_TEST_BODY_STATS = 4;
	constexpr int32 SHEET_TEST_SPIRIT_STATS = 3;
	constexpr int32 SHEET_TEST_SCOPE_LINES = 3;
	constexpr int32 SHEET_TEST_ELEMENT_LINES = 9;
	constexpr int32 SHEET_TEST_BODY = 1;
	constexpr int32 SHEET_TEST_DEFENCE_ROW = 0;
	constexpr int32 SHEET_TEST_RAW_DAMAGE_ROW = 2;
	constexpr int32 SHEET_TEST_RESISTANCES = 3;
	constexpr int32 SHEET_TEST_CURSES = 4;
	constexpr int32 SHEET_TEST_ELEMENT_LINE = 4; // past the three scope lines
	constexpr float SHEET_TEST_WEAKNESS = -0.25f;
}

BEGIN_DEFINE_SPEC(FCharacterSheetSpec, "WoR.UI.CharacterSheet", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FCharacterSheetSpec)

void FCharacterSheetSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
	});
	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("formats a multiplier and a share the same way everywhere", [this]()
	{
		TestTrue(TEXT("x1.48"), WoRCharacterSheet::FormatMultiplier(1.48f).ToString().Contains(TEXT("1.48")));
		TestEqual(TEXT("a resisted type"), WoRCharacterSheet::FormatPercent(0.4f, true).ToString(), FString(TEXT("+40%")));
		TestEqual(TEXT("a weakness"), WoRCharacterSheet::FormatPercent(-0.25f, true).ToString(), FString(TEXT("-25%")));
		TestEqual(TEXT("nothing"), WoRCharacterSheet::FormatPercent(0.0f, true).ToString(), FString(TEXT("0%")));
		TestEqual(TEXT("unsigned"), WoRCharacterSheet::FormatPercent(0.09f, false).ToString(), FString(TEXT("9%")));
	});

	It("groups the sub-stats under Mind, Body and Spirit, each with a value (CL-39)", [this]()
	{
		AActor *Actor = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		const UCharacterDataComponent *Character = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("character"), Character))
		{
			return;
		}
		const TArray<FCharacterSheetPillar> Pillars = WoRCharacterSheet::BuildPillars(*Character);
		if (!TestEqual(TEXT("three pillars"), Pillars.Num(), SHEET_TEST_PILLARS))
		{
			return;
		}
		const int32 Expected[] = {SHEET_TEST_MIND_STATS, SHEET_TEST_BODY_STATS, SHEET_TEST_SPIRIT_STATS};
		for (int32 Index = 0; Index < Pillars.Num(); ++Index)
		{
			TestFalse(TEXT("named"), Pillars[Index].Name.IsEmpty());
			TestEqual(TEXT("its sub-stats"), Pillars[Index].Stats.Num(), Expected[Index]);
			TestEqual(TEXT("out of the live cap"), Pillars[Index].MaxLevel, WorldStatConstants::LIVE_MAX);
			for (const FCharacterSheetStat &Stat : Pillars[Index].Stats)
			{
				TestFalse(TEXT("a sub-stat is named"), Stat.Label.IsEmpty());
				TestFalse(TEXT("and has a value"), Stat.Value.IsEmpty());
			}
		}
	});

	It("lists All, Physical and Spell, then the nine elements, with All equal to the Defence line (D-168)", [this]()
	{
		AActor *Actor = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		const UCharacterDataComponent *Character = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("character"), Character))
		{
			return;
		}
		int32 ScopeLines = 0;
		const TArray<FCharacterSheetResistance> Lines = WoRCharacterSheet::BuildResistances(Actor, ScopeLines);
		TestEqual(TEXT("three scope lines"), ScopeLines, SHEET_TEST_SCOPE_LINES);
		if (!TestEqual(TEXT("then nine elements"), Lines.Num(), SHEET_TEST_SCOPE_LINES + SHEET_TEST_ELEMENT_LINES))
		{
			return;
		}
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			TestFalse(TEXT("named"), Lines[Index].Label.IsEmpty());
			TestEqual(TEXT("only the element lines carry an element"), Lines[Index].Element != ESpellElement::None, Index >= ScopeLines);
		}
		const TArray<FCharacterSheetPillar> Pillars = WoRCharacterSheet::BuildPillars(*Character);
		if (TestTrue(TEXT("Body has Defence first"), Pillars.IsValidIndex(SHEET_TEST_BODY) && Pillars[SHEET_TEST_BODY].Stats.IsValidIndex(SHEET_TEST_DEFENCE_ROW)))
		{
			TestEqual(TEXT("All is the Defence number"), WoRCharacterSheet::FormatPercent(Lines[0].Fraction, false).ToString(),
					  Pillars[SHEET_TEST_BODY].Stats[SHEET_TEST_DEFENCE_ROW].Value.ToString());
		}
	});

	It("names every sub-stat", [this]()
	{
		const UEnum *Stats = StaticEnum<ESubStat>();
		for (int32 Index = 0; Index < Stats->NumEnums() - 1; ++Index) // the last is _MAX
		{
			const ESubStat Stat = static_cast<ESubStat>(Stats->GetValueByIndex(Index));
			TestEqual(*FString::Printf(TEXT("%s is named unless it is None"), *Stats->GetNameStringByIndex(Index)),
					  WoRGameNames::GetSubStatName(Stat).IsEmpty(), Stat == ESubStat::None);
		}
	});

	It("says so when there is no party, and the screen closes on Back (headless)", [this]()
	{
		TestFalse(TEXT("no party: a reason"), WoRCharacterSheet::BuildPage(TestWorld->GetWorld(), 0).Refusal.IsEmpty());
		UCharacterSheetWidget *Screen = CreateWidget<UCharacterSheetWidget>(TestWorld->GetWorld(), UCharacterSheetWidget::StaticClass());
		if (!TestNotNull(TEXT("screen created"), Screen))
		{
			return;
		}
		Screen->ActivateWidget();
		TestTrue(TEXT("activated"), Screen->IsActivated());
		TestFalse(TEXT("the page says why it is empty"), Screen->GetPage().Refusal.IsEmpty());
		TestEqual(TEXT("B-635: no groups on a refused page"), Screen->GetGroups().Num(), 0);
		Screen->MoveMember(1);
		TestEqual(TEXT("nobody to move to"), Screen->GetMemberIndex(), 0);
		Screen->MoveInColumn(1);
		Screen->PressSelect();
		TestFalse(TEXT("B-635: nothing to select"), Screen->IsOnEntries());
		TestNull(TEXT("and nothing in Details"), Screen->GetLitEntry());
		Screen->PressBack();
		TestFalse(TEXT("Back closes it"), Screen->IsActivated());
	});

	It("B-635: builds the Profile's five groups, each sub-stat with its bar, what it does and where it comes from", [this]()
	{
		AActor *Actor = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		const UCharacterDataComponent *Character = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!TestNotNull(TEXT("character"), Character))
		{
			return;
		}
		FCharacterSheetPage Page;
		Page.Pillars = WoRCharacterSheet::BuildPillars(*Character);
		Page.Resistances = WoRCharacterSheet::BuildResistances(Actor, Page.ScopeLineCount);
		Page.Resistances[SHEET_TEST_ELEMENT_LINE].Fraction = SHEET_TEST_WEAKNESS;
		Page.Curses.Add({FText::FromString(TEXT("Hex")), FText::FromString(TEXT("A test curse."))});
		const TArray<FCharacterSheetGroup> Groups = WoRCharacterSheet::BuildGroups(Page);
		if (!TestEqual(TEXT("Mind, Body, Spirit, Resistances, Curses"), Groups.Num(), WoRCharacterSheet::GROUP_COUNT))
		{
			return;
		}
		for (int32 Index = 0; Index < SHEET_TEST_PILLARS; ++Index)
		{
			const FCharacterSheetGroup &Group = Groups[Index];
			TestEqual(TEXT("a pillar group is its pillar"), Group.Name.ToString(), Page.Pillars[Index].Name.ToString());
			TestEqual(TEXT("its row's number is the level"), Group.Count.ToString(), FText::AsNumber(Page.Pillars[Index].Level).ToString());
			TestFalse(TEXT("its row line"), Group.Blurb.IsEmpty());
			TestFalse(TEXT("its heading line"), Group.Subtitle.IsEmpty());
			TestEqual(TEXT("its sub-stats"), Group.Entries.Num(), Page.Pillars[Index].Stats.Num());
			for (const FCharacterSheetStat &Stat : Group.Entries)
			{
				TestFalse(*FString::Printf(TEXT("%s says what it does"), *Stat.Label.ToString()), Stat.Description.IsEmpty());
				TestTrue(*FString::Printf(TEXT("%s's bar is 0..1"), *Stat.Label.ToString()), Stat.Fill >= 0.0f && Stat.Fill <= 1.0f);
			}
		}
		const FCharacterSheetStat &RawDamage = Groups[SHEET_TEST_BODY].Entries[SHEET_TEST_RAW_DAMAGE_ROW];
		if (TestTrue(TEXT("Raw Damage has its layers"), RawDamage.Sources.Num() > 0))
		{
			TestTrue(TEXT("the first is its pillar and level"), RawDamage.Sources[0].Label.ToString().StartsWith(Page.Pillars[SHEET_TEST_BODY].Name.ToString()));
		}
		const FCharacterSheetGroup &Resistances = Groups[SHEET_TEST_RESISTANCES];
		TestTrue(TEXT("Resistances has no number"), Resistances.Count.IsEmpty());
		if (TestEqual(TEXT("a line per resistance"), Resistances.Entries.Num(), Page.Resistances.Num()))
		{
			TestTrue(TEXT("a line below zero is a weakness"), Resistances.Entries[SHEET_TEST_ELEMENT_LINE].bWeak);
			TestEqual(TEXT("All is weak only below zero"), Resistances.Entries[0].bWeak, Page.Resistances[0].Fraction < 0.0f);
		}
		const FCharacterSheetGroup &Curses = Groups[SHEET_TEST_CURSES];
		TestEqual(TEXT("Curses counts the curses held"), Curses.Count.ToString(), FString(TEXT("1")));
		if (TestEqual(TEXT("one entry per curse"), Curses.Entries.Num(), 1))
		{
			TestTrue(TEXT("a curse has no number, only its text"), Curses.Entries[0].Value.IsEmpty() && !Curses.Entries[0].Description.IsEmpty());
		}
		FCharacterSheetPage Refused;
		Refused.Refusal = FText::FromString(TEXT("Not here."));
		TestEqual(TEXT("a refused page has no groups"), WoRCharacterSheet::BuildGroups(Refused).Num(), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
