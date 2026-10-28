// ActiveGearRuleSpec.cpp
// B-243 (DRY audit A2): the active-weapon / ring rule is written once (the entry getters); the asset getters
// read it. For each class and slot case, each getter pair agrees and the rule picks the expected piece.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

BEGIN_DEFINE_SPEC(FActiveGearRuleSpec, "WoR.Loadout.ActiveGearRule", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UWeaponData *Sword = nullptr;
	UWeaponData *Axe = nullptr;
	URingData *Ring = nullptr;

	/** A combatant of Class whose only (active) saved loadout is Gear. */
	ULoadoutComponent *SpawnWith(ECharacterClass Class, const FCombatLoadout &Gear)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = Class;
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		if (!Actor)
		{
			return nullptr;
		}
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		Inventory->SavedLoadouts = {Gear};
		Inventory->ActiveLoadoutIndex = 0;
		return FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
	}

	void ExpectPairsAgree(const ULoadoutComponent &Loadout, const FString &Case)
	{
		const FWeaponLoadoutEntry *ActiveWeapon = Loadout.GetActiveWeaponLoadout();
		const FWeaponLoadoutEntry *PrimaryWeapon = Loadout.GetPrimaryWeaponLoadout();
		const FWeaponLoadoutEntry *SecondaryWeapon = Loadout.GetSecondaryWeaponLoadout();
		const FRingLoadoutEntry *PrimaryRing = Loadout.GetPrimaryRingLoadout();
		const FRingLoadoutEntry *ActiveRing = Loadout.GetActiveRingLoadout();
		TestTrue(Case + TEXT(": active weapon pair"), Loadout.GetActiveWeapon() == (ActiveWeapon ? ActiveWeapon->WeaponEntry.Weapon : nullptr));
		TestTrue(Case + TEXT(": primary weapon pair"), Loadout.GetPrimaryWeapon() == (PrimaryWeapon ? PrimaryWeapon->WeaponEntry.Weapon : nullptr));
		TestTrue(Case + TEXT(": secondary weapon pair"),
				 Loadout.GetSecondaryWeapon() == (SecondaryWeapon ? SecondaryWeapon->WeaponEntry.Weapon : nullptr));
		TestTrue(Case + TEXT(": primary ring pair"), Loadout.GetPrimaryRing() == (PrimaryRing ? PrimaryRing->RingEntry.Ring : nullptr));
		TestTrue(Case + TEXT(": active ring pair"), Loadout.GetActiveRing() == (ActiveRing ? ActiveRing->RingEntry.Ring : nullptr));
	}
END_DEFINE_SPEC(FActiveGearRuleSpec)

void FActiveGearRuleSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Axe = NewObject<UWeaponData>(GetTransientPackage());
		Ring = NewObject<URingData>(GetTransientPackage());
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("Generic, two weapons: the shown one is active", [this]()
	{
		FCombatLoadout Gear;
		Gear.PrimarySlotType = EPrimarySlotType::Weapon;
		Gear.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Gear.SecondarySlotType = ESecondarySlotType::Weapon;
		Gear.SecondaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Axe);
		for (const bool bShowPrimary : {true, false})
		{
			Gear.bShowPrimary = bShowPrimary;
			const ULoadoutComponent *Loadout = SpawnWith(ECharacterClass::Generic, Gear);
			if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
			TestTrue(TEXT("shown weapon"), Loadout->GetActiveWeapon() == (bShowPrimary ? Sword : Axe));
			ExpectPairsAgree(*Loadout, bShowPrimary ? TEXT("Generic primary shown") : TEXT("Generic secondary shown"));
		}
	});

	It("an evolved Generic uses its secondary weapon; an evolved Caster has none", [this]()
	{
		FCombatLoadout Gear;
		Gear.PrimarySlotType = EPrimarySlotType::Evolution;
		Gear.SecondarySlotType = ESecondarySlotType::Weapon;
		Gear.SecondaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Axe);
		const ULoadoutComponent *Generic = SpawnWith(ECharacterClass::Generic, Gear);
		const ULoadoutComponent *Caster = SpawnWith(ECharacterClass::Caster, Gear);
		if (!TestNotNull(TEXT("Generic"), Generic) || !TestNotNull(TEXT("Caster"), Caster)) return;
		TestTrue(TEXT("Generic: the secondary"), Generic->GetActiveWeapon() == Axe);
		TestNull(TEXT("Caster: none"), Caster->GetActiveWeapon());
		ExpectPairsAgree(*Generic, TEXT("evolved Generic"));
		ExpectPairsAgree(*Caster, TEXT("evolved Caster"));
	});

	It("a Caster with a ring primary has no weapon and its primary ring", [this]()
	{
		FCombatLoadout Gear;
		Gear.PrimarySlotType = EPrimarySlotType::Ring;
		Gear.PrimaryRing.RingEntry = FRingInventoryEntry::CreateFromRing(Ring);
		const ULoadoutComponent *Caster = SpawnWith(ECharacterClass::Caster, Gear);
		if (!TestNotNull(TEXT("Caster"), Caster)) return;
		TestNull(TEXT("no weapon"), Caster->GetActiveWeapon());
		TestTrue(TEXT("primary ring"), Caster->GetPrimaryRing() == Ring);
		ExpectPairsAgree(*Caster, TEXT("Caster ring primary"));
	});

	It("a Resonator's active ring is its ring loadout's active entry", [this]()
	{
		FCombatLoadout Gear;
		FRingLoadoutEntry &Entry = Gear.RingLoadout.AddDefaulted_GetRef();
		Entry.RingEntry = FRingInventoryEntry::CreateFromRing(Ring);
		Gear.ActiveRingIndex = 0;
		const ULoadoutComponent *Resonator = SpawnWith(ECharacterClass::Resonator, Gear);
		if (!TestNotNull(TEXT("Resonator"), Resonator)) return;
		TestTrue(TEXT("active ring"), Resonator->GetActiveRing() == Ring);
		ExpectPairsAgree(*Resonator, TEXT("Resonator ring loadout"));
	});
}

#endif
