// GiveSpec.cpp
// B-323 (D-117): giving between your own characters. A given sword leaves A and is B's (owner, armouries and
// loadouts follow); giving A's primary is refused (D-116); a worn secondary comes off as it goes; a piece B
// already owns a copy of wakes B's duplicate choice (D-115); a pouch stack moves; a stack to a full pouch is
// refused.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "InventoryChangeRecorder.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"

namespace
{
	constexpr int32 GIVE_PARTY_SIZE = 2;
	constexpr int32 GIVE_STACK = 3;
	constexpr int32 GIVE_PART = 2;

	/** The Index-th gem type (Garnet first) at tier F: ten distinct item types fill ten slots. */
	FCrystalId GiveItemType(int32 Index)
	{
		return FCrystalId(static_cast<ECrystalType>(static_cast<int32>(ECrystalType::Garnet) + Index), EItemTier::F_Tier);
	}
	const FCrystalId GIVE_ELEVENTH(ECrystalType::DamageStone, EItemTier::F_Tier);
}

BEGIN_DEFINE_SPEC(FGiveSpec, "WoR.Pool.Give", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Dagger = nullptr;
	TArray<UCharacterData *> Members;

	FName KeyOf(int32 Index) const { return UPoolSubsystem::GetCharacterKey(Members[Index]); }

	/** A loadout with the sword as primary and, when given, the dagger worn as the secondary weapon. */
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
END_DEFINE_SPEC(FGiveSpec)

void FGiveSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Give sword");
		Dagger = NewObject<UWeaponData>(GetTransientPackage());
		Dagger->Name = TEXT("Give dagger");
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		for (int32 Index = 0; Index < GIVE_PARTY_SIZE; ++Index)
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

	It("moves a given dagger from A to B: owner, armouries and A's loadout follow", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *A = SpawnMember(0, true, true);
		UInventoryComponent *B = SpawnMember(1, false, false);
		if (!A || !B) return;
		const FGuid DaggerID = OwnedID(A, Dagger);
		if (!TestTrue(TEXT("A owns the dagger"), DaggerID.IsValid())) return;
		TestEqual(TEXT("A wears it as secondary"), A->GetLoadoutsUsing(DaggerID).Num(), 1);
		TestEqual(TEXT("B has no dagger"), OwnedID(B, Dagger), FGuid());

		const FText Refusal = WoRGive::TryGiveGear(TestWorld->GetWorld(), KeyOf(0), KeyOf(1), DaggerID);
		TestTrue(FString::Printf(TEXT("Given [%s]"), *Refusal.ToString()), Refusal.IsEmpty());
		TestEqual(TEXT("The pool says it is B's"), Pool->GetGearOwner(DaggerID), KeyOf(1));
		TestEqual(TEXT("It left A"), OwnedID(A, Dagger), FGuid());
		TestEqual(TEXT("It came off A's loadout"), A->GetLoadoutsUsing(DaggerID).Num(), 0);
		TestEqual(TEXT("It is B's, same instance"), OwnedID(B, Dagger), DaggerID);
		TestEqual(TEXT("A still has its sword"), Pool->GetGearOwner(OwnedID(A, Sword)), KeyOf(0));
	});

	It("refuses to give a loadout's primary (D-116)", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *A = SpawnMember(0, false, false);
		UInventoryComponent *B = SpawnMember(1, false, false);
		if (!A || !B) return;
		const FGuid SwordID = OwnedID(A, Sword);
		const FText Refusal = WoRGive::TryGiveGear(TestWorld->GetWorld(), KeyOf(0), KeyOf(1), SwordID);
		TestFalse(TEXT("Refused with a reason"), Refusal.IsEmpty());
		TestEqual(TEXT("Still A's"), Pool->GetGearOwner(SwordID), KeyOf(0));
		TestEqual(TEXT("Still in A's armoury"), OwnedID(A, Sword), SwordID);
	});

	It("refuses what the giver doesn't own, and giving to oneself", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *A = SpawnMember(0, true, false);
		UInventoryComponent *B = SpawnMember(1, false, false);
		if (!A || !B) return;
		const FGuid DaggerID = OwnedID(A, Dagger);
		TestFalse(TEXT("B can't give A's dagger"), WoRGive::TryGiveGear(TestWorld->GetWorld(), KeyOf(1), KeyOf(0), DaggerID).IsEmpty());
		TestFalse(TEXT("A can't give to A"), WoRGive::TryGiveGear(TestWorld->GetWorld(), KeyOf(0), KeyOf(0), DaggerID).IsEmpty());
		TestEqual(TEXT("Still A's"), Pool->GetGearOwner(DaggerID), KeyOf(0));
	});

	It("refuses gear to a run-only recruit, and the recruit hears no duplicate for another member's new piece", [this]()
	{
		if (!Pool) return;
		TestWorld->GetSubsystem<UPartySessionSubsystem>()->GetLocalParty()->Members[1].bRunOnly = true;
		UInventoryComponent *A = SpawnMember(0, true, false);
		UInventoryComponent *Recruit = SpawnMember(1, false, false);
		if (!A || !Recruit) return;
		const FGuid DaggerID = OwnedID(A, Dagger);
		TestFalse(TEXT("A recruit can't be given gear"), WoRGive::TryGiveGear(TestWorld->GetWorld(), KeyOf(0), KeyOf(1), DaggerID).IsEmpty());
		TestEqual(TEXT("Still A's"), Pool->GetGearOwner(DaggerID), KeyOf(0));

		UDuplicateGearRecorder *Recorder = NewObject<UDuplicateGearRecorder>(GetTransientPackage());
		Recorder->Listen(Recruit);
		A->AddWeapon(Dagger);
		TestEqual(TEXT("A's second dagger is no gift to the recruit"), Recorder->NewIDs.Num(), 0);
	});

	It("wakes the receiver's duplicate choice when it already owns a copy (D-115)", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *A = SpawnMember(0, true, false);
		UInventoryComponent *B = SpawnMember(1, true, false);
		if (!A || !B) return;
		UDuplicateGearRecorder *Recorder = NewObject<UDuplicateGearRecorder>(GetTransientPackage());
		Recorder->Listen(B);
		const FGuid GivenID = OwnedID(A, Dagger);
		const FGuid KeptID = OwnedID(B, Dagger);
		TestTrue(TEXT("Given"), WoRGive::TryGiveGear(TestWorld->GetWorld(), KeyOf(0), KeyOf(1), GivenID).IsEmpty());
		if (!TestEqual(TEXT("One duplicate notice"), Recorder->NewIDs.Num(), 1)) return;
		TestEqual(TEXT("The given piece is the new one"), Recorder->NewIDs[0], GivenID);
		TestEqual(TEXT("Against the copy B had"), Recorder->OwnedIDs[0], KeptID);
		TestEqual(TEXT("B holds both until it chooses"), B->Weapons.FilterByPredicate([this](const FWeaponInventoryEntry &E) { return E.Weapon == Dagger; }).Num(), 2);
	});

	It("gives part of a pouch stack, and refuses a stack a full pouch can't take", [this]()
	{
		if (!Pool) return;
		const FName A = KeyOf(0);
		const FName B = KeyOf(1);
		const FCrystalId Garnet = GiveItemType(0);
		Pool->CarryOrStore(A, Garnet, GIVE_STACK);
		TestTrue(TEXT("Two given"), WoRGive::TryGiveCarried(TestWorld->GetWorld(), A, B, Garnet, GIVE_PART).IsEmpty());
		TestEqual(TEXT("A keeps one"), Pool->GetCarriedCount(A, Garnet), GIVE_STACK - GIVE_PART);
		TestEqual(TEXT("B has two"), Pool->GetCarriedCount(B, Garnet), GIVE_PART);
		TestFalse(TEXT("A can't give more than it carries"), WoRGive::TryGiveCarried(TestWorld->GetWorld(), A, B, Garnet, GIVE_PART).IsEmpty());

		for (int32 Index = 1; Index <= InventoryConstants::ITEM_SLOTS_START - 1; ++Index)
		{
			Pool->CarryOrStore(B, GiveItemType(Index), 1);
		}
		TestEqual(TEXT("B's pouch is full"), Pool->GetUsedItemSlots(B), InventoryConstants::ITEM_SLOTS_START);
		Pool->CarryOrStore(A, GIVE_ELEVENTH, 1);
		const FText Refusal = WoRGive::TryGiveCarried(TestWorld->GetWorld(), A, B, GIVE_ELEVENTH, 1);
		TestFalse(TEXT("A full pouch refuses a new type"), Refusal.IsEmpty());
		TestEqual(TEXT("A still carries it"), Pool->GetCarriedCount(A, GIVE_ELEVENTH), 1);
		TestEqual(TEXT("B doesn't"), Pool->GetCarriedCount(B, GIVE_ELEVENTH), 0);
		TestTrue(TEXT("But a type B already stacks still fits"), WoRGive::TryGiveCarried(TestWorld->GetWorld(), A, B, Garnet, 1).IsEmpty());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
