// ImpactVisualsSpec.cpp
// B-369 (Q-369, Q-369b changed by Crown 2026-11-08): the impact is the skill's (Impact entry; a Crit its CritImpact
// entry, else the Impact entry larger); without one, the element default, then a generic one (UImpactVisualDefaults);
// Guard / Dodge are the defender's cosmetic slots, a Perfect Guard nothing here, a projectile arrival nothing extra.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Presentation/ImpactVisuals.h"
#include "Combat/Presentation/ImpactVisualDefaults.h"
#include "Character/CosmeticsData.h"
#include "Skills/Definitions/AbilityData.h"
#include "NiagaraSystem.h"

namespace
{
	TSoftObjectPtr<UNiagaraSystem> TestVFX(const TCHAR *Name)
	{
		return TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(FString::Printf(TEXT("/Game/Test/%s.%s"), Name, Name)));
	}

	UImpactVisualDefaults *TestDefaults()
	{
		UImpactVisualDefaults *Defaults = NewObject<UImpactVisualDefaults>(GetTransientPackage());
		Defaults->ElementHitVFX.Add(ESpellElement::Fire, TestVFX(TEXT("FireHit")));
		Defaults->GenericHitVFX = TestVFX(TEXT("GenericHit"));
		return Defaults;
	}

	UCosmeticsData *TestDefenderCosmetics()
	{
		UCosmeticsData *Cosmetics = NewObject<UCosmeticsData>(GetTransientPackage());
		Cosmetics->GuardSparkVFX = TestVFX(TEXT("Spark"));
		Cosmetics->DodgeWhiffVFX = TestVFX(TEXT("Whiff"));
		return Cosmetics;
	}

	UAbilityData *SkillWithImpactEntry(bool bWithCritEntry = false)
	{
		UAbilityData *Skill = NewObject<UAbilityData>(GetTransientPackage());
		FSkillVFXEntry &Muzzle = Skill->VFXArray.AddDefaulted_GetRef();
		Muzzle.Role = EVFXRole::Muzzle;
		Muzzle.VFX = TestVFX(TEXT("Muzzle"));
		FSkillVFXEntry &Impact = Skill->VFXArray.AddDefaulted_GetRef();
		Impact.Role = EVFXRole::Impact;
		Impact.VFX = TestVFX(TEXT("SkillImpact"));
		if (bWithCritEntry)
		{
			FSkillVFXEntry &Crit = Skill->VFXArray.AddDefaulted_GetRef();
			Crit.Role = EVFXRole::CritImpact;
			Crit.VFX = TestVFX(TEXT("SkillCrit"));
		}
		return Skill;
	}
}

BEGIN_DEFINE_SPEC(FImpactVisualsSpec, "WoR.Presentation.ImpactVisuals",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FImpactVisualsSpec)

void FImpactVisualsSpec::Define()
{
	using namespace ImpactVisuals;

	It("reads the outcome from the defense and the crit", [this]()
	{
		TestTrue(TEXT("a dodge"), OutcomeFor(EDefenseType::Dodge, true, false, false) == EImpactOutcome::Dodge);
		TestTrue(TEXT("a failed dodge lands"), OutcomeFor(EDefenseType::Dodge, false, false, false) == EImpactOutcome::Hit);
		TestTrue(TEXT("a guard"), OutcomeFor(EDefenseType::Block, true, false, true) == EImpactOutcome::Guard);
		TestTrue(TEXT("a Perfect Guard (a Parry press too)"), OutcomeFor(EDefenseType::Parry, true, true, false) == EImpactOutcome::PerfectGuard);
		TestTrue(TEXT("a crit"), OutcomeFor(EDefenseType::None, false, false, true) == EImpactOutcome::Crit);
		TestTrue(TEXT("a hit"), OutcomeFor(EDefenseType::None, false, false, false) == EImpactOutcome::Hit);
	});

	It("plays the skill's impact first, then the element default, then a generic one", [this]()
	{
		UImpactVisualDefaults *Defaults = TestDefaults();
		const FImpactVisualChoice FromSkill = Choose(EImpactOutcome::Hit, SkillWithImpactEntry(), ESpellElement::Fire, Defaults, nullptr, false);
		TestTrue(TEXT("the skill's Impact entry first"), FromSkill.Source == EImpactVisualSource::SkillEntry && FromSkill.VFX == TestVFX(TEXT("SkillImpact")));

		UAbilityData *Plain = NewObject<UAbilityData>(GetTransientPackage());
		const FImpactVisualChoice FromElement = Choose(EImpactOutcome::Hit, Plain, ESpellElement::Fire, Defaults, nullptr, false);
		TestTrue(TEXT("then the element (a fire skill's impact is fire)"), FromElement.Source == EImpactVisualSource::ElementDefault && FromElement.VFX == TestVFX(TEXT("FireHit")));

		const FImpactVisualChoice FromGeneric = Choose(EImpactOutcome::Hit, Plain, ESpellElement::Water, Defaults, nullptr, false);
		TestTrue(TEXT("then the generic hit"), FromGeneric.Source == EImpactVisualSource::GenericDefault && FromGeneric.ShouldSpawn());

		TestFalse(TEXT("nothing anywhere: nothing"), Choose(EImpactOutcome::Hit, Plain, ESpellElement::Water, nullptr, nullptr, false).ShouldSpawn());
	});

	It("never takes a hit visual from the defender's cosmetics", [this]()
	{
		UAbilityData *Plain = NewObject<UAbilityData>(GetTransientPackage());
		TestFalse(TEXT("defender cosmetics only: no hit visual"),
				  Choose(EImpactOutcome::Hit, Plain, ESpellElement::Fire, nullptr, TestDefenderCosmetics(), false).ShouldSpawn());
	});

	It("plays a crit's own entry, else the impact larger", [this]()
	{
		UImpactVisualDefaults *Defaults = TestDefaults();
		const FImpactVisualChoice OwnCrit = Choose(EImpactOutcome::Crit, SkillWithImpactEntry(true), ESpellElement::Fire, Defaults, nullptr, false);
		TestTrue(TEXT("the CritImpact entry"), OwnCrit.Source == EImpactVisualSource::SkillCritEntry && OwnCrit.VFX == TestVFX(TEXT("SkillCrit")));
		TestEqual(TEXT("at its own size"), OwnCrit.Scale, 1.0f);
		const FImpactVisualChoice Scaled = Choose(EImpactOutcome::Crit, SkillWithImpactEntry(), ESpellElement::Fire, Defaults, nullptr, false);
		TestEqual(TEXT("no crit entry: the impact scaled"), Scaled.Scale, Defaults->CritImpactVFXScale);
		TestEqual(TEXT("no defaults asset: the fallback scale"),
				  Choose(EImpactOutcome::Crit, SkillWithImpactEntry(), ESpellElement::Fire, nullptr, nullptr, false).Scale, FALLBACK_CRIT_SCALE);
		TestEqual(TEXT("a hit is not scaled"), Choose(EImpactOutcome::Hit, SkillWithImpactEntry(true), ESpellElement::Fire, Defaults, nullptr, false).Scale, 1.0f);
		TestTrue(TEXT("a hit ignores the crit entry"),
				 Choose(EImpactOutcome::Hit, SkillWithImpactEntry(true), ESpellElement::Fire, Defaults, nullptr, false).VFX == TestVFX(TEXT("SkillImpact")));
	});

	It("takes Guard and Dodge from the defender's cosmetics, and Perfect Guard nothing here", [this]()
	{
		UCosmeticsData *Cosmetics = TestDefenderCosmetics();
		TestTrue(TEXT("guard spark"), Choose(EImpactOutcome::Guard, SkillWithImpactEntry(), ESpellElement::Fire, TestDefaults(), Cosmetics, false).VFX == TestVFX(TEXT("Spark")));
		TestTrue(TEXT("dodge whiff"), Choose(EImpactOutcome::Dodge, SkillWithImpactEntry(), ESpellElement::Fire, TestDefaults(), Cosmetics, false).VFX == TestVFX(TEXT("Whiff")));
		const FImpactVisualChoice Perfect = Choose(EImpactOutcome::PerfectGuard, SkillWithImpactEntry(), ESpellElement::Fire, TestDefaults(), Cosmetics, false);
		TestTrue(TEXT("the DefenseSystem flash, not spawned here"), Perfect.Source == EImpactVisualSource::PerfectGuardFlash && !Perfect.ShouldSpawn());
		Cosmetics->DodgeWhiffVFX.Reset();
		TestFalse(TEXT("no whiff set: nothing"), Choose(EImpactOutcome::Dodge, nullptr, ESpellElement::Fire, TestDefaults(), Cosmetics, false).ShouldSpawn());
	});

	It("adds nothing to a projectile arrival's hit, but still shows its guard", [this]()
	{
		UCosmeticsData *Cosmetics = TestDefenderCosmetics();
		TestFalse(TEXT("a hit: the projectile draws it"), Choose(EImpactOutcome::Hit, SkillWithImpactEntry(), ESpellElement::Fire, TestDefaults(), Cosmetics, true).ShouldSpawn());
		TestTrue(TEXT("a guard spark still"), Choose(EImpactOutcome::Guard, SkillWithImpactEntry(), ESpellElement::Fire, TestDefaults(), Cosmetics, true).ShouldSpawn());
	});

	It("prints the choice", [this]()
	{
		const FImpactVisualChoice Choice = Choose(EImpactOutcome::Crit, SkillWithImpactEntry(true), ESpellElement::Fire, TestDefaults(), nullptr, false);
		const FString Line = GetChoiceString(EImpactOutcome::Crit, Choice);
		TestTrue(TEXT("names the outcome and source"), Line.Contains(TEXT("Crit")) && Line.Contains(TEXT("CritImpact")) && Line.Contains(TEXT("SkillCrit")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
