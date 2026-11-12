// WeaponsNoSpellsSpec.cpp
// B-098 (D-027, Q-098): weapons never give spells. A weapon crystal's spells (built-in, assigned or picked) are not
// listed, castable, editable or offered in the menu; the weapon's abilities still are. The spells stay in the data,
// so wor.Loadout.WeaponSpells 1 (the old path, until the sweep) lists them again.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/FCrystalAttachment.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/WeaponSpellRules.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "UI/Combat/FCombatCapabilities.h"

namespace
{
	/** An E-tier gem: two spell slots (Q-147). */
	const FCrystalId WEAPONS_NO_SPELLS_TEST_GEM(ECrystalType::Garnet, EItemTier::E_Tier);
	constexpr int32 WEAPONS_NO_SPELLS_TEST_DURABILITY = 5;
	constexpr int32 WEAPONS_NO_SPELLS_TEST_FIRST = 0;

	FRuntimeAttachedItem WeaponsNoSpellsTestGem()
	{
		FRuntimeAttachedItem Socket;
		Socket.Kind = EAttachedItemKind::Crystal;
		Socket.Crystal = FCrystalAttachment(WEAPONS_NO_SPELLS_TEST_GEM, WEAPONS_NO_SPELLS_TEST_DURABILITY);
		return Socket;
	}
}

BEGIN_DEFINE_SPEC(FWeaponsNoSpellsSpec, "WoR.Loadout.WeaponsNoSpells", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	TUniquePtr<WeaponSpellRules::FScopedWeaponSpells> Rule;
	USpellData *CrystalSpell = nullptr;
END_DEFINE_SPEC(FWeaponsNoSpellsSpec)

void FWeaponsNoSpellsSpec::Define()
{
	BeforeEach([this]()
	{
		Rule = MakeUnique<WeaponSpellRules::FScopedWeaponSpells>(false); // the D-027 default, whatever the console says
		CrystalSpell = NewObject<USpellData>(GetTransientPackage());
	});
	AfterEach([this]()
	{
		CrystalSpell = nullptr;
		Rule.Reset();
	});

	It("a weapon crystal lists, slots and takes no spells; the data stays", [this]()
	{
		FWeaponLoadoutEntry Weapon;
		Weapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon.WeaponEntry.AttachedItem = WeaponsNoSpellsTestGem();
		Weapon.WeaponEntry.AssignedSpells = {CrystalSpell};
		Weapon.AssignedSpells = {CrystalSpell};

		TestFalse(TEXT("off by default"), WeaponSpellRules::WeaponsGiveSpells());
		TestEqual(TEXT("no spells"), Weapon.GetAllSpells().Num(), 0);
		TestEqual(TEXT("no spell slots"), Weapon.GetSpellSlots().Num(), 0);
		TestFalse(TEXT("no pick"), Weapon.ReplaceSpell(WEAPONS_NO_SPELLS_TEST_FIRST, CrystalSpell));
		TestTrue(TEXT("the saved data is untouched"), Weapon.WeaponEntry.AssignedSpells.Contains(CrystalSpell));

		WeaponSpellRules::FScopedWeaponSpells OldPath(true);
		TestTrue(TEXT("the old path lists them again"), Weapon.GetAllSpells().Contains(CrystalSpell));
	});

	Describe("in a loadout", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("world"), TestWorld->Create());
		});
		AfterEach([this]() { TestWorld.Reset(); });

		It("offers the weapon's abilities but not its crystal's spells, for every class", [this]()
		{
			for (const ECharacterClass Class : {ECharacterClass::Generic, ECharacterClass::Caster, ECharacterClass::Resonator})
			{
				const FString ClassName = UEnum::GetValueAsString(Class);
				UCharacterData *Data = FWoRTestWorld::NewCharacterData();
				Data->CharacterClass = Class;
				UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
				Sword->Name = TEXT("Weapons No Spells Test Sword");
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
				Active.PrimaryWeapon.WeaponEntry.AttachedItem = WeaponsNoSpellsTestGem();
				Active.PrimaryWeapon.WeaponEntry.AssignedSpells = {CrystalSpell};
				Active.PrimaryWeapon.AssignedAbilities = {Ability};

				TestFalse(ClassName + TEXT(": not castable"), Loadout->GetAvailableSpells().Contains(CrystalSpell));
				TestEqual(ClassName + TEXT(": no resonate spells"), Loadout->GetWeaponResonateSpells().Num(), 0);
				const FCombatCapabilities Caps = FCombatCapabilities::BuildFrom(Loadout, Class, FLinearColor::White, [](int32) { return FLinearColor::White; });
				TestFalse(ClassName + TEXT(": no weapon spell menu"), Caps.bHasWeaponCrystal);
				TestEqual(ClassName + TEXT(": no weapon crystal spells"), Caps.WeaponCrystalSpells.Num(), 0);
				TestTrue(ClassName + TEXT(": the weapon's ability is offered"), Caps.WeaponAbilities.Contains(Ability));
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
