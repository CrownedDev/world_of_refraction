// WornGearSpec.cpp
// D-221 stage 1: the worn-gear record (FWornGear). B-505: it holds the loadout with no loss (ToWornGear and
// CreateFromWornGear give back the same pieces, places and weapon in hand), a piece the member no longer draws leaves
// its place empty, and the pool keeps one record per character with the wheel pick apart from the gear.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Debug/Inventory/LoadoutRulesDebug.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Inventory/InventoryData.h"
#include "Loadout/FSavedLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/WornGear.h"
#include "Pool/PoolSubsystem.h"

namespace
{
	const FName WORN_TEST_KEY(TEXT("/Game/Test/DA_WornGearMember"));
	constexpr int32 WORN_TEST_PICKED_ENTRY = 2;

	FWeaponInventoryEntry WornTestWeapon(const TCHAR *Name)
	{
		UWeaponData *Asset = NewObject<UWeaponData>(GetTransientPackage());
		Asset->Name = Name;
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Asset);
		Entry.PersistentID = FGuid::NewGuid();
		return Entry;
	}

	FRingInventoryEntry WornTestRing()
	{
		FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(NewObject<URingData>(GetTransientPackage()));
		Entry.PersistentID = FGuid::NewGuid();
		return Entry;
	}
}

BEGIN_DEFINE_SPEC(FWornGearSpec, "WoR.Loadout.WornGear", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UInventoryComponent *Inventory = nullptr;
	UEvolutionInventoryComponent *Evolutions = nullptr;
	FWeaponInventoryEntry Sword;
	FWeaponInventoryEntry Dagger;
	FWeaponInventoryEntry Axe;

	/** A Generic's sword-and-dagger set and its axe, the axe in hand. */
	FCombatLoadout GenericLoadout() const
	{
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = Sword;
		Loadout.PrimaryWeapon.OffHandEntry = Dagger;
		Loadout.SecondarySlotType = ESecondarySlotType::Weapon;
		Loadout.SecondaryWeapon.WeaponEntry = Axe;
		Loadout.bShowPrimary = false;
		return Loadout;
	}
END_DEFINE_SPEC(FWornGearSpec)

void FWornGearSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		AActor *Actor = TestWorld->GetWorld()->SpawnActor<AActor>();
		Evolutions = FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		Sword = WornTestWeapon(TEXT("Worn Sword"));
		Dagger = WornTestWeapon(TEXT("Worn Dagger"));
		Axe = WornTestWeapon(TEXT("Worn Axe"));
		Inventory->Weapons = {Sword, Dagger, Axe};
	});

	AfterEach([this]()
	{
		Inventory = nullptr;
		Evolutions = nullptr;
		TestWorld.Reset();
	});

	It("records a Generic's places, its set's off hand and the weapon in hand", [this]()
	{
		const FWornGear Worn = GenericLoadout().ToWornGear(ECharacterClass::Generic);
		TestEqual(TEXT("primary and secondary places"), Worn.Equipment.Num(), WornGearLayout::FIRST_ADAPTER_INDEX);
		TestEqual(TEXT("the sword first"), Worn.Equipment[WornGearLayout::PRIMARY_INDEX].GearID, Sword.PersistentID);
		TestEqual(TEXT("its dagger as the off hand"), Worn.Equipment[WornGearLayout::PRIMARY_INDEX].OffHandID, Dagger.PersistentID);
		TestEqual(TEXT("the axe second"), Worn.Equipment[WornGearLayout::SECONDARY_INDEX].GearID, Axe.PersistentID);
		TestEqual(TEXT("the axe is the ACTIVE entry"), Worn.ActiveGearID, Axe.PersistentID);
		TestEqual(TEXT("three pieces"), Worn.GetPieceIDs().Num(), 3);
	});

	It("builds the same loadout back from the record (no loss)", [this]()
	{
		const FWornGear Worn = GenericLoadout().ToWornGear(ECharacterClass::Generic);
		const FCombatLoadout Built = FCombatLoadout::CreateFromWornGear(Worn, ECharacterClass::Generic, Inventory, Evolutions);
		TestEqual(TEXT("the sword in the primary"), Built.PrimaryWeapon.WeaponEntry.PersistentID, Sword.PersistentID);
		TestEqual(TEXT("with its dagger"), Built.PrimaryWeapon.OffHandEntry.PersistentID, Dagger.PersistentID);
		TestEqual(TEXT("the axe in the secondary"), Built.SecondaryWeapon.WeaponEntry.PersistentID, Axe.PersistentID);
		TestFalse(TEXT("the axe in hand (the secondary shown)"), Built.bShowPrimary);
		TestTrue(TEXT("and it records the same again"), Built.ToWornGear(ECharacterClass::Generic).SameAs(Worn));
	});

	It("leaves a piece the member no longer draws out, instead of building it from its asset", [this]()
	{
		const FWornGear Worn = GenericLoadout().ToWornGear(ECharacterClass::Generic);
		Inventory->Weapons = {Sword}; // the dagger and the axe are gone (sold, or left at home for a run)
		const FCombatLoadout Built = FCombatLoadout::CreateFromWornGear(Worn, ECharacterClass::Generic, Inventory, Evolutions);
		TestEqual(TEXT("the sword stays"), Built.PrimaryWeapon.WeaponEntry.PersistentID, Sword.PersistentID);
		TestFalse(TEXT("no off hand"), Built.PrimaryWeapon.OffHandEntry.IsValid());
		TestFalse(TEXT("no secondary"), Built.HasSecondaryWeapon());
	});

	It("keeps a Resonator's adapters in order and a shown caster weapon as ACTIVE", [this]()
	{
		const FRingInventoryEntry First = WornTestRing();
		const FRingInventoryEntry Second = WornTestRing();
		Inventory->Rings = {First, Second};
		FCombatLoadout Resonator;
		Resonator.RingLoadout.AddDefaulted_GetRef().RingEntry = First;
		Resonator.RingLoadout.AddDefaulted_GetRef().RingEntry = Second;
		const FWornGear Worn = Resonator.ToWornGear(ECharacterClass::Resonator);
		TestEqual(TEXT("two adapters after the two places"), Worn.Equipment.Num(), WornGearLayout::FIRST_ADAPTER_INDEX + 2);
		const FCombatLoadout Built = FCombatLoadout::CreateFromWornGear(Worn, ECharacterClass::Resonator, Inventory, Evolutions);
		TestTrue(TEXT("same order back"), Built.RingLoadout.Num() == 2 && Built.RingLoadout[0].RingEntry.PersistentID == First.PersistentID &&
											   Built.RingLoadout[1].RingEntry.PersistentID == Second.PersistentID);

		FCombatLoadout Caster;
		Caster.PrimarySlotType = EPrimarySlotType::Weapon;
		Caster.PrimaryWeapon.WeaponEntry = Sword;
		Caster.bShowPrimary = false; // the caster's weapon put away
		const FWornGear Sheathed = Caster.ToWornGear(ECharacterClass::Caster);
		TestFalse(TEXT("a put-away weapon is not ACTIVE"), Sheathed.ActiveGearID.IsValid());
		TestFalse(TEXT("and stays put away"),
				  FCombatLoadout::CreateFromWornGear(Sheathed, ECharacterClass::Caster, Inventory, Evolutions).bShowPrimary);
	});

	It("keeps one record per character, the wheel pick apart from the gear", [this]()
	{
		UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		FWheelPick Pick;
		Pick.FieldAttackIndex = WORN_TEST_PICKED_ENTRY;
		Pick.bPicked = true;
		TestFalse(TEXT("no pick is kept before the first record"), Pool->SetWheelPick(WORN_TEST_KEY, Pick));
		TestNull(TEXT("no record yet"), Pool->GetWornGear(WORN_TEST_KEY));

		Pool->SetWornGear(WORN_TEST_KEY, GenericLoadout().ToWornGear(ECharacterClass::Generic));
		TestTrue(TEXT("the pick is kept once there is a record"), Pool->SetWheelPick(WORN_TEST_KEY, Pick));
		FCombatLoadout Changed = GenericLoadout();
		Changed.bShowPrimary = true; // the sword in hand now
		Pool->SetWornGear(WORN_TEST_KEY, Changed.ToWornGear(ECharacterClass::Generic));
		const FWornGear *Stored = Pool->GetWornGear(WORN_TEST_KEY);
		if (!TestNotNull(TEXT("record"), Stored)) return;
		TestEqual(TEXT("the gear write took"), Stored->ActiveGearID, Sword.PersistentID);
		TestTrue(TEXT("and kept the wheel pick"), Stored->WheelPick.bPicked && Stored->WheelPick.FieldAttackIndex == WORN_TEST_PICKED_ENTRY);
		TestTrue(TEXT("the debug line names the ACTIVE sword"),
				 ULoadoutRulesDebug::GetWornGearString(WORN_TEST_KEY, *Stored, Pool).Contains(TEXT("[ACTIVE]")));

		Pool->ResetStore();
		TestNull(TEXT("a store reset drops the records"), Pool->GetWornGear(WORN_TEST_KEY));
	});
}

// B-506: every map's pawn wears the record, and every change writes it. The real spawn cascade runs (the components,
// then the character data component's BeginPlay: InitializeFromCharacterData, DrawSharedGear), as WheelWeaponCarry.
BEGIN_DEFINE_SPEC(FWornGearMapsSpec, "WoR.Loadout.WornGear.Maps", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCharacterData *Data = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Axe = nullptr;
	FGuid MemberId;

	AActor *SpawnMemberPawn(bool bStamped)
	{
		AActor *Actor = TestWorld->GetWorld()->SpawnActor<AActor>();
		if (!Actor)
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		UCharacterDataComponent *CharComp = NewObject<UCharacterDataComponent>(Actor);
		CharComp->CharacterData = Data;
		CharComp->PartyMemberId = bStamped ? MemberId : FGuid();
		Actor->AddInstanceComponent(CharComp);
		CharComp->RegisterComponent(); // BeginPlay: the spawn cascade
		return Actor;
	}

	static bool HoldsSecondary(const AActor *Actor)
	{
		const UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
		return Inventory && Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex) &&
			   Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].HasSecondaryWeapon();
	}

	/** Takes the axe off through the real commit (TryChangeGear), as the loadout screen does. */
	bool TakeAxeOff(AActor *Actor)
	{
		ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
		const UInventoryComponent *Inventory = Actor->FindComponentByClass<UInventoryComponent>();
		FText Refusal;
		return Loadout && Inventory && Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [](FCombatLoadout &Changed)
		{
			Changed.SecondarySlotType = ESecondarySlotType::None;
			Changed.SecondaryWeapon = FWeaponLoadoutEntry();
		}, Refusal);
	}

	const FWornGear *Record() const
	{
		const UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		return Pool ? Pool->GetWornGear(UPoolSubsystem::GetCharacterKey(Data)) : nullptr;
	}
END_DEFINE_SPEC(FWornGearMapsSpec)

void FWornGearMapsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Maps Sword");
		Axe = NewObject<UWeaponData>(GetTransientPackage());
		Axe->Name = TEXT("Maps Axe");
		Data = FWoRTestWorld::NewCharacterData();
		Data->Name = TEXT("Worn gear member");
		Data->CharacterClass = ECharacterClass::Generic;
		UInventoryData *Inventory = NewObject<UInventoryData>(GetTransientPackage());
		Inventory->Weapons = {Sword, Axe};
		FSavedLoadout Lore; // the character's own loadout: before D-221 rebuilt from this asset on every map
		Lore.LoadoutName = TEXT("Sword and axe");
		Lore.RequiredClass = ECharacterClass::Generic;
		Lore.PrimarySlotType = EPrimarySlotType::Weapon;
		Lore.PrimaryWeapon = Sword;
		Lore.SecondarySlotType = ESecondarySlotType::Weapon;
		Lore.SecondaryWeapon = Axe;
		Inventory->SavedLoadouts = {Lore};
		Data->Inventory = Inventory;
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party)) return;
		Party->Members.Empty();
		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Data;
		Member.MemberId = FGuid::NewGuid();
		MemberId = Member.MemberId;
	});

	AfterEach([this]()
	{
		Data = nullptr;
		Sword = nullptr;
		Axe = nullptr;
		TestWorld.Reset();
	});

	It("seeds the record once from the character's own loadout", [this]()
	{
		AActor *Field = SpawnMemberPawn(/*bStamped=*/true);
		if (!TestNotNull(TEXT("Field pawn"), Field) || !TestNotNull(TEXT("seeded"), Record())) return;
		TestEqual(TEXT("the sword and the axe"), Record()->GetPieceIDs().Num(), 2);
	});

	It("keeps a gear change through the next map and a fight (CS-2, CS-7, CS-8)", [this]()
	{
		AActor *Field = SpawnMemberPawn(/*bStamped=*/true);
		if (!TestNotNull(TEXT("Field pawn"), Field) || !TestTrue(TEXT("the axe comes off"), TakeAxeOff(Field))) return;
		TestEqual(TEXT("the record has it at once"), Record() ? Record()->GetPieceIDs().Num() : 0, 1);
		Field->Destroy(); // the travel to the fight

		AActor *Battle = SpawnMemberPawn(/*bStamped=*/true);
		TestFalse(TEXT("the fight's pawn wears no axe (no rebuild from the asset)"), HoldsSecondary(Battle));
		Battle->Destroy(); // back to the trial

		AActor *Back = SpawnMemberPawn(/*bStamped=*/true);
		TestFalse(TEXT("and nor does the next map's"), HoldsSecondary(Back));
	});

	It("leaves the record alone for an unstamped pawn sharing the asset (an enemy)", [this]()
	{
		AActor *Field = SpawnMemberPawn(/*bStamped=*/true);
		if (!TestNotNull(TEXT("Field pawn"), Field) || !TestTrue(TEXT("the axe comes off"), TakeAxeOff(Field))) return;
		const FWornGear Before = Record() ? *Record() : FWornGear();
		AActor *Enemy = SpawnMemberPawn(/*bStamped=*/false);
		TestTrue(TEXT("the enemy keeps its asset's axe"), HoldsSecondary(Enemy));
		ULoadoutComponent *Loadout = Enemy->FindComponentByClass<ULoadoutComponent>();
		FText Refusal;
		Loadout->TryChangeGear(Enemy->FindComponentByClass<UInventoryComponent>()->ActiveLoadoutIndex,
							   [](FCombatLoadout &Changed) { Changed.bShowPrimary = false; }, Refusal);
		TestTrue(TEXT("its change never reaches the record"), Record() && Record()->SameAs(Before));
	});

	It("puts the record on a hub pawn once its controller stamps it", [this]()
	{
		AActor *Field = SpawnMemberPawn(/*bStamped=*/true);
		if (!TestNotNull(TEXT("Field pawn"), Field) || !TestTrue(TEXT("the axe comes off"), TakeAxeOff(Field))) return;
		Field->Destroy();

		AActor *Hub = SpawnMemberPawn(/*bStamped=*/false);
		TestTrue(TEXT("unstamped, it wears its asset's loadout"), HoldsSecondary(Hub));
		Hub->FindComponentByClass<UCharacterDataComponent>()->StampPartyMember(MemberId);
		TestFalse(TEXT("stamped, it wears the record"), HoldsSecondary(Hub));
		TestEqual(TEXT("and the stamp didn't write the asset's axe back"), Record() ? Record()->GetPieceIDs().Num() : 0, 1);
	});
}

#endif
