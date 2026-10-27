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

	/** The Pattern P stats composed on GAS (B-042p), and their attributes. */
	const TPair<ESubStat, FGameplayAttribute> *GetComposedStats(int32 &OutCount)
	{
		static const TPair<ESubStat, FGameplayAttribute> ComposedStats[] = {
			{ESubStat::SpellDamage, UWoRAttributeSet_Stats::GetSpellDamageAttribute()},
			{ESubStat::RawDamage, UWoRAttributeSet_Stats::GetRawDamageAttribute()},
			{ESubStat::ActionSpeed, UWoRAttributeSet_Stats::GetActionSpeedAttribute()},
			{ESubStat::SpellSpeed, UWoRAttributeSet_Stats::GetSpellSpeedAttribute()},
		};
		OutCount = UE_ARRAY_COUNT(ComposedStats);
		return ComposedStats;
	}

	bool SameLayers(const FWoRStatLayers &A, const FWoRStatLayers &B)
	{
		return A.StatBase == B.StatBase && A.GearFactor == B.GearFactor && A.StoneFactor == B.StoneFactor &&
			   A.TransientFactor == B.TransientFactor;
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
	// Turn order's stat x buff layer, as UStatModifierDebug's snapshot composes it.
	const USkillEffectManager *SEM = AttributeSyncSubsystem<USkillEffectManager>(Actor);
	if (SEM && Char->CharacterData)
	{
		Add(UWoRAttributeSet_Stats::GetTurnSpeedAttribute(),
			Char->CharacterData->CalculateTurnSpeedWithSpirit(Char->GetEvolutionModifiedSpirit()) *
				(1.0f + SEM->GetNetStatModifier(const_cast<AActor *>(Actor), ESubStat::TurnSpeed) / CombatConstants::STAT_PERCENT_DIVISOR));
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
	int32 ComposedCount = 0;
	const TPair<ESubStat, FGameplayAttribute> *ComposedStats = GetComposedStats(ComposedCount);
	const UCharacterDataComponent *Char = GetOwner()->FindComponentByClass<UCharacterDataComponent>();
	for (const FWoRAttributeSourceValue &Source : ReadSourceValues(GetOwner()))
	{
		bool bComposed = false;
		for (int32 Index = 0; Index < ComposedCount && Char; ++Index)
		{
			if (ComposedStats[Index].Value == Source.Attribute)
			{
				FWoRStatLayers Layers;
				Char->GetStatLayers(ComposedStats[Index].Key, Layers);
				SyncComposedStat(*ASC, ComposedStats[Index].Key, Source.Attribute, Layers);
				bComposed = true;
				break;
			}
		}
		if (!bComposed)
		{
			ASC->SetNumericAttributeBase(Source.Attribute, Source.Value);
		}
	}
}

void UWoRAttributeSyncComponent::SyncComposedStat(UAbilitySystemComponent &ASC, ESubStat Stat, const FGameplayAttribute &Attribute,
												  const FWoRStatLayers &Layers)
{
	ASC.SetNumericAttributeBase(Attribute, Layers.StatBase);

	FWoRAppliedStatSources &Applied = AppliedStatSources.FindOrAdd(Stat);
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
		UE_LOG(LogWoR, Verbose, TEXT("[AttributeSync] %s %s sources updated: %s"), *GetOwner()->GetName(),
			   *UEnum::GetValueAsString(Stat), *Layers.ToString());
		return;
	}
	const FGameplayEffectSpecHandle Spec =
		ASC.MakeOutgoingSpec(UWoRGE_StatSources::GetClassFor(Stat), STAT_SOURCES_EFFECT_LEVEL, ASC.MakeEffectContext());
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
	UE_LOG(LogWoR, Verbose, TEXT("[AttributeSync] %s %s sources: %s"), *GetOwner()->GetName(), *UEnum::GetValueAsString(Stat),
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
		for (const TPair<ESubStat, FWoRAppliedStatSources> &Applied : AppliedStatSources)
		{
			ASC->RemoveActiveGameplayEffect(Applied.Value.Handle);
		}
	}
	AppliedStatSources.Reset();
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
