// SpellSchoolSpec.cpp
// B-374 (D-163; was B-356 / B-356c, D-146): six spell schools, each a spell's identity. None is retired (Destruction
// reads as itself again); every school has a name; the command menu lists only the schools a character has spells
// in; a validator proposes each spell's school from its main effect and flags a mismatch (Q-374). The AI's spell
// picks still read a spell's effects, never its school.

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
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "UI/Combat/PieMenuButtonData.h"

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

	constexpr float SCHOOL_TEST_MAGNITUDE = 10.0f;
	constexpr int32 SCHOOL_COUNT = 6;

	/** A 0-damage spell of School at Target carrying one payload of Type. */
	USpellData *SchoolTestEffectSpell(ESpellSchool School, ETargetType Target, ESkillEffectType Type)
	{
		USpellData *Spell = SchoolTestSpell(School, Target, 0, false);
		UEffectDefinition *Def = NewObject<UEffectDefinition>(GetTransientPackage());
		FSkillEffectPayload Payload;
		Payload.EffectType = Type;
		Payload.Magnitude = SCHOOL_TEST_MAGNITUDE;
		FSkillEffect Effect;
		Effect.Payloads.Add(Payload);
		Def->Effects.Add(Effect);
		Spell->ReferencedEffects.Add(Def);
		return Spell;
	}

	bool Proposes(const USpellData *Spell, ESpellSchool Expected)
	{
		ESpellSchool Proposed = ESpellSchool::Conjuration;
		return USpellDataDebug::ProposeSchool(Spell, Proposed) && Proposed == Expected;
	}
}

BEGIN_DEFINE_SPEC(FSpellSchoolSpec, "WoR.Spells.Schools",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FSpellSchoolSpec)

void FSpellSchoolSpec::Define()
{
	It("lists six schools, each named, none retired; Destruction reads as itself (D-163)", [this]()
	{
		const UEnum *SchoolEnum = StaticEnum<ESpellSchool>();
		if (!TestNotNull(TEXT("ESpellSchool"), SchoolEnum)) return;
		TestEqual(TEXT("six schools"), SchoolEnum->NumEnums() - 1, SCHOOL_COUNT); // minus the generated _MAX
		for (int32 Index = 0; Index < SchoolEnum->NumEnums() - 1; ++Index)
		{
			const ESpellSchool School = static_cast<ESpellSchool>(SchoolEnum->GetValueByIndex(Index));
			const FString Name = SchoolEnum->GetNameStringByIndex(Index);
			TestTrue(Name + TEXT(" maps to itself"), SpellSchools::ToCurrent(School) == School);
			TestFalse(Name + TEXT(" is not retired"), SpellSchools::IsRetired(School));
			TestFalse(Name + TEXT(" has a name"), WoRGameNames::GetSpellSchoolName(School).IsEmpty());
		}
		TestTrue(TEXT("Destruction's own name"), WoRGameNames::GetSpellSchoolName(ESpellSchool::Destruction).ToString() == TEXT("Destruction"));
		TestFalse(TEXT("Destruction and Conjuration differ"),
				  WoRGameNames::GetSpellSchoolName(ESpellSchool::Destruction).EqualTo(WoRGameNames::GetSpellSchoolName(ESpellSchool::Conjuration)));
		USpellData *Destruction = SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false);
		TestTrue(TEXT("a Destruction spell shows as Destruction"), Destruction->GetSchool() == ESpellSchool::Destruction);
		TestEqual(TEXT("the menu enum mirrors the six"), StaticEnum<EPieMenuSpellSchool>()->NumEnums() - 1, SCHOOL_COUNT);
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

	It("finds a school's spells by their own school; Destruction and Conjuration are apart (B-374)", [this]()
	{
		FSpellCollection Spells;
		Spells.LearnedSpells.Add(FSpellInstance(SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)));
		Spells.LearnedSpells.Add(FSpellInstance(SchoolTestSpell(ESpellSchool::Conjuration, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)));
		Spells.LearnedSpells.Add(FSpellInstance(SchoolTestSpell(ESpellSchool::Restoration, ETargetType::Ally, 0, true)));
		TestEqual(TEXT("Conjuration: one"), Spells.GetSpellsBySchool(ESpellSchool::Conjuration).Num(), 1);
		TestEqual(TEXT("Destruction: one"), Spells.GetSpellsBySchool(ESpellSchool::Destruction).Num(), 1);
		TestEqual(TEXT("Manipulation: none"), Spells.GetSpellsBySchool(ESpellSchool::Manipulation).Num(), 0);
	});

	It("prints the shown school (B-356b)", [this]()
	{
		TestEqual(TEXT("Destruction"), USpellDataDebug::GetSchoolString(SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)),
				  FString(TEXT("Destruction")));
		TestEqual(TEXT("Enhancement"), USpellDataDebug::GetSchoolString(SchoolTestSpell(ESpellSchool::Enhancement, ETargetType::Ally, 0, false)),
				  FString(TEXT("Enhancement")));
	});

	It("proposes a school from the main effect (B-374, Q-374)", [this]()
	{
		TestTrue(TEXT("damage at an enemy: Destruction"), Proposes(SchoolTestSpell(ESpellSchool::Conjuration, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false), ESpellSchool::Destruction));
		TestTrue(TEXT("a drain: Destruction"), Proposes(SchoolTestSpell(ESpellSchool::Restoration, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, true), ESpellSchool::Destruction));
		TestTrue(TEXT("a heal: Restoration"), Proposes(SchoolTestSpell(ESpellSchool::Enhancement, ETargetType::Ally, 0, true), ESpellSchool::Restoration));
		TestTrue(TEXT("a cleanse: Restoration"), Proposes(SchoolTestEffectSpell(ESpellSchool::Enhancement, ETargetType::Ally, ESkillEffectType::CleanseAllies), ESpellSchool::Restoration));
		// Crown 2026-11-08: stun / silence / skip are Impairment; Manipulation changes what the enemy does.
		TestTrue(TEXT("a stun: Impairment"), Proposes(SchoolTestEffectSpell(ESpellSchool::Manipulation, ETargetType::Enemy, ESkillEffectType::Stun), ESpellSchool::Impairment));
		TestTrue(TEXT("apply stun: Impairment"), Proposes(SchoolTestEffectSpell(ESpellSchool::Manipulation, ETargetType::Enemy, ESkillEffectType::ApplyStunToTarget), ESpellSchool::Impairment));
		TestTrue(TEXT("a skip turn: Impairment"), Proposes(SchoolTestEffectSpell(ESpellSchool::Manipulation, ETargetType::Enemy, ESkillEffectType::SkipTurn), ESpellSchool::Impairment));
		TestTrue(TEXT("a silence: Impairment"), Proposes(SchoolTestEffectSpell(ESpellSchool::Manipulation, ETargetType::Enemy, ESkillEffectType::Silenced), ESpellSchool::Impairment));
		TestTrue(TEXT("confusion (Random Skill): Manipulation"), Proposes(SchoolTestEffectSpell(ESpellSchool::Impairment, ETargetType::Enemy, ESkillEffectType::RandomSkill), ESpellSchool::Manipulation));
		TestTrue(TEXT("a stat debuff: Impairment"), Proposes(SchoolTestEffectSpell(ESpellSchool::Manipulation, ETargetType::Enemy, ESkillEffectType::DefenseDebuff), ESpellSchool::Impairment));
		USpellData *StatusAttack = SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Enemy, 0, false);
		StatusAttack->StatusBuildup = SCHOOL_TEST_BUILDUP;
		TestTrue(TEXT("a 0-damage status attack: Impairment"), Proposes(StatusAttack, ESpellSchool::Impairment));
		TestTrue(TEXT("a buff: Enhancement"), Proposes(SchoolTestEffectSpell(ESpellSchool::Destruction, ETargetType::Ally, ESkillEffectType::DefenseBuff), ESpellSchool::Enhancement));
		ESpellSchool Unused = ESpellSchool::Conjuration;
		TestFalse(TEXT("an empty spell: no proposal"), USpellDataDebug::ProposeSchool(SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Ally, 0, false), Unused));
	});

	It("flags a Destruction-tagged buff; never a Conjuration spell or an agreeing one (B-374)", [this]()
	{
		TestTrue(TEXT("a Destruction-tagged buff"), USpellDataDebug::IsSchoolMismatch(SchoolTestEffectSpell(ESpellSchool::Destruction, ETargetType::Ally, ESkillEffectType::DefenseBuff)));
		TestFalse(TEXT("an Enhancement-tagged buff"), USpellDataDebug::IsSchoolMismatch(SchoolTestEffectSpell(ESpellSchool::Enhancement, ETargetType::Ally, ESkillEffectType::DefenseBuff)));
		TestFalse(TEXT("a Destruction-tagged attack"), USpellDataDebug::IsSchoolMismatch(SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)));
		TestFalse(TEXT("a conjured attack (D-163: it may deal damage)"), USpellDataDebug::IsSchoolMismatch(SchoolTestSpell(ESpellSchool::Conjuration, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)));
		TestFalse(TEXT("a conjured buff"), USpellDataDebug::IsSchoolMismatch(SchoolTestEffectSpell(ESpellSchool::Conjuration, ETargetType::Ally, ESkillEffectType::DefenseBuff)));
		TestFalse(TEXT("an empty spell"), USpellDataDebug::IsSchoolMismatch(SchoolTestSpell(ESpellSchool::Impairment, ETargetType::Ally, 0, false)));
		TestTrue(TEXT("a Restoration-tagged attack"), USpellDataDebug::IsSchoolMismatch(SchoolTestSpell(ESpellSchool::Restoration, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)));
	});

	It("shows only the schools a character has spells in, in school order, then Back (B-374)", [this]()
	{
		const TArray<USpellData *> Spells = {
			SchoolTestSpell(ESpellSchool::Restoration, ETargetType::Ally, 0, true),
			SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false),
			SchoolTestEffectSpell(ESpellSchool::Impairment, ETargetType::Enemy, ESkillEffectType::DefenseDebuff),
			SchoolTestSpell(ESpellSchool::Destruction, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)};
		const TArray<FPieMenuButtonData> Buttons = UCombatCommandMenuSubsystem::BuildSchoolButtons(Spells);
		if (!TestEqual(TEXT("three schools and Back"), Buttons.Num(), 4)) return;
		TestTrue(TEXT("Destruction first"), Buttons[0].School == EPieMenuSpellSchool::Destruction);
		TestEqual(TEXT("Destruction's own button"), Buttons[0].ButtonID, FString(TEXT("School_Destruction")));
		TestTrue(TEXT("its two spells"), Buttons[0].Description.ToString().Contains(TEXT("2")));
		TestTrue(TEXT("then Restoration"), Buttons[1].School == EPieMenuSpellSchool::Restoration);
		TestTrue(TEXT("then Impairment"), Buttons[2].School == EPieMenuSpellSchool::Impairment);
		TestEqual(TEXT("Impairment's name"), Buttons[2].DisplayName.ToString(), FString(TEXT("Impairment")));
		TestTrue(TEXT("Back last"), Buttons[3].Category != EPieMenuCategory::School);
	});

	It("reports the authored spells whose school disagrees with their effects (B-356b, informational)", [this]()
	{
		// Informational, never fails: the content findings are recorded in docs/PIE/B-356.md (editor step 2).
		AddInfo(FString::Printf(TEXT("school vs effects (B-356b audit):\n%s"), *USpellDataDebug::GetSchoolEffectMismatchString()));
	});

	It("keeps the 6-per-school innate cap per school (D-163)", [this]()
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
		TestFalse(TEXT("a Destruction spell counts under its own school (B-374)"), Flagged());
		Saved.InnateSpells.Add(FSpellRef(SchoolTestSpell(ESpellSchool::Conjuration, ETargetType::Enemy, SCHOOL_TEST_DAMAGE, false)));
		TestTrue(TEXT("a seventh Conjuration spell is one too many"), Flagged());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
