// BossPhaseComponent.cpp

#include "Combat/Boss/BossPhaseComponent.h"

#include "Character/CharacterDataComponent.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Conditions/WoRConditions.h"
#include "Reaction/ReactionTrigger.h"
#include "Combat/Mechanics/WeatherStateManager.h"
#include "Combat/TurnManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "HAL/IConsoleManager.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/EffectDefinition.h"
#include "Skills/Effects/EffectIdentity.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "WoRLog.h"

namespace BossPhaseConstants
{
	/** Status resistance gained per status break this fight (Elden Ring's rising resistance).
	 *  TODO: tune. */
	constexpr float STATUS_RESIST_STEP_PER_BREAK = 0.1f;
	constexpr float PERCENT_SCALE = 100.0f;
	/** Phase effect ids: base + the phase's index (packed like gear ids). */
	constexpr int32 PHASE_EFFECT_SOURCE_ID_BASE = 193000;
	/** B-355: a guard on one evaluation's rule firings (each fires once, so a list never needs more). */
	constexpr int32 MAX_RULE_FIRINGS_PER_EVALUATION = 64;
}

UBossPhaseComponent::UBossPhaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBossPhaseComponent::Setup(UBossData *Data)
{
	BossData = Data;
	Phases = Data ? Data->GetPhasesToUse() : TArray<FBossPhase>();
	ReachedPhaseCount = 0;
	OwnTurnsTaken = 0;
	StatusBreakCount = 0;
	RuleState = FBossFightState();
	// B-355: the authored state-change rules; fight-start rules fire from BossTriggers, moments from UFightTriggers.
	AuthoredRules.Reset();
	for (const FBossRule &Rule : Data ? Data->Rules : TArray<FBossRule>())
	{
		if (Rule.When == EBossRuleWhen::StateChange)
		{
			AuthoredRules.Add(Rule);
		}
		else if (Rule.When == EBossRuleWhen::AtMoment)
		{
			// TODO(B-355b): UFightTriggers still fires moments from the old trigger list only.
			UE_LOG(LogWoR, Warning, TEXT("[BossPhase] %s: rule %s is AtMoment, which nothing fires yet (B-355b); author it as a Trigger for now"),
				   *GetNameSafe(Data), *Rule.RuleId.ToString());
		}
	}
	AuthoredRuleFired.Init(false, AuthoredRules.Num());
	RebuildPhaseRules();
	UnbindAll();

	if (UCharacterDataComponent *CharComp = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr)
	{
		CharComp->OnHPChanged.AddDynamic(this, &UBossPhaseComponent::HandleHPChanged);
	}
	const UGameInstance *GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UTurnManager *TurnManager = GameInstance ? GameInstance->GetSubsystem<UTurnManager>() : nullptr)
	{
		TurnManager->OnTurnStarted.AddDynamic(this, &UBossPhaseComponent::HandleTurnStarted);
	}
	UE_LOG(LogWoR, Log, TEXT("[BossPhase] %s: %d phase(s) from %s"), *GetNameSafe(GetOwner()), Phases.Num(), *GetNameSafe(Data));
}

void UBossPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAll();
	Super::EndPlay(EndPlayReason);
}

void UBossPhaseComponent::UnbindAll()
{
	if (UCharacterDataComponent *CharComp = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr)
	{
		CharComp->OnHPChanged.RemoveDynamic(this, &UBossPhaseComponent::HandleHPChanged);
	}
	const UGameInstance *GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UTurnManager *TurnManager = GameInstance ? GameInstance->GetSubsystem<UTurnManager>() : nullptr)
	{
		TurnManager->OnTurnStarted.RemoveDynamic(this, &UBossPhaseComponent::HandleTurnStarted);
	}
}

void UBossPhaseComponent::HandleHPChanged(int32 /*CurrentHP*/, int32 /*MaxHP*/)
{
	EvaluatePhases();
}

void UBossPhaseComponent::HandleTurnStarted(AActor *Actor, int32 /*TurnNumber*/)
{
	if (Actor == GetOwner())
	{
		++OwnTurnsTaken;
	}
	EvaluatePhases();
}

void UBossPhaseComponent::RecordStatusBreak()
{
	++StatusBreakCount;
	UE_LOG(LogWoR, Log, TEXT("[BossPhase] %s status break %d: status resistance +%.0f%%"), *GetNameSafe(GetOwner()),
		   StatusBreakCount, GetStatusResistanceBonus() * BossPhaseConstants::PERCENT_SCALE);
	EvaluatePhases();
}

float UBossPhaseComponent::GetStatusResistanceBonus() const
{
	return StatusBreakCount * BossPhaseConstants::STATUS_RESIST_STEP_PER_BREAK;
}

int32 UBossPhaseComponent::GetAggressionSteps() const
{
	int32 Steps = 0;
	for (int32 Index = 0; Index < ReachedPhaseCount && Index < Phases.Num(); ++Index)
	{
		Steps += Phases[Index].AggressionSteps;
	}
	return Steps;
}

float UBossPhaseComponent::GetWeatherIntensity() const
{
	float Intensity = 0.0f;
	for (int32 Index = 0; Index < ReachedPhaseCount && Index < Phases.Num(); ++Index)
	{
		Intensity = FMath::Max(Intensity, Phases[Index].WeatherIntensity);
	}
	return Intensity;
}

bool UBossPhaseComponent::IsConditionMet(const FBossPhaseCondition &Condition) const
{
	const AActor *Owner = GetOwner();
	const UCharacterDataComponent *CharComp = Owner ? Owner->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	switch (Condition.Kind)
	{
	case EBossPhaseConditionKind::HPBelowPercent:
		return CharComp && CharComp->MaxHP > 0 &&
			   CharComp->CurrentHP * BossPhaseConstants::PERCENT_SCALE <= Condition.Value * CharComp->MaxHP;
	case EBossPhaseConditionKind::TurnCount:
		return OwnTurnsTaken >= Condition.Value;
	case EBossPhaseConditionKind::AbsorptionMaxed:
	{
		const UBrokenDarknessManager *BrokenDarkness = Owner ? Owner->FindComponentByClass<UBrokenDarknessManager>() : nullptr;
		return BrokenDarkness && BrokenDarkness->IsAtMaxStacks();
	}
	case EBossPhaseConditionKind::CrystalBroken:
	{
		const ULoadoutComponent *Loadout = Owner ? Owner->FindComponentByClass<ULoadoutComponent>() : nullptr;
		const FRuntimeAttachedItem *Attachment = Loadout ? Loadout->GetActiveWeaponAttachment() : nullptr;
		return Attachment && Attachment->IsBroken();
	}
	case EBossPhaseConditionKind::StatusBreaks:
		return StatusBreakCount >= Condition.Value;
	case EBossPhaseConditionKind::InfusionHits:
	default:
		return false;
	}
}

void UBossPhaseComponent::EvaluatePhases()
{
	// Entering a phase can change HP (its effects), which re-enters through OnHPChanged.
	if (bEvaluating)
	{
		return;
	}
	TGuardValue<bool> Guard(bEvaluating, true);
	const UCharacterDataComponent *CharComp = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	// A killing blow broadcasts HP 0 before the death flag is set: a dying boss reaches no phase.
	if (CharComp && (!CharComp->bIsAlive || CharComp->CurrentHP <= 0))
	{
		return;
	}
	// Damage carries past a phase point (D-098): one hit can pass several phases. B-355: through the
	// rule list: authored rules first (each once per fight), then the phases' rules, until none fires.
	for (int32 Firing = 0; Firing < BossPhaseConstants::MAX_RULE_FIRINGS_PER_EVALUATION; ++Firing)
	{
		const FWoRBossState BossState = MakeBossState();
		FWoRConditionContext Context;
		Context.Boss = &BossState;
		const FBossRule *Fired = nullptr;
		for (int32 Index = 0; Index < AuthoredRules.Num() && !Fired; ++Index)
		{
			if (!AuthoredRuleFired[Index] && BossRules::Fires(AuthoredRules[Index], Context))
			{
				AuthoredRuleFired[Index] = true;
				Fired = &AuthoredRules[Index];
			}
		}
		for (int32 Index = 0; Index < PhaseRules.Num() && !Fired; ++Index)
		{
			if (BossRules::Fires(PhaseRules[Index], Context))
			{
				Fired = &PhaseRules[Index];
			}
		}
		if (!Fired)
		{
			return;
		}
		// A copy: its results may rebuild PhaseRules (skip / add phase).
		const FBossRule Rule = *Fired;
		if (BossRules::ApplyResults(GetOwner(), Rule, RuleState))
		{
			ReactionTriggers::ApplyBossSteps(GetOwner(), RuleState.NetSteps, /*bBeforeFight=*/false);
		}
	}
	UE_LOG(LogWoR, Warning, TEXT("[BossPhase] %s: rule firings stopped at %d in one check"), *GetNameSafe(GetOwner()),
		   BossPhaseConstants::MAX_RULE_FIRINGS_PER_EVALUATION);
}

void UBossPhaseComponent::EnterNextPhase()
{
	if (Phases.IsValidIndex(ReachedPhaseCount))
	{
		EnterPhase(ReachedPhaseCount);
	}
}

FWoRBossState UBossPhaseComponent::MakeBossState() const
{
	FWoRBossState State;
	const AActor *Owner = GetOwner();
	if (const UCharacterDataComponent *CharComp = Owner ? Owner->FindComponentByClass<UCharacterDataComponent>() : nullptr)
	{
		State.CurrentHP = CharComp->CurrentHP;
		State.MaxHP = CharComp->MaxHP;
	}
	State.TurnsTaken = OwnTurnsTaken;
	State.StatusBreaks = StatusBreakCount;
	State.PhasesReached = ReachedPhaseCount;
	const UBrokenDarknessManager *BrokenDarkness = Owner ? Owner->FindComponentByClass<UBrokenDarknessManager>() : nullptr;
	State.bAbsorptionMaxed = BrokenDarkness && BrokenDarkness->IsAtMaxStacks();
	const ULoadoutComponent *Loadout = Owner ? Owner->FindComponentByClass<ULoadoutComponent>() : nullptr;
	const FRuntimeAttachedItem *Attachment = Loadout ? Loadout->GetActiveWeaponAttachment() : nullptr;
	State.bCrystalBroken = Attachment && Attachment->IsBroken();
	return State;
}

void UBossPhaseComponent::RebuildPhaseRules()
{
	PhaseRules.Reset();
	for (int32 Index = 0; Index < Phases.Num(); ++Index)
	{
		FBossRule Rule;
		if (BossRules::MakePhaseRule(Phases[Index], Index, Rule))
		{
			PhaseRules.Add(Rule);
		}
	}
}

void UBossPhaseComponent::EnterPhase(int32 PhaseIndex)
{
	// A copy: listeners may add or skip phases while this one is applied.
	const FBossPhase Phase = Phases[PhaseIndex];
	ReachedPhaseCount = PhaseIndex + 1;
	AActor *Owner = GetOwner();

	const UGameInstance *GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (USkillEffectManager *Effects = GameInstance ? GameInstance->GetSubsystem<USkillEffectManager>() : nullptr)
	{
		const int32 SourceId = BossPhaseConstants::PHASE_EFFECT_SOURCE_ID_BASE + PhaseIndex;
		int32 EffectIndex = 0;
		for (const UEffectDefinition *Definition : Phase.PhaseEffects)
		{
			if (EffectIndex > EffectIdentity::EFFECT_ID_SUBBAND_MAX)
			{
				break;
			}
			for (const FSkillEffect &Effect : Definition ? Definition->Effects : TArray<FSkillEffect>())
			{
				if (EffectIndex > EffectIdentity::EFFECT_ID_SUBBAND_MAX)
				{
					UE_LOG(LogWoR, Warning, TEXT("[BossPhase] %s: more than %d phase effects, the rest are skipped"),
						   *Phase.PhaseId.ToString(), EffectIdentity::EFFECT_ID_SUBBAND_MAX + 1);
					break;
				}
				// TODO: payload targets (the party, allies) - phase effects go on the boss for now.
				for (FActiveSkillEffect Runtime :
					 FActiveSkillEffect::CreateAllFromSkillEffect(Phase.PhaseId.ToString(), SourceId, Effect, EffectIndex))
				{
					Runtime.bFromGear = true; // part of the boss for the rest of the fight
					Effects->ApplyEffect(Owner, Runtime);
				}
				++EffectIndex;
			}
		}
	}
	if (UCharacterDataComponent *CharComp = Owner ? Owner->FindComponentByClass<UCharacterDataComponent>() : nullptr)
	{
		// Max HP may change; current HP stays (damage already dealt carries), at most the new max.
		CharComp->RecomputeMaxPools();
		if (CharComp->CurrentHP > CharComp->MaxHP)
		{
			CharComp->ServerSetHP(CharComp->MaxHP);
		}
	}
	if (Phase.bChangeElement || !Phase.AddedSkills.IsEmpty())
	{
		UE_LOG(LogWoR, Log, TEXT("[BossPhase] %s %s: element / skill changes are not applied yet (TODO)"),
			   *GetNameSafe(Owner), *Phase.PhaseId.ToString());
	}
	UE_LOG(LogWoR, Log, TEXT("[BossPhase] %s reached phase %d/%d (%s): AI +%d, weather %.2f, dialogue %s"), *GetNameSafe(Owner),
		   ReachedPhaseCount, Phases.Num(), *Phase.PhaseId.ToString(), GetAggressionSteps(), GetWeatherIntensity(),
		   *Phase.DialogueId.ToString());

	if (UWeatherStateManager *Weather = GameInstance ? GameInstance->GetSubsystem<UWeatherStateManager>() : nullptr)
	{
		Weather->RefreshWeather();
	}
	OnBossPhaseReached.Broadcast(Owner, PhaseIndex);
}

void UBossPhaseComponent::SkipNextPhase()
{
	if (Phases.IsValidIndex(ReachedPhaseCount))
	{
		UE_LOG(LogWoR, Log, TEXT("[BossPhase] %s: phase %s skipped"), *GetNameSafe(GetOwner()), *Phases[ReachedPhaseCount].PhaseId.ToString());
		Phases.RemoveAt(ReachedPhaseCount);
		RebuildPhaseRules(); // later phases moved down one index
	}
}

void UBossPhaseComponent::AddPhase(const FBossPhase &Phase)
{
	Phases.Add(Phase);
	RebuildPhaseRules();
	UE_LOG(LogWoR, Log, TEXT("[BossPhase] %s: phase %s added"), *GetNameSafe(GetOwner()), *Phase.PhaseId.ToString());
}

FString UBossPhaseComponent::GetBossPhaseString() const
{
	TArray<FString> Names;
	for (int32 Index = 0; Index < Phases.Num(); ++Index)
	{
		Names.Add(FString::Printf(TEXT("%s%s"), *Phases[Index].PhaseId.ToString(), Index < ReachedPhaseCount ? TEXT(" (reached)") : TEXT("")));
	}
	FString Out = FString::Printf(TEXT("Boss %s: phase %d/%d [%s] | own turns %d | status breaks %d (resist +%.0f%%) | AI +%d | weather %.2f"),
								  *GetNameSafe(GetOwner()), ReachedPhaseCount, Phases.Num(), *FString::Join(Names, TEXT(", ")),
								  OwnTurnsTaken, StatusBreakCount, GetStatusResistanceBonus() * BossPhaseConstants::PERCENT_SCALE,
								  GetAggressionSteps(), GetWeatherIntensity());
	// B-355: the state-change rules (authored, then the phases').
	for (int32 Index = 0; Index < AuthoredRules.Num(); ++Index)
	{
		Out += FString::Printf(TEXT("\n  rule %s%s"), *BossRules::GetRuleString(AuthoredRules[Index]), AuthoredRuleFired[Index] ? TEXT(" (fired)") : TEXT(""));
	}
	for (const FBossRule &Rule : PhaseRules)
	{
		Out += FString::Printf(TEXT("\n  rule %s"), *BossRules::GetRuleString(Rule));
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GBossPrintCommand(
	TEXT("wor.Boss.Print"),
	TEXT("Print every boss in the fight: its phases (reached or not), turns, status breaks, AI steps and weather (B-193)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		int32 Bosses = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (const UBossPhaseComponent *Boss = It->FindComponentByClass<UBossPhaseComponent>())
			{
				UE_LOG(LogWoR, Display, TEXT("%s"), *Boss->GetBossPhaseString());
				++Bosses;
			}
		}
		if (Bosses == 0)
		{
			UE_LOG(LogWoR, Display, TEXT("wor.Boss.Print: no boss in this level."));
		}
	}));
#endif
