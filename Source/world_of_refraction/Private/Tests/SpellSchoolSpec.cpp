// SpellSchoolSpec.cpp
// B-356 (D-146, Q-356): three spell schools. Every ESpellSchool value maps onto Destruction,
// Enhancement or Restoration (a retired one is flagged and shows its mapped name), and the AI's
// spell picks read a spell's effects, never its school: a spell tagged against its effects is judged
// by the effects.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/AIDecisionManager.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/SpellSchool.h"
#include "Skills/Effects/EffectDefinition.h"
#include "WoRGameNames.h"

namespace
{
	constexpr int32 SCHOOL_TEST_DAMAGE = 10;
	constexpr float SCHOOL_TEST_HEAL = 20.0f;

	USpellData *SchoolTestSpell(ESpellSchool School, ETargetType Target, int32 BaseDamage, bool bWithHeal)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->School = School;
		Spell->TargetType = Target;
		Spell->BaseDamage = BaseDamage;
		if (bWithHeal)
		{
			UEffectDefinition *Def = NewObject<UEffectDefinition>(GetTransientPackage());
			FSkillEffectPayload Heal;
			Heal.EffectType = ESkillEffectType::HealthRestore;
			Heal.Magnitude = SCHOOL_TEST_HEAL;
			FSkillEffect Effect;
			Effect.Payloads.Add(Heal);
			Def->Effects.Add(Effect);
			Spell->ReferencedEffects.Add(Def);
		}
		return Spell;
	}
}

BEGIN_DEFINE_SPEC(FSpellSchoolSpec, "WoR.Spells.Schools",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FSpellSchoolSpec)

void FSpellSchoolSpec::Define()
{
	It("maps every school onto one of the three; only a retired one moves", [this]()
	{
		const UEnum *SchoolEnum = StaticEnum<ESpellSchool>();
		if (!TestNotNull(TEXT("ESpellSchool"), SchoolEnum)) return;
		const TSet<ESpellSchool> Three = {ESpellSchool::Destruction, ESpellSchool::Enhancement, ESpellSchool::Restoration};
		for (int32 Index = 0; Index < SchoolEnum->NumEnums() - 1; ++Index) // minus the generated _MAX
		{
			const ESpellSchool School = static_cast<ESpellSchool>(SchoolEnum->GetValueByIndex(Index));
			const FString Name = SchoolEnum->GetNameStringByIndex(Index);
			const ESpellSchool Current = SpellSchools::ToCurrent(School);
			TestTrue(Name + TEXT(" maps onto one of the three"), Three.Contains(Current));
			TestTrue(Name + TEXT(" is retired exactly when it moves"), SpellSchools::IsRetired(School) == (Current != School));
			TestFalse(Name + TEXT(" has a name"), WoRGameNames::GetSpellSchoolName(School).IsEmpty());
			TestTrue(Name + TEXT(" shows its mapped name"),
					 WoRGameNames::GetSpellSchoolName(School).EqualTo(WoRGameNames::GetSpellSchoolName(Current)));
		}
		for (const ESpellSchool School : Three)
		{
			TestFalse(TEXT("the three stay"), SpellSchools::IsRetired(School));
		}
		TestTrue(TEXT("Conjuration reads as Enhancement (Q-356)"),
				 SpellSchools::ToCurrent(ESpellSchool::Conjuration) == ESpellSchool::Enhancement);
		USpellData *Retired = SchoolTestSpell(ESpellSchool::Conjuration, ETargetType::Ally, 0, false);
		TestTrue(TEXT("a retired spell shows as Enhancement"), Retired->GetSchool() == ESpellSchool::Enhancement);
	});

	It("judges a spell's offence by its effects, not its school", [this]()
	{
		TestTrue(TEXT("Restoration-tagged damage at an enemy is offensive"),
				 UAIDecisionManager::IsOffensiveSpell(*SchoolTestSpell(ESpellSchool::Restoration, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)));
		TestFalse(TEXT("a Destruction-tagged spell aimed at an ally is not"),
				  UAIDecisionManager::IsOffensiveSpell(*SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Ally, SCHOOL_TEST_DAMAGE, false)));
		TestFalse(TEXT("a Destruction-tagged spell with no damage or debuff is not"),
				  UAIDecisionManager::IsOffensiveSpell(*SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Enemy, 0, false)));
	});

	It("finds a heal by its effects, not its school", [this]()
	{
		TestTrue(TEXT("a Destruction-tagged heal is a heal"),
				 UAIDecisionManager::IsHealingSpell(*SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Ally, 0, true)));
		TestFalse(TEXT("a Restoration-tagged spell without a heal is not"),
				  UAIDecisionManager::IsHealingSpell(*SchoolTestSpell(ESpellSchool::Restoration, ETargetType::Ally, 0, false)));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
