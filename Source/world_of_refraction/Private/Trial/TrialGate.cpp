// TrialGate.cpp

#include "Trial/TrialGate.h"

#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

#define LOCTEXT_NAMESPACE "TrialGate"

namespace TrialGateInternal
{
	// TODO: tune (Q-183).
	constexpr float EASY_ENEMY_STRENGTH = 0.8f;
	constexpr float MEDIUM_ENEMY_STRENGTH = 1.0f;
	constexpr float HARD_ENEMY_STRENGTH = 1.25f;
	constexpr float EASY_PAYOUT = 0.75f;
	constexpr float MEDIUM_PAYOUT = 1.0f;
	constexpr float HARD_PAYOUT = 1.5f;

	constexpr int32 ENEMY_STRENGTH_HP_EFFECT_ID = 183001;
	constexpr int32 ENEMY_STRENGTH_DAMAGE_EFFECT_ID = 183002;
}

float TrialGate::GetEnemyStrengthMultiplier(ETrialDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case ETrialDifficulty::Easy:
		return TrialGateInternal::EASY_ENEMY_STRENGTH;
	case ETrialDifficulty::Hard:
		return TrialGateInternal::HARD_ENEMY_STRENGTH;
	case ETrialDifficulty::Medium:
	default:
		return TrialGateInternal::MEDIUM_ENEMY_STRENGTH;
	}
}

float TrialGate::GetPayoutMultiplier(ETrialDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case ETrialDifficulty::Easy:
		return TrialGateInternal::EASY_PAYOUT;
	case ETrialDifficulty::Hard:
		return TrialGateInternal::HARD_PAYOUT;
	case ETrialDifficulty::Medium:
	default:
		return TrialGateInternal::MEDIUM_PAYOUT;
	}
}

FText TrialGate::GetDifficultyName(ETrialDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case ETrialDifficulty::Easy:
		return LOCTEXT("Easy", "Easy");
	case ETrialDifficulty::Hard:
		return LOCTEXT("Hard", "Hard");
	case ETrialDifficulty::Medium:
	default:
		return LOCTEXT("Medium", "Medium");
	}
}

ETrialDifficulty TrialGate::NextDifficulty(ETrialDifficulty Difficulty)
{
	return static_cast<ETrialDifficulty>((static_cast<int32>(Difficulty) + 1) % DIFFICULTY_COUNT);
}

bool TrialGate::ApplyEnemyStrength(AActor *Enemy, float StrengthMultiplier)
{
	UCharacterDataComponent *CharComp = Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const UGameInstance *GI = Enemy && Enemy->GetWorld() ? Enemy->GetWorld()->GetGameInstance() : nullptr;
	USkillEffectManager *Effects = GI ? GI->GetSubsystem<USkillEffectManager>() : nullptr;
	if (!CharComp || !Effects)
	{
		return false;
	}
	if (FMath::IsNearlyEqual(StrengthMultiplier, 1.0f))
	{
		return true;
	}

	const bool bStronger = StrengthMultiplier > 1.0f;
	const float Percent = FMath::Abs(StrengthMultiplier - 1.0f) * CombatConstants::STAT_PERCENT_DIVISOR;
	// The level is part of the enemy, not a buff: no cleanse strips it (same rule as gear, D-015).
	FActiveSkillEffect HPEffect = FActiveSkillEffect::CreatePersistent(TEXT("Enemy strength (HP)"),
		TrialGateInternal::ENEMY_STRENGTH_HP_EFFECT_ID,
		bStronger ? ESkillEffectType::MaxHPBuff : ESkillEffectType::MaxHPDebuff, Percent);
	HPEffect.bFromGear = true;
	FActiveSkillEffect DamageEffect = FActiveSkillEffect::CreatePersistent(TEXT("Enemy strength (damage)"),
		TrialGateInternal::ENEMY_STRENGTH_DAMAGE_EFFECT_ID,
		bStronger ? ESkillEffectType::DamageBuff : ESkillEffectType::DamageDebuff, Percent);
	DamageEffect.bFromGear = true;
	Effects->ApplyEffect(Enemy, HPEffect);
	Effects->ApplyEffect(Enemy, DamageEffect);
	CharComp->RecomputeMaxPools();
	CharComp->CurrentHP = CharComp->MaxHP;
	return true;
}

#undef LOCTEXT_NAMESPACE
