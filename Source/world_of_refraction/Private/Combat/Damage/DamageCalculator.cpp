// DamageCalculator.cpp
// Centralized damage calculation implementation

#include "Combat/Damage/DamageCalculator.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Equipment/Weapons/WeaponAttackData.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Engine/GameInstance.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/WeaponManager.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/Crystals/CrystalEffectTable.h"

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

	// Step 1.25: Attached-whetstone raw-damage multiplier. Live-resolves the
	// active weapon's attachment from the attacker's loadout — physical actions
	// only, matching the equipment-bonus gate above. Tiered base% + flat attached
	// bonus, applied as a DIRECT multiplier (these are whole-number percentages,
	// not per-point fractions, so RAW_DAMAGE_PER_POINT does not apply).
	if (Attacker && Input.ActionType != EActionType::Spell)
	{
		if (ULoadoutComponent *Loadout = Attacker->FindComponentByClass<ULoadoutComponent>())
		{
			if (const FWeaponLoadoutEntry *ActiveWeapon = Loadout->GetActiveWeaponLoadout())
			{
				const FRuntimeAttachedItem &Attachment = ActiveWeapon->WeaponEntry.GetAttachedItem();
				const bool bIsWhetstone =
					Attachment.IsRefined() && Attachment.Refined.Id.Type == ECrystalType::Whetstone;
				if (bIsWhetstone)
				{
					const float WhetstonePercent =
						CrystalEffectTable::GetWhetstoneBasePercent(Attachment.Refined.Id) +
						CombatConstants::WHETSTONE_ATTACHED_BONUS;
					const float BeforeWhetstone = RunningDamage;
					RunningDamage *= (1.0f + WhetstonePercent / 100.0f);
					UE_LOG(LogTemp, Verbose,
						   TEXT("[DamageCalculator] Whetstone +%.0f%% raw: %.1f -> %.1f"),
						   WhetstonePercent, BeforeWhetstone, RunningDamage);
				}
			}
		}
	}

	// Step 1.5: Grid position damage modifier (attacker)
	UCombatGridSubsystem *Grid = GetCombatGridSubsystem();
	if (Grid)
	{
		float GridDamageMod = Grid->GetDamageModifier(Attacker);
		RunningDamage *= GridDamageMod;
	}

	// Step 2: Status effect modifiers (buffs/debuffs)
	float StatusMod = GetStatusEffectDamageModifier(Attacker, Defender, Input.ActionType);
	RunningDamage *= StatusMod;

	// Step 3: Element interaction — no elemental advantage system; always neutral.
	Result.ElementMultiplier = 1.0f;

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

		// GuaranteedCrit (passive skill-effect): forces a crit when active on attacker.
		bool bForceCrit = false;
		if (USkillEffectManager *CritMgr = GetSkillEffectManager())
		{
			bForceCrit = CritMgr->HasEffectOfType(Attacker, ESkillEffectType::GuaranteedCrit);
		}
		Result.bWasCritical = bForceCrit || (FMath::FRand() < CritChance);

		if (Result.bWasCritical)
		{
			const float CritMult = DamageConstants::CRIT_MULTIPLIER * GetCritDamageMultiplier(Attacker);
			Result.CritMultiplier = CritMult;
			RunningDamage *= CritMult;
		}
	}

	// Store damage before defense
	Result.DamageBeforeDefense = FMath::RoundToInt(RunningDamage);

	// Step 5: Defender's flat defense — Input.bIgnoreDefense OR the attacker
	// having an IgnoreDefense skill effect both skip this step.
	bool bSkipDefense = Input.bIgnoreDefense;
	if (!bSkipDefense && Attacker)
	{
		if (USkillEffectManager *DefMgr = GetSkillEffectManager())
		{
			bSkipDefense = DefMgr->HasEffectOfType(Attacker, ESkillEffectType::IgnoreDefense);
		}
	}
	if (!bSkipDefense && Defender)
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

	// Crystal-aware Spell/Raw damage multiplier — uses GetEvolutionModifiedMind/Body
	// so the slotted primary evolution crystal's pillar modifier feeds the curve.
	if (ActionType == EActionType::Spell)
	{
		return AttackerComp->GetEvolutionModifiedSpellDamage();
	}
	else
	{
		return AttackerComp->GetEvolutionModifiedRawDamage();
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

	// Crystal-aware flat defense — uses GetEvolutionModifiedBody so the slotted
	// primary evolution crystal's Body pillar modifier feeds the curve.
	int32 BaseDefense = DefenderComp->GetEvolutionModifiedFlatDefense();

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

	// Crystal-aware crit chance — uses GetEvolutionModifiedMind so the slotted
	// primary evolution crystal's Mind pillar modifier feeds the curve.
	float BaseCrit = AttackerComp->GetEvolutionModifiedCritChance();

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

		// Passive-layer ModifyCritChance: additive percent on top of buff/debuff.
		float ModifyCrit = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::ModifyCritChance);
		BaseCrit += ModifyCrit / 100.0f;
	}

	return FMath::Clamp(BaseCrit, 0.0f, DamageConstants::MAX_CRIT_CHANCE);
}

bool UDamageCalculator::RollCriticalHit(AActor *Attacker, float OverrideChance) const
{
	float Chance = OverrideChance >= 0.0f ? OverrideChance : GetCriticalChance(Attacker);
	return FMath::FRand() < Chance;
}

// ==================== STATUS EFFECT CALCULATIONS ====================
// CalculateStatusBuildup was dead code (zero callers) and was removed in
// feature/integration-gaps-sweep-3. The live buildup pipeline is
// UStatusBuildupManager::AddStatusBuildup — it applies the attacker's
// crystal-aware StatusMultiplier + equipment bonus, then (sweep-3) the
// StatusMultiplierBuff/Debuff skill-effect deltas, then (fix-bd-stack-multiplier)
// the BD absorption-stack amplification via UBrokenDarknessManager::
// GetElementStackStatusMultiplier as step 5c, then the defender's element-
// filtered Resistance reduction.
//
// GetBDStackStatusMultiplier was a thin wrapper over BDManager methods that
// lost its only caller when CalculateStatusBuildup was deleted (leaving BD
// stacks inert). Removed in feature/fix-bd-stack-multiplier; the element-gated
// accessor moved onto the manager itself.

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
	// Crystal-aware: GetEvolutionModifiedSpellDamageForHealing reads
	// GetEvolutionModifiedMind, so the slotted primary evolution crystal's Mind
	// pillar modifier feeds the curve.
	if (Healer)
	{
		if (UCharacterDataComponent *HealerComp = Healer->FindComponentByClass<UCharacterDataComponent>())
		{
			Healing *= HealerComp->GetEvolutionModifiedSpellDamageForHealing();
		}
	}

	// Skill-effect-driven healing modifier (passive-layer ModifyHealing).
	if (Healer)
	{
		if (USkillEffectManager *StatusManager = GetSkillEffectManager())
		{
			float ModifyHeal = StatusManager->GetTotalStatModifier(Healer, ESkillEffectType::ModifyHealing);
			Healing *= (1.0f + ModifyHeal / 100.0f);
		}
	}

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

float UDamageCalculator::GetCritDamageMultiplier(AActor *Attacker) const
{
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (!StatusManager || !Attacker)
	{
		return 1.0f;
	}
	const float Modify = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::ModifyCritDamage);
	return 1.0f + FMath::Max(0.0f, Modify / 100.0f);
}

float UDamageCalculator::GetStatusEffectDamageModifier(AActor *Attacker, AActor *Defender, EActionType ActionType) const
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

		// Passive-layer ModifyDamageDealt: percent boost to outgoing damage.
		float ModifyDealt = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::ModifyDamageDealt);
		Modifier *= (1.0f + ModifyDealt / 100.0f);

		// RawDamageBuff/Debuff: whole-number-percent boost to PHYSICAL outgoing
		// damage only (ActionType != Spell). The Amethyst gamble is the only live
		// producer and emits percent magnitudes; spell actions are unaffected.
		if (ActionType != EActionType::Spell)
		{
			float RawBuff = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::RawDamageBuff);
			float RawDebuff = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::RawDamageDebuff);
			const float BeforeRaw = Modifier;
			Modifier *= (1.0f + (RawBuff - RawDebuff) / 100.0f);
			UE_LOG(LogTemp, Verbose,
				   TEXT("[DamageCalculator] RawDamage status +%.1f%% / -%.1f%%: mult %.3f -> %.3f"),
				   RawBuff, RawDebuff, BeforeRaw, Modifier);
		}
	}

	// Defender-side damage-taken modifier — applies to incoming damage regardless
	// of attacker. Defense-flat-reduction is handled separately in CalculateDamage.
	if (Defender)
	{
		// ModifyDamageTaken was split (item-system-redesign Phase 1): ReduceDamageTaken
		// (buff) and IncreaseDamageTaken (debuff) are now separate, both POSITIVE
		// magnitude. CONTENT PASS REQUIRED: any asset still carrying a negative
		// ModifyDamageTaken magnitude redirects to ReduceDamageTaken by name but
		// keeps the old sign — a negative reduction reads as zero/negative here.
		// Re-author those magnitudes as positive percent values.
		float Reduction = StatusManager->GetTotalStatModifier(Defender, ESkillEffectType::ReduceDamageTaken);
		float Increase = StatusManager->GetTotalStatModifier(Defender, ESkillEffectType::IncreaseDamageTaken);
		// Reduction capped at 90%, increase uncapped
		Reduction = FMath::Min(Reduction, 90.0f);
		Modifier *= (1.0f - Reduction / 100.0f);
		Modifier *= (1.0f + Increase / 100.0f);
	}

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