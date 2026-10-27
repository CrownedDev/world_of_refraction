// DamageCalculator.cpp
// Centralized damage calculation implementation

#include "Combat/Damage/DamageCalculator.h"
#include "Combat/Damage/DamageMath.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Combat/CombatConstants.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Combat/Mechanics/BattleRandomSubsystem.h"
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
	// Checked here as well as in Compute so a no-damage hit takes no crit roll from the battle stream.
	if (Input.BaseDamage <= 0)
	{
		FDamageCalculationResult Result;
		Result.EffectiveElement = Input.Element;
		return Result;
	}
	return WoRDamageMath::Compute(GatherDamageMathInputs(Attacker, Defender, Input));
}

FDamageMathInputs UDamageCalculator::GatherDamageMathInputs(AActor *Attacker, AActor *Defender,
															const FDamageCalculationInput &Input) const
{
	FDamageMathInputs Inputs;
	Inputs.BaseDamage = Input.BaseDamage;
	Inputs.bIsSpell = Input.ActionType == EActionType::Spell;
	Inputs.Element = Input.Element;
	Inputs.ActionMods = Input.ActionMods;

	// Attacker. Cross-stat scaling is authored per skill (StatScaling), so the baseline stat is the
	// action type's default.
	Inputs.AttackerStatMultiplier = GetAttackerDamageMultiplier(Attacker, Input.ActionType);
	UCharacterDataComponent *AttackerComp = Attacker ? Attacker->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (AttackerComp)
	{
		Inputs.bHasAttackerStats = true;
		Inputs.EquipmentTerm = Inputs.bIsSpell ? AttackerComp->GetEquipmentSpellDamageTerm() : AttackerComp->GetEquipmentRawDamageTerm();
		Inputs.StoneFactor = Inputs.bIsSpell ? AttackerComp->GetStoneSpellDamageFactor() : AttackerComp->GetStoneRawDamageFactor();
		Inputs.TransientFactor = Inputs.bIsSpell ? AttackerComp->GetTransientSpellDamageFactor() : AttackerComp->GetTransientRawDamageFactor();
		Inputs.EvolutionModifiedStat = Inputs.bIsSpell ? AttackerComp->GetEvolutionModifiedSpellDamage() : AttackerComp->GetEvolutionModifiedRawDamage();
		// The gear term joins the attacker multiplier only with a loadout (as before B-219).
		if (Attacker->FindComponentByClass<ULoadoutComponent>())
		{
			Inputs.EquipmentTermInMultiplier = Inputs.EquipmentTerm;
		}
	}

	// Souls-style per-skill scaling (stage b2 / b2b): each stat normalised in its own bucket.
	for (const FStatScaling &Entry : Input.StatScaling)
	{
		if (Entry.Stat == ESubStat::None)
		{
			continue;
		}
		FDamageMathScalingTerm Term;
		Term.Coefficient = GetScalingTierCoefficient(Entry.Tier);
		Term.StatFraction = GetScalingFraction(Entry.Stat, GetEffectiveStatForScaling(Attacker, Entry.Stat));
		Inputs.ScalingTerms.Add(Term);
	}

	UCombatGridSubsystem *Grid = GetCombatGridSubsystem();
	if (Grid)
	{
		Inputs.GridDamageModifier = Grid->GetDamageModifier(Attacker);
	}
	Inputs.StatusEffectModifier = GetStatusEffectDamageModifier(Attacker, Defender, Input.ActionType);

	// Crit. The one battle-random roll; skipped when the crit is forced, as before.
	Inputs.bCanCrit = Input.bCanCrit;
	if (Input.bCanCrit)
	{
		Inputs.CritChance = Input.OverrideCritChance >= 0.0f ? Input.OverrideCritChance : GetCriticalChance(Attacker);
		if (USkillEffectManager *CritMgr = GetSkillEffectManager())
		{
			Inputs.bForceCrit = CritMgr->HasEffectOfType(Attacker, ESkillEffectType::GuaranteedCrit);
		}
		if (!Inputs.bForceCrit)
		{
			Inputs.CritRoll = UBattleRandomSubsystem::FRand(this);
		}
		// GetCritDamageMultiplier is the full multiplier (x1.5 base + stat ramp, then gear/transient).
		Inputs.CritDamageMultiplier = GetCritDamageMultiplier(Attacker);
	}

	// Defender. Input.bIgnoreDefense or the attacker's IgnoreDefense effect skips defense.
	bool bSkipDefense = Input.bIgnoreDefense;
	if (!bSkipDefense && Attacker)
	{
		if (USkillEffectManager *DefMgr = GetSkillEffectManager())
		{
			bSkipDefense = DefMgr->HasEffectOfType(Attacker, ESkillEffectType::IgnoreDefense);
		}
	}
	Inputs.bIgnoreDefense = bSkipDefense;

	// Element resistance (D-005 C, D-036): the defender's per-element resistance; Ice resistance
	// also resists Water (Q-145b). Non-elemental hits read none: physical is Defense only (Q-145).
	if (!Input.bIgnoreResistance)
	{
		Inputs.ElementResistance = WoRResistance::GetElementDamageResistance(Defender, Input.Element);
	}

	if (!bSkipDefense && Defender)
	{
		Inputs.bApplyDefense = true;
		Inputs.DefenderDefense = GetDefenderFlatDefense(Defender);
	}
	if (Grid && Defender)
	{
		Inputs.GridDefenseModifier = Grid->GetDefenseModifier(Defender);
	}
	return Inputs;
}

FDamageCalculationResult UDamageCalculator::CalculateAttackDamage(
	AActor *Attacker,
	AActor *Target,
	USkillDataBase *Attack,
	bool bIsInfused)
{
	// RETIREMENT NOTE: no live callers — AI attack scoring now routes through
	// EstimateAbilityDamage (attacks are UAbilityData), the execution-accurate path that
	// applies req-gap / tier-gap / tier-power / expected-crit. Kept (not deleted) pending
	// confirmation; candidate for removal.
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
	Input.ActionType = EActionType::Ability; // attack/ability merge: Attack folded into Ability (both scale RawDamage)

	// Requirement scaling now flows per-pillar through ComputeActionStatModifiers, NOT a flat
	// penalty here — the old (1 - sqrt-deficit) requirement-penalty multiply was removed
	// (RequirementGapScaling). CalculateRequirementPenalty has no C++ reader (Blueprint only);
	// the BD strain gate reads GetTotalDeficit. It is no longer applied to damage.

	// Attacks are physical unless infused. Infused attacks still scale by RawDamage; their element
	// steers status and, since B-064, the defender's element resistance.
	Input.Element = bIsInfused ? AttackerData->InnateElement : ESpellElement::None;

	Input.bCanCrit = true;
	Input.bWasInfused = bIsInfused;
	Input.HitCount = Attack->HitCount;

	// Per-instance weapon StatBonus (BonusRawDamage / BonusCritDamage) is no longer
	// read directly here — it flows through the composed getters
	// (GetEvolutionModifiedRawDamage / GetCritDamageMultiplier), so reading it again
	// here would double-count.

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

float UDamageCalculator::GetEffectiveStatForScaling(AActor *Attacker, ESubStat Stat) const
{
	if (!Attacker)
	{
		return 0.0f;
	}

	UCharacterDataComponent *Comp = Attacker->FindComponentByClass<UCharacterDataComponent>();
	if (!Comp || !Comp->CharacterData)
	{
		return 0.0f;
	}

	// Returns each stat's composed, crystal-aware EFFECTIVE value in its OWN units; GetScalingFraction
	// (b2b) normalizes per bucket. RawDamage/SpellDamage reuse the SAME GetEvolutionModified* getters the
	// baseline AttackerMult uses (consistency). Resistance reads the CAPPED [0,0.5] accessor (not the
	// unclamped GetEffectiveResistance) so its bucket-B normalization divides against a value that can't
	// exceed the cap. Reflex/TurnSpeed are asset-intrinsic (mirrors the TurnSpeed pattern).
	switch (Stat)
	{
	case ESubStat::RawDamage:        return Comp->GetEvolutionModifiedRawDamage();
	case ESubStat::SpellDamage:      return Comp->GetEvolutionModifiedSpellDamage();
	case ESubStat::ActionSpeed:      return Comp->GetEffectiveActionSpeed();
	case ESubStat::SpellSpeed:       return Comp->GetEffectiveSpellSpeed();
	case ESubStat::StatusMultiplier: return Comp->GetEffectiveStatusMultiplier();
	case ESubStat::CritDamage:       return GetCritDamageMultiplier(Attacker);
	case ESubStat::Resistance:       return Comp->GetCrystalResistanceStatCapped();
	case ESubStat::Efficiency:       return Comp->GetEffectiveEfficiencyMultiplier();
	case ESubStat::Luck:             return Comp->GetEquipmentModifiedLuck();
	case ESubStat::Defense:          return Comp->GetEvolutionModifiedFlatDefense();
	case ESubStat::TurnSpeed:        return Comp->CharacterData->CalculateTurnSpeed();
	case ESubStat::Reflex:           return Comp->CharacterData->CalculateReflexWindowBonus();
	case ESubStat::None:
	default:                         return 0.0f;
	}
}

float UDamageCalculator::GetDefenderFlatDefense(AActor *Defender) const
{
	if (!Defender)
	{
		return 0.0f;
	}

	UCharacterDataComponent *DefenderComp = Defender->FindComponentByClass<UCharacterDataComponent>();
	if (!DefenderComp || !DefenderComp->CharacterData)
	{
		return 0.0f;
	}

	// Pattern P (cluster 5a, revised) — stat-capped, gear-beyond. The crystal-aware STAT reduction
	// is capped ALONE at UNIVERSAL_STAT_CAP (0.5 = the stat ceiling); THEN stone/buff MULTIPLY it
	// OUTSIDE that clamp, scaling the capped stat upward toward the DEFENSE_REDUCTION_CEILING (0.85) hard ceiling.
	// Multiplicative by design intent (Crown): a +X% stone/buff rewards a high-defense build more
	// than a low one (0.5 × 1.3 = 0.65 vs 0.2 × 1.3 = 0.26). GetEvolutionModifiedBody feeds the
	// slotted crystal's Body pillar into the curve.
	float Reduction = FMath::Min(DefenderComp->GetEvolutionModifiedFlatDefense(), CombatConstants::UNIVERSAL_STAT_CAP);

	if (ULoadoutComponent *Loadout = Defender->FindComponentByClass<ULoadoutComponent>())
	{
		// Equipment BonusDefense gear (A3) — reinterpreted as a % MULTIPLIER on the capped stat
		// reduction (option-ii: the per-point magnitude read as a FRACTION, same shape as every other
		// 5e gear field). Composes multiplicatively with the stone + buff below, OUTSIDE the 0.5 stat
		// cap, toward the DEFENSE_REDUCTION_CEILING (0.85) ceiling (final clamp). ×1 (inert) when BonusDefense is 0.
		Reduction *= (1.0f + Loadout->GetActiveStatBonus(Defender).BonusDefense * CombatConstants::DEFENSE_PER_POINT);

		// Attached DefenseStone — a % MULTIPLIER (×(1 + StonePct/100)) on the capped stat reduction,
		// OUTSIDE the 0.5 cap, from the defender's OWN active weapon and ring (B-082) attachments
		// (live-resolved, not cached). Inert (×1) unless a DefenseStone is attached.
		Reduction *= (1.0f + Loadout->GetAttachedStonePercent(ESubStat::Defense) / CombatConstants::STAT_PERCENT_DIVISOR);
	}

	// Combat-buff/debuff modifiers (from skill casts, e.g. Stoneskin) — MULTIPLICATIVE transient: the
	// DefenseBuff/Debuff net scales the (permanent-gear) reduction proportionally, ×(1 + net/100), past
	// the 0.5 stat cap toward the DEFENSE_REDUCTION_CEILING (0.85) ceiling. Max(0,…) floors a ≥100% debuff
	// at ×0 (never inverts); buff/debuff in lockstep. The final clamp bounds the result to [0, 0.85].
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (StatusManager)
	{
		Reduction *= FMath::Max(0.0f, 1.0f + StatusManager->GetNetStatModifier(Defender, ESubStat::Defense) / CombatConstants::STAT_PERCENT_DIVISOR);
	}

	return FMath::Clamp(Reduction, 0.0f, CombatConstants::DEFENSE_REDUCTION_CEILING);
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

	// (5e-C2) Crit-chance gear/stone REMOVED here — crit chance now sources entirely from Luck:
	// BaseCrit above = GetEvolutionModifiedCritChance -> GetLuckModifiedChance, which already folds in
	// BonusLuck gear. The old BonusCritChance multiply is dropped to avoid double-counting (that Mind
	// gear field is renamed BonusCritDamage and now drives crit DAMAGE via GetCritDamageMultiplier).
	// The CritStone read is RETIRED here rather than repointed to a luck stone: a crit-ONLY luck channel
	// wouldn't apply to break-skip/dodge. CritStone's final home (a crit-DAMAGE stone, or a Luck stone
	// wired into GetEquipmentModifiedLuck so it boosts ALL luck consumers) is a 5e-C3 content decision.

	// Combat-buff/debuff modifiers (from skill casts).
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (StatusManager)
	{
		float CritBuff = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::CritChanceBuff);
		float CritDebuff = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::CritChanceDebuff);

		// Multiplicative compounding: GetTotalStatModifier already sums within each
		// category, so each category applies as one factor on the running crit value.
		// Debuff is ×(1 − pct), NOT ×(1 + pct) — a 30% crit debuff is ×0.7.
		BaseCrit *= (1.0f + CritBuff / 100.0f);
		BaseCrit *= (1.0f - CritDebuff / 100.0f);

		// Passive-layer ModifyCritChance: compounds as another multiplicative factor.
		float ModifyCrit = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::ModifyCritChance);
		BaseCrit *= (1.0f + ModifyCrit / 100.0f);
	}

	return FMath::Clamp(BaseCrit, 0.0f, 1.0f);
}

bool UDamageCalculator::RollCriticalHit(AActor *Attacker, float OverrideChance) const
{
	float Chance = OverrideChance >= 0.0f ? OverrideChance : GetCriticalChance(Attacker);
	return UBattleRandomSubsystem::FRand(this) < Chance;
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

// ==================== DEBUG ====================

void UDamageCalculator::DebugPrintCalculation(const FDamageCalculationResult &Result) const
{
	UE_LOG(LogTemp, Display, TEXT("=== DAMAGE CALCULATION ==="));
	UE_LOG(LogTemp, Display, TEXT("Damage Before Defense: %d"), Result.DamageBeforeDefense);
	UE_LOG(LogTemp, Display, TEXT("Attacker Multiplier: %.2fx"), Result.AttackerDamageMultiplier);
	UE_LOG(LogTemp, Display, TEXT("Element Multiplier: %.2fx"), Result.ElementMultiplier);
	UE_LOG(LogTemp, Display, TEXT("Critical: %s (%.2fx)"), Result.bWasCritical ? TEXT("YES") : TEXT("NO"), Result.CritMultiplier);
	UE_LOG(LogTemp, Display, TEXT("Defense Reduction: %.2f (blocked %d)"), Result.DefenderFlatDefense, Result.DamageBlockedByDefense);
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
	// Returns the FULL crit-damage multiplier. Crown-locked (5e-B-fix): un-invested crit = CRIT_DMG_BASE
	// (x1.0 = normal damage); the crit-damage stat ramps it to x1.5 (stat-ALONE cap); gear/transient
	// MULTIPLY past toward x2.0 (Pattern P / cluster 5). The fixed x1.5 CRIT_MULTIPLIER base is GONE
	// from the crit path — a crit now does NOTHING extra without crit-damage investment.
	if (!Attacker)
	{
		return CombatConstants::CRIT_DMG_BASE; // x1.0 (normal damage) when no attacker
	}

	// Stat-derived crit damage: CRIT_DMG_BASE (x1.0) + the crit-damage stat ramp (Mind x CritDamage
	// points x CRIT_DAMAGE_PER_POINT, up to +0.5), capped ALONE at CRIT_DAMAGE_STAT_CAP (x1.5). The Mind
	// CritDamage substat drives crit DAMAGE; crit CHANCE is Luck-driven (5e-C2). Gear/transient multiply
	// the capped stat past x1.5 below.
	float StatCritDmg = CombatConstants::CRIT_DMG_BASE;
	if (UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>())
	{
		if (AttackerComp->CharacterData)
		{
			const float ModifiedMind = AttackerComp->GetEvolutionModifiedMind();
			const int32 CritDmgPoints = AttackerComp->CharacterData->GetTotalCritDamage();
			StatCritDmg = FMath::Min(
				CombatConstants::CRIT_DMG_BASE + (ModifiedMind * CritDmgPoints * CombatConstants::CRIT_DAMAGE_PER_POINT),
				CombatConstants::CRIT_DAMAGE_STAT_CAP);
		}
	}

	// Gear + transient MULTIPLY the capped stat past x1.5 toward CRIT_DAMAGE_GEAR_CEILING (cluster-5
	// shape). x1 (inert) when none, so a zero-crit-stat character with no gear/transient is exactly
	// CRIT_DMG_BASE (x1.0 = normal).
	float Result = StatCritDmg;

	// Equipment BonusCritDamage gear — MULTIPLIES (option-ii: the per-point magnitude is read as a
	// fraction, same shape as RawDamage/SpellDamage gear in 5a). 5e-C3 wired this now that the field
	// was renamed BonusCritChance->BonusCritDamage and freed from the crit-CHANCE path (5e-C2).
	if (ULoadoutComponent *Loadout = Attacker->FindComponentByClass<ULoadoutComponent>())
	{
		const float BonusCritDamage = Loadout->GetActiveStatBonus(Attacker).BonusCritDamage;
		Result *= (1.0f + BonusCritDamage * CombatConstants::CRIT_DAMAGE_PER_POINT);

		// Attached CritStone (5f-A) — a % MULTIPLIER on the capped stat, from the attacker's OWN weapon
		// and ring attachments (B-082). StoneTargetStat(CritStone) == ESubStat::CritDamage, so GetAttachedStonePercent
		// returns 0 (factor x1, inert) unless a CritStone is attached. Mirrors GetStoneRawDamageFactor.
		Result *= (1.0f + Loadout->GetAttachedStonePercent(ESubStat::CritDamage) / CombatConstants::STAT_PERCENT_DIVISOR);
	}

	// Transient crit-damage modifiers — MULTIPLY too. Directional (5f-C): ModifyCritDamage is the BUFF
	// ("Crit Damage Up", up-only — Max(0,..) guards malformed negative assets); CritDamageDebuff is its
	// paired debuff (positive magnitude, SUBTRACTED). Net (buff − debuff) can go negative, dragging crit
	// damage down — the final clamp floors the whole multiplier at CRIT_DMG_BASE (x1.0), so a crit-damage
	// debuff can cancel the crit BONUS toward a normal hit but never make a crit weaker than one.
	if (USkillEffectManager *StatusManager = GetSkillEffectManager())
	{
		// The table's CritDamage row floors ModifyCritDamage at 0 before subtracting the debuff.
		Result *= (1.0f + StatusManager->GetNetStatModifier(Attacker, ESubStat::CritDamage) / CombatConstants::STAT_PERCENT_DIVISOR);
	}
	return FMath::Clamp(Result, CombatConstants::CRIT_DMG_BASE, CombatConstants::CRIT_DAMAGE_GEAR_CEILING);
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
		Modifier *= (1.0f + (DamageBuff - DamageDebuff) / CombatConstants::STAT_PERCENT_DIVISOR);

		// Passive-layer ModifyDamageDealt: percent boost to outgoing damage.
		float ModifyDealt = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::ModifyDamageDealt);
		Modifier *= (1.0f + ModifyDealt / CombatConstants::STAT_PERCENT_DIVISOR);

		// RawDamageBuff/Debuff: whole-number-percent boost to PHYSICAL outgoing damage only
		// (ActionType != Spell). DRY-sourced from GetTransientRawDamageFactor(), which returns
		// the IDENTICAL (1 + (RawDamageBuff − RawDamageDebuff)/100) via the same SEM reads on the
		// same actor. Same multiplicative op, same spot/order (symmetric with the spell arm
		// below). The Amethyst gamble is the only live producer; spell actions are unaffected.
		if (ActionType != EActionType::Spell)
		{
			if (UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>())
			{
				Modifier *= AttackerComp->GetTransientRawDamageFactor();
			}
		}

		// SpellDamageBuff/Debuff: the magical mirror — whole-number-percent boost to
		// SPELL outgoing damage only (ActionType == Spell). Symmetric with the physical
		// RawDamageBuff block above. DRY-sourced from GetTransientSpellDamageFactor(),
		// which returns the IDENTICAL (1 + (SpellDamageBuff − SpellDamageDebuff)/100) via
		// the same SEM reads on the same actor. Same multiplicative op, same spot — still
		// bundled here in the SAME order with the other Step-2 terms (generic DamageBuff /
		// ModifyDamageDealt before, defender-side after). This is the read-site that gives
		// the SpellDamageStone CONSUMABLE its teeth and the Amethyst gamble's spell arm.
		if (ActionType == EActionType::Spell)
		{
			if (UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>())
			{
				Modifier *= AttackerComp->GetTransientSpellDamageFactor();
			}
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
		Modifier *= (1.0f - Reduction / CombatConstants::STAT_PERCENT_DIVISOR);
		Modifier *= (1.0f + Increase / CombatConstants::STAT_PERCENT_DIVISOR);
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