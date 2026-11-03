// SharedGearSpec.cpp
// B-091, changed by B-321 (D-117): one pool holds the party's gear, but each piece has an owner and a member's
// armoury lists only its own; banking, saves and sold pieces still go through the pool.

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

	It("gives the sword only to the member who owns it (D-117)", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *First = SpawnMember(0, true, true);
		UInventoryComponent *Second = SpawnMember(1, false, false);
		UInventoryComponent *Third = SpawnMember(2, false, false);
		if (!First || !Second || !Third) return;

		TestEqual(TEXT("One sword in the pool"), Pool->GetOwnedWeapons().Num(), 1);
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;
		TestEqual(TEXT("Owned by the first member"), Pool->GetGearOwner(SwordID), UPoolSubsystem::GetCharacterKey(Members[0]));
		TestTrue(TEXT("In the first member's armoury"), First->Weapons.Num() == 1 && First->Weapons[0].PersistentID == SwordID);
		TestEqual(TEXT("Not in the second's"), Second->Weapons.Num(), 0);
		TestEqual(TEXT("Not in the third's"), Third->Weapons.Num(), 0);
		TestEqual(TEXT("Its owner wears it"), ActiveWeaponID(First), SwordID);
	});

	It("doesn't give a second member the sword its loadout names but it doesn't own (D-117)", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *First = SpawnMember(0, true, true);
		UInventoryComponent *Second = SpawnMember(1, false, true);
		if (!First || !Second) return;

		TestEqual(TEXT("Still one sword"), Pool->GetOwnedWeapons().Num(), 1);
		TestFalse(TEXT("Taken out of the second member's loadout"), ActiveWeaponID(Second).IsValid());
		TestEqual(TEXT("Still worn by the first"), ActiveWeaponID(First), Pool->GetOwnedWeapons()[0].PersistentID);
	});

	It("keeps the sword its owner's when the owner takes it off (D-117)", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *First = SpawnMember(0, true, true);
		UInventoryComponent *Second = SpawnMember(1, false, false);
		if (!First || !Second) return;
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;

		TestTrue(TEXT("First switches to a loadout without it"),
				 First->GetOwner()->FindComponentByClass<ULoadoutComponent>()->SetActiveLoadoutIndex(1));
		TestEqual(TEXT("Still the first member's"), Pool->GetGearOwner(SwordID), UPoolSubsystem::GetCharacterKey(Members[0]));
		TestEqual(TEXT("Never in the second member's list"), Second->GetSelectableWeapons().Num(), 0);
		TestTrue(TEXT("Still the first member's to pick"), First->GetSelectableWeapons().Num() == 1);
	});

	It("lets two members each own the same sword (D-117)", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *First = SpawnMember(0, true, true);
		UInventoryComponent *Second = SpawnMember(1, true, true);
		if (!First || !Second) return;

		TestEqual(TEXT("Two swords in the pool"), Pool->GetOwnedWeapons().Num(), 2);
		if (!TestTrue(TEXT("One each"), First->Weapons.Num() == 1 && Second->Weapons.Num() == 1)) return;
		TestNotEqual(TEXT("Different swords"), First->Weapons[0].PersistentID, Second->Weapons[0].PersistentID);
		TestEqual(TEXT("Each wears its own"), ActiveWeaponID(Second), Second->Weapons[0].PersistentID);
	});

	It("gives a piece from before D-117 to whoever wears it, else the party leader", [this]()
	{
		if (!Pool) return;
		FWeaponInventoryEntry Worn = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Worn.PersistentID = FGuid::NewGuid();
		FWeaponInventoryEntry Loose = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Loose.PersistentID = FGuid::NewGuid();
		Pool->AddWeaponToPool(Worn);
		Pool->AddWeaponToPool(Loose);
		Pool->SetWearer(Worn.PersistentID, UPoolSubsystem::GetCharacterKey(Members[1]));
		TestEqual(TEXT("Both given an owner"), Pool->AssignUnownedGear(UPoolSubsystem::GetCharacterKey(Members[0])), 2);
		TestEqual(TEXT("The worn one to its wearer"), Pool->GetGearOwner(Worn.PersistentID), UPoolSubsystem::GetCharacterKey(Members[1]));
		TestEqual(TEXT("The loose one to the leader"), Pool->GetGearOwner(Loose.PersistentID), UPoolSubsystem::GetCharacterKey(Members[0]));
		TestEqual(TEXT("Each piece owned once"), Pool->AssignUnownedGear(UPoolSubsystem::GetCharacterKey(Members[0])), 0);
	});

	It("gives a loaded member with no primary a free owned sword, never one another member wears (D-116)", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *First = SpawnMember(0, true, false);
		if (!First || !TestEqual(TEXT("One sword"), Pool->GetOwnedWeapons().Num(), 1)) return;
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;
		TestEqual(TEXT("The empty preset got the free sword"), ActiveWeaponID(First), SwordID);
		TestEqual(TEXT("And wears it"), Pool->GetWearer(SwordID), UPoolSubsystem::GetCharacterKey(Members[0]));

		UInventoryComponent *Second = SpawnMember(1, false, false);
		if (!Second) return;
		TestFalse(TEXT("The second doesn't take the worn sword"), ActiveWeaponID(Second).IsValid());
	});

	It("keeps a crystal one member attaches on its own sword only (D-117)", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *First = SpawnMember(0, true, true);
		UInventoryComponent *Second = SpawnMember(1, false, false);
		if (!First || !Second) return;
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;
		TestTrue(TEXT("Fire gem owned"), First->AddCrystal(SHARED_GEAR_FIRE_GEM));
		if (!TestTrue(TEXT("Attached"), First->AttachCrystalToWeapon(SwordID, SHARED_GEAR_FIRE_GEM))) return;

		TestTrue(TEXT("The pool's sword holds it"), Pool->GetOwnedWeapons()[0].AttachedItem.IsCrystal());
		TestEqual(TEXT("The second member has no copy"), Second->Weapons.Num(), 0);
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
		TestEqual(TEXT("Same owner (B-321)"), Restored->GetGearOwner(SwordID), UPoolSubsystem::GetCharacterKey(Members[0]));
		TestTrue(TEXT("Starting gear stays banked"), Restored->HasBankedGear(UPoolSubsystem::GetCharacterKey(Members[0])));
	});

	It("keeps a preset weapon the authored inventory doesn't list, as starting gear", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *Member = SpawnMember(0, false, true);
		if (!Member) return;
		TestEqual(TEXT("Banked into the pool"), Pool->GetOwnedWeapons().Num(), 1);
		TestTrue(TEXT("Still in its loadout"), ActiveWeaponID(Member).IsValid());
	});

	It("adopts one copy when two presets name the same unlisted weapon", [this]()
	{
		if (!Pool) return;
		AActor *Actor = TestWorld->SpawnCombatant(Members[0]);
		if (!Actor) return;
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		UInventoryComponent *Member = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		Member->SavedLoadouts = {SwordPreset(Sword), SwordPreset(Sword)};
		Member->ActiveLoadoutIndex = 0;
		TestTrue(TEXT("Drawn"), Member->DrawSharedGear());
		TestEqual(TEXT("One sword banked"), Pool->GetOwnedWeapons().Num(), 1);
		TestEqual(TEXT("Both presets hold it"), Member->SavedLoadouts[1].PrimaryWeapon.WeaponEntry.PersistentID,
				  Member->SavedLoadouts[0].PrimaryWeapon.WeaponEntry.PersistentID);
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
