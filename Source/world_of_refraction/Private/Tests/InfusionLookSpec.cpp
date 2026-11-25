// InfusionLookSpec.cpp
// B-626 (D-277, Crown 2026-11-25): an infusion's look depends on its source. A weapon crystal shows the weapon glow, an
// adapter a ring flame on its finger, Innate / Evolution / Raw a body aura. Rings take fingers in loadout slot order,
// index first (Q-626: the right hand). Stacked same-element sources each show their kind. Unproven here: the look on
// screen (no art yet; docs/PIE/B-626.md).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Presentation/ImpactVisualDefaults.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Infusion/InfusionLook.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FLoadoutSlot.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	const FCrystalId LOOK_TEST_FIRE_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	const FCrystalId LOOK_TEST_WATER_GEM(ECrystalType::Sapphire, EItemTier::F_Tier);
	constexpr int32 LOOK_TEST_INTACT = 5;
	constexpr int32 LOOK_TEST_L1 = 1;

	FLoadoutSlot LookTestSlot(UEquipmentDataBase *Asset, EGearKind Kind)
	{
		FLoadoutSlot Slot;
		Slot.Gear.Asset = Asset;
		Slot.Gear.Kind = Kind;
		return Slot;
	}
}

BEGIN_DEFINE_SPEC(FInfusionLookSpec, "WoR.Presentation.InfusionLook",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FInfusionLookSpec)

void FInfusionLookSpec::Define()
{
	AfterEach([this]() { TestWorld.Reset(); });

	It("gives each source its look kind", [this]()
	{
		TestTrue(TEXT("weapon crystal: weapon glow"), InfusionLook::KindForSource(EInfusionSourceOption::WeaponCrystal) == EInfusionLookKind::Weapon);
		TestTrue(TEXT("active adapter: ring flame"), InfusionLook::KindForSource(EInfusionSourceOption::ActiveRing) == EInfusionLookKind::Ring);
		TestTrue(TEXT("primary adapter: ring flame"), InfusionLook::KindForSource(EInfusionSourceOption::PrimaryRing) == EInfusionLookKind::Ring);
		TestTrue(TEXT("innate: aura"), InfusionLook::KindForSource(EInfusionSourceOption::Innate) == EInfusionLookKind::Aura);
		TestTrue(TEXT("evolution: aura"), InfusionLook::KindForSource(EInfusionSourceOption::Evolution) == EInfusionLookKind::Aura);
		TestTrue(TEXT("raw: aura (Q-626)"), InfusionLook::KindForSource(EInfusionSourceOption::Raw) == EInfusionLookKind::Aura);
		TestTrue(TEXT("no infusion: none"), InfusionLook::KindForSource(EInfusionSourceOption::None) == EInfusionLookKind::None);
		TestTrue(TEXT("a look only replaces its own kind"), InfusionLook::CanWear(EInfusionLookKind::Ring, EInfusionLookKind::Ring) &&
															   !InfusionLook::CanWear(EInfusionLookKind::Aura, EInfusionLookKind::Weapon) &&
															   !InfusionLook::CanWear(EInfusionLookKind::None, EInfusionLookKind::None));
	});

	It("puts rings on fingers in loadout slot order, index finger first, weapons not counted", [this]()
	{
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		TArray<FLoadoutSlot> Slots = {LookTestSlot(Sword, EGearKind::Weapon), LookTestSlot(NewObject<URingData>(GetTransientPackage()), EGearKind::Adapter),
									  LookTestSlot(nullptr, EGearKind::Adapter), LookTestSlot(NewObject<URingData>(GetTransientPackage()), EGearKind::Adapter)};
		TestEqual(TEXT("the first ring (after a weapon) is the index finger"), InfusionLook::RingFingerIndex(Slots, &Slots[1]), 0);
		TestEqual(TEXT("the next ring (past an empty slot) is the middle finger"), InfusionLook::RingFingerIndex(Slots, &Slots[3]), 1);
		TestEqual(TEXT("a weapon has no finger"), InfusionLook::RingFingerIndex(Slots, &Slots[0]), static_cast<int32>(INDEX_NONE));

		const TArray<FName> Hand = GetDefault<UImpactVisualDefaults>()->RingFingerSockets;
		TestEqual(TEXT("four fingers by default"), Hand.Num(), 4);
		TestEqual(TEXT("the right index finger first"), InfusionLook::FingerSocket(Hand, 0), FName(TEXT("index_01_r")));
		TestEqual(TEXT("a fifth ring shares the little finger"), InfusionLook::FingerSocket(Hand, 4), FName(TEXT("pinky_01_r")));
		TestTrue(TEXT("no finger, no socket"), InfusionLook::FingerSocket(Hand, INDEX_NONE).IsNone());
	});

	It("shows each stacked kind once, in stack order", [this]()
	{
		const TArray<EInfusionLookKind> Kinds = InfusionLook::KindsForStack(
			{EInfusionSourceOption::Innate, EInfusionSourceOption::ActiveRing, EInfusionSourceOption::WeaponCrystal, EInfusionSourceOption::Evolution});
		TestTrue(TEXT("aura, ring, weapon (two auras show one)"),
				 Kinds == TArray<EInfusionLookKind>({EInfusionLookKind::Aura, EInfusionLookKind::Ring, EInfusionLookKind::Weapon}));
	});

	It("shows the weapon glow and the aura for a Fire gem stacked with a Fire innate, and only the aura for a Water gem", [this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Caster;
		Data->InnateElement = ItemIdentity::GetElement(LOOK_TEST_FIRE_GEM);
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data, NewObject<UWeaponData>(GetTransientPackage()));
		AActor *Hero = Loadout ? Loadout->GetOwner() : nullptr;
		UInventoryComponent *Inventory = Hero ? Hero->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!TestNotNull(TEXT("hero"), Hero) || !TestTrue(TEXT("a loadout"), Inventory && Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)))
		{
			return;
		}
		UInfusionVFXComponent *VFX = NewObject<UInfusionVFXComponent>(Hero);
		Hero->AddInstanceComponent(VFX);
		VFX->RegisterComponent();
		FRuntimeAttachedItem Gem;
		Gem.Kind = EAttachedItemKind::Crystal;
		Gem.Crystal = FCrystalAttachment(LOOK_TEST_FIRE_GEM, LOOK_TEST_INTACT);
		FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		Active.PrimaryWeapon.WeaponEntry.AttachedItem = Gem;

		TestTrue(TEXT("Fire gem picked: weapon glow, then the innate's aura"),
				 VFX->GetLookKindsFor(EInfusionSourceOption::WeaponCrystal, LOOK_TEST_L1) == TArray<EInfusionLookKind>({EInfusionLookKind::Weapon, EInfusionLookKind::Aura}));
		TestTrue(TEXT("innate picked: aura, then the gem's weapon glow"),
				 VFX->GetLookKindsFor(EInfusionSourceOption::Innate, LOOK_TEST_L1) == TArray<EInfusionLookKind>({EInfusionLookKind::Aura, EInfusionLookKind::Weapon}));

		Gem.Crystal = FCrystalAttachment(LOOK_TEST_WATER_GEM, LOOK_TEST_INTACT);
		Active.PrimaryWeapon.WeaponEntry.AttachedItem = Gem;
		TestTrue(TEXT("a Water gem doesn't join a Fire innate: the aura alone"),
				 VFX->GetLookKindsFor(EInfusionSourceOption::Innate, LOOK_TEST_L1) == TArray<EInfusionLookKind>({EInfusionLookKind::Aura}));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
