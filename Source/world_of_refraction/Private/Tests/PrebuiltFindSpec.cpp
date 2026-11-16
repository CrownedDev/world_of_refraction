// PrebuiltFindSpec.cpp
// B-522b (asset overhaul §3 "Found pieces", Crown Q7): a prebuilt find arrives with its gem or evolution in the socket,
// the evolution owned by the finder; a plain find arrives empty. B-527 (D-226 (1)): a merchant sells one the same way,
// priced as its piece plus its socket. B-532 (D-231): a prebuilt keeps its own crystal unless its setting lets it out; its
// second socket takes a stone only; a ring the player built is unchanged.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Merchant/MerchantData.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/GearSockets.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FGearInstance.h"
#include "Loadout/LoadoutComponent.h"
#include "Trial/PrebuiltFindData.h"
#include "Trial/TrialLoot.h"
#include "UI/Menus/LoadoutScreenModel.h"

namespace
{
	const FCrystalId PREBUILT_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId PREBUILT_TEST_OTHER_GEM(ECrystalType::Sapphire, EItemTier::F_Tier);
	const FCrystalId PREBUILT_TEST_STONE(ECrystalType::DamageStone, EItemTier::F_Tier);
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

	/** The loadout screen's view of an owned ring: both sockets filled, fixed by the one rule (D-231). */
	static FLoadoutScreenRow ScreenRowFor(const FRingInventoryEntry &Ring)
	{
		FLoadoutScreenRow Row;
		Row.Kind = ELoadoutScreenRowKind::Adapter;
		Row.GearID = Ring.PersistentID;
		Row.Sockets.SetNum(2);
		Row.Sockets[LoadoutScreenSockets::CRYSTAL].bEmpty = Ring.AttachedItem.IsEmpty();
		Row.Sockets[LoadoutScreenSockets::STONE].bEmpty = Ring.StoneSocket.IsEmpty();
		Row.bSocketsFixed = GearSockets::IsSocketFixed(FGearInstance::FromRingEntry(Ring));
		return Row;
	}

	/** A ring find with PREBUILT_TEST_GEM in, granted; then a second gem and a stone in the pouch. */
	const FRingInventoryEntry *GrantGemRing(bool bRemovable)
	{
		UPrebuiltFindData *Find = NewFind(NewObject<URingData>(GetTransientPackage()));
		Find->Crystal = PREBUILT_TEST_GEM;
		Find->bCrystalRemovable = bRemovable;
		if (!TrialLoot::GrantTo(Finder, Find) || Inventory->Rings.Num() != 1)
		{
			return nullptr;
		}
		Inventory->AddCrystal(PREBUILT_TEST_OTHER_GEM);
		Inventory->AddCrystal(PREBUILT_TEST_STONE);
		return &Inventory->Rings[0];
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

	It("D-231: a prebuilt refuses a second gem, takes a stone, and keeps its own crystal by default", [this]()
	{
		const FRingInventoryEntry *Ring = GrantGemRing(false);
		if (!TestNotNull(TEXT("Granted"), Ring)) return;
		const FGuid RingId = Ring->PersistentID;
		TestTrue(TEXT("Prebuilt, its crystal fixed"), Ring->bPrebuilt && !Ring->bCrystalRemovable);
		TestFalse(TEXT("A second gem refused"), Inventory->AttachCrystalToRing(RingId, PREBUILT_TEST_OTHER_GEM));
		TestTrue(TEXT("A stone taken, in the second socket"), Inventory->AttachCrystalToRing(RingId, PREBUILT_TEST_STONE) &&
																	 Inventory->Rings[0].StoneSocket.IsAugmentStone());
		TestFalse(TEXT("Taking the crystal out refused"), Inventory->RemoveCrystalFromRing(0));
		TestTrue(TEXT("Its gem stays"), Inventory->Rings[0].AttachedItem.Crystal.Id == PREBUILT_TEST_GEM);
		TestTrue(TEXT("Its stone stays (it would take the gem with it, CL-28b)"), Inventory->Rings[0].StoneSocket.IsAugmentStone());

		const FLoadoutScreenRow Row = ScreenRowFor(Inventory->Rings[0]);
		for (const int32 Socket : {LoadoutScreenSockets::CRYSTAL, LoadoutScreenSockets::STONE})
		{
			FText Refusal;
			TestTrue(TEXT("The screen says no"), WoRLoadoutScreen::GetTakeOffSocketWarning(Row, Socket, Refusal).IsEmpty());
			TestTrue(TEXT("Because it is built in"), Refusal.ToString().Contains(TEXT("built-in")) || Refusal.ToString().Contains(TEXT("built in")));
		}
	});

	It("D-231: a prebuilt set removable lets its crystal out", [this]()
	{
		const FRingInventoryEntry *Ring = GrantGemRing(true);
		if (!TestNotNull(TEXT("Granted"), Ring)) return;
		TestTrue(TEXT("Prebuilt, removable"), Ring->bPrebuilt && Ring->bCrystalRemovable);
		FText Refusal;
		TestFalse(TEXT("The screen offers the take-off"), WoRLoadoutScreen::GetTakeOffSocketWarning(ScreenRowFor(*Ring), LoadoutScreenSockets::CRYSTAL, Refusal).IsEmpty());
		TestTrue(TEXT("Taken out"), Inventory->RemoveCrystalFromRing(0) && Inventory->Rings[0].AttachedItem.IsEmpty());
		TestTrue(TEXT("Another gem goes in"), Inventory->AttachCrystalToRing(Inventory->Rings[0].PersistentID, PREBUILT_TEST_OTHER_GEM));
	});

	It("D-231: a prebuilt's evolution stays unless set removable", [this]()
	{
		UPrebuiltFindData *Find = NewFind(NewObject<URingData>(GetTransientPackage()));
		Find->Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		if (!TestTrue(TEXT("Given"), TrialLoot::GrantTo(Finder, Find))) return;
		TestFalse(TEXT("Fixed: refused"), Inventory->RemoveEvolutionFromRing(Inventory->Rings[0].PersistentID));
		TestTrue(TEXT("The evolution stays"), Inventory->Rings[0].AttachedItem.IsEvolution());
		TestFalse(TEXT("Nor can it be dismantled out"), Inventory->RemoveEvolutionInstance(Inventory->Rings[0].AttachedItem.Evolution.InstanceID));
		TestTrue(TEXT("Still there"), Inventory->Rings[0].AttachedItem.IsEvolution());
	});

	It("D-231: a ring the player built is unchanged", [this]()
	{
		if (!TestTrue(TEXT("Given"), TrialLoot::GrantTo(Finder, NewObject<URingData>(GetTransientPackage())))) return;
		Inventory->AddCrystal(PREBUILT_TEST_GEM);
		Inventory->AddCrystal(PREBUILT_TEST_OTHER_GEM);
		Inventory->AddCrystal(PREBUILT_TEST_STONE);
		const FGuid RingId = Inventory->Rings[0].PersistentID;
		TestTrue(TEXT("A gem goes in"), Inventory->AttachCrystalToRing(RingId, PREBUILT_TEST_GEM));
		TestFalse(TEXT("Not prebuilt"), Inventory->Rings[0].bPrebuilt);
		TestFalse(TEXT("A second gem refused, as on every piece"), Inventory->AttachCrystalToRing(RingId, PREBUILT_TEST_OTHER_GEM));
		TestTrue(TEXT("A stone goes in"), Inventory->AttachCrystalToRing(RingId, PREBUILT_TEST_STONE));
		TestTrue(TEXT("Both come off, as before"), Inventory->RemoveCrystalFromRing(0) && Inventory->Rings[0].AttachedItem.IsEmpty() &&
														Inventory->Rings[0].StoneSocket.IsEmpty());
	});
}

#endif
