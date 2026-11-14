// GateScreenSpec.cpp
// B-466 (D-191, D-192): the Gate screen's model and its keys, headless. A trial row shows its name and line, never its
// boss; a locked one is "???" with how it unlocks, and opens once its quest is finished; three difficulty cards;
// Enter refuses a locked or missing trial and a gate with no level; an AI seat shows nothing for who plays it. The
// screen's look is the PIE sheet (docs/PIE/B-466.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Memory/HiddenQuestData.h"
#include "Memory/HiddenQuestSubsystem.h"
#include "Memory/MemoryConditions.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Trial/TrialData.h"
#include "UI/Trial/GateScreenModel.h"
#include "UI/Trial/GateScreenWidget.h"

namespace
{
	const FName GATE_SCREEN_TEST_QUEST(TEXT("GateScreenSpecQuest"));
	const FName GATE_SCREEN_TEST_SPOT(TEXT("Spot_GateScreenSpec"));
	const TCHAR *GATE_SCREEN_TEST_LEVEL = TEXT("/Game/GateScreenSpec/NoSuchMap.NoSuchMap");
	constexpr int32 GATE_SCREEN_TEST_TRIALS = 3;
	constexpr int32 GATE_SCREEN_TEST_LOCKED = 1;

	/** Two open trials around one locked by the test quest; the boss is named so the spec can check it never shows. */
	UTrialData *NewGate(const TCHAR *Name, bool bWithLevel)
	{
		UTrialData *Gate = NewObject<UTrialData>(GetTransientPackage());
		Gate->Name = FText::FromString(Name);
		if (bWithLevel)
		{
			Gate->Level = TSoftObjectPtr<UWorld>(FSoftObjectPath(GATE_SCREEN_TEST_LEVEL));
		}
		UCharacterData *Boss = FWoRTestWorld::NewCharacterData();
		Boss->Name = TEXT("GateSpecBoss");
		for (int32 Index = 0; Index < GATE_SCREEN_TEST_TRIALS; ++Index)
		{
			FTrialVariation &Trial = Gate->Variations.AddDefaulted_GetRef();
			Trial.Name = FText::FromString(FString::Printf(TEXT("Trial%d"), Index));
			Trial.Description = FText::FromString(FString::Printf(TEXT("Line%d"), Index));
			Trial.Boss = Boss;
		}
		FTrialVariation &Locked = Gate->Variations[GATE_SCREEN_TEST_LOCKED];
		Locked.UnlockQuestId = GATE_SCREEN_TEST_QUEST;
		Locked.UnlockHint = FText::FromString(TEXT("Unlocks with the quest: GateSpec"));
		return Gate;
	}

	/** One step at the test spot. */
	UHiddenQuestData *NewUnlockQuest()
	{
		UHiddenQuestData *Quest = NewObject<UHiddenQuestData>(GetTransientPackage());
		Quest->QuestId = GATE_SCREEN_TEST_QUEST;
		Quest->Kind = EQuestKind::Side;
		FWoRCondition There;
		There.Subject = EWoRConditionSubject::AtLocation;
		There.LocationId = GATE_SCREEN_TEST_SPOT;
		FHiddenQuestStep Step;
		Step.Conditions.Conditions.Add(There);
		Quest->Steps.Add(Step);
		return Quest;
	}

	FPartyMember GateMember(const TCHAR *Name, bool bAISeat)
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

BEGIN_DEFINE_SPEC(FGateScreenSpec, "WoR.UI.GateScreen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	UGameInstance *Game() const { return TestWorld->GetWorld()->GetGameInstance(); }
	void FinishUnlockQuest() const
	{
		UHiddenQuestSubsystem *Quests = TestWorld->GetSubsystem<UHiddenQuestSubsystem>();
		UHiddenQuestData *Quest = NewUnlockQuest();
		Quests->StartQuest(Quest);
		Quests->CheckEvent(FMemoryEvent(), {Quest});
		FMemoryEvent There;
		There.Locations.Add(GATE_SCREEN_TEST_SPOT);
		Quests->CheckEvent(There, {Quest});
	}
END_DEFINE_SPEC(FGateScreenSpec)

void FGateScreenSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
	});
	AfterEach([this]() { TestWorld.Reset(); });

	It("shows a trial's name and line, never its boss; a locked one as ??? with how it unlocks, open once its quest is done (D-191)", [this]()
	{
		UTrialData *Gate = NewGate(TEXT("GateSpecA"), true);
		FGateScreenPage Page = WoRGateScreen::BuildPage(TestWorld->GetWorld(), {Gate}, Gate);
		if (!TestEqual(TEXT("three trials"), Page.Trials.Num(), GATE_SCREEN_TEST_TRIALS)) return;
		TestTrue(TEXT("one tab, by name"), Page.Gates.Num() == 1 && Page.Gates[0].ToString() == TEXT("GateSpecA"));
		TestTrue(TEXT("name and line"), Page.Trials[0].Name.ToString() == TEXT("Trial0") && Page.Trials[0].Line.ToString() == TEXT("Line0"));
		const FGateTrialRow &Locked = Page.Trials[GATE_SCREEN_TEST_LOCKED];
		TestTrue(TEXT("locked: ??? and how"), Locked.bLocked && Locked.Name.ToString() == TEXT("???") &&
												  Locked.Line.ToString() == TEXT("Unlocks with the quest: GateSpec"));
		for (const FGateTrialRow &Row : Page.Trials)
		{
			TestFalse(TEXT("never the boss"), Row.Name.ToString().Contains(TEXT("GateSpecBoss")) || Row.Line.ToString().Contains(TEXT("GateSpecBoss")));
		}

		FinishUnlockQuest();
		Page = WoRGateScreen::BuildPage(TestWorld->GetWorld(), {Gate}, Gate);
		TestTrue(TEXT("open once its quest is finished"), !Page.Trials[GATE_SCREEN_TEST_LOCKED].bLocked &&
															  Page.Trials[GATE_SCREEN_TEST_LOCKED].Name.ToString() == TEXT("Trial1"));
	});

	It("has the three difficulties with their damage and payout; Hard pays more", [this]()
	{
		const FGateScreenPage Page = WoRGateScreen::BuildPage(TestWorld->GetWorld(), {}, nullptr);
		if (!TestEqual(TEXT("three cards"), Page.Difficulties.Num(), TrialGate::DIFFICULTY_COUNT)) return;
		const FGateDifficultyCard &Hard = Page.Difficulties[static_cast<int32>(ETrialDifficulty::Hard)];
		TestTrue(TEXT("lines"), Hard.DamageLine.ToString().StartsWith(TEXT("Enemy damage x")) && Hard.PayoutLine.ToString().StartsWith(TEXT("Payout x")));
		TestTrue(TEXT("Hard pays more, Medium doesn't"), Hard.bPaysMore && !Page.Difficulties[static_cast<int32>(ETrialDifficulty::Medium)].bPaysMore);
		TestFalse(TEXT("the wipes line"), Page.WipesLine.IsEmpty());
	});

	It("refuses Enter for a locked or missing trial and a gate with no level", [this]()
	{
		UTrialData *Gate = NewGate(TEXT("GateSpecB"), true);
		TestFalse(TEXT("no level"), WoRGateScreen::GetEnterRefusal(Game(), NewGate(TEXT("GateSpecNoLevel"), false), 0).IsEmpty());
		TestFalse(TEXT("no trial picked"), WoRGateScreen::GetEnterRefusal(Game(), Gate, INDEX_NONE).IsEmpty());
		TestFalse(TEXT("locked"), WoRGateScreen::GetEnterRefusal(Game(), Gate, GATE_SCREEN_TEST_LOCKED).IsEmpty());
		TestTrue(TEXT("an open trial goes"), WoRGateScreen::GetEnterRefusal(Game(), Gate, 0).IsEmpty());
	});

	It("lists who goes in; an AI seat shows nothing for who plays it (D-191)", [this]()
	{
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		if (!TestNotNull(TEXT("party session"), PartySession)) return;
		PartySession->RestoreLocalParty(FText::FromString(TEXT("Gate")), false, {GateMember(TEXT("Sera"), false), GateMember(TEXT("Bram"), true)});
		const FGateScreenPage Page = WoRGateScreen::BuildPage(TestWorld->GetWorld(), {}, nullptr);
		if (!TestEqual(TEXT("two members"), Page.Members.Num(), 2)) return;
		TestTrue(TEXT("you"), Page.Members[0].Name.ToString() == TEXT("Sera") && Page.Members[0].PlayedBy.ToString() == TEXT("You"));
		TestTrue(TEXT("the AI: nothing"), Page.Members[1].PlayedBy.IsEmpty());
	});

	It("the screen: picks the first open trial, refuses a locked one, steps the difficulty and wraps the gates (headless)", [this]()
	{
		UGateScreenWidget *Screen = CreateWidget<UGateScreenWidget>(TestWorld->GetWorld(), UGateScreenWidget::StaticClass());
		if (!TestNotNull(TEXT("screen"), Screen)) return;
		UTrialData *GateA = NewGate(TEXT("GateSpecA"), true);
		UTrialData *GateB = NewGate(TEXT("GateSpecB"), true);
		GateB->Variations[0].UnlockQuestId = GATE_SCREEN_TEST_QUEST; // B's first open trial is its third
		Screen->SetGates({GateA, GateB}, GateB);
		TestTrue(TEXT("opens on the door's gate"), Screen->GetGate() == GateB);
		TestEqual(TEXT("its first open trial is picked and lit"), Screen->GetChosenTrial(), GATE_SCREEN_TEST_TRIALS - 1);
		TestEqual(TEXT("lit"), Screen->GetFocus(), GATE_SCREEN_TEST_TRIALS - 1);

		Screen->MoveFocus(-1);
		Screen->PressSelect();
		TestTrue(TEXT("a locked trial isn't picked"), Screen->GetChosenTrial() == GATE_SCREEN_TEST_TRIALS - 1 && !Screen->GetMessage().IsEmpty());
		Screen->MoveFocus(1);
		Screen->PressSelect();
		TestTrue(TEXT("picking a trial moves to the difficulty"), Screen->GetFocus() == Screen->GetDifficultyRow() && Screen->GetMessage().IsEmpty());
		Screen->StepDifficulty(1);
		TestEqual(TEXT("Medium to Hard"), Screen->GetDifficulty(), ETrialDifficulty::Hard);
		Screen->StepDifficulty(1);
		TestEqual(TEXT("wraps to Easy"), Screen->GetDifficulty(), ETrialDifficulty::Easy);
		Screen->PressSelect();
		TestEqual(TEXT("A on the difficulty moves to Enter"), Screen->GetFocus(), Screen->GetEnterRow());

		Screen->MoveGate(1);
		TestTrue(TEXT("RB wraps to the first gate, its first trial picked"), Screen->GetGate() == GateA && Screen->GetChosenTrial() == 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
