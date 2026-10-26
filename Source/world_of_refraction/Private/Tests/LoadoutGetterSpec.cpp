// LoadoutGetterSpec.cpp
// ULoadoutComponent's read getters (B-022): the active loadout is the inventory's saved
// loadout at ActiveLoadoutIndex (or an empty default), and the loadout's class follows the
// owner's CharacterData. Written against the copying getter and the synced CharacterClass
// field first, so the reference getter and GetCharacterClass must give the same results.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	const ECharacterClass LOADOUT_TEST_CLASSES[] = {ECharacterClass::Generic, ECharacterClass::Caster, ECharacterClass::Resonator};
	const TCHAR *LOADOUT_TEST_SECOND_NAME = TEXT("Second");

	template <class TComponent>
	TComponent *LoadoutTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FLoadoutGetterSpec, "WoR.Loadout.Getters",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;

/** A combatant of the given class with an inventory and a loadout initialised from its data. */
ULoadoutComponent *SpawnLoadoutOwner(ECharacterClass CharClass, UInventoryComponent *&OutInventory)
{
	UCharacterData *Data = FWoRTestWorld::NewCharacterData();
	Data->CharacterClass = CharClass;
	AActor *Actor = TestWorld->SpawnCombatant(Data);
	if (!Actor)
	{
		return nullptr;
	}
	OutInventory = LoadoutTestAddComponent<UInventoryComponent>(Actor);
	ULoadoutComponent *Loadout = LoadoutTestAddComponent<ULoadoutComponent>(Actor);
	Loadout->InitializeFromCharacterData(Data, OutInventory);
	return Loadout;
}
END_DEFINE_SPEC(FLoadoutGetterSpec)

void FLoadoutGetterSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	Describe("CharacterClass", [this]()
	{
		It("matches the owner's CharacterData for every class", [this]()
		{
			for (const ECharacterClass CharClass : LOADOUT_TEST_CLASSES)
			{
				UInventoryComponent *Inventory = nullptr;
				ULoadoutComponent *Loadout = SpawnLoadoutOwner(CharClass, Inventory);
				if (!TestNotNull(TEXT("loadout owner spawned"), Loadout))
				{
					return;
				}
				const FString ClassName = UEnum::GetValueAsString(CharClass);
				TestEqual(*FString::Printf(TEXT("%s: class"), *ClassName), Loadout->GetCharacterClass(), CharClass);
				TestEqual(*FString::Printf(TEXT("%s: synced field"), *ClassName), Loadout->CharacterClass, CharClass);
			}
		});
	});

	Describe("GetActiveLoadout", [this]()
	{
		It("returns the saved loadout at the active index", [this]()
		{
			UInventoryComponent *Inventory = nullptr;
			ULoadoutComponent *Loadout = SpawnLoadoutOwner(ECharacterClass::Caster, Inventory);
			if (!TestNotNull(TEXT("loadout owner spawned"), Loadout))
			{
				return;
			}
			TestEqual(TEXT("one seeded loadout"), Inventory->SavedLoadouts.Num(), 1);
			TestEqual(TEXT("seeded loadout is active"), Loadout->GetActiveLoadout().LoadoutName,
					  Inventory->SavedLoadouts[0].LoadoutName);

			FCombatLoadout Second;
			Second.LoadoutName = LOADOUT_TEST_SECOND_NAME;
			Second.PrimarySlotType = EPrimarySlotType::Ring;
			Inventory->SavedLoadouts.Add(Second);
			TestTrue(TEXT("switched to the second loadout"), Loadout->SetActiveLoadoutIndex(1));
			TestEqual(TEXT("second loadout name"), Loadout->GetActiveLoadout().LoadoutName, FString(LOADOUT_TEST_SECOND_NAME));
			TestEqual(TEXT("second loadout slot type"), Loadout->GetActiveLoadout().PrimarySlotType, EPrimarySlotType::Ring);

			Inventory->SavedLoadouts[1].PrimarySlotType = EPrimarySlotType::Weapon;
			TestEqual(TEXT("reads the saved loadout's current state"), Loadout->GetActiveLoadout().PrimarySlotType,
					  EPrimarySlotType::Weapon);
		});

		It("returns an empty loadout when the active index is out of range", [this]()
		{
			UInventoryComponent *Inventory = nullptr;
			ULoadoutComponent *Loadout = SpawnLoadoutOwner(ECharacterClass::Generic, Inventory);
			if (!TestNotNull(TEXT("loadout owner spawned"), Loadout))
			{
				return;
			}
			Inventory->ActiveLoadoutIndex = Inventory->SavedLoadouts.Num();
			const FCombatLoadout Empty;
			TestEqual(TEXT("empty name"), Loadout->GetActiveLoadout().LoadoutName, Empty.LoadoutName);
			TestEqual(TEXT("empty slot type"), Loadout->GetActiveLoadout().PrimarySlotType, Empty.PrimarySlotType);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
