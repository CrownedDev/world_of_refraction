// DerivedStatsSpec.cpp
// GAS stage 3 (B-042s): crit chance and Efficiency derive on GAS from other attributes. Crit chance =
// (CRIT_CHANCE_BASE + Luck x CRIT_CHANCE_LUCK_BONUS) x its buff layer; Efficiency = 1 - EfficiencyReduction,
// a composed stat. The current values equal UDamageCalculator::GetCriticalChance and
// UCharacterDataComponent::GetEffectiveEfficiencyMultiplier.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Debug/GAS/WoRAttributeDebug.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRGameplayEffects.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 DERIVED_LUCK_BUFF_ID = 4901;
	constexpr int32 DERIVED_CRIT_DEBUFF_ID = 4902;
	constexpr int32 DERIVED_EFFICIENCY_BUFF_ID = 4903;
	constexpr int32 DERIVED_TURNS = 3;
	constexpr float DERIVED_BUFF_PERCENT = 50.0f;
	constexpr float DERIVED_DEBUFF_PERCENT = 30.0f;
	/** Points that put Luck and Efficiency well inside their ranges. */
	constexpr int32 DERIVED_STAT_POINTS = 10;
	/** A Luck value written straight onto the attribute, past what the getter gives. */
	constexpr float DERIVED_DIRECT_LUCK = 0.8f;
	/** Past Luck's gear ceiling (2.0) and the reduction's (0.9), so only the clamp decides. */
	constexpr float DERIVED_PAST_CEILING_LUCK = 2.5f;
	constexpr float DERIVED_PAST_CEILING_REDUCTION = 1.5f;
	constexpr float DERIVED_TOLERANCE = 1.0e-4f;
}

BEGIN_DEFINE_SPEC(FDerivedStatsSpec, "WoR.GAS.DerivedStats",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Hero = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	UCharacterDataComponent *Char = nullptr;
	UWoRAttributeSyncComponent *Sync = nullptr;
	const UDamageCalculator *Calc = nullptr;

	float Current(const FGameplayAttribute &Attribute) const { return ASC->GetNumericAttribute(Attribute); }

	TArray<FActiveGameplayEffectHandle> DerivedEffects(const FGameplayAttribute &Attribute) const
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = UWoRGE_DerivedStat::GetClassFor(Attribute);
		return ASC->GetActiveEffects(Query);
	}

	void Buff(int32 EffectID, ESkillEffectType Type, float Percent) const
	{
		if (USkillEffectManager *SEM = TestWorld->GetSubsystem<USkillEffectManager>())
		{
			SEM->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Derived stats test"), EffectID, Type, Percent, DERIVED_TURNS), Hero);
		}
		Sync->SyncAttributes();
	}
END_DEFINE_SPEC(FDerivedStatsSpec)

void FDerivedStatsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->Luck = DERIVED_STAT_POINTS;
		Data->Efficiency = DERIVED_STAT_POINTS;
		Hero = TestWorld->SpawnCombatant(Data);
		if (Hero)
		{
			ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			Sync = FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Hero);
			Char = Hero->FindComponentByClass<UCharacterDataComponent>();
		}
		Calc = TestWorld->GetSubsystem<UDamageCalculator>();
		if (!TestNotNull(TEXT("ASC"), ASC) || !TestNotNull(TEXT("Sync"), Sync) || !TestNotNull(TEXT("Character data"), Char) ||
			!TestNotNull(TEXT("Damage calculator"), Calc))
		{
			ASC = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Hero = nullptr;
		ASC = nullptr;
		Char = nullptr;
		Sync = nullptr;
		Calc = nullptr;
		TestWorld.Reset();
	});

	It("derives crit chance and Efficiency equal to their getters, one effect each", [this]()
	{
		if (!ASC) return;
		const FGameplayAttribute CritChance = UWoRAttributeSet_Stats::GetCritChanceAttribute();
		const FGameplayAttribute Efficiency = UWoRAttributeSet_Stats::GetEfficiencyAttribute();
		TestEqual(TEXT("Crit chance = getter"), Current(CritChance), Calc->GetCriticalChance(Hero), DERIVED_TOLERANCE);
		TestEqual(TEXT("Crit chance base"), ASC->GetNumericAttributeBase(CritChance), CombatConstants::CRIT_CHANCE_BASE);
		TestEqual(TEXT("Efficiency = getter"), Current(Efficiency), Char->GetEffectiveEfficiencyMultiplier(), DERIVED_TOLERANCE);
		TestEqual(TEXT("Efficiency = 1 - reduction"), Current(Efficiency),
				  1.0f - Current(UWoRAttributeSet_Stats::GetEfficiencyReductionAttribute()), DERIVED_TOLERANCE);
		TestTrue(TEXT("Efficiency below full cost"), Current(Efficiency) < 1.0f);
		TestEqual(TEXT("One crit chance effect"), DerivedEffects(CritChance).Num(), 1);
		TestEqual(TEXT("One Efficiency effect"), DerivedEffects(Efficiency).Num(), 1);
		TestTrue(TEXT("The attribute compare finds no mismatch"), UWoRAttributeDebug::GetAttributeMismatchString(Hero).IsEmpty());
	});

	It("follows a Luck change live, with no re-sync", [this]()
	{
		if (!ASC) return;
		const FGameplayAttribute CritChance = UWoRAttributeSet_Stats::GetCritChanceAttribute();
		const FGameplayAttribute Luck = UWoRAttributeSet_Stats::GetLuckAttribute();
		const float Before = Current(CritChance);
		ASC->SetNumericAttributeBase(Luck, DERIVED_DIRECT_LUCK);
		const float Expected = CombatConstants::CRIT_CHANCE_BASE + Current(Luck) * CombatConstants::CRIT_CHANCE_LUCK_BONUS;
		TestTrue(TEXT("Luck went up"), Current(Luck) > 0.0f);
		TestTrue(TEXT("Crit chance followed"), Current(CritChance) > Before);
		TestEqual(TEXT("Crit chance = base + Luck x bonus"), Current(CritChance), Expected, DERIVED_TOLERANCE);
	});

	It("clamps the captured Luck and reduction as the getters do", [this]()
	{
		if (!ASC) return;
		const FGameplayAttribute Luck = UWoRAttributeSet_Stats::GetLuckAttribute();
		ASC->SetNumericAttributeBase(Luck, DERIVED_PAST_CEILING_LUCK);
		TestEqual(TEXT("Luck at its ceiling"), Current(Luck), CombatConstants::LUCK_GEAR_CEILING);
		TestEqual(TEXT("Crit chance from the clamped Luck"), Current(UWoRAttributeSet_Stats::GetCritChanceAttribute()),
				  CombatConstants::CRIT_CHANCE_BASE + CombatConstants::LUCK_GEAR_CEILING * CombatConstants::CRIT_CHANCE_LUCK_BONUS,
				  DERIVED_TOLERANCE);

		const FGameplayAttribute Reduction = UWoRAttributeSet_Stats::GetEfficiencyReductionAttribute();
		ASC->SetNumericAttributeBase(Reduction, DERIVED_PAST_CEILING_REDUCTION);
		TestEqual(TEXT("Reduction at its ceiling"), Current(Reduction), CombatConstants::EFFICIENCY_GEAR_CEILING);
		TestEqual(TEXT("Efficiency from the clamped reduction"), Current(UWoRAttributeSet_Stats::GetEfficiencyAttribute()),
				  1.0f - CombatConstants::EFFICIENCY_GEAR_CEILING, DERIVED_TOLERANCE);
	});

	It("takes a Luck buff through the derived effect without re-applying it", [this]()
	{
		if (!ASC) return;
		const FGameplayAttribute CritChance = UWoRAttributeSet_Stats::GetCritChanceAttribute();
		const TArray<FActiveGameplayEffectHandle> HandlesBefore = DerivedEffects(CritChance);
		const float Before = Current(CritChance);
		Buff(DERIVED_LUCK_BUFF_ID, ESkillEffectType::LuckBuff, DERIVED_BUFF_PERCENT);
		TestTrue(TEXT("Crit chance went up"), Current(CritChance) > Before);
		TestEqual(TEXT("Crit chance = getter"), Current(CritChance), Calc->GetCriticalChance(Hero), DERIVED_TOLERANCE);
		TestTrue(TEXT("Same derived effect"), DerivedEffects(CritChance) == HandlesBefore);
	});

	It("updates the crit chance buff layer in place under a debuff", [this]()
	{
		if (!ASC) return;
		const FGameplayAttribute CritChance = UWoRAttributeSet_Stats::GetCritChanceAttribute();
		const TArray<FActiveGameplayEffectHandle> HandlesBefore = DerivedEffects(CritChance);
		const float Before = Current(CritChance);
		Buff(DERIVED_CRIT_DEBUFF_ID, ESkillEffectType::CritChanceDebuff, DERIVED_DEBUFF_PERCENT);
		TestTrue(TEXT("Crit chance went down"), Current(CritChance) < Before);
		TestEqual(TEXT("Crit chance = getter"), Current(CritChance), Calc->GetCriticalChance(Hero), DERIVED_TOLERANCE);
		TestTrue(TEXT("Same derived effect"), DerivedEffects(CritChance) == HandlesBefore);
	});

	It("derives Efficiency from its buffed reduction", [this]()
	{
		if (!ASC) return;
		const FGameplayAttribute Efficiency = UWoRAttributeSet_Stats::GetEfficiencyAttribute();
		const float Before = Current(Efficiency);
		Buff(DERIVED_EFFICIENCY_BUFF_ID, ESkillEffectType::EfficiencyBuff, DERIVED_BUFF_PERCENT);
		TestTrue(TEXT("Cheaper"), Current(Efficiency) < Before);
		TestEqual(TEXT("Efficiency = getter"), Current(Efficiency), Char->GetEffectiveEfficiencyMultiplier(), DERIVED_TOLERANCE);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
