// WoRAttributeSets.h
// GAS stage 2 (B-041, Design/GASMigration.md): the combatant's stats as GAS attributes.
// Until stage 3 (effects as GameplayEffects) these MIRROR the composed values the
// UCharacterDataComponent / UDamageCalculator getters return (Q-041); UWoRAttributeSyncComponent
// pushes them. Nothing reads them for gameplay yet. Debug: UWoRAttributeDebug (wor.CompareAttributes).

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Combat/Resistance/EWoRDamageType.h"
#include "WoRAttributeSets.generated.h"

#define WOR_ATTRIBUTE_ACCESSORS(ClassName, PropertyName)           \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)               \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)               \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** HP, EP and the status bar. No clamping here: EP may overload above MaxEnergy (Broken Darkness),
 *  and while these mirror the component its own rules already hold. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRAttributeSet_Vitals : public UAttributeSet
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Vitals, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Vitals, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_Energy)
	FGameplayAttributeData Energy;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Vitals, Energy)

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_MaxEnergy)
	FGameplayAttributeData MaxEnergy;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Vitals, MaxEnergy)

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_Buildup)
	FGameplayAttributeData Buildup;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Vitals, Buildup)

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_MaxBuildup)
	FGameplayAttributeData MaxBuildup;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Vitals, MaxBuildup)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Energy(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_MaxEnergy(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Buildup(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_MaxBuildup(const FGameplayAttributeData &OldValue);
};

/** The three pillars (gear-modified) and the twelve composed sub-stats, plus crit chance. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRAttributeSet_Stats : public UAttributeSet
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, Category = "Pillars", ReplicatedUsing = OnRep_Mind)
	FGameplayAttributeData Mind;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, Mind)

	UPROPERTY(BlueprintReadOnly, Category = "Pillars", ReplicatedUsing = OnRep_Body)
	FGameplayAttributeData Body;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, Body)

	UPROPERTY(BlueprintReadOnly, Category = "Pillars", ReplicatedUsing = OnRep_Spirit)
	FGameplayAttributeData Spirit;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, Spirit)

	UPROPERTY(BlueprintReadOnly, Category = "SubStats", ReplicatedUsing = OnRep_Efficiency)
	FGameplayAttributeData Efficiency;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, Efficiency)

	UPROPERTY(BlueprintReadOnly, Category = "SubStats", ReplicatedUsing = OnRep_SpellDamage)
	FGameplayAttributeData SpellDamage;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, SpellDamage)

	UPROPERTY(BlueprintReadOnly, Category = "SubStats", ReplicatedUsing = OnRep_StatusMultiplier)
	FGameplayAttributeData StatusMultiplier;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, StatusMultiplier)

	UPROPERTY(BlueprintReadOnly, Category = "SubStats", ReplicatedUsing = OnRep_CritDamage)
	FGameplayAttributeData CritDamage;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, CritDamage)

	UPROPERTY(BlueprintReadOnly, Category = "SubStats", ReplicatedUsing = OnRep_SpellSpeed)
	FGameplayAttributeData SpellSpeed;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, SpellSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "SubStats", ReplicatedUsing = OnRep_Defense)
	FGameplayAttributeData Defense;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, Defense)

	UPROPERTY(BlueprintReadOnly, Category = "SubStats", ReplicatedUsing = OnRep_ActionSpeed)
	FGameplayAttributeData ActionSpeed;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, ActionSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "SubStats", ReplicatedUsing = OnRep_RawDamage)
	FGameplayAttributeData RawDamage;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, RawDamage)

	UPROPERTY(BlueprintReadOnly, Category = "SubStats", ReplicatedUsing = OnRep_Resistance)
	FGameplayAttributeData Resistance;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, Resistance)

	UPROPERTY(BlueprintReadOnly, Category = "SubStats", ReplicatedUsing = OnRep_TurnSpeed)
	FGameplayAttributeData TurnSpeed;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, TurnSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "SubStats", ReplicatedUsing = OnRep_Luck)
	FGameplayAttributeData Luck;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, Luck)

	/** The reflex window factor gear gives (UCharacterDataComponent::ReflexWindowGearFactor). */
	UPROPERTY(BlueprintReadOnly, Category = "SubStats", ReplicatedUsing = OnRep_Reflex)
	FGameplayAttributeData Reflex;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, Reflex)

	UPROPERTY(BlueprintReadOnly, Category = "Derived", ReplicatedUsing = OnRep_CritChance)
	FGameplayAttributeData CritChance;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Stats, CritChance)

protected:
	UFUNCTION()
	void OnRep_Mind(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Body(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Spirit(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Efficiency(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_SpellDamage(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_StatusMultiplier(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_CritDamage(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_SpellSpeed(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_ActionSpeed(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_RawDamage(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resistance(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_TurnSpeed(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Luck(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Reflex(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_CritChance(const FGameplayAttributeData &OldValue);
};

/** B-216 (ResistanceRedesign.md §3.2): resistance as fractions (0.15 = 15%). Resist_<type> is one
 *  damage type's resistance (WoRResistance::GetTypeResistance), uncapped; Resist_NonElemental the
 *  bucket for buffs keyed to no element; StatusResistance the general buildup resistance (#1-3) and
 *  StatusResistBonus its signed blanket term (#7-8). A mirror until GAS stage 3 (B-042). */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRAttributeSet_Resistance : public UAttributeSet
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

	/** The attribute holding DamageType's resistance (None: Resist_NonElemental). */
	static FGameplayAttribute GetTypeAttribute(EWoRDamageType DamageType);

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Fire)
	FGameplayAttributeData Resist_Fire;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Fire)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Water)
	FGameplayAttributeData Resist_Water;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Water)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Earth)
	FGameplayAttributeData Resist_Earth;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Earth)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Wind)
	FGameplayAttributeData Resist_Wind;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Wind)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Light)
	FGameplayAttributeData Resist_Light;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Light)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Darkness)
	FGameplayAttributeData Resist_Darkness;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Darkness)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Lightning)
	FGameplayAttributeData Resist_Lightning;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Lightning)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Void)
	FGameplayAttributeData Resist_Void;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Void)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Reality)
	FGameplayAttributeData Resist_Reality;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Reality)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Slash)
	FGameplayAttributeData Resist_Slash;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Slash)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Pierce)
	FGameplayAttributeData Resist_Pierce;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Pierce)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Impact)
	FGameplayAttributeData Resist_Impact;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Impact)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_Ice)
	FGameplayAttributeData Resist_Ice;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_Ice)

	UPROPERTY(BlueprintReadOnly, Category = "DamageType", ReplicatedUsing = OnRep_Resist_NonElemental)
	FGameplayAttributeData Resist_NonElemental;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, Resist_NonElemental)

	UPROPERTY(BlueprintReadOnly, Category = "Buildup", ReplicatedUsing = OnRep_StatusResistance)
	FGameplayAttributeData StatusResistance;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, StatusResistance)

	UPROPERTY(BlueprintReadOnly, Category = "Buildup", ReplicatedUsing = OnRep_StatusResistBonus)
	FGameplayAttributeData StatusResistBonus;
	WOR_ATTRIBUTE_ACCESSORS(UWoRAttributeSet_Resistance, StatusResistBonus)

protected:
	UFUNCTION()
	void OnRep_Resist_Fire(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_Water(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_Earth(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_Wind(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_Light(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_Darkness(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_Lightning(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_Void(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_Reality(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_Slash(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_Pierce(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_Impact(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_Ice(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_Resist_NonElemental(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_StatusResistance(const FGameplayAttributeData &OldValue);
	UFUNCTION()
	void OnRep_StatusResistBonus(const FGameplayAttributeData &OldValue);
};
