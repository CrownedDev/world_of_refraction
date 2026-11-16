// PrebuiltFindSpec.cpp
// B-522b (asset overhaul §3 "Found pieces", Crown Q7): a prebuilt find arrives with its gem or evolution in the socket,
// the evolution owned by the finder; a plain find arrives empty. B-527 (D-226 (1)): a merchant sells one the same way,
// priced as its piece plus its socket.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Merchant/MerchantData.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Trial/PrebuiltFindData.h"
#include "Trial/TrialLoot.h"

namespace
{
	const FCrystalId PREBUILT_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	constexpr int32 PREBUILT_TEST_SEED = 522;
	const TCHAR *PREBUILT_TEST_POOL = TEXT("Source.Zone.Tier4");
}

BEGIN_DEFINE_SPEC(FPrebuiltFindSpec, "WoR.Trial.PrebuiltFind", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Finder = nullptr;
	UInventoryComponent *Inventory = nullptr;
	UEvolutionInventoryComponent *Evolutions = nullptr;
	UCrystalInventoryComponent *Crystals = nullptr;

	static UPrebuiltFindData *NewFind(UEquipmentDataBase *Piece)
	{
		UPrebuiltFindData *Find = NewObject<UPrebuiltFindData>(GetTransientPackage());
		Find->Piece = Piece;
		return Find;
	}
END_DEFINE_SPEC(FPrebuiltFindSpec)

void FPrebuiltFindSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Finder = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Crystals = FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Finder);
		Evolutions = FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Finder);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Finder);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Finder);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Finder);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("gives a prebuilt ring with its evolution in the socket, owned by the finder", [this]()
	{
		UPrebuiltFindData *Find = NewFind(NewObject<URingData>(GetTransientPackage()));
		Find->Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		if (!TestTrue(TEXT("Given"), TrialLoot::GrantTo(Finder, Find)) || !TestEqual(TEXT("One ring"), Inventory->Rings.Num(), 1)) return;
		const FRingInventoryEntry &Ring = Inventory->Rings.Last();
		TestTrue(TEXT("The ring is the find's piece"), Ring.Ring == Find->Piece);
		TestTrue(TEXT("An evolution in the socket"), Ring.AttachedItem.IsEvolution());
		if (TestEqual(TEXT("The evolution is owned"), Evolutions->Entries.Num(), 1))
		{
			TestEqual(TEXT("The socket holds the owned one"), Ring.AttachedItem.Evolution.InstanceID, Evolutions->Entries[0].InstanceID);
		}
	});

	It("gives a prebuilt weapon with its gem in the socket, none left in the pouch", [this]()
	{
		UPrebuiltFindData *Find = NewFind(NewObject<UWeaponData>(GetTransientPackage()));
		Find->Crystal = PREBUILT_TEST_GEM;
		if (!TestTrue(TEXT("Given"), TrialLoot::GrantTo(Finder, Find)) || !TestEqual(TEXT("One weapon"), Inventory->Weapons.Num(), 1)) return;
		const FWeaponInventoryEntry &Weapon = Inventory->Weapons.Last();
		TestTrue(TEXT("A gem in the socket"), Weapon.AttachedItem.IsCrystal());
		TestTrue(TEXT("The find's gem"), Weapon.AttachedItem.Crystal.Id == PREBUILT_TEST_GEM);
		TestEqual(TEXT("None loose"), Crystals->GetCount(PREBUILT_TEST_GEM), 0);
	});

	It("gives a plain ring empty, and refuses a find with no piece", [this]()
	{
		if (!TestTrue(TEXT("Given"), TrialLoot::GrantTo(Finder, NewObject<URingData>(GetTransientPackage())))) return;
		TestTrue(TEXT("Empty socket"), Inventory->Rings.Last().AttachedItem.IsEmpty());
		TestFalse(TEXT("No piece: refused"), TrialLoot::GrantTo(Finder, NewFind(nullptr)));
	});

	It("rolls in its pool by its own tags and is named with its socket", [this]()
	{
		const FGameplayTag Pool = FGameplayTag::RequestGameplayTag(PREBUILT_TEST_POOL);
		URingData *Piece = NewObject<URingData>(GetTransientPackage());
		Piece->Name = TEXT("Adapter Ring");
		UPrebuiltFindData *Find = NewFind(Piece);
		Find->SourceTags.AddTag(Pool);
		TestTrue(TEXT("Picked from its pool"), TrialLoot::PickFromPool({Find}, Pool, PREBUILT_TEST_SEED) == Find);
		TestEqual(TEXT("Empty: the piece's name"), TrialLoot::GetItemName(Find).ToString(), FString(TEXT("Adapter Ring")));
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Evolution->ItemName = TEXT("Phoenix Fire");
		Find->Evolution = Evolution;
		TestEqual(TEXT("With its evolution"), TrialLoot::GetItemName(Find).ToString(), FString(TEXT("Adapter Ring with Phoenix Fire")));
	});

	It("is sold by a merchant with its gem in, priced as the ring plus the gem (B-527, D-226)", [this]()
	{
		UEconomyService *Economy = TestWorld->GetSubsystem<UEconomyService>();
		UCurrencyComponent *Wallet = Finder->FindComponentByClass<UCurrencyComponent>();
		if (!TestNotNull(TEXT("EconomyService"), Economy) || !TestNotNull(TEXT("Wallet"), Wallet)) return;
		URingData *Piece = NewObject<URingData>(GetTransientPackage());
		Piece->Tier = EItemTier::F_Tier;
		UPrebuiltFindData *Find = NewFind(Piece);
		Find->Crystal = PREBUILT_TEST_GEM;
		FMerchantStockEntry Prebuilt;
		Prebuilt.Asset = Find;
		FMerchantStockEntry Ring;
		Ring.Asset = Piece;
		FMerchantStockEntry Gem;
		Gem.Crystal = PREBUILT_TEST_GEM;
		const FPurchaseCost Cost = Economy->PreviewCartCost({Prebuilt});
		const FPurchaseCost Parts = Economy->PreviewCartCost({Ring, Gem});
		TestEqual(TEXT("Prisms: the ring's plus the gem's"), Cost.Prisms, Parts.Prisms);
		TestEqual(TEXT("Elemental: the gem's"), Cost.ElementalEssence, Parts.ElementalEssence);
		TestTrue(TEXT("Dearer than the bare ring"), Cost.Prisms > Economy->PreviewCartCost({Ring}).Prisms);

		Wallet->Add(ECurrencyType::Prisms, Cost.Prisms);
		Wallet->Add(ECurrencyType::ElementalEssence, Cost.ElementalEssence);
		if (!TestTrue(TEXT("Bought"), Economy->Purchase(Finder, {Prebuilt})) || !TestEqual(TEXT("One ring"), Inventory->Rings.Num(), 1)) return;
		TestTrue(TEXT("The gem in the socket"), Inventory->Rings.Last().AttachedItem.IsCrystal() &&
													 Inventory->Rings.Last().AttachedItem.Crystal.Id == PREBUILT_TEST_GEM);
		TestEqual(TEXT("None loose"), Crystals->GetCount(PREBUILT_TEST_GEM), 0);
		TestEqual(TEXT("All Prisms spent"), Wallet->GetBalance(ECurrencyType::Prisms), 0);
	});
}

#endif
