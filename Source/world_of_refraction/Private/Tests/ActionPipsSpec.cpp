// ActionPipsSpec.cpp
// B-170a (ActionPips.md §2, §3.1; Q-170, Q-170g, Q-170h): the banked actions on the character. Max Actions
// = 2 + one per whole 100 action points (progress shown), Reflex sets the regain (B-170i, Q-170i: 1 + one per
// whole step of Reflex value, Body levels cancel, Body boosts count), the regain never fills past Max, a gain may pass Max
// when it says so and the extra is lost at turn end (Q-170f2), a fight
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
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr int32 ACTION_PIPS_GEAR_SMALL = 25;
	constexpr int32 ACTION_PIPS_GEAR_LARGE = 50;
	constexpr int32 ACTION_PIPS_ONE_AND_THREE_QUARTERS = 175;
	constexpr int32 ACTION_PIPS_THREE_QUARTERS = 75;
	constexpr int32 ACTION_PIPS_FAR_PAST_CAP = 10000;
	/** Far past every regain step, so the regain is at the cap. */
	constexpr int32 ACTION_PIPS_HUGE_REFLEX = 100000;
	constexpr float ACTION_PIPS_JUST_UNDER = 0.01f;
	/** B-170i: a light Reflex build (one step and a bit) and a focused one (two steps and a bit), at Body level 0. */
	constexpr int32 ACTION_PIPS_LIGHT_REFLEX = 16;
	constexpr int32 ACTION_PIPS_FOCUSED_REFLEX = 31;
	constexpr int32 ACTION_PIPS_HIGH_BODY_LEVEL = 7;
	/** "Regain 2 (Reflex 18.0 / 30.0)": value 18 with a step of 15. */
	constexpr float ACTION_PIPS_REGAIN_PROGRESS_VALUE = 18.0f;
	constexpr float ACTION_PIPS_REGAIN_STEP_SHOWN = 15.0f;
	/** A regain of 3, for the bank rules (a plain number, not a Reflex threshold). */
	constexpr int32 ACTION_PIPS_REGAIN_THREE = 3;
	/** B-170i: Reflex points under one step alone, and a +50% Body buff that lifts them past it (11 x 1.5 = 16.5). */
	constexpr int32 ACTION_PIPS_REFLEX_UNDER_STEP = 11;
	constexpr float ACTION_PIPS_BODY_BUFF_PERCENT = 50.0f;
	constexpr int32 ACTION_PIPS_BODY_BUFF_ID = 17001;
	constexpr int32 ACTION_PIPS_BODY_BUFF_TURNS = 3;
}

BEGIN_DEFINE_SPEC(FActionPipsSpec, "WoR.Combat.ActionPips",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCharacterData *Data = nullptr;
	UCharacterDataComponent *Char = nullptr;
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
		It("is 1 + one per whole step of Reflex value, at most the Max Actions cap (B-170i)", [this]()
		{
			TestEqual(TEXT("No Reflex"), ActionPipsRules::RegainForReflexValue(0.0f, REGAIN_STEP), ACTIONS_REGAIN_BASE);
			TestEqual(TEXT("Just under one step"),
					  ActionPipsRules::RegainForReflexValue(REGAIN_STEP - ACTION_PIPS_JUST_UNDER, REGAIN_STEP), ACTIONS_REGAIN_BASE);
			TestEqual(TEXT("One step"), ActionPipsRules::RegainForReflexValue(REGAIN_STEP, REGAIN_STEP), ACTIONS_REGAIN_BASE + 1);
			TestEqual(TEXT("Just under two steps"),
					  ActionPipsRules::RegainForReflexValue(2.0f * REGAIN_STEP - ACTION_PIPS_JUST_UNDER, REGAIN_STEP), ACTIONS_REGAIN_BASE + 1);
			TestEqual(TEXT("Two steps"), ActionPipsRules::RegainForReflexValue(2.0f * REGAIN_STEP, REGAIN_STEP), ACTIONS_REGAIN_BASE + 2);
			TestEqual(TEXT("Huge Reflex stops at the cap"), ActionPipsRules::RegainForReflexValue(ACTION_PIPS_HUGE_REFLEX, REGAIN_STEP),
					  MAX_ACTIONS_CAP);
			TestEqual(TEXT("A negative value is the base"), ActionPipsRules::RegainForReflexValue(-REGAIN_STEP, REGAIN_STEP),
					  ACTIONS_REGAIN_BASE);
			TestEqual(TEXT("A step of 0 is the base"), ActionPipsRules::RegainForReflexValue(REGAIN_STEP, 0.0f), ACTIONS_REGAIN_BASE);
		});

		It("shows the progress to the next regain, and none at the cap", [this]()
		{
			TestEqual(TEXT("Progress"), ActionPipsRules::RegainString(ACTION_PIPS_REGAIN_PROGRESS_VALUE, ACTION_PIPS_REGAIN_STEP_SHOWN),
					  FString(TEXT("Regain 2 (Reflex 18.0 / 30.0)")));
			TestEqual(TEXT("At the cap"), ActionPipsRules::RegainString(ACTION_PIPS_HUGE_REFLEX, REGAIN_STEP),
					  FString::Printf(TEXT("Regain %d"), MAX_ACTIONS_CAP));
		});

		It("never fills past Max, and adds nothing at or above it", [this]()
		{
			TestEqual(TEXT("1 + 3 stops at Max 2"), ActionPipsRules::BankAfterRegain(1, ACTION_PIPS_REGAIN_THREE, MAX_ACTIONS_BASE),
					  MAX_ACTIONS_BASE);
			TestEqual(TEXT("At Max"), ActionPipsRules::BankAfterRegain(MAX_ACTIONS_BASE, ACTIONS_REGAIN_BASE, MAX_ACTIONS_BASE), MAX_ACTIONS_BASE);
			TestEqual(TEXT("Over Max stays"), ActionPipsRules::BankAfterRegain(MAX_ACTIONS_BASE + 2, ACTIONS_REGAIN_BASE, MAX_ACTIONS_BASE),
					  MAX_ACTIONS_BASE + 2);
		});

		It("loses bonus actions above Max at turn end and keeps the bank up to Max (Q-170f2)", [this]()
		{
			TestEqual(TEXT("Above Max: trimmed to Max"), ActionPipsRules::BankAtTurnEnd(MAX_ACTIONS_BASE + 2, MAX_ACTIONS_BASE), MAX_ACTIONS_BASE);
			TestEqual(TEXT("At Max: kept"), ActionPipsRules::BankAtTurnEnd(MAX_ACTIONS_BASE, MAX_ACTIONS_BASE), MAX_ACTIONS_BASE);
			TestEqual(TEXT("Below Max: carries over"), ActionPipsRules::BankAtTurnEnd(ACTIONS_REGAIN_BASE, MAX_ACTIONS_BASE), ACTIONS_REGAIN_BASE);
		});

		It("starts a fight with the regain, capped at Max", [this]()
		{
			TestEqual(TEXT("Regain 1"), ActionPipsRules::ActionsAtFightStart(ACTIONS_REGAIN_BASE, MAX_ACTIONS_BASE), ACTIONS_REGAIN_BASE);
			TestEqual(TEXT("Regain 3, Max 2"), ActionPipsRules::ActionsAtFightStart(ACTION_PIPS_REGAIN_THREE, MAX_ACTIONS_BASE),
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

		It("regains 1 with no Reflex, 2 with a light Reflex build and 3 with a focused one (B-170i)", [this]()
		{
			if (!Char) return;
			TestEqual(TEXT("No Reflex"), Char->GetActionRegain(), ACTIONS_REGAIN_BASE);

			Data->Reflex = ACTION_PIPS_LIGHT_REFLEX;
			TestEqual(TEXT("Light Reflex"), Char->GetActionRegain(), ACTIONS_REGAIN_BASE + 1);

			Data->Reflex = ACTION_PIPS_FOCUSED_REFLEX;
			TestEqual(TEXT("Focused Reflex"), Char->GetActionRegain(), ACTIONS_REGAIN_BASE + 2);
		});

		It("gives the same regain for the same Reflex points at any Body level (levelling Body cancels out)", [this]()
		{
			if (!Char) return;
			for (const int32 Points : {ACTION_PIPS_LIGHT_REFLEX - 2, ACTION_PIPS_LIGHT_REFLEX, ACTION_PIPS_FOCUSED_REFLEX})
			{
				Data->Reflex = Points;
				Char->LiveWorldBody = 0;
				const int32 AtBodyZero = Char->GetActionRegain();
				Char->LiveWorldBody = ACTION_PIPS_HIGH_BODY_LEVEL;
				TestTrue(TEXT("The step grows with Body"), Char->GetRegainReflexStep() > REGAIN_STEP);
				TestEqual(FString::Printf(TEXT("%d points"), Points), Char->GetActionRegain(), AtBodyZero);
			}
		});

		It("counts an external Body boost in the Reflex value but not in the step", [this]()
		{
			// The pillar layers sit behind the loadout (ApplyEvolutionPillarModifier), so this combatant carries one.
			UCharacterData *GearedData = FWoRTestWorld::NewCharacterData();
			ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(GearedData, NewObject<UWeaponData>(GetTransientPackage()));
			AActor *Geared = Loadout ? Loadout->GetOwner() : nullptr;
			UCharacterDataComponent *GearedChar = Geared ? Geared->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			USkillEffectManager *SEM = TestWorld->GetSubsystem<USkillEffectManager>();
			if (!TestNotNull(TEXT("Geared combatant"), GearedChar) || !TestNotNull(TEXT("Skill effect manager"), SEM)) return;
			GearedData->Reflex = ACTION_PIPS_REFLEX_UNDER_STEP;
			const float StepBefore = GearedChar->GetRegainReflexStep();
			TestEqual(TEXT("Under one step alone"), GearedChar->GetActionRegain(), ACTIONS_REGAIN_BASE);

			SEM->ApplyEffect(Geared,
							 FActiveSkillEffect::CreateBuff(TEXT("Regain Body buff"), ACTION_PIPS_BODY_BUFF_ID, ESkillEffectType::BodyBuff,
															ACTION_PIPS_BODY_BUFF_PERCENT, ACTION_PIPS_BODY_BUFF_TURNS),
							 Geared);
			TestEqual(TEXT("The step is unchanged"), GearedChar->GetRegainReflexStep(), StepBefore);
			TestEqual(TEXT("The Body buff lifts it past one step"), GearedChar->GetActionRegain(), ACTIONS_REGAIN_BASE + 1);
		});

		It("starts a fight with the regain capped at Max, and regains up to Max only", [this]()
		{
			if (!Char) return;
			Data->Reflex = ACTION_PIPS_HUGE_REFLEX;
			Char->ResetActionsForFight();
			TestEqual(TEXT("The capped regain, capped again at Max 2"), Char->CurrentActions, MAX_ACTIONS_BASE);

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

		It("gains up to Max, or past it when the gain may pass, and loses the extra at turn end", [this]()
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
			Char->ExpireActionsAboveMax();
			TestEqual(TEXT("Turn end: the bonus above Max is lost"), Char->CurrentActions, MAX_ACTIONS_BASE);
			Char->SpendActions(1);
			Char->ExpireActionsAboveMax();
			TestEqual(TEXT("Turn end: a bank below Max carries over"), Char->CurrentActions, MAX_ACTIONS_BASE - 1);
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
