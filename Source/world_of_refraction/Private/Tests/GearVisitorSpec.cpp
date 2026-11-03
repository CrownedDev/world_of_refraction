// GearVisitorSpec.cpp
// B-306 (Q-244 A, D-110 unified loadout, step 6): one rule for which gear counts: the adapter in use and the weapon in
// hand, for stats, resistances and effects alike. Checked: a primary weapon whose slot type is None (a fresh or
// auto-filled loadout) counts; an evolution seat's secondary weapon gives its stats and effects; the adapter is
// visited before the weapon; a Caster's ring and a Resonator's adapter plus weapon feed stats and effects alike.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/EffectDefinition.h"

namespace
{
	constexpr int32 GEAR_VISITOR_TEST_DEFENSE = 7;
	constexpr int32 GEAR_VISITOR_ADAPTER_DEFENSE = 3;

	template <typename TGear>
	TGear *GearVisitorTestGear()
	{
		TGear *Gear = NewObject<TGear>(GetTransientPackage());
		UEffectDefinition *Bundle = NewObject<UEffectDefinition>(GetTransientPackage());
		FSkillEffect Effect;
		FSkillEffectPayload Payload;
		Payload.EffectType = ESkillEffectType::DefenseBuff;
		Effect.Payloads.Add(Payload);
		Bundle->Effects.Add(Effect);
		Gear->ReferencedEffects.Add(Bundle);
		return Gear;
	}

	UWeaponData *GearVisitorTestWeapon() { return GearVisitorTestGear<UWeaponData>(); }
}

BEGIN_DEFINE_SPEC(FGearVisitorSpec, "WoR.Loadout.GearVisitor", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	static FCombatLoadout &ActiveOf(ULoadoutComponent *Loadout)
	{
		UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}

	ULoadoutComponent *SpawnAs(ECharacterClass Class, UWeaponData *Primary)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = Class;
		return TestWorld->SpawnCombatantWithLoadout(Data, Primary);
	}

	ULoadoutComponent *SpawnGeneric(UWeaponData *Primary) { return SpawnAs(ECharacterClass::Generic, Primary); }

	static FRingLoadoutEntry TestAdapter()
	{
		FRingLoadoutEntry Adapter;
		Adapter.RingEntry.Ring = GearVisitorTestGear<URingData>();
		Adapter.RingEntry.StatBonus.BonusDefense = GEAR_VISITOR_ADAPTER_DEFENSE;
		return Adapter;
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

	It("counts a primary weapon whose slot type is None for stats and effects (a fresh loadout)", [this]()
	{
		ULoadoutComponent *Loadout = SpawnGeneric(GearVisitorTestWeapon());
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		ActiveOf(Loadout).PrimarySlotType = EPrimarySlotType::None;
		ActiveOf(Loadout).PrimaryWeapon.WeaponEntry.StatBonus.BonusDefense = GEAR_VISITOR_TEST_DEFENSE;
		TestEqual(TEXT("its stats"), Loadout->GetActiveStatBonus(Loadout->GetOwner()).BonusDefense, GEAR_VISITOR_TEST_DEFENSE);
		TestEqual(TEXT("its effects too (Q-244)"), Loadout->GetActiveEffects(Loadout->GetOwner()).Num(), 1);
	});

	It("counts an evolution seat's secondary weapon for its stats and effects (Q-244)", [this]()
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
		TestEqual(TEXT("its stats count"), Loadout->GetActiveStatBonus(Loadout->GetOwner()).BonusDefense, GEAR_VISITOR_TEST_DEFENSE);
		TestEqual(TEXT("its effects count"), Loadout->GetActiveEffects(Loadout->GetOwner()).Num(), 1);
	});

	It("visits the adapter in use, then the weapon in hand", [this]()
	{
		ULoadoutComponent *Loadout = SpawnGeneric(nullptr);
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		FCombatLoadout &L = ActiveOf(Loadout);
		L.PrimarySlotType = EPrimarySlotType::Ring;
		L.PrimaryRing = TestAdapter();
		L.SecondarySlotType = ESecondarySlotType::Weapon;
		L.SecondaryWeapon.WeaponEntry.Weapon = GearVisitorTestWeapon();
		TArray<FString> Order;
		Loadout->ForEachContributingGear([&Order](const FWeaponLoadoutEntry *Weapon, const FRingLoadoutEntry *Adapter)
		{
			Order.Add(Weapon ? TEXT("Weapon") : TEXT("Adapter"));
		});
		TestEqual(TEXT("adapter, then weapon"), FString::Join(Order, TEXT(",")), FString(TEXT("Adapter,Weapon")));
	});

	It("counts a Caster's ring for stats and effects alike", [this]()
	{
		ULoadoutComponent *Loadout = SpawnAs(ECharacterClass::Caster, nullptr);
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		FCombatLoadout &L = ActiveOf(Loadout);
		L.PrimarySlotType = EPrimarySlotType::Ring;
		L.PrimaryRing = TestAdapter();
		TestEqual(TEXT("its stats"), Loadout->GetActiveStatBonus(Loadout->GetOwner()).BonusDefense, GEAR_VISITOR_ADAPTER_DEFENSE);
		TestEqual(TEXT("its effects"), Loadout->GetActiveEffects(Loadout->GetOwner()).Num(), 1);
	});

	It("counts a Resonator's adapter and its weapon for stats and effects alike", [this]()
	{
		ULoadoutComponent *Loadout = SpawnAs(ECharacterClass::Resonator, GearVisitorTestWeapon());
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		FCombatLoadout &L = ActiveOf(Loadout);
		L.PrimaryWeapon.WeaponEntry.StatBonus.BonusDefense = GEAR_VISITOR_TEST_DEFENSE;
		L.RingLoadout = {TestAdapter()};
		L.ActiveRingIndex = 0;
		TestEqual(TEXT("both stats"), Loadout->GetActiveStatBonus(Loadout->GetOwner()).BonusDefense,
				  GEAR_VISITOR_TEST_DEFENSE + GEAR_VISITOR_ADAPTER_DEFENSE);
		TestEqual(TEXT("both effects"), Loadout->GetActiveEffects(Loadout->GetOwner()).Num(), 2);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
