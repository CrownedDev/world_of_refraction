// WoRGameplayEffects.cpp

#include "GAS/WoRGameplayEffects.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Combat/CombatConstants.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRDerivedStatMagnitudes.h"
#include "GAS/WoRTickExecution.h"
#include "Tags/WoRGameplayTags.h"

void UWoRGameplayEffect::AddAssetTag(const FGameplayTag &Tag)
{
	UAssetTagsGameplayEffectComponent *AssetTags =
		CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("AssetTags"));
	GEComponents.Add(AssetTags);

	FInheritedTagContainer Tags;
	Tags.Added.AddTag(Tag);
	AssetTags->SetAndApplyAssetTagChanges(Tags);
}

void UWoRGameplayEffect::AddGrantedTag(const FGameplayTag &Tag)
{
	UTargetTagsGameplayEffectComponent *GrantedTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantedTags"));
	GEComponents.Add(GrantedTags);

	FInheritedTagContainer Tags;
	Tags.Added.AddTag(Tag);
	GrantedTags->SetAndApplyTargetTagChanges(Tags);
}

// TODO: B-042c - no stacking policy on the lasting GEs: the facade mirrors the enum path's
// identity / refresh / stack decision (D-012, D-053, Q-044) through FWoREffectSpecBuilder::Apply,
// which replaces the GE with the same identity, so GAS stacking stays off.

UWoRGE_Stun::UWoRGE_Stun()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Status);
	AddGrantedTag(WoRTags::Status_Stun);
}

UWoRGE_Silence::UWoRGE_Silence()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Status);
	AddGrantedTag(WoRTags::Status_Silence);
}

UWoRGE_HealBlock::UWoRGE_HealBlock()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Status);
	AddGrantedTag(WoRTags::Status_HealBlock);
}

UWoRGE_Immunity::UWoRGE_Immunity()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Immunity);
}

UWoRGE_StatModifier::UWoRGE_StatModifier()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_StatModifier);
}

UWoRGE_DamageOverTime::UWoRGE_DamageOverTime()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_DOT);
	AddGrantedTag(WoRTags::Status_DOT);
}

UWoRGE_HealOverTime::UWoRGE_HealOverTime()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_HOT);
}

UWoRGE_HealthTick::UWoRGE_HealthTick()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UWoRTickExecution::StaticClass();
	Executions.Add(Execution);
}

UWoRGE_HealTick::UWoRGE_HealTick()
{
	UTargetTagRequirementsGameplayEffectComponent *Requirements =
		CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TargetTagRequirements"));
	GEComponents.Add(Requirements);
	Requirements->ApplicationTagRequirements.IgnoreTags.AddTag(WoRTags::Status_HealBlock);
}

UWoRGE_Reflect::UWoRGE_Reflect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Reflect);
}

UWoRGE_LastStand::UWoRGE_LastStand()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_LastStand);
}

UWoRGE_OnHit::UWoRGE_OnHit()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_OnHit);
}

UWoRGE_AttackModifier::UWoRGE_AttackModifier()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_AttackModifier);
}

UWoRGE_Control::UWoRGE_Control()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Control);
}

UWoRGE_Periodic::UWoRGE_Periodic()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Periodic);
}

UWoRGE_Action::UWoRGE_Action()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Action);
}

TSubclassOf<UGameplayEffect> UWoRGE_StatSources::GetClassFor(ESubStat Stat)
{
	switch (Stat)
	{
	case ESubStat::SpellDamage: return UWoRGE_StatSources_SpellDamage::StaticClass();
	case ESubStat::RawDamage: return UWoRGE_StatSources_RawDamage::StaticClass();
	case ESubStat::ActionSpeed: return UWoRGE_StatSources_ActionSpeed::StaticClass();
	case ESubStat::SpellSpeed: return UWoRGE_StatSources_SpellSpeed::StaticClass();
	case ESubStat::StatusMultiplier: return UWoRGE_StatSources_StatusMultiplier::StaticClass();
	case ESubStat::Defense: return UWoRGE_StatSources_Defense::StaticClass();
	case ESubStat::CritDamage: return UWoRGE_StatSources_CritDamage::StaticClass();
	case ESubStat::Luck: return UWoRGE_StatSources_Luck::StaticClass();
	case ESubStat::Reflex: return UWoRGE_StatSources_Reflex::StaticClass();
	case ESubStat::Efficiency: return UWoRGE_StatSources_Efficiency::StaticClass();
	case ESubStat::TurnSpeed: return UWoRGE_StatSources_TurnSpeed::StaticClass();
	default: return nullptr;
	}
}

TSubclassOf<UGameplayEffect> UWoRGE_StatSources::GetClassFor(EWorldPillar Pillar)
{
	switch (Pillar)
	{
	case EWorldPillar::Mind: return UWoRGE_StatSources_Mind::StaticClass();
	case EWorldPillar::Body: return UWoRGE_StatSources_Body::StaticClass();
	case EWorldPillar::Spirit: return UWoRGE_StatSources_Spirit::StaticClass();
	default: return nullptr;
	}
}

void UWoRGE_StatSources::AddSourceModifiers(const FGameplayAttribute &Attribute)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_StatSources);
	// Each layer multiplies the result (MultiplyCompound): base x gear x stone x transient.
	const FGameplayTag Layers[] = {WoRTags::SetByCaller_Stat_Gear, WoRTags::SetByCaller_Stat_Stone, WoRTags::SetByCaller_Stat_Transient};
	for (const FGameplayTag &Layer : Layers)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = Layer;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		Modifiers.Add(Modifier);
	}
}

UWoRGE_StatSources_SpellDamage::UWoRGE_StatSources_SpellDamage()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetSpellDamageAttribute());
}

UWoRGE_StatSources_RawDamage::UWoRGE_StatSources_RawDamage()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetRawDamageAttribute());
}

UWoRGE_StatSources_ActionSpeed::UWoRGE_StatSources_ActionSpeed()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetActionSpeedAttribute());
}

UWoRGE_StatSources_SpellSpeed::UWoRGE_StatSources_SpellSpeed()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetSpellSpeedAttribute());
}

UWoRGE_StatSources_StatusMultiplier::UWoRGE_StatSources_StatusMultiplier()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetStatusMultiplierAttribute());
}

UWoRGE_StatSources_Defense::UWoRGE_StatSources_Defense()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetDefenseAttribute());
}

UWoRGE_StatSources_CritDamage::UWoRGE_StatSources_CritDamage()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetCritDamageAttribute());
}

UWoRGE_StatSources_Luck::UWoRGE_StatSources_Luck()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetLuckAttribute());
}

UWoRGE_StatSources_Reflex::UWoRGE_StatSources_Reflex()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetReflexAttribute());
}

UWoRGE_StatSources_Efficiency::UWoRGE_StatSources_Efficiency()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetEfficiencyReductionAttribute());
}

UWoRGE_StatSources_TurnSpeed::UWoRGE_StatSources_TurnSpeed()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetTurnSpeedAttribute());
}

UWoRGE_StatSources_Mind::UWoRGE_StatSources_Mind()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetMindAttribute());
}

UWoRGE_StatSources_Body::UWoRGE_StatSources_Body()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetBodyAttribute());
}

UWoRGE_StatSources_Spirit::UWoRGE_StatSources_Spirit()
{
	AddSourceModifiers(UWoRAttributeSet_Stats::GetSpiritAttribute());
}

UWoRGE_StatSources_StatusResistance::UWoRGE_StatSources_StatusResistance()
{
	AddSourceModifiers(UWoRAttributeSet_Resistance::GetStatusResistanceAttribute());
}

UWoRGE_ResistanceSources::UWoRGE_ResistanceSources()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_StatSources);
}

namespace
{
	FGameplayModifierInfo AddBaseBySetByCaller(const FGameplayAttribute &Attribute, const FGameplayTag &DataTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = DataTag;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::AddBase;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		return Modifier;
	}
}

TConstArrayView<TSubclassOf<UGameplayEffect>> UWoRGE_ResistanceSources::GetSourceClasses()
{
	static const TSubclassOf<UGameplayEffect> SourceClasses[] = {
		UWoRGE_ResistanceSources_Effect::StaticClass(),
		UWoRGE_ResistanceSources_Class::StaticClass(),
		UWoRGE_ResistanceSources_Gear::StaticClass(),
		UWoRGE_ResistanceSources_Boss::StaticClass(),
	};
	return SourceClasses;
}

bool UWoRGE_ResistanceSources::CoversAttribute(const FGameplayAttribute &Attribute)
{
	if (Attribute == UWoRAttributeSet_Resistance::GetStatusResistBonusAttribute())
	{
		return true;
	}
	for (const EWoRDamageType DamageType : WoRDamageType::All())
	{
		if (Attribute == UWoRAttributeSet_Resistance::GetTypeAttribute(DamageType))
		{
			return true;
		}
	}
	return false;
}

void UWoRGE_ResistanceSources::AddTypeModifiers()
{
	for (const EWoRDamageType DamageType : WoRDamageType::All())
	{
		Modifiers.Add(AddBaseBySetByCaller(UWoRAttributeSet_Resistance::GetTypeAttribute(DamageType), WoRTags::DamageTypeToTag(DamageType)));
	}
}

void UWoRGE_ResistanceSources::AddStatusResistBonusModifier()
{
	Modifiers.Add(AddBaseBySetByCaller(UWoRAttributeSet_Resistance::GetStatusResistBonusAttribute(), WoRTags::SetByCaller_Magnitude));
}

UWoRGE_ResistanceSources_Effect::UWoRGE_ResistanceSources_Effect()
{
	AddTypeModifiers();
	AddStatusResistBonusModifier();
}

UWoRGE_ResistanceSources_Class::UWoRGE_ResistanceSources_Class()
{
	AddTypeModifiers();
}

UWoRGE_ResistanceSources_Gear::UWoRGE_ResistanceSources_Gear()
{
	AddTypeModifiers();
}

UWoRGE_ResistanceSources_Boss::UWoRGE_ResistanceSources_Boss()
{
	AddStatusResistBonusModifier();
}

TSubclassOf<UGameplayEffect> UWoRGE_PoolSources::GetClassFor(const FGameplayAttribute &Attribute)
{
	if (Attribute == UWoRAttributeSet_Vitals::GetMaxHealthAttribute())
	{
		return UWoRGE_PoolSources_MaxHealth::StaticClass();
	}
	if (Attribute == UWoRAttributeSet_Vitals::GetMaxEnergyAttribute())
	{
		return UWoRGE_PoolSources_MaxEnergy::StaticClass();
	}
	return nullptr;
}

void UWoRGE_PoolSources::AddPoolModifiers(const FGameplayAttribute &Attribute)
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_StatSources);
	// (stat portion + gear) x percent: AddBase lands before the multiply in the aggregator.
	const TPair<FGameplayTag, EGameplayModOp::Type> Layers[] = {
		{WoRTags::SetByCaller_Stat_Gear, EGameplayModOp::AddBase},
		{WoRTags::SetByCaller_Stat_Stone, EGameplayModOp::MultiplyCompound},
	};
	for (const TPair<FGameplayTag, EGameplayModOp::Type> &Layer : Layers)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = Layer.Key;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = Layer.Value;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		Modifiers.Add(Modifier);
	}
}

UWoRGE_PoolSources_MaxHealth::UWoRGE_PoolSources_MaxHealth()
{
	AddPoolModifiers(UWoRAttributeSet_Vitals::GetMaxHealthAttribute());
}

UWoRGE_PoolSources_MaxEnergy::UWoRGE_PoolSources_MaxEnergy()
{
	AddPoolModifiers(UWoRAttributeSet_Vitals::GetMaxEnergyAttribute());
}

TSubclassOf<UGameplayEffect> UWoRGE_DerivedStat::GetClassFor(const FGameplayAttribute &Attribute)
{
	if (Attribute == UWoRAttributeSet_Stats::GetCritChanceAttribute())
	{
		return UWoRGE_DerivedCritChance::StaticClass();
	}
	if (Attribute == UWoRAttributeSet_Stats::GetEfficiencyAttribute())
	{
		return UWoRGE_DerivedEfficiency::StaticClass();
	}
	return nullptr;
}

UWoRGE_DerivedStat::UWoRGE_DerivedStat()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_DerivedStat);
}

void UWoRGE_DerivedStat::AddClampedTerm(const FGameplayAttribute &Target, TSubclassOf<UWoRMMC_ClampedStatTerm> Term)
{
	FCustomCalculationBasedFloat Calculation;
	Calculation.CalculationClassMagnitude = Term;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = Target;
	Modifier.ModifierOp = EGameplayModOp::AddBase;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Calculation);
	Modifiers.Add(Modifier);
}

UWoRGE_DerivedCritChance::UWoRGE_DerivedCritChance()
{
	const FGameplayAttribute CritChance = UWoRAttributeSet_Stats::GetCritChanceAttribute();
	AddClampedTerm(CritChance, UWoRMMC_CritChanceLuckTerm::StaticClass());
	FSetByCallerFloat Transient;
	Transient.DataTag = WoRTags::SetByCaller_Stat_Transient;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = CritChance;
	Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Transient);
	Modifiers.Add(Modifier);
}

UWoRGE_DerivedEfficiency::UWoRGE_DerivedEfficiency()
{
	AddClampedTerm(UWoRAttributeSet_Stats::GetEfficiencyAttribute(), UWoRMMC_EfficiencyReductionTerm::StaticClass());
}
