// TrialGate.cpp

#include "Trial/TrialGate.h"

#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Combat/CombatConstants.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Combat/Mechanics/BattleRandomSubsystem.h"
#include "Combat/Tuning/CombatBaselineConstants.h"
#include "Combat/Tuning/CombatTuning.h"
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
	constexpr int32 ENEMY_RANK_HP_EFFECT_ID = 224001;
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
	CharComp->ServerSetHP(CharComp->MaxHP); // B-131: server-only, and the HP change is broadcast
	return true;
}

// ENEMY_RANK_HEALTH is positional on the enum: Normal, Mini-boss, Boss, then Elite (appended).
static_assert(static_cast<int32>(EEncounterRank::Normal) == 0 && static_cast<int32>(EEncounterRank::MiniBoss) == 1 &&
				  static_cast<int32>(EEncounterRank::Boss) == 2 && static_cast<int32>(EEncounterRank::Elite) == 3 &&
				  CombatBaseline::ENEMY_RANK_COUNT == 4,
			  "EEncounterRank order changed: update CombatBaseline::ENEMY_RANK_HEALTH");

float TrialGate::GetRankHealthMultiplier(EEncounterRank Rank)
{
	const int32 RankValue = static_cast<int32>(Rank);
	return WoRTuning::GetLadderValue(ECombatLadder::EnemyRankHealth, RankValue, CombatBaseline::CodeEnemyRankHealth(RankValue));
}

float TrialGate::GetEnemyTierHealthMultiplier(const AActor *Enemy)
{
	const UCharacterDataComponent *CharComp = Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const UCharacterData *Data = CharComp ? CharComp->CharacterData : nullptr;
	return TierPowerScaling::GetTierPowerMultiplier(Data ? Data->EnemyTier : EItemTier::F_Tier);
}

bool TrialGate::ApplyEnemyRankHealth(AActor *Enemy, float StrengthMultiplier, EEncounterRank Rank)
{
	UCharacterDataComponent *CharComp = Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const UGameInstance *GI = Enemy && Enemy->GetWorld() ? Enemy->GetWorld()->GetGameInstance() : nullptr;
	USkillEffectManager *Effects = GI ? GI->GetSubsystem<USkillEffectManager>() : nullptr;
	if (!CharComp || !Effects)
	{
		return false;
	}
	const float RankMultiplier = GetRankHealthMultiplier(Rank);
	const float TierMultiplier = GetEnemyTierHealthMultiplier(Enemy);
	const float HealthMultiplier = RankMultiplier * TierMultiplier;
	if (FMath::IsNearlyEqual(HealthMultiplier, CombatBaseline::ENEMY_RANK_HEALTH_NORMAL))
	{
		return true;
	}
	const float Percent = StrengthMultiplier * (HealthMultiplier - 1.0f) * CombatConstants::STAT_PERCENT_DIVISOR;
	FActiveSkillEffect HPEffect = FActiveSkillEffect::CreatePersistent(TEXT("Enemy rank (HP)"), TrialGateInternal::ENEMY_RANK_HP_EFFECT_ID,
																	   Percent >= 0.0f ? ESkillEffectType::MaxHPBuff : ESkillEffectType::MaxHPDebuff,
																	   FMath::Abs(Percent));
	HPEffect.bFromGear = true; // part of the enemy, not a cleansable buff (D-015)
	Effects->ApplyEffect(Enemy, HPEffect);
	CharComp->RecomputeMaxPools();
	CharComp->ServerSetHP(CharComp->MaxHP);
	UE_LOG(LogTemp, Log, TEXT("[TrialGate] %s: rank %s HP x%.2f, tier x%.2f (strength x%.2f)"), *GetNameSafe(Enemy),
		   *UEnum::GetValueAsString(Rank), RankMultiplier, TierMultiplier, StrengthMultiplier);
	return true;
}

#undef LOCTEXT_NAMESPACE

float TrialGate::GetLevelDropChance(EItemTier EnemyTier, EEncounterRank Rank)
{
	if (Rank == EEncounterRank::MiniBoss || Rank == EEncounterRank::Boss)
	{
		return 0.0f;
	}
	const int32 TierIndex = static_cast<int32>(EnemyTier);
	const float Chance = WoRTuning::GetLadderValue(ECombatLadder::LevelDropChance, TierIndex, CombatBaseline::CodeLevelDropChance(TierIndex));
	const float RankFactor = Rank == EEncounterRank::Elite ? CombatBaseline::LEVEL_DROP_ELITE_CHANCE_FACTOR : 1.0f;
	return FMath::Clamp(Chance * RankFactor, 0.0f, 1.0f);
}

int32 TrialGate::GetGuaranteedLevels(EItemTier EnemyTier, EEncounterRank Rank)
{
	if (Rank != EEncounterRank::MiniBoss && Rank != EEncounterRank::Boss)
	{
		return 0;
	}
	const int32 TierIndex = static_cast<int32>(EnemyTier);
	const float Levels = WoRTuning::GetLadderValue(ECombatLadder::MiniBossLevels, TierIndex,
												   static_cast<float>(CombatBaseline::CodeMiniBossLevels(TierIndex)));
	return FMath::Max(0, FMath::RoundToInt(Levels));
}

int32 TrialGate::RollLevelDrops(const UObject *WorldContext, EItemTier EnemyTier, EEncounterRank Rank)
{
	const int32 Guaranteed = GetGuaranteedLevels(EnemyTier, Rank);
	if (Guaranteed > 0)
	{
		return Guaranteed;
	}
	const float Chance = GetLevelDropChance(EnemyTier, Rank);
	return Chance > 0.0f && UBattleRandomSubsystem::FRand(WorldContext) < Chance ? 1 : 0;
}

