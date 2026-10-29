// EconomyNumbersSpec.cpp
// B-290: pins the economy numbers to the rules page (docs/Rules/05-economy-and-runs.md).
// ER-24 / D-102: a run vendor's world-stat point costs 50 Gold, +25 for each one bought this run
//                (base + N x bought), and the ramp restarts with the run.
// ER-29: a merge costs half the produced tier's Prism price (E 25 ... S 800).
// ER-23: a spell costs element essence at its tier (F 10 ... S 48), pillar essence per scaling
//        grade and +50 Prisms per grade (F=1 ... S=7). D-105: Quartz's essence is the Generic one.
// ER-38: fresh-pickup quality weights F 26 / E 22 / D 18 / C 14 / B 10 / A 6 / S 4; at maxed Luck
//        S goes to about 7% (tilt 0.5). Rolled on a fixed seed.
// Not pinned here (code and ruling disagree, reported on B-290): the world-stat cap (LIVE_MAX 20
// vs D-102's 7 per pillar) and D-105's Quartz-essence price for Generic spells.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "HAL/PlatformTime.h"
#include "Character/CharacterDataComponent.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Merchant/MerchantData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	// ER-24 / D-102
	constexpr int32 ECONOMY_NUMBERS_WORLDSTAT_FIRST = 50;
	constexpr int32 ECONOMY_NUMBERS_WORLDSTAT_STEP = 25;
	constexpr int32 ECONOMY_NUMBERS_WORLDSTAT_SECOND = 75;

	// ER-29: E 25, D 50, C 100, B 200, A 400, S 800.
	constexpr int32 ECONOMY_NUMBERS_MERGE_E = 25;
	constexpr int32 ECONOMY_NUMBERS_MERGE_D = 50;
	constexpr int32 ECONOMY_NUMBERS_MERGE_C = 100;
	constexpr int32 ECONOMY_NUMBERS_MERGE_B = 200;
	constexpr int32 ECONOMY_NUMBERS_MERGE_A = 400;
	constexpr int32 ECONOMY_NUMBERS_MERGE_S = 800;
	constexpr int32 ECONOMY_NUMBERS_MERGE_PRISMS = 1000;
	constexpr ECrystalType ECONOMY_NUMBERS_MERGE_TYPE = ECrystalType::Garnet;

	// ER-23 (and ER-22's D-tier Prism price 100).
	constexpr int32 ECONOMY_NUMBERS_ESSENCE_F = 10;
	constexpr int32 ECONOMY_NUMBERS_ESSENCE_D = 17;
	constexpr int32 ECONOMY_NUMBERS_ESSENCE_S = 48;
	constexpr int32 ECONOMY_NUMBERS_SURCHARGE_PER_GRADE = 50;
	constexpr int32 ECONOMY_NUMBERS_PRISMS_D = 100;
	constexpr int32 ECONOMY_NUMBERS_GRADE_C_NUMBER = 4;

	// ER-38
	constexpr float ECONOMY_NUMBERS_QUALITY_WEIGHTS[] = {26.0f, 22.0f, 18.0f, 14.0f, 10.0f, 6.0f, 4.0f}; // F .. S
	constexpr int32 ECONOMY_NUMBERS_QUALITY_GRADES = static_cast<int32>(UE_ARRAY_COUNT(ECONOMY_NUMBERS_QUALITY_WEIGHTS));
	constexpr float ECONOMY_NUMBERS_LUCK_TILT = 0.5f;
	constexpr float ECONOMY_NUMBERS_MAXED_LUCK = 1.0f;
	/** At maxed Luck the 0.5 tilt makes S weigh 6 of 82.33 (13, 14.67, 15, 14, 11.67, 8, 6): 7.29%, the page's
	 *  "about 7%". Pinned to the computed share so a halved tilt (about 5.5%) fails. */
	constexpr float ECONOMY_NUMBERS_MAXED_LUCK_S_SHARE = 0.0729f;
	/** About 3 standard deviations of a 7.3% share over 10 000 rolls. */
	constexpr float ECONOMY_NUMBERS_S_SHARE_TOLERANCE = 0.008f;
	constexpr int32 ECONOMY_NUMBERS_QUALITY_SEED = 2908;
	constexpr int32 ECONOMY_NUMBERS_QUALITY_ROLLS = 10000;
	/** Allowed gap per grade share: about 3 standard deviations of the widest share over 10 000 rolls. */
	constexpr float ECONOMY_NUMBERS_SHARE_TOLERANCE = 0.015f;

	/** Share of each quality grade over Rolls seeded rolls. RollQuality draws from FMath's global random, so it is
	 *  seeded here. TODO: let RollQuality take an FRandomStream (like BattleRandom) so this needn't touch it. */
	TArray<float> EconomyNumbersQualityShares(float NormalizedLuck)
	{
		TArray<int32> Counts;
		Counts.Init(0, ECONOMY_NUMBERS_QUALITY_GRADES);
		FMath::RandInit(ECONOMY_NUMBERS_QUALITY_SEED);
		for (int32 Roll = 0; Roll < ECONOMY_NUMBERS_QUALITY_ROLLS; ++Roll)
		{
			const int32 Grade = static_cast<int32>(EconomyYield::RollQuality(NormalizedLuck));
			if (Counts.IsValidIndex(Grade))
			{
				++Counts[Grade];
			}
		}
		// Not re-seeded from the clock afterwards: that would undo a -FixedSeed run for every later test.

		TArray<float> Shares;
		for (const int32 Count : Counts)
		{
			Shares.Add(static_cast<float>(Count) / ECONOMY_NUMBERS_QUALITY_ROLLS);
		}
		return Shares;
	}

	USpellData *EconomyNumbersFireSpell(EItemTier Tier)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Element = ESpellElement::Fire;
		Spell->Tier = Tier;
		return Spell;
	}
}

BEGIN_DEFINE_SPEC(FEconomyNumbersSpec, "WoR.Economy.Numbers",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UEconomyService *Economy = nullptr;
	AActor *Owner = nullptr;
	UCurrencyComponent *Wallet = nullptr;
	UCharacterDataComponent *CharComp = nullptr;
	UInventoryComponent *Inventory = nullptr;
	UCrystalInventoryComponent *Crystals = nullptr;

	FPurchaseCost SpellCost(USpellData *Spell) const
	{
		FMerchantStockEntry Entry;
		Entry.Asset = Spell;
		return Economy->PreviewCartCost({Entry});
	}
END_DEFINE_SPEC(FEconomyNumbersSpec)

void FEconomyNumbersSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Economy = TestWorld->GetSubsystem<UEconomyService>();
		Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (Owner)
		{
			CharComp = Owner->FindComponentByClass<UCharacterDataComponent>();
			Crystals = FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Owner);
			Wallet = FWoRTestWorld::AddComponent<UCurrencyComponent>(Owner);
			Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);
		}
		if (!TestNotNull(TEXT("EconomyService"), Economy) || !TestNotNull(TEXT("Wallet"), Wallet) ||
			!TestNotNull(TEXT("CharacterData"), CharComp) || !TestNotNull(TEXT("Inventory"), Inventory) ||
			!TestNotNull(TEXT("Crystals"), Crystals))
		{
			Economy = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Economy = nullptr;
		Owner = nullptr;
		Wallet = nullptr;
		CharComp = nullptr;
		Inventory = nullptr;
		Crystals = nullptr;
		TestWorld.Reset();
	});

	Describe("the world-stat vendor (ER-24, D-102)", [this]()
	{
		It("starts at 50 Gold and adds 25 per point bought", [this]()
		{
			TestEqual(TEXT("WORLDSTAT_BUY_BASE_COST"), EconomyYield::Constants::WORLDSTAT_BUY_BASE_COST, ECONOMY_NUMBERS_WORLDSTAT_FIRST);
			TestEqual(TEXT("WORLDSTAT_BUY_COST_STEP"), EconomyYield::Constants::WORLDSTAT_BUY_COST_STEP, ECONOMY_NUMBERS_WORLDSTAT_STEP);
		});

		It("charges 50, then 75, then refuses what the wallet can't cover", [this]()
		{
			if (!Economy)
			{
				return;
			}
			Wallet->Add(ECurrencyType::Gold, ECONOMY_NUMBERS_WORLDSTAT_FIRST + ECONOMY_NUMBERS_WORLDSTAT_SECOND);
			const int32 MindBefore = CharComp->GetWorldStat(EWorldPillar::Mind);

			TestTrue(TEXT("first buy"), Economy->BuyWorldStat(Owner, EWorldPillar::Mind));
			TestEqual(TEXT("first buy costs 50"), Wallet->GetBalance(ECurrencyType::Gold), ECONOMY_NUMBERS_WORLDSTAT_SECOND);
			TestEqual(TEXT("Mind +1"), CharComp->GetWorldStat(EWorldPillar::Mind), MindBefore + 1);

			TestTrue(TEXT("second buy, another pillar"), Economy->BuyWorldStat(Owner, EWorldPillar::Body));
			TestEqual(TEXT("second buy costs 75"), Wallet->GetBalance(ECurrencyType::Gold), 0);

			Wallet->Add(ECurrencyType::Gold, ECONOMY_NUMBERS_WORLDSTAT_SECOND);
			TestFalse(TEXT("third buy (100) refused at 75 Gold"), Economy->BuyWorldStat(Owner, EWorldPillar::Spirit));
			TestEqual(TEXT("nothing spent"), Wallet->GetBalance(ECurrencyType::Gold), ECONOMY_NUMBERS_WORLDSTAT_SECOND);
		});

		It("starts again at 50 when the run resets", [this]()
		{
			if (!Economy)
			{
				return;
			}
			Wallet->Add(ECurrencyType::Gold, ECONOMY_NUMBERS_WORLDSTAT_FIRST + ECONOMY_NUMBERS_WORLDSTAT_FIRST);
			TestTrue(TEXT("first buy"), Economy->BuyWorldStat(Owner, EWorldPillar::Mind));
			CharComp->ResetRunWorldStats();
			TestTrue(TEXT("first buy of the next run"), Economy->BuyWorldStat(Owner, EWorldPillar::Mind));
			TestEqual(TEXT("both cost 50"), Wallet->GetBalance(ECurrencyType::Gold), 0);
		});
	});

	Describe("a crystal merge (ER-29)", [this]()
	{
		It("costs half the produced tier's Prism price", [this]()
		{
			TestEqual(TEXT("to E"), EconomyYield::GetMergeCostForTier(EItemTier::E_Tier), ECONOMY_NUMBERS_MERGE_E);
			TestEqual(TEXT("to D"), EconomyYield::GetMergeCostForTier(EItemTier::D_Tier), ECONOMY_NUMBERS_MERGE_D);
			TestEqual(TEXT("to C"), EconomyYield::GetMergeCostForTier(EItemTier::C_Tier), ECONOMY_NUMBERS_MERGE_C);
			TestEqual(TEXT("to B"), EconomyYield::GetMergeCostForTier(EItemTier::B_Tier), ECONOMY_NUMBERS_MERGE_B);
			TestEqual(TEXT("to A"), EconomyYield::GetMergeCostForTier(EItemTier::A_Tier), ECONOMY_NUMBERS_MERGE_A);
			TestEqual(TEXT("to S"), EconomyYield::GetMergeCostForTier(EItemTier::S_Tier), ECONOMY_NUMBERS_MERGE_S);
		});

		It("takes 100 Prisms for a merge to C", [this]()
		{
			if (!Economy)
			{
				return;
			}
			Wallet->Add(ECurrencyType::Prisms, ECONOMY_NUMBERS_MERGE_PRISMS);
			TArray<FCrystalId> Ids;
			Ids.Init(FCrystalId(ECONOMY_NUMBERS_MERGE_TYPE, EItemTier::D_Tier), 2); // 4 + 4 = one C (8)
			TestTrue(TEXT("crystals given"), Inventory->AddCrystals(Ids));
			TestTrue(TEXT("merged"), Economy->MergeCrystals(Owner, ECONOMY_NUMBERS_MERGE_TYPE, EItemTier::C_Tier));
			TestEqual(TEXT("one C made"), Crystals->GetCount(FCrystalId(ECONOMY_NUMBERS_MERGE_TYPE, EItemTier::C_Tier)), 1);
			TestEqual(TEXT("100 Prisms spent"), Wallet->GetBalance(ECurrencyType::Prisms),
					  ECONOMY_NUMBERS_MERGE_PRISMS - ECONOMY_NUMBERS_MERGE_C);
		});
	});

	Describe("a spell's price (ER-23, D-105)", [this]()
	{
		It("charges element essence at its tier: F 10, D 17, S 48", [this]()
		{
			TestEqual(TEXT("TYPED_ESSENCE_BUY_F"), EconomyYield::Constants::TYPED_ESSENCE_BUY_F, ECONOMY_NUMBERS_ESSENCE_F);
			TestEqual(TEXT("TYPED_ESSENCE_BUY_D"), EconomyYield::Constants::TYPED_ESSENCE_BUY_D, ECONOMY_NUMBERS_ESSENCE_D);
			TestEqual(TEXT("TYPED_ESSENCE_BUY_S"), EconomyYield::Constants::TYPED_ESSENCE_BUY_S, ECONOMY_NUMBERS_ESSENCE_S);
			if (!Economy)
			{
				return;
			}
			TestEqual(TEXT("F Fire spell: Fire essence"),
					  SpellCost(EconomyNumbersFireSpell(EItemTier::F_Tier)).Typed.FindRef(EEssenceType::Fire), ECONOMY_NUMBERS_ESSENCE_F);
			TestEqual(TEXT("D Fire spell: Fire essence"),
					  SpellCost(EconomyNumbersFireSpell(EItemTier::D_Tier)).Typed.FindRef(EEssenceType::Fire), ECONOMY_NUMBERS_ESSENCE_D);
			TestEqual(TEXT("S Fire spell: Fire essence"),
					  SpellCost(EconomyNumbersFireSpell(EItemTier::S_Tier)).Typed.FindRef(EEssenceType::Fire), ECONOMY_NUMBERS_ESSENCE_S);
		});

		It("adds pillar essence and 50 Prisms per scaling grade number", [this]()
		{
			TestEqual(TEXT("PRISMS_SCALING_SURCHARGE_PER_GRADE"), EconomyYield::Constants::PRISMS_SCALING_SURCHARGE_PER_GRADE,
					  ECONOMY_NUMBERS_SURCHARGE_PER_GRADE);
			TestEqual(TEXT("grade F is 1"), EconomyYield::GetScalingGradeNumber(EScalingTier::F), 1);
			TestEqual(TEXT("grade C is 4"), EconomyYield::GetScalingGradeNumber(EScalingTier::C), ECONOMY_NUMBERS_GRADE_C_NUMBER);
			TestEqual(TEXT("grade S is 7"), EconomyYield::GetScalingGradeNumber(EScalingTier::S), 7);
			if (!Economy)
			{
				return;
			}
			USpellData *Spell = EconomyNumbersFireSpell(EItemTier::D_Tier);
			FStatScaling Scaling;
			Scaling.Stat = ESubStat::SpellDamage; // a Mind sub-stat
			Scaling.Tier = EScalingTier::C;
			Spell->StatScaling.Add(Scaling);

			const FPurchaseCost Cost = SpellCost(Spell);
			TestEqual(TEXT("Prisms: D base 100 + 50 x 4"), Cost.Prisms,
					  ECONOMY_NUMBERS_PRISMS_D + ECONOMY_NUMBERS_SURCHARGE_PER_GRADE * ECONOMY_NUMBERS_GRADE_C_NUMBER);
			TestEqual(TEXT("Fire essence at D"), Cost.Typed.FindRef(EEssenceType::Fire), ECONOMY_NUMBERS_ESSENCE_D);
			TestEqual(TEXT("Mind essence at grade C"), Cost.Typed.FindRef(EEssenceType::Mind),
					  EconomyYield::GetTypedEssencePurchaseCostForTier(EItemTier::C_Tier));
		});

		It("gives Quartz's essence as the Generic (Quartz) essence (D-105)", [this]()
		{
			TestEqual(TEXT("dismantled Quartz"), EconomyYield::ResolveEssenceType(FCrystalId(ECrystalType::Quartz, EItemTier::F_Tier)),
					  EEssenceType::Generic);
		});
	});

	Describe("drop quality (ER-38)", [this]()
	{
		It("weighs F 26 / E 22 / D 18 / C 14 / B 10 / A 6 / S 4, tilted 0.5 at maxed Luck", [this]()
		{
			const float Weights[] = {
				EconomyYield::Constants::QUALITY_WEIGHT_F, EconomyYield::Constants::QUALITY_WEIGHT_E, EconomyYield::Constants::QUALITY_WEIGHT_D,
				EconomyYield::Constants::QUALITY_WEIGHT_C, EconomyYield::Constants::QUALITY_WEIGHT_B, EconomyYield::Constants::QUALITY_WEIGHT_A,
				EconomyYield::Constants::QUALITY_WEIGHT_S};
			static_assert(static_cast<int32>(UE_ARRAY_COUNT(Weights)) == ECONOMY_NUMBERS_QUALITY_GRADES, "one rule weight per grade");
			for (int32 Grade = 0; Grade < ECONOMY_NUMBERS_QUALITY_GRADES; ++Grade)
			{
				TestEqual(*FString::Printf(TEXT("weight of grade %d"), Grade), Weights[Grade], ECONOMY_NUMBERS_QUALITY_WEIGHTS[Grade]);
			}
			TestEqual(TEXT("QUALITY_LUCK_MAX_TILT"), EconomyYield::Constants::QUALITY_LUCK_MAX_TILT, ECONOMY_NUMBERS_LUCK_TILT);
		});

		It("rolls each grade at its weight with no Luck, on a fixed seed", [this]()
		{
			const TArray<float> Shares = EconomyNumbersQualityShares(0.0f);
			for (int32 Grade = 0; Grade < ECONOMY_NUMBERS_QUALITY_GRADES; ++Grade)
			{
				TestNearlyEqual(*FString::Printf(TEXT("share of grade %d"), Grade), Shares[Grade],
								ECONOMY_NUMBERS_QUALITY_WEIGHTS[Grade] / 100.0f, ECONOMY_NUMBERS_SHARE_TOLERANCE);
			}
		});

		It("rolls S about 7% of the time at maxed Luck, on a fixed seed", [this]()
		{
			const TArray<float> Shares = EconomyNumbersQualityShares(ECONOMY_NUMBERS_MAXED_LUCK);
			TestNearlyEqual(TEXT("S share"), Shares.Last(), ECONOMY_NUMBERS_MAXED_LUCK_S_SHARE, ECONOMY_NUMBERS_S_SHARE_TOLERANCE);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
