// CustomLoadoutsSpec.cpp
// B-311 (D-111): the player's own loadouts are kept per character (up to 5) through the pool and the save; the
// character asset's lore loadouts are rebuilt each spawn and never kept or deleted.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"

namespace
{
	const FString CUSTOM_LOADOUT_NAME = TEXT("Mine");
}

BEGIN_DEFINE_SPEC(FCustomLoadoutsSpec, "WoR.Loadout.CustomLoadouts", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UCharacterData *Member = nullptr;

	/** Spawns the member as a level does: its lore loadout holding the sword, then the shared draw. */
	UInventoryComponent *SpawnMember()
	{
		AActor *Actor = TestWorld->SpawnCombatant(Member);
		if (!Actor)
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		if (!Pool->HasBankedGear(UPoolSubsystem::GetCharacterKey(Member)))
		{
			Inventory->AddWeapon(Sword);
		}
		FCombatLoadout Lore;
		Lore.LoadoutName = TEXT("Lore");
		Lore.PrimarySlotType = EPrimarySlotType::Weapon;
		Lore.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Lore.bLore = true;
		Inventory->SavedLoadouts = {Lore};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
		return Inventory;
	}

	static ULoadoutComponent *LoadoutOf(UInventoryComponent *Inventory)
	{
		return Inventory->GetOwner()->FindComponentByClass<ULoadoutComponent>();
	}

	FName Key() const { return UPoolSubsystem::GetCharacterKey(Member); }
END_DEFINE_SPEC(FCustomLoadoutsSpec)

void FCustomLoadoutsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		Member = FWoRTestWorld::NewCharacterData();
		FPartyMember &PartyMember = Party->Members.AddDefaulted_GetRef();
		PartyMember.PawnClass = ACombatCharacter::StaticClass();
		PartyMember.CharacterData = Member;
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Fire Sword");
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		Member = nullptr;
		TestWorld.Reset();
	});

	It("makes a copy of a lore loadout the player's own, and won't delete the lore one", [this]()
	{
		UInventoryComponent *Inventory = SpawnMember();
		if (!TestNotNull(TEXT("Inventory"), Inventory)) return;
		const int32 Copy = LoadoutOf(Inventory)->DuplicateLoadout(0, CUSTOM_LOADOUT_NAME);
		if (!TestEqual(TEXT("Copied"), Copy, 1)) return;
		TestFalse(TEXT("The copy is custom"), Inventory->SavedLoadouts[Copy].bLore);
		TestFalse(TEXT("The lore loadout stays"), LoadoutOf(Inventory)->DeleteLoadout(0));
		TestTrue(TEXT("The copy can go"), LoadoutOf(Inventory)->DeleteLoadout(Copy));
	});

	It("stops at five custom loadouts", [this]()
	{
		UInventoryComponent *Inventory = SpawnMember();
		if (!TestNotNull(TEXT("Inventory"), Inventory)) return;
		for (int32 Index = 0; Index < PoolConstants::MAX_CUSTOM_LOADOUTS; ++Index)
		{
			TestTrue(TEXT("Room"), LoadoutOf(Inventory)->DuplicateLoadout(0, CUSTOM_LOADOUT_NAME) != INDEX_NONE);
		}
		TestEqual(TEXT("No sixth"), LoadoutOf(Inventory)->DuplicateLoadout(0, CUSTOM_LOADOUT_NAME), static_cast<int32>(INDEX_NONE));
	});

	It("keeps a custom loadout through the pool and the next spawn, the sword by its GUID", [this]()
	{
		UInventoryComponent *Inventory = SpawnMember();
		if (!TestNotNull(TEXT("Inventory"), Inventory)) return;
		LoadoutOf(Inventory)->DuplicateLoadout(0, CUSTOM_LOADOUT_NAME);
		const FGuid SwordID = Inventory->SavedLoadouts[1].PrimaryWeapon.WeaponEntry.PersistentID;
		TestTrue(TEXT("The sword is an owned piece"), SwordID.IsValid());
		Inventory->GetOwner()->Destroy(); // EndPlay hands the custom loadout to the pool

		const TArray<FSavedLoadout> *Kept = Pool->GetCustomLoadouts(Key());
		if (!TestTrue(TEXT("The pool keeps one"), Kept && Kept->Num() == 1)) return;
		TestEqual(TEXT("By GUID"), (*Kept)[0].PrimaryWeaponInstance, SwordID);

		UInventoryComponent *Again = SpawnMember();
		if (!TestNotNull(TEXT("Respawned"), Again) || !TestEqual(TEXT("Lore + custom"), Again->SavedLoadouts.Num(), 2)) return;
		TestTrue(TEXT("The lore one first"), Again->SavedLoadouts[0].bLore);
		TestEqual(TEXT("Then the custom one"), Again->SavedLoadouts[1].LoadoutName, CUSTOM_LOADOUT_NAME);
		TestFalse(TEXT("Custom"), Again->SavedLoadouts[1].bLore);
		TestEqual(TEXT("With the same sword"), Again->SavedLoadouts[1].PrimaryWeapon.WeaponEntry.PersistentID, SwordID);
	});

	It("saves custom loadouts (v6) and loads them back", [this]()
	{
		UInventoryComponent *Inventory = SpawnMember();
		if (!TestNotNull(TEXT("Inventory"), Inventory)) return;
		LoadoutOf(Inventory)->DuplicateLoadout(0, CUSTOM_LOADOUT_NAME);
		Inventory->PushCustomLoadoutsToPool();
		const FSavedPool Saved = USaveSubsystem::CapturePool(*Pool);
		if (!TestEqual(TEXT("One character's"), Saved.CustomLoadouts.Num(), 1)) return;

		FWoRTestWorld After;
		UPoolSubsystem *Restored = After.Create() ? After.GetSubsystem<UPoolSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Restored pool"), Restored)) return;
		USaveSubsystem::RestorePool(Saved, *Restored);
		const TArray<FSavedLoadout> *Back = Restored->GetCustomLoadouts(Key());
		if (!TestTrue(TEXT("One loadout back"), Back && Back->Num() == 1)) return;
		TestEqual(TEXT("Its name"), (*Back)[0].LoadoutName, CUSTOM_LOADOUT_NAME);
		TestTrue(TEXT("Its sword"), (*Back)[0].PrimaryWeapon == Sword);
		TestEqual(TEXT("By GUID"), (*Back)[0].PrimaryWeaponInstance, Inventory->SavedLoadouts[1].PrimaryWeapon.WeaponEntry.PersistentID);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
