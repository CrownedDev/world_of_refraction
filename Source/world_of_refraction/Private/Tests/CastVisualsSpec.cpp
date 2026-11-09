// CastVisualsSpec.cpp
// B-394 (D-170, Q-394): a weapon's casts play its fixed CastVFX; an adapter's (and a Refractor's innate) spell plays
// its adapter type's default, Ring with no adapter; a Generic wearing both follows the class preference; a spell item
// plays its own; with nothing authored the skill's own visual plays (play unchanged).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Presentation/CastVisuals.h"
#include "Combat/Presentation/ImpactVisualDefaults.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "NiagaraSystem.h"

namespace
{
	TSoftObjectPtr<UNiagaraSystem> CastTestVFX(const TCHAR *Name)
	{
		return TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(FString::Printf(TEXT("/Game/Test/%s.%s"), Name, Name)));
	}

	UImpactVisualDefaults *CastTestDefaults(ECastVFXPreference Preference = ECastVFXPreference::Adapter)
	{
		UImpactVisualDefaults *Defaults = NewObject<UImpactVisualDefaults>(GetTransientPackage());
		Defaults->AdapterCastVFX.Add(EAdapterForm::Ring, CastTestVFX(TEXT("RingCast")));
		Defaults->AdapterCastVFX.Add(EAdapterForm::Pistol, CastTestVFX(TEXT("PistolCast")));
		Defaults->GenericCastVFXPreference = Preference;
		return Defaults;
	}

	UWeaponData *CastTestWeapon(bool bWithCastVFX = true)
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		if (bWithCastVFX)
		{
			Weapon->CastVFX = CastTestVFX(TEXT("SwordCast"));
		}
		return Weapon;
	}

	URingData *CastTestAdapter(EAdapterForm Form)
	{
		URingData *Adapter = NewObject<URingData>(GetTransientPackage());
		Adapter->Form = Form;
		return Adapter;
	}

	FCastVisualInputs SpellInputs(ECharacterClass Class, ESpellSource Source, const UWeaponData *Weapon, const URingData *Adapter)
	{
		FCastVisualInputs Inputs;
		Inputs.CharacterClass = Class;
		Inputs.bIsSpell = true;
		Inputs.SpellSource = Source;
		Inputs.Weapon = Weapon;
		Inputs.Adapter = Adapter;
		Inputs.SkillVFX = CastTestVFX(TEXT("SkillMuzzle"));
		return Inputs;
	}
}

BEGIN_DEFINE_SPEC(FCastVisualsSpec, "WoR.Presentation.CastVisuals",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCastVisualsSpec)

void FCastVisualsSpec::Define()
{
	using namespace CastVisuals;

	It("plays the weapon's fixed visual for its attacks, abilities and weapon-crystal spells", [this]()
	{
		UImpactVisualDefaults *Defaults = CastTestDefaults();
		UWeaponData *Sword = CastTestWeapon();

		FCastVisualInputs Ability = SpellInputs(ECharacterClass::Resonator, ESpellSource::Innate, Sword, nullptr);
		Ability.bIsSpell = false;
		const FCastVisualChoice FromAbility = Resolve(Ability, Defaults);
		TestTrue(TEXT("an ability plays the weapon's"), FromAbility.Source == ECastVFXSource::Weapon && FromAbility.VFX == CastTestVFX(TEXT("SwordCast")));

		const FCastVisualChoice FromCrystal = Resolve(SpellInputs(ECharacterClass::Caster, ESpellSource::WeaponCrystal, Sword, nullptr), Defaults);
		TestTrue(TEXT("a weapon-crystal spell plays the weapon's"), FromCrystal.Source == ECastVFXSource::Weapon && FromCrystal.VFX == CastTestVFX(TEXT("SwordCast")));
		TestFalse(TEXT("no fallback"), FromCrystal.bFellBackToSkill);
	});

	It("plays the adapter type's default for adapter spells, Ring without an adapter", [this]()
	{
		UImpactVisualDefaults *Defaults = CastTestDefaults();

		const FCastVisualChoice FromPistol = Resolve(
			SpellInputs(ECharacterClass::Resonator, ESpellSource::RingCrystal, CastTestWeapon(), CastTestAdapter(EAdapterForm::Pistol)), Defaults);
		TestTrue(TEXT("a Resonator's pistol adapter plays the pistol default"),
				 FromPistol.Source == ECastVFXSource::AdapterDefault && FromPistol.AdapterForm == EAdapterForm::Pistol && FromPistol.VFX == CastTestVFX(TEXT("PistolCast")));

		const FCastVisualChoice FromInnate = Resolve(SpellInputs(ECharacterClass::Caster, ESpellSource::Innate, nullptr, nullptr), Defaults);
		TestTrue(TEXT("a Refractor's innate spell plays the ring default"),
				 FromInnate.Source == ECastVFXSource::AdapterDefault && FromInnate.AdapterForm == EAdapterForm::Ring && FromInnate.VFX == CastTestVFX(TEXT("RingCast")));
	});

	It("keeps the skill's own visual for a non-Refractor's innate spell with no adapter (enemies)", [this]()
	{
		const FCastVisualChoice Enemy = Resolve(SpellInputs(ECharacterClass::Generic, ESpellSource::Innate, nullptr, nullptr), CastTestDefaults());
		TestTrue(TEXT("skill's own visual"), Enemy.Source == ECastVFXSource::Skill);
	});

	It("lets the class preference decide for a Generic wearing a weapon and an adapter", [this]()
	{
		UWeaponData *Sword = CastTestWeapon();
		URingData *Ring = CastTestAdapter(EAdapterForm::Ring);

		const FCastVisualInputs WeaponSpell = SpellInputs(ECharacterClass::Generic, ESpellSource::WeaponCrystal, Sword, Ring);
		const FCastVisualChoice PrefersAdapter = Resolve(WeaponSpell, CastTestDefaults(ECastVFXPreference::Adapter));
		TestTrue(TEXT("preference Adapter: even a weapon-crystal spell plays the ring"),
				 PrefersAdapter.Source == ECastVFXSource::AdapterDefault && PrefersAdapter.VFX == CastTestVFX(TEXT("RingCast")));

		const FCastVisualInputs AdapterSpell = SpellInputs(ECharacterClass::Generic, ESpellSource::RingCrystal, Sword, Ring);
		const FCastVisualChoice PrefersWeapon = Resolve(AdapterSpell, CastTestDefaults(ECastVFXPreference::Weapon));
		TestTrue(TEXT("preference Weapon: even an adapter spell plays the weapon's"),
				 PrefersWeapon.Source == ECastVFXSource::Weapon && PrefersWeapon.VFX == CastTestVFX(TEXT("SwordCast")));

		const FCastVisualChoice NoAsset = Resolve(AdapterSpell, nullptr);
		TestTrue(TEXT("no defaults asset: the Adapter preference"), NoAsset.Source == ECastVFXSource::AdapterDefault);

		const FCastVisualChoice NotGeneric = Resolve(SpellInputs(ECharacterClass::Caster, ESpellSource::WeaponCrystal, Sword, Ring),
													 CastTestDefaults(ECastVFXPreference::Adapter));
		TestTrue(TEXT("a Refractor's weapon-crystal spell stays the weapon's"), NotGeneric.Source == ECastVFXSource::Weapon);
	});

	It("plays the skill's own visual when the source has nothing authored, for a spell item, and without an action", [this]()
	{
		const TSoftObjectPtr<UNiagaraSystem> SkillMuzzle = CastTestVFX(TEXT("SkillMuzzle"));

		const FCastVisualChoice BareWeapon = Resolve(
			SpellInputs(ECharacterClass::Resonator, ESpellSource::WeaponCrystal, CastTestWeapon(false), nullptr), CastTestDefaults());
		TestTrue(TEXT("a weapon without CastVFX falls back"),
				 BareWeapon.Source == ECastVFXSource::Weapon && BareWeapon.bFellBackToSkill && BareWeapon.VFX == SkillMuzzle);

		const FCastVisualChoice NoDefaults = Resolve(SpellInputs(ECharacterClass::Caster, ESpellSource::Innate, nullptr, nullptr), nullptr);
		TestTrue(TEXT("no defaults asset falls back"), NoDefaults.bFellBackToSkill && NoDefaults.VFX == SkillMuzzle);

		const FCastVisualChoice Trainer = Resolve(
			SpellInputs(ECharacterClass::Resonator, ESpellSource::RingCrystal, nullptr, CastTestAdapter(EAdapterForm::Trainer)), CastTestDefaults());
		TestTrue(TEXT("an adapter type without a row falls back"), Trainer.bFellBackToSkill && Trainer.VFX == SkillMuzzle);

		const FCastVisualChoice Item = Resolve(SpellInputs(ECharacterClass::Generic, ESpellSource::Item, CastTestWeapon(), CastTestAdapter(EAdapterForm::Ring)), CastTestDefaults());
		TestTrue(TEXT("a spell item plays its own"), Item.Source == ECastVFXSource::Skill && !Item.bFellBackToSkill && Item.VFX == SkillMuzzle);

		FCastVisualInputs NoAction = SpellInputs(ECharacterClass::Caster, ESpellSource::Innate, nullptr, nullptr);
		NoAction.bHasAction = false;
		TestTrue(TEXT("no action: the skill's own"), Resolve(NoAction, CastTestDefaults()).Source == ECastVFXSource::Skill);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
