// StatSourcesSpec.cpp
// GAS stage 3 (B-042p, B-042q): the composed stats (UWoRAttributeSet_Stats::GetComposedStats) on GAS. The
// attribute's base is the capped stat, a UWoRGE_StatSources effect multiplies in the gear, stone and
// buff layers, and the current value equals the getter (UCharacterDataComponent::GetEffective*,
// UDamageCalculator::GetDefenderFlatDefense / GetCritDamageMultiplier), clamped as the getter is.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Combat/CombatConstants.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Debug/GAS/WoRAttributeDebug.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	constexpr int32 STAT_SOURCES_BUFF_ID = 4801;
	constexpr int32 STAT_SOURCES_HUGE_BUFF_ID = 4802;
	constexpr int32 STAT_SOURCES_TURNS = 3;
	/** Percents, as buff values are. */
	constexpr float STAT_SOURCES_BUFF_PERCENT = 50.0f;
	/** Far past the compose ceiling, so the clamp decides. */
	constexpr float STAT_SOURCES_HUGE_BUFF_PERCENT = 500.0f;
	/** Larger than the huge buff, so the net goes negative and only the clamp floors it. */
	constexpr float STAT_SOURCES_HUGE_DEBUFF_PERCENT = 1000.0f;
	constexpr int32 STAT_SOURCES_DEBUFF_ID = 4803;
	/** Enough points for a stat alone (Defense, Luck) to reach its cap. */
	constexpr int32 STAT_SOURCES_STAT_POINTS = 50;
	constexpr float STAT_SOURCES_TOLERANCE = 1.0e-4f;
}

BEGIN_DEFINE_SPEC(FStatSourcesSpec, "WoR.GAS.StatSources",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Hero = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	UCharacterDataComponent *Char = nullptr;
	UWoRAttributeSyncComponent *Sync = nullptr;

	/** Each composed stat with its attribute and getter. */
	TArray<TTuple<ESubStat, FGameplayAttribute, float>> ComposedStats() const
	{
		const UDamageCalculator *Calc = TestWorld->GetSubsystem<UDamageCalculator>();
		return {
			MakeTuple(ESubStat::SpellDamage, UWoRAttributeSet_Stats::GetSpellDamageAttribute(), Char->GetEffectiveSpellDamage()),
			MakeTuple(ESubStat::RawDamage, UWoRAttributeSet_Stats::GetRawDamageAttribute(), Char->GetEffectiveRawDamage()),
			MakeTuple(ESubStat::ActionSpeed, UWoRAttributeSet_Stats::GetActionSpeedAttribute(), Char->GetEffectiveActionSpeed()),
			MakeTuple(ESubStat::SpellSpeed, UWoRAttributeSet_Stats::GetSpellSpeedAttribute(), Char->GetEffectiveSpellSpeed()),
			MakeTuple(ESubStat::StatusMultiplier, UWoRAttributeSet_Stats::GetStatusMultiplierAttribute(), Char->GetEffectiveStatusMultiplier()),
			MakeTuple(ESubStat::Defense, UWoRAttributeSet_Stats::GetDefenseAttribute(), Calc ? Calc->GetDefenderFlatDefense(Hero) : 0.0f),
			MakeTuple(ESubStat::CritDamage, UWoRAttributeSet_Stats::GetCritDamageAttribute(), Calc ? Calc->GetCritDamageMultiplier(Hero) : 0.0f),
			MakeTuple(ESubStat::Luck, UWoRAttributeSet_Stats::GetLuckAttribute(), Char->GetEquipmentModifiedLuck()),
			MakeTuple(ESubStat::Reflex, UWoRAttributeSet_Stats::GetReflexAttribute(), Char->ReflexWindowGearFactor()),
		};
	}

	/** The sources effects share one family tag: one per composed stat and per pillar (B-042t). */
	static int32 ComposedCount() { return UWoRAttributeSet_Stats::GetComposedStats().Num() + UWoRAttributeSet_Stats::GetComposedPillars().Num(); }

	TArray<FActiveGameplayEffectHandle> SourceEffects() const
	{
		const FGameplayEffectQuery Query =
			FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FGameplayTagContainer(WoRTags::Effect_Family_StatSources));
		return ASC->GetActiveEffects(Query);
	}

	int32 CountSourceEffects() const { return SourceEffects().Num(); }

	void Buff(int32 EffectID, ESkillEffectType Type, float Percent) const
	{
		if (USkillEffectManager *SEM = TestWorld->GetSubsystem<USkillEffectManager>())
		{
			SEM->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Stat sources test"), EffectID, Type, Percent, STAT_SOURCES_TURNS), Hero);
		}
		Sync->SyncAttributes();
	}
END_DEFINE_SPEC(FStatSourcesSpec)

void FStatSourcesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (Hero)
		{
			ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Hero);
			Sync = FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Hero);
			Char = Hero->FindComponentByClass<UCharacterDataComponent>();
		}
		if (!TestNotNull(TEXT("ASC"), ASC) || !TestNotNull(TEXT("Sync"), Sync) || !TestNotNull(TEXT("Character data"), Char) ||
			!TestNotNull(TEXT("Damage calculator"), TestWorld->GetSubsystem<UDamageCalculator>()))
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
		TestWorld.Reset();
	});

	It("holds the capped stat as the base and the getter as the current value", [this]()
	{
		if (!ASC) return;
		TestEqual(TEXT("One sources effect per composed stat"), CountSourceEffects(), ComposedCount());
		for (const TTuple<ESubStat, FGameplayAttribute, float> &Stat : ComposedStats())
		{
			const FString Name = UEnum::GetValueAsString(Stat.Get<0>());
			FWoRStatLayers Layers;
			TestTrue(*(Name + TEXT(": has layers")), Char->GetStatLayers(Stat.Get<0>(), Layers));
			TestEqual(*(Name + TEXT(": base = capped stat")), ASC->GetNumericAttributeBase(Stat.Get<1>()), Layers.StatBase,
					  STAT_SOURCES_TOLERANCE);
			TestEqual(*(Name + TEXT(": current = getter")), ASC->GetNumericAttribute(Stat.Get<1>()), Stat.Get<2>(),
					  STAT_SOURCES_TOLERANCE);
		}
	});

	It("multiplies a buff in by updating the sources effects in place", [this]()
	{
		if (!ASC) return;
		const FGameplayAttribute SpellDamage = UWoRAttributeSet_Stats::GetSpellDamageAttribute();
		const float BaseBefore = ASC->GetNumericAttributeBase(SpellDamage);
		const float Before = ASC->GetNumericAttribute(SpellDamage);
		const TArray<FActiveGameplayEffectHandle> HandlesBefore = SourceEffects();
		Buff(STAT_SOURCES_BUFF_ID, ESkillEffectType::SpellDamageBuff, STAT_SOURCES_BUFF_PERCENT);
		TestTrue(TEXT("The buff raised the getter"), Char->GetEffectiveSpellDamage() > Before);
		TestEqual(TEXT("Current = getter"), ASC->GetNumericAttribute(SpellDamage), Char->GetEffectiveSpellDamage(), STAT_SOURCES_TOLERANCE);
		TestEqual(TEXT("Base unchanged"), ASC->GetNumericAttributeBase(SpellDamage), BaseBefore, STAT_SOURCES_TOLERANCE);
		const TArray<FActiveGameplayEffectHandle> HandlesAfter = SourceEffects();
		TestEqual(TEXT("Still one sources effect per stat"), HandlesAfter.Num(), ComposedCount());
		for (const FActiveGameplayEffectHandle &Handle : HandlesBefore)
		{
			TestTrue(TEXT("Same effect, updated in place"), HandlesAfter.Contains(Handle));
		}
	});

	It("is seen by the attribute compare when a sources effect is missing", [this]()
	{
		if (!ASC) return;
		TestTrue(TEXT("Synced: no mismatch"), UWoRAttributeDebug::GetAttributeMismatchString(Hero).IsEmpty());
		for (const FActiveGameplayEffectHandle &Handle : SourceEffects())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
		TestTrue(TEXT("Missing sources reported"),
				 UWoRAttributeDebug::GetAttributeMismatchString(Hero).Contains(TEXT("stat-sources effects, expected 1")));
	});

	It("re-applies a sources effect that something removed", [this]()
	{
		if (!ASC) return;
		for (const FActiveGameplayEffectHandle &Handle : SourceEffects())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
		Sync->SyncAttributes();
		TestEqual(TEXT("One sources effect per stat again"), CountSourceEffects(), ComposedCount());
		for (const TTuple<ESubStat, FGameplayAttribute, float> &Stat : ComposedStats())
		{
			TestEqual(*(UEnum::GetValueAsString(Stat.Get<0>()) + TEXT(": current = getter")), ASC->GetNumericAttribute(Stat.Get<1>()),
					  Stat.Get<2>(), STAT_SOURCES_TOLERANCE);
		}
	});

	It("clamps at the compose ceiling as the getter does", [this]()
	{
		if (!ASC) return;
		Buff(STAT_SOURCES_HUGE_BUFF_ID, ESkillEffectType::ActionSpeedBuff, STAT_SOURCES_HUGE_BUFF_PERCENT);
		const FGameplayAttribute ActionSpeed = UWoRAttributeSet_Stats::GetActionSpeedAttribute();
		TestEqual(TEXT("Getter at the ceiling"), Char->GetEffectiveActionSpeed(), CombatConstants::STAT_MODIFIER_MAX);
		TestEqual(TEXT("Attribute at the ceiling"), ASC->GetNumericAttribute(ActionSpeed), CombatConstants::STAT_MODIFIER_MAX);
	});

	It("clamps Defense at its own reduction ceiling", [this]()
	{
		if (!ASC) return;
		Char->CharacterData->Defense = STAT_SOURCES_STAT_POINTS;
		Buff(STAT_SOURCES_HUGE_BUFF_ID, ESkillEffectType::DefenseBuff, STAT_SOURCES_HUGE_BUFF_PERCENT);
		const UDamageCalculator *Calc = TestWorld->GetSubsystem<UDamageCalculator>();
		TestEqual(TEXT("Getter at the Defense ceiling"), Calc->GetDefenderFlatDefense(Hero), CombatConstants::DEFENSE_REDUCTION_CEILING);
		TestEqual(TEXT("Attribute at the Defense ceiling"), ASC->GetNumericAttribute(UWoRAttributeSet_Stats::GetDefenseAttribute()),
				  CombatConstants::DEFENSE_REDUCTION_CEILING);
	});

	It("clamps Luck at its gear ceiling and floors it at 0", [this]()
	{
		if (!ASC) return;
		Char->CharacterData->Luck = STAT_SOURCES_STAT_POINTS;
		Buff(STAT_SOURCES_HUGE_BUFF_ID, ESkillEffectType::LuckBuff, STAT_SOURCES_HUGE_BUFF_PERCENT);
		const FGameplayAttribute Luck = UWoRAttributeSet_Stats::GetLuckAttribute();
		TestEqual(TEXT("Getter at the Luck ceiling"), Char->GetEquipmentModifiedLuck(), CombatConstants::LUCK_GEAR_CEILING);
		TestEqual(TEXT("Attribute at the Luck ceiling"), ASC->GetNumericAttribute(Luck), CombatConstants::LUCK_GEAR_CEILING);

		Buff(STAT_SOURCES_DEBUFF_ID, ESkillEffectType::LuckDebuff, STAT_SOURCES_HUGE_DEBUFF_PERCENT);
		TestEqual(TEXT("Getter floored at 0"), Char->GetEquipmentModifiedLuck(), 0.0f);
		TestEqual(TEXT("Attribute floored at 0"), ASC->GetNumericAttribute(Luck), 0.0f);
	});

	It("leaves the Reflex factor unclamped, as its getter is", [this]()
	{
		if (!ASC) return;
		Buff(STAT_SOURCES_HUGE_BUFF_ID, ESkillEffectType::ReflexBuff, STAT_SOURCES_HUGE_BUFF_PERCENT);
		const float Getter = Char->ReflexWindowGearFactor();
		TestTrue(TEXT("Getter past the default ceiling"), Getter > CombatConstants::STAT_MODIFIER_MAX);
		TestEqual(TEXT("Attribute = getter"), ASC->GetNumericAttribute(UWoRAttributeSet_Stats::GetReflexAttribute()), Getter,
				  STAT_SOURCES_TOLERANCE);
	});

	It("floors crit damage at a normal hit under a large debuff", [this]()
	{
		if (!ASC) return;
		Buff(STAT_SOURCES_DEBUFF_ID, ESkillEffectType::CritDamageDebuff, STAT_SOURCES_HUGE_BUFF_PERCENT);
		const UDamageCalculator *Calc = TestWorld->GetSubsystem<UDamageCalculator>();
		TestEqual(TEXT("Getter at a normal hit"), Calc->GetCritDamageMultiplier(Hero), CombatConstants::CRIT_DMG_BASE);
		TestEqual(TEXT("Attribute at a normal hit"), ASC->GetNumericAttribute(UWoRAttributeSet_Stats::GetCritDamageAttribute()),
				  CombatConstants::CRIT_DMG_BASE);
	});

	It("leaves every other attribute equal to its getter", [this]()
	{
		if (!ASC) return;
		Buff(STAT_SOURCES_BUFF_ID, ESkillEffectType::SpellDamageBuff, STAT_SOURCES_BUFF_PERCENT);
		for (const FWoRAttributeSourceValue &Source : UWoRAttributeSyncComponent::ReadSourceValues(Hero))
		{
			TestEqual(*Source.Attribute.GetName(), ASC->GetNumericAttribute(Source.Attribute), Source.Value, STAT_SOURCES_TOLERANCE);
		}
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
