// SocketedCrystalItemSpec.cpp
// B-612 (Crown: the Emerald went into the Hand Axe and still showed as "Emerald (S) x1" in the fight's Items): a crystal
// socketed from the loadout leaves the pouch, so it is no fight item; a loose one still is (D-133, D-250). A crystal in a
// class kit piece's socket goes back to the pouch when the kit resets at a trial start (D-221), and is an item again.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryData.h"
#include "Loadout/ClassDefaultLoadoutData.h"
#include "Loadout/Entries/FItemLoadoutSlot.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRGameModeBase.h"
#include "Pool/PoolSubsystem.h"

namespace
{
	const FCrystalId SOCKETED_ITEM_EMERALD(ECrystalType::Emerald, EItemTier::S_Tier);
	const FCrystalId SOCKETED_ITEM_GARNET(ECrystalType::Garnet, EItemTier::F_Tier);
}

BEGIN_DEFINE_SPEC(FSocketedCrystalItemSpec, "WoR.Loadout.SocketedCrystalItem", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadout = nullptr;
	UWeaponData *Axe = nullptr;
	FName Key;

	FGuid OwnedAxe(bool bKit) const
	{
		const FWeaponInventoryEntry *Found = Inventory->Weapons.FindByPredicate([this, bKit](const FWeaponInventoryEntry &E)
																				 { return E.Weapon == Axe && Pool->IsClassDefault(E.PersistentID) == bKit; });
		return Found ? Found->PersistentID : FGuid();
	}

	bool IsOfferedInFight(const FCrystalId &Id) const
	{
		return Loadout->GetUsableItems().ContainsByPredicate([&Id](const FItemLoadoutSlot &Item) { return Item.CrystalId == Id && Item.Quantity > 0; });
	}
END_DEFINE_SPEC(FSocketedCrystalItemSpec)

void FSocketedCrystalItemSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("party"), Party) || !TestNotNull(TEXT("pool"), Pool)) return;
		Axe = NewObject<UWeaponData>(GetTransientPackage());
		Axe->Name = TEXT("Hand Axe");
		Axe->WeaponType = EWeaponType::Axe;
		Axe->Tier = EItemTier::S_Tier;
		UCharacterData *Leader = FWoRTestWorld::NewCharacterData();
		Leader->CharacterClass = ECharacterClass::Generic;
		Leader->Inventory = NewObject<UInventoryData>(GetTransientPackage());
		Leader->Inventory->StartingGear.Pieces.AddDefaulted_GetRef().Piece = Axe; // Crown's Hand Axe, home gear
		Key = UPoolSubsystem::GetCharacterKey(Leader);
		Party->Members.Empty();
		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Leader;
		Member.MemberId = FGuid::NewGuid();
		APawn *Pawn = TestWorld->GetWorld()->SpawnActor<APawn>();
		UCharacterDataComponent *CharComp = Pawn ? NewObject<UCharacterDataComponent>(Pawn) : nullptr;
		if (!TestNotNull(TEXT("pawn"), CharComp)) return;
		CharComp->CharacterData = FWoRTestWorld::NewCharacterData();
		Pawn->AddInstanceComponent(CharComp);
		if (!TestTrue(TEXT("stood as the leader"), AWoRGameModeBase::StandAsPartyLeader(Pawn, Party))) return;
		CharComp->RegisterComponent();
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Pawn);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Pawn);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Pawn);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Pawn);
		Loadout = FWoRTestWorld::AddComponent<ULoadoutComponent>(Pawn);
		Inventory->DrawSharedGear(); // binds the pouch
		TestTrue(TEXT("given as wor.Economy.GiveCrystal does"), Inventory->AddCrystals({SOCKETED_ITEM_EMERALD, SOCKETED_ITEM_GARNET}));
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Inventory = nullptr;
		Loadout = nullptr;
		TestWorld.Reset();
	});

	It("takes a socketed crystal out of the pouch and the fight's items; a loose one stays (D-133)", [this]()
	{
		if (!Inventory) return;
		const FGuid AxeID = OwnedAxe(false);
		if (!TestTrue(TEXT("the home Hand Axe owned"), AxeID.IsValid())) return;
		TestTrue(TEXT("before: the Emerald is a fight item"), Pool->IsFightItem(Key, SOCKETED_ITEM_EMERALD) && IsOfferedInFight(SOCKETED_ITEM_EMERALD));
		if (!TestTrue(TEXT("socketed in the axe"), Inventory->AttachCrystalToWeapon(AxeID, SOCKETED_ITEM_EMERALD))) return;
		TestEqual(TEXT("out of the pouch"), Pool->GetCarriedCount(Key, SOCKETED_ITEM_EMERALD), 0);
		TestFalse(TEXT("no longer a fight item"), Pool->IsFightItem(Key, SOCKETED_ITEM_EMERALD));
		TestFalse(TEXT("not offered in the fight"), IsOfferedInFight(SOCKETED_ITEM_EMERALD));
		TestTrue(TEXT("the loose Garnet is still offered"), IsOfferedInFight(SOCKETED_ITEM_GARNET));
		Inventory->DrawSharedGear(); // the hub visited again
		TestFalse(TEXT("and stays out after the next draw"), IsOfferedInFight(SOCKETED_ITEM_EMERALD));
	});

	It("gives a kit piece's crystal back to the pouch when the kit resets at a trial start (D-221)", [this]()
	{
		if (!Inventory) return;
		FClassDefaultLoadout Kit;
		Kit.Pieces.AddDefaulted_GetRef().Piece = Axe;
		Pool->MintClassDefaultKit(Key, Kit);
		Inventory->DrawSharedGear();
		const FGuid KitAxeID = OwnedAxe(true);
		if (!TestTrue(TEXT("the kit's Hand Axe owned too"), KitAxeID.IsValid())) return;
		if (!TestTrue(TEXT("socketed in the kit axe"), Inventory->AttachCrystalToWeapon(KitAxeID, SOCKETED_ITEM_EMERALD))) return;
		TestFalse(TEXT("not offered while socketed"), IsOfferedInFight(SOCKETED_ITEM_EMERALD));
		Pool->ResetClassDefaultKit(Key, Kit); // what a trial start does to the kit
		const FWeaponInventoryEntry *KitAxe = Pool->FindWeaponInPool(KitAxeID);
		TestTrue(TEXT("the kit axe is empty again"), KitAxe && KitAxe->AttachedItem.IsEmpty());
		TestEqual(TEXT("the Emerald is back in the pouch"), Pool->GetCarriedCount(Key, SOCKETED_ITEM_EMERALD), 1);
		TestTrue(TEXT("so it is a fight item again"), Pool->IsFightItem(Key, SOCKETED_ITEM_EMERALD) && IsOfferedInFight(SOCKETED_ITEM_EMERALD));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
