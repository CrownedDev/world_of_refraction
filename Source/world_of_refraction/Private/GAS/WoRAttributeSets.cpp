// WoRAttributeSets.cpp

#include "GAS/WoRAttributeSets.h"
#include "Character/CharacterDataComponent.h"
#include "Character/StatLayers.h"
#include "Net/UnrealNetwork.h"

#define WOR_REPLICATE_ATTRIBUTE(ClassName, PropertyName) \
	DOREPLIFETIME_CONDITION_NOTIFY(ClassName, PropertyName, COND_None, REPNOTIFY_Always)

#define WOR_ATTRIBUTE_REPNOTIFY(ClassName, PropertyName)                              \
	void ClassName::OnRep_##PropertyName(const FGameplayAttributeData &OldValue)     \
	{                                                                                  \
		GAMEPLAYATTRIBUTE_REPNOTIFY(ClassName, PropertyName, OldValue);               \
	}

void UWoRAttributeSet_Vitals::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Vitals, Health);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Vitals, MaxHealth);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Vitals, Energy);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Vitals, MaxEnergy);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Vitals, Buildup);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Vitals, MaxBuildup);
}

WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Vitals, Health)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Vitals, MaxHealth)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Vitals, Energy)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Vitals, MaxEnergy)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Vitals, Buildup)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Vitals, MaxBuildup)

void UWoRAttributeSet_Vitals::PreAttributeChange(const FGameplayAttribute &Attribute, float &NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxEnergyAttribute())
	{
		NewValue = FMath::RoundToFloat(NewValue);
	}
}

void UWoRAttributeSet_Stats::PreAttributeChange(const FGameplayAttribute &Attribute, float &NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	// As Lyra clamps Health here: the stat-sources effect multiplies these past the base (Q-042p).
	ESubStat Stat;
	EWorldPillar Pillar;
	if (GetComposedStat(Attribute, Stat))
	{
		NewValue = FWoRStatLayers::ForStat(Stat).Clamp(NewValue);
	}
	else if (GetComposedPillar(Attribute, Pillar))
	{
		// The pillar modifier is capped alone, so the clamp follows the world base the sync set.
		NewValue = FWoRStatLayers::ForPillar(Attribute.GetGameplayAttributeData(this)->GetBaseValue()).Clamp(NewValue);
	}
	else if (Attribute == GetCritChanceAttribute())
	{
		// A chance; GetCriticalChance clamps the same.
		NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
	}
}

TConstArrayView<TPair<ESubStat, FGameplayAttribute>> UWoRAttributeSet_Stats::GetComposedStats()
{
	static const TPair<ESubStat, FGameplayAttribute> ComposedStats[] = {
		{ESubStat::SpellDamage, GetSpellDamageAttribute()},
		{ESubStat::RawDamage, GetRawDamageAttribute()},
		{ESubStat::ActionSpeed, GetActionSpeedAttribute()},
		{ESubStat::SpellSpeed, GetSpellSpeedAttribute()},
		{ESubStat::StatusMultiplier, GetStatusMultiplierAttribute()},
		{ESubStat::Defense, GetDefenseAttribute()},
		{ESubStat::CritDamage, GetCritDamageAttribute()},
		{ESubStat::Luck, GetLuckAttribute()},
		{ESubStat::Reflex, GetReflexAttribute()},
		{ESubStat::Efficiency, GetEfficiencyReductionAttribute()},
	};
	return ComposedStats;
}

bool UWoRAttributeSet_Stats::GetComposedStat(const FGameplayAttribute &Attribute, ESubStat &OutStat)
{
	for (const TPair<ESubStat, FGameplayAttribute> &Composed : GetComposedStats())
	{
		if (Composed.Value == Attribute)
		{
			OutStat = Composed.Key;
			return true;
		}
	}
	return false;
}

TConstArrayView<TPair<EWorldPillar, FGameplayAttribute>> UWoRAttributeSet_Stats::GetComposedPillars()
{
	static const TPair<EWorldPillar, FGameplayAttribute> ComposedPillars[] = {
		{EWorldPillar::Mind, GetMindAttribute()},
		{EWorldPillar::Body, GetBodyAttribute()},
		{EWorldPillar::Spirit, GetSpiritAttribute()},
	};
	return ComposedPillars;
}

bool UWoRAttributeSet_Stats::GetComposedPillar(const FGameplayAttribute &Attribute, EWorldPillar &OutPillar)
{
	for (const TPair<EWorldPillar, FGameplayAttribute> &Composed : GetComposedPillars())
	{
		if (Composed.Value == Attribute)
		{
			OutPillar = Composed.Key;
			return true;
		}
	}
	return false;
}

void UWoRAttributeSet_Stats::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, Mind);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, Body);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, Spirit);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, Efficiency);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, SpellDamage);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, StatusMultiplier);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, CritDamage);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, SpellSpeed);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, Defense);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, ActionSpeed);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, RawDamage);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, Resistance);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, TurnSpeed);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, Luck);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, Reflex);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, CritChance);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Stats, EfficiencyReduction);
}

WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, Mind)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, Body)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, Spirit)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, Efficiency)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, SpellDamage)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, StatusMultiplier)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, CritDamage)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, SpellSpeed)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, Defense)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, ActionSpeed)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, RawDamage)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, Resistance)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, TurnSpeed)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, Luck)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, Reflex)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, CritChance)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Stats, EfficiencyReduction)

void UWoRAttributeSet_Resistance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Fire);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Water);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Earth);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Wind);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Light);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Darkness);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Lightning);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Void);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Reality);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Slash);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Pierce);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Impact);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_Ice);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, Resist_NonElemental);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, StatusResistance);
	WOR_REPLICATE_ATTRIBUTE(UWoRAttributeSet_Resistance, StatusResistBonus);
}

WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Fire)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Water)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Earth)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Wind)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Light)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Darkness)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Lightning)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Void)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Reality)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Slash)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Pierce)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Impact)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_Ice)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, Resist_NonElemental)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, StatusResistance)
WOR_ATTRIBUTE_REPNOTIFY(UWoRAttributeSet_Resistance, StatusResistBonus)

FGameplayAttribute UWoRAttributeSet_Resistance::GetTypeAttribute(EWoRDamageType DamageType)
{
	switch (DamageType)
	{
	case EWoRDamageType::Fire: return GetResist_FireAttribute();
	case EWoRDamageType::Water: return GetResist_WaterAttribute();
	case EWoRDamageType::Earth: return GetResist_EarthAttribute();
	case EWoRDamageType::Wind: return GetResist_WindAttribute();
	case EWoRDamageType::Light: return GetResist_LightAttribute();
	case EWoRDamageType::Darkness: return GetResist_DarknessAttribute();
	case EWoRDamageType::Lightning: return GetResist_LightningAttribute();
	case EWoRDamageType::Void: return GetResist_VoidAttribute();
	case EWoRDamageType::Reality: return GetResist_RealityAttribute();
	case EWoRDamageType::Slash: return GetResist_SlashAttribute();
	case EWoRDamageType::Pierce: return GetResist_PierceAttribute();
	case EWoRDamageType::Impact: return GetResist_ImpactAttribute();
	case EWoRDamageType::Ice: return GetResist_IceAttribute();
	case EWoRDamageType::None:
	default:
		return GetResist_NonElementalAttribute();
	}
}

#undef WOR_REPLICATE_ATTRIBUTE
#undef WOR_ATTRIBUTE_REPNOTIFY
