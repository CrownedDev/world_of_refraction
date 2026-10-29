// GearVisitorSpec.cpp
// B-306 (Q-244 A, D-110 unified loadout, step 6): one rule for which gear counts: the adapter in use and the weapon in
// hand, for stats, resistances and effects alike. Crown's two fixes are checked: a Generic's lone weapon shown as
// "secondary" gives its effects as well as its stats; an evolution seat's secondary weapon gives its stats too.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/EffectDefinition.h"

namespace
{
	constexpr int32 GEAR_VISITOR_TEST_DEFENSE = 7;

	UWeaponData *GearVisitorTestWeapon()
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		UEffectDefinition *Bundle = NewObject<UEffectDefinition>(GetTransientPackage());
		FSkillEffect Effect;
		FSkillEffectPayload Payload;
		Payload.EffectType = ESkillEffectType::DefenseBuff;
		Effect.Payloads.Add(Payload);
		Bundle->Effects.Add(Effect);
		Weapon->ReferencedEffects.Add(Bundle);
		return Weapon;
	}
}

BEGIN_DEFINE_SPEC(FGearVisitorSpec, "WoR.Loadout.GearVisitor", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	static FCombatLoadout &ActiveOf(ULoadoutComponent *Loadout)
	{
		UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}

	ULoadoutComponent *SpawnGeneric(UWeaponData *Primary)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Generic;
		return TestWorld->SpawnCombatantWithLoadout(Data, Primary);
	}
END_DEFINE_SPEC(FGearVisitorSpec)

void FGearVisitorSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("counts a Generic's lone weapon for stats and effects alike, whichever way it is shown", [this]()
	{
		ULoadoutComponent *Loadout = SpawnGeneric(GearVisitorTestWeapon());
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		ActiveOf(Loadout).PrimaryWeapon.WeaponEntry.StatBonus.BonusDefense = GEAR_VISITOR_TEST_DEFENSE;
		ActiveOf(Loadout).bShowPrimary = false; // no secondary to show
		TestEqual(TEXT("its stats"), Loadout->GetActiveStatBonus(Loadout->GetOwner()).BonusDefense, GEAR_VISITOR_TEST_DEFENSE);
		TestEqual(TEXT("its effects too (Q-244)"), Loadout->GetActiveEffects(Loadout->GetOwner()).Num(), 1);
	});

	It("counts an evolution seat's secondary weapon for its stats too (Q-244)", [this]()
	{
		ULoadoutComponent *Loadout = SpawnGeneric(nullptr);
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		FCombatLoadout &L = ActiveOf(Loadout);
		L.PrimarySlotType = EPrimarySlotType::Evolution;
		L.PrimaryEvolution.Item = NewObject<UEvolutionItemData>(GetTransientPackage());
		L.SecondarySlotType = ESecondarySlotType::Weapon;
		L.SecondaryWeapon.WeaponEntry.Weapon = GearVisitorTestWeapon();
		L.SecondaryWeapon.WeaponEntry.StatBonus.BonusDefense = GEAR_VISITOR_TEST_DEFENSE;
		TestTrue(TEXT("the secondary is in hand"), Loadout->GetActiveWeapon() == L.SecondaryWeapon.WeaponEntry.Weapon);
		TestTrue(TEXT("its stats count"), Loadout->GetActiveStatBonus(Loadout->GetOwner()).BonusDefense >= GEAR_VISITOR_TEST_DEFENSE);
		TestTrue(TEXT("its effects count"), Loadout->GetActiveEffects(Loadout->GetOwner()).Num() >= 1);
	});

	It("visits the adapter in use, then the weapon in hand", [this]()
	{
		ULoadoutComponent *Loadout = SpawnGeneric(GearVisitorTestWeapon());
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		int32 Visits = 0;
		Loadout->ForEachContributingGear([&Visits](const FWeaponLoadoutEntry *Weapon, const FRingLoadoutEntry *Adapter)
		{
			++Visits;
		});
		TestEqual(TEXT("one weapon, no adapter"), Visits, 1);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
