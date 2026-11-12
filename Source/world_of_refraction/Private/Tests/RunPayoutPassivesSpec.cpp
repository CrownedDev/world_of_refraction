// RunPayoutPassivesSpec.cpp
// B-096 (Q-096): an evolution's RunPayoutBonusPercent raises the run-end payout while it is worn and not broken; the
// party's bonuses add up to one multiplier, capped; the orchestrator's fight-end refresh feeds B-073's multiplier.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/EvolutionPlacement.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Trial/RunRewards.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr float RUN_PAYOUT_TEST_BONUS_PERCENT = 20.0f;
	constexpr float RUN_PAYOUT_TEST_HUGE_PERCENT = 500.0f;
	constexpr int32 RUN_PAYOUT_TEST_DURABILITY = 10;
	constexpr int32 RUN_PAYOUT_TEST_SEED = 11;
	constexpr float RUN_PAYOUT_TEST_TOLERANCE = 0.001f;

	FAttachedEvolution RunPayoutTestAttached(float BonusPercent, EEvolutionPlace Place)
	{
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->RunPayoutBonusPercent = BonusPercent;
		FAttachedEvolution Attached;
		Attached.Attachment.Item = Evolution;
		Attached.Attachment.CurrentDurability = RUN_PAYOUT_TEST_DURABILITY;
		Attached.Place = Place;
		return Attached;
	}
}

BEGIN_DEFINE_SPEC(FRunPayoutPassivesSpec, "WoR.Trial.RunPayoutPassives", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FRunPayoutPassivesSpec)

void FRunPayoutPassivesSpec::Define()
{
	It("adds up worn bonuses from any place and skips broken ones", [this]()
	{
		TArray<FAttachedEvolution> Attached = {RunPayoutTestAttached(RUN_PAYOUT_TEST_BONUS_PERCENT, EEvolutionPlace::Character),
											   RunPayoutTestAttached(RUN_PAYOUT_TEST_BONUS_PERCENT, EEvolutionPlace::Weapon)};
		TestEqual(TEXT("two worn: both count"), RunRewards::SumPayoutBonusPercent(Attached), 2.0f * RUN_PAYOUT_TEST_BONUS_PERCENT,
				  RUN_PAYOUT_TEST_TOLERANCE);
		Attached[1].Attachment.Item->Breakability = EBreakability::Breakable;
		Attached[1].Attachment.CurrentDurability = 0;
		TestEqual(TEXT("a broken one gives nothing"), RunRewards::SumPayoutBonusPercent(Attached), RUN_PAYOUT_TEST_BONUS_PERCENT,
				  RUN_PAYOUT_TEST_TOLERANCE);
		TestEqual(TEXT("none worn"), RunRewards::SumPayoutBonusPercent({}), 0.0f, RUN_PAYOUT_TEST_TOLERANCE);
	});

	It("turns the bonus into a multiplier: base plus percent, capped", [this]()
	{
		TestEqual(TEXT("no bonus: base"), RunRewards::PayoutMultiplierForBonus(0.0f), RunRewards::BASE_PAYOUT_MULTIPLIER, RUN_PAYOUT_TEST_TOLERANCE);
		TestEqual(TEXT("+20%: x1.2"), RunRewards::PayoutMultiplierForBonus(RUN_PAYOUT_TEST_BONUS_PERCENT),
				  RunRewards::BASE_PAYOUT_MULTIPLIER + RUN_PAYOUT_TEST_BONUS_PERCENT / 100.0f, RUN_PAYOUT_TEST_TOLERANCE);
		TestEqual(TEXT("capped"), RunRewards::PayoutMultiplierForBonus(RUN_PAYOUT_TEST_HUGE_PERCENT), RunRewards::MAX_PAYOUT_MULTIPLIER,
				  RUN_PAYOUT_TEST_TOLERANCE);
		TestEqual(TEXT("never below base"), RunRewards::PayoutMultiplierForBonus(-RUN_PAYOUT_TEST_BONUS_PERCENT),
				  RunRewards::BASE_PAYOUT_MULTIPLIER, RUN_PAYOUT_TEST_TOLERANCE);
	});

	Describe("a fighter wearing a payout evolution", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("world"), TestWorld->Create());
		});
		AfterEach([this]() { TestWorld.Reset(); });

		It("raises the run-end payout after the fight's refresh, only inside a run", [this]()
		{
			UTrialRunSubsystem *Run = TestWorld->GetSubsystem<UTrialRunSubsystem>();
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->CharacterClass = ECharacterClass::Generic;
			ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data);
			UInventoryComponent *Inventory = Loadout ? Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
			if (!TestTrue(TEXT("setup"), Run && Inventory && Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)))
			{
				return;
			}
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			Active.PrimarySlotType = EPrimarySlotType::Evolution;
			Active.PrimaryEvolution = RunPayoutTestAttached(RUN_PAYOUT_TEST_BONUS_PERCENT, EEvolutionPlace::Character).Attachment;
			const TArray<AActor *> Fighters = {Loadout->GetOwner()};

			Run->RefreshPayoutMultiplier(Fighters);
			TestEqual(TEXT("outside a run: unchanged"), Run->GetRunPayoutMultiplier(), RunRewards::BASE_PAYOUT_MULTIPLIER, RUN_PAYOUT_TEST_TOLERANCE);

			Run->RestoreRun(NewObject<UTrialData>(GetTransientPackage()), RUN_PAYOUT_TEST_SEED, INDEX_NONE, ETrialDifficulty::Medium, {}, {}, {});
			Run->RecordFightReward(TestWorld->GetWorld(), EEncounterRank::Boss);
			const FRunPayout Plain = Run->GetRunEndPayout();
			Run->RefreshPayoutMultiplier(Fighters);
			TestEqual(TEXT("the multiplier"), Run->GetRunPayoutMultiplier(),
					  RunRewards::PayoutMultiplierForBonus(RUN_PAYOUT_TEST_BONUS_PERCENT), RUN_PAYOUT_TEST_TOLERANCE);
			TestTrue(TEXT("the run's end pays more"), Run->GetRunEndPayout().HubGold > Plain.HubGold);

			Active.PrimaryEvolution = FEvolutionAttachment(); // taken off before the next fight
			Run->RefreshPayoutMultiplier(Fighters);
			TestEqual(TEXT("taken off: base again"), Run->GetRunPayoutMultiplier(), RunRewards::BASE_PAYOUT_MULTIPLIER, RUN_PAYOUT_TEST_TOLERANCE);
		});
	});
}

#endif
