// FleeSpec.cpp
// D-091 (Crown, 2026-10-26; B-176): the party can flee a normal fight, not a mini-boss or boss;
// fleeing costs a share of each member's Gold and ends the fight as Fled.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "ActionExecutedRecorder.h"
#include "CombatResultRecorder.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/FleeRules.h"
#include "Currency/CurrencyComponent.h"

namespace
{
	constexpr int32 FLEE_TEST_GOLD = 100;
	/** A purse too small to lose anything at the current share. */
	constexpr int32 FLEE_TEST_SMALL_GOLD = 5;
	/** Test-world ticks are 0.01s; enough for the next-tick end of the fight. */
	constexpr int32 FLEE_SETTLE_FRAMES = 3;
}

BEGIN_DEFINE_SPEC(FFleeSpec, "WoR.Combat.Flee",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UActionExecutedRecorder *Actions = nullptr;
	UCombatResultRecorder *Results = nullptr;
	UCurrencyComponent *HeroGold = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	/** Equal stats: the tie-break gives the local party (Hero) turn 1. */
	bool StartFight(EEncounterRank Rank)
	{
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Fight set up"), Orchestrator && Hero && Foe))
		{
			return false;
		}
		HeroGold = NewObject<UCurrencyComponent>(Hero);
		Hero->AddInstanceComponent(HeroGold);
		HeroGold->RegisterComponent();
		HeroGold->Add(ECurrencyType::Gold, FLEE_TEST_GOLD);

		Actions = NewObject<UActionExecutedRecorder>();
		Actions->AddToRoot();
		Actions->Listen(Orchestrator);
		Results = NewObject<UCombatResultRecorder>();
		Results->AddToRoot();
		Results->Listen(Orchestrator);

		Orchestrator->SetEncounterRank(Rank);
		Orchestrator->StartCombat({Hero}, {Foe});
		return TestTrue(TEXT("Hero acts first"), Orchestrator->GetCurrentActor() == Hero);
	}

	void Flee()
	{
		FAction Action;
		Action.ActionType = EActionType::Flee;
		Orchestrator->SubmitActionAsync(Action);
		TestWorld->Tick(FLEE_SETTLE_FRAMES);
	}
END_DEFINE_SPEC(FFleeSpec)

void FFleeSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		for (UObject *Recorder : TArray<UObject *>{Actions, Results})
		{
			if (Recorder)
			{
				Recorder->RemoveFromRoot();
			}
		}
		Actions = nullptr;
		Results = nullptr;
		HeroGold = nullptr;
		Orchestrator = nullptr;
		Hero = Foe = nullptr;
		TestWorld.Reset();
	});

	Describe("the rule", [this]()
	{
		It("allows only a normal fight", [this]()
		{
			TestTrue(TEXT("normal"), FleeRules::CanFleeRank(EEncounterRank::Normal));
			TestFalse(TEXT("mini-boss"), FleeRules::CanFleeRank(EEncounterRank::MiniBoss));
			TestFalse(TEXT("boss"), FleeRules::CanFleeRank(EEncounterRank::Boss));
		});

		It("costs a share of the Gold, rounded down", [this]()
		{
			TestEqual(TEXT("share of a full purse"), FleeRules::GoldLost(FLEE_TEST_GOLD), FMath::FloorToInt(FLEE_TEST_GOLD * FleeRules::GOLD_LOSS_SHARE));
			TestEqual(TEXT("a small purse loses nothing"), FleeRules::GoldLost(FLEE_TEST_SMALL_GOLD), 0);
			TestEqual(TEXT("an empty purse"), FleeRules::GoldLost(0), 0);
		});
	});

	Describe("in a fight", [this]()
	{
		It("flees a normal fight: Gold paid, fight ends as Fled", [this]()
		{
			if (!StartFight(EEncounterRank::Normal))
			{
				return;
			}
			TestTrue(TEXT("no reason to refuse"), Orchestrator->GetFleeRefusedReason(Hero).IsEmpty());
			Flee();

			TestTrue(TEXT("the flee was reported"), Actions->Saw(Hero, EActionType::Flee, true));
			TestEqual(TEXT("Gold paid"), HeroGold->GetBalance(ECurrencyType::Gold), FLEE_TEST_GOLD - FleeRules::GoldLost(FLEE_TEST_GOLD));
			TestTrue(TEXT("ended once, as Fled"), Results->EndStates.Num() == 1 && Results->EndStates[0] == ECombatState::Fled);
			TestTrue(TEXT("orchestrator idle"), Orchestrator->GetCombatState() == ECombatState::Idle);
		});

		It("refuses to flee a boss fight", [this]()
		{
			if (!StartFight(EEncounterRank::Boss))
			{
				return;
			}
			TestFalse(TEXT("refused with a reason"), Orchestrator->GetFleeRefusedReason(Hero).IsEmpty());
			Flee();

			TestFalse(TEXT("no flee reported"), Actions->Saw(Hero, EActionType::Flee, true));
			TestEqual(TEXT("Gold untouched"), HeroGold->GetBalance(ECurrencyType::Gold), FLEE_TEST_GOLD);
			TestEqual(TEXT("fight not ended"), Results->EndStates.Num(), 0);
			TestTrue(TEXT("still fighting"), Orchestrator->GetCombatState() == ECombatState::InProgress);
		});

		It("refuses a mini-boss fight and an enemy", [this]()
		{
			if (!StartFight(EEncounterRank::MiniBoss))
			{
				return;
			}
			TestFalse(TEXT("mini-boss refused"), Orchestrator->GetFleeRefusedReason(Hero).IsEmpty());
			Orchestrator->SetEncounterRank(EEncounterRank::Normal);
			TestFalse(TEXT("an enemy can't flee"), Orchestrator->GetFleeRefusedReason(Foe).IsEmpty());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
