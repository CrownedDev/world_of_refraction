// WheelWeaponCarrySpec.cpp
// B-503s2 (D-218): a weapon picked on the field wheel is the one in hand when the fight starts. Every map's pawn is
// rebuilt from the pool, and a lore loadout from its asset, so the weapon in hand lives on the member's record
// (FPartyMember::ActiveWeaponId) and each draw puts it back. The real spawn cascade runs: the components first, then
// the character data component's BeginPlay (InitializeFromCharacterData, DrawSharedGear).

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
#include "Field/FieldMovementComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryData.h"
#include "Loadout/FSavedLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"

BEGIN_DEFINE_SPEC(FWheelWeaponCarrySpec, "WoR.Field.WheelWeaponCarry", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UCharacterData *Data = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Axe = nullptr;
	FGuid MemberId;

	/** A member's pawn as a map spawns it: its components, then the character data (stamped with MemberId when
	 *  bStamped, as the trial and battle game modes do) whose BeginPlay draws the loadout. */
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

	static const UWeaponData *InHand(const AActor *Actor)
	{
		const ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
		return Loadout ? Loadout->GetActiveWeapon() : nullptr;
	}

	/** Picks the axe's own slice on the field wheel, as the player does. */
	bool PickAxe(AActor *Actor)
	{
		const UInventoryComponent *Inventory = Actor->FindComponentByClass<UInventoryComponent>();
		const FWeaponInventoryEntry *Owned = Inventory ? Inventory->Weapons.FindByPredicate([this](const FWeaponInventoryEntry &E) { return E.Weapon == Axe; }) : nullptr;
		UFieldMovementComponent *Field = FWoRTestWorld::AddComponent<UFieldMovementComponent>(Actor);
		const TArray<FFieldAttackEntry> Entries = Field->GetFieldAttackEntries();
		for (int32 Index = 0; Owned && Index < Entries.Num(); ++Index)
		{
			if (Entries[Index].WeaponId == Owned->PersistentID)
			{
				Field->SelectFieldAttack(Index);
				return true;
			}
		}
		return false;
	}
END_DEFINE_SPEC(FWheelWeaponCarrySpec)

void FWheelWeaponCarrySpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Carry Sword");
		Axe = NewObject<UWeaponData>(GetTransientPackage());
		Axe->Name = TEXT("Carry Axe");
		Data = FWoRTestWorld::NewCharacterData();
		Data->Name = TEXT("Wheel carry member");
		Data->CharacterClass = ECharacterClass::Generic;
		UInventoryData *Inventory = NewObject<UInventoryData>(GetTransientPackage());
		Inventory->Weapons = {Sword, Axe};
		FSavedLoadout Lore; // the character's own loadout: rebuilt from this asset on every map
		Lore.LoadoutName = TEXT("Sword and axe");
		Lore.RequiredClass = ECharacterClass::Generic;
		Lore.PrimarySlotType = EPrimarySlotType::Weapon;
		Lore.PrimaryWeapon = Sword;
		Lore.SecondarySlotType = ESecondarySlotType::Weapon;
		Lore.SecondaryWeapon = Axe;
		Lore.bShowPrimary = true;
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

	It("keeps the axe picked on the field wheel in hand when the fight's pawn is drawn (Crown's PIE note)", [this]()
	{
		AActor *Field = SpawnMemberPawn(/*bStamped=*/true);
		if (!TestNotNull(TEXT("Field pawn"), Field)) return;
		TestTrue(TEXT("the sword in hand at first"), InHand(Field) == Sword);
		TestTrue(TEXT("the axe's slice is on the wheel"), PickAxe(Field));
		TestTrue(TEXT("the pick puts the axe in hand on the map"), InHand(Field) == Axe);
		// D-221 (B-506): the pick and the weapon in hand are one write on the worn-gear record (LA-1).
		const UPoolSubsystem *Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		const FWornGear *Worn = Pool ? Pool->GetWornGear(UPoolSubsystem::GetCharacterKey(Data)) : nullptr;
		const UInventoryComponent *FieldInventory = Field->FindComponentByClass<UInventoryComponent>();
		const FWeaponInventoryEntry *OwnedAxe = FieldInventory->Weapons.FindByPredicate([this](const FWeaponInventoryEntry &E) { return E.Weapon == Axe; });
		TestTrue(TEXT("the record's ACTIVE entry is the axe"), Worn && OwnedAxe && Worn->ActiveGearID == OwnedAxe->PersistentID);
		TestTrue(TEXT("and the record holds the pick"), Worn && Worn->WheelPick.bPicked);
		Field->Destroy(); // the travel to the fight

		AActor *Battle = SpawnMemberPawn(/*bStamped=*/true);
		if (!TestNotNull(TEXT("Battle pawn"), Battle)) return;
		TestTrue(TEXT("the fight starts with the axe in hand"), InHand(Battle) == Axe);
		const UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		const FPartyMember *Record = PartySession ? PartySession->GetMemberCarriedState(0) : nullptr;
		TestTrue(TEXT("the pick is on the record (the field pawn is stamped)"), Record && Record->bFieldAttackPicked);
	});

	It("puts the recorded weapon in hand on a hub pawn once its controller stamps it", [this]()
	{
		AActor *Field = SpawnMemberPawn(/*bStamped=*/true);
		if (!TestNotNull(TEXT("Field pawn"), Field) || !TestTrue(TEXT("picked"), PickAxe(Field))) return;
		Field->Destroy();

		AActor *Hub = SpawnMemberPawn(/*bStamped=*/false);
		if (!TestNotNull(TEXT("Hub pawn"), Hub)) return;
		TestTrue(TEXT("unstamped, it can't read the record yet"), InHand(Hub) == Sword);
		Hub->FindComponentByClass<UCharacterDataComponent>()->StampPartyMember(MemberId);
		TestTrue(TEXT("stamped, the axe is in hand"), InHand(Hub) == Axe);
	});

	It("leaves an unstamped pawn's weapon alone (an enemy sharing the asset)", [this]()
	{
		AActor *Field = SpawnMemberPawn(/*bStamped=*/true);
		if (!TestNotNull(TEXT("Field pawn"), Field) || !TestTrue(TEXT("picked"), PickAxe(Field))) return;
		AActor *Enemy = SpawnMemberPawn(/*bStamped=*/false);
		TestTrue(TEXT("the enemy keeps its asset's sword"), InHand(Enemy) == Sword);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
