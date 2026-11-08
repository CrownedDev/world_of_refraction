// TwoSocketsSpec.cpp
// B-351 (D-141): every weapon and adapter has an element socket (a gem or an evolution) and a stone socket (a stone).
// A gem and a stone sit together; a second gem is refused; an adapter takes a stone; a save below the minimum version
// (one from before the two sockets) is wiped (D-138). B-351b: removing a crystal takes the stone off too; a stone
// authored in the element socket blocks a second; the carry and the run save keep the stone socket.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/GearSockets.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/CarriedGearState.h"
#include "Save/WoRSaveTypes.h"
#include "Save/WoRSaveGame.h"
#include "UObject/UnrealType.h"

namespace
{
	const FCrystalId SOCKET_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId SOCKET_TEST_OTHER_GEM(ECrystalType::Sapphire, EItemTier::F_Tier);
	const FCrystalId SOCKET_TEST_STONE(ECrystalType::DamageStone, EItemTier::F_Tier);
	const FCrystalId SOCKET_TEST_OTHER_STONE(ECrystalType::DefenseStone, EItemTier::F_Tier);
	constexpr int32 SOCKET_TEST_STACK = 3;
	constexpr int32 SOCKET_TEST_OLD_VERSION_STEP = 1;
	constexpr int32 SOCKET_TEST_SAVED_ROWS = 2;
	const FName SOCKET_TEST_SAVED_VERSION(TEXT("SavedDataVersion"));
	const FName SOCKET_TEST_LOADED_VERSION(TEXT("LoadedDataVersion"));
}

BEGIN_DEFINE_SPEC(FTwoSocketsSpec, "WoR.Inventory.TwoSockets", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UInventoryComponent *Inventory = nullptr;
	UCrystalInventoryComponent *Crystals = nullptr;
	FGuid WeaponId;
	FGuid RingId;

	/** Sets an int UPROPERTY by name (the save's version fields are protected on ULocalPlayerSaveGame). */
	static bool SetIntProperty(UObject *Object, FName Name, int32 Value)
	{
		FIntProperty *Property = FindFProperty<FIntProperty>(Object->GetClass(), Name);
		if (!Property)
		{
			return false;
		}
		Property->SetPropertyValue_InContainer(Object, Value);
		return true;
	}
END_DEFINE_SPEC(FTwoSocketsSpec)

void FTwoSocketsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		AActor *Owner = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("Owner"), Owner)) return;
		Crystals = FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Owner);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Owner);
		TestTrue(TEXT("weapon added"), Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage())));
		TestTrue(TEXT("adapter added"), Inventory->AddRing(NewObject<URingData>(GetTransientPackage())));
		WeaponId = Inventory->Weapons.Num() > 0 ? Inventory->Weapons[0].PersistentID : FGuid();
		RingId = Inventory->Rings.Num() > 0 ? Inventory->Rings[0].PersistentID : FGuid();
		for (const FCrystalId &Id : {SOCKET_TEST_GEM, SOCKET_TEST_OTHER_GEM, SOCKET_TEST_STONE, SOCKET_TEST_OTHER_STONE})
		{
			TestTrue(TEXT("crystals added"), Inventory->AddCrystal(Id, SOCKET_TEST_STACK));
		}
	});

	AfterEach([this]()
	{
		Inventory = nullptr;
		Crystals = nullptr;
		TestWorld.Reset();
	});

	It("sorts each kind into its socket", [this]()
	{
		TestEqual(TEXT("a gem: element"), GearSockets::SocketFor(EAttachedItemKind::Crystal), EGearSocket::Element);
		TestEqual(TEXT("an evolution: element"), GearSockets::SocketFor(EAttachedItemKind::Evolution), EGearSocket::Element);
		TestEqual(TEXT("a stone: stone"), GearSockets::SocketFor(EAttachedItemKind::AugmentStone), EGearSocket::Stone);
		TestEqual(TEXT("a fusion: neither"), GearSockets::SocketFor(EAttachedItemKind::Fusion), EGearSocket::None);
	});

	It("holds a gem and a stone together on one weapon", [this]()
	{
		if (!Inventory) return;
		TestTrue(TEXT("gem"), Inventory->AttachCrystalToWeapon(WeaponId, SOCKET_TEST_GEM));
		TestTrue(TEXT("stone"), Inventory->AttachCrystalToWeapon(WeaponId, SOCKET_TEST_STONE));
		TestTrue(TEXT("the gem in the element socket"), Inventory->Weapons[0].AttachedItem.IsCrystal());
		TestTrue(TEXT("the stone in the stone socket"), Inventory->Weapons[0].StoneSocket.IsAugmentStone());
		TestTrue(TEXT("the stone's id"), Inventory->Weapons[0].StoneSocket.Crystal.Id == SOCKET_TEST_STONE);
	});

	It("refuses a second gem and a second stone, debiting neither", [this]()
	{
		if (!Inventory) return;
		TestTrue(TEXT("first gem"), Inventory->AttachCrystalToWeapon(WeaponId, SOCKET_TEST_GEM));
		TestTrue(TEXT("first stone"), Inventory->AttachCrystalToWeapon(WeaponId, SOCKET_TEST_STONE));
		TestFalse(TEXT("second gem refused"), Inventory->AttachCrystalToWeapon(WeaponId, SOCKET_TEST_OTHER_GEM));
		TestFalse(TEXT("second stone refused"), Inventory->AttachCrystalToWeapon(WeaponId, SOCKET_TEST_OTHER_STONE));
		TestEqual(TEXT("other gem not debited"), Crystals->GetCount(SOCKET_TEST_OTHER_GEM), SOCKET_TEST_STACK);
		TestEqual(TEXT("other stone not debited"), Crystals->GetCount(SOCKET_TEST_OTHER_STONE), SOCKET_TEST_STACK);
		TestTrue(TEXT("the first gem stays"), Inventory->Weapons[0].AttachedItem.Crystal.Id == SOCKET_TEST_GEM);
	});

	It("gives an adapter a stone socket too", [this]()
	{
		if (!Inventory) return;
		TestTrue(TEXT("gem on the adapter"), Inventory->AttachCrystalToRing(RingId, SOCKET_TEST_GEM));
		TestTrue(TEXT("stone on the adapter"), Inventory->AttachCrystalToRing(RingId, SOCKET_TEST_STONE));
		TestTrue(TEXT("the stone in the adapter's stone socket"), Inventory->Rings[0].StoneSocket.IsAugmentStone());
	});

	It("takes a socketed stone off with the gem (B-351b)", [this]()
	{
		if (!Inventory) return;
		TestTrue(TEXT("gem"), Inventory->AttachCrystalToWeapon(WeaponId, SOCKET_TEST_GEM));
		TestTrue(TEXT("stone"), Inventory->AttachCrystalToWeapon(WeaponId, SOCKET_TEST_STONE));
		TestTrue(TEXT("removed"), Inventory->RemoveCrystalFromWeapon(0));
		TestTrue(TEXT("element socket empty"), Inventory->Weapons[0].AttachedItem.IsEmpty());
		TestTrue(TEXT("stone socket empty"), Inventory->Weapons[0].StoneSocket.IsEmpty());
		TestTrue(TEXT("a new stone fits again"), Inventory->AttachCrystalToWeapon(WeaponId, SOCKET_TEST_OTHER_STONE));
	});

	It("refuses a second stone over one authored in the element socket (B-351b)", [this]()
	{
		if (!Inventory) return;
		FRuntimeAttachedItem Authored; // the pre-B-351 shape: a stone in the element socket
		Authored.Kind = EAttachedItemKind::AugmentStone;
		Authored.Crystal.Id = SOCKET_TEST_STONE;
		Inventory->Weapons[0].AttachedItem = Authored;
		TestFalse(TEXT("second stone refused"), Inventory->AttachCrystalToWeapon(WeaponId, SOCKET_TEST_OTHER_STONE));
		TestEqual(TEXT("not debited"), Crystals->GetCount(SOCKET_TEST_OTHER_STONE), SOCKET_TEST_STACK);
	});

	It("carries the stone socket from fight to fight and through the run save (B-351b)", [this]()
	{
		FCarriedGearState Carried;
		Carried.bCarried = true;
		FCarriedAttachment &Entry = Carried.Attachments.AddDefaulted_GetRef();
		Entry.Holder = NewObject<UWeaponData>(GetTransientPackage());
		Entry.GearID = FGuid::NewGuid();
		Entry.StoneSocket.Kind = EAttachedItemKind::AugmentStone;
		Entry.StoneSocket.Crystal.Id = SOCKET_TEST_STONE;
		const FSavedCarriedGear Saved = FSavedCarriedGear::Make(Carried);
		if (!TestEqual(TEXT("one saved row"), Saved.Attachments.Num(), 1)) return;
		TestTrue(TEXT("saved"), Saved.Attachments[0].StoneSocket.IsAugmentStone() && Saved.Attachments[0].StoneSocket.Crystal.Id == SOCKET_TEST_STONE);
		// Restore resolves the holder by asset path; a transient weapon has none, so read the saved row directly.
		TestTrue(TEXT("the element socket stays empty"), Saved.Attachments[0].Attached.IsEmpty());
	});

	It("wipes a save from before the two sockets", [this]()
	{
		UWoRSaveGame *Save = NewObject<UWoRSaveGame>(GetTransientPackage());
		const int32 OldVersion = WoRSaveVersion::MIN_SUPPORTED - SOCKET_TEST_OLD_VERSION_STEP;
		const bool bSet = SetIntProperty(Save, SOCKET_TEST_SAVED_VERSION, OldVersion);
		SetIntProperty(Save, SOCKET_TEST_LOADED_VERSION, OldVersion);
		if (!TestTrue(TEXT("version field found"), bSet)) return;
		Save->Data.Wallet.Balances.AddDefaulted(SOCKET_TEST_SAVED_ROWS);
		Save->HandlePostLoad();
		TestTrue(TEXT("wiped"), Save->WasWiped());
		TestEqual(TEXT("data reset"), Save->Data.Wallet.Balances.Num(), 0);
		TestEqual(TEXT("the minimum is the latest (D-138)"), WoRSaveVersion::MIN_SUPPORTED, WoRSaveVersion::LATEST);
	});
}

#endif
