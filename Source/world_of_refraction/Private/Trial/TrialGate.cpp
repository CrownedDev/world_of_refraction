// TrialGate.cpp

#include "Trial/TrialGate.h"
#include "Combat/Tier/CombatTier.h"

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
	// B-344 (D-136): the trial difficulty's enemy AI tier.
	constexpr EAIDifficulty ENEMY_AI_EASY = EAIDifficulty::Easy;
	constexpr EAIDifficulty ENEMY_AI_MEDIUM = EAIDifficulty::Medium;
	constexpr EAIDifficulty ENEMY_AI_HARD = EAIDifficulty::Hard;

	// Enemy damage by difficulty (Q-183; B-293c: even steps, was 0.8 / 1 / 1.25). TODO: tune.
	constexpr float EASY_ENEMY_STRENGTH = 0.8f;
	constexpr float MEDIUM_ENEMY_STRENGTH = 1.0f;
	constexpr float HARD_ENEMY_STRENGTH = 1.2f;
	/** B-293c (Q-293c): the difficulty leaves enemy HP alone (Expedition 33 scales enemy damage and the defence
	 *  windows, not HP bars). */
	constexpr float ENEMY_HEALTH_BY_DIFFICULTY = 1.0f;
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

float TrialGate::GetEnemyHealthMultiplier(ETrialDifficulty Difficulty)
{
	return TrialGateInternal::ENEMY_HEALTH_BY_DIFFICULTY;
}

EAIDifficulty TrialGate::GetEnemyAIDifficulty(ETrialDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case ETrialDifficulty::Easy: return TrialGateInternal::ENEMY_AI_EASY;
	case ETrialDifficulty::Hard: return TrialGateInternal::ENEMY_AI_HARD;
	case ETrialDifficulty::Medium:
	default: return TrialGateInternal::ENEMY_AI_MEDIUM;
	}
}

EAIDifficulty TrialGate::ResolveEncounterAIDifficulty(ETrialDifficulty TrialDifficulty, bool bOverride, EAIDifficulty Override)
{
	return bOverride ? Override : GetEnemyAIDifficulty(TrialDifficulty);
}

bool TrialGate::ApplyEnemyStrength(AActor *Enemy, float StrengthMultiplier)
{
	return ApplyEnemyStrength(Enemy, StrengthMultiplier, StrengthMultiplier);
}

bool TrialGate::ApplyEnemyStrength(AActor *Enemy, float HealthMultiplier, float DamageMultiplier)
{
	UCharacterDataComponent *CharComp = Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const UGameInstance *GI = Enemy && Enemy->GetWorld() ? Enemy->GetWorld()->GetGameInstance() : nullptr;
	USkillEffectManager *Effects = WoRFight::Get<USkillEffectManager>(GI);
	if (!CharComp || !Effects)
	{
		return false;
	}
	const bool bHealth = !FMath::IsNearlyEqual(HealthMultiplier, 1.0f);
	const bool bDamage = !FMath::IsNearlyEqual(DamageMultiplier, 1.0f);
	if (!bHealth && !bDamage)
	{
		return true;
	}

	// The level is part of the enemy, not a buff: no cleanse strips it (same rule as gear, D-015).
	if (bHealth)
	{
		FActiveSkillEffect HPEffect = FActiveSkillEffect::CreatePersistent(TEXT("Enemy strength (HP)"),
			TrialGateInternal::ENEMY_STRENGTH_HP_EFFECT_ID,
			HealthMultiplier > 1.0f ? ESkillEffectType::MaxHPBuff : ESkillEffectType::MaxHPDebuff,
			FMath::Abs(HealthMultiplier - 1.0f) * CombatConstants::STAT_PERCENT_DIVISOR);
		HPEffect.bFromGear = true;
		Effects->ApplyEffect(Enemy, HPEffect);
	}
	if (bDamage)
	{
		FActiveSkillEffect DamageEffect = FActiveSkillEffect::CreatePersistent(TEXT("Enemy strength (damage)"),
			TrialGateInternal::ENEMY_STRENGTH_DAMAGE_EFFECT_ID,
			DamageMultiplier > 1.0f ? ESkillEffectType::DamageBuff : ESkillEffectType::DamageDebuff,
			FMath::Abs(DamageMultiplier - 1.0f) * CombatConstants::STAT_PERCENT_DIVISOR);
		DamageEffect.bFromGear = true;
		Effects->ApplyEffect(Enemy, DamageEffect);
	}
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
	// B-325 (D-118): the enemy's Combat Tier (its gear and skills; EnemyTier only as the fallback).
	return TierPowerScaling::GetTierPowerMultiplier(WoRCombatTier::GetCombatTier(Enemy));
}

bool TrialGate::ApplyEnemyRankHealth(AActor *Enemy, float StrengthMultiplier, EEncounterRank Rank)
{
	UCharacterDataComponent *CharComp = Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const UGameInstance *GI = Enemy && Enemy->GetWorld() ? Enemy->GetWorld()->GetGameInstance() : nullptr;
	USkillEffectManager *Effects = WoRFight::Get<USkillEffectManager>(GI);
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
	// Strength (the difficulty's HP share, x1 since B-293c; D-120: no zone HP) is already a +(S - 1) MaxHPBuff from
	// ApplyEnemyStrength. MaxHPBuff percents add, so S x (rank x tier - 1) on top makes S x rank x tier (B-327,
	// WoR.Trial.ZoneHealthOnce).
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

