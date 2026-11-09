// ActionPipsSpec.cpp
// B-170a (ActionPips.md §2, §3.1; Q-170, Q-170g, Q-170h): the banked actions on the character. Max Actions
// = 2 + one per whole 100 action points (progress shown), Reflex sets the regain (1 / 2 / 3 at 0.5 / 0.8
// of its window range), the regain never fills past Max, a gain may pass Max when it says so, a fight
// starts with the regain capped at Max, and OnActionsChanged reports each change. Nothing reads the bank
// for turns yet (B-170b / B-170c).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "ActionsChangedRecorder.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionPipsRules.h"
#include "Combat/CombatConstants.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr int32 ACTION_PIPS_GEAR_SMALL = 25;
	constexpr int32 ACTION_PIPS_GEAR_LARGE = 50;
	constexpr int32 ACTION_PIPS_ONE_AND_THREE_QUARTERS = 175;
	constexpr int32 ACTION_PIPS_THREE_QUARTERS = 75;
	constexpr int32 ACTION_PIPS_FAR_PAST_CAP = 10000;
	/** Far past the Reflex window cap, so the share is 1. */
	constexpr int32 ACTION_PIPS_HUGE_REFLEX = 100000;
	/** Between the two regain thresholds. */
	constexpr float ACTION_PIPS_MID_SHARE = 0.65f;
	constexpr float ACTION_PIPS_JUST_UNDER = 0.01f;
}

BEGIN_DEFINE_SPEC(FActionPipsSpec, "WoR.Combat.ActionPips",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCharacterData *Data = nullptr;
	UCharacterDataComponent *Char = nullptr;

	/** Reflex points that put the share at ShareWanted (the stat term alone; no gear on this combatant). */
	int32 ReflexPointsForShare(float ShareWanted) const
	{
		const float PerPoint = Data->GetEffectiveBody() * CombatConstants::REFLEX_WINDOW_PER_POINT;
		return PerPoint > KINDA_SMALL_NUMBER ? FMath::RoundToInt(ShareWanted * CombatConstants::WINDOW_CAP_SECONDS / PerPoint) : 0;
	}
END_DEFINE_SPEC(FActionPipsSpec)

void FActionPipsSpec::Define()
{
	using namespace ActionPipsConstants;

	Describe("Max Actions from action points", [this]()
	{
		It("is the base with no action points", [this]()
		{
			TestEqual(TEXT("Max"), ActionPipsRules::MaxActionsForPoints(0), MAX_ACTIONS_BASE);
			TestEqual(TEXT("Progress"), ActionPipsRules::ActionPointProgress(0), 0);
		});

		It("adds the layers up and counts only whole actions (+25 +50 = 75: Max 2, +75/100)", [this]()
		{
			FWoRActionPointLayers Layers;
			Layers.Gear = ACTION_PIPS_GEAR_SMALL;
			Layers.Evolution = ACTION_PIPS_GEAR_LARGE;
			TestEqual(TEXT("Points"), Layers.Compose(), ACTION_PIPS_THREE_QUARTERS);
			TestEqual(TEXT("Max"), ActionPipsRules::MaxActionsForPoints(Layers.Compose()), MAX_ACTIONS_BASE);
			TestEqual(TEXT("Progress"), ActionPipsRules::ActionPointProgress(Layers.Compose()), ACTION_PIPS_THREE_QUARTERS);
			TestEqual(TEXT("Line"), ActionPipsRules::MaxActionsString(Layers.Compose()), FString(TEXT("Max 2 (+75/100)")));
		});

		It("gives Max 3 at 175 points, with 75 toward the next", [this]()
		{
			TestEqual(TEXT("Max"), ActionPipsRules::MaxActionsForPoints(ACTION_PIPS_ONE_AND_THREE_QUARTERS), MAX_ACTIONS_BASE + 1);
			TestEqual(TEXT("Progress"), ActionPipsRules::ActionPointProgress(ACTION_PIPS_ONE_AND_THREE_QUARTERS), ACTION_PIPS_THREE_QUARTERS);
			TestEqual(TEXT("Line"), ActionPipsRules::MaxActionsString(ACTION_PIPS_ONE_AND_THREE_QUARTERS), FString(TEXT("Max 3 (+75/100)")));
		});

		It("stops at the safety cap and never composes below 0", [this]()
		{
			TestEqual(TEXT("Capped"), ActionPipsRules::MaxActionsForPoints(ACTION_PIPS_FAR_PAST_CAP), MAX_ACTIONS_CAP);
			FWoRActionPointLayers Layers;
			Layers.Transient = -ACTION_PIPS_GEAR_LARGE;
			TestEqual(TEXT("Floored"), Layers.Compose(), 0);
		});
	});

	Describe("Regain from Reflex", [this]()
	{
		It("is 1, then 2 at threshold 2, then 3 at threshold 3", [this]()
		{
			TestEqual(TEXT("No Reflex"), ActionPipsRules::RegainForReflexShare(0.0f), ACTIONS_REGAIN_BASE);
			TestEqual(TEXT("Just under 2"), ActionPipsRules::RegainForReflexShare(ACTIONS_REGAIN_REFLEX_THRESHOLD_2 - ACTION_PIPS_JUST_UNDER),
					  ACTIONS_REGAIN_BASE);
			TestEqual(TEXT("At 2"), ActionPipsRules::RegainForReflexShare(ACTIONS_REGAIN_REFLEX_THRESHOLD_2), ACTIONS_REGAIN_AT_THRESHOLD_2);
			TestEqual(TEXT("Just under 3"), ActionPipsRules::RegainForReflexShare(ACTIONS_REGAIN_REFLEX_THRESHOLD_3 - ACTION_PIPS_JUST_UNDER),
					  ACTIONS_REGAIN_AT_THRESHOLD_2);
			TestEqual(TEXT("At 3"), ActionPipsRules::RegainForReflexShare(ACTIONS_REGAIN_REFLEX_THRESHOLD_3), ACTIONS_REGAIN_AT_THRESHOLD_3);
			TestEqual(TEXT("Full"), ActionPipsRules::RegainForReflexShare(1.0f), ACTIONS_REGAIN_AT_THRESHOLD_3);
		});

		It("never fills past Max, and adds nothing at or above it", [this]()
		{
			TestEqual(TEXT("1 + 3 stops at Max 2"), ActionPipsRules::BankAfterRegain(1, ACTIONS_REGAIN_AT_THRESHOLD_3, MAX_ACTIONS_BASE),
					  MAX_ACTIONS_BASE);
			TestEqual(TEXT("At Max"), ActionPipsRules::BankAfterRegain(MAX_ACTIONS_BASE, ACTIONS_REGAIN_BASE, MAX_ACTIONS_BASE), MAX_ACTIONS_BASE);
			TestEqual(TEXT("Over Max stays"), ActionPipsRules::BankAfterRegain(MAX_ACTIONS_BASE + 2, ACTIONS_REGAIN_BASE, MAX_ACTIONS_BASE),
					  MAX_ACTIONS_BASE + 2);
		});

		It("starts a fight with the regain, capped at Max", [this]()
		{
			TestEqual(TEXT("Regain 1"), ActionPipsRules::ActionsAtFightStart(ACTIONS_REGAIN_BASE, MAX_ACTIONS_BASE), ACTIONS_REGAIN_BASE);
			TestEqual(TEXT("Regain 3, Max 2"), ActionPipsRules::ActionsAtFightStart(ACTIONS_REGAIN_AT_THRESHOLD_3, MAX_ACTIONS_BASE),
					  MAX_ACTIONS_BASE);
		});
	});

	Describe("On the character", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
			Data = FWoRTestWorld::NewCharacterData();
			AActor *Hero = TestWorld->SpawnCombatant(Data);
			Char = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			TestNotNull(TEXT("Character data component"), Char);
		});

		AfterEach([this]()
		{
			Char = nullptr;
			Data = nullptr;
			TestWorld.Reset();
		});

		It("replicates the bank", [this]()
		{
			const FProperty *Property = FindFProperty<FProperty>(UCharacterDataComponent::StaticClass(), TEXT("CurrentActions"));
			TestTrue(TEXT("CurrentActions replicates"), Property && Property->HasAnyPropertyFlags(CPF_Net));
		});

		It("has Max 2 with no action points, and Max 3 (+75/100) at 175", [this]()
		{
			if (!Char) return;
			TestEqual(TEXT("Points"), Char->GetActionPoints(), 0);
			TestEqual(TEXT("Max"), Char->GetMaxActions(), MAX_ACTIONS_BASE);
			Char->DebugActionPoints = ACTION_PIPS_ONE_AND_THREE_QUARTERS;
			TestEqual(TEXT("Max"), Char->GetMaxActions(), MAX_ACTIONS_BASE + 1);
			TestEqual(TEXT("Progress"), Char->GetActionPointProgress(), ACTION_PIPS_THREE_QUARTERS);
		});

		It("regains 1 with no Reflex, 2 between the thresholds and 3 at full Reflex", [this]()
		{
			if (!Char) return;
			TestEqual(TEXT("No Reflex"), Char->GetActionRegain(), ACTIONS_REGAIN_BASE);

			Data->Reflex = ReflexPointsForShare(ACTION_PIPS_MID_SHARE);
			const float MidShare = Char->GetReflexRegainShare();
			TestTrue(TEXT("Mid share between the thresholds"),
					 MidShare >= ACTIONS_REGAIN_REFLEX_THRESHOLD_2 && MidShare < ACTIONS_REGAIN_REFLEX_THRESHOLD_3);
			TestEqual(TEXT("Mid Reflex"), Char->GetActionRegain(), ACTIONS_REGAIN_AT_THRESHOLD_2);

			Data->Reflex = ACTION_PIPS_HUGE_REFLEX;
			TestTrue(TEXT("Share at or past threshold 3"), Char->GetReflexRegainShare() >= ACTIONS_REGAIN_REFLEX_THRESHOLD_3);
			TestEqual(TEXT("Full Reflex"), Char->GetActionRegain(), ACTIONS_REGAIN_AT_THRESHOLD_3);
		});

		It("starts a fight with the regain capped at Max, and regains up to Max only", [this]()
		{
			if (!Char) return;
			Data->Reflex = ACTION_PIPS_HUGE_REFLEX;
			Char->ResetActionsForFight();
			TestEqual(TEXT("Regain 3 capped at Max 2"), Char->CurrentActions, MAX_ACTIONS_BASE);

			TestTrue(TEXT("Spent 2"), Char->SpendActions(MAX_ACTIONS_BASE));
			Data->Reflex = 0;
			Char->RegainActions();
			TestEqual(TEXT("Regain 1"), Char->CurrentActions, ACTIONS_REGAIN_BASE);
			Char->RegainActions();
			Char->RegainActions();
			TestEqual(TEXT("Never past Max"), Char->CurrentActions, MAX_ACTIONS_BASE);
		});

		It("spends only what is banked, and loses never below 0", [this]()
		{
			if (!Char) return;
			Char->ResetActionsForFight();
			TestEqual(TEXT("Fight start, no Reflex"), Char->CurrentActions, ACTIONS_REGAIN_BASE);
			TestFalse(TEXT("A cost above the bank is refused"), Char->SpendActions(ACTIONS_REGAIN_BASE + 1));
			TestEqual(TEXT("Nothing spent"), Char->CurrentActions, ACTIONS_REGAIN_BASE);
			TestFalse(TEXT("A cost of 0 is refused"), Char->SpendActions(0));
			Char->LoseActions(ACTIONS_REGAIN_BASE + 1);
			TestEqual(TEXT("Never below 0"), Char->CurrentActions, 0);
		});

		It("gains up to Max, or past it when the gain may pass, and the extra stays banked", [this]()
		{
			if (!Char) return;
			Char->GainActions(MAX_ACTIONS_CAP, /*bMayPassMax=*/false);
			TestEqual(TEXT("Stops at Max"), Char->CurrentActions, MAX_ACTIONS_BASE);
			Char->GainActions(1, /*bMayPassMax=*/true);
			TestEqual(TEXT("Passes Max"), Char->CurrentActions, MAX_ACTIONS_BASE + 1);
			Char->GainActions(1, /*bMayPassMax=*/false);
			TestEqual(TEXT("A capped gain never trims"), Char->CurrentActions, MAX_ACTIONS_BASE + 1);
			Char->RegainActions();
			TestEqual(TEXT("Regain adds nothing over Max"), Char->CurrentActions, MAX_ACTIONS_BASE + 1);
		});

		It("broadcasts OnActionsChanged with the bank and Max on each change, and not when nothing changes", [this]()
		{
			if (!Char) return;
			UActionsChangedRecorder *Recorder = NewObject<UActionsChangedRecorder>();
			Char->OnActionsChanged.AddDynamic(Recorder, &UActionsChangedRecorder::Record);

			Char->ResetActionsForFight();
			Char->SpendActions(ACTIONS_REGAIN_BASE);
			Char->LoseActions(1); // already 0: no change
			Char->SpendActions(1); // refused: no change

			if (TestEqual(TEXT("Two broadcasts"), Recorder->Broadcasts.Num(), 2))
			{
				TestEqual(TEXT("Fight start: current"), Recorder->Broadcasts[0].Key, ACTIONS_REGAIN_BASE);
				TestEqual(TEXT("Fight start: max"), Recorder->Broadcasts[0].Value, MAX_ACTIONS_BASE);
				TestEqual(TEXT("Spent: current"), Recorder->Broadcasts[1].Key, 0);
			}
			Char->OnActionsChanged.RemoveAll(Recorder);
		});
	});
}

#endif
