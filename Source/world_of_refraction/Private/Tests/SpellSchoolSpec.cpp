// SpellSchoolSpec.cpp
// B-356 / B-356c (D-146 as corrected, Q-356): three spell schools. Every ESpellSchool value maps onto
// Conjuration, Enhancement or Restoration (a retired one is flagged and shows its mapped name), and the AI's
// spell picks read a spell's effects, never its school: a spell tagged against its effects is judged
// by the effects.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AI/AIDecisionManager.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/SpellSchool.h"
#include "Skills/Effects/EffectDefinition.h"
#include "WoRGameNames.h"
#include "Loadout/FSavedLoadout.h"
#include "Loadout/Entries/FSpellCollection.h"
#include "Loadout/SpellPoolConstants.h"
#include "Debug/Skills/SpellDataDebug.h"

namespace
{
	constexpr int32 SCHOOL_TEST_DAMAGE = 10;
	constexpr float SCHOOL_TEST_HEAL = 20.0f;
	constexpr int32 SCHOOL_TEST_BUILDUP = 15;

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
		const TSet<ESpellSchool> Three = {ESpellSchool::Conjuration, ESpellSchool::Enhancement, ESpellSchool::Restoration};
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
		TestTrue(TEXT("Destruction reads as Conjuration (Q-356, D-146 corrected)"),
				 SpellSchools::ToCurrent(ESpellSchool::Destruction) == ESpellSchool::Conjuration);
		TestTrue(TEXT("Conjuration maps to itself"),
				 SpellSchools::ToCurrent(ESpellSchool::Conjuration) == ESpellSchool::Conjuration);
		USpellData *Retired = SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false);
		TestTrue(TEXT("a retired spell shows as Conjuration"), Retired->GetSchool() == ESpellSchool::Conjuration);
	});

	It("judges a spell's offence by its effects, not its school", [this]()
	{
		TestTrue(TEXT("Restoration-tagged damage at an enemy is offensive"),
				 UAIDecisionManager::IsOffensiveSpell(*SchoolTestSpell(ESpellSchool::Restoration, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)));
		TestFalse(TEXT("a Conjuration-tagged spell aimed at an ally is not"),
				  UAIDecisionManager::IsOffensiveSpell(*SchoolTestSpell(ESpellSchool::Conjuration, ETargetType::Ally, SCHOOL_TEST_DAMAGE, false)));
		TestFalse(TEXT("a Conjuration-tagged spell with no damage or debuff is not"),
				  UAIDecisionManager::IsOffensiveSpell(*SchoolTestSpell(ESpellSchool::Conjuration, ETargetType::Enemy, 0, false)));
		USpellData *StatusAttack = SchoolTestSpell(ESpellSchool::Conjuration, ETargetType::Enemy, 0, false);
		StatusAttack->StatusBuildup = SCHOOL_TEST_BUILDUP;
		TestTrue(TEXT("a 0-damage status attack at an enemy is offensive (B-356b)"), UAIDecisionManager::IsOffensiveSpell(*StatusAttack));
	});

	It("finds a heal by its effects, not its school", [this]()
	{
		TestTrue(TEXT("a Conjuration-tagged heal is a heal"),
				 UAIDecisionManager::IsHealingSpell(*SchoolTestSpell(ESpellSchool::Conjuration, ETargetType::Ally, 0, true)));
		TestFalse(TEXT("a Restoration-tagged spell without a heal is not"),
				  UAIDecisionManager::IsHealingSpell(*SchoolTestSpell(ESpellSchool::Restoration, ETargetType::Ally, 0, false)));
	});

	It("never picks a drain as the heal (B-356b)", [this]()
	{
		const USpellData *Drain = SchoolTestSpell(ESpellSchool::Conjuration, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, true);
		TestTrue(TEXT("a drain restores health"), UAIDecisionManager::IsHealingSpell(*Drain));
		TestTrue(TEXT("a drain is offensive"), UAIDecisionManager::IsOffensiveSpell(*Drain));
		TestFalse(TEXT("so it is not the heal pick"), UAIDecisionManager::IsHealPick(*Drain));
		TestTrue(TEXT("a plain heal is the heal pick"),
				 UAIDecisionManager::IsHealPick(*SchoolTestSpell(ESpellSchool::Restoration, ETargetType::Ally, 0, true)));
	});

	It("finds a school's spells whichever name the filter uses (B-356b)", [this]()
	{
		FSpellCollection Spells;
		Spells.LearnedSpells.Add(FSpellInstance(SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)));
		Spells.LearnedSpells.Add(FSpellInstance(SchoolTestSpell(ESpellSchool::Conjuration, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)));
		Spells.LearnedSpells.Add(FSpellInstance(SchoolTestSpell(ESpellSchool::Restoration, ETargetType::Ally, 0, true)));
		TestEqual(TEXT("asking for Conjuration finds both damage spells"), Spells.GetSpellsBySchool(ESpellSchool::Conjuration).Num(), 2);
		TestEqual(TEXT("asking with the retired name finds the same two"), Spells.GetSpellsBySchool(ESpellSchool::Destruction).Num(), 2);
		TestEqual(TEXT("by element and school too"),
				  Spells.GetSpellsByElementAndSchool(ESpellElement::Fire, ESpellSchool::Destruction).Num(), 2);
	});

	It("prints the shown school and flags a retired one (B-356b)", [this]()
	{
		TestEqual(TEXT("retired"), USpellDataDebug::GetSchoolString(SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)),
				  FString(TEXT("Conjuration (authored Destruction, retired)")));
		TestEqual(TEXT("current"), USpellDataDebug::GetSchoolString(SchoolTestSpell(ESpellSchool::Enhancement, ETargetType::Ally, 0, false)),
				  FString(TEXT("Enhancement")));
	});

	It("reports the authored spells whose school disagrees with their effects (B-356b, informational)", [this]()
	{
		// Informational, never fails: the content findings are recorded in docs/PIE/B-356.md (editor step 2).
		AddInfo(FString::Printf(TEXT("school vs effects (B-356b audit):\n%s"), *USpellDataDebug::GetSchoolEffectMismatchString()));
	});

	It("counts a retired spell under its mapped school's innate cap", [this]()
	{
		FSavedLoadout Saved;
		Saved.RequiredClass = ECharacterClass::Caster;
		for (int32 i = 0; i < SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL; ++i)
		{
			Saved.InnateSpells.Add(FSpellRef(SchoolTestSpell(ESpellSchool::Conjuration, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)));
		}
		const FString TooMany = FString::Printf(TEXT("Too many %s innate spells"), *UEnum::GetValueAsString(ESpellSchool::Conjuration)); // B-356b: the enum's own string
		auto Flagged = [&]() {
			return Saved.GetValidationErrors().ContainsByPredicate([&](const FString &R) { return R.Contains(TooMany); });
		};
		TestFalse(TEXT("a full Conjuration pool passes"), Flagged());
		Saved.InnateSpells.Add(FSpellRef(SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)));
		TestTrue(TEXT("one Destruction-tagged spell more is one Conjuration spell too many"), Flagged());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
