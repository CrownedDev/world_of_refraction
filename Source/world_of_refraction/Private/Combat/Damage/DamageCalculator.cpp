// DamageCalculator.cpp
// Centralized damage calculation implementation

#include "Combat/Damage/DamageCalculator.h"
#include "Combat/CombatConstants.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Skills/Definitions/SkillDataBase.h"
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
	// Cross-stat scaling (e.g. a fire-punch scaling off SpellDamage) is now authored per-skill via the
	// StatScaling tiers (the tier loop below), so the old Raw↔Spell stat-swap was retired — the baseline
	// stat is simply the ActionType default.
	float AttackerMult = GetAttackerDamageMultiplier(Attacker, Input.ActionType);
	const ESubStat AttackerStat = (Input.ActionType == EActionType::Spell) ? ESubStat::SpellDamage : ESubStat::RawDamage;
	AttackerMult = Input.ActionMods.ApplyTo(AttackerMult, AttackerStat);

	// Equipment stat bonus — direct read from the attacker's active loadout.
	// Replaces the prior RawDamageBuff/StatusMultiplierBuff status-effect path
	// (never wired in production; that dead chain was deleted in cleanup).
	// Folded into AttackerMult so each rolled point contributes a small
	// fractional multiplier rather than flat damage.
	if (Attacker)
	{
		if (ULoadoutComponent *Loadout = Attacker->FindComponentByClass<ULoadoutComponent>())
		{
			if (Input.ActionType != EActionType::Spell)
			{
				// L2 (physical) — DRY-sourced from the shared helper, which returns the
				// IDENTICAL BonusRawDamage × RAW_DAMAGE_PER_POINT. Mirrors the spell branch
				// below; the outer Loadout guard is kept so the add only fires with a loadout.
				if (UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>())
				{
					AttackerMult += AttackerComp->GetEquipmentRawDamageTerm();
				}
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

	// Souls-style authored per-skill scaling (stage b2 / b2b): each (stat, grade) entry adds
	// GetScalingTierCoefficient(grade) × GetScalingFraction(stat, attacker's effective stat) to the
	// attacker multiplier, additive on top of the baseline above. GetScalingFraction (b2b) normalizes
	// each stat in its OWN bucket (Model Y) so Luck/Defense/Resistance/Efficiency/Reflex contribute
	// instead of the naive StatFraction's 0. EMPTY array → loop runs zero times → TierScalingBonus 0 →
	// byte-identical to pre-b2.
	float TierScalingBonus = 0.0f;
	for (const FStatScaling &Entry : Input.StatScaling)
	{
		if (Entry.Stat == ESubStat::None)
		{
			continue;
		}
		TierScalingBonus += GetScalingTierCoefficient(Entry.Tier) * GetScalingFraction(Entry.Stat, GetEffectiveStatForScaling(Attacker, Entry.Stat));
	}
	AttackerMult += TierScalingBonus;

	Result.AttackerDamageMultiplier = AttackerMult;
	RunningDamage *= AttackerMult;

	// Step 1.25: Attached augment-stone raw-damage multiplier — DRY-sourced from
	// GetStoneRawDamageFactor() (physical actions only; the magical mirror is Step 1.25b).
	// Byte-identical to the prior inline IsAugmentStone()||IsFusion() guard: GetAttachedStonePercent
	// returns 0 for any non-stone / non-fusion attachment (and the 1.0 fallbacks cover no-loadout /
	// no-active-weapon), so the factor is 1.0 exactly where the inline guard skipped. Same
	// multiplicative op, same spot. A fusion's DamageStone half(s) still flow.
	if (Attacker && Input.ActionType != EActionType::Spell)
	{
		if (UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>())
		{
			RunningDamage *= AttackerComp->GetStoneRawDamageFactor();
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

	// Step 2.5: Pattern P (cluster 5a) — stat-capped, gear-beyond (SPELL only). The STAT term
	// (GetEvolutionModifiedSpellDamage) is capped ALONE at STAT_MULT_CAP (×1.5 — the stat ceiling,
	// now ENFORCED in the live path); THEN equipment MULTIPLIES it (×(1+EquipTerm)) and stone/transient
	// apply OUTSIDE that clamp, bounded by the higher STAT_MODIFIER_MAX (×2.0) compose ceiling. So the
	// stat saturates at ×1.5 and gear/stone/buff scale it from there toward ×2.0. We re-derive the target
	// modifier and divide out the UNCLAMPED product currently baked into RunningDamage (Steps 1 /
	// 1.25b / 2) as a scalar correction. Below the caps, target == raw → Correction is 1.0f →
	// byte-identical. ActionMods (folded into L1), Grid, and defender terms are deliberately
	// OUTSIDE this product — call-specific, left uncapped. Mirrors GetEffectiveSpellDamage.
	if (Input.ActionType == EActionType::Spell && Attacker)
	{
		if (UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>())
		{
			const float EquipTerm = AttackerComp->GetEquipmentSpellDamageTerm();
			const float Stone = AttackerComp->GetStoneSpellDamageFactor();
			const float Transient = AttackerComp->GetTransientSpellDamageFactor();
			// Uncapped char modifier currently baked into RunningDamage (the denominator).
			const float RawCharMod =
				(AttackerComp->GetEvolutionModifiedSpellDamage() + EquipTerm) * Stone * Transient;
			// Pattern-P target: stat clamped to ×1.5 ALONE, THEN gear MULTIPLIES (×(1+EquipTerm)),
			// stone/transient beyond, capped ×2.0. EquipTerm read as a FRACTION (option ii).
			const float StatBase =
				FMath::Min(AttackerComp->GetEvolutionModifiedSpellDamage(), CombatConstants::STAT_MULT_CAP);
			const float TargetCharMod =
				FMath::Clamp(StatBase * (1.0f + EquipTerm) * Stone * Transient,
							 CombatConstants::STAT_MODIFIER_MIN, CombatConstants::STAT_MODIFIER_MAX);
			const float Correction = (RawCharMod > KINDA_SMALL_NUMBER) ? (TargetCharMod / RawCharMod) : 1.0f;
			RunningDamage *= Correction;
		}
	}

	// Step 2.6: Pattern P (cluster 5a) — physical mirror of Step 2.5, gated != Spell (the complement,
	// so exactly one of the two fires per action). The STAT term (GetEvolutionModifiedRawDamage) is
	// capped ALONE at STAT_MULT_CAP (×1.5 — now ENFORCED live); THEN equipment MULTIPLIES it
	// (×(1+EquipTerm)) and stone/transient apply OUTSIDE that clamp, bounded by STAT_MODIFIER_MAX
	// (×2.0). Stat saturates at ×1.5; gear/stone/buff scale it toward ×2.0. We divide out the UNCLAMPED product baked into
	// RunningDamage as a scalar correction. Below the caps, target == raw → Correction 1.0f →
	// byte-identical. ActionMods (folded into L1), Grid, and defender terms stay OUTSIDE this
	// product — call-specific, left uncapped. Mirrors GetEffectiveRawDamage.
	if (Input.ActionType != EActionType::Spell && Attacker)
	{
		if (UCharacterDataComponent *AttackerComp = Attacker->FindComponentByClass<UCharacterDataComponent>())
		{
			const float EquipTerm = AttackerComp->GetEquipmentRawDamageTerm();
			const float Stone = AttackerComp->GetStoneRawDamageFactor();
			const float Transient = AttackerComp->GetTransientRawDamageFactor();
			// Uncapped char modifier currently baked into RunningDamage (the denominator).
			const float RawCharMod =
				(AttackerComp->GetEvolutionModifiedRawDamage() + EquipTerm) * Stone * Transient;
			// Pattern-P target: stat clamped to ×1.5 ALONE, THEN gear MULTIPLIES (×(1+EquipTerm)),
			// stone/transient beyond, capped ×2.0. EquipTerm read as a FRACTION (option ii).
			const float StatBase =
				FMath::Min(AttackerComp->GetEvolutionModifiedRawDamage(), CombatConstants::STAT_MULT_CAP);
			const float TargetCharMod =
				FMath::Clamp(StatBase * (1.0f + EquipTerm) * Stone * Transient,
							 CombatConstants::STAT_MODIFIER_MIN, CombatConstants::STAT_MODIFIER_MAX);
			const float Correction = (RawCharMod > KINDA_SMALL_NUMBER) ? (TargetCharMod / RawCharMod) : 1.0f;
			RunningDamage *= Correction;
		}
	}

	// Step 3: Element interaction — no elemental advantage system; always neutral.
	Result.ElementMultiplier = 1.0f;

	// Step 4: Critical hit
	if (Input.bCanCrit)
	{
		float CritChance = Input.OverrideCritChance >= 0.0f ? Input.OverrideCritChance : GetCriticalChance(Attacker);
		// ActionMods per-action crit-chance boost. Cluster 5e-C2: crit chance is Luck-driven, so this
		// routes through the Luck axis (was ESubStat::CritChance — which becomes crit DAMAGE in 5e-C3;
		// routing here prevents a crit-CHANCE modifier from silently flipping onto crit-DAMAGE). No
		// re-clamp — preserves the prior uncapped Reality-boost behaviour at this site.
		CritChance = Input.ActionMods.ApplyTo(CritChance, ESubStat::Luck);

		// (5e-C2) The standalone Luck-driven crit BONUS block was REMOVED here. Luck IS the crit chance
		// now — GetCriticalChance -> GetEvolutionModifiedCritChance -> GetLuckModifiedChance — not a
		// ×(1+LuckCritBonus) layered on top. Re-applying it would double-count Luck.

		// GuaranteedCrit (passive skill-effect): forces a crit when active on attacker.
		bool bForceCrit = false;
		if (USkillEffectManager *CritMgr = GetSkillEffectManager())
		{
			bForceCrit = CritMgr->HasEffectOfType(Attacker, ESkillEffectType::GuaranteedCrit);
		}
		Result.bWasCritical = bForceCrit || (FMath::FRand() < CritChance);

		if (Result.bWasCritical)
		{
			// GetCritDamageMultiplier now returns the FULL multiplier (x1.5 base + stat ramp, then
			// gear/transient) — 5e-B folded the base in, so it is NOT pre-multiplied here any more.
			const float CritMult = GetCritDamageMultiplier(Attacker);
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
		// Defense is now a capped % reduction (cluster 4): dmg ×= (1 − reduction), reduction in
		// [0, 0.5]. DamageBlockedByDefense records the HP actually removed by the reduction.
		Result.DefenderFlatDefense = GetDefenderFlatDefense(Defender);
		const int32 PreDefenseDamage = FMath::RoundToInt(RunningDamage);
		RunningDamage *= (1.0f - Result.DefenderFlatDefense);
		Result.DamageBlockedByDefense = PreDefenseDamage - FMath::RoundToInt(RunningDamage);
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
	USkillDataBase *Attack,
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
	Input.ActionType = EActionType::Ability; // attack/ability merge: Attack folded into Ability (both scale RawDamage)

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
	// OUTSIDE that clamp, scaling the capped stat upward toward the RESISTANCE_MAX (1.0) hard ceiling.
	// Multiplicative by design intent (Crown): a +X% stone/buff rewards a high-defense build more
	// than a low one (0.5 × 1.3 = 0.65 vs 0.2 × 1.3 = 0.26). GetEvolutionModifiedBody feeds the
	// slotted crystal's Body pillar into the curve.
	float Reduction = FMath::Min(DefenderComp->GetEvolutionModifiedFlatDefense(), CombatConstants::UNIVERSAL_STAT_CAP);

	if (ULoadoutComponent *Loadout = Defender->FindComponentByClass<ULoadoutComponent>())
	{
		// Equipment BonusDefense gear (A3) — reinterpreted as a % MULTIPLIER on the capped stat
		// reduction (option-ii: the per-point magnitude read as a FRACTION, same shape as every other
		// 5e gear field). Composes multiplicatively with the stone + buff below, OUTSIDE the 0.5 stat
		// cap, toward the RESISTANCE_MAX (1.0) ceiling (final clamp). ×1 (inert) when BonusDefense is 0.
		Reduction *= (1.0f + Loadout->GetActiveStatBonus(Defender).BonusDefense * CombatConstants::DEFENSE_PER_POINT);

		// Attached DefenseStone — a % MULTIPLIER (×(1 + StonePct/100)) on the capped stat reduction,
		// OUTSIDE the 0.5 cap, from the defender's OWN active weapon attachment (live-resolved, not
		// cached). Inert (×1) unless a DefenseStone is attached.
		if (const FRuntimeAttachedItem *AttPtr = Loadout->GetActiveWeaponAttachment())
		{
			const FRuntimeAttachedItem &Attachment = *AttPtr;
			const float StonePct =
				CrystalEffectTable::GetAttachedStonePercent(Attachment, ESubStat::Defense);
			Reduction *= (1.0f + StonePct / CombatConstants::STAT_PERCENT_DIVISOR);
		}
	}

	// Combat-buff/debuff modifiers (from skill casts, e.g. Stoneskin) — MULTIPLICATIVE transient: the
	// DefenseBuff/Debuff net scales the (permanent-gear) reduction proportionally, ×(1 + net/100), past
	// the 0.5 stat cap toward the 1.0 ceiling. Max(0,…) floors a ≥100% debuff at ×0 (never inverts);
	// buff/debuff in lockstep. The final clamp bounds the result to [0, 1.0].
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (StatusManager)
	{
		float DefenseBuff = StatusManager->GetTotalStatModifier(Defender, ESkillEffectType::DefenseBuff);
		float DefenseDebuff = StatusManager->GetTotalStatModifier(Defender, ESkillEffectType::DefenseDebuff);
		Reduction *= FMath::Max(0.0f, 1.0f + (DefenseBuff - DefenseDebuff) / CombatConstants::STAT_PERCENT_DIVISOR);
	}

	return FMath::Clamp(Reduction, 0.0f, CombatConstants::RESISTANCE_MAX);
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

		// Attached CritStone (5f-A) — a % MULTIPLIER on the capped stat, from the attacker's OWN active
		// weapon attachment. StoneTargetStat(CritStone) == ESubStat::CritDamage, so GetAttachedStonePercent
		// returns 0 (factor x1, inert) unless a CritStone is attached. Mirrors GetStoneRawDamageFactor.
		if (const FRuntimeAttachedItem *Att = Loadout->GetActiveWeaponAttachment())
		{
			Result *= (1.0f + CrystalEffectTable::GetAttachedStonePercent(*Att, ESubStat::CritDamage) / CombatConstants::STAT_PERCENT_DIVISOR);
		}
	}

	// Transient crit-damage modifiers — MULTIPLY too. Directional (5f-C): ModifyCritDamage is the BUFF
	// ("Crit Damage Up", up-only — Max(0,..) guards malformed negative assets); CritDamageDebuff is its
	// paired debuff (positive magnitude, SUBTRACTED). Net (buff − debuff) can go negative, dragging crit
	// damage down — the final clamp floors the whole multiplier at CRIT_DMG_BASE (x1.0), so a crit-damage
	// debuff can cancel the crit BONUS toward a normal hit but never make a crit weaker than one.
	if (USkillEffectManager *StatusManager = GetSkillEffectManager())
	{
		const float Modify = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::ModifyCritDamage);
		const float Debuff = StatusManager->GetTotalStatModifier(Attacker, ESkillEffectType::CritDamageDebuff);
		Result *= (1.0f + (FMath::Max(0.0f, Modify) - Debuff) / CombatConstants::STAT_PERCENT_DIVISOR);
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