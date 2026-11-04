// AIOneSkillPathSpec.cpp
// B-345 (D-136): spells and abilities share one AI scoring path, and the weights sit in UCombatTuningData.
// At default weights the one path builds the same probe actions as the old spell / ability twins did,
// and for a fixed scenario (two spells, an ability, an ability the AI can't pay for) it picks the same
// skill the old twin loops picked: the best affordable estimate, first on a tie, with the strongest
// unaffordable one held for the Emerald. Status buildup is 0 here, so a skill's score is its damage.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AI/AIDecisionManager.h"
#include "AI/AIDecisionConstants.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Tuning/CombatTuningData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr int32 ONE_PATH_TEST_WEAK_DAMAGE = 5;
	constexpr int32 ONE_PATH_TEST_STRONG_DAMAGE = 40;
	constexpr int32 ONE_PATH_TEST_MID_DAMAGE = 20;
	/** Far above every affordable skill, so the held (unaffordable) one is the strongest. */
	constexpr int32 ONE_PATH_TEST_HELD_DAMAGE = 200;
	constexpr int32 ONE_PATH_TEST_ATTACKER_EP = 10;
	/** Above the attacker's EP: the held ability can't be paid for. */
	constexpr int32 ONE_PATH_TEST_HELD_COST = 50;
	constexpr int32 ONE_PATH_TEST_INFUSION_LEVEL = 1;
}

BEGIN_DEFINE_SPEC(FAIOneSkillPathSpec, "WoR.AI.OneSkillPath",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UAIDecisionManager *AI = nullptr;
	AActor *Attacker = nullptr;
	AActor *Target = nullptr;

	USpellData *NewSpell(int32 BaseDamage) const
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->BaseDamage = BaseDamage;
		Spell->School = ESpellSchool::Conjuration;
		return Spell;
	}

	UAbilityData *NewAbility(int32 BaseDamage, int32 EnergyCost = 0) const
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->BaseDamage = BaseDamage;
		Ability->BaseEnergyCost = EnergyCost;
		return Ability;
	}

	/** The probe the old EstimateSpellDamage built (before B-345). */
	static FAction OldSpellProbe(AActor *Target, USpellData *Spell, int32 Level, EInfusionSourceOption Source)
	{
		FAction Action;
		Action.ActionType = EActionType::Spell;
		Action.SpellData = Spell;
		Action.SpellSource = ESpellSource::Innate; // no loadout: the old code's fallback
		Action.SpellInfusionLevel = Level;
		Action.SelectedSource = Level > 0 ? Source : EInfusionSourceOption::None;
		Action.Targets.Add(Target);
		return Action;
	}

	/** The probe the old EstimateAbilityDamage built (before B-345). */
	static FAction OldAbilityProbe(AActor *Target, UAbilityData *Ability, int32 Level, EInfusionSourceOption Source)
	{
		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Ability;
		Action.AbilityInfusionLevel = Level;
		Action.SelectedSource = Level > 0 ? Source : EInfusionSourceOption::None;
		Action.Targets.Add(Target);
		return Action;
	}

	void TestSameProbe(const TCHAR *What, const FAction &New, const FAction &Old)
	{
		TestTrue(FString::Printf(TEXT("%s: type"), What), New.ActionType == Old.ActionType);
		TestTrue(FString::Printf(TEXT("%s: spell"), What), New.SpellData == Old.SpellData);
		TestTrue(FString::Printf(TEXT("%s: skill"), What), New.SkillData == Old.SkillData);
		TestTrue(FString::Printf(TEXT("%s: spell source"), What), New.SpellSource == Old.SpellSource);
		TestEqual(FString::Printf(TEXT("%s: spell level"), What), New.SpellInfusionLevel, Old.SpellInfusionLevel);
		TestEqual(FString::Printf(TEXT("%s: ability level"), What), New.AbilityInfusionLevel, Old.AbilityInfusionLevel);
		TestTrue(FString::Printf(TEXT("%s: infusion source"), What), New.SelectedSource == Old.SelectedSource);
		TestEqual(FString::Printf(TEXT("%s: targets"), What), New.Targets.Num(), Old.Targets.Num());
	}
END_DEFINE_SPEC(FAIOneSkillPathSpec)

void FAIOneSkillPathSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		AI = TestWorld->GetSubsystem<UAIDecisionManager>();
		Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		Target = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("AI"), AI) || !TestNotNull(TEXT("Attacker"), Attacker) || !TestNotNull(TEXT("Target"), Target))
		{
			AI = nullptr; // each case returns early
			return;
		}
		UCharacterDataComponent *Char = Attacker->FindComponentByClass<UCharacterDataComponent>();
		if (TestNotNull(TEXT("Attacker character data"), Char))
		{
			Char->CurrentEP = ONE_PATH_TEST_ATTACKER_EP;
		}
	});

	AfterEach([this]()
	{
		AI = nullptr;
		TestWorld.Reset();
	});

	It("starts with the AI weights equal to the C++ constants", [this]()
	{
		const UCombatTuningData *D = GetDefault<UCombatTuningData>();
		TestEqual(TEXT("AIKillPotentialScore"), D->AIKillPotentialScore, AIConstants::KILL_POTENTIAL_SCORE);
		TestEqual(TEXT("AIHPMissingWeight"), D->AIHPMissingWeight, AIConstants::HP_MISSING_WEIGHT);
		TestEqual(TEXT("AIThreatWeight"), D->AIThreatWeight, AIConstants::THREAT_WEIGHT);
		TestEqual(TEXT("AIRawDamageThreatMult"), D->AIRawDamageThreatMult, AIConstants::RAW_DAMAGE_THREAT_MULT);
		TestEqual(TEXT("AIStatusMultiplierThreatMult"), D->AIStatusMultiplierThreatMult, AIConstants::STATUS_MULTIPLIER_THREAT_MULT);
		TestEqual(TEXT("AISpellPowerThreatMult"), D->AISpellPowerThreatMult, AIConstants::SPELL_POWER_THREAT_MULT);
		TestEqual(TEXT("AIStatusScoreWeight"), D->AIStatusScoreWeight, AIConstants::STATUS_SCORE_WEIGHT);
		TestEqual(TEXT("AIStatusScoreTrigger"), D->AIStatusScoreTrigger, AIConstants::STATUS_SCORE_TRIGGER);
		TestEqual(TEXT("AIStatusScoreContribute"), D->AIStatusScoreContribute, AIConstants::STATUS_SCORE_CONTRIBUTE);
		TestEqual(TEXT("AIStatusScoreRedundant"), D->AIStatusScoreRedundant, AIConstants::STATUS_SCORE_REDUNDANT);
		TestEqual(TEXT("AIStatusBarNearTrigger"), D->AIStatusBarNearTrigger, AIConstants::STATUS_BAR_NEAR_TRIGGER_THRESHOLD);
		TestEqual(TEXT("AIEnergyConservationThreshold"), D->AIEnergyConservationThreshold, AIConstants::ENERGY_CONSERVATION_THRESHOLD);
		TestEqual(TEXT("AIEnergyAbundantThreshold"), D->AIEnergyAbundantThreshold, AIConstants::ENERGY_ABUNDANT_THRESHOLD);
		TestEqual(TEXT("AIHealHPThresholdHard"), D->AIHealHPThresholdHard, AIConstants::HARDEXPERT_HEAL_HP_THRESHOLD);
		TestEqual(TEXT("AIHealHPThresholdEasy"), D->AIHealHPThresholdEasy, AIConstants::EASYMED_HEAL_HP_THRESHOLD);
		TestEqual(TEXT("AILastStandHPThreshold"), D->AILastStandHPThreshold, AIConstants::LAST_STAND_HP_THRESHOLD);
		TestEqual(TEXT("AIKillSecureFactor"), D->AIKillSecureFactor, AIConstants::KILL_SECURE_FACTOR);
		TestEqual(TEXT("AIFreeActionFactor"), D->AIFreeActionFactor, AIConstants::FREE_ACTION_FACTOR);
		TestEqual(TEXT("AIStarveMargin"), D->AIStarveMargin, AIConstants::STARVE_MARGIN);
		TestEqual(TEXT("AIDelayDecay"), D->AIDelayDecay, AIConstants::DELAY_DECAY);
		TestEqual(TEXT("AIDefenseAttemptEasy"), D->AIDefenseAttemptEasy, AIConstants::EASY_DEFENSE_ATTEMPT);
		TestEqual(TEXT("AIDefenseAttemptMedium"), D->AIDefenseAttemptMedium, AIConstants::MEDIUM_DEFENSE_ATTEMPT);
		TestEqual(TEXT("AIDefenseAttemptHard"), D->AIDefenseAttemptHard, AIConstants::HARD_DEFENSE_ATTEMPT);
		TestEqual(TEXT("AIDefenseAttemptExpert"), D->AIDefenseAttemptExpert, AIConstants::EXPERT_DEFENSE_ATTEMPT);
	});

	It("builds the same probe actions the spell and ability twins built", [this]()
	{
		if (!AI) return;
		USpellData *Spell = NewSpell(ONE_PATH_TEST_STRONG_DAMAGE);
		UAbilityData *Ability = NewAbility(ONE_PATH_TEST_STRONG_DAMAGE);
		TestSameProbe(TEXT("spell L0"), AI->MakeSkillAction(Attacker, Target, Spell),
					  OldSpellProbe(Target, Spell, 0, EInfusionSourceOption::None));
		TestSameProbe(TEXT("spell L1"),
					  AI->MakeSkillAction(Attacker, Target, Spell, ONE_PATH_TEST_INFUSION_LEVEL, EInfusionSourceOption::Innate),
					  OldSpellProbe(Target, Spell, ONE_PATH_TEST_INFUSION_LEVEL, EInfusionSourceOption::Innate));
		TestSameProbe(TEXT("ability L1"),
					  AI->MakeSkillAction(Attacker, Target, Ability, ONE_PATH_TEST_INFUSION_LEVEL, EInfusionSourceOption::Raw),
					  OldAbilityProbe(Target, Ability, ONE_PATH_TEST_INFUSION_LEVEL, EInfusionSourceOption::Raw));
		TestEqual(TEXT("spell estimate"), AI->EstimateSkillDamage(Attacker, Target, Spell),
				  AI->EstimateActionDamage(Attacker, Target, OldSpellProbe(Target, Spell, 0, EInfusionSourceOption::None)));
		TestEqual(TEXT("ability estimate"), AI->EstimateSkillDamage(Attacker, Target, Ability),
				  AI->EstimateActionDamage(Attacker, Target, OldAbilityProbe(Target, Ability, 0, EInfusionSourceOption::None)));
	});

	It("picks the same skill as before for a fixed scenario", [this]()
	{
		if (!AI) return;
		USpellData *WeakSpell = NewSpell(ONE_PATH_TEST_WEAK_DAMAGE);
		USpellData *StrongSpell = NewSpell(ONE_PATH_TEST_STRONG_DAMAGE);
		UAbilityData *MidAbility = NewAbility(ONE_PATH_TEST_MID_DAMAGE);
		UAbilityData *HeldAbility = NewAbility(ONE_PATH_TEST_HELD_DAMAGE, ONE_PATH_TEST_HELD_COST);
		if (!TestFalse(TEXT("the held ability is unaffordable"), AI->CanAffordSkill(Attacker, HeldAbility)) ||
			!TestTrue(TEXT("the free spell is affordable"), AI->CanAffordSkill(Attacker, StrongSpell)))
		{
			return;
		}

		// The old spell loop's pick: the best affordable estimate (status 0 here), first on a tie.
		const int32 OldWeak = AI->EstimateActionDamage(Attacker, Target, OldSpellProbe(Target, WeakSpell, 0, EInfusionSourceOption::None));
		const int32 OldStrong = AI->EstimateActionDamage(Attacker, Target, OldSpellProbe(Target, StrongSpell, 0, EInfusionSourceOption::None));
		USpellData *OldSpellPick = OldStrong > OldWeak ? StrongSpell : WeakSpell;

		float SpellScore = 0.0f;
		float UnaffordableBest = 0.0f;
		int32 UnaffordableCost = 0;
		TArray<USkillDataBase *> Spells;
		Spells.Add(WeakSpell);
		Spells.Add(StrongSpell);
		USkillDataBase *SpellPick = AI->ChooseBestSkill(Attacker, Target, Spells, SpellScore, UnaffordableBest, UnaffordableCost);
		TestTrue(TEXT("spells: same pick"), SpellPick == OldSpellPick);
		TestEqual(TEXT("spells: same score"), FMath::RoundToInt(SpellScore), FMath::Max(OldWeak, OldStrong));

		TArray<USkillDataBase *> Abilities;
		Abilities.Add(MidAbility);
		Abilities.Add(HeldAbility);
		float AbilityScore = 0.0f;
		USkillDataBase *AbilityPick = AI->ChooseBestSkill(Attacker, Target, Abilities, AbilityScore, UnaffordableBest, UnaffordableCost);
		TestTrue(TEXT("abilities: the affordable one"), AbilityPick == MidAbility);
		TestTrue(TEXT("the unaffordable one is held for the Emerald"), UnaffordableBest > 0.0f);
		TestTrue(TEXT("its cost is recorded"), UnaffordableCost > ONE_PATH_TEST_ATTACKER_EP);
	});

	It("keeps the first skill on a tie", [this]()
	{
		if (!AI) return;
		USpellData *First = NewSpell(ONE_PATH_TEST_MID_DAMAGE);
		USpellData *Second = NewSpell(ONE_PATH_TEST_MID_DAMAGE);
		float Score = 0.0f;
		float UnaffordableBest = 0.0f;
		int32 UnaffordableCost = 0;
		TArray<USkillDataBase *> Tied;
		Tied.Add(First);
		Tied.Add(Second);
		TestTrue(TEXT("first wins"), AI->ChooseBestSkill(Attacker, Target, Tied, Score, UnaffordableBest, UnaffordableCost) == First);
	});

	It("builds a spell and an ability action through the one apply step: right fields, an affordable level, a source only when infused", [this]()
	{
		if (!AI) return;
		UCharacterDataComponent *Char = Attacker->FindComponentByClass<UCharacterDataComponent>();
		if (!TestNotNull(TEXT("Attacker character data"), Char)) return;

		USpellData *Spell = NewSpell(ONE_PATH_TEST_STRONG_DAMAGE);
		FAction SpellAction;
		SpellAction.Targets.Add(Target);
		AI->ApplySkillChoice(Attacker, Char, Spell, SpellAction);
		TestTrue(TEXT("spell: type"), SpellAction.ActionType == EActionType::Spell);
		TestTrue(TEXT("spell: data"), SpellAction.SpellData == Spell && SpellAction.SkillData == nullptr);
		TestEqual(TEXT("spell: no ability level"), SpellAction.AbilityInfusionLevel, 0);
		TestTrue(TEXT("spell: the level is affordable"), AI->CanAffordSkill(Attacker, Spell, SpellAction.SpellInfusionLevel));
		TestTrue(TEXT("spell: a source only when infused"),
				 (SpellAction.SpellInfusionLevel > 0) == (SpellAction.SelectedSource != EInfusionSourceOption::None));

		UAbilityData *Ability = NewAbility(ONE_PATH_TEST_MID_DAMAGE, ONE_PATH_TEST_ATTACKER_EP);
		FAction AbilityAction;
		AbilityAction.Targets.Add(Target);
		AI->ApplySkillChoice(Attacker, Char, Ability, AbilityAction);
		TestTrue(TEXT("ability: type"), AbilityAction.ActionType != EActionType::Spell);
		TestTrue(TEXT("ability: data"), AbilityAction.SkillData == Ability && AbilityAction.SpellData == nullptr);
		TestEqual(TEXT("ability: no spell level"), AbilityAction.SpellInfusionLevel, 0);
		TestTrue(TEXT("ability: the level is affordable"), AI->CanAffordSkill(Attacker, Ability, AbilityAction.AbilityInfusionLevel));
		TestTrue(TEXT("ability: a source only when infused"),
				 (AbilityAction.AbilityInfusionLevel > 0) == (AbilityAction.SelectedSource != EInfusionSourceOption::None));
		TestEqual(TEXT("ability: the target is kept"), AbilityAction.Targets.Num(), 1);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
