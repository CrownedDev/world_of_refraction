// InfusionResolverSpec.cpp
// B-307 (D-110 unified loadout, step 7; SimplifyAudit S4): one infusion-source resolver. Both adapter options read the
// adapter in use; a broken attachment pushes no element on every path (CL-24) and a broken adapter is no source
// (Q-163 = B); Raw splits by the weapon in hand's mode.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

namespace
{
	const FCrystalId INFUSION_RESOLVER_TEST_GEM(ECrystalType::Garnet, EItemTier::F_Tier);
	constexpr int32 INFUSION_RESOLVER_TEST_INTACT = 5;
	constexpr int32 INFUSION_RESOLVER_TEST_BROKEN = 0;

	FRuntimeAttachedItem InfusionResolverTestGem(int32 Durability)
	{
		FRuntimeAttachedItem Attached;
		Attached.Kind = EAttachedItemKind::Crystal;
		Attached.Crystal = FCrystalAttachment(INFUSION_RESOLVER_TEST_GEM, Durability);
		return Attached;
	}
}

BEGIN_DEFINE_SPEC(FInfusionResolverSpec, "WoR.Infusion.SourceResolver", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UActionExecutor *Executor = nullptr;

	static FCombatLoadout &ActiveOf(ULoadoutComponent *Loadout)
	{
		UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}
END_DEFINE_SPEC(FInfusionResolverSpec)

void FInfusionResolverSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
	});

	AfterEach([this]()
	{
		Executor = nullptr;
		TestWorld.Reset();
	});

	It("gives a broken weapon crystal no element, and a Raw infusion the weapon's mode (CL-24)", [this]()
	{
		if (!TestNotNull(TEXT("Executor"), Executor)) return;
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->InfusionMode = EInfusionMode::Physical;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(), Weapon);
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		AActor *Actor = Loadout->GetOwner();
		ActiveOf(Loadout).PrimaryWeapon.WeaponEntry.AttachedItem = InfusionResolverTestGem(INFUSION_RESOLVER_TEST_INTACT);
		TestTrue(TEXT("intact: the gem's element"),
				 Executor->ResolveInfusionSource(Actor, EInfusionSourceOption::WeaponCrystal).Element == ItemIdentity::GetElement(INFUSION_RESOLVER_TEST_GEM));
		ActiveOf(Loadout).PrimaryWeapon.WeaponEntry.AttachedItem = InfusionResolverTestGem(INFUSION_RESOLVER_TEST_BROKEN);
		TestTrue(TEXT("broken: no element"),
				 Executor->ResolveInfusionSource(Actor, EInfusionSourceOption::WeaponCrystal).Element == ESpellElement::None);
		TestTrue(TEXT("Raw splits by the weapon's mode"),
				 Executor->ResolveInfusionSource(Actor, EInfusionSourceOption::Raw).Mode == EInfusionMode::Physical);
	});

	It("reads the adapter in use through both adapter options, and refuses a broken one (Q-163 = B)", [this]()
	{
		if (!TestNotNull(TEXT("Executor"), Executor)) return;
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Resonator;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(Data);
		if (!TestNotNull(TEXT("Loadout"), Loadout)) return;
		AActor *Actor = Loadout->GetOwner();
		FRingLoadoutEntry Entry;
		Entry.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Entry.RingEntry.AttachedItem = InfusionResolverTestGem(INFUSION_RESOLVER_TEST_INTACT);
		ActiveOf(Loadout).RingLoadout = {Entry};
		ActiveOf(Loadout).ActiveRingIndex = 0;

		const FInfusionSourceView Active = Executor->ResolveInfusionSource(Actor, EInfusionSourceOption::ActiveRing);
		const FInfusionSourceView Primary = Executor->ResolveInfusionSource(Actor, EInfusionSourceOption::PrimaryRing);
		TestTrue(TEXT("usable"), Active.bUsable);
		TestTrue(TEXT("both options read the same adapter"), Active.Element == Primary.Element && Active.bUsable == Primary.bUsable);
		TestTrue(TEXT("offered"), Executor->GetAvailableInfusionSources(Actor).Contains(EInfusionSourceOption::ActiveRing));

		ActiveOf(Loadout).RingLoadout[0].RingEntry.AttachedItem = InfusionResolverTestGem(INFUSION_RESOLVER_TEST_BROKEN);
		const FInfusionSourceView Broken = Executor->ResolveInfusionSource(Actor, EInfusionSourceOption::ActiveRing);
		TestFalse(TEXT("a broken adapter is no source"), Broken.bUsable);
		TestTrue(TEXT("and pushes no element"), Broken.Element == ESpellElement::None);
		TestFalse(TEXT("not offered"), Executor->GetAvailableInfusionSources(Actor).Contains(EInfusionSourceOption::ActiveRing));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
