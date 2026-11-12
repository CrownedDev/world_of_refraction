// ClassSlotsNotGatesSpec.cpp
// B-097 (D-027): a class is made by its slots, not a skill gate. Every class with a weapon in hand is offered that
// weapon's abilities (the class gates CanUseAbilities / CanUseSpells are retired); behaviour is unchanged.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "UI/Combat/FCombatCapabilities.h"

BEGIN_DEFINE_SPEC(FClassSlotsNotGatesSpec, "WoR.Character.ClassSlotsNotGates",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FClassSlotsNotGatesSpec)

void FClassSlotsNotGatesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
	});
	AfterEach([this]() { TestWorld.Reset(); });

	It("offers a held weapon's abilities to every class", [this]()
	{
		for (const ECharacterClass Class : {ECharacterClass::Generic, ECharacterClass::Caster, ECharacterClass::Resonator})
		{
			const FString ClassName = UEnum::GetValueAsString(Class);
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->CharacterClass = Class;
			UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
			Sword->Name = TEXT("Class Gate Test Sword");
			Sword->WeaponAttack = NewObject<UAbilityData>(GetTransientPackage());
			ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, Sword);
			UInventoryComponent *Inventory = Loadout ? Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
			if (!TestTrue(ClassName + TEXT(": setup"), Inventory && Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)))
			{
				continue;
			}
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
			Active.PrimarySlotType = EPrimarySlotType::Weapon;
			Active.PrimaryWeapon.WeaponEntry.Weapon = Sword;
			Active.PrimaryWeapon.AssignedAbilities = {Ability};

			const FCombatCapabilities Caps = FCombatCapabilities::BuildFrom(Loadout, Class, FLinearColor::White, [](int32) { return FLinearColor::White; });
			TestTrue(ClassName + TEXT(": can use abilities"), Caps.bCanUseAbilities);
			TestTrue(ClassName + TEXT(": the weapon's ability is offered"), Caps.WeaponAbilities.Contains(Ability));
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
