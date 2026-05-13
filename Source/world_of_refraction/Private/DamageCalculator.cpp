// DamageCalculator.cpp
// Centralized damage calculation implementation

#include "DamageCalculator.h"
#include "CharacterData.h"
#include "CharacterDataComponent.h"
#include "WeaponAttackData.h"
#include "SkillEffectManager.h"
#include "BrokenDarknessManager.h"
#include "Engine/GameInstance.h"
#include "WeaponData.h"
#include "WeaponManager.h"
#include "CombatGridSubsystem.h"
#include "LoadoutComponent.h"
#include "FEquipmentStatBonus.h"

void UDamageCalculator::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[DamageCalculator] Initialized"));
}

// ==================== MAIN CALCULATION ====================

FDamageCalculationResult UDamageCalculator::CalculateDamage(
	AActor *Attacker,
	AActor *Defender,
	const FDamageCalculationInput &Input)
{
	FDamageCalculationResult Result;
	Result.EffectiveElement = Input.Element;

	if (Input.BaseDamage <= 0)
	{
		return Result;
	}

	float RunningDamage = static_cast<float>(Input.BaseDamage);

	// Step 1: Attacker's damage multiplier — branched on EActionType.
	// Spell → SpellDamage. Ability/Attack/None → RawDamage. Per-action ActionMods
	// boost the matching sub-stat. ActionMods carries Reality + Evolution + any
	// future per-action stat modifier sources.
	float AttackerMult = GetAttackerDamageMultiplier(Attacker, Input.ActionType);
	const ESubStat AttackerStat = (Input.ActionType == EActionType::Spell) ? ESubStat::SpellDamage : ESubStat::RawDamage;
	AttackerMult = Input.ActionMods.ApplyTo(AttackerMult, AttackerStat);

	// Equipment stat bonus — direct read from the attacker's active loadout.
	// Replaces the prior RawDamageBuff/StatusMultiplierBuff status-effect path
	// (which depended on ApplyWeaponBonuses, never wired in production).
	// Folded into AttackerMult so each rolled point contributes a small
	// fractional multiplier rather than flat damage.
	if (Attacker)
	{
		if (ULoadoutComponent *Loadout = Attacker->FindComponentByClass<ULoadoutComponent>())
		{
			const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Attacker);
			if (Input.ActionType != EActionType::Spell)
			{
				AttackerMult += Bonus.BonusRawDamage * CombatConstants::RAW_DAMAGE_PER_POINT;
			}
			else
			{
				AttackerMult += Bonus.BonusSpellDamage * CombatConstants::SPELL_DAMAGE_PER_POINT;
			}
		}
	}

	Result.AttackerDamageMultiplier = AttackerMult;
	RunningDamage *= AttackerMult;

	// Step 1.5: Grid position damage modifier (attacker)
	UCombatGridSubsystem *Grid = GetCombatGridSubsystem();
	if (Grid)
	{
		float GridDamageMod = Grid->GetDamageModifier(Attacker);
		RunningDamage *= GridDamageMod;
	}

	// Step 2: Status effect modifiers (buffs/debuffs)
	float StatusMod = GetStatusEffectDamageModifier(Attacker, Defender);
	RunningDamage *= StatusMod;

	// Step 3: Element interaction (weakness/resistance)
	if (Input.Element != ESpellElement::Generic && Defender)
	{
		UCharacterData *DefenderData = GetCharacterData(Defender);
		if (DefenderData)
		{
			Result.ElementMultiplier = GetElementInteractionMultiplier(Input.Element, DefenderData->InnateElement);
			RunningDamage *= Result.ElementMultiplier;
		}
	}

	// Step 4: Critical hit
	if (Input.bCanCrit)
	{
		float CritChance = Input.OverrideCritChance >= 0.0f ? Input.OverrideCritChance : GetCriticalChance(Attacker);
		// ActionMods.CritChance applies whether crit chance came from override
		// or computed default. No re-clamp here — preserves prior bool-path
		// behaviour (Reality boost was deliberately uncapped at this site).
		CritChance = Input.ActionMods.ApplyTo(CritChance, ESubStat::CritChance);

		// Luck-driven crit bonus. Linearly scaled from raw Luck (0.0-LUCK_RAW_MAX)
		// to consumer cap LUCK_CRIT_BONUS_MAX. Additive on top — matches locked
		// design where Luck grants extra crit chance ON TOP OF CritChance.
		// GetEquipmentModifiedLuck folds in crystal Spirit modifier and
		// active-loadout BonusLuck.
		if (Attacker)
		{
			if (UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>())
			{
				const float RawLuck = AttackerComp->GetEquipmentModifiedLuck();
				const float LuckCritBonus = (RawLuck / CombatConstants::LUCK_RAW_MAX) * CombatConstants::LUCK_CRIT_BONUS_MAX;
				CritChance += LuckCritBonus;
			}
		}

		Result.bWasCritical = FMath::FRand() < CritChance;

		if (Result.bWasCritical)
		{
			Result.CritMultiplier = DamageConstants::CRIT_MULTIPLIER;
			RunningDamage *= DamageConstants::CRIT_MULTIPLIER;
		}
	}

	// Store damage before defense
	Result.DamageBeforeDefense = FMath::RoundToInt(RunningDamage);

	// Step 5: Defender's flat defense
	if (!Input.bIgnoreDefense && Defender)
	{
		Result.DefenderFlatDefense = GetDefenderFlatDefense(Defender);
		int32 Blocked = FMath::Min(Result.DefenderFlatDefense, FMath::RoundToInt(RunningDamage));
		Result.DamageBlockedByDefense = Blocked;
		RunningDamage -= Blocked;
	}

	// Step 6.5: Grid position defense modifier (defender)
	if (Grid && Defender)
	{
		float GridDefenseMod = Grid->GetDefenseModifier(Defender);
		if (GridDefenseMod > 0.0f)
		{
			RunningDamage /= GridDefenseMod;
		}
	}

	// Step 7: Ensure minimum damage
	Result.FinalDamage = FMath::Max(DamageConstants::MIN_DAMAGE, FMath::RoundToInt(RunningDamage));

	// Step 7: Ensure minimum damage
	Result.FinalDamage = FMath::Max(DamageConstants::MIN_DAMAGE, FMath::RoundToInt(RunningDamage));

	// Calculate status buildup if applicable
	// (Caller should handle this separately based on spell/ability data)

	return Result;
}

FDamageCalculationResult UDamageCalculator::CalculateAttackDamage(
	AActor *Attacker,
	AActor *Target,
	UWeaponAttackData *Attack,
	bool bIsInfused)
{
	FDamageCalculationResult Result;

	if (!Attack)
	{
		return Result;
	}

	UCharacterData *AttackerData = GetCharacterData(Attacker);
	if (!AttackerData)
	{
		return Result;
	}

	// Build input
	FDamageCalculationInput Input;

	// Base damage — sourced from the attack asset. Strict replace per Phase 4 design:
	// no implicit fallback. Attack assets with BaseDamage == 0 deal 0 damage before
	// weapon-stat bonuses and multipliers.
	Input.BaseDamage = Attack->BaseDamage;
	Input.ActionType = EActionType::Attack;

	// Apply requirement penalty (matches Ability/Spell pattern — multiplicative reduction).
	const float RequirementPenalty = Attack->CalculateRequirementPenalty(AttackerData);
	if (RequirementPenalty > 0.0f)
	{
		Input.BaseDamage = FMath::RoundToInt(static_cast<float>(Input.BaseDamage) * (1.0f - RequirementPenalty));
	}

	// Attacks are physical unless infused. Per locked design, infused attacks
	// still scale by RawDamage — the element only affects status routing.
	Input.Element = bIsInfused ? AttackerData->InnateElement : ESpellElement::Generic;

	Input.bCanCrit = true;
	Input.bWasInfused = bIsInfused;
	Input.HitCount = Attack->HitCount;

	// Per-instance weapon StatBonus (BonusRawDamage / BonusCritChance) is no longer
	// read directly here. It's applied as persistent status effects at equip time
	// (RawDamageBuff via CalculateDamage; CritChanceBuff via GetCriticalChance), so
	// reading it again would double-count.

	// Calculate with main function
	Result = CalculateDamage(Attacker, Target, Input);

	return Result;
}

// ==================== COMPONENT CALCULATIONS ====================

float UDamageCalculator::GetAttackerDamageMultiplier(AActor *Attacker, EActionType ActionType) const
{
	if (!Attacker)
	{
		return 1.0f;
	}

	UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
	if (!AttackerComp || !AttackerComp->CharacterData)
	{
		return 1.0f;
	}

	// Crystal-aware Spell/Raw damage multiplier — uses GetCrystalModifiedMind/Body
	// so the slotted primary evolution crystal's pillar modifier feeds the curve.
	if (ActionType == EActionType::Spell)
	{
		return AttackerComp->GetCrystalModifiedSpellDamage();
	}
	else
	{
		return AttackerComp->GetCrystalModifiedRawDamage();
	}
}

int32 UDamageCalculator::GetDefenderFlatDefense(AActor *Defender) const
{
	if (!Defender)
	{
		return 0;
	}

	UCharacterDataComponent *DefenderComp = Defender->FindComponentByClass<UCharacterDataComponent>();
	if (!DefenderComp || !DefenderComp->CharacterData)
	{
		return 0;
	}

	// Crystal-aware flat defense — uses GetCrystalModifiedBody so the slotted
	// primary evolution crystal's Body pillar modifier feeds the curve.
	int32 BaseDefense = DefenderComp->GetCrystalModifiedFlatDefense();

	// Equipment stat bonus — flat additive to defense. Direct read from the
	// defender's active loadout (the bonus is an int rolled per-instance).
	if (Defender)
	{
		if (ULoadoutComponent *Loadout = Defender->FindComponentByClass<ULoadoutComponent>())
		{
			const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Defender);
			BaseDefense += Bonus.BonusDefense;
		}
	}

	// Combat-buff/debuff modifiers (from skill casts, e.g. Stoneskin). Kept
	// separate from the equipment-bonus path above — these are percentage
	// modifiers applied multiplicatively, while equipment is flat additive.
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (StatusManager)
	{
		float DefenseBuff = StatusManager->GetTotalStatModifier(Defender, ESkillEffectType::DefenseBuff);
		float DefenseDebuff = StatusManager->GetTotalStatModifier(Defender, ESkillEffectType::DefenseDebuff);

		// Buffs/debuffs are percentage modifiers
		float Modifier = 1.0f + (DefenseBuff - DefenseDebuff) / 100.0f;
		BaseDefense = FMath::RoundToInt(BaseDefense * FMath::Max(0.0f, Modifier));
	}

	return BaseDefense;
}

float UDamageCalculator::GetCriticalChance(AActor *Attacker) const
{
	if (!Attacker)
	{
		return DamageConstants::BASE_CRIT_CHANCE;
	}

	UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
	if (!AttackerComp || !AttackerComp->CharacterData)
	{
		return DamageConstants::BASE_CRIT_CHANCE;
	}

	// Crystal-aware crit chance — uses GetCrystalModifiedMind so the slotted
	// primary evolution crystal's Mind pillar modifier feeds the curve.
	float BaseCrit = AttackerComp->GetCrystalModifiedCritChance();

	// Equipment stat bonus — BonusCritChance is already a float percentage,
	// so divide by 100 to convert to the 0-1 crit-chance space.
	if (Attacker)
	{
		if (ULoadoutComponent *Loadout = Attacker->FindComponentByClass<ULoadoutComponent>())
		{
			const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Attacker);
			BaseCrit += Bonus.BonusCritChance / 100.0f;
		}
	}

	// Combat-buff/debuff modifiers (from skill casts).
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (StatusManager)
	{
		float CritBuff = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::CritChanceBuff);
		float CritDebuff = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::CritChanceDebuff);

		BaseCrit += (CritBuff - CritDebuff) / 100.0f;
	}

	return FMath::Clamp(BaseCrit, 0.0f, DamageConstants::MAX_CRIT_CHANCE);
}

bool UDamageCalculator::RollCriticalHit(AActor *Attacker, float OverrideChance) const
{
	float Chance = OverrideChance >= 0.0f ? OverrideChance : GetCriticalChance(Attacker);
	return FMath::FRand() < Chance;
}

float UDamageCalculator::GetElementInteractionMultiplier(ESpellElement AttackElement, ESpellElement DefenderElement) const
{
	if (IsWeakTo(DefenderElement, AttackElement))
	{
		return DamageConstants::WEAKNESS_MULTIPLIER;
	}

	if (ResistsElement(DefenderElement, AttackElement))
	{
		return DamageConstants::RESISTANCE_MULTIPLIER;
	}

	return DamageConstants::NEUTRAL_MULTIPLIER;
}

// ==================== STATUS EFFECT CALCULATIONS ====================

int32 UDamageCalculator::CalculateStatusBuildup(
	AActor *Attacker,
	AActor *Target,
	int32 BaseBuildup,
	ESpellElement Element)
{
	if (BaseBuildup <= 0)
	{
		return 0;
	}

	float Buildup = static_cast<float>(BaseBuildup);

	// Apply BD stack multiplier
	float BDMult = GetBDStackStatusMultiplier(Attacker, Element);
	Buildup *= BDMult;

	// TODO: Apply status effect modifiers (status potency buffs/debuffs)

	return FMath::RoundToInt(Buildup);
}

float UDamageCalculator::GetBDStackStatusMultiplier(AActor *Attacker, ESpellElement Element) const
{
	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Attacker);
	if (!BDManager || !BDManager->IsTransformed())
	{
		return 1.0f;
	}

	// Only apply multiplier if spell element matches BD alignment
	if (Element == BDManager->GetCurrentAlignment())
	{
		return BDManager->GetStackStatusMultiplier();
	}

	return 1.0f;
}

// ==================== HEALING CALCULATIONS ====================

int32 UDamageCalculator::CalculateHealing(
	AActor *Healer,
	AActor *Target,
	int32 BaseHealing)
{
	if (BaseHealing <= 0)
	{
		return 0;
	}

	float Healing = static_cast<float>(BaseHealing);

	// Apply healer's SpellDamage multiplier. Healing is a spell-class effect — it
	// scales with the caster's spell power (Mind), not status-buildup amplification.
	// Crystal-aware: GetCrystalModifiedSpellDamageForHealing reads
	// GetCrystalModifiedMind, so the slotted primary evolution crystal's Mind
	// pillar modifier feeds the curve.
	if (Healer)
	{
		if (UCharacterDataComponent *HealerComp = Healer->FindComponentByClass<UCharacterDataComponent>())
		{
			Healing *= HealerComp->GetCrystalModifiedSpellDamageForHealing();
		}
	}

	// TODO: Add HealingReceivedBuff/Debuff to ESkillEffectType when needed
	// Currently no healing modifiers in status effect system

	return FMath::Max(0, FMath::RoundToInt(Healing));
}

// ==================== UTILITY ====================

float UDamageCalculator::GetInfusionDamageMultiplier(int32 InfusionLevel)
{
	switch (InfusionLevel)
	{
	case 1:
		return DamageConstants::POWER_INFUSION_L1_MULT;
	case 2:
		return DamageConstants::POWER_INFUSION_L2_MULT;
	default:
		return 1.0f;
	}
}

bool UDamageCalculator::IsWeakTo(ESpellElement Defender, ESpellElement Attacker)
{
	// No element weakness system - all elements are neutral
	return false;
}

bool UDamageCalculator::ResistsElement(ESpellElement Defender, ESpellElement Attacker)
{
	// No element resistance system - all elements are neutral
	return false;
}

// ==================== DEBUG ====================

void UDamageCalculator::DebugPrintCalculation(const FDamageCalculationResult &Result) const
{
	UE_LOG(LogTemp, Display, TEXT("=== DAMAGE CALCULATION ==="));
	UE_LOG(LogTemp, Display, TEXT("Damage Before Defense: %d"), Result.DamageBeforeDefense);
	UE_LOG(LogTemp, Display, TEXT("Attacker Multiplier: %.2fx"), Result.AttackerDamageMultiplier);
	UE_LOG(LogTemp, Display, TEXT("Element Multiplier: %.2fx"), Result.ElementMultiplier);
	UE_LOG(LogTemp, Display, TEXT("Critical: %s (%.2fx)"), Result.bWasCritical ? TEXT("YES") : TEXT("NO"), Result.CritMultiplier);
	UE_LOG(LogTemp, Display, TEXT("Flat Defense: %d (blocked %d)"), Result.DefenderFlatDefense, Result.DamageBlockedByDefense);
	UE_LOG(LogTemp, Display, TEXT("FINAL DAMAGE: %d"), Result.FinalDamage);
	UE_LOG(LogTemp, Display, TEXT("Status Buildup: %d"), Result.StatusBuildup);
	UE_LOG(LogTemp, Display, TEXT("=========================="));
}

// ==================== PRIVATE HELPERS ====================

UCharacterData *UDamageCalculator::GetCharacterData(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
	return Comp ? Comp->CharacterData : nullptr;
}

USkillEffectManager *UDamageCalculator::GetSkillEffectManager() const
{
	if (!CachedSkillEffectManager)
	{
		// SkillEffectManager is a GameInstanceSubsystem like DamageCalculator
		if (UGameInstance *GI = GetGameInstance())
		{
			CachedSkillEffectManager = GI->GetSubsystem<USkillEffectManager>();
		}
	}
	return CachedSkillEffectManager;
}

UBrokenDarknessManager *UDamageCalculator::GetBrokenDarknessManager(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	return Actor->FindComponentByClass<UBrokenDarknessManager>();
}

float UDamageCalculator::GetStatusEffectDamageModifier(AActor *Attacker, AActor *Defender) const
{
	float Modifier = 1.0f;

	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (!StatusManager)
	{
		return Modifier;
	}

	// Attacker damage buffs/debuffs
	if (Attacker)
	{
		float DamageBuff = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::DamageBuff);
		float DamageDebuff = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::DamageDebuff);
		Modifier *= (1.0f + (DamageBuff - DamageDebuff) / 100.0f);
	}

	// Defender defense modifiers affect incoming damage indirectly
	// (handled separately in defense calculations)
	// TODO: Add DamageTakenBuff/Debuff to ESkillEffectType if needed

	return FMath::Max(0.0f, Modifier);
}

UCombatGridSubsystem *UDamageCalculator::GetCombatGridSubsystem() const
{
	if (!CachedCombatGridSubsystem)
	{
		if (UGameInstance *GI = GetGameInstance())
		{
			CachedCombatGridSubsystem = GI->GetSubsystem<UCombatGridSubsystem>();
		}
	}
	return CachedCombatGridSubsystem;
}