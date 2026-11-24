// QAKitSpec.cpp
// wor.Debug.QAKit, headless: which ability shows an infusion HP cost on a weapon (costs EP, not the attack, the weapon's
// type and wield mode), which Caster joins (of the element asked, or any with an element of its own, never one already
// in the party), a brand-new character's learn (no skills kept yet: banked first, then learned in the store and on the
// member), and with no played character the kit says so.
// Unproven here: the skill put in a real slot and the Caster joining a real party (the brain's live check).

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimMontage.h"
#include "Character/CharacterData.h"
#include "Debug/Inventory/QAKitDebug.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Misc/AutomationTest.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FQAKitSpec, "WoR.Debug.QAKit", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FQAKitSpec)

void FQAKitSpec::Define()
{
	It("takes an ability that costs EP, has a SkillMontage and fits the weapon's type and wield mode", [this]()
	{
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->WeaponType = EWeaponType::Sword;
		Sword->WieldMode = EWeaponWieldMode::Single;
		UAnimMontage *Swing = NewObject<UAnimMontage>(GetTransientPackage());
		const auto MakeAbility = [Swing](int32 EnergyCost, EWeaponType Type, EWeaponWieldMode Wield, bool bAttack)
		{
			UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
			Ability->BaseEnergyCost = EnergyCost;
			Ability->RequiredWeaponType = Type;
			Ability->RequiredWieldMode = Wield;
			Ability->bIsAttack = bAttack;
			Ability->SkillMontage = Swing;
			return Ability;
		};
		UAbilityData *NoMontage = MakeAbility(5, EWeaponType::Sword, EWeaponWieldMode::Single, false);
		NoMontage->SkillMontage = nullptr;
		TestFalse(TEXT("one with no SkillMontage can't play in a fight"), WoRQAKit::IsHPCostSkillFor(NoMontage, Sword));
		TestTrue(TEXT("a sword skill that costs EP"), WoRQAKit::IsHPCostSkillFor(MakeAbility(5, EWeaponType::Sword, EWeaponWieldMode::Single, false), Sword));
		TestFalse(TEXT("a free one shows no HP cost"), WoRQAKit::IsHPCostSkillFor(MakeAbility(0, EWeaponType::Sword, EWeaponWieldMode::Single, false), Sword));
		TestFalse(TEXT("another weapon type's"), WoRQAKit::IsHPCostSkillFor(MakeAbility(5, EWeaponType::Bow, EWeaponWieldMode::Single, false), Sword));
		TestFalse(TEXT("a dual wield one on a single sword"), WoRQAKit::IsHPCostSkillFor(MakeAbility(5, EWeaponType::Sword, EWeaponWieldMode::Dual, false), Sword));
		TestFalse(TEXT("the weapon's attack"), WoRQAKit::IsHPCostSkillFor(MakeAbility(5, EWeaponType::Sword, EWeaponWieldMode::Single, true), Sword));
		TestFalse(TEXT("no weapon"), WoRQAKit::IsHPCostSkillFor(MakeAbility(5, EWeaponType::Sword, EWeaponWieldMode::Single, false), nullptr));
	});

	It("picks a Caster of the element asked, or any with its own element, never one in the party", [this]()
	{
		const auto MakeData = [](ECharacterClass Class, ESpellElement Element)
		{
			UCharacterData *Data = NewObject<UCharacterData>(GetTransientPackage());
			Data->CharacterClass = Class;
			Data->InnateElement = Element;
			return Data;
		};
		UCharacterData *Generic = MakeData(ECharacterClass::Generic, ESpellElement::Generic);
		UCharacterData *Inheriting = MakeData(ECharacterClass::Caster, ESpellElement::Generic);
		UCharacterData *Fire = MakeData(ECharacterClass::Caster, ESpellElement::Fire);
		UCharacterData *Darkness = MakeData(ECharacterClass::Caster, ESpellElement::Darkness);
		const TArray<UCharacterData *> Candidates = {Generic, Inheriting, Fire, Darkness};
		TestEqual(TEXT("any element: the first Caster with its own"), WoRQAKit::PickCaster(Candidates, ESpellElement::None, {}), Fire);
		TestEqual(TEXT("Darkness asked"), WoRQAKit::PickCaster(Candidates, ESpellElement::Darkness, {}), Darkness);
		TestEqual(TEXT("one in the party is skipped"), WoRQAKit::PickCaster(Candidates, ESpellElement::None, {Fire}), Darkness);
		TestNull(TEXT("no Caster of that element"), WoRQAKit::PickCaster(Candidates, ESpellElement::Light, {}));
	});

	It("learns for a brand-new character whose skills aren't kept yet: banks what it knows, then learns there and on the member", [this]()
	{
		FWoRTestWorld TestWorld;
		if (!TestTrue(TEXT("test world created"), TestWorld.Create())) return;
		UPoolSubsystem *Pool = TestWorld.GetSubsystem<UPoolSubsystem>();
		AActor *Member = TestWorld.SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UInventoryComponent *Inventory = Member ? FWoRTestWorld::AddComponent<UInventoryComponent>(Member) : nullptr;
		if (!TestNotNull(TEXT("pool"), Pool) || !TestNotNull(TEXT("inventory"), Inventory)) return;
		const FName Key(TEXT("QAKitNewCharacter"));
		UAbilityData *Known = NewObject<UAbilityData>(GetTransientPackage()); // from its asset
		UAbilityData *QuickStrike = NewObject<UAbilityData>(GetTransientPackage());
		Inventory->Abilities.LearnAbility(Known);
		if (!TestFalse(TEXT("nothing kept yet"), Pool->HasBankedSkills(Key))) return;

		TestTrue(TEXT("learned"), WoRQAKit::LearnForKit(*Pool, Key, *Inventory, QuickStrike));
		TestTrue(TEXT("its asset's skill is kept"), Pool->KnowsAbility(Key, Known));
		TestTrue(TEXT("the new one is kept"), Pool->KnowsAbility(Key, QuickStrike));
		TestTrue(TEXT("and on the member, for the skills page"), Inventory->HasAbility(QuickStrike));
		TestFalse(TEXT("a second time: already known"), WoRQAKit::LearnForKit(*Pool, Key, *Inventory, QuickStrike));
	});

	It("says so when there is no played character", [this]()
	{
		FWoRTestWorld TestWorld;
		TestTrue(TEXT("test world created"), TestWorld.Create());
		TestTrue(TEXT("no played character"), WoRQAKit::RunQAKit(TestWorld.GetWorld(), {}).Contains(TEXT("no played character")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
