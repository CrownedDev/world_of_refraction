// EvolutionSecondarySpec.cpp
// B-099 (D-027): a Generic whose primary slot holds an evolution keeps its secondary weapon,
// attacks with it and uses its abilities; every loadout query agrees.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "UI/Combat/FCombatCapabilities.h"

namespace
{
	template <class TComponent>
	TComponent *EvolutionSecondaryAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FEvolutionSecondarySpec, "WoR.Loadout.EvolutionSecondary",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ULoadoutComponent *Loadout = nullptr;
	UAbilityData *SwordAbility = nullptr;
END_DEFINE_SPEC(FEvolutionSecondarySpec)

void FEvolutionSecondarySpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());

		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Generic;
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		UInventoryComponent *Inventory = EvolutionSecondaryAddComponent<UInventoryComponent>(Actor);
		Loadout = EvolutionSecondaryAddComponent<ULoadoutComponent>(Actor);
		Loadout->InitializeFromCharacterData(Data, Inventory);

		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimarySlotType = EPrimarySlotType::Evolution;
		Active.PrimaryEvolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
		Active.SecondarySlotType = ESecondarySlotType::Weapon;
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Test Sword");
		Active.SecondaryWeapon.WeaponEntry.Weapon = Sword;
		SwordAbility = NewObject<UAbilityData>(GetTransientPackage());
		Active.SecondaryWeapon.AssignedAbilities = {SwordAbility};
	});

	AfterEach([this]()
	{
		Loadout = nullptr;
		SwordAbility = nullptr;
		TestWorld.Reset();
	});

	It("keeps the secondary weapon with an evolution primary (B-099)", [this]()
	{
		TestTrue(TEXT("has secondary equipment"), Loadout->HasSecondaryEquipment());
		TestTrue(TEXT("the secondary is the weapon in hand"), Loadout->GetActiveWeaponLoadout() == Loadout->GetSecondaryWeaponLoadout());
		TestFalse(TEXT("no ring in the secondary slot"), Loadout->HasRingInSecondary());
	});

	It("attacks with the sword and uses its abilities, with no weapon switch", [this]()
	{
		const FCombatCapabilities Caps = FCombatCapabilities::BuildFrom(
			Loadout, ECharacterClass::Generic, FLinearColor::White, [](int32) { return FLinearColor::White; });
		TestTrue(TEXT("can attack"), Caps.bCanAttack);
		TestTrue(TEXT("the sword's ability is offered"), Caps.WeaponAbilities.Contains(SwordAbility));
		TestFalse(TEXT("one weapon: no switch"), Caps.bCanSwitchWeapon);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
