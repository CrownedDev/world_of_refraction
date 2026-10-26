// CrystalMergeSpec.cpp
// ER-28 (Crown, 2026-10-26; B-177): a crystal merge gives back its overshoot as the fewest
// lower-tier crystals of the same type; an exact merge is unchanged.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"

namespace
{
	/** Plenty for any merge's Prism cost. */
	constexpr int32 MERGE_TEST_PRISMS = 10000;
	constexpr ECrystalType MERGE_TEST_TYPE = ECrystalType::Garnet;

	template <class TComponent>
	TComponent *MergeTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FCrystalMergeSpec, "WoR.Economy.CrystalMerge",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UEconomyService *Economy = nullptr;
	AActor *Owner = nullptr;
	UInventoryComponent *Inventory = nullptr;
	UCrystalInventoryComponent *Crystals = nullptr;

	void SpawnOwner()
	{
		Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Crystals = MergeTestAddComponent<UCrystalInventoryComponent>(Owner);
		UCurrencyComponent *Currency = MergeTestAddComponent<UCurrencyComponent>(Owner);
		Inventory = MergeTestAddComponent<UInventoryComponent>(Owner);
		Currency->Add(ECurrencyType::Prisms, MERGE_TEST_PRISMS);
	}

	int32 Count(EItemTier Tier) const
	{
		return Crystals->GetCount(FCrystalId(MERGE_TEST_TYPE, Tier));
	}

	void Give(EItemTier Tier, int32 Num)
	{
		TArray<FCrystalId> Ids;
		Ids.Init(FCrystalId(MERGE_TEST_TYPE, Tier), Num);
		TestTrue(TEXT("crystals given"), Inventory->AddCrystals(Ids));
	}
END_DEFINE_SPEC(FCrystalMergeSpec)

void FCrystalMergeSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Economy = TestWorld->GetSubsystem<UEconomyService>();
		TestNotNull(TEXT("economy service"), Economy);
	});

	AfterEach([this]()
	{
		Economy = nullptr;
		Owner = nullptr;
		Inventory = nullptr;
		Crystals = nullptr;
		TestWorld.Reset();
	});

	Describe("the change", [this]()
	{
		It("is empty for an exact merge", [this]()
		{
			TestEqual(TEXT("no change"), EconomyYield::GetMergeChange(0, EItemTier::C_Tier).Num(), 0);
		});

		It("uses the fewest crystals, highest tier first, below the target", [this]()
		{
			TestEqual(TEXT("2 -> one E"), EconomyYield::GetMergeChange(2, EItemTier::C_Tier), TArray<EItemTier>({EItemTier::E_Tier}));
			TestEqual(TEXT("3 -> E + F"), EconomyYield::GetMergeChange(3, EItemTier::C_Tier), TArray<EItemTier>({EItemTier::E_Tier, EItemTier::F_Tier}));
			TestEqual(TEXT("6 -> D + E"), EconomyYield::GetMergeChange(6, EItemTier::B_Tier), TArray<EItemTier>({EItemTier::D_Tier, EItemTier::E_Tier}));
			TestEqual(TEXT("31 below S -> B C D E F"), EconomyYield::GetMergeChange(31, EItemTier::S_Tier),
					  TArray<EItemTier>({EItemTier::B_Tier, EItemTier::C_Tier, EItemTier::D_Tier, EItemTier::E_Tier, EItemTier::F_Tier}));
		});
	});

	Describe("a merge", [this]()
	{
		It("is unchanged when exact", [this]()
		{
			SpawnOwner();
			Give(EItemTier::D_Tier, 2);
			TestTrue(TEXT("merged"), Economy->MergeCrystals(Owner, MERGE_TEST_TYPE, EItemTier::C_Tier));
			TestEqual(TEXT("one C"), Count(EItemTier::C_Tier), 1);
			TestEqual(TEXT("both D spent"), Count(EItemTier::D_Tier), 0);
			TestEqual(TEXT("no change"), Count(EItemTier::E_Tier) + Count(EItemTier::F_Tier), 0);
		});

		It("gives back its overshoot", [this]()
		{
			// Lowest-first: E (2) + D (4) + D (4) = 10 for a C (8), so 2 comes back as one E.
			SpawnOwner();
			Give(EItemTier::E_Tier, 1);
			Give(EItemTier::D_Tier, 2);
			TestTrue(TEXT("merged"), Economy->MergeCrystals(Owner, MERGE_TEST_TYPE, EItemTier::C_Tier));
			TestEqual(TEXT("one C"), Count(EItemTier::C_Tier), 1);
			TestEqual(TEXT("both D spent"), Count(EItemTier::D_Tier), 0);
			TestEqual(TEXT("one E back"), Count(EItemTier::E_Tier), 1);
			TestEqual(TEXT("no F"), Count(EItemTier::F_Tier), 0);
		});

		It("can give change at a tier it didn't spend from", [this]()
		{
			// F (1) x3 + D (4) x2 = 11 for a C (8): 3 back as one E + one F.
			SpawnOwner();
			Give(EItemTier::F_Tier, 3);
			Give(EItemTier::D_Tier, 2);
			TestTrue(TEXT("merged"), Economy->MergeCrystals(Owner, MERGE_TEST_TYPE, EItemTier::C_Tier));
			TestEqual(TEXT("one C"), Count(EItemTier::C_Tier), 1);
			TestEqual(TEXT("both D spent"), Count(EItemTier::D_Tier), 0);
			TestEqual(TEXT("one E back"), Count(EItemTier::E_Tier), 1);
			TestEqual(TEXT("one F back"), Count(EItemTier::F_Tier), 1);
		});

		It("is refunded whole when the change doesn't fit (Q-177)", [this]()
		{
			// Same merge as above, but the shared E tier is already full of another gem type.
			SpawnOwner();
			TArray<FCrystalId> FullTier;
			FullTier.Init(FCrystalId(ECrystalType::Sapphire, EItemTier::E_Tier), InventoryConstants::CRYSTAL_PER_TIER_CAP);
			TestTrue(TEXT("E tier filled"), Inventory->AddCrystals(FullTier));
			Give(EItemTier::F_Tier, 3);
			Give(EItemTier::D_Tier, 2);
			UCurrencyComponent *Currency = Owner->FindComponentByClass<UCurrencyComponent>();

			TestFalse(TEXT("refused"), Economy->MergeCrystals(Owner, MERGE_TEST_TYPE, EItemTier::C_Tier));
			TestEqual(TEXT("no C"), Count(EItemTier::C_Tier), 0);
			TestEqual(TEXT("D refunded"), Count(EItemTier::D_Tier), 2);
			TestEqual(TEXT("F refunded"), Count(EItemTier::F_Tier), 3);
			TestEqual(TEXT("Prisms refunded"), Currency->GetBalance(ECurrencyType::Prisms), MERGE_TEST_PRISMS);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
