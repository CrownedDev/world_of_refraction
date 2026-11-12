// ActionPipsTextSpec.cpp
// B-170e (ActionPips.md §3.5, Q-170e2): what the placeholder UI shows of banked actions, from plain numbers. The pips
// (filled = the bank, empty up to Max, all filled past Max), the Max Actions line with its action-point progress
// ("Max 3 (+75/100)", no progress at the cap), a button's cost label, and the two greyed reasons (above Max: never;
// above the bank: not yet). Headless, no world.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Actions/ActionPipsRules.h"
#include "Combat/Actions/ActionPipsText.h"

namespace
{
	constexpr int32 PIPS_TEXT_NONE = 0;
	constexpr int32 PIPS_TEXT_ONE = 1;
	constexpr int32 PIPS_TEXT_TWO = 2;
	constexpr int32 PIPS_TEXT_THREE = 3;
	constexpr int32 PIPS_TEXT_FOUR = 4;
	constexpr int32 PIPS_TEXT_PROGRESS = 75;
}

BEGIN_DEFINE_SPEC(FActionPipsTextSpec, "WoR.Combat.ActionPipsText",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FActionPipsTextSpec)

void FActionPipsTextSpec::Define()
{
	Describe("GetCostState", [this]()
	{
		It("is affordable when the bank covers the cost", [this]()
		{
			TestTrue(TEXT("1 of 2"), ActionPipsText::GetCostState(PIPS_TEXT_ONE, PIPS_TEXT_TWO, PIPS_TEXT_TWO) == EActionCostState::Affordable);
			TestTrue(TEXT("2 of 2"), ActionPipsText::GetCostState(PIPS_TEXT_TWO, PIPS_TEXT_TWO, PIPS_TEXT_TWO) == EActionCostState::Affordable);
			TestTrue(TEXT("a cost of 0 with an empty bank"),
					 ActionPipsText::GetCostState(PIPS_TEXT_NONE, PIPS_TEXT_NONE, PIPS_TEXT_TWO) == EActionCostState::Affordable);
		});

		It("needs more when the cost is above the bank but within Max", [this]()
		{
			TestTrue(TEXT("2 with 1 banked, Max 2"),
					 ActionPipsText::GetCostState(PIPS_TEXT_TWO, PIPS_TEXT_ONE, PIPS_TEXT_TWO) == EActionCostState::NeedsMore);
		});

		It("is above Max when the cost can never be paid, whatever the bank", [this]()
		{
			TestTrue(TEXT("3 with Max 2"), ActionPipsText::GetCostState(PIPS_TEXT_THREE, PIPS_TEXT_ONE, PIPS_TEXT_TWO) == EActionCostState::AboveMax);
			TestTrue(TEXT("3 with Max 2 even with 4 banked past Max"),
					 ActionPipsText::GetCostState(PIPS_TEXT_THREE, PIPS_TEXT_FOUR, PIPS_TEXT_TWO) == EActionCostState::AboveMax);
		});
	});

	Describe("GetCostRefusedReason", [this]()
	{
		It("is empty when affordable and gives two different reasons otherwise", [this]()
		{
			TestTrue(TEXT("Affordable: no reason"), ActionPipsText::GetCostRefusedReason(PIPS_TEXT_ONE, PIPS_TEXT_ONE, PIPS_TEXT_TWO).IsEmpty());
			const FText Needs = ActionPipsText::GetCostRefusedReason(PIPS_TEXT_TWO, PIPS_TEXT_ONE, PIPS_TEXT_TWO);
			const FText AboveMax = ActionPipsText::GetCostRefusedReason(PIPS_TEXT_THREE, PIPS_TEXT_ONE, PIPS_TEXT_TWO);
			TestEqual(TEXT("Needs names the cost"), Needs.ToString(), FString(TEXT("Needs 2 actions.")));
			TestEqual(TEXT("Above Max"), AboveMax.ToString(), FString(TEXT("Costs more actions than you can hold.")));
		});

		It("uses the singular for one action", [this]()
		{
			TestEqual(TEXT("Needs 1 action."), ActionPipsText::GetCostRefusedReason(PIPS_TEXT_ONE, PIPS_TEXT_NONE, PIPS_TEXT_TWO).ToString(),
					  FString(TEXT("Needs 1 action.")));
		});
	});

	Describe("WithCost", [this]()
	{
		It("adds the cost to a label, and nothing for a cost of 0", [this]()
		{
			const FText Label = FText::FromString(TEXT("Fireball"));
			TestEqual(TEXT("one"), ActionPipsText::WithCost(Label, PIPS_TEXT_ONE).ToString(), FString(TEXT("Fireball (1 action)")));
			TestEqual(TEXT("two"), ActionPipsText::WithCost(Label, PIPS_TEXT_TWO).ToString(), FString(TEXT("Fireball (2 actions)")));
			TestEqual(TEXT("zero"), ActionPipsText::WithCost(Label, PIPS_TEXT_NONE).ToString(), FString(TEXT("Fireball")));
		});
	});

	Describe("MakePips", [this]()
	{
		It("fills the bank and leaves the rest of Max empty", [this]()
		{
			TestEqual(TEXT("2 of 3"), ActionPipsText::MakePips(PIPS_TEXT_TWO, PIPS_TEXT_THREE).ToString(), FString(TEXT("●●○")));
			TestEqual(TEXT("0 of 2"), ActionPipsText::MakePips(PIPS_TEXT_NONE, PIPS_TEXT_TWO).ToString(), FString(TEXT("○○")));
			TestEqual(TEXT("2 of 2"), ActionPipsText::MakePips(PIPS_TEXT_TWO, PIPS_TEXT_TWO).ToString(), FString(TEXT("●●")));
		});

		It("draws actions past Max as bonus pips, apart from the bank (Q-170f3, Crown)", [this]()
		{
			TestEqual(TEXT("4 with Max 3"), ActionPipsText::MakePips(PIPS_TEXT_FOUR, PIPS_TEXT_THREE).ToString(), FString(TEXT("●●●◆")));
			TestEqual(TEXT("4 with a lowered Max 2"), ActionPipsText::MakePips(PIPS_TEXT_FOUR, PIPS_TEXT_TWO).ToString(), FString(TEXT("●●◆◆")));
			TestEqual(TEXT("bonus count"), ActionPipsText::GetBonusActions(PIPS_TEXT_FOUR, PIPS_TEXT_TWO), PIPS_TEXT_TWO);
			TestEqual(TEXT("no bonus at Max"), ActionPipsText::GetBonusActions(PIPS_TEXT_TWO, PIPS_TEXT_TWO), PIPS_TEXT_NONE);
		});

		It("draws nothing past the display bound", [this]()
		{
			TestTrue(TEXT("too many to draw"), ActionPipsText::MakePips(ActionPipsText::MAX_PIPS_SHOWN + 1, PIPS_TEXT_TWO).IsEmpty());
			TestFalse(TEXT("exactly the bound draws"), ActionPipsText::MakePips(ActionPipsText::MAX_PIPS_SHOWN, PIPS_TEXT_TWO).IsEmpty());
		});
	});

	Describe("Max Actions", [this]()
	{
		It("shows the action-point progress below the cap", [this]()
		{
			TestEqual(TEXT("Max 3 (+75/100)"), ActionPipsText::MakeMaxActionsText(PIPS_TEXT_THREE, PIPS_TEXT_PROGRESS).ToString(),
					  FString::Printf(TEXT("Max 3 (+75/%d)"), ActionPipsConstants::ACTION_POINTS_PER_ACTION));
		});

		It("drops the progress at the cap", [this]()
		{
			TestEqual(TEXT("at the cap"),
					  ActionPipsText::MakeMaxActionsText(ActionPipsConstants::MAX_ACTIONS_CAP, PIPS_TEXT_PROGRESS).ToString(),
					  FString::Printf(TEXT("Max %d"), ActionPipsConstants::MAX_ACTIONS_CAP));
		});

		It("puts the word, the pips, the number and Max on the card's line", [this]()
		{
			TestEqual(TEXT("the line"), ActionPipsText::MakeActionsLine(PIPS_TEXT_TWO, PIPS_TEXT_THREE, PIPS_TEXT_PROGRESS).ToString(),
					  FString::Printf(TEXT("Actions ●●○ 2 · Max 3 (+75/%d)"), ActionPipsConstants::ACTION_POINTS_PER_ACTION));
		});

		It("says bonus actions must be used this turn (Q-170f3, Crown)", [this]()
		{
			TestEqual(TEXT("the line"), ActionPipsText::MakeActionsLine(PIPS_TEXT_FOUR, PIPS_TEXT_THREE, PIPS_TEXT_PROGRESS).ToString(),
					  FString::Printf(TEXT("Actions ●●●◆ 4 · Max 3 (+75/%d) · 1 bonus, lost at turn end"), ActionPipsConstants::ACTION_POINTS_PER_ACTION));
		});

		It("shows the turn's skills against the limit once one is used (Q-170j, Crown)", [this]()
		{
			TestEqual(TEXT("the counter"), ActionPipsText::MakeSkillsUsedText(PIPS_TEXT_TWO).ToString(),
					  FString::Printf(TEXT("Skills 2/%d"), ActionPipsConstants::SKILLS_PER_TURN_MAX));
			TestTrue(TEXT("on the card's line"),
					 ActionPipsText::MakeActionsLine(PIPS_TEXT_ONE, PIPS_TEXT_THREE, PIPS_TEXT_PROGRESS, PIPS_TEXT_TWO).ToString().EndsWith(
						 FString::Printf(TEXT("· Skills 2/%d"), ActionPipsConstants::SKILLS_PER_TURN_MAX)));
			TestFalse(TEXT("not before the first skill"),
					  ActionPipsText::MakeActionsLine(PIPS_TEXT_ONE, PIPS_TEXT_THREE, PIPS_TEXT_PROGRESS).ToString().Contains(TEXT("Skills")));
		});

		It("keeps the number when there are too many pips to draw", [this]()
		{
			const FString Line = ActionPipsText::MakeActionsLine(ActionPipsText::MAX_PIPS_SHOWN + 1, PIPS_TEXT_THREE, PIPS_TEXT_NONE).ToString();
			TestTrue(TEXT("the number shows"), Line.Contains(FString::FromInt(ActionPipsText::MAX_PIPS_SHOWN + 1)));
			TestFalse(TEXT("no pips"), Line.Contains(TEXT("●")));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
