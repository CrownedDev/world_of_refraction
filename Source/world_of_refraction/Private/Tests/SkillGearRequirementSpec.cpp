// SkillGearRequirementSpec.cpp
// B-520 (D-224, AssetOverhaulPlan §5.2): a skill names the gear it needs; SkillRequirementRules::IsMet checks it
// against the piece the skill sits in. Unauthored skills read the old fields (an ability's RequiredWeaponType, a
// spell's RequiredEvolutionCrystal), so nothing changes until the editor pass sets them.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Loadout/SkillRequirementRules.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "UI/Menus/LoadoutSkillsModel.h"

namespace
{
	constexpr int32 GEAR_NEED_TEST_DURABILITY = 10;

	FSkillGearHost WeaponHost(EWeaponType Type)
	{
		FWeaponInventoryEntry Entry;
		Entry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Entry.Weapon->WeaponType = Type;
		return FSkillGearHost::FromWeapon(Entry);
	}

	FSkillGearHost RingHost(EAdapterForm Form, const FRuntimeAttachedItem &Socket)
	{
		FRingInventoryEntry Entry;
		Entry.Ring = NewObject<URingData>(GetTransientPackage());
		Entry.Ring->Form = Form;
		Entry.AttachedItem = Socket;
		return FSkillGearHost::FromAdapter(Entry);
	}

	FRuntimeAttachedItem Garnet()
	{
		FRuntimeAttachedItem Gem;
		Gem.Kind = EAttachedItemKind::Crystal;
		Gem.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier), GEAR_NEED_TEST_DURABILITY);
		return Gem;
	}

	FRuntimeAttachedItem Evolved(UEvolutionItemData *Evolution)
	{
		FRuntimeAttachedItem Socket;
		Socket.Kind = EAttachedItemKind::Evolution;
		Socket.Evolution.Item = Evolution;
		return Socket;
	}

	UAbilityData *AbilityFor(EWeaponType Type)
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->RequiredWeaponType = Type;
		return Ability;
	}
}

BEGIN_DEFINE_SPEC(FSkillGearRequirementSpec, "WoR.Loadout.SkillGearRequirement",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FSkillGearRequirementSpec)

void FSkillGearRequirementSpec::Define()
{
	using namespace SkillRequirementRules;

	It("reads an unauthored ability's RequiredWeaponType: an axe skill needs an axe", [this]()
	{
		UAbilityData *Chop = AbilityFor(EWeaponType::Axe);
		TestTrue(TEXT("axe: met"), IsMet(Chop, WeaponHost(EWeaponType::Axe)));
		TestFalse(TEXT("sword: not met"), IsMet(Chop, WeaponHost(EWeaponType::Sword)));
		TestFalse(TEXT("an adapter: not met"), IsMet(Chop, RingHost(EAdapterForm::Ring, Garnet())));
		TestFalse(TEXT("no skill: not met"), IsMet(nullptr, WeaponHost(EWeaponType::Axe)));
	});

	It("lets an authored requirement name any weapon, a type, or an element", [this]()
	{
		UAbilityData *AnyWeapon = AbilityFor(EWeaponType::Sword);
		AnyWeapon->GearRequirement.Kind = ESkillGearKind::Weapon;
		TestTrue(TEXT("any weapon: an axe"), IsMet(AnyWeapon, WeaponHost(EWeaponType::Axe)));
		TestTrue(TEXT("any weapon: a spear"), IsMet(AnyWeapon, WeaponHost(EWeaponType::Spear)));

		AnyWeapon->GearRequirement.bNeedsWeaponType = true;
		AnyWeapon->GearRequirement.WeaponType = EWeaponType::Dagger;
		TestFalse(TEXT("authored Dagger overrides the old Sword: a sword is not met"), IsMet(AnyWeapon, WeaponHost(EWeaponType::Sword)));
		TestTrue(TEXT("a dagger is met"), IsMet(AnyWeapon, WeaponHost(EWeaponType::Dagger)));

		USpellData *FireBolt = NewObject<USpellData>(GetTransientPackage());
		FireBolt->GearRequirement.Kind = ESkillGearKind::Adapter;
		FireBolt->GearRequirement.Element = ESpellElement::Fire;
		TestTrue(TEXT("a Garnet ring: met"), IsMet(FireBolt, RingHost(EAdapterForm::Ring, Garnet())));
		TestFalse(TEXT("an empty ring: not met"), IsMet(FireBolt, RingHost(EAdapterForm::Ring, FRuntimeAttachedItem())));
	});

	It("gates a spell on an adapter, its form, or an evolution", [this]()
	{
		USpellData *Plain = NewObject<USpellData>(GetTransientPackage());
		TestTrue(TEXT("nothing set: a ring"), IsMet(Plain, RingHost(EAdapterForm::Ring, Garnet())));
		TestTrue(TEXT("nothing set: any spell source"), IsMet(Plain, FSkillGearHost::FromEvolutionSeat(NewObject<UEvolutionItemData>(GetTransientPackage()))));

		USpellData *Bolt = NewObject<USpellData>(GetTransientPackage());
		Bolt->GearRequirement.Kind = ESkillGearKind::Adapter;
		TestTrue(TEXT("adapter: a ring"), IsMet(Bolt, RingHost(EAdapterForm::Ring, Garnet())));
		TestFalse(TEXT("adapter: not the evolution seat"),
				  IsMet(Bolt, FSkillGearHost::FromEvolutionSeat(NewObject<UEvolutionItemData>(GetTransientPackage()))));
		Bolt->GearRequirement.bNeedsForm = true;
		Bolt->GearRequirement.Form = EAdapterForm::Pistol;
		TestFalse(TEXT("a pistol's spell: not a ring"), IsMet(Bolt, RingHost(EAdapterForm::Ring, Garnet())));
		TestTrue(TEXT("a pistol's spell: a pistol"), IsMet(Bolt, RingHost(EAdapterForm::Pistol, Garnet())));

		UEvolutionItemData *Phoenix = NewObject<UEvolutionItemData>(GetTransientPackage());
		UEvolutionItemData *Gale = NewObject<UEvolutionItemData>(GetTransientPackage());
		USpellData *Rebirth = NewObject<USpellData>(GetTransientPackage());
		Rebirth->RequiredEvolutionCrystal = Phoenix; // the old field, read until the pass
		TestTrue(TEXT("Phoenix in the seat"), IsMet(Rebirth, FSkillGearHost::FromEvolutionSeat(Phoenix)));
		TestTrue(TEXT("Phoenix in a ring"), IsMet(Rebirth, RingHost(EAdapterForm::Ring, Evolved(Phoenix))));
		TestFalse(TEXT("another evolution"), IsMet(Rebirth, FSkillGearHost::FromEvolutionSeat(Gale)));
		TestFalse(TEXT("a plain Garnet ring"), IsMet(Rebirth, RingHost(EAdapterForm::Ring, Garnet())));
		TestFalse(TEXT("need text says what"), GetNeedText(Rebirth).IsEmpty());
	});

	It("offers on a weapon's Skills tab only the skills it meets", [this]()
	{
		FLoadoutSkillsPage Page;
		FLoadoutSkillTab &Tab = Page.Tabs.AddDefaulted_GetRef();
		Tab.List = ELoadoutSkillList::WeaponSkills;
		Tab.Host = WeaponHost(EWeaponType::Sword);
		Tab.Rows.AddDefaulted();
		UAbilityData *Slash = AbilityFor(EWeaponType::Sword);
		UAbilityData *Chop = AbilityFor(EWeaponType::Axe);
		const TArray<FLoadoutScreenChoice> Choices =
			WoRLoadoutSkills::BuildSkillChoices(Page, 0, Tab.Rows[0], {Slash, Chop}, {}, {});
		TestEqual(TEXT("one choice"), Choices.Num(), 1);
		TestTrue(TEXT("the sword skill"), Choices.Num() == 1 && Choices[0].Row.Skill == Slash);
	});
}

#endif
