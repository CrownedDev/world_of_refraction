// StoreViewsSpec.cpp
// B-312 (D-117, Q-312): the pool is the one store of owned gear. Lookups by ID go through its GUID index (still right
// after a removal shifts the arrays); a member's gear is a view over the store by owner; each member's copies agree
// with its view after the draw, a give and a chest put (CompareWithStore is empty), so the views can replace them.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "InventoryChangeRecorder.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"

namespace
{
	constexpr int32 STORE_PARTY_SIZE = 2;
	constexpr int32 STORE_INDEX_PIECES = 4;
}

BEGIN_DEFINE_SPEC(FStoreViewsSpec, "WoR.Pool.Store", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Dagger = nullptr;
	TArray<UCharacterData *> Members;

	FName KeyOf(int32 Index) const { return UPoolSubsystem::GetCharacterKey(Members[Index]); }

	static FCombatLoadout Preset(UWeaponData *Primary, UWeaponData *Secondary)
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Primary);
		if (Secondary)
		{
			Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
			Loadout.SecondaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Secondary);
		}
		return Loadout;
	}

	/** Spawns party member Index as each level does: the sword (its primary), the dagger when bBringsDagger
	 *  (worn as secondary when bWearsDagger), then the draw. */
	UInventoryComponent *SpawnMember(int32 Index, bool bBringsDagger, bool bWearsDagger)
	{
		AActor *Actor = TestWorld->SpawnCombatant(Members[Index]);
		if (!Actor)
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		Inventory->AddWeapon(Sword);
		if (bBringsDagger)
		{
			Inventory->AddWeapon(Dagger);
		}
		Inventory->SavedLoadouts = {Preset(Sword, bWearsDagger ? Dagger : nullptr)};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
		return Inventory;
	}

	static FGuid OwnedID(const UInventoryComponent *Inventory, const UWeaponData *Asset)
	{
		const FWeaponInventoryEntry *Entry = Inventory->Weapons.FindByPredicate([Asset](const FWeaponInventoryEntry &E) { return E.Weapon == Asset; });
		return Entry ? Entry->PersistentID : FGuid();
	}
END_DEFINE_SPEC(FStoreViewsSpec)

void FStoreViewsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Store sword");
		Dagger = NewObject<UWeaponData>(GetTransientPackage());
		Dagger->Name = TEXT("Store dagger");
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		for (int32 Index = 0; Index < STORE_PARTY_SIZE; ++Index)
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->Name = FString::Printf(TEXT("Member %d"), Index);
			FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
			Members.Add(Data);
		}
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		Dagger = nullptr;
		Members.Empty();
		TestWorld.Reset();
	});

	It("finds every piece by ID through the index, also after a removal shifts the store", [this]()
	{
		if (!Pool) return;
		TArray<FGuid> IDs;
		for (int32 Index = 0; Index < STORE_INDEX_PIECES; ++Index)
		{
			FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
			Entry.PersistentID = FGuid::NewGuid();
			Pool->AddWeaponToPool(Entry);
			IDs.Add(Entry.PersistentID);
		}
		for (const FGuid &ID : IDs)
		{
			const FWeaponInventoryEntry *Found = Pool->FindWeaponInPool(ID);
			TestTrue(TEXT("found by ID"), Found && Found->PersistentID == ID);
		}
		Pool->RemoveGear(IDs[0]);
		TestNull(TEXT("the removed piece is gone"), Pool->FindWeaponInPool(IDs[0]));
		for (int32 Index = 1; Index < IDs.Num(); ++Index)
		{
			const FWeaponInventoryEntry *Found = Pool->FindWeaponInPool(IDs[Index]);
			TestTrue(TEXT("the rest still found after the shift"), Found && Found->PersistentID == IDs[Index]);
		}
		TestNull(TEXT("an unknown ID finds nothing"), Pool->FindWeaponInPool(FGuid::NewGuid()));
		TestNull(TEXT("no ID finds nothing"), Pool->FindWeaponInPool(FGuid()));
	});

	It("gives each member a view of its own gear, and the copies agree with it after the draw", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *A = SpawnMember(0, true, false);
		UInventoryComponent *B = SpawnMember(1, false, false);
		if (!A || !B) return;
		const FPoolGearView ViewA = Pool->GetOwnedGearOf(KeyOf(0));
		const FPoolGearView ViewB = Pool->GetOwnedGearOf(KeyOf(1));
		TestEqual(TEXT("A owns a sword and a dagger"), ViewA.Weapons.Num(), 2);
		TestEqual(TEXT("B owns its sword"), ViewB.Weapons.Num(), 1);
		TestTrue(TEXT("the view points into the store"), ViewA.Weapons.Num() > 0 && Pool->FindWeaponInPool(ViewA.Weapons[0]->PersistentID) == ViewA.Weapons[0]);
		TestEqual(TEXT("A's copies agree with its view"), A->CompareWithStore(), FString());
		TestEqual(TEXT("B's copies agree with its view"), B->CompareWithStore(), FString());
		TestTrue(TEXT("the print lists the store"), Pool->GetStoreString().Contains(TEXT("Store (B-312)")));
	});

	It("keeps the copies in agreement with the views after a give and a chest put", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *A = SpawnMember(0, true, false);
		UInventoryComponent *B = SpawnMember(1, true, false);
		if (!A || !B) return;
		const FGuid GivenDagger = OwnedID(A, Dagger);
		const FGuid ChestDagger = OwnedID(B, Dagger);
		TestTrue(TEXT("A gives its dagger to B"), WoRGive::TryGiveGear(TestWorld->GetWorld(), KeyOf(0), KeyOf(1), GivenDagger).IsEmpty());
		TestTrue(TEXT("B puts its own dagger in the chest"), WoRGive::TryPutInChest(TestWorld->GetWorld(), KeyOf(1), ChestDagger).IsEmpty());
		TestEqual(TEXT("A agrees"), A->CompareWithStore(), FString());
		TestEqual(TEXT("B agrees"), B->CompareWithStore(), FString());
		const FPoolGearView Chest = Pool->GetOwnedGearOf(UPoolSubsystem::GetChestKey());
		TestTrue(TEXT("the chest's view holds the put dagger"), Chest.Weapons.Num() == 1 && Chest.Weapons[0]->PersistentID == ChestDagger);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
