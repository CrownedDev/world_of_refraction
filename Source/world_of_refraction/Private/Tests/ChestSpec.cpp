// ChestSpec.cpp
// B-324 (D-117, Q-321): the shared home chest holds gear. A puts a spare sword in the chest (it leaves A's
// armoury, the chest owns it, every member's draw leaves it alone); B takes it and owns it; worn gear can't be put
// away; the chest survives a save. B-347b (Q-347): the chest stays at home while a run is open.

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
	constexpr int32 CHEST_PARTY_SIZE = 2;
}

BEGIN_DEFINE_SPEC(FChestSpec, "WoR.Pool.Chest", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
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
END_DEFINE_SPEC(FChestSpec)

void FChestSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Chest sword");
		Dagger = NewObject<UWeaponData>(GetTransientPackage());
		Dagger->Name = TEXT("Chest dagger");
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		for (int32 Index = 0; Index < CHEST_PARTY_SIZE; ++Index)
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

	It("lets A put a spare dagger in the chest and B take it: B owns it", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *A = SpawnMember(0, true, false);
		UInventoryComponent *B = SpawnMember(1, false, false);
		if (!A || !B) return;
		const FGuid DaggerID = OwnedID(A, Dagger);
		if (!TestTrue(TEXT("A owns the dagger"), DaggerID.IsValid())) return;

		const FText PutRefusal = WoRGive::TryPutInChest(TestWorld->GetWorld(), KeyOf(0), DaggerID);
		TestTrue(FString::Printf(TEXT("Put [%s]"), *PutRefusal.ToString()), PutRefusal.IsEmpty());
		TestTrue(TEXT("In the chest"), Pool->IsInChest(DaggerID));
		TestEqual(TEXT("It left A"), OwnedID(A, Dagger), FGuid());
		TestEqual(TEXT("B doesn't have it either"), OwnedID(B, Dagger), FGuid());
		TestFalse(TEXT("A can't take what A put in as a give"), WoRGive::TryGiveGear(TestWorld->GetWorld(), KeyOf(0), KeyOf(1), DaggerID).IsEmpty());

		const FText TakeRefusal = WoRGive::TryTakeFromChest(TestWorld->GetWorld(), KeyOf(1), DaggerID);
		TestTrue(FString::Printf(TEXT("Taken [%s]"), *TakeRefusal.ToString()), TakeRefusal.IsEmpty());
		TestEqual(TEXT("B owns it"), Pool->GetGearOwner(DaggerID), KeyOf(1));
		TestEqual(TEXT("It is in B's armoury, same instance"), OwnedID(B, Dagger), DaggerID);
		TestFalse(TEXT("Not in the chest any more"), Pool->IsInChest(DaggerID));
		TestFalse(TEXT("Taking it again is refused"), WoRGive::TryTakeFromChest(TestWorld->GetWorld(), KeyOf(0), DaggerID).IsEmpty());
	});

	It("keeps a put-away piece out of a run-only recruit's armoury", [this]()
	{
		if (!Pool) return;
		TestWorld->GetSubsystem<UPartySessionSubsystem>()->GetLocalParty()->Members[1].bRunOnly = true;
		UInventoryComponent *A = SpawnMember(0, false, false);
		UInventoryComponent *Recruit = SpawnMember(1, false, false);
		if (!A || !Recruit) return;
		// A finds its spare after the recruit's draw, so the recruit hears it but doesn't wear it.
		A->AddWeapon(Dagger);
		const FGuid DaggerID = OwnedID(A, Dagger);
		TestEqual(TEXT("The recruit sees A's spare"), OwnedID(Recruit, Dagger), DaggerID);
		const FText Refusal = WoRGive::TryPutInChest(TestWorld->GetWorld(), KeyOf(0), DaggerID);
		TestTrue(FString::Printf(TEXT("Put [%s]"), *Refusal.ToString()), Refusal.IsEmpty());
		TestEqual(TEXT("It left the recruit too"), OwnedID(Recruit, Dagger), FGuid());
		Recruit->DrawSharedGear();
		TestEqual(TEXT("And a fresh draw leaves it in the chest"), OwnedID(Recruit, Dagger), FGuid());
	});

	It("wakes the taker's duplicate choice when it already owns a copy (D-115)", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *A = SpawnMember(0, true, false);
		UInventoryComponent *B = SpawnMember(1, true, false);
		if (!A || !B) return;
		const FGuid PutID = OwnedID(A, Dagger);
		const FGuid KeptID = OwnedID(B, Dagger);
		TestTrue(TEXT("Put"), WoRGive::TryPutInChest(TestWorld->GetWorld(), KeyOf(0), PutID).IsEmpty());
		UDuplicateGearRecorder *Recorder = NewObject<UDuplicateGearRecorder>(GetTransientPackage());
		Recorder->Listen(B);
		TestTrue(TEXT("Taken"), WoRGive::TryTakeFromChest(TestWorld->GetWorld(), KeyOf(1), PutID).IsEmpty());
		if (!TestEqual(TEXT("One duplicate notice"), Recorder->NewIDs.Num(), 1)) return;
		TestEqual(TEXT("The taken piece is the new one"), Recorder->NewIDs[0], PutID);
		TestEqual(TEXT("Against B's own"), Recorder->OwnedIDs[0], KeptID);
	});

	It("stays at home during a run: no put, no take until it ends (B-347b, Q-347)", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *A = SpawnMember(0, true, false);
		UInventoryComponent *B = SpawnMember(1, false, false);
		if (!A || !B) return;
		const FGuid DaggerID = OwnedID(A, Dagger);
		TestTrue(TEXT("Put at home"), WoRGive::TryPutInChest(TestWorld->GetWorld(), KeyOf(0), DaggerID).IsEmpty());
		Pool->BeginRunLayer();
		TestFalse(TEXT("Take refused during the run"), WoRGive::TryTakeFromChest(TestWorld->GetWorld(), KeyOf(1), DaggerID).IsEmpty());
		TestTrue(TEXT("Still in the chest"), Pool->IsInChest(DaggerID));
		Pool->EndRunLayer();
		TestTrue(TEXT("Take works after the run"), WoRGive::TryTakeFromChest(TestWorld->GetWorld(), KeyOf(1), DaggerID).IsEmpty());
		TestEqual(TEXT("B owns it"), Pool->GetGearOwner(DaggerID), KeyOf(1));
	});

	It("refuses to put worn gear away", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *A = SpawnMember(0, true, true);
		if (!A) return;
		const FGuid DaggerID = OwnedID(A, Dagger);
		const FGuid SwordID = OwnedID(A, Sword);
		TestFalse(TEXT("The worn secondary stays"), WoRGive::TryPutInChest(TestWorld->GetWorld(), KeyOf(0), DaggerID).IsEmpty());
		TestFalse(TEXT("The primary stays"), WoRGive::TryPutInChest(TestWorld->GetWorld(), KeyOf(0), SwordID).IsEmpty());
		TestEqual(TEXT("Still A's"), Pool->GetGearOwner(DaggerID), KeyOf(0));
		TestFalse(TEXT("Not in the chest"), Pool->IsInChest(DaggerID));
	});

	It("leaves chest gear alone at every draw, and keeps it through a save", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *A = SpawnMember(0, true, false);
		if (!A) return;
		const FGuid DaggerID = OwnedID(A, Dagger);
		TestTrue(TEXT("Put"), WoRGive::TryPutInChest(TestWorld->GetWorld(), KeyOf(0), DaggerID).IsEmpty());

		// A later level: a member's draw hands out unowned gear to the leader, but the chest's is owned by the chest.
		UInventoryComponent *B = SpawnMember(1, false, false);
		if (!B) return;
		TestTrue(TEXT("Still in the chest after B's draw"), Pool->IsInChest(DaggerID));
		TestEqual(TEXT("Not in B's armoury"), OwnedID(B, Dagger), FGuid());

		const FSavedPool Saved = USaveSubsystem::CapturePool(*Pool);
		FWoRTestWorld After;
		UPoolSubsystem *Restored = After.Create() ? After.GetSubsystem<UPoolSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Pool after"), Restored)) return;
		USaveSubsystem::RestorePool(Saved, *Restored);
		TestTrue(TEXT("The chest keeps it"), Restored->IsInChest(DaggerID));
		TestEqual(TEXT("A draw after the load doesn't hand it out"), Restored->AssignUnownedGear(KeyOf(0)), 0);
		TestTrue(TEXT("Still the chest's"), Restored->IsInChest(DaggerID));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
