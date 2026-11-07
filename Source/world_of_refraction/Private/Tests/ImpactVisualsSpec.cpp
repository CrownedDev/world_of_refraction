// ImpactVisualsSpec.cpp
// B-369 (Q-369): each impact outcome picks its visual through the fallback chain (skill Impact entry → element
// default → generic), Guard / Dodge their own, a Perfect Guard nothing here, a spell arrival nothing extra.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Presentation/ImpactVisuals.h"
#include "Character/CosmeticsData.h"
#include "Skills/Definitions/AbilityData.h"
#include "NiagaraSystem.h"

namespace
{
	TSoftObjectPtr<UNiagaraSystem> TestVFX(const TCHAR *Name)
	{
		return TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(FString::Printf(TEXT("/Game/Test/%s.%s"), Name, Name)));
	}

	UCosmeticsData *TestCosmetics()
	{
		UCosmeticsData *Cosmetics = NewObject<UCosmeticsData>(GetTransientPackage());
		Cosmetics->ElementHitVFX.Add(ESpellElement::Fire, TestVFX(TEXT("FireHit")));
		Cosmetics->GenericHitVFX = TestVFX(TEXT("GenericHit"));
		Cosmetics->GuardSparkVFX = TestVFX(TEXT("Spark"));
		Cosmetics->DodgeWhiffVFX = TestVFX(TEXT("Whiff"));
		return Cosmetics;
	}

	UAbilityData *SkillWithImpactEntry()
	{
		UAbilityData *Skill = NewObject<UAbilityData>(GetTransientPackage());
		FSkillVFXEntry &Muzzle = Skill->VFXArray.AddDefaulted_GetRef();
		Muzzle.Role = EVFXRole::Muzzle;
		Muzzle.VFX = TestVFX(TEXT("Muzzle"));
		FSkillVFXEntry &Impact = Skill->VFXArray.AddDefaulted_GetRef();
		Impact.Role = EVFXRole::Impact;
		Impact.VFX = TestVFX(TEXT("SkillImpact"));
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

	It("walks the hit fallback chain: skill entry, element, generic", [this]()
	{
		UCosmeticsData *Cosmetics = TestCosmetics();
		const FImpactVisualChoice FromSkill = Choose(EImpactOutcome::Hit, SkillWithImpactEntry(), ESpellElement::Fire, Cosmetics, false);
		TestTrue(TEXT("the skill's Impact entry first"), FromSkill.Source == EImpactVisualSource::SkillEntry && FromSkill.VFX == TestVFX(TEXT("SkillImpact")));

		UAbilityData *Plain = NewObject<UAbilityData>(GetTransientPackage());
		const FImpactVisualChoice FromElement = Choose(EImpactOutcome::Hit, Plain, ESpellElement::Fire, Cosmetics, false);
		TestTrue(TEXT("then the element"), FromElement.Source == EImpactVisualSource::ElementDefault && FromElement.VFX == TestVFX(TEXT("FireHit")));

		const FImpactVisualChoice FromGeneric = Choose(EImpactOutcome::Hit, Plain, ESpellElement::Water, Cosmetics, false);
		TestTrue(TEXT("then the generic hit"), FromGeneric.Source == EImpactVisualSource::GenericDefault && FromGeneric.ShouldSpawn());

		TestFalse(TEXT("nothing anywhere: nothing"), Choose(EImpactOutcome::Hit, Plain, ESpellElement::Water, nullptr, false).ShouldSpawn());
	});

	It("never spawns nothing for a skill without an Impact entry when a default exists", [this]()
	{
		UAbilityData *NoEntry = NewObject<UAbilityData>(GetTransientPackage());
		TestTrue(TEXT("a default plays"), Choose(EImpactOutcome::Hit, NoEntry, ESpellElement::Earth, TestCosmetics(), false).ShouldSpawn());
	});

	It("plays a crit larger", [this]()
	{
		UCosmeticsData *Cosmetics = TestCosmetics();
		const FImpactVisualChoice Crit = Choose(EImpactOutcome::Crit, SkillWithImpactEntry(), ESpellElement::Fire, Cosmetics, false);
		TestEqual(TEXT("scaled"), Crit.Scale, Cosmetics->CritImpactVFXScale);
		TestEqual(TEXT("a hit is not"), Choose(EImpactOutcome::Hit, SkillWithImpactEntry(), ESpellElement::Fire, Cosmetics, false).Scale, 1.0f);
	});

	It("gives Guard and Dodge their own, and Perfect Guard nothing here", [this]()
	{
		UCosmeticsData *Cosmetics = TestCosmetics();
		TestTrue(TEXT("guard spark"), Choose(EImpactOutcome::Guard, SkillWithImpactEntry(), ESpellElement::Fire, Cosmetics, false).VFX == TestVFX(TEXT("Spark")));
		TestTrue(TEXT("dodge whiff"), Choose(EImpactOutcome::Dodge, SkillWithImpactEntry(), ESpellElement::Fire, Cosmetics, false).VFX == TestVFX(TEXT("Whiff")));
		const FImpactVisualChoice Perfect = Choose(EImpactOutcome::PerfectGuard, SkillWithImpactEntry(), ESpellElement::Fire, Cosmetics, false);
		TestTrue(TEXT("the DefenseSystem flash, not spawned here"), Perfect.Source == EImpactVisualSource::PerfectGuardFlash && !Perfect.ShouldSpawn());
		Cosmetics->DodgeWhiffVFX.Reset();
		TestFalse(TEXT("no whiff set: nothing"), Choose(EImpactOutcome::Dodge, nullptr, ESpellElement::Fire, Cosmetics, false).ShouldSpawn());
	});

	It("adds nothing to a spell arrival's hit, but still shows its guard", [this]()
	{
		UCosmeticsData *Cosmetics = TestCosmetics();
		TestFalse(TEXT("a hit: the projectile draws it"), Choose(EImpactOutcome::Hit, SkillWithImpactEntry(), ESpellElement::Fire, Cosmetics, true).ShouldSpawn());
		TestTrue(TEXT("a guard spark still"), Choose(EImpactOutcome::Guard, SkillWithImpactEntry(), ESpellElement::Fire, Cosmetics, true).ShouldSpawn());
	});

	It("prints the choice", [this]()
	{
		const FImpactVisualChoice Choice = Choose(EImpactOutcome::Crit, SkillWithImpactEntry(), ESpellElement::Fire, TestCosmetics(), false);
		const FString Line = GetChoiceString(EImpactOutcome::Crit, Choice);
		TestTrue(TEXT("names the outcome and source"), Line.Contains(TEXT("Crit")) && Line.Contains(TEXT("skill Impact entry")) && Line.Contains(TEXT("SkillImpact")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
