// EnemyFillSpec.cpp
// Q-518 (B-518): gear has no built-ins (D-224), so until stage 5 an enemy's piece with no picks fills from the
// enemy's known skills, in order, up to its slot count: a weapon with the abilities it can hold, a ring with a spell
// source with the spells of its element that aren't innate. A piece that has picks keeps them.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/FCrystalAttachment.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/FCombatLoadout.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr int32 ENEMY_FILL_C_SLOTS = 4; // CL-20: a C weapon's tier slots
	constexpr int32 ENEMY_FILL_KNOWN = 6;
	constexpr int32 ENEMY_FILL_DURABILITY = 5;
	const FCrystalId ENEMY_FILL_GEM(ECrystalType::Garnet, EItemTier::E_Tier);
}

BEGIN_DEFINE_SPEC(FEnemyFillSpec, "WoR.Loadout.EnemyFill", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	FCombatLoadout Loadout;
	TArray<UAbilityData *> KnownAbilities;
	TArray<USpellData *> KnownSpells;

	static UAbilityData *Ability(EWeaponType Type)
	{
		UAbilityData *Made = NewObject<UAbilityData>(GetTransientPackage());
		Made->RequiredWeaponType = Type;
		return Made;
	}

	static USpellData *Spell(ESpellElement Element)
	{
		USpellData *Made = NewObject<USpellData>(GetTransientPackage());
		Made->Element = Element;
		return Made;
	}
END_DEFINE_SPEC(FEnemyFillSpec)

void FEnemyFillSpec::Define()
{
	BeforeEach([this]()
	{
		Loadout = FCombatLoadout();
		Loadout.PrimaryWeapon.WeaponEntry.Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Loadout.PrimaryWeapon.WeaponEntry.Weapon->WeaponType = EWeaponType::Sword;
		Loadout.PrimaryWeapon.WeaponEntry.Tier = EItemTier::C_Tier;
		KnownAbilities = {Ability(EWeaponType::Axe)};
		for (int32 Index = 0; Index < ENEMY_FILL_KNOWN; ++Index)
		{
			KnownAbilities.Add(Ability(EWeaponType::Sword));
		}
		FRingLoadoutEntry &Ring = Loadout.RingLoadout.AddDefaulted_GetRef();
		Ring.RingEntry.Ring = NewObject<URingData>(GetTransientPackage());
		Ring.RingEntry.AttachedItem.Kind = EAttachedItemKind::Crystal;
		Ring.RingEntry.AttachedItem.Crystal = FCrystalAttachment(ENEMY_FILL_GEM, ENEMY_FILL_DURABILITY);
		const ESpellElement RingElement = Ring.RingEntry.GetElement();
		const ESpellElement OtherElement = RingElement == ESpellElement::Water ? ESpellElement::Fire : ESpellElement::Water;
		KnownSpells = {Spell(OtherElement)};
		for (int32 Index = 0; Index < ENEMY_FILL_KNOWN; ++Index)
		{
			KnownSpells.Add(Spell(RingElement));
		}
	});

	It("starts with empty slots: gear has no built-ins", [this]()
	{
		TestEqual(TEXT("no weapon skills"), Loadout.PrimaryWeapon.GetSkillSlotAbilities().Num(), 0);
		TestEqual(TEXT("no ring spells"), Loadout.RingLoadout[0].GetAllSpells().Num(), 0);
	});

	It("fills an empty weapon with the known abilities it can hold, up to its slots", [this]()
	{
		Loadout.FillEmptyPicksFromKnown(KnownAbilities, KnownSpells);
		const TArray<UAbilityData *> Skills = Loadout.PrimaryWeapon.GetSkillSlotAbilities();
		TestEqual(TEXT("the C weapon's slots"), Skills.Num(), ENEMY_FILL_C_SLOTS);
		TestFalse(TEXT("not the axe ability"), Skills.Contains(KnownAbilities[0]));
		TestTrue(TEXT("in order"), Skills.IsValidIndex(0) && Skills[0] == KnownAbilities[1]);
	});

	It("fills an empty ring with known spells of its element, not the innate ones, up to its cap", [this]()
	{
		Loadout.InnateSpells = {KnownSpells[1]};
		Loadout.FillEmptyPicksFromKnown(KnownAbilities, KnownSpells);
		const FRingLoadoutEntry &Ring = Loadout.RingLoadout[0];
		const TArray<USpellData *> Spells = Ring.GetAllSpells();
		TestEqual(TEXT("the ring's cap"), Spells.Num(), FMath::Min(Ring.GetSpellSlotCap(), ENEMY_FILL_KNOWN - 1));
		TestFalse(TEXT("not the other element"), Spells.Contains(KnownSpells[0]));
		TestFalse(TEXT("not the innate spell"), Spells.Contains(KnownSpells[1]));
	});

	It("keeps a piece's own picks", [this]()
	{
		UAbilityData *Own = Ability(EWeaponType::Sword);
		Loadout.PrimaryWeapon.ReplaceSkill(0, Own);
		Loadout.FillEmptyPicksFromKnown(KnownAbilities, KnownSpells);
		const TArray<UAbilityData *> Skills = Loadout.PrimaryWeapon.GetSkillSlotAbilities();
		TestEqual(TEXT("only the own pick"), Skills.Num(), 1);
		TestTrue(TEXT("the own pick"), Skills.Contains(Own));
	});
}

#endif
