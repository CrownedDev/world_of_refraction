// WoREffectSpecBuilder.cpp

#include "GAS/WoREffectSpecBuilder.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GAS/WoRGameplayEffects.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Tags/WoRGameplayTags.h"
#include "WoRLog.h"

namespace
{
	constexpr float BUILT_EFFECT_LEVEL = 1.0f;

	/** Types whose value is summed by a reader (GetTotalStatModifier / GetNetStatModifier). */
	const ESkillEffectType STAT_MODIFIER_TYPES[] = {
		ESkillEffectType::MindBuff, ESkillEffectType::MindDebuff,
		ESkillEffectType::BodyBuff, ESkillEffectType::BodyDebuff,
		ESkillEffectType::SpiritBuff, ESkillEffectType::SpiritDebuff,
		ESkillEffectType::EfficiencyBuff, ESkillEffectType::EfficiencyDebuff,
		ESkillEffectType::SpellCostBuff, ESkillEffectType::SpellCostDebuff,
		ESkillEffectType::SpellDamageBuff, ESkillEffectType::SpellDamageDebuff,
		ESkillEffectType::StatusMultiplierBuff, ESkillEffectType::StatusMultiplierDebuff,
		ESkillEffectType::CritChanceBuff, ESkillEffectType::CritChanceDebuff,
		ESkillEffectType::CritDamageDebuff, ESkillEffectType::ModifyCritDamage,
		ESkillEffectType::SpellSpeedBuff, ESkillEffectType::SpellSpeedDebuff,
		ESkillEffectType::DefenseBuff, ESkillEffectType::DefenseDebuff,
		ESkillEffectType::ActionSpeedBuff, ESkillEffectType::ActionSpeedDebuff,
		ESkillEffectType::RawDamageBuff, ESkillEffectType::RawDamageDebuff,
		ESkillEffectType::MaxHPBuff, ESkillEffectType::MaxHPDebuff,
		ESkillEffectType::MaxEnergyBuff, ESkillEffectType::MaxEnergyDebuff,
		ESkillEffectType::TurnSpeedBuff, ESkillEffectType::TurnSpeedDebuff, ESkillEffectType::ModifyTurnSpeed,
		ESkillEffectType::LuckBuff, ESkillEffectType::LuckDebuff,
		ESkillEffectType::ReflexBuff, ESkillEffectType::ReflexDebuff,
		ESkillEffectType::ModifyStatusResist,
		ESkillEffectType::DamageBuff, ESkillEffectType::DamageDebuff,
		ESkillEffectType::ModifyDamageDealt, ESkillEffectType::ModifyCritChance, ESkillEffectType::ModifyEnergyCost,
	};

	/** The Grant*Immunity types: tagged (WoRTags::EffectTypeToTag) and not one of the statuses. */
	bool IsImmunityType(ESkillEffectType Type)
	{
		switch (Type)
		{
		case ESkillEffectType::Stun:
		case ESkillEffectType::Silenced:
		case ESkillEffectType::HealBlock:
		case ESkillEffectType::DOT:
			return false;
		default:
			return WoRTags::EffectTypeToTag(Type).IsValid();
		}
	}

	const FWoREffectContext *ContextOf(const FActiveGameplayEffect &Active)
	{
		return FWoREffectContext::Get(Active.Spec.GetContext());
	}

	FGameplayEffectSpecHandle MakeSpec(UAbilitySystemComponent &TargetASC, TSubclassOf<UGameplayEffect> Class,
									   const FWoREffectIdentity &Identity, AActor *Source, float Magnitude)
	{
		FGameplayEffectContextHandle Context = TargetASC.MakeEffectContext();
		Context.AddInstigator(Source, Source);
		if (FWoREffectContext *WoRContext = FWoREffectContext::GetMutable(Context))
		{
			WoRContext->Identity = Identity;
		}
		FGameplayEffectSpecHandle Spec = TargetASC.MakeOutgoingSpec(Class, BUILT_EFFECT_LEVEL, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(WoRTags::SetByCaller_Magnitude, Magnitude);
		}
		return Spec;
	}
}

TSubclassOf<UGameplayEffect> FWoREffectSpecBuilder::GetEffectClass(ESkillEffectType Type)
{
	switch (Type)
	{
	case ESkillEffectType::Stun:
		return UWoRGE_Stun::StaticClass();
	case ESkillEffectType::Silenced:
		return UWoRGE_Silence::StaticClass();
	case ESkillEffectType::HealBlock:
		return UWoRGE_HealBlock::StaticClass();
	case ESkillEffectType::DOT:
		return UWoRGE_DamageOverTime::StaticClass();
	case ESkillEffectType::HealthRestore:
		return UWoRGE_HealOverTime::StaticClass();
	default:
		break;
	}
	if (IsImmunityType(Type))
	{
		return UWoRGE_Immunity::StaticClass();
	}
	for (const ESkillEffectType StatType : STAT_MODIFIER_TYPES)
	{
		if (StatType == Type)
		{
			return UWoRGE_StatModifier::StaticClass();
		}
	}
	return nullptr;
}

FWoREffectIdentity FWoREffectSpecBuilder::MakeIdentity(const FActiveSkillEffect &Effect)
{
	FWoREffectIdentity Identity;
	Identity.EffectType = Effect.EffectType;
	Identity.Element = Effect.Element;
	Identity.EffectID = Effect.EffectID;
	Identity.SourceKey = Effect.SourceKey;
	Identity.bFromGear = Effect.bFromGear;
	return Identity;
}

FWoRBuiltEffect FWoREffectSpecBuilder::Build(UAbilitySystemComponent &TargetASC, const FActiveSkillEffect &Effect, AActor *Source)
{
	FWoRBuiltEffect Built;
	const TSubclassOf<UGameplayEffect> Class = GetEffectClass(Effect.EffectType);
	if (!Class || Effect.IsInstant())
	{
		UE_LOG(LogWoREffects, Verbose, TEXT("[EffectSpecBuilder] %s: not built (%s)"), *Effect.EffectName,
			   Class ? TEXT("instant") : TEXT("type not ported"));
		return Built;
	}

	const FWoREffectIdentity Identity = MakeIdentity(Effect);
	const float Magnitude = Effect.GetStackedValue();
	Built.Spec = MakeSpec(TargetASC, Class, Identity, Source, Magnitude);
	if (!Built.IsValid())
	{
		return Built;
	}
	if (IsImmunityType(Effect.EffectType))
	{
		Built.Spec.Data->DynamicGrantedTags.AddTag(WoRTags::EffectTypeToTag(Effect.EffectType));
	}
	if (Effect.bFromGear)
	{
		Built.Spec.Data->AddDynamicAssetTag(WoRTags::Effect_Origin_Gear);
	}

	// TODO: B-042e - a DOT's tick is its stacked value as flat damage; the enum path's
	// GetDotTickDamage maths (share of max HP, resistance) moves into the damage execution.
	if (Effect.EffectType == ESkillEffectType::DOT)
	{
		Built.TickSpec = MakeSpec(TargetASC, UWoRGE_HealthTick::StaticClass(), Identity, Source, -Magnitude);
	}
	else if (Effect.EffectType == ESkillEffectType::HealthRestore)
	{
		Built.TickSpec = MakeSpec(TargetASC, UWoRGE_HealthTick::StaticClass(), Identity, Source, Magnitude);
	}

	Built.bPermanent = Effect.bPermanent;
	Built.Turns = Effect.bCountsHolderTurns ? Effect.HolderTurns.TurnsLeft : Effect.RemainingTurns;
	return Built;
}

FActiveGameplayEffectHandle FWoREffectSpecBuilder::Apply(AActor *Target, const FActiveSkillEffect &Effect, AActor *Source)
{
	UAbilitySystemComponent *ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!ASC)
	{
		return FActiveGameplayEffectHandle();
	}
	const FWoRBuiltEffect Built = Build(*ASC, Effect, Source);
	if (!Built.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	UWoRTurnEffectTracker *Tracker = Target->FindComponentByClass<UWoRTurnEffectTracker>();
	const FActiveGameplayEffectHandle Existing = FindByIdentity(*ASC, MakeIdentity(Effect));
	if (Existing.IsValid())
	{
		// Q-044 A: a re-apply of the same identity replaces it, with a fresh span.
		if (!Tracker || !Tracker->RemoveTracked(Existing))
		{
			ASC->RemoveActiveGameplayEffect(Existing);
		}
	}

	if (Built.bPermanent)
	{
		return ASC->ApplyGameplayEffectSpecToSelf(*Built.Spec.Data.Get());
	}
	if (!Tracker)
	{
		UE_LOG(LogWoREffects, Warning, TEXT("[EffectSpecBuilder] %s has no turn tracker; %s not applied"),
			   *GetNameSafe(Target), *Effect.EffectName);
		return FActiveGameplayEffectHandle();
	}
	return Tracker->ApplySpecForHolderTurns(Built.Spec, Built.Turns, Built.TickSpec);
}

FActiveGameplayEffectHandle FWoREffectSpecBuilder::FindByIdentity(const UAbilitySystemComponent &ASC, const FWoREffectIdentity &Identity)
{
	const FGameplayEffectQuery Query(FActiveGameplayEffectQueryCustomMatch::CreateLambda(
		[&Identity](const FActiveGameplayEffect &Active)
		{
			const FWoREffectContext *Context = ContextOf(Active);
			return Context && Context->Identity.IsSameIdentity(Identity);
		}));
	const TArray<FActiveGameplayEffectHandle> Found = ASC.GetActiveEffects(Query);
	return Found.Num() > 0 ? Found[0] : FActiveGameplayEffectHandle();
}

float FWoREffectSpecBuilder::SumMagnitude(const UAbilitySystemComponent &ASC, ESkillEffectType Type)
{
	const FGameplayEffectQuery Query(FActiveGameplayEffectQueryCustomMatch::CreateLambda(
		[Type](const FActiveGameplayEffect &Active)
		{
			const FWoREffectContext *Context = ContextOf(Active);
			return Context && Context->Identity.EffectType == Type;
		}));
	float Total = 0.0f;
	for (const FActiveGameplayEffectHandle &Handle : ASC.GetActiveEffects(Query))
	{
		if (const FActiveGameplayEffect *Active = ASC.GetActiveGameplayEffect(Handle))
		{
			Total += Active->Spec.GetSetByCallerMagnitude(WoRTags::SetByCaller_Magnitude, false);
		}
	}
	return Total;
}

FString FWoREffectSpecBuilder::GetDebugString(const UAbilitySystemComponent &ASC)
{
	const FGameplayEffectQuery Everything(FActiveGameplayEffectQueryCustomMatch::CreateLambda(
		[](const FActiveGameplayEffect &) { return true; }));
	TArray<FString> Lines;
	for (const FActiveGameplayEffectHandle &Handle : ASC.GetActiveEffects(Everything))
	{
		if (const FActiveGameplayEffect *Active = ASC.GetActiveGameplayEffect(Handle))
		{
			const FWoREffectContext *Context = ContextOf(*Active);
			Lines.Add(FString::Printf(TEXT("%s [%s] magnitude=%.2f"), *GetNameSafe(Active->Spec.Def.Get()),
									  Context ? *Context->Identity.ToString() : TEXT("no WoR context"),
									  Active->Spec.GetSetByCallerMagnitude(WoRTags::SetByCaller_Magnitude, false)));
		}
	}
	return Lines.Num() > 0 ? FString::Join(Lines, TEXT("\n")) : TEXT("no active GameplayEffects");
}
