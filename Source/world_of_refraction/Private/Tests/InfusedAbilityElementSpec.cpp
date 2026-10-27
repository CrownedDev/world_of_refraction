// InfusedAbilityElementSpec.cpp
// B-080 (D-007): an ability starts Physical (no element) and an infusion gives it the selected
// source's element: a Generic swinging through a fire (Garnet) weapon crystal deals Fire.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"

namespace
{
	constexpr int32 INFUSED_ELEMENT_TEST_DURABILITY = 10;
	constexpr int32 INFUSED_ELEMENT_TEST_BASE_DAMAGE = 100;

	template <class TComponent>
	TComponent *InfusedElementAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}
}

BEGIN_DEFINE_SPEC(FInfusedAbilityElementSpec, "WoR.Combat.InfusedAbilityElement",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
UActionExecutor *Executor = nullptr;
AActor *User = nullptr;

/** A combatant of the class and innate element, whose primary weapon holds a fire (Garnet) crystal. */
void SpawnWithFireWeaponCrystal(ECharacterClass CharacterClass, ESpellElement InnateElement)
{
	UCharacterData *Data = FWoRTestWorld::NewCharacterData();
	Data->CharacterClass = CharacterClass;
	Data->InnateElement = InnateElement;
	User = TestWorld->SpawnCombatant(Data);
	UInventoryComponent *Inventory = InfusedElementAddComponent<UInventoryComponent>(User);
	ULoadoutComponent *Loadout = InfusedElementAddComponent<ULoadoutComponent>(User);
	Loadout->InitializeFromCharacterData(Data, Inventory);
	FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	Active.PrimarySlotType = EPrimarySlotType::Weapon;
	Active.PrimaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
	FRuntimeAttachedItem Crystal;
	Crystal.Kind = EAttachedItemKind::Crystal;
	Crystal.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier), INFUSED_ELEMENT_TEST_DURABILITY);
	Active.PrimaryWeapon.WeaponEntry.AttachedItem = Crystal;
}

ESpellElement PreviewAbilityElement(EInfusionSourceOption Source) const
{
	FAction Swing;
	Swing.ActionType = EActionType::Ability;
	UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
	Ability->BaseDamage = INFUSED_ELEMENT_TEST_BASE_DAMAGE;
	Swing.SkillData = Ability;
	Swing.SelectedSource = Source;
	return Executor->PreviewActionDamage(User, Swing, FActionStatModifiers()).Element;
}
END_DEFINE_SPEC(FInfusedAbilityElementSpec)

void FInfusedAbilityElementSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		Executor = nullptr;
		User = nullptr;
	});

	It("a Generic infused through a fire weapon crystal deals Fire", [this]()
	{
		SpawnWithFireWeaponCrystal(ECharacterClass::Generic, ESpellElement::Generic);
		if (!TestNotNull(TEXT("executor"), Executor) || !TestNotNull(TEXT("user"), User))
		{
			return;
		}
		TestEqual(TEXT("crystal's element"), PreviewAbilityElement(EInfusionSourceOption::WeaponCrystal),
				  ESpellElement::Fire);
	});

	It("uninfused, the same ability deals Physical (no element)", [this]()
	{
		SpawnWithFireWeaponCrystal(ECharacterClass::Generic, ESpellElement::Generic);
		if (!TestNotNull(TEXT("executor"), Executor) || !TestNotNull(TEXT("user"), User))
		{
			return;
		}
		TestEqual(TEXT("uninfused"), PreviewAbilityElement(EInfusionSourceOption::None), ESpellElement::None);
		TestEqual(TEXT("Raw (elementless infusion)"), PreviewAbilityElement(EInfusionSourceOption::Raw),
				  ESpellElement::None);
	});

	It("an infusion through the innate source deals the innate element, not the crystal's", [this]()
	{
		SpawnWithFireWeaponCrystal(ECharacterClass::Caster, ESpellElement::Water);
		if (!TestNotNull(TEXT("executor"), Executor) || !TestNotNull(TEXT("user"), User))
		{
			return;
		}
		TestEqual(TEXT("innate"), PreviewAbilityElement(EInfusionSourceOption::Innate), ESpellElement::Water);
		TestEqual(TEXT("crystal"), PreviewAbilityElement(EInfusionSourceOption::WeaponCrystal), ESpellElement::Fire);
	});

	It("a Generic's innate source (no element) stays Physical", [this]()
	{
		SpawnWithFireWeaponCrystal(ECharacterClass::Generic, ESpellElement::Generic);
		if (!TestNotNull(TEXT("executor"), Executor) || !TestNotNull(TEXT("user"), User))
		{
			return;
		}
		TestEqual(TEXT("never Generic"), PreviewAbilityElement(EInfusionSourceOption::Innate), ESpellElement::None);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
