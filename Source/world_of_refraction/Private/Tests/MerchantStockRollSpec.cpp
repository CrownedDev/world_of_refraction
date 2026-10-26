// MerchantStockRollSpec.cpp
// B-179 (Crown 2026-10-26, rules page 05): a trial vendor rolls a random part of its stock per run
// from the run seed; a home shop keeps its full, fixed list.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Merchant/MerchantData.h"
#include "Merchant/MerchantStockRoll.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	constexpr int32 STOCK_ROLL_TEST_POOL_SIZE = 20;
	constexpr int32 STOCK_ROLL_TEST_SEED = 12345;
	constexpr int32 STOCK_ROLL_TEST_OTHER_SEEDS = 10;

	/** A merchant whose lines are told apart by Count (1..Lines), in authored order. */
	UMerchantData *StockRollTestMerchant(int32 Lines)
	{
		UMerchantData *Merchant = NewObject<UMerchantData>(GetTransientPackage());
		for (int32 i = 0; i < Lines; ++i)
		{
			FMerchantStockEntry Entry;
			Entry.Crystal = FCrystalId(ECrystalType::Garnet, EItemTier::E_Tier);
			Entry.Count = i + 1;
			Merchant->Stock.Add(Entry);
		}
		return Merchant;
	}

	TArray<int32> StockRollTestLines(const TArray<FMerchantStockEntry> &Stock)
	{
		TArray<int32> Lines;
		for (const FMerchantStockEntry &Entry : Stock)
		{
			Lines.Add(Entry.Count);
		}
		return Lines;
	}
}

BEGIN_DEFINE_SPEC(FMerchantStockRollSpec, "WoR.Merchant.StockRoll",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UMerchantData *Merchant = nullptr;
END_DEFINE_SPEC(FMerchantStockRollSpec)

void FMerchantStockRollSpec::Define()
{
	BeforeEach([this]()
	{
		Merchant = StockRollTestMerchant(STOCK_ROLL_TEST_POOL_SIZE);
		Merchant->AddToRoot(); // the trial-run case creates a world, which may collect garbage
	});

	AfterEach([this]()
	{
		Merchant->RemoveFromRoot();
		Merchant = nullptr;
	});

	Describe("a home shop", [this]()
	{
		It("shows its full authored list", [this]()
		{
			TestEqual(TEXT("full list"), StockRollTestLines(MerchantStockRoll::StockFor(*Merchant, false, STOCK_ROLL_TEST_SEED)),
					  StockRollTestLines(Merchant->Stock));
		});

		It("shows its full list through the trial run when no trial is active", [this]()
		{
			FWoRTestWorld TestWorld;
			TestTrue(TEXT("test world created"), TestWorld.Create());
			const UTrialRunSubsystem *TrialRun = TestWorld.GetSubsystem<UTrialRunSubsystem>();
			if (!TestNotNull(TEXT("trial run subsystem"), TrialRun))
			{
				return;
			}
			TestEqual(TEXT("no run seed"), TrialRun->GetRunSeed(), 0);
			TestEqual(TEXT("full list"), TrialRun->GetMerchantStock(Merchant).Num(), STOCK_ROLL_TEST_POOL_SIZE);
		});
	});

	Describe("a trial vendor", [this]()
	{
		It("rolls the same stock from the same seed", [this]()
		{
			TestEqual(TEXT("same stock"), StockRollTestLines(MerchantStockRoll::StockFor(*Merchant, true, STOCK_ROLL_TEST_SEED)),
					  StockRollTestLines(MerchantStockRoll::StockFor(*Merchant, true, STOCK_ROLL_TEST_SEED)));
		});

		It("shows TRIAL_VENDOR_STOCK_SIZE different lines, in authored order", [this]()
		{
			const TArray<int32> Lines = StockRollTestLines(MerchantStockRoll::StockFor(*Merchant, true, STOCK_ROLL_TEST_SEED));
			TestEqual(TEXT("stock size"), Lines.Num(), MerchantStockRoll::TRIAL_VENDOR_STOCK_SIZE);
			for (int32 i = 1; i < Lines.Num(); ++i)
			{
				TestTrue(TEXT("authored order, no repeats"), Lines[i - 1] < Lines[i]);
			}
		});

		It("rolls a different stock on another run", [this]()
		{
			const TArray<int32> First = StockRollTestLines(MerchantStockRoll::StockFor(*Merchant, true, STOCK_ROLL_TEST_SEED));
			bool bAnyDifferent = false;
			for (int32 i = 1; i <= STOCK_ROLL_TEST_OTHER_SEEDS; ++i)
			{
				bAnyDifferent |= StockRollTestLines(MerchantStockRoll::StockFor(*Merchant, true, STOCK_ROLL_TEST_SEED + i)) != First;
			}
			TestTrue(TEXT("another seed rolls other lines"), bAnyDifferent);
		});

		It("shows everything when its list is smaller than the stock size", [this]()
		{
			UMerchantData *Small = StockRollTestMerchant(MerchantStockRoll::TRIAL_VENDOR_STOCK_SIZE - 1);
			TestEqual(TEXT("whole list"), MerchantStockRoll::StockFor(*Small, true, STOCK_ROLL_TEST_SEED).Num(),
					  MerchantStockRoll::TRIAL_VENDOR_STOCK_SIZE - 1);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
