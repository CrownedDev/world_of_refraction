// WoRAttributeSyncComponent.cpp

#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRAttributeSets.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Combat/CombatConstants.h"
#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Combat/TurnManager.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "GAS/WoRGameplayEffects.h"
#include "Tags/WoRGameplayTags.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "WoRLog.h"

namespace
{
	template <typename TSubsystem>
	TSubsystem *AttributeSyncSubsystem(const UObject *WorldContext)
	{
		const UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<TSubsystem>() : nullptr;
	}

	constexpr float STAT_SOURCES_EFFECT_LEVEL = 1.0f;
	/** Efficiency's derived base: the full cost multiplier before its reduction. */
	constexpr float EFFICIENCY_FULL_COST = 1.0f;

	bool SameLayers(const FWoRStatLayers &A, const FWoRStatLayers &B)
	{
		return A.StatBase == B.StatBase && A.GearFactor == B.GearFactor && A.StoneFactor == B.StoneFactor &&
			   A.TransientFactor == B.TransientFactor;
	}

	bool SamePoolLayers(const FWoRPoolLayers &A, const FWoRPoolLayers &B)
	{
		return A.StatPortion == B.StatPortion && A.GearAmount == B.GearAmount && A.PercentFactor == B.PercentFactor;
	}
}

UWoRAttributeSyncComponent::UWoRAttributeSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UAbilitySystemComponent *UWoRAttributeSyncComponent::PrepareAbilitySystem(AActor *Owner)
{
	UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	if (!ASC)
	{
		return nullptr;
	}
	// ACombatCharacter creates both sets as default subobjects (the ASC finds them itself). This
	// fallback is for actors that add an ASC at runtime (tests), and only on the authority, so a
	// client never makes its own copies beside the replicated ones.
	if (!Owner->HasAuthority())
	{
		return ASC;
	}
	if (!ASC->GetSet<UWoRAttributeSet_Vitals>())
	{
		ASC->AddAttributeSetSubobject(NewObject<UWoRAttributeSet_Vitals>(Owner));
	}
	if (!ASC->GetSet<UWoRAttributeSet_Stats>())
	{
		ASC->AddAttributeSetSubobject(NewObject<UWoRAttributeSet_Stats>(Owner));
	}
	if (!ASC->GetSet<UWoRAttributeSet_Resistance>())
	{
		ASC->AddAttributeSetSubobject(NewObject<UWoRAttributeSet_Resistance>(Owner));
	}
	// The ASC sets this in its own InitializeComponent; repeated for an ASC added after that ran.
	ASC->InitAbilityActorInfo(Owner, Owner);
	return ASC;
}

bool UWoRAttributeSyncComponent::ReadSyncedAttribute(const UAbilitySystemComponent *ASC, const FGameplayAttribute &Attribute, float &Out)
{
	const AActor *Owner = ASC ? ASC->GetOwner() : nullptr;
	const UCharacterDataComponent *Char = Owner ? Owner->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!Char || !Char->CharacterData || !Owner->FindComponentByClass<UWoRAttributeSyncComponent>() ||
		!ASC->HasAttributeSetForAttribute(Attribute))
	{
		return false;
	}
	Out = ASC->GetNumericAttribute(Attribute);
	return true;
}

TArray<FWoRAttributeSourceValue> UWoRAttributeSyncComponent::ReadSourceValues(const AActor *Actor)
{
	TArray<FWoRAttributeSourceValue> Values;
	const UCharacterDataComponent *Char = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!Char)
	{
		return Values;
	}
	auto Add = [&Values](const FGameplayAttribute &Attribute, float Value) { Values.Add({Attribute, Value}); };

	Add(UWoRAttributeSet_Vitals::GetHealthAttribute(), Char->CurrentHP);
	Add(UWoRAttributeSet_Vitals::GetMaxHealthAttribute(), Char->MaxHP);
	Add(UWoRAttributeSet_Vitals::GetEnergyAttribute(), Char->CurrentEP);
	Add(UWoRAttributeSet_Vitals::GetMaxEnergyAttribute(), Char->MaxEP);
	if (UStatusBuildupManager *Buildup = AttributeSyncSubsystem<UStatusBuildupManager>(Actor))
	{
		AActor *MutableActor = const_cast<AActor *>(Actor);
		Add(UWoRAttributeSet_Vitals::GetBuildupAttribute(), Buildup->GetStatusBarBuildup(MutableActor));
	}
	// The bar's cap, as OnStatusBuildupChanged reports it (GetBuildupToTrigger is the amount still needed).
	Add(UWoRAttributeSet_Vitals::GetMaxBuildupAttribute(), CombatConstants::STATUS_EFFECT_THRESHOLD);

	Add(UWoRAttributeSet_Stats::GetMindAttribute(), Char->GetEvolutionModifiedMind());
	Add(UWoRAttributeSet_Stats::GetBodyAttribute(), Char->GetEvolutionModifiedBody());
	Add(UWoRAttributeSet_Stats::GetSpiritAttribute(), Char->GetEvolutionModifiedSpirit());
	Add(UWoRAttributeSet_Stats::GetEfficiencyAttribute(), Char->GetEffectiveEfficiencyMultiplier());
	Add(UWoRAttributeSet_Stats::GetEfficiencyReductionAttribute(), Char->GetEffectiveEfficiencyReduction());
	Add(UWoRAttributeSet_Stats::GetSpellDamageAttribute(), Char->GetEffectiveSpellDamage());
	Add(UWoRAttributeSet_Stats::GetStatusMultiplierAttribute(), Char->GetEffectiveStatusMultiplier());
	Add(UWoRAttributeSet_Stats::GetSpellSpeedAttribute(), Char->GetEffectiveSpellSpeed());
	Add(UWoRAttributeSet_Stats::GetActionSpeedAttribute(), Char->GetEffectiveActionSpeed());
	Add(UWoRAttributeSet_Stats::GetRawDamageAttribute(), Char->GetEffectiveRawDamage());
	Add(UWoRAttributeSet_Stats::GetResistanceAttribute(), Char->GetEffectiveResistance());
	Add(UWoRAttributeSet_Stats::GetLuckAttribute(), Char->GetEquipmentModifiedLuck());
	Add(UWoRAttributeSet_Stats::GetReflexAttribute(), Char->ReflexWindowGearFactor());
	if (const UDamageCalculator *Calc = AttributeSyncSubsystem<UDamageCalculator>(Actor))
	{
		AActor *MutableActor = const_cast<AActor *>(Actor);
		Add(UWoRAttributeSet_Stats::GetDefenseAttribute(), Calc->GetDefenderFlatDefense(MutableActor));
		Add(UWoRAttributeSet_Stats::GetCritChanceAttribute(), Calc->GetCriticalChance(MutableActor));
		Add(UWoRAttributeSet_Stats::GetCritDamageAttribute(), Calc->GetCritDamageMultiplier(MutableActor));
	}
	// The speed turn order runs this actor at (B-042v).
	if (const UTurnManager *Turns = AttributeSyncSubsystem<UTurnManager>(Actor))
	{
		Add(UWoRAttributeSet_Stats::GetTurnSpeedAttribute(), Turns->GetEffectiveTurnSpeed(Actor));
	}

	// B-216: one resistance per damage type (None is Resist_NonElemental), and the buildup layers.
	AActor *ResistTarget = const_cast<AActor *>(Actor);
	for (const EWoRDamageType DamageType : WoRDamageType::All())
	{
		Add(UWoRAttributeSet_Resistance::GetTypeAttribute(DamageType), WoRResistance::GetTypeResistance(ResistTarget, DamageType));
	}
	Add(UWoRAttributeSet_Resistance::GetStatusResistanceAttribute(), WoRResistance::GetGeneralStatusResistance(ResistTarget));
	Add(UWoRAttributeSet_Resistance::GetStatusResistBonusAttribute(), WoRResistance::GetStatusResistBonus(ResistTarget));
	return Values;
}

void UWoRAttributeSyncComponent::SyncAttributes()
{
	// The authority owns the values; clients receive them by replication.
	UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!ASC || !GetOwner()->HasAuthority())
	{
		return;
	}
	const UCharacterDataComponent *Char = GetOwner()->FindComponentByClass<UCharacterDataComponent>();
	const UTurnManager *Turns = AttributeSyncSubsystem<UTurnManager>(this);
	const bool bResistanceSet = ASC->GetSet<UWoRAttributeSet_Resistance>() != nullptr;
	for (const FWoRAttributeSourceValue &Source : ReadSourceValues(GetOwner()))
	{
		if (bResistanceSet && UWoRGE_ResistanceSources::CoversAttribute(Source.Attribute))
		{
			ASC->SetNumericAttributeBase(Source.Attribute, 0.0f); // the sources add it all (SyncResistanceSources)
			continue;
		}
		ESubStat Stat;
		EWorldPillar Pillar;
		FWoRStatLayers Layers;
		const bool bComposed = Char && UWoRAttributeSet_Stats::GetComposedStat(Source.Attribute, Stat);
		if (bComposed && Char->GetStatLayers(Stat, Layers))
		{
			SyncComposedStat(*ASC, Source.Attribute, Layers, UWoRGE_StatSources::GetClassFor(Stat));
			continue;
		}
		const bool bPillar = Char && UWoRAttributeSet_Stats::GetComposedPillar(Source.Attribute, Pillar);
		if (bPillar && Char->GetPillarLayers(Pillar, Layers))
		{
			SyncComposedStat(*ASC, Source.Attribute, Layers, UWoRGE_StatSources::GetClassFor(Pillar));
			continue;
		}
		const bool bStatusResistance = Source.Attribute == UWoRAttributeSet_Resistance::GetStatusResistanceAttribute();
		if (bStatusResistance && WoRResistance::GetGeneralStatusResistanceLayers(GetOwner(), Layers))
		{
			SyncComposedStat(*ASC, Source.Attribute, Layers, UWoRGE_StatSources_StatusResistance::StaticClass());
			continue;
		}
		const bool bTurnSpeed = Source.Attribute == UWoRAttributeSet_Stats::GetTurnSpeedAttribute();
		if (bTurnSpeed && Turns && Turns->GetTurnSpeedLayers(GetOwner(), Layers))
		{
			SyncComposedStat(*ASC, Source.Attribute, Layers, UWoRGE_StatSources::GetClassFor(ESubStat::TurnSpeed));
			continue;
		}
		const TSubclassOf<UGameplayEffect> PoolClass = UWoRGE_PoolSources::GetClassFor(Source.Attribute);
		FWoRPoolLayers PoolLayers;
		const bool bMaxHealth = Source.Attribute == UWoRAttributeSet_Vitals::GetMaxHealthAttribute();
		if (PoolClass && Char && (bMaxHealth ? Char->GetMaxHPLayers(PoolLayers) : Char->GetMaxEPLayers(PoolLayers)))
		{
			SyncPoolStat(*ASC, Source.Attribute, PoolLayers, PoolClass);
			continue;
		}
		// Not composed (or no layers, e.g. Defense without CharacterData, a max set another way): the
		// getter's value, no sources.
		if (bComposed || bPillar || PoolClass || bTurnSpeed || bStatusResistance)
		{
			RemoveStatSources(*ASC, Source.Attribute);
		}
		if (const TSubclassOf<UGameplayEffect> DerivedClass = UWoRGE_DerivedStat::GetClassFor(Source.Attribute))
		{
			SyncDerivedStat(*ASC, Source.Attribute, DerivedClass);
			continue;
		}
		ASC->SetNumericAttributeBase(Source.Attribute, Source.Value);
	}
	if (bResistanceSet)
	{
		SyncResistanceSources(*ASC);
	}
}

TArray<TPair<TSubclassOf<UGameplayEffect>, TMap<FGameplayTag, float>>> UWoRAttributeSyncComponent::GetResistanceSourceMagnitudes(
	AActor *Owner)
{
	TMap<FGameplayTag, float> Effect;
	TMap<FGameplayTag, float> Class;
	TMap<FGameplayTag, float> Gear;
	TMap<FGameplayTag, float> Boss;
	for (const EWoRDamageType DamageType : WoRDamageType::All())
	{
		const WoRResistance::FTypeResistanceSources Sources = WoRResistance::GetTypeResistanceSources(Owner, DamageType);
		const FGameplayTag Tag = WoRTags::DamageTypeToTag(DamageType);
		Effect.Add(Tag, Sources.Effect);
		Class.Add(Tag, Sources.Class);
		Gear.Add(Tag, Sources.Gear);
	}
	const WoRResistance::FStatusResistBonusSources Bonus = WoRResistance::GetStatusResistBonusSources(Owner);
	Effect.Add(WoRTags::SetByCaller_Magnitude, Bonus.Effect);
	Boss.Add(WoRTags::SetByCaller_Magnitude, Bonus.Boss);

	return {
		MakeTuple(TSubclassOf<UGameplayEffect>(UWoRGE_ResistanceSources_Effect::StaticClass()), MoveTemp(Effect)),
		MakeTuple(TSubclassOf<UGameplayEffect>(UWoRGE_ResistanceSources_Class::StaticClass()), MoveTemp(Class)),
		MakeTuple(TSubclassOf<UGameplayEffect>(UWoRGE_ResistanceSources_Gear::StaticClass()), MoveTemp(Gear)),
		MakeTuple(TSubclassOf<UGameplayEffect>(UWoRGE_ResistanceSources_Boss::StaticClass()), MoveTemp(Boss)),
	};
}

void UWoRAttributeSyncComponent::SyncResistanceSources(UAbilitySystemComponent &ASC)
{
	for (const TPair<TSubclassOf<UGameplayEffect>, TMap<FGameplayTag, float>> &Source : GetResistanceSourceMagnitudes(GetOwner()))
	{
		SyncResistanceSource(ASC, Source.Key, Source.Value);
	}
}

void UWoRAttributeSyncComponent::SyncResistanceSource(UAbilitySystemComponent &ASC, TSubclassOf<UGameplayEffect> SourceClass,
													  const TMap<FGameplayTag, float> &Magnitudes)
{
	FWoRAppliedResistanceSources &Applied = AppliedResistanceSources.FindOrAdd(SourceClass.Get());
	const bool bLive = ASC.GetActiveGameplayEffect(Applied.Handle) != nullptr;
	if (bLive && Applied.Magnitudes.OrderIndependentCompareEqual(Magnitudes))
	{
		return;
	}
	if (bLive)
	{
		ASC.UpdateActiveGameplayEffectSetByCallerMagnitudes(Applied.Handle, Magnitudes);
		Applied.Magnitudes = Magnitudes;
		UE_LOG(LogWoR, Verbose, TEXT("[AttributeSync] %s %s updated"), *GetOwner()->GetName(), *SourceClass->GetName());
		return;
	}
	const FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(SourceClass, STAT_SOURCES_EFFECT_LEVEL, ASC.MakeEffectContext());
	if (!Spec.IsValid())
	{
		UE_LOG(LogWoR, Warning, TEXT("[AttributeSync] %s: no spec for %s; resistances miss that source"), *GetOwner()->GetName(),
			   *SourceClass->GetName());
		AppliedResistanceSources.Remove(SourceClass.Get());
		return;
	}
	for (const TPair<FGameplayTag, float> &Magnitude : Magnitudes)
	{
		Spec.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value);
	}
	Applied.Handle = ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	Applied.Magnitudes = Magnitudes;
	UE_LOG(LogWoR, Verbose, TEXT("[AttributeSync] %s %s applied"), *GetOwner()->GetName(), *SourceClass->GetName());
}

void UWoRAttributeSyncComponent::SyncDerivedStat(UAbilitySystemComponent &ASC, const FGameplayAttribute &Attribute,
												 TSubclassOf<UGameplayEffect> DerivedClass)
{
	// Crit chance = (CRIT_CHANCE_BASE + Luck x bonus) x buff layer; Efficiency = 1 - EfficiencyReduction.
	// Without CharacterData GetCriticalChance skips the buff layer, so the GAS one stays at 1.
	const bool bCritChance = Attribute == UWoRAttributeSet_Stats::GetCritChanceAttribute();
	ASC.SetNumericAttributeBase(Attribute, bCritChance ? CombatConstants::CRIT_CHANCE_BASE : EFFICIENCY_FULL_COST);
	float Transient = 1.0f;
	const UCharacterDataComponent *Char = GetOwner()->FindComponentByClass<UCharacterDataComponent>();
	const UDamageCalculator *Calc = AttributeSyncSubsystem<UDamageCalculator>(this);
	if (bCritChance && Calc && Char && Char->CharacterData)
	{
		Transient = Calc->GetCritChanceTransientFactor(GetOwner());
	}

	FWoRAppliedDerivedStat &Applied = AppliedDerivedStats.FindOrAdd(Attribute);
	if (ASC.GetActiveGameplayEffect(Applied.Handle))
	{
		if (bCritChance && Applied.Transient != Transient)
		{
			ASC.UpdateActiveGameplayEffectSetByCallerMagnitude(Applied.Handle, WoRTags::SetByCaller_Stat_Transient, Transient);
			Applied.Transient = Transient;
		}
		return;
	}
	const FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(DerivedClass, STAT_SOURCES_EFFECT_LEVEL, ASC.MakeEffectContext());
	if (!Spec.IsValid())
	{
		AppliedDerivedStats.Remove(Attribute);
		return;
	}
	if (bCritChance)
	{
		Spec.Data->SetSetByCallerMagnitude(WoRTags::SetByCaller_Stat_Transient, Transient);
	}
	Applied.Handle = ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	Applied.Transient = Transient;
	UE_LOG(LogWoR, Verbose, TEXT("[AttributeSync] %s %s derived (buff layer %.4f)"), *GetOwner()->GetName(), *Attribute.GetName(),
		   Transient);
}

void UWoRAttributeSyncComponent::RemoveStatSources(UAbilitySystemComponent &ASC, const FGameplayAttribute &Attribute)
{
	FWoRAppliedStatSources Applied;
	if (AppliedStatSources.RemoveAndCopyValue(Attribute, Applied))
	{
		ASC.RemoveActiveGameplayEffect(Applied.Handle);
	}
	FWoRAppliedPoolSources AppliedPool;
	if (AppliedPoolSources.RemoveAndCopyValue(Attribute, AppliedPool))
	{
		ASC.RemoveActiveGameplayEffect(AppliedPool.Handle);
	}
}

void UWoRAttributeSyncComponent::SyncPoolStat(UAbilitySystemComponent &ASC, const FGameplayAttribute &Attribute,
											  const FWoRPoolLayers &Layers, TSubclassOf<UGameplayEffect> PoolClass)
{
	ASC.SetNumericAttributeBase(Attribute, Layers.StatPortion);

	FWoRAppliedPoolSources &Applied = AppliedPoolSources.FindOrAdd(Attribute);
	const bool bLive = ASC.GetActiveGameplayEffect(Applied.Handle) != nullptr;
	if (bLive && SamePoolLayers(Applied.Layers, Layers))
	{
		return;
	}
	if (bLive)
	{
		ASC.UpdateActiveGameplayEffectSetByCallerMagnitudes(Applied.Handle, {
			{WoRTags::SetByCaller_Stat_Gear, Layers.GearAmount},
			{WoRTags::SetByCaller_Stat_Stone, Layers.PercentFactor},
		});
		Applied.Layers = Layers;
		UE_LOG(LogWoR, Verbose, TEXT("[AttributeSync] %s %s pool sources updated: %s"), *GetOwner()->GetName(), *Attribute.GetName(),
			   *Layers.ToString());
		return;
	}
	const FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(PoolClass, STAT_SOURCES_EFFECT_LEVEL, ASC.MakeEffectContext());
	if (!Spec.IsValid())
	{
		// No effect to carry the layers, so the base holds the composed max.
		ASC.SetNumericAttributeBase(Attribute, Layers.Compose());
		AppliedPoolSources.Remove(Attribute);
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(WoRTags::SetByCaller_Stat_Gear, Layers.GearAmount);
	Spec.Data->SetSetByCallerMagnitude(WoRTags::SetByCaller_Stat_Stone, Layers.PercentFactor);
	Applied.Handle = ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	Applied.Layers = Layers;
	UE_LOG(LogWoR, Verbose, TEXT("[AttributeSync] %s %s pool sources: %s"), *GetOwner()->GetName(), *Attribute.GetName(),
		   *Layers.ToString());
}

void UWoRAttributeSyncComponent::SyncComposedStat(UAbilitySystemComponent &ASC, const FGameplayAttribute &Attribute,
												  const FWoRStatLayers &Layers, TSubclassOf<UGameplayEffect> SourcesClass)
{
	ASC.SetNumericAttributeBase(Attribute, Layers.StatBase);

	FWoRAppliedStatSources &Applied = AppliedStatSources.FindOrAdd(Attribute);
	const bool bLive = ASC.GetActiveGameplayEffect(Applied.Handle) != nullptr;
	if (bLive && SameLayers(Applied.Layers, Layers))
	{
		return;
	}
	if (bLive)
	{
		ASC.UpdateActiveGameplayEffectSetByCallerMagnitudes(Applied.Handle, {
			{WoRTags::SetByCaller_Stat_Gear, Layers.GearFactor},
			{WoRTags::SetByCaller_Stat_Stone, Layers.StoneFactor},
			{WoRTags::SetByCaller_Stat_Transient, Layers.TransientFactor},
		});
		Applied.Layers = Layers;
		UE_LOG(LogWoR, Verbose, TEXT("[AttributeSync] %s %s sources updated: %s"), *GetOwner()->GetName(), *Attribute.GetName(),
			   *Layers.ToString());
		return;
	}
	const FGameplayEffectSpecHandle Spec = ASC.MakeOutgoingSpec(SourcesClass, STAT_SOURCES_EFFECT_LEVEL, ASC.MakeEffectContext());
	if (!Spec.IsValid())
	{
		Applied = FWoRAppliedStatSources();
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(WoRTags::SetByCaller_Stat_Gear, Layers.GearFactor);
	Spec.Data->SetSetByCallerMagnitude(WoRTags::SetByCaller_Stat_Stone, Layers.StoneFactor);
	Spec.Data->SetSetByCallerMagnitude(WoRTags::SetByCaller_Stat_Transient, Layers.TransientFactor);
	Applied.Handle = ASC.ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	Applied.Layers = Layers;
	UE_LOG(LogWoR, Verbose, TEXT("[AttributeSync] %s %s sources: %s"), *GetOwner()->GetName(), *Attribute.GetName(),
		   *Layers.ToString());
}

void UWoRAttributeSyncComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!PrepareAbilitySystem(GetOwner()))
	{
		UE_LOG(LogWoR, Warning, TEXT("[AttributeSync] %s has no AbilitySystemComponent; attributes not mirrored"), *GetOwner()->GetName());
		return;
	}
	BindSources();
	SyncAttributes();
}

void UWoRAttributeSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindSources();
	if (UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		for (const TPair<FGameplayAttribute, FWoRAppliedStatSources> &Applied : AppliedStatSources)
		{
			ASC->RemoveActiveGameplayEffect(Applied.Value.Handle);
		}
		for (const TPair<FGameplayAttribute, FWoRAppliedDerivedStat> &Applied : AppliedDerivedStats)
		{
			ASC->RemoveActiveGameplayEffect(Applied.Value.Handle);
		}
		for (const TPair<FGameplayAttribute, FWoRAppliedPoolSources> &Applied : AppliedPoolSources)
		{
			ASC->RemoveActiveGameplayEffect(Applied.Value.Handle);
		}
		for (const TPair<const UClass *, FWoRAppliedResistanceSources> &Applied : AppliedResistanceSources)
		{
			ASC->RemoveActiveGameplayEffect(Applied.Value.Handle);
		}
	}
	AppliedStatSources.Reset();
	AppliedPoolSources.Reset();
	AppliedResistanceSources.Reset();
	AppliedDerivedStats.Reset();
	Super::EndPlay(EndPlayReason);
}

void UWoRAttributeSyncComponent::BindSources()
{
	if (UCharacterDataComponent *Char = GetOwner()->FindComponentByClass<UCharacterDataComponent>())
	{
		Char->OnHPChanged.AddUniqueDynamic(this, &UWoRAttributeSyncComponent::HandleVitalChanged);
		Char->OnEPChanged.AddUniqueDynamic(this, &UWoRAttributeSyncComponent::HandleVitalChanged);
	}
	if (ULoadoutComponent *Loadout = GetOwner()->FindComponentByClass<ULoadoutComponent>())
	{
		Loadout->OnLoadoutChanged.AddUniqueDynamic(this, &UWoRAttributeSyncComponent::HandleLoadoutChanged);
	}
	if (USkillEffectManager *SEM = AttributeSyncSubsystem<USkillEffectManager>(this))
	{
		SEM->OnEffectApplied.AddUniqueDynamic(this, &UWoRAttributeSyncComponent::HandleEffectChanged);
		SEM->OnEffectRemoved.AddUniqueDynamic(this, &UWoRAttributeSyncComponent::HandleEffectChanged);
		SEM->OnEffectStacksChanged.AddUniqueDynamic(this, &UWoRAttributeSyncComponent::HandleEffectStacksChanged);
	}
	if (UStatusBuildupManager *Buildup = AttributeSyncSubsystem<UStatusBuildupManager>(this))
	{
		Buildup->OnStatusBuildupChanged.AddUniqueDynamic(this, &UWoRAttributeSyncComponent::HandleBuildupChanged);
	}
	if (UTurnManager *Turns = AttributeSyncSubsystem<UTurnManager>(this))
	{
		Turns->OnTurnStarted.AddUniqueDynamic(this, &UWoRAttributeSyncComponent::HandleTurnStarted);
		// A loadout change re-caches turn speed after it broadcasts, and some re-caches have no loadout event.
		Turns->OnSpeedChanged.AddUniqueDynamic(this, &UWoRAttributeSyncComponent::HandleSpeedChanged);
	}
}

void UWoRAttributeSyncComponent::UnbindSources()
{
	// The subsystems outlive this actor; the owner's components go with it.
	if (USkillEffectManager *SEM = AttributeSyncSubsystem<USkillEffectManager>(this))
	{
		SEM->OnEffectApplied.RemoveDynamic(this, &UWoRAttributeSyncComponent::HandleEffectChanged);
		SEM->OnEffectRemoved.RemoveDynamic(this, &UWoRAttributeSyncComponent::HandleEffectChanged);
		SEM->OnEffectStacksChanged.RemoveDynamic(this, &UWoRAttributeSyncComponent::HandleEffectStacksChanged);
	}
	if (UStatusBuildupManager *Buildup = AttributeSyncSubsystem<UStatusBuildupManager>(this))
	{
		Buildup->OnStatusBuildupChanged.RemoveDynamic(this, &UWoRAttributeSyncComponent::HandleBuildupChanged);
	}
	if (UTurnManager *Turns = AttributeSyncSubsystem<UTurnManager>(this))
	{
		Turns->OnTurnStarted.RemoveDynamic(this, &UWoRAttributeSyncComponent::HandleTurnStarted);
		Turns->OnSpeedChanged.RemoveDynamic(this, &UWoRAttributeSyncComponent::HandleSpeedChanged);
	}
}

void UWoRAttributeSyncComponent::HandleVitalChanged(int32 Current, int32 Max)
{
	SyncAttributes();
}

void UWoRAttributeSyncComponent::HandleEffectChanged(AActor *Target, const FActiveSkillEffect &Effect)
{
	if (Target == GetOwner())
	{
		SyncAttributes();
	}
}

void UWoRAttributeSyncComponent::HandleEffectStacksChanged(AActor *Target, const FActiveSkillEffect &Effect, int32 NewStacks)
{
	if (Target == GetOwner())
	{
		SyncAttributes();
	}
}

void UWoRAttributeSyncComponent::HandleBuildupChanged(AActor *Target, float CurrentBuildup, float MaxBuildup, ESpellElement PendingElement, AActor *Source)
{
	if (Target == GetOwner())
	{
		SyncAttributes();
	}
}

void UWoRAttributeSyncComponent::HandleLoadoutChanged(int32 NewLoadoutIndex)
{
	SyncAttributes();
}

void UWoRAttributeSyncComponent::HandleTurnStarted(AActor *Actor, int32 TurnNumber)
{
	if (Actor == GetOwner())
	{
		SyncAttributes();
	}
}

void UWoRAttributeSyncComponent::HandleSpeedChanged(AActor *Actor)
{
	if (Actor == GetOwner())
	{
		SyncAttributes();
	}
}
