// TrialPoolLookSpec.cpp
// B-565 (TrialPoolDesign stage 1, the look): what the kit card, the dropped line and the duplicate prompt say, and the
// prompt's choice, headless. Unproven here: how the three look on screen (PIE sheet B-565, built unseen).

#if WITH_DEV_AUTOMATION_TESTS

#include "Inventory/ItemTier.h"
#include "Misc/AutomationTest.h"
#include "Trial/TrialPoolLook.h"
#include "UI/Trial/DuplicateDropWidget.h"

BEGIN_DEFINE_SPEC(FTrialPoolLookSpec, "WoR.Trial.PoolLook", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTrialPoolLookSpec)

void FTrialPoolLookSpec::Define()
{
	It("says a dropped piece arrives empty or built, and gives a skill or crystal no such word", [this]()
	{
		FTrialDropView Drop = WoRTrialPoolLook::MakeDropExample();
		TestEqual(TEXT("empty"), WoRTrialPoolLook::GetDroppedLine(Drop).ToString(), FString(TEXT("Dropped: Arming Sword (D), empty")));
		Drop.bPrebuilt = true;
		TestEqual(TEXT("built (D-231)"), WoRTrialPoolLook::GetDroppedLine(Drop).ToString(), FString(TEXT("Dropped: Arming Sword (D), built")));
		Drop.bIsPiece = false;
		TestEqual(TEXT("no word"), WoRTrialPoolLook::GetDroppedLine(Drop).ToString(), FString(TEXT("Dropped: Arming Sword (D)")));
	});

	It("asks about a duplicate with both tiers and offers the three choices", [this]()
	{
		const FDuplicateDropView View = WoRTrialPoolLook::MakeDuplicateExample();
		TestEqual(TEXT("question"), WoRTrialPoolLook::GetDuplicateQuestion(View).ToString(), FString(TEXT("You already have Arming Sword (F). This one is D.")));
		TestEqual(TEXT("three choices"), static_cast<int32>(UE_ARRAY_COUNT(WoRTrialPoolLook::DUPLICATE_CHOICES)), 3);
		TestTrue(TEXT("convert names the essence"), WoRTrialPoolLook::GetDuplicateChoiceText(EDuplicateChoice::Convert, View).ToString().Contains(FString::FromInt(View.EssenceValue)));
		TestTrue(TEXT("Back keeps both (Q-565)"), WoRTrialPoolLook::DUPLICATE_BACK_CHOICE == EDuplicateChoice::Add);
	});

	It("shows an enemy's pieces and skills, and never a rank word (D-187)", [this]()
	{
		const FEnemyKitCard Card = WoRTrialPoolLook::MakeKitCardExample();
		TestEqual(TEXT("one piece"), Card.Pieces.Num(), 1);
		TestEqual(TEXT("the default three skills"), Card.Skills.Num(), 3);
		const FString Shown = WoRTrialPoolLook::GetKitCardString(Card);
		for (const TCHAR *Rank : {TEXT("Elite"), TEXT("Mini-boss"), TEXT("Boss")})
		{
			TestFalse(FString::Printf(TEXT("no %s"), Rank), Shown.Contains(Rank));
		}
	});

	It("hands the prompt's choice over once", [this]()
	{
		UDuplicateDropWidget *Prompt = NewObject<UDuplicateDropWidget>();
		int32 Calls = 0;
		EDuplicateChoice Got = EDuplicateChoice::Replace;
		Prompt->SetView(WoRTrialPoolLook::MakeDuplicateExample(), [&Calls, &Got](EDuplicateChoice Choice) { ++Calls; Got = Choice; });
		Prompt->MoveSelection(-1);
		TestEqual(TEXT("Up from the first wraps to Convert"), Prompt->GetSelectedChoice(), 2);
		Prompt->Choose(EDuplicateChoice::Convert);
		Prompt->Choose(EDuplicateChoice::Add);
		TestEqual(TEXT("once"), Calls, 1);
		TestTrue(TEXT("the first choice"), Got == EDuplicateChoice::Convert);
	});
}

#endif
