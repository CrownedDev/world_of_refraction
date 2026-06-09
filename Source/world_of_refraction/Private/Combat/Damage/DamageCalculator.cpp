// DamageCalculator.cpp
// Centralized damage calculation implementation

#include "Combat/Damage/DamageCalculator.h"
#include "Combat/CombatConstants.h"
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
			if (Input.ActionType != EActionType::Spell)
			{
				const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Attacker);
				AttackerMult += Bonus.BonusRawDamage * CombatConstants::RAW_DAMAGE_PER_POINT;
			}
			// L2 (spell) — DRY-sourced from the shared helper, which returns the
			// IDENTICAL BonusSpellDamage × SPELL_DAMAGE_PER_POINT. Same additive op,
			// same spot (after ActionMods on L1). The outer Loadout guard is kept so
			// the add only fires when a loadout exists, matching prior behaviour.
			else if (UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>())
			{
				AttackerMult += AttackerComp->GetEquipmentSpellDamageTerm();
			}
		}
	}

	Result.AttackerDamageMultiplier = AttackerMult;
	RunningDamage *= AttackerMult;

	// Step 1.25: Attached augment-stone raw-damage multiplier. Live-resolves the
	// active weapon's attachment from the attacker's loadout — physical actions
	// only, matching the equipment-bonus gate above. Tiered base% only, applied as
	// a DIRECT multiplier (these are whole-number percentages, not per-point
	// fractions, so RAW_DAMAGE_PER_POINT does not apply).
	if (Attacker && Input.ActionType != EActionType::Spell)
	{
		if (ULoadoutComponent *Loadout = Attacker->FindComponentByClass<ULoadoutComponent>())
		{
			if (const FWeaponLoadoutEntry *ActiveWeapon = Loadout->GetActiveWeaponLoadout())
			{
				const FRuntimeAttachedItem &Attachment = ActiveWeapon->WeaponEntry.GetAttachedItem();
				// Route through the fusion-aware chokepoint. For a plain DamageStone this
				// is byte-identical to GetDamageStoneBasePercent (StoneTargetStat(DamageStone)
				// == RawDamage, same GetStoneBasePercent curve); a fusion's DamageStone
				// half(s) + RawDamage bonus now contribute via GetAttachedStonePercent.
				if (Attachment.IsAugmentStone() || Attachment.IsFusion())
				{
					const float DamageStonePercent =
						CrystalEffectTable::GetAttachedStonePercent(Attachment, ESubStat::RawDamage);
					const float BeforeDamageStone = RunningDamage;
					RunningDamage *= (1.0f + DamageStonePercent / CombatConstants::STAT_PERCENT_DIVISOR);
					UE_LOG(LogTemp, Verbose,
						   TEXT("[DamageCalculator] Damage stone +%.0f%% raw: %.1f -> %.1f"),
						   DamageStonePercent, BeforeDamageStone, RunningDamage);
				}
			}
		}
	}

	// Step 1.25b: Attached augment-stone SPELL-damage multiplier — the magical mirror
	// of the physical block above, gated to Spell actions (mutually exclusive with the
	// != Spell gate above, so a given action runs at most one of the two). DRY-sourced
	// from GetStoneSpellDamageFactor(), which returns the IDENTICAL fusion-aware
	// (1 + GetAttachedStonePercent(.., SpellDamage)/100): for a non-stone / non-fusion
	// attachment (or no active weapon) GetAttachedStonePercent is 0, so the factor is
	// 1.0 exactly as the prior IsAugmentStone()||IsFusion() guard's skip. Same
	// multiplicative op, same spot. A fusion's SpellDamageStone half(s) still flow.
	if (Attacker && Input.ActionType == EActionType::Spell)
	{
		if (UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>())
		{
			RunningDamage *= AttackerComp->GetStoneSpellDamageFactor();
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

	// Step 2.5: [-100%, +100%] normalization — cap the CHARACTER SpellDamage modifier to [0, 2]
	// (SPELL only). Recompose the getter's exact (L1+L2)×L3×L4 product as a standalone UNCLAMPED
	// scalar from the same layer helpers (GetEffectiveSpellDamage itself now clamps, so we read
	// its components directly) — RawCharMod equals GetEffectiveSpellDamage()'s pre-clamp value,
	// so the cast and BD/wear cap the IDENTICAL quantity. Apply clamped/raw as a scalar correction
	// on RunningDamage. Below 2.0, clamped == raw → Correction is EXACTLY 1.0f → RunningDamage
	// unchanged → byte-identical normal casts. ActionMods (folded into L1 with the multiplier),
	// Grid, and defender terms are deliberately OUTSIDE this product — call-specific, left uncapped.
	// A scalar correction commutes with those, so placement right after Step 2 is purely for clarity.
	if (Input.ActionType == EActionType::Spell && Attacker)
	{
		if (UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>())
		{
			const float RawCharMod =
				(AttackerComp->GetEvolutionModifiedSpellDamage() + AttackerComp->GetEquipmentSpellDamageTerm())
				* AttackerComp->GetStoneSpellDamageFactor() * AttackerComp->GetTransientSpellDamageFactor();
			const float ClampedCharMod =
				FMath::Clamp(RawCharMod, CombatConstants::STAT_MODIFIER_MIN, CombatConstants::STAT_MODIFIER_MAX);
			const float Correction = (RawCharMod > KINDA_SMALL_NUMBER) ? (ClampedCharMod / RawCharMod) : 1.0f;
			RunningDamage *= Correction;
		}
	}

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

		// Luck-driven crit bonus. RawLuck/LUCK_RAW_MAX is upper-clamped to 1 (positive
		// luck plateaus at +LUCK_CRIT_BONUS_MAX) but NOT lower-clamped: negative luck
		// (curse) flows through as a negative bonus, so ×(1 + LuckCritBonus) reduces
		// crit. An extremely negative bonus can drive the local CritChance below 0;
		// the roll (FRand() in [0,1) < CritChance) then never crits — the correct
		// cursed outcome. This local never escapes Step 4, so the negative cannot
		// reach the AI scorer (which reads GetCriticalChance, sans luck).
		// GetEquipmentModifiedLuck folds in crystal Spirit modifier and
		// active-loadout BonusLuck.
		if (Attacker)
		{
			if (UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>())
			{
				const float RawLuck = AttackerComp->GetEquipmentModifiedLuck();
				const float LuckCritBonus = FMath::Min(RawLuck / CombatConstants::LUCK_RAW_MAX, 1.0f) * CombatConstants::LUCK_CRIT_BONUS_MAX;
				CritChance *= (1.0f + LuckCritBonus);
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

			// Attached DefenseStone — a PERMANENT, equipment-derived defense
			// multiplier from the defender's OWN active weapon attachment
			// (live-resolved, not cached). Distinct from the timed DefenseBuff/
			// DefenseDebuff layer below. Inert (×1) unless a DefenseStone is
			// attached — GetAttachedStonePercent returns 0 for any other attachment.
			if (const FWeaponLoadoutEntry *ActiveWeapon = Loadout->GetActiveWeaponLoadout())
			{
				const FRuntimeAttachedItem &Attachment = ActiveWeapon->WeaponEntry.GetAttachedItem();
				const float StonePct =
					CrystalEffectTable::GetAttachedStonePercent(Attachment, ESubStat::Defense);
				BaseDefense = FMath::RoundToInt(BaseDefense * (1.0f + StonePct / CombatConstants::STAT_PERCENT_DIVISOR));
			}
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
		float Modifier = 1.0f + (DefenseBuff - DefenseDebuff) / CombatConstants::STAT_PERCENT_DIVISOR;
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

	// Equipment stat bonus — BonusCritChance is a float percentage; it compounds
	// multiplicatively as ×(1 + BonusCritChance/100) on the running crit value.
	if (Attacker)
	{
		if (ULoadoutComponent *Loadout = Attacker->FindComponentByClass<ULoadoutComponent>())
		{
			const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Attacker);
			BaseCrit *= (1.0f + Bonus.BonusCritChance / 100.0f);

			// Attached CritStone — a PERMANENT, equipment-derived crit multiplier from
			// the ATTACKER's OWN active weapon attachment (live-resolved, not cached).
			// Multiplicative ×(1 + pct/100), consistent with the rest of this chain.
			// Inert (×1) unless a CritStone is attached — GetAttachedStonePercent returns
			// 0 for any other attachment (stat-match guard). Mirrors the DefenseStone
			// hook in GetDefenderFlatDefense, applied on the attacker side.
			if (const FWeaponLoadoutEntry *ActiveWeapon = Loadout->GetActiveWeaponLoadout())
			{
				const FRuntimeAttachedItem &Att = ActiveWeapon->WeaponEntry.GetAttachedItem();
				const float StonePct =
					CrystalEffectTable::GetAttachedStonePercent(Att, ESubStat::CritChance);
				BaseCrit *= (1.0f + StonePct / CombatConstants::STAT_PERCENT_DIVISOR);
			}
		}
	}

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
	// scales with the caster's FULL composed spell power, not status-buildup amplification.
	// GetEffectiveSpellDamage = innate (crystal-aware Mind) + equipment BonusSpellDamage,
	// ×SpellDamageStone ×SpellDamageBuff transient — so spell gear/buffs now boost heals
	// (Crown's call). Byte-identical to the prior pillar-only …ForHealing read for a bare
	// healer (no equipment/stone/buff). The ModifyHealing layer below is a DISTINCT
	// heal-specific transient — applied on top, no double-count.
	if (Healer)
	{
		if (UCharacterDataComponent *HealerComp = Healer->FindComponentByClass<UCharacterDataComponent>())
		{
			Healing *= HealerComp->GetEffectiveSpellDamage();
		}
	}

	// Skill-effect-driven healing modifier (passive-layer ModifyHealing).
	if (Healer)
	{
		if (USkillEffectManager *StatusManager = GetSkillEffectManager())
		{
			float ModifyHeal = StatusManager->GetTotalStatModifier(Healer, ESkillEffectType::ModifyHealing);
			Healing *= (1.0f + ModifyHeal / CombatConstants::STAT_PERCENT_DIVISOR);
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
	return 1.0f + FMath::Max(0.0f, Modify / CombatConstants::STAT_PERCENT_DIVISOR);
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

		// RawDamageBuff/Debuff: whole-number-percent boost to PHYSICAL outgoing
		// damage only (ActionType != Spell). The Amethyst gamble is the only live
		// producer and emits percent magnitudes; spell actions are unaffected.
		if (ActionType != EActionType::Spell)
		{
			float RawBuff = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::RawDamageBuff);
			float RawDebuff = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::RawDamageDebuff);
			const float BeforeRaw = Modifier;
			Modifier *= (1.0f + (RawBuff - RawDebuff) / CombatConstants::STAT_PERCENT_DIVISOR);
			UE_LOG(LogTemp, Verbose,
				   TEXT("[DamageCalculator] RawDamage status +%.1f%% / -%.1f%%: mult %.3f -> %.3f"),
				   RawBuff, RawDebuff, BeforeRaw, Modifier);
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