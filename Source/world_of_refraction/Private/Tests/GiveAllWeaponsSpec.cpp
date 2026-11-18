// GiveAllWeaponsSpec.cpp
// wor.Debug.GiveAllWeapons: the give itself, headless. Every weapon arrives in the asked count even past the weapon
// inventory's room, a missing one is named, and the room applies again afterwards. Unproven here: the asset registry
// list and the loadout screen (PIE sheet debug-give-all-weapons).

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/CharacterData.h"
#include "Debug/Inventory/GiveAllWeaponsDebug.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FGiveAllWeaponsSpec, "WoR.Debug.GiveAllWeapons", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FGiveAllWeaponsSpec)

void FGiveAllWeaponsSpec::Define()
{
	It("gives the asked count of every weapon past the inventory's room, names a missing one, and leaves the room in force", [this]()
	{
		FWoRTestWorld TestWorld;
		TestTrue(TEXT("test world created"), TestWorld.Create());
		AActor *Owner = TestWorld.SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("owner"), Owner)) return;
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);

		const int32 Count = WoRGiveAllWeapons::DEFAULT_COUNT;
		TArray<UWeaponData *> Weapons;
		while (Weapons.Num() * Count <= InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS)
		{
			Weapons.Add(NewObject<UWeaponData>(GetTransientPackage()));
		}
		const int32 Real = Weapons.Num();
		Weapons.Add(nullptr);

		TArray<FString> Lines;
		const int32 Total = WoRGiveAllWeapons::GiveWeapons(*Inventory, Weapons, Count, Lines);
		TestEqual(TEXT("every piece added, past the room"), Total, Real * Count);
		TestEqual(TEXT("all owned"), Inventory->Weapons.Num(), Real * Count);
		TestEqual(TEXT("one line per weapon"), Lines.Num(), Weapons.Num());
		TestTrue(TEXT("the missing one is named as refused"), Lines.Last().EndsWith(TEXT("(REFUSED)")));
		TestFalse(TEXT("a given one is not"), Lines[0].EndsWith(TEXT("(REFUSED)")));
		TestFalse(TEXT("the room applies again"), Inventory->bPassingGearThrough || Inventory->AddWeapon(Weapons[0]));
		TestFalse(TEXT("the duplicate notice is back on"), Inventory->bSuppressDuplicateNotice);
	});
}

#endif
