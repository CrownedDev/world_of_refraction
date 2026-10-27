// SharedGearSpec.cpp
// B-091 (D-020, Q-091): the party owns one armoury. Every member's inventory sees each piece once,
// a piece is worn by one member at a time, and a change one member makes reaches the others.

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
	const FCrystalId SHARED_GEAR_FIRE_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	constexpr int32 SHARED_GEAR_PARTY_SIZE = 3;
}

BEGIN_DEFINE_SPEC(FSharedGearSpec, "WoR.Pool.SharedGear",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	TArray<UCharacterData *> Members;

	/** A preset holding the sword (by asset; it links to an owned one). */
	static FCombatLoadout SwordPreset(UWeaponData *Weapon)
	{
		FCombatLoadout Preset;
		Preset.PrimarySlotType = EPrimarySlotType::Weapon;
		Preset.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
		return Preset;
	}

	/** Spawns party member Index as each level does: its authored gear first (the sword when it
	 *  brings one), its presets (the first names the sword when bNamesSword), then the shared draw. */
	UInventoryComponent *SpawnMember(int32 Index, bool bBringsSword, bool bNamesSword)
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
		if (bBringsSword)
		{
			Inventory->AddWeapon(Sword);
		}
		Inventory->SavedLoadouts = {bNamesSword ? SwordPreset(Sword) : FCombatLoadout(), FCombatLoadout()};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
		return Inventory;
	}

	static FGuid ActiveWeaponID(const UInventoryComponent *Inventory)
	{
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.PersistentID;
	}
END_DEFINE_SPEC(FSharedGearSpec)

void FSharedGearSpec::Define()
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
		for (int32 Index = 0; Index < SHARED_GEAR_PARTY_SIZE; ++Index)
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->Name = FString::Printf(TEXT("Member %d"), Index);
			FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
			Members.Add(Data);
		}
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Fire Sword");
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		Members.Empty();
		TestWorld.Reset();
	});

	It("gives a party of three with one sword exactly one sword", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *First = SpawnMember(0, true, true);
		UInventoryComponent *Second = SpawnMember(1, false, false);
		UInventoryComponent *Third = SpawnMember(2, false, false);
		if (!First || !Second || !Third) return;

		TestEqual(TEXT("One sword in the pool"), Pool->GetOwnedWeapons().Num(), 1);
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;
		for (const UInventoryComponent *Inventory : {First, Second, Third})
		{
			TestTrue(TEXT("Each member sees that one sword"), Inventory->Weapons.Num() == 1 && Inventory->Weapons[0].PersistentID == SwordID);
		}
		TestEqual(TEXT("Its owner wears it"), ActiveWeaponID(First), SwordID);
		TestEqual(TEXT("Worn by the first member"), Pool->GetWearer(SwordID), UPoolSubsystem::GetCharacterKey(Members[0]));
	});

	It("doesn't duplicate the sword when a second member's loadout names it", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *First = SpawnMember(0, true, true);
		UInventoryComponent *Second = SpawnMember(1, false, true);
		if (!First || !Second) return;

		TestEqual(TEXT("Still one sword"), Pool->GetOwnedWeapons().Num(), 1);
		TestFalse(TEXT("Taken out of the second member's loadout"), ActiveWeaponID(Second).IsValid());
		TestEqual(TEXT("Still worn by the first"), ActiveWeaponID(First), Pool->GetOwnedWeapons()[0].PersistentID);
		TestTrue(TEXT("The clash is noted for the party panel"),
				 Pool->GetClashes().Num() == 1 && Pool->GetClashes()[0].LoserKey == UPoolSubsystem::GetCharacterKey(Members[1]));
	});

	It("passes the sword on once its wearer takes it off", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *First = SpawnMember(0, true, true);
		UInventoryComponent *Second = SpawnMember(1, false, true);
		if (!First || !Second) return;
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;

		TestTrue(TEXT("First switches to a loadout without it"),
				 First->GetOwner()->FindComponentByClass<ULoadoutComponent>()->SetActiveLoadoutIndex(1));
		TestTrue(TEXT("Nobody wears it"), Pool->GetWearer(SwordID).IsNone());
		Second->SavedLoadouts[0] = SwordPreset(Sword);
		Second->LinkLoadoutsToOwned();
		TestEqual(TEXT("The second claims it without a clash"), Second->ClaimActiveSharedGear().Num(), 0);
		TestEqual(TEXT("Worn by the second"), Pool->GetWearer(SwordID), UPoolSubsystem::GetCharacterKey(Members[1]));
	});

	It("shows a crystal one member attaches on everyone's copy", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *First = SpawnMember(0, true, true);
		UInventoryComponent *Second = SpawnMember(1, false, false);
		if (!First || !Second) return;
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;
		TestTrue(TEXT("Fire gem owned"), First->AddCrystal(SHARED_GEAR_FIRE_GEM));
		if (!TestTrue(TEXT("Attached"), First->AttachCrystalToWeapon(SwordID, SHARED_GEAR_FIRE_GEM))) return;

		TestTrue(TEXT("The pool's sword holds it"), Pool->GetOwnedWeapons()[0].AttachedItem.IsCrystal());
		TestTrue(TEXT("The second member's copy holds it"), Second->Weapons.Num() == 1 && Second->Weapons[0].AttachedItem.IsCrystal());
	});

	It("draws the same sword next level without banking it again", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *FirstLevel = SpawnMember(0, true, true);
		if (!FirstLevel) return;
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;

		UInventoryComponent *NextLevel = SpawnMember(0, true, true);
		if (!NextLevel) return;
		TestEqual(TEXT("Still one sword"), Pool->GetOwnedWeapons().Num(), 1);
		TestEqual(TEXT("Its preset holds the shared sword"), ActiveWeaponID(NextLevel), SwordID);
	});

	It("takes a sold sword away from every member", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *First = SpawnMember(0, true, true);
		UInventoryComponent *Second = SpawnMember(1, false, false);
		if (!First || !Second) return;
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;

		TestTrue(TEXT("Sold"), First->RemoveWeaponByPersistentID(SwordID));
		TestEqual(TEXT("Gone from the pool"), Pool->GetOwnedWeapons().Num(), 0);
		TestEqual(TEXT("Gone from the second member"), Second->Weapons.Num(), 0);
		TestTrue(TEXT("Nobody wears it"), Pool->GetWearer(SwordID).IsNone());
	});

	It("keeps who wears what and whose gear is banked through a save", [this]()
	{
		if (!Pool) return;
		if (!SpawnMember(0, true, true)) return;
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;
		const FSavedPool Saved = USaveSubsystem::CapturePool(*Pool);

		FWoRTestWorld After;
		if (!TestTrue(TEXT("World"), After.Create())) return;
		UPoolSubsystem *Restored = After.GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("Restored pool"), Restored)) return;
		USaveSubsystem::RestorePool(Saved, *Restored);
		TestEqual(TEXT("Same wearer"), Restored->GetWearer(SwordID), UPoolSubsystem::GetCharacterKey(Members[0]));
		TestTrue(TEXT("Starting gear stays banked"), Restored->HasBankedGear(UPoolSubsystem::GetCharacterKey(Members[0])));
	});

	It("leaves a character outside the party on its own gear", [this]()
	{
		if (!Pool) return;
		AActor *Enemy = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!Enemy) return;
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Enemy);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Enemy);
		Inventory->AddWeapon(Sword);
		TestFalse(TEXT("No shared draw"), Inventory->DrawSharedGear());
		TestEqual(TEXT("Its sword stays its own"), Inventory->Weapons.Num(), 1);
		TestEqual(TEXT("Nothing banked"), Pool->GetOwnedWeapons().Num(), 0);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
