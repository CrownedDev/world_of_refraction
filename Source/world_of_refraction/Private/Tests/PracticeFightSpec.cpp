// PracticeFightSpec.cpp
// B-611: the training dummy's practice fight. A marked fighter can't fall (its HP refills at 0), the dummy passes every
// turn at once, leaving by the Flee row costs no Gold, a cast wears nothing and a fight item keeps its use. The hub
// refuses to start one with no party. Unproven here: the level travel there and back, the return stand in the hub, the
// prompt and the "End practice" label on screen (docs/PIE/B-611.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "CombatResultRecorder.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/CombatOrchestrator.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/CrystalManager.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Hub/HubTrainingDummy.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 PRACTICE_TEST_GOLD = 100;
	constexpr int32 PRACTICE_TEST_CARRIED = 2;
	/** Test-world ticks are 0.01 s: enough for the dummy's next-tick pass and a next-tick fight end. */
	constexpr int32 PRACTICE_SETTLE_FRAMES = 3;
	/** A cast that wears its crystal outside practice: a fully infused spell from a tier S action on a tier F crystal. */
	constexpr int32 PRACTICE_TEST_INFUSION = 3;
	const FCrystalId PRACTICE_TEST_POTION(ECrystalType::HealthPotion, EItemTier::F_Tier);
}

BEGIN_DEFINE_SPEC(FPracticeFightSpec, "WoR.Hub.PracticeFight",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UTrialRunSubsystem *Run = nullptr;
	ACombatOrchestrator *Orchestrator = nullptr;
	UCombatResultRecorder *Results = nullptr;
	AActor *Hero = nullptr;
	AActor *Dummy = nullptr;

	void SetPractice(bool bOn) { Run->bPracticeFight = bOn; }

	/** Equal stats: the tie-break gives the local party (Hero) turn 1. The dummy is marked as the battle level does. */
	bool StartFight()
	{
		Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Dummy = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Fight set up"), Orchestrator && Hero && Dummy))
		{
			return false;
		}
		WoRPractice::MarkFighter(Hero, false);
		WoRPractice::MarkFighter(Dummy, true);
		Results = NewObject<UCombatResultRecorder>();
		Results->AddToRoot();
		Results->Listen(Orchestrator);
		Orchestrator->SetEncounterRank(EEncounterRank::Normal);
		Orchestrator->StartCombat({Hero}, {Dummy});
		return TestTrue(TEXT("Hero acts first"), Orchestrator->GetCurrentActor() == Hero);
	}

	void Submit(EActionType Type)
	{
		FAction Action;
		Action.ActionType = Type;
		Orchestrator->SubmitActionAsync(Action);
		TestWorld->Tick(PRACTICE_SETTLE_FRAMES);
	}
END_DEFINE_SPEC(FPracticeFightSpec)

void FPracticeFightSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
		TestNotNull(TEXT("Run"), Run);
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		if (Results)
		{
			Results->RemoveFromRoot();
		}
		if (Run)
		{
			SetPractice(false);
		}
		Results = nullptr;
		Orchestrator = nullptr;
		Hero = Dummy = nullptr;
		Run = nullptr;
		TestWorld.Reset();
	});

	It("marks a fighter: it can't fall, and only the dummy passes its turns", [this]()
	{
		if (!Run) return;
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Dummy = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		WoRPractice::MarkFighter(Hero, false);
		WoRPractice::MarkFighter(Dummy, true);
		const UCharacterDataComponent *HeroData = Hero->FindComponentByClass<UCharacterDataComponent>();
		UCharacterDataComponent *DummyData = Dummy->FindComponentByClass<UCharacterDataComponent>();
		TestTrue(TEXT("hero can't fall"), HeroData->bCannotFall);
		TestFalse(TEXT("hero still acts"), HeroData->bTrainingDummy);
		TestTrue(TEXT("dummy can't fall and passes"), DummyData->bCannotFall && DummyData->bTrainingDummy);

		DummyData->ServerTakeDamage(DummyData->MaxHP);
		TestTrue(TEXT("a lethal hit leaves the dummy standing"), DummyData->bIsAlive);
		TestEqual(TEXT("with its HP refilled"), DummyData->CurrentHP, DummyData->MaxHP);
	});

	It("passes the dummy's turn at once and gives the turn back", [this]()
	{
		if (!Run) return;
		SetPractice(true);
		if (!StartFight()) return;
		Submit(EActionType::EndTurn);
		TestTrue(TEXT("back to the hero"), Orchestrator->GetCurrentActor() == Hero);
		TestTrue(TEXT("still fighting"), Orchestrator->GetCombatState() == ECombatState::InProgress);
		TestEqual(TEXT("nothing ended"), Results->EndStates.Num(), 0);
	});

	It("ends by the Flee row with no Gold lost", [this]()
	{
		if (!Run) return;
		SetPractice(true);
		if (!StartFight()) return;
		UCurrencyComponent *Gold = FWoRTestWorld::AddComponent<UCurrencyComponent>(Hero);
		Gold->Add(ECurrencyType::Gold, PRACTICE_TEST_GOLD);
		TestTrue(TEXT("the row is open"), Orchestrator->GetFleeRefusedReason(Hero).IsEmpty());
		Submit(EActionType::Flee);
		TestEqual(TEXT("Gold untouched"), Gold->GetBalance(ECurrencyType::Gold), PRACTICE_TEST_GOLD);
		TestTrue(TEXT("ended once, as Fled"), Results->EndStates.Num() == 1 && Results->EndStates[0] == ECombatState::Fled);
	});

	It("wears nothing on a cast", [this]()
	{
		if (!Run) return;
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		const int32 Worn = UCrystalManager::ComputeCastWear(Hero, EItemTier::F_Tier, EItemTier::S_Tier, PRACTICE_TEST_INFUSION, true);
		if (!TestTrue(TEXT("the cast wears outside practice"), Worn > 0)) return;
		SetPractice(true);
		TestEqual(TEXT("and nothing in practice"), UCrystalManager::ComputeCastWear(Hero, EItemTier::F_Tier, EItemTier::S_Tier, PRACTICE_TEST_INFUSION, true), 0);
	});

	It("keeps a fight item's use", [this]()
	{
		if (!Run) return;
		UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Executor"), Executor) || !TestNotNull(TEXT("Pool"), Pool) || !TestNotNull(TEXT("Party"), Party)) return;
		// A party member with a pouch, as PotionsSpec spawns one.
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Party->Members.Empty();
		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Data;
		Hero = TestWorld->SpawnCombatant(Data);
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Hero);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Hero);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Hero);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Hero);
		ULoadoutComponent *Loadout = FWoRTestWorld::AddComponent<ULoadoutComponent>(Hero);
		Inventory->SavedLoadouts = {FCombatLoadout()};
		Inventory->DrawSharedGear();
		const FName Key = UPoolSubsystem::GetCharacterKey(Data);
		Pool->CarryOrStore(Key, PRACTICE_TEST_POTION, PRACTICE_TEST_CARRIED);
		Inventory->ActiveLoadoutIndex = 0;
		if (!TestTrue(TEXT("Ready"), Loadout->PrepareForBattle(Inventory))) return;
		UCharacterDataComponent *HeroData = Hero->FindComponentByClass<UCharacterDataComponent>();
		HeroData->ServerTakeDamage(HeroData->CurrentHP - 1);

		SetPractice(true);
		TestTrue(TEXT("the potion lands"), Executor->ExecuteItem(Hero, PRACTICE_TEST_POTION, {Hero}).bSuccess);
		TestEqual(TEXT("and keeps its use"), Pool->GetCarriedCount(Key, PRACTICE_TEST_POTION), PRACTICE_TEST_CARRIED);

		SetPractice(false);
		TestTrue(TEXT("outside practice it lands"), Executor->ExecuteItem(Hero, PRACTICE_TEST_POTION, {Hero}).bSuccess);
		TestEqual(TEXT("and one is spent"), Pool->GetCarriedCount(Key, PRACTICE_TEST_POTION), PRACTICE_TEST_CARRIED - 1);
	});

	It("ends a practice fight left without End practice when a trial or a real fight starts", [this]()
	{
		if (!Run) return;
		SetPractice(true);
		Run->SetPendingReturnStand(FTransform(FVector(1.f, 2.f, 3.f)));
		Run->EndAbandonedPractice(); // what EnterTrial and EnterEncounter call first
		TestFalse(TEXT("not a practice fight"), Run->IsPracticeFight());
		FTransform Stand;
		TestFalse(TEXT("the dummy's return stand is dropped"), Run->ConsumePendingReturnStand(Stand));

		Run->SetPendingReturnStand(FTransform(FVector(1.f, 2.f, 3.f)));
		Run->EndAbandonedPractice();
		TestTrue(TEXT("outside practice a rest spot's stand is kept"), Run->ConsumePendingReturnStand(Stand));
	});

	It("refuses to start with no party, and stays out of practice", [this]()
	{
		if (!Run) return;
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		if (UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
		{
			Party->Members.Empty(); // never a real travel from a spec
		}
		AddExpectedError(TEXT("Practice fight refused: no living party member"), EAutomationExpectedErrorFlags::Contains, 1);
		TestFalse(TEXT("refused"), Run->EnterPracticeEncounter());
		TestFalse(TEXT("not a practice fight"), Run->IsPracticeFight());
		TestFalse(TEXT("nothing in the static check either"), UTrialRunSubsystem::IsPracticeFight(TestWorld->GetWorld()));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
