// GiveTestWeaponsSpec.cpp
// wor.Debug.GiveTestWeapons, headless: every test asset present here loads as a weapon; the give adds the asked count of
// each with the duplicate notice left on afterwards, names a missing one, and is refused by a full inventory.
// Unproven here: the loadout screen and the weapons in the hand (PIE sheet debug-give-test-weapons).

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/CharacterData.h"
#include "Debug/Inventory/GiveTestWeaponsDebug.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FGiveTestWeaponsSpec, "WoR.Debug.GiveTestWeapons", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FGiveTestWeaponsSpec)

void FGiveTestWeaponsSpec::Define()
{
	It("names test weapons that load where their asset is in this checkout, and few enough to fit an empty inventory", [this]()
	{
		int32 Asked = 0;
		int32 Missing = 0;
		for (const WoRGiveTestWeapons::FTestWeapon &Entry : WoRGiveTestWeapons::TEST_WEAPONS)
		{
			Asked += Entry.Count;
			TestTrue(TEXT("a count of at least one"), Entry.Count >= 1);
			// A test asset made in the editor may not be committed yet: one that is absent is counted, not failed.
			if (FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(FString(Entry.Path))))
			{
				TestNotNull(Entry.Path, LoadObject<UWeaponData>(nullptr, Entry.Path));
			}
			else
			{
				++Missing;
			}
		}
		AddInfo(FString::Printf(TEXT("%d of %d test weapon assets are not in this checkout"), Missing, static_cast<int32>(UE_ARRAY_COUNT(WoRGiveTestWeapons::TEST_WEAPONS))));
		TestTrue(TEXT("the set fits the weapon room"), Asked <= InventoryConstants::MAX_WEAPON_INVENTORY_SLOTS);
	});

	It("gives the asked count of each weapon quietly, names a missing one, and is refused by a full inventory", [this]()
	{
		FWoRTestWorld TestWorld;
		TestTrue(TEXT("test world created"), TestWorld.Create());
		AActor *Owner = TestWorld.SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("owner"), Owner)) return;
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Owner);

		const int32 Count = 2;
		const TArray<UWeaponData *> Weapons = {NewObject<UWeaponData>(GetTransientPackage()), NewObject<UWeaponData>(GetTransientPackage()), nullptr};
		TArray<FString> Lines;
		TestEqual(TEXT("every real piece added"), WoRGiveTestWeapons::GiveWeapons(*Inventory, Weapons, Count, Lines), 2 * Count);
		TestEqual(TEXT("all owned"), Inventory->Weapons.Num(), 2 * Count);
		TestEqual(TEXT("one line per weapon"), Lines.Num(), Weapons.Num());
		TestFalse(TEXT("a given one is not refused"), Lines[0].EndsWith(TEXT("(REFUSED)")));
		TestTrue(TEXT("the missing one is named as refused"), Lines.Last().EndsWith(TEXT("(REFUSED)")));
		TestFalse(TEXT("the duplicate notice is back on"), Inventory->bSuppressDuplicateNotice);

		while (Inventory->AddWeapon(NewObject<UWeaponData>(GetTransientPackage())))
		{
		}
		Lines.Reset();
		TestEqual(TEXT("a full inventory takes none"), WoRGiveTestWeapons::GiveWeapons(*Inventory, {Weapons[0]}, Count, Lines), 0);
		TestTrue(TEXT("and says so"), Lines.Num() == 1 && Lines[0].EndsWith(TEXT("(REFUSED)")));
	});
}

#endif
