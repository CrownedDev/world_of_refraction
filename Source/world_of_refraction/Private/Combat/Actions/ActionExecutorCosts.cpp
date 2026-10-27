// ActionExecutorCosts.cpp
// B-033 (move-only): UActionExecutor's cost resolution (the action energy cost, the charge
// infusion bands and multipliers, commit-time cost application), moved out of
// ActionExecutor.cpp unchanged.

#include "Combat/Actions/ActionExecutor.h"
#include "ActionExecutorInternal.h"
#include "MotionWarpingComponent.h"
#include "WoRLog.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "AI/AIDecisionManager.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/EffectIdentity.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/ESpellSource.h"
#include "Combat/CombatConstants.h"
#include "Infusion/InfusionConstants.h"
#include "Inventory/ItemExecutor.h"
#include "Equipment/Weapons/WeaponManager.h"
#include "Equipment/Crystals/CrystalManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Equipment/Rings/RingManager.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Rings/RingManager.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/GuardRules.h"
#include "Combat/Defense/LuckDodge.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Combat/Mechanics/BattleRandomSubsystem.h"
#include "Infusion/HybridSpellColors.h"
#include "Combat/Projectile/SkillProjectile.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Infusion/EInfusionSourceOption.h"
#include "Equipment/Rings/RingData.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Inventory/InventoryComponent.h" // ResolveSpellTier/ResolveAbilityTier (spell-instance ii-c)
#include "Equipment/FEquipmentStatBonus.h"
#include "Combat/Damage/TierGapConstants.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Debug/Combat/TierGapDamageDebug.h"

#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Combat/CombatAnimInstance.h"
#include "Combat/CombatNotify.h"
#include "Animation/AnimMontage.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Combat/TurnManager.h"
#include "Combat/Mechanics/RealityBoost.h"
#include "Skills/Definitions/SupportSkillRules.h"
#include "GameFramework/Character.h"
#include "Combat/CombatControl.h"
#include "Combat/Tuning/CombatTuningData.h"

int32 UActionExecutor::CalculateActionEnergyCost(AActor *Actor, const FAction &Action) const
{
	// D8: a deferred fire already paid its full cost at ARM — the fire-time
	// resubmission is cost-free. Dormant until 8c sets the flag.
	if (Action.bIsDeferredFire)
	{
		return 0;
	}

	UCharacterData *CharData = GetCharacterData(Actor);

	switch (Action.ActionType)
	{
	case EActionType::Spell:
		if (Action.SpellData)
		{
			// Ring and weapon crystal spells (including ring/weapon-attached
			// evolutions) are free to cast. Only innate refraction spells and
			// primary-slot evolution spells draw energy.
			if (Action.SpellSource == ESpellSource::RingCrystal ||
				Action.SpellSource == ESpellSource::WeaponCrystal)
			{
				return 0;
			}

			// Broken Darkness: primary-slot evolution casts are free at the EP
			// layer (durability wear in ExecuteSpellAsync is the cost), EXCEPT
			// when the player explicitly infuses with their innate (Darkness)
			// element at L1/L2 — that conversion pays EP on top of wear.
			// Non-BD evolution casts always draw EP.
			if (Action.SpellSource == ESpellSource::Evolution)
			{
				UCharacterDataComponent *CharComp = GetCharacterDataComponent(Actor);
				if (CharComp && CharComp->IsBrokenDarkness())
				{
					const bool bInnateDarknessInfusion =
						Action.SpellInfusionLevel >= 1 &&
						Action.SelectedSource == EInfusionSourceOption::Innate;
					if (!bInnateDarknessInfusion)
					{
						return 0;
					}
				}
			}

			int32 BaseCost = Action.SpellData->CalculateEnergyCost(CharData);
			// Spell infusion: charge multiplier (x1.5/x2.0 at L1/L2) x upside-only
			// SpellDamage surcharge. This function is BOTH the spell preview AND the
			// spell spend (ExecuteSpellAsync calls it), so preview == spend.
			const UCharacterDataComponent *Comp = GetCharacterDataComponent(Actor);
			const float CostMultiplier = ComputeInfusionCostMultiplier(Action.SpellInfusionLevel, /*bIsSpell*/ true, Comp);
			// Efficiency reduction — character substat + equipment BonusEfficiency.
			const float EfficiencyMult = GetEffectiveEnergyCostEfficiencyMultiplier(Actor);
			// Own-tier power: SAME-DIRECTION cost (higher tier = higher cost), not reciprocal.
			const float PowerMult = TierPowerScaling::GetTierPowerMultiplier(UInventoryComponent::ResolveSpellTier(Actor, Action.SpellData));
			// Tier-gap cost (Cluster 3a): reciprocal gap — cheaper through a stronger
			// channel, pricier through a weaker one. Own ladder, not the damage accessor.
			return FMath::RoundToInt(BaseCost * CostMultiplier * EfficiencyMult * PowerMult * GetTierGapCostMultiplier(Actor, Action));
		}
		break;

	case EActionType::Ability:
		// Cluster 3: covers attacks too (folded into Ability). Reads the merged pointer so a
		// cost-bearing attack reports the same cost the merged dispatch spends; default attacks
		// (BaseEnergyCost=0) report 0. Preview MUST match the spend (ExecuteAbilityAsync): apply
		// the charge multiplier + RawDamage surcharge, and zero EP for crystal sources.
		if (USkillDataBase *Skill = ResolveActionSkill(Action))
		{
			// Crystal-sourced abilities are powered by the crystal's charge, not EP.
			const bool bCrystalSource =
				Action.SelectedSource == EInfusionSourceOption::ActiveRing ||
				Action.SelectedSource == EInfusionSourceOption::PrimaryRing ||
				Action.SelectedSource == EInfusionSourceOption::WeaponCrystal;
			if (bCrystalSource)
			{
				return 0;
			}

			const bool bIsInfused = (Action.SelectedSource != EInfusionSourceOption::None);
			int32 BaseCost = Skill->CalculateEnergyCost(CharData, bIsInfused);
			const UCharacterDataComponent *Comp = GetCharacterDataComponent(Actor);
			const float CostMultiplier = ComputeInfusionCostMultiplier(Action.AbilityInfusionLevel, /*bIsSpell*/ false, Comp);
			const float EfficiencyMult = GetEffectiveEnergyCostEfficiencyMultiplier(Actor);
			// Own-tier power: SAME-DIRECTION cost (higher tier = higher cost), not reciprocal.
			const float PowerMult = TierPowerScaling::GetTierPowerMultiplier(UInventoryComponent::ResolveAbilityTier(Actor, Skill));
			// Tier-gap cost (Cluster 3a): reciprocal gap — cheaper through a stronger
			// channel, pricier through a weaker one. Own ladder, not the damage accessor.
			return FMath::RoundToInt(BaseCost * CostMultiplier * EfficiencyMult * PowerMult * GetTierGapCostMultiplier(Actor, Action));
		}
		break;

	case EActionType::Item:
		// Items typically don't cost energy
		return 0;

	case EActionType::Defend:
		return 0;

	default:
		break;
	}

	return 0;
}

// ========================================
// CHARGE INFUSION HELPERS
// ========================================

// StatFraction (upside-only stat surcharge, max(0, stat - 1)) moved to the shared
// Skills/Definitions/EScalingTier.h so the infusion cost/effect path here and the damage-scaling
// tier term in DamageCalculator share ONE definition (no drift). Reached transitively via
// SkillDataBase.h; the call sites below (cost multiplier + charge getters) are byte-identical.

// Charge effect band: pick the L1 vs L2 value for a (L1,L2) constant pair. Callers guard
// Level <= 0 → 1.0 before reaching here, so any Level >= 2 takes the L2 band.
static float ChargeBand(int32 Level, float L1Value, float L2Value)
{
	return (Level == 1) ? L1Value : L2Value;
}

float UActionExecutor::GetChargeDamageMultiplier(int32 Level, EInfusionMode Mode, bool bIsSpell, const UCharacterDataComponent *Comp) const
{
	if (Level <= 0 || !Comp)
	{
		return 1.0f; // uninfused (or no comp): no charge bonus
	}

	// Damage is the FOCUS in Physical mode, OFF-FOCUS in Status, BALANCED in Balanced.
	const UCombatTuningData &Tuning = WoRTuning::Get();
	float Base;
	switch (Mode)
	{
	case EInfusionMode::Physical:
		Base = ChargeBand(Level, Tuning.ChargeFocusL1, Tuning.ChargeFocusL2);
		break;
	case EInfusionMode::Status:
		Base = ChargeBand(Level, Tuning.ChargeOffFocusL1, Tuning.ChargeOffFocusL2);
		break;
	case EInfusionMode::Balanced:
	default:
		Base = ChargeBand(Level, Tuning.ChargeBalancedL1, Tuning.ChargeBalancedL2);
		break;
	}

	const float Stat = bIsSpell ? Comp->GetEffectiveStats().SpellDamage
								 : Comp->GetEffectiveRawDamage();
	return Base * (1.0f + StatFraction(Stat));
}

float UActionExecutor::GetChargeStatusMultiplier(int32 Level, EInfusionMode Mode, const UCharacterDataComponent *Comp) const
{
	if (Level <= 0 || !Comp)
	{
		return 1.0f;
	}

	// Status is the FOCUS in Status mode, OFF-FOCUS in Physical, BALANCED in Balanced.
	const UCombatTuningData &Tuning = WoRTuning::Get();
	float Base;
	switch (Mode)
	{
	case EInfusionMode::Physical:
		Base = ChargeBand(Level, Tuning.ChargeOffFocusL1, Tuning.ChargeOffFocusL2);
		break;
	case EInfusionMode::Status:
		Base = ChargeBand(Level, Tuning.ChargeFocusL1, Tuning.ChargeFocusL2);
		break;
	case EInfusionMode::Balanced:
	default:
		Base = ChargeBand(Level, Tuning.ChargeBalancedL1, Tuning.ChargeBalancedL2);
		break;
	}

	return Base * (1.0f + StatFraction(Comp->GetEffectiveStatusMultiplier()));
}

EInfusionMode UActionExecutor::ResolveInfusionMode(EInfusionSourceOption Source, AActor *Actor) const
{
	switch (Source)
	{
	case EInfusionSourceOption::ActiveRing:
		if (ULoadoutComponent *LC = GetLoadoutComponent(Actor))
		{
			if (URingData *Ring = LC->GetActiveRing())
			{
				return Ring->InfusionMode;
			}
		}
		break;
	case EInfusionSourceOption::PrimaryRing:
		if (ULoadoutComponent *LC = GetLoadoutComponent(Actor))
		{
			if (URingData *Ring = LC->GetPrimaryRing())
			{
				return Ring->InfusionMode;
			}
		}
		break;
	case EInfusionSourceOption::Innate:
		if (UCharacterData *CharData = GetCharacterData(Actor))
		{
			return CharData->InfusionMode;
		}
		break;
	case EInfusionSourceOption::Evolution:
		if (ULoadoutComponent *LC = GetLoadoutComponent(Actor))
		{
			if (UEvolutionItemData *Evo = LC->GetPrimaryEvolution())
			{
				return Evo->InfusionMode;
			}
		}
		break;
	case EInfusionSourceOption::Raw:
	case EInfusionSourceOption::WeaponCrystal:
	default:
		if (ULoadoutComponent *LC = GetLoadoutComponent(Actor))
		{
			if (UWeaponData *Weapon = LC->GetActiveWeapon())
			{
				return Weapon->InfusionMode;
			}
		}
		break;
	}
	return EInfusionMode::Balanced; // null-safe fallback
}

float UActionExecutor::GetAbilityChargeCostMultiplier(int32 Level) const
{
	// Energy cost scales with charge level for abilities. Uses the generic
	// (non-spell) energy multipliers — spells use SPELL_L1/L2_ENERGY_MULT.
	switch (Level)
	{
	case 1:
		return InfusionConstants::L1_ENERGY_MULT; // x1.5 (an infused ability also pays INFUSION_ENERGY_MULTIPLIER, D-035)
	case 2:
		return InfusionConstants::L2_ENERGY_MULT; // x2.0
	default:
		return 1.0f;
	}
}

float UActionExecutor::ComputeInfusionCostMultiplier(int32 Level, bool bIsSpell, const UCharacterDataComponent *Comp) const
{
	if (Level <= 0)
	{
		// Uninfused: no charge multiplier, no surcharge — caller pays BaseCost * Efficiency.
		return 1.0f;
	}

	const float ChargeMult = bIsSpell
								  ? GetSpellInfusionCostMultiplier(Level)
								  : GetAbilityChargeCostMultiplier(Level);

	// Stat surcharge — UPSIDE-ONLY (a below-neutral stat never reduces the cost).
	// Ability scales with RawDamage, spell with SpellDamage. Stacking the stat makes
	// infusion cost MORE in BOTH directions (damage AND cost) — self-balancing.
	float Frac = 0.0f;
	if (Comp)
	{
		const float Stat = bIsSpell ? Comp->GetEffectiveStats().SpellDamage
									 : Comp->GetEffectiveRawDamage();
		Frac = StatFraction(Stat);
	}

	return ChargeMult * (1.0f + Frac);
}

// ============================================================
// COMMIT-TIME COST APPLICATION (Phase 4c)
// ============================================================

void UActionExecutor::ApplyCommitCosts(AActor *Actor, const FAction &Action)
{
	if (!Actor)
	{
		return;
	}

	// D8: a deferred fire already paid commit costs at ARM — skip on the
	// fire-time resubmission. Dormant until 8c sets the flag.
	if (Action.bIsDeferredFire)
	{
		return;
	}

	// Determine the infusion level for this action type.
	// Spells use SpellInfusionLevel; abilities/attacks use AbilityInfusionLevel.
	int32 Level = 0;
	if (Action.ActionType == EActionType::Spell)
	{
		Level = Action.SpellInfusionLevel;
	}
	else if (Action.ActionType == EActionType::Ability)
	{
		Level = Action.AbilityInfusionLevel;
	}

	// Level 0 = no infusion, no commit cost regardless of source
	if (Level <= 0)
	{
		return;
	}

	// HP basis (rework 6-2-3c): the PRE-Efficiency, stat-scaled infused EP cost.
	// Source-independent — it's what this infused action's EP costs before the
	// Efficiency reduction. The HP-paying branches (Raw/Innate/Evolution) feed this
	// to CalculateHPCost, which converts it to HP (% of MaxEP -> % of MaxHP) and
	// applies Resistance. Crystal sources pay durability, not HP, so they ignore it.
	const bool bIsSpellAction = (Action.ActionType == EActionType::Spell);
	UCharacterData *CommitCharData = GetCharacterData(Actor);
	const UCharacterDataComponent *CommitComp = GetCharacterDataComponent(Actor);
	int32 PreEffInfusedEP = 0;
	{
		int32 BaseCost = 0;
		float PowerMult = 1.0f;
		if (bIsSpellAction)
		{
			if (Action.SpellData)
			{
				BaseCost = Action.SpellData->CalculateEnergyCost(CommitCharData);
				PowerMult = TierPowerScaling::GetTierPowerMultiplier(UInventoryComponent::ResolveSpellTier(Actor, Action.SpellData));
			}
		}
		else if (USkillDataBase *Skill = ResolveActionSkill(Action))
		{
			const bool bIsInfused = (Action.SelectedSource != EInfusionSourceOption::None);
			BaseCost = Skill->CalculateEnergyCost(CommitCharData, bIsInfused);
			PowerMult = TierPowerScaling::GetTierPowerMultiplier(UInventoryComponent::ResolveAbilityTier(Actor, Skill));
		}
		// Own-tier power mirrors the cost multiplier so the HP-infusion penalty basis tracks the
		// now-tier-scaled EP cost (this block recomputes cost; it does NOT read CalculateActionEnergyCost).
		// Tier-gap cost (Cluster 3a): the HP-infusion basis tracks the now-gap-scaled
		// EP cost, so the reciprocal gap flows through to the HP penalty too.
		PreEffInfusedEP = FMath::RoundToInt(BaseCost * ComputeInfusionCostMultiplier(Level, bIsSpellAction, CommitComp) * PowerMult * GetTierGapCostMultiplier(Actor, Action));
	}

	// Route by source
	switch (Action.SelectedSource)
	{
	case EInfusionSourceOption::None:
		// No source means no infusion cost path. Should not normally reach here
		// with Level > 0 — log to surface inconsistencies.
		UE_LOG(LogTemp, Verbose,
			   TEXT("[ActionExecutor] %s: SelectedSource=None but InfusionLevel=%d — no cost applied"),
			   *Actor->GetName(), Level);
		break;

	case EInfusionSourceOption::Raw:
	case EInfusionSourceOption::Innate:
		// Raw ALWAYS pays HP. Innate pays HP only on a SPELL — an innate-element
		// ABILITY is "oiling the sword" (EP only, no self-strain); only its HP is
		// dropped, the EP surcharge (6-2-2) still applies. Cost is DEFERRED to
		// FinalizeAsyncAction via PendingInfusionHPCost so a lethal cost lands after
		// the infused effect resolves.
		if (CurrentExecutionContext.IsSet())
		{
			const bool bInnateAbilityNoHP =
				(Action.SelectedSource == EInfusionSourceOption::Innate) && !bIsSpellAction;
			if (!bInnateAbilityNoHP)
			{
				CurrentExecutionContext->PendingInfusionHPCost = UInfusionCostHelper::CalculateHPCost(Actor, PreEffInfusedEP);
			}
		}
		break;

	case EInfusionSourceOption::ActiveRing:
	{
		// Ring sources pay durability wear on the active ring's slotted crystal.
		// Action tier resolution per Phase 4d Path A:
		//   - Spell: SpellData->Tier (the spell's own tier)
		//   - Ability/Attack: Weapon->Tier (action tier inherits from weapon)
		ULoadoutComponent *RingLoadout = GetLoadoutComponent(Actor);
		if (!RingLoadout)
		{
			break;
		}

		URingData *Ring = RingLoadout->GetActiveRing();
		if (!Ring)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: ActiveRing infusion but no active ring resolved"),
				   *Actor->GetName());
			break;
		}

		const bool bIsSpell = (Action.ActionType == EActionType::Spell);
		EItemTier ActionTier = EItemTier::F_Tier; // sensible default

		if (bIsSpell && Action.SpellData)
		{
			ActionTier = UInventoryComponent::ResolveSpellTier(Actor, Action.SpellData);
		}
		else
		{
			// Ability or attack: action tier inherits from the active weapon's
			// instance entry (leveling-correct); fall back to base asset tier.
			if (const FWeaponInventoryEntry *WeaponEntry = ResolveActiveWeaponEntry(Actor))
			{
				ActionTier = WeaponEntry->Tier;
			}
			else if (UWeaponData *Weapon = RingLoadout->GetActiveWeapon())
			{
				ActionTier = Weapon->Tier;
			}
		}

		ULoadoutComponent *LC = GetLoadoutComponent(Actor);
		FRuntimeAttachedItem *Attachment = LC ? LC->FindAttachedItemByHolder(Ring) : nullptr;
		if (Attachment && !Attachment->IsEmpty())
		{
			if (UCrystalManager *CrystalMgr = GetGameInstance()
												  ? GetGameInstance()->GetSubsystem<UCrystalManager>()
												  : nullptr)
			{
				CrystalMgr->ProcessPostCastWear(
					Actor, Ring, *Attachment, ActionTier, Level, bIsSpell);
			}
		}

		// Per-action stat modifiers are computed by ComputeActionStatModifiers
		// at action start (ExecuteActionAsync) and stashed on
		// FActionExecutionContext::ActionMods. No commit-time call needed here.
		break;
	}
	case EInfusionSourceOption::PrimaryRing:
	{
		// Primary Ring source: Generic/Caster with a ring in their primary slot.
		// Mirrors ActiveRing structure — same wear path, same Iolite check,
		// just resolves the primary ring instead of active.
		ULoadoutComponent *RingLoadout = GetLoadoutComponent(Actor);
		if (!RingLoadout)
		{
			break;
		}

		URingData *Ring = RingLoadout->GetPrimaryRing();
		if (!Ring)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: PrimaryRing infusion but no primary ring resolved"),
				   *Actor->GetName());
			break;
		}

		const bool bIsSpell = (Action.ActionType == EActionType::Spell);
		EItemTier ActionTier = EItemTier::F_Tier;

		if (bIsSpell && Action.SpellData)
		{
			ActionTier = UInventoryComponent::ResolveSpellTier(Actor, Action.SpellData);
		}
		else
		{
			// Ability or attack: action tier inherits from the active weapon's
			// instance entry (leveling-correct); fall back to base asset tier.
			if (const FWeaponInventoryEntry *WeaponEntry = ResolveActiveWeaponEntry(Actor))
			{
				ActionTier = WeaponEntry->Tier;
			}
			else if (UWeaponData *Weapon = RingLoadout->GetActiveWeapon())
			{
				ActionTier = Weapon->Tier;
			}
		}

		ULoadoutComponent *LC = GetLoadoutComponent(Actor);
		FRuntimeAttachedItem *Attachment = LC ? LC->FindAttachedItemByHolder(Ring) : nullptr;
		if (Attachment && !Attachment->IsEmpty())
		{
			if (UCrystalManager *CrystalMgr = GetGameInstance()
												  ? GetGameInstance()->GetSubsystem<UCrystalManager>()
												  : nullptr)
			{
				CrystalMgr->ProcessPostCastWear(
					Actor, Ring, *Attachment, ActionTier, Level, bIsSpell);
			}
		}

		// Per-action stat modifiers live on the execution context (action-start), not here.
		break;
	}

	case EInfusionSourceOption::WeaponCrystal:
	{
		// Weapon crystal infusion: durability wear on the slotted crystal.
		// ActionTier dispatch:
		//   Spell: SpellData->Tier (the spell's own tier — symmetric with Ring path)
		//   Ability/Attack: Weapon->Tier (action tier inherits from weapon)
		// Spells wear based on their own tier regardless of catalyst (weapon vs
		// ring), so an S-Tier spell stresses any catalyst's crystal equally.
		ULoadoutComponent *WeaponLoadout = GetLoadoutComponent(Actor);
		if (!WeaponLoadout)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: WeaponCrystal infusion but no LoadoutComponent"),
				   *Actor->GetName());
			break;
		}

		UWeaponData *Weapon = WeaponLoadout->GetActiveWeapon();
		if (!Weapon)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: WeaponCrystal infusion but no active weapon"),
				   *Actor->GetName());
			break;
		}

		ULoadoutComponent *LC = GetLoadoutComponent(Actor);
		FRuntimeAttachedItem *Attachment = LC ? LC->FindAttachedItemByHolder(Weapon) : nullptr;
		if (!Attachment || Attachment->IsEmpty())
		{
			break;
		}

		UCrystalManager *CrystalMgr = GetGameInstance()
										  ? GetGameInstance()->GetSubsystem<UCrystalManager>()
										  : nullptr;
		if (!CrystalMgr)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: WeaponCrystal infusion but CrystalManager unavailable"),
				   *Actor->GetName());
			break;
		}

		const bool bIsSpell = (Action.ActionType == EActionType::Spell);
		// Instance tier from the weapon's inventory entry (leveling-correct); the
		// asset `Weapon` above is still used to resolve the slotted-crystal holder,
		// and remains the asset-tier fallback when no entry resolves.
		const FWeaponInventoryEntry *WeaponEntry = ResolveActiveWeaponEntry(Actor);
		EItemTier ActionTier = WeaponEntry ? WeaponEntry->Tier : Weapon->Tier;
		if (bIsSpell && Action.SpellData)
		{
			ActionTier = UInventoryComponent::ResolveSpellTier(Actor, Action.SpellData);
		}

		// A gear-attached evolution provides the weapon's spells (so WeaponCrystal infusion is
		// offered), but ProcessPostCastWear skips evolution — route it to the gear-evolution wear
		// path instead so the socketed evolution actually wears in combat (gear-wear).
		if (Attachment->IsEvolution())
		{
			CrystalMgr->ProcessPostCastGearEvolutionWear(
				Actor, *Attachment, ActionTier, Level, bIsSpell);
		}
		else
		{
			CrystalMgr->ProcessPostCastWear(
				Actor, Weapon, *Attachment, ActionTier, Level, bIsSpell);
		}

		// Per-action stat modifiers live on the execution context (action-start), not here.
		break;
	}

	case EInfusionSourceOption::Evolution:
	{
		// Phase 6 — Evolution backlash:
		//   HP cost: the infused EP's share of Max EP, as a share of Max HP, less Resistance
		//   (UInfusionCostHelper::CalculateHPCost, ED-32), deferred to finalize like Innate/Raw.
		//   Self-status build: 15 at L1, 25 at L2 (using evolution element).
		// Self-status build is logged as intent — actual application pending the
		// element-to-status mapping system (separate workstream).

		// 1. Resolve evolution element from active weapon's runtime attachment
		//    for logging; needed by the future status-build wiring.
		ULoadoutComponent *LC = GetLoadoutComponent(Actor);
		const FWeaponLoadoutEntry *ActiveWeaponLoadout = LC ? LC->GetActiveWeaponLoadout() : nullptr;
		UEvolutionItemData *WeaponCrystal = nullptr;
		if (ActiveWeaponLoadout && ActiveWeaponLoadout->WeaponEntry.AttachedItem.IsEvolution())
		{
			WeaponCrystal = ActiveWeaponLoadout->WeaponEntry.AttachedItem.Evolution.Item;
		}
		if (!WeaponCrystal)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: Evolution infusion but weapon is not evolved or no slotted crystal"),
				   *Actor->GetName());
			break;
		}

		const ESpellElement EvolutionElement = WeaponCrystal->GetAssociatedElement();

		// 2. HP cost — same EP-based formula as Innate/Raw. Computed now
		//    (commit-time HP) but DEFERRED to FinalizeAsyncAction via
		//    PendingInfusionHPCost so a lethal backlash lands after the action.
		if (CurrentExecutionContext.IsSet())
		{
			CurrentExecutionContext->PendingInfusionHPCost = UInfusionCostHelper::CalculateHPCost(Actor, PreEffInfusedEP);
		}

		// 3. Self-status build (logged intent, not yet applied — pending element-to-status mapping)
		const float SelfStatusAmount = (Level == 1)
										   ? InfusionConstants::EVOLUTION_L1_SELF_STATUS_BUILD
										   : InfusionConstants::EVOLUTION_L2_SELF_STATUS_BUILD;
		UE_LOG(LogTemp, Log,
			   TEXT("[ActionExecutor] %s Evolution L%d backlash: HP cost applied. Would apply %.0f %s self-status build (pending mapping system)"),
			   *Actor->GetName(), Level, SelfStatusAmount,
			   *UEnum::GetValueAsString(EvolutionElement));

		// 4. Durability wear on the primary-slot evolution. Non-spell actions
		//    only — spells pay via the ExecuteSpellAsync evolution hook, so an
		//    unconditional charge here would double-bill an evolution-source
		//    spell. Tier follows the non-spell sibling convention (active
		//    weapon's tier, as in the ActiveRing/WeaponCrystal cases). The
		//    BD/Reality breakable gate lives inside ProcessPostCastEvolutionWear.
		if (Action.ActionType != EActionType::Spell)
		{
			// Instance tier from the active weapon's inventory entry (leveling-correct);
			// fall back to base asset tier when no entry resolves.
			EItemTier ActionTier = EItemTier::F_Tier;
			if (const FWeaponInventoryEntry *WeaponEntry = ResolveActiveWeaponEntry(Actor))
			{
				ActionTier = WeaponEntry->Tier;
			}
			else if (ULoadoutComponent *TierLoadout = GetLoadoutComponent(Actor))
			{
				if (UWeaponData *Weapon = TierLoadout->GetActiveWeapon())
				{
					ActionTier = Weapon->Tier;
				}
			}

			if (UCrystalManager *CrystalMgr = GetGameInstance()
												  ? GetGameInstance()->GetSubsystem<UCrystalManager>()
												  : nullptr)
			{
				CrystalMgr->ProcessPostCastEvolutionWear(
					Actor, LC, ActionTier, Level, /*bIsSpell=*/false);
			}
		}

		break;
	}

	default:
		UE_LOG(LogTemp, Warning,
			   TEXT("[ActionExecutor] %s: Unknown infusion source %d at L%d"),
			   *Actor->GetName(), static_cast<int32>(Action.SelectedSource), Level);
		break;
	}
}

void UActionExecutor::ApplyPendingInfusionHPCost(AActor *Actor)
{
	if (!Actor || !CurrentExecutionContext.IsSet())
	{
		return;
	}

	// Cost was computed and stashed at commit (ApplyCommitCosts) from commit-time
	// HP — apply that exact value, NOT a recompute, so reflected damage taken
	// mid-action cannot change what the caster pays.
	const int32 Cost = CurrentExecutionContext->PendingInfusionHPCost;
	if (Cost <= 0)
	{
		return;
	}

	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[ActionExecutor] %s: deferred infusion HP cost %d but no CharacterDataComponent — cost lost"),
			   *Actor->GetName(), Cost);
		return;
	}

	const int32 Before = CharComp->CurrentHP;

	// No clamp — design changed: infusion CAN kill. Full cost routes through
	// ServerTakeDamage so OnHPChanged broadcasts AND a lethal cost trips
	// CheckDeath -> OnDied (same death path as any killing blow). Runs at
	// finalize, so the infused effect has already resolved.
	CharComp->ServerTakeDamage(Cost);

	UE_LOG(LogTemp, Log,
		   TEXT("[ActionExecutor] %s paid %d HP for infusion at finalize (HP: %d -> %d)%s"),
		   *Actor->GetName(), Cost, Before, CharComp->CurrentHP,
		   CharComp->bIsAlive ? TEXT("") : TEXT(" — LETHAL"));
}
