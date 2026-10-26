// WoRAttributeSets.cpp

#include "GAS/WoRAttributeSets.h"
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
