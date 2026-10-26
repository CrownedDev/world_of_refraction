// BossPhaseComponent.cpp

#include "Combat/Boss/BossPhaseComponent.h"

#include "Character/CharacterDataComponent.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
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
	// Damage carries past a phase point (D-098): one hit can pass several phases.
	while (Phases.IsValidIndex(ReachedPhaseCount))
	{
		const FBossPhase &Next = Phases[ReachedPhaseCount];
		if (Next.Conditions.IsEmpty())
		{
			return;
		}
		for (const FBossPhaseCondition &Condition : Next.Conditions)
		{
			if (!IsConditionMet(Condition))
			{
				return;
			}
		}
		EnterPhase(ReachedPhaseCount);
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
	}
}

void UBossPhaseComponent::AddPhase(const FBossPhase &Phase)
{
	Phases.Add(Phase);
	UE_LOG(LogWoR, Log, TEXT("[BossPhase] %s: phase %s added"), *GetNameSafe(GetOwner()), *Phase.PhaseId.ToString());
}

FString UBossPhaseComponent::GetBossPhaseString() const
{
	TArray<FString> Names;
	for (int32 Index = 0; Index < Phases.Num(); ++Index)
	{
		Names.Add(FString::Printf(TEXT("%s%s"), *Phases[Index].PhaseId.ToString(), Index < ReachedPhaseCount ? TEXT(" (reached)") : TEXT("")));
	}
	return FString::Printf(TEXT("Boss %s: phase %d/%d [%s] | own turns %d | status breaks %d (resist +%.0f%%) | AI +%d | weather %.2f"),
						   *GetNameSafe(GetOwner()), ReachedPhaseCount, Phases.Num(), *FString::Join(Names, TEXT(", ")),
						   OwnTurnsTaken, StatusBreakCount, GetStatusResistanceBonus() * BossPhaseConstants::PERCENT_SCALE,
						   GetAggressionSteps(), GetWeatherIntensity());
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
