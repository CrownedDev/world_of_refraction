// ActiveWeaponRuleSpec.cpp
// B-503p / B-503q: the stance and the weapon skill lists follow the one ACTIVE weapon rule (GetActiveWeaponLoadout):
// a Generic's toggle picks the stance, and a primary weapon whose slot type is None (AutoPopulateLoadout leaves it)
// stands, attacks and lists its abilities as a Weapon-type one does.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/StanceData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"

BEGIN_DEFINE_SPEC(FActiveWeaponRuleSpec, "WoR.Loadout.ActiveWeaponRule", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	static FCombatLoadout &ActiveOf(ULoadoutComponent *Loadout)
	{
		UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}

	static UWeaponData *NewWeapon()
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->WeaponAttack = NewObject<UAbilityData>(GetTransientPackage());
		Weapon->WeaponStance = NewObject<UStanceData>(GetTransientPackage());
		return Weapon;
	}

	ULoadoutComponent *SpawnGeneric(UWeaponData *Primary)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Generic;
		return TestWorld->SpawnCombatantWithLoadout(Data, Primary);
	}
END_DEFINE_SPEC(FActiveWeaponRuleSpec)

void FActiveWeaponRuleSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("stands in the ACTIVE weapon's stance, following a Generic's toggle (B-503p)", [this]()
	{
		UWeaponData *Primary = NewWeapon();
		ULoadoutComponent *Loadout = SpawnGeneric(Primary);
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		UWeaponData *Secondary = NewWeapon();
		ActiveOf(Loadout).SecondarySlotType = ESecondarySlotType::Weapon;
		ActiveOf(Loadout).SecondaryWeapon.WeaponEntry.Weapon = Secondary;
		TestTrue(TEXT("the primary's stance"), Loadout->GetCurrentStance() == Primary->WeaponStance);
		Loadout->ToggleEquipment();
		TestTrue(TEXT("toggled: the weapon in hand is the secondary"), Loadout->GetActiveWeapon() == Secondary);
		TestTrue(TEXT("and so is the stance"), Loadout->GetCurrentStance() == Secondary->WeaponStance);
	});

	It("stands, attacks and lists the abilities of a primary weapon whose slot type is None (B-503q)", [this]()
	{
		UWeaponData *Primary = NewWeapon();
		ULoadoutComponent *Loadout = SpawnGeneric(Primary);
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		UAbilityData *Slotted = NewObject<UAbilityData>(GetTransientPackage());
		ActiveOf(Loadout).PrimarySlotType = EPrimarySlotType::None;
		ActiveOf(Loadout).PrimaryWeapon.AssignedAbilities = {Slotted};
		TestTrue(TEXT("the weapon in hand"), Loadout->GetActiveWeapon() == Primary);
		TestTrue(TEXT("its stance, not unarmed"), Loadout->GetCurrentStance() == Primary->WeaponStance);
		TestTrue(TEXT("its attack is listed"), Loadout->GetAllWeaponAttacks().Contains(Primary->WeaponAttack));
		TestTrue(TEXT("its slotted ability is listed"), ActiveOf(Loadout).GetAllAbilities().Contains(Slotted));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
