// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Actions/ActionExecutor.h"
#include "MotionWarpingComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/ESpellSource.h"
#include "Combat/Actions/ActionUtils.h"
#include "Combat/CombatConstants.h"
#include "Infusion/InfusionConstants.h"
#include "Inventory/ItemExecutor.h"
#include "Equipment/Weapons/WeaponManager.h"
#include "Equipment/Crystals/CrystalManager.h"
#include "Engine/World.h"
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
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Infusion/HybridSpellColors.h"
#include "Combat/Projectile/SkillProjectile.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Infusion/EInfusionSourceOption.h"
#include "Equipment/Rings/RingData.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Combat/Damage/TierGapConstants.h"
#include "Combat/Damage/TierGapDamageDebug.h"

#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Combat/CombatAnimInstance.h"
#include "Combat/CombatNotify.h"
#include "Animation/AnimMontage.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "Combat/TurnManager.h"
#include "Combat/Mechanics/RealityBoost.h"
#include "GameFramework/Character.h"

class UCharacterDataComponent;
class UCharacterData;
class USkillEffectManager;
class USpellData;
class UAbilityData;
class UEvolutionItemData;

// The unified skill pointer for an attack/ability action. The legacy AbilityData/AttackData pointers
// were removed (Cluster 4); SkillData is populated at every construction site. Kept as the single named
// read-point that the call sites go through. Spells use SpellData (a separate path).
static USkillDataBase *ResolveActionSkill(const FAction &Action)
{
	return Action.SkillData;
}

void UActionExecutor::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	BindDefenseSystemEvents();

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Initialized"));
}

void UActionExecutor::Deinitialize()
{
	SkillEffectManagerRef = nullptr;
	UnbindDefenseSystemEvents();
	CancelAsyncAction(); // Clean up any pending action

	Super::Deinitialize();
}

// ========================================
// INFUSION MULTIPLIERS (Static)
// ========================================

float UActionExecutor::GetSpellInfusionSizeMultiplier(int32 InfusionLevel)
{
	switch (InfusionLevel)
	{
	case 1:
		return InfusionConstants::SPELL_L1_SIZE_MULT; // Level 1: 50% size increase
	case 2:
		return InfusionConstants::SPELL_L2_SIZE_MULT; // Level 2: 100% size increase
	default:
		return 1.0f; // No infusion
	}
}

float UActionExecutor::GetSpellInfusionCostMultiplier(int32 InfusionLevel)
{
	switch (InfusionLevel)
	{
	case 1:
		return InfusionConstants::SPELL_L1_ENERGY_MULT; // Level 1: 30% cost increase
	case 2:
		return InfusionConstants::SPELL_L2_ENERGY_MULT; // Level 2: 60% cost increase
	default:
		return 1.0f; // No infusion
	}
}

// ========================================
// VALIDATION
// ========================================

FActionValidationResult UActionExecutor::ValidateAction(AActor *Actor, const FAction &Action) const
{
	if (!Actor)
	{
		return FActionValidationResult(false, TEXT("Invalid actor"));
	}

	if (!Action.IsValid())
	{
		return FActionValidationResult(false, TEXT("Invalid action data"));
	}

	// D8 8c rev: a deferred fire is a committed last act — stun/silence gained
	// after arming cannot stop it. Target validation below still applies: a
	// ritual with no valid targets fizzles.
	if (!Action.bIsDeferredFire)
	{
		// Check if actor can act (not stunned)
		if (!CanActorAct(Actor))
		{
			return FActionValidationResult(false, TEXT("Cannot act (Stunned)"));
		}

		// Check if actor can cast spells (not silenced)
		if (Action.ActionType == EActionType::Spell && !CanActorCastSpells(Actor))
		{
			return FActionValidationResult(false, TEXT("Cannot cast spells (Silenced)"));
		}
	}

	// Check targets
	if (Action.RequiresTarget() && Action.Targets.Num() == 0)
	{
		return FActionValidationResult(false, TEXT("No targets selected"));
	}

	// Validate targets are alive. Deferred fires bypass the strict
	// any-target-dead rejection: FilterValidTargets in the execution path
	// drops the dead and fires on survivors; zero survivors → fizzle there
	// (single-target ritual whose target died → fizzles, as designed).
	if (!Action.bIsDeferredFire)
	{
		for (AActor *Target : Action.Targets)
		{
			if (!IsTargetAlive(Target))
			{
				return FActionValidationResult(false, TEXT("Target is dead"));
			}
		}
	}

	// Infusion consistency: Level > 0 requires a real source.
	// None means "no infusion at all" — incompatible with Level > 0.
	// Use Raw for HP-cost infusion without an element.
	const int32 ChargeLevel = Action.GetChargeLevel();
	if (ChargeLevel > 0 && Action.SelectedSource == EInfusionSourceOption::None)
	{
		return FActionValidationResult(
			false,
			TEXT("Infusion level set but no source selected (use Raw for elementless infusion)"));
	}

	// Calculate energy cost
	int32 EnergyCost = CalculateActionEnergyCost(Actor, Action);

	// Energy check — CurrentEP is the unified spend pool for Broken Darkness
	// (event-driven absorption) and non-BD (regenerating EP) characters alike.
	UCharacterDataComponent *CharComp = GetCharacterDataComponent(Actor);
	if (CharComp && CharComp->CurrentEP < EnergyCost)
	{
		return FActionValidationResult(false, TEXT("Not enough energy"), EnergyCost);
	}

	// Check requirements (world stat requirements)
	UCharacterData *CharData = GetCharacterData(Actor);
	if (CharData)
	{
		switch (Action.ActionType)
		{
		case EActionType::Spell:
			if (Action.SpellData && !Action.SpellData->MeetsRequirements(CharData))
			{
				// Allow with penalty, but could warn
				// return FActionValidationResult(false, TEXT("Requirements not met"));
			}
			// Element gate (Caster-only): Reality and BrokenDarkness sources unlock
			// any-spell access per locked design. Generic and Resonator have no
			// element gate.
			if (Action.SpellData && CharData->CharacterClass == ECharacterClass::Caster)
			{
				// Source-side: Reality / BrokenDarkness infusion sources unlock any element.
				const ESpellElement SourceElement =
					GetElementForSourceOption(Actor, Action.SelectedSource);
				const bool bAnyElement = ElementHelpers::IsAnySpellSource(SourceElement);

				// Character-side capability — innate match for normal Casters,
				// Darkness + absorbed elements for Broken Darkness, plus any
				// element channelled by an equipped crystal. Shared with
				// LoadoutComponent::GetValidationErrors via the same helper.
				const bool bElementCastable = UBrokenDarknessManager::IsElementCastable(
					Actor, CharComp, GetBrokenDarknessManager(Actor), Action.SpellData->Element);

				if (!bAnyElement && !bElementCastable)
				{
					return FActionValidationResult(false, TEXT("Element restricted"));
				}
			}

			// 6-5-f: enforce the spell-source binding. An infused spell's SelectedSource must be one
			// its origin allows (GetAllowedInfusionSourcesForSpell — the single source of truth shared
			// with the AI and the UI source cache). Spells are origin-bound; abilities are NOT, so this
			// lives in the Spell case only. Uninfused spells (L0) skip it; the None-source-at-L>0 case is
			// the separate consistency check above. A non-infusable spell (empty allowed set) infused at
			// L>0 is correctly rejected here.
			if (Action.SpellData &&
				Action.GetChargeLevel() > 0 &&
				Action.SelectedSource != EInfusionSourceOption::None)
			{
				const TArray<EInfusionSourceOption> AllowedSources =
					GetAllowedInfusionSourcesForSpell(Actor, Action.SpellData);
				if (!AllowedSources.Contains(Action.SelectedSource))
				{
					return FActionValidationResult(false,
						TEXT("Infusion source not allowed for this spell's origin"));
				}
			}
			break;

		case EActionType::Ability:
			if (Action.SkillData && !Action.SkillData->MeetsRequirements(CharData))
			{
				// Allow with penalty
			}
			break;

		default:
			break;
		}
	}

	return FActionValidationResult(true, TEXT(""), EnergyCost);
}

bool UActionExecutor::CanActorAct(AActor *Actor) const
{
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (StatusManager && StatusManager->IsStunned(Actor))
	{
		return false;
	}
	return true;
}

bool UActionExecutor::CanActorCastSpells(AActor *Actor) const
{
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (StatusManager && StatusManager->IsSilenced(Actor))
	{
		return false;
	}
	return true;
}

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
			return FMath::RoundToInt(BaseCost * CostMultiplier * EfficiencyMult);
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
			return FMath::RoundToInt(BaseCost * CostMultiplier * EfficiencyMult);
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
// TIER-GAP RESOLUTION (D9)
// ========================================

EItemTier UActionExecutor::ResolveActionTier(AActor *Actor, const FAction &Action) const
{
	// Spell: the spell's own tier. Ability/attack: tier inherits from the active
	// weapon. Same dispatch as the ApplyCommitCosts wear blocks (which keep
	// their inline copies until the swap is approved separately).
	if (Action.ActionType == EActionType::Spell && Action.SpellData)
	{
		return Action.SpellData->Tier;
	}

	if (UWeaponManager *WeaponMgr = GetWeaponManager())
	{
		if (UWeaponData *Weapon = WeaponMgr->GetActiveWeapon(Actor))
		{
			return Weapon->Tier;
		}
	}
	return EItemTier::F_Tier;
}

TOptional<EItemTier> UActionExecutor::ResolveChannelTier(AActor *Actor, const FAction &Action) const
{
	// Attack/Ability channel through the active weapon.
	if (Action.ActionType != EActionType::Spell)
	{
		if (UWeaponManager *WeaponMgr = GetWeaponManager())
		{
			if (UWeaponData *Weapon = WeaponMgr->GetActiveWeapon(Actor))
			{
				return Weapon->Tier;
			}
		}
		return TOptional<EItemTier>(); // unarmed — no channel
	}

	switch (Action.SpellSource)
	{
	case ESpellSource::Innate:
	// TODO: Item channel tier when spell items get tier data (consumption itself
	// is still unimplemented — see ProcessPostCastBySource).
	case ESpellSource::Item:
		return TOptional<EItemTier>();

	case ESpellSource::Evolution:
	{
		// Primary-slot evolution: same attachment read as the ApplyCommitCosts
		// evolution block.
		ULoadoutComponent *LC = GetLoadoutComponent(Actor);
		const FWeaponLoadoutEntry *ActiveWeaponLoadout = LC ? LC->GetActiveWeaponLoadout() : nullptr;
		if (ActiveWeaponLoadout &&
			ActiveWeaponLoadout->WeaponEntry.AttachedItem.IsEvolution() &&
			ActiveWeaponLoadout->WeaponEntry.AttachedItem.Evolution.Item)
		{
			return ActiveWeaponLoadout->WeaponEntry.AttachedItem.Evolution.Item->Tier;
		}
		return TOptional<EItemTier>();
	}

	case ESpellSource::RingCrystal:
	case ESpellSource::WeaponCrystal:
	{
		ULoadoutComponent *LC = GetLoadoutComponent(Actor);
		UObject *Holder = LC ? LC->FindSpellCatalystHolder(Action.SpellData) : nullptr;
		FRuntimeAttachedItem *Attachment = Holder ? LC->FindAttachedItemByHolder(Holder) : nullptr;
		if (!Attachment || Attachment->IsEmpty())
		{
			return TOptional<EItemTier>();
		}

		// Fusion channels through the GEM half — same keying as the wear path.
		// Broken is intentionally NOT excluded: crystals break at turn end, so
		// the channel is intact for the cast being assembled.
		if (Attachment->IsFusion())
		{
			return Attachment->Fusion.HasGemHalf()
					   ? TOptional<EItemTier>(Attachment->Fusion.GemHalf().Tier)
					   : TOptional<EItemTier>();
		}
		if (Attachment->IsCrystal())
		{
			return Attachment->Crystal.Id.Tier;
		}
		return TOptional<EItemTier>();
	}
	}

	return TOptional<EItemTier>();
}

float UActionExecutor::GetTierGapDamageMultiplier(AActor *Actor, const FAction &Action) const
{
	// Non-logging on purpose — the AI calls this once per candidate action while
	// scoring. Execution-path logging lives in ResolveTierGapMultiplier.
	const TOptional<EItemTier> ChannelTier = ResolveChannelTier(Actor, Action);
	if (!ChannelTier.IsSet())
	{
		return TierGapDamage::MATCHED_TIER;
	}
	return TierGapDamage::GetTierGapDamageMultiplier(
		ResolveActionTier(Actor, Action), ChannelTier.GetValue());
}

float UActionExecutor::ResolveTierGapMultiplier(AActor *Actor, const FAction &Action, const FString &ActionName) const
{
	// The APPLIED value comes from the shared accessor — the same one the AI
	// preview uses — so real damage and AI estimates cannot diverge. The tiers
	// are re-resolved below only to format the log line.
	const float Multiplier = GetTierGapDamageMultiplier(Actor, Action);

	const EItemTier ActionTier = ResolveActionTier(Actor, Action);
	const TOptional<EItemTier> ChannelTier = ResolveChannelTier(Actor, Action);
	if (!ChannelTier.IsSet())
	{
		UE_LOG(LogTemp, Display,
			   TEXT("[TierGap] %s action=%s channel=NONE -> mult=1.00 (no channel, no scaling)"),
			   *ActionName, *TierHelpers::GetTierName(ActionTier));
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("[TierGap] %s %s (applied)"),
			   *ActionName,
			   *UTierGapDamageDebug::GetMultiplierString(ActionTier, ChannelTier.GetValue()));
	}

	return Multiplier;
}

// ========================================
// DEFERRED ACTIVATION (D8)
// ========================================

int32 UActionExecutor::GetActionActivationDelay(AActor *Actor, const FAction &Action) const
{
	const USkillDataBase *Skill = nullptr;
	switch (Action.ActionType)
	{
	case EActionType::Spell:
		Skill = Action.SpellData;
		break;
	case EActionType::Ability:
		// Cluster 3: covers attacks (folded into Ability) — reads the merged pointer, populated at
		// construction. (The old weapon-default re-resolution is gone: SkillData is set up front.)
		Skill = ResolveActionSkill(Action);
		break;
	default:
		break;
	}
	return Skill ? Skill->ActivationDelay : 0;
}

bool UActionExecutor::TryArmDeferredActivation(AActor *Actor, const FAction &Action, FOnActionComplete &OnComplete)
{
	const int32 Delay = GetActionActivationDelay(Actor, Action);
	if (Delay <= 0 || Action.bIsDeferredFire)
	{
		// Regression guard: zero-delay actions and fire-time resubmissions
		// never enter the deferral path.
		return false;
	}

	// Freeze the intent: the action's SkillData is resolved at construction (basic attacks pull the
	// active weapon's attack there), so the armed copy already carries what will fire — a weapon switch
	// during the delay can't change it. (Cluster 3: the old null-AttackData re-resolution is gone.)
	FAction ArmedAction = Action;

	// Arming consumes this turn's action and pays FULL costs now; the skill
	// executes at fire time (8c) cost-free (bIsDeferredFire skips both paths).
	const int32 EnergyCost = CalculateActionEnergyCost(Actor, ArmedAction);
	if (!SpendEnergy(Actor, EnergyCost))
	{
		FActionResult FailResult;
		FailResult.Executor = Actor;
		FailResult.ActionType = Action.ActionType;
		FailResult.bSuccess = false;
		FailResult.ErrorMessage = TEXT("Failed to spend energy (arm)");
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(FailResult);
		}
		return true;
	}

	// ApplyCommitCosts stashes deferred infusion HP on the execution context —
	// give it a minimal one, then settle the HP immediately: the arm IS the
	// commit, no finalize is coming for this action.
	FActionExecutionContext ArmContext;
	ArmContext.Action = ArmedAction;
	ArmContext.Executor = Actor;
	ArmContext.bInProgress = true;
	CurrentExecutionContext = ArmContext;
	ApplyCommitCosts(Actor, ArmedAction);
	ApplyPendingInfusionHPCost(Actor);

	OnActionStarted.Broadcast(Actor, ArmedAction, EnergyCost);

	UE_LOG(LogTemp, Log, TEXT("[Deferred] %s armed %s — fires in %d turn(s)"),
		   *Actor->GetName(), *ArmedAction.GetActionName(), Delay);

	// The orchestrator owns the queue (NOT TurnManager — its sim is replayed
	// by belt preview) and registers via this broadcast.
	OnActionDeferredArmed.Broadcast(Actor, ArmedAction, Delay);

	FActionResult Result;
	Result.Executor = Actor;
	Result.ActionType = Action.ActionType;
	Result.bSuccess = true;
	Result.EnergySpent = EnergyCost;

	// 2b: the ritual cast montage IS the arm turn — play ONLY it, hold the turn
	// open, and complete the arm when it ends (FinishArmTurn). The cost/commit
	// above is the commit; the montage is the visible channel. Presence-driven:
	// no RitualCastMontage → keep the synchronous immediate-complete (the SC9
	// no-arm-gesture fallback). The skill resolves from the slot just set.
	USkillDataBase *ArmSkill = GetCurrentSkillData();
	if (ArmSkill && ArmSkill->RitualCastMontage)
	{
		// Hold the context open (bInProgress = true) across the wait so the
		// next-action guard sees the arm in progress; FinishArmTurn settles it.
		// Cache the armed-success payload + stash the completion in the runner's
		// single-fire slot. PendingExecutionActor lets the montage-end dispatcher
		// resolve the actor (the arm returns before the runner sets it).
		PendingExecutionActor = Actor;
		PendingFinalResult = Result;
		AsyncActionCallback = OnComplete;
		bArmingRitual = true;
		BindActionAnimationEnd(Actor); // sets bWaitingForAnimationEnd
		UE_LOG(LogTemp, Log, TEXT("[Montage] PlayArm %s"),
			   *ArmSkill->RitualCastMontage->GetName());
		PlayActionMontageOnActor(Actor, ArmSkill->RitualCastMontage, 1.0f);
		return true;
	}

	// No ritual clip — synchronous immediate-complete (today's banked arm).
	CurrentExecutionContext.Reset();
	if (OnComplete.IsBound())
	{
		OnComplete.Execute(Result);
	}
	return true;
}

// ========================================
// EXECUTION - MAIN ENTRY POINT
// ========================================

FActionResult UActionExecutor::ExecuteAction(AActor *Actor, const FAction &Action)
{
	FActionResult Result;
	Result.Executor = Actor;
	Result.ActionType = Action.ActionType;

	// Validate first
	FActionValidationResult Validation = ValidateAction(Actor, Action);
	if (!Validation.bIsValid)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = Validation.ErrorMessage;
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Action validation failed: %s"), *Validation.ErrorMessage);
		return Result;
	}

	// Broadcast start
	OnActionStarted.Broadcast(Actor, Action, Validation.EnergyCost);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s executing %s (Cost: %d EP)"),
		   *Actor->GetName(), *Action.GetActionName(), Validation.EnergyCost);

	// Check for Broken Darkness break triggers BEFORE executing
	UCharacterData *CharData = GetCharacterData(Actor);
	CheckBrokenDarknessBreak(Actor, Action, CharData);

	// Activate infusion VFX if applicable
	int32 MaxInfusionLevel = FMath::Max(Action.SpellInfusionLevel, Action.AbilityInfusionLevel);
	if (MaxInfusionLevel > 0)
	{
		if (UInfusionVFXComponent *InfusionVFX = Actor->FindComponentByClass<UInfusionVFXComponent>())
		{
			InfusionVFX->SetInfusionLevel(MaxInfusionLevel);
		}
	}

	// Apply commit-time costs (HP / crystal wear / etc.) based on infusion source.
	// Costs are paid at commit, not at cast success — wear/HP loss happens even
	// if the action subsequently fails to land.
	ApplyCommitCosts(Actor, Action);

	// Route to appropriate executor.
	// Phase D: Spell/Ability/Attack sync paths retired — async + ApplyHit is the only path
	// for those action types. Sync ExecuteAction now only handles instant-resolution actions
	// (Item, Defend). Spell/Ability/Attack callers must use ExecuteActionAsync; reaching this
	// switch with one of those types is a caller bug.
	switch (Action.ActionType)
	{
	case EActionType::Item:
		Result = ExecuteItem(Actor, Action.ItemData, Action.Targets);
		break;

	case EActionType::Defend:
		Result = ExecuteDefend(Actor);
		break;

	case EActionType::Spell:
	case EActionType::Ability:
		UE_LOG(LogTemp, Warning,
			   TEXT("[ActionExecutor::ExecuteAction] %s called sync for action type %d — Spell/Ability/Attack must go through ExecuteActionAsync (Phase D)"),
			   *Actor->GetName(), static_cast<int32>(Action.ActionType));
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Spell/Ability/Attack must use ExecuteActionAsync");
		break;

	default:
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Unhandled action type");
		break;
	}

	// Broadcast completion
	OnActionCompleted.Broadcast(Actor, Result);

	return Result;
}

// ========================================
// EXECUTION - ASYNC
// ========================================
void UActionExecutor::ExecuteActionAsync(AActor *Actor, const FAction &Action, FOnActionComplete OnComplete)
{
	// Lazy-bind to DefenseSystem on first use. Subsystem init order is alphabetical, so
	// ActionExecutor::Initialize runs before DefenseSystem exists; binding there silently no-ops.
	// By the time any action runs, DefenseSystem exists. Idempotent — bDefenseEventsBound guards re-binding.
	if (!bDefenseEventsBound)
	{
		BindDefenseSystemEvents();
	}

	// Check for existing async action
	if (CurrentExecutionContext.IsSet() && CurrentExecutionContext->bInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Cannot start async action - another in progress"));

		FActionResult FailResult;
		FailResult.bSuccess = false;
		FailResult.ErrorMessage = TEXT("Another async action in progress");
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(FailResult);
		}
		return;
	}
	// Validate action
	FActionValidationResult Validation = ValidateAction(Actor, Action);
	if (!Validation.bIsValid)
	{
		FActionResult FailResult;
		FailResult.bSuccess = false;
		FailResult.ErrorMessage = Validation.ErrorMessage;

		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Async action validation failed: %s"),
			   *Validation.ErrorMessage);

		if (OnComplete.IsBound())
		{
			OnComplete.Execute(FailResult);
		}
		return;
	}

	// D8 Stage 8b: a skill with ActivationDelay > 0 ARMS instead of executing —
	// costs paid now, queued on the orchestrator, fired in 8c. Zero-delay and
	// fire-time resubmissions fall through to the normal path unchanged.
	if (TryArmDeferredActivation(Actor, Action, OnComplete))
	{
		return;
	}

	// Create execution context
	FActionExecutionContext Context;
	Context.Action = Action;
	Context.Executor = Actor;
	Context.bInProgress = true;
	Context.StartTime = FPlatformTime::Seconds();

	// Initialize partial result
	Context.PartialResult.Executor = Actor;
	Context.PartialResult.ActionType = Action.ActionType;
	Context.PartialResult.bSuccess = true;

	// Store context and callback
	CurrentExecutionContext = Context;
	AsyncActionCallback = OnComplete;

	// Reset coordination flags. FinalizeAsyncAction fires only when both flip true:
	// bAllDefensesResolved (set by CheckAndFinalizeAsyncAction) and !bWaitingForAnimationEnd
	// (cleared by UnbindActionAnimationEnd in OnActionAnimationEnded).
	bWaitingForAnimationEnd = false;
	bAllDefensesResolved = false;
	bAsyncCompletionFired = false;

	// Broadcast start
	OnActionStarted.Broadcast(Actor, Action, Validation.EnergyCost);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Starting async action: %s by %s"),
		   *Action.GetActionName(), *Actor->GetName());

	// Get character data for calculations
	UCharacterData *CharData = GetCharacterData(Actor);

	// === BROKEN DARKNESS & INFUSION HOOKS ===
	// Check for Broken Darkness break triggers
	CheckBrokenDarknessBreak(Actor, Action, CharData);

	// Cache for approach completion callback
	PendingExecutionActor = Actor;
	PendingExecutionCharData = CharData;

	// Handle instant actions (no animation, no movement)
	if (Action.ActionType == EActionType::Defend ||
		Action.ActionType == EActionType::SwitchWeapon ||
		Action.ActionType == EActionType::Flee)
	{
		FActionResult Result = ExecuteAction(Actor, Action);
		CurrentExecutionContext.Reset();
		PendingExecutionActor = nullptr;
		PendingExecutionCharData = nullptr;
		if (OnComplete.IsBound())
		{
			OnComplete.Execute(Result);
		}
		return;
	}

	// Handle Item - has animation but no movement
	if (Action.ActionType == EActionType::Item)
	{
		ExecuteItemAsync(Actor, Action, CharData);
		return;
	}

	// Apply commit-time costs for Attack / Ability / Spell.
	// Placed AFTER instant-action and Item early-returns so:
	//   - Defend/SwitchWeapon/Flee path goes through synchronous ExecuteAction,
	//     which calls ApplyCommitCosts there (avoiding double-charge here).
	//   - Items have no infusion cost path.
	//   - Attack/Ability/Spell pay exactly once, here, before movement starts.
	ApplyCommitCosts(Actor, Action);

	// Per-action stat modifiers accumulated from all active sources
	// (Reality innate/slotted/infused, Evolution slotted/infused).
	// Stashed on context so all consumers across the async lifecycle
	// (movement → animation → damage → defense) read the same snapshot.
	const FActionStatModifiers ActionMods = ComputeActionStatModifiers(Action, Actor);
	if (CurrentExecutionContext.IsSet())
	{
		CurrentExecutionContext->ActionMods = ActionMods;
	}

	if (ActionMods.IsActive())
	{
		UE_LOG(LogTemp, Log,
			   TEXT("[ActionExecutor] %s ActionMods active — CritDmg:%.1f%% RawDmg:%.1f%% SpellDmg:%.1f%% StatusMult:%.1f%% ActSpd:%.1f%%"),
			   *Actor->GetName(),
			   ActionMods.CritDamage, ActionMods.RawDamage,
			   ActionMods.SpellDamage, ActionMods.StatusMultiplier, ActionMods.ActionSpeed);
	}

	// Execution start (W3/SC-D): ALL action types take one path — record the
	// warp-origin snapshot (the pre-action pose the ReturnMontage warp targets),
	// then begin the skill. No lerp-approach branch: the skill montage's root
	// motion (warped, if the montage carries a window) carries melee to the
	// target; the ReturnMontage carries it back. Un-warped montages attack in
	// place (the interim). The snapshot lives on the runner now — the component
	// is gone (SC-D); the snapshot target/arena-center are no longer recorded
	// (they were never read by the warp-return).
	GridPosition = Actor->GetActorLocation();
	GridRotation = Actor->GetActorRotation();
	bHasGridPosition = true;
	BeginSkillExecution(Actor);
}

// ========================================
// ASYNC EXECUTOR SHARED HELPERS
// ========================================

bool UActionExecutor::ValidateInfusionGate(const FAction &Action, bool bImmuneToInfusion, int32 InfusionLevel)
{
	const bool bWantsInfusion =
		(Action.SelectedSource != EInfusionSourceOption::None) ||
		(InfusionLevel > 0);
	if (bWantsInfusion && bImmuneToInfusion)
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("This action cannot be infused.");
		FinalizeAsyncAction();
		return false;
	}
	return true;
}

bool UActionExecutor::IsInfusionImmune(AActor *User, bool bActionImmune) const
{
	if (bActionImmune)
	{
		return true;
	}
	if (!User)
	{
		return false;
	}
	if (UWeaponManager *WM = GetWeaponManager())
	{
		if (UWeaponData *Weapon = WM->GetActiveWeapon(User))
		{
			if (Weapon->bImmuneToInfusion)
			{
				return true;
			}
		}
	}
	if (URingManager *RM = GetRingManager())
	{
		if (URingData *Ring = RM->GetActiveRing(User))
		{
			if (Ring->bImmuneToInfusion)
			{
				return true;
			}
		}
	}
	return false;
}

// ==================== HYBRID PER-HIT FLAGS (B1) ====================

// Resolve authored per-hit flags into a full per-impact table (length HitCount), parallel to
// ResolveImpactDifficulty: each authored FDamageSplitEntry places its flags at HitNumber-1; unauthored
// hits keep all-false defaults. Sparse → full so ApplyOneImpact reads by ImpactIndex.
static TArray<FResolvedHitFlags> ResolveImpactFlags(int32 HitCount, const TArray<FDamageSplitEntry> &Split)
{
	TArray<FResolvedHitFlags> Table;
	Table.SetNum(FMath::Max(1, HitCount)); // all-false defaults
	for (const FDamageSplitEntry &E : Split)
	{
		const int32 Idx = E.HitNumber - 1;
		if (Table.IsValidIndex(Idx))
		{
			Table[Idx].bIgnoreDamage = E.bIgnoreDamage;
			Table[Idx].bIgnoreStatus = E.bIgnoreStatus;
			Table[Idx].bInterruptable = E.bInterruptable;
		}
	}
	return Table;
}

// Spell analog — per cast-entry (index i = CastArray[i]'s flags), parallel to ResolveCastDifficulty.
static TArray<FResolvedHitFlags> ResolveCastFlags(const TArray<FSkillCastEntry> &CastArray)
{
	TArray<FResolvedHitFlags> Table;
	Table.Reserve(CastArray.Num());
	for (const FSkillCastEntry &E : CastArray)
	{
		FResolvedHitFlags F;
		F.bIgnoreDamage = E.bIgnoreDamage;
		F.bIgnoreStatus = E.bIgnoreStatus;
		F.bInterruptable = E.bInterruptable;
		F.bOverrideStatScaling = E.bOverrideStatScaling; // per-cast stat toggle (spell): stashed per CastEntryIndex
		Table.Add(F);
	}
	return Table;
}

// Stash the current impact's flags onto the per-target context (read index-agnostically by ApplyOneImpact):
// melee passes ResolvedHitFlags + ImpactIndex; spell passes ResolvedCastFlags + CastEntryIndex.
static void StashHitFlags(FPendingDefenseContext &Ctx, const TArray<FResolvedHitFlags> &Table, int32 Index)
{
	const FResolvedHitFlags F = Table.IsValidIndex(Index) ? Table[Index] : FResolvedHitFlags();
	Ctx.bIgnoreDamage = F.bIgnoreDamage;
	Ctx.bIgnoreStatus = F.bIgnoreStatus;
	Ctx.bInterruptable = F.bInterruptable;
	// Separate cast slot (Option A): only spell's ResolveCastFlags populates F.bOverrideStatScaling; melee's
	// ResolveImpactFlags leaves it false, so this never clobbers the physical attack-level Ctx.bOverrideStatScaling.
	Ctx.bCastOverrideStatScaling = F.bOverrideStatScaling;
}

void UActionExecutor::FinalizeDamageInputs(const USkillDataBase *Skill, int32 FinalDamage, int32 HitCount, int32 &OutDamagePerHit)
{
	CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense = FinalDamage;

	// The even DamagePerHit out-param feeds the PRE-defense consumers only
	// (defense-window/AI heuristic inputs) — a representative average. The
	// APPLIED per-hit damage consumes ResolvedDamageSplit below in
	// ApplyDamageAfterDefense (D1 reader switch, Stage 12 SC4).
	OutDamagePerHit = FinalDamage / FMath::Max(1, HitCount);

	CurrentExecutionContext->ResolvedDamageSplit = ResolveDamageSplit(
		HitCount, Skill ? Skill->DamageSplit : TArray<FDamageSplitEntry>());

	// Resolve per-impact defense difficulty from the SAME DamageSplit array — one source for both
	// damage and difficulty, so a hit's two halves can't drift. Null/unauthored skill → empty split +
	// all-Inherit default → ResolveImpactDifficulty fills the table with Easy (×1.0) per impact, so
	// cluster 4 multiplies by ×1.0 and existing attacks are unchanged. Read at ResolveImpactDefense by ImpactIndex.
	CurrentExecutionContext->ResolvedDifficulty = ResolveImpactDifficulty(
		HitCount,
		Skill ? Skill->DamageSplit : TArray<FDamageSplitEntry>(),
		Skill ? Skill->DefaultDifficulty : FDefenseDifficultyTriple());

	// Per-hit hybrid flags (B1), HitNumber-matched in lockstep with ResolvedDifficulty above.
	CurrentExecutionContext->ResolvedHitFlags = ResolveImpactFlags(
		HitCount, Skill ? Skill->DamageSplit : TArray<FDamageSplitEntry>());
}

void UActionExecutor::LogActionDispatch(
	EActionType ActionType,
	int32 InfusionLevel,
	int32 FinalDamage,
	int32 NumTargets) const
{
	UE_LOG(LogTemp, Log,
		   TEXT("[ActionExecutor] %s async L%d - %d damage, opened %d defense windows"),
		   *UEnum::GetValueAsString(ActionType),
		   InfusionLevel,
		   FinalDamage,
		   NumTargets);
}

void UActionExecutor::ExecuteSpellAsync(AActor *Caster, const FAction &Action, UCharacterData *CasterData)
{
	USpellData *Spell = Action.SpellData;
	if (!Spell || !CasterData)
	{
		CancelAsyncAction();
		return;
	}

	UCharacterDataComponent *CasterComp = GetCharacterDataComponent(Caster);
	if (!CasterComp)
	{
		CancelAsyncAction();
		return;
	}

	// Commit 3: reject early when bImmuneToInfusion is true but the action carries
	// infusion (source selection OR charge level). No energy spent, no damage dealt.
	// Equipment-level immunity (active weapon / ring) ORs in via IsInfusionImmune.
	if (!ValidateInfusionGate(Action, IsInfusionImmune(Caster, Spell->bImmuneToInfusion), Action.SpellInfusionLevel))
	{
		return;
	}

	// Calculate and spend energy. Single source of truth: the validator.
	// Honours the ring/weapon-crystal waiver (0 EP) and applies the same
	// base * infusion-multiplier * efficiency for innate/evolution spells.
	const int32 FinalEnergyCost = CalculateActionEnergyCost(Caster, Action);

	if (!SpendEnergy(Caster, FinalEnergyCost))
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("Failed to spend energy");
		FinalizeAsyncAction();
		return;
	}
	CurrentExecutionContext->PartialResult.EnergySpent = FinalEnergyCost;

	// Broken Darkness pays durability wear on every evolution-source cast
	// (the EP-waiver counterpart in CalculateActionEnergyCost). Universal
	// hook — the wear formula self-returns 0 for matched-tier uninfused
	// casts, so no extra gating is required here.
	if (CasterComp->IsBrokenDarkness() && Action.SpellSource == ESpellSource::Evolution)
	{
		if (ULoadoutComponent *LC = GetLoadoutComponent(Caster))
		{
			if (UCrystalManager *CrystalMgr = GetGameInstance()
												  ? GetGameInstance()->GetSubsystem<UCrystalManager>()
												  : nullptr)
			{
				CrystalMgr->ProcessPostCastEvolutionWear(
					Caster, LC, Spell->Tier, Action.SpellInfusionLevel, /*bIsSpell=*/true);
			}
		}
	}

	// Spell size — D6 reader switch (SC6): the primary Cast entry's
	// VisualScale (= old BaseSize for migrated content), scaled by infusion.
	// VisualScale, NOT Size: Size is the hitbox (BaseSize×HitboxRatio) and
	// would shrink every defense window ×0.8 — Size-keyed defense sizing is a
	// banked future balance decision. Empty CastArray (necessarily all-default
	// per the migration guard) → DEFAULT_SPELL_SIZE = the old BaseSize default.
	const float BaseVisualScale = Spell->CastArray.Num() > 0
									  ? Spell->CastArray[0].VisualScale
									  : CombatConstants::DEFAULT_SPELL_SIZE;
	float FinalSpellSize = BaseVisualScale * GetSpellInfusionSizeMultiplier(Action.SpellInfusionLevel);

	// Per-action stat modifiers (Reality, Evolution, future buffs) — populated
	// on the execution context by ExecuteActionAsync via ComputeActionStatModifiers.
	const FActionStatModifiers ActionMods = CurrentExecutionContext.IsSet()
												? CurrentExecutionContext->ActionMods
												: FActionStatModifiers();

	// Calculate damage with charge infusion multiplier. Stage 6 cluster 5: SINGLE-entry spells source
	// the base from the cast entry's own Damage (the SPELL damage layer) when authored (>0); 0 or
	// multi-entry → -1 → the skill-level Spell->BaseDamage as before (byte-identical). The override only
	// swaps the raw base; SpellDamage/Mind/element scaling still runs at ApplyHit (ActionType=Spell).
	const int32 EntryBaseOverride =
		(Spell->CastArray.Num() == 1 && Spell->CastArray[0].Damage > 0)
			? Spell->CastArray[0].Damage
			: -1;
	int32 BaseDamage = Spell->CalculateDamage(CasterData, ActionMods, EntryBaseOverride);
	// 6-3-3: per-mode, stat-scaled charge damage. Mode resolved once from the action's
	// infusion source (ring/weapon/innate/evolution), reused for the status log below.
	const EInfusionMode SpellMode = ResolveInfusionMode(Action.SelectedSource, Caster);
	float DamageMultiplier = GetChargeDamageMultiplier(Action.SpellInfusionLevel, SpellMode, /*bIsSpell*/ true, CasterComp);
	int32 FinalDamage = FMath::RoundToInt(BaseDamage * DamageMultiplier);

	// Tier-gap (B2): final multiplicative factor, stacking with the charge
	// multiplier above. 1.0 (no channel / matched tier) leaves damage unchanged.
	const float TierGapMult = ResolveTierGapMultiplier(Caster, Action, Spell->Name);
	FinalDamage = FMath::RoundToInt(FinalDamage * TierGapMult);

	// 6-4: the live spell status multiplier — per-mode, stat-scaled (L0 → ×1.0). Logged here and
	// applied to SpellBaseBuildup below (replaces the retired inline L1 +50%).
	float StatusMultiplier = GetChargeStatusMultiplier(Action.SpellInfusionLevel, SpellMode, CasterComp);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Spell charge L%d - Size: %.1fx, Damage: %d (%.1fx), Status: %.1fx"),
		   Action.SpellInfusionLevel,
		   GetSpellInfusionSizeMultiplier(Action.SpellInfusionLevel),
		   FinalDamage,
		   DamageMultiplier,
		   StatusMultiplier);

	// Store in result for reference. BaseDamageBeforeDefense receives the
	// infused damage because that is what defense will reduce — "before defense"
	// refers to the defense pipeline, not "before infusion".
	CurrentExecutionContext->PartialResult.AttackSize = FinalSpellSize;
	CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense = FinalDamage;
	CurrentExecutionContext->PartialResult.AttackElement = Spell->Element;

	// Get valid targets
	TArray<AActor *> ValidTargets = FilterValidTargets(Action.Targets);

	if (ValidTargets.Num() == 0)
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("No valid targets");
		FinalizeAsyncAction();
		return;
	}

	// Cache spell data for notify-triggered VFX
	PendingSpellCaster = Caster;
	PendingSpellData = Spell;
	PendingSpellTargets = ValidTargets;
	PendingSpellSize = FinalSpellSize;
	PendingSpellDamage = FinalDamage;

	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Caster);
	bPendingSpellIsBrokenDarkness = BDManager && BDManager->IsTransformed();

	// Bind to notify for VFX timing
	BindSpellNotify(Caster);

	// Play animation - VFX spawns on SpellRelease notify (NOT here)
	PlaySpellAnimation(Caster, Spell, FinalSpellSize, ActionMods);

	// Calculate damage per hit (infused total split across hits)
	int32 DamagePerHit = FinalDamage / FMath::Max(1, Spell->HitCount);

	// Check for forbidden element self-damage (BD casting Dark Light/Void).
	// Backlash scales with the spell's intrinsic power, NOT the infused amount —
	// infusion multiplies output damage, not the metaphysical strain of the cast.
	ProcessForbiddenElementCast(Caster, Spell->Element, static_cast<float>(BaseDamage));

	// Phase C1: Spell buildup flows through the defense pipeline. Session Y
	// moved trigger resolution into UStatusBuildupManager (resolves from Element
	// + PhysicalType), so no StatusType is plumbed through the pipeline anymore.
	// Spells have no physical type - PhysicalDamageType::None.
	int32 SpellBaseBuildup = 0;
	if (Spell->StatusBuildup > 0)
	{
		// 6-4: apply the unified per-mode stat-scaled status multiplier (computed above for the
		// log) — replaces the retired inline L1 +50%. L0 → ×1.0; L1/L2 → progressive per-mode bonus.
		SpellBaseBuildup = FMath::RoundToInt(Spell->StatusBuildup * StatusMultiplier);
	}

	// Commit 2: if bIsRawMode, fold StatusBuildup into FinalDamage at the
	// orchestrator boundary so downstream defense + ApplyHit see normalised inputs.
	ActionUtils::ApplyRawModeRedirect(Spell->bIsRawMode, FinalDamage, SpellBaseBuildup);

	FinalizeDamageInputs(Spell, FinalDamage, Spell->HitCount, DamagePerHit);
	PendingSpellDamage = FinalDamage; // Spell-specific: cached for VFX notify

	// Stage 6 cluster 2: resolve per-cast-entry defense difficulty (Option A — keyed by cast-entry
	// index, not impact ordinal). Parallel to the melee ResolvedDifficulty fill in FinalizeDamageInputs;
	// inert until cluster 4 reads it at projectile arrival (cluster 3 stamps the index on the projectile).
	CurrentExecutionContext->ResolvedCastDifficulty =
		ResolveCastDifficulty(Spell->CastArray, Spell->DefaultDifficulty);

	// Per-cast-entry hybrid flags (B1), parallel to ResolvedCastDifficulty (indexed by CastEntryIndex).
	CurrentExecutionContext->ResolvedCastFlags = ResolveCastFlags(Spell->CastArray);

	// Open defense windows for all targets (damage and buildup both applied after defense resolves)
	OpenDefenseWindowsForTargets(
		Caster,
		ValidTargets,
		FinalSpellSize,
		FinalDamage,
		DamagePerHit,
		Spell->HitCount,
		Spell->Element,
		true,					   // Can crit
		EActionType::Spell,		   // ActionType — drives post-defense stat selection
		Action.SpellInfusionLevel, // InfusionLevel
		Action.SelectedSource,	   // SelectedSource
		SpellBaseBuildup,		   // BaseStatusBuildup (Phase C1)
		EPhysicalDamageType::None, // PhysicalDamageType - spells have none (Session Y)
		0.3f,					   // Default window duration - TODO: get from spell data
		false					   // hybrid stat toggle is PHYSICAL-ONLY now; spell reads it per-cast from ResolvedCastFlags
	);

	// Stage 6 cluster 4/6: single-entry PROJECTILE/HOMING spells defend PER-IMPACT at projectile arrival.
	// Convert the just-opened lumped windows (0.3s timer-close) to COUNT-BASED so they survive the cast
	// animation + flight (8s failsafe), and mark ExpectedImpacts = the delivery's Count so the window
	// closes only after the LAST of N staggered arrivals. Done HERE (action start), NOT at dispatch: the
	// cast-time window would otherwise auto-close at 0.3s before the SpellRelease notify fires. Gate: one
	// cast entry, single ASSET hit (Spell->HitCount==1, the physical-only field), Projectile, ANY
	// Count — single (Count==1) is the N=1 case of the same path. Projectile/AOE/Instant single-entry
	// all convert here; multi-entry and asset-multi-hit stay on the lumped path (untouched).
	//
	// Burst even-split: window BaseDamage stays the FULL FinalDamage; the runtime Ctx.HitCount = Count
	// makes each arrival's slice State.BaseDamage/HitCount = FinalDamage/Count (the ResolvedDamageSplit
	// table is sized to Spell->HitCount==1, so it mismatches Count → the even-fallback slice fires, with
	// the remainder distributed across the first R arrivals in ResolveImpactDefense). Spell->HitCount (the
	// asset field) is NOT touched — only the runtime per-impact count.
	//
	// Double-apply guard: re-opening closes the lumped window, which would fire OnDefenseWindowClosed →
	// apply NOW; so REMOVE the pending context first (that close becomes a no-op), re-open count-based,
	// then restore it — the per-impact arrivals become the ONLY apply.
	if (Spell->CastArray.Num() == 1 && Spell->HitCount == 1)
	{
		const FSkillCastEntry &E0 = Spell->CastArray[0];
		if (E0.DeliveryType == ESpellDeliveryType::Projectile)
		{
			if (UDefenseSystem *DefenseSys = GetDefenseSystem(); DefenseSys && CurrentExecutionContext.IsSet())
			{
				const int32 BurstCount = FMath::Max(1, E0.Count);
				for (AActor *Target : ValidTargets)
				{
					if (FPendingDefenseContext *PDC = CurrentExecutionContext->PendingDefenses.Find(Target))
					{
						FPendingDefenseContext Saved = *PDC;
						CurrentExecutionContext->PendingDefenses.Remove(Target); // close-on-reopen → no-op
						// 0.3f = AI reaction-delay seed (matches the lumped open); bManualClose makes the
						// real close the per-impact arrivals, with the 8s failsafe covering cast + flight.
						DefenseSys->OpenDefenseWindow(Caster, Target, FinalSpellSize, FinalDamage,
													 0.3f, /*bManualClose=*/true);
						Saved.ExpectedImpacts = BurstCount; // close after the Nth arrival
						Saved.HitCount = BurstCount;		// runtime per-impact count (NOT the asset HitCount)
						Saved.ImpactsLanded = 0;
						CurrentExecutionContext->PendingDefenses.Add(Target, Saved);
					}
				}
			}
		}
		else if (E0.DeliveryType == ESpellDeliveryType::AOE || E0.DeliveryType == ESpellDeliveryType::Instant)
		{
			// Stage 6 cluster 6 — single-entry AOE AND Instant: each defender takes ONE hit, resolved at
			// the Cast notify (no travel — SpawnAOEEffect / ResolveInstantSpell IS the impact moment).
			// The two deliveries share this branch byte-for-byte: one full-damage impact per defender,
			// closed at the Cast resolve. Mirror the projectile conversion: swap the just-opened lumped
			// cast-time window for a count-based one (ExpectedImpacts=1 per defender, 8s failsafe so it
			// survives the cast animation) so the ONLY apply is the per-defender resolve — no lumped 0.3s
			// auto-close double-apply. FULL FinalDamage per defender — neither is even-split (everyone hit
			// takes the whole hit). N defenders = N independent single-impact windows. Instant differs from
			// AOE only in DEFENSE availability (all three, Hard) — authored in SpellData, not here.
			if (UDefenseSystem *DefenseSys = GetDefenseSystem(); DefenseSys && CurrentExecutionContext.IsSet())
			{
				for (AActor *Target : ValidTargets)
				{
					if (FPendingDefenseContext *PDC = CurrentExecutionContext->PendingDefenses.Find(Target))
					{
						FPendingDefenseContext Saved = *PDC;
						CurrentExecutionContext->PendingDefenses.Remove(Target); // close-on-reopen → no-op
						DefenseSys->OpenDefenseWindow(Caster, Target, FinalSpellSize, FinalDamage,
													 0.3f, /*bManualClose=*/true);
						Saved.ExpectedImpacts = 1; // ONE AOE impact per defender
						Saved.HitCount = 1;		   // runtime per-impact count (full damage, no split)
						Saved.ImpactsLanded = 0;
						CurrentExecutionContext->PendingDefenses.Add(Target, Saved);
					}
				}
			}
		}
	}

	LogActionDispatch(EActionType::Spell, Action.SpellInfusionLevel, FinalDamage, ValidTargets.Num());
}

void UActionExecutor::ExecuteSkillAsync(AActor *User, const FAction &Action, UCharacterData *UserData)
{
	// Merged attack+ability dispatch (Cluster 2). Reads the unified skill pointer (base type). An
	// attack is just an ability with AbilityInfusionLevel=0 — every charge multiplier is L0-neutral
	// (cost ×1.0, damage ×1.0, status 0), so this body produces the attack's prior numbers. The local
	// stays named `Ability`: every field it touches (BaseDamage/HitCount/StatusBuildup/bIsRawMode/
	// DamageSplit/bImmuneToInfusion, CalculateDamage/CalculateEnergyCost) lives on USkillDataBase now.
	USkillDataBase *Ability = ResolveActionSkill(Action);

	// SkillData is populated at construction (basic attacks resolve the active weapon's attack there),
	// so a null skill here is a genuinely malformed action → cancel. (Cluster 3 dropped the old
	// Attack-gated weapon-default re-resolution; the enum no longer distinguishes attacks.)
	if (!Ability || !UserData)
	{
		CancelAsyncAction();
		return;
	}

	UCharacterDataComponent *UserComp = GetCharacterDataComponent(User);
	if (!UserComp)
	{
		CancelAsyncAction();
		return;
	}

	// Commit 3: reject early when bImmuneToInfusion is true but the action carries
	// infusion (source selection OR charge level). No energy spent, no damage dealt.
	// Equipment-level immunity (active weapon / ring) ORs in via IsInfusionImmune.
	if (!ValidateInfusionGate(Action, IsInfusionImmune(User, Ability->bImmuneToInfusion), Action.AbilityInfusionLevel))
	{
		return;
	}

	// Calculate and spend energy
	const bool bIsInfused = (Action.SelectedSource != EInfusionSourceOption::None);
	int32 BaseEnergyCost = Ability->CalculateEnergyCost(UserData, bIsInfused);

	// Crystal-sourced abilities are powered by the crystal's stored charge, not the
	// caster's EP — zero EP (they pay in durability via ApplyCommitCosts). Mirrors the
	// ring/weapon-crystal spell waiver in CalculateActionEnergyCost.
	int32 FinalEnergyCost = 0;
	const bool bCrystalSource =
		Action.SelectedSource == EInfusionSourceOption::ActiveRing ||
		Action.SelectedSource == EInfusionSourceOption::PrimaryRing ||
		Action.SelectedSource == EInfusionSourceOption::WeaponCrystal;
	if (!bCrystalSource)
	{
		// Charge multiplier (x1.5/x2.0) x upside-only RawDamage surcharge (L>0), then
		// efficiency. Mirrors CalculateActionEnergyCost so validation and spend agree.
		const float CostMultiplier = ComputeInfusionCostMultiplier(Action.AbilityInfusionLevel, /*bIsSpell*/ false, UserComp);
		const float EfficiencyMult = GetEffectiveEnergyCostEfficiencyMultiplier(User);
		FinalEnergyCost = FMath::RoundToInt(BaseEnergyCost * CostMultiplier * EfficiencyMult);
	}

	if (!SpendEnergy(User, FinalEnergyCost))
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("Failed to spend energy");
		FinalizeAsyncAction();
		return;
	}
	CurrentExecutionContext->PartialResult.EnergySpent = FinalEnergyCost;

	// Calculate base damage. Element-infusion damage penalty was removed per
	// the locked cost matrix — infusion now pays its tax via durability wear /
	// HP backlash / status build / energy multipliers, not flat damage tax.
	int32 BaseDamage = Ability->CalculateDamage(UserData, bIsInfused);

	// Element handling. Infusion routes the user's innate element through;
	// non-infused abilities stay Generic.
	ESpellElement Element = ESpellElement::Generic;
	if (bIsInfused && UserData->HasInnateElement())
	{
		Element = UserData->InnateElement;
	}

	// 6-3-3: per-mode, stat-scaled charge damage (L1 now also bonused, was ×1.0 before).
	// Mode resolved once from the action's infusion source, reused for the status site below.
	const EInfusionMode AbilityMode = ResolveInfusionMode(Action.SelectedSource, User);
	float DamageMultiplier = GetChargeDamageMultiplier(Action.AbilityInfusionLevel, AbilityMode, /*bIsSpell*/ false, UserComp);
	int32 FinalDamage = FMath::RoundToInt(BaseDamage * DamageMultiplier);

	// Tier-gap (B2): final multiplicative factor, stacking with the charge
	// multiplier above (RequirementPenalty already sits inside CalculateDamage).
	const float TierGapMult = ResolveTierGapMultiplier(User, Action, Ability->Name);
	FinalDamage = FMath::RoundToInt(FinalDamage * TierGapMult);

	// Spell Size (fixed, no character scaling)
	float AttackSize = 1.0f;

	// Store in result. BaseDamageBeforeDefense receives the post-multiplier damage —
	// "before defense" refers to the defense pipeline, not "before infusion".
	CurrentExecutionContext->PartialResult.AttackSize = AttackSize; // remove attack size its pointless for abilities
	CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense = FinalDamage;
	CurrentExecutionContext->PartialResult.AttackElement = Element;

	// Per-action stat modifiers (stashed on context by ExecuteActionAsync).
	const FActionStatModifiers ActionMods = CurrentExecutionContext.IsSet()
												? CurrentExecutionContext->ActionMods
												: FActionStatModifiers();

	// Get valid targets BEFORE animating — zero survivors fizzles cleanly
	// without a wasted cast animation (matches the spell path's ordering).
	TArray<AActor *> ValidTargets = FilterValidTargets(Action.Targets);

	if (ValidTargets.Num() == 0)
	{
		CurrentExecutionContext->PartialResult.bSuccess = false;
		CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("No valid targets");
		FinalizeAsyncAction();
		return;
	}

	// Play animation (merged path — PlaySkillAnimation reads the unified base SkillMontage)
	PlaySkillAnimation(User, Ability, ActionMods);

	// Buildup amount only. Session Y: trigger type resolves in the manager from
	// (Element, PhysicalType). Abilities have no physical type - if the action
	// is non-infused (Element=Generic, Physical=None) the resolver returns None
	// and nothing fires on cap; if infused, Element drives the trigger.
	// TODO: a future ability-specific override (via Effects[]) could route a
	// different trigger when uninfused — wire when the design lands.
	int32 AbilityBaseBuildup = 0;
	if (Ability->StatusBuildup > 0)
	{
		// 6-4: scale ability status by the per-mode stat-scaled charge multiplier (L0 → ×1.0,
		// uninfused unchanged; L>0 → the mode's status bonus). Same getter as the damage path.
		const float StatusMult = GetChargeStatusMultiplier(Action.AbilityInfusionLevel, AbilityMode, UserComp);
		AbilityBaseBuildup = FMath::RoundToInt(Ability->StatusBuildup * StatusMult);
	}

	ActionUtils::ApplyRawModeRedirect(Ability->bIsRawMode, FinalDamage, AbilityBaseBuildup);

	int32 DamagePerHit = 0;
	FinalizeDamageInputs(Ability, FinalDamage, Ability->HitCount, DamagePerHit);

	// Ability buildup-bar source resolution (per locked design):
	//   - PhysicalDamageType: from active weapon (matches the weapon being
	//     wielded, not the per-attack data which no longer owns the field)
	//   - Element: from Action.SelectedSource — per-action infusion choice.
	//     Only resolves to the user's InnateElement when an elemental source
	//     is selected (Generic/Resonator default to Generic; Caster picks
	//     elemental source to push their innate element)
	ESpellElement AbilityElement = ESpellElement::Generic;
	EPhysicalDamageType AbilityPhysicalType = EPhysicalDamageType::None;

	if (UWeaponManager *WeaponMgr = GetWeaponManager())
	{
		if (UWeaponData *Weapon = WeaponMgr->GetActiveWeapon(User))
		{
			AbilityPhysicalType = Weapon->PhysicalDamageType;
		}
	}

	// bIsInfused was already computed at the top of the function from
	// Action.SelectedSource — reuse it here for the buildup-bar element.
	if (bIsInfused && UserData && UserData->HasInnateElement())
	{
		AbilityElement = UserData->InnateElement;
	}

	OpenDefenseWindowsForTargets(
		User,
		ValidTargets,
		AttackSize,
		FinalDamage,
		DamagePerHit,
		Ability->HitCount,
		AbilityElement,
		true,						 // Can crit
		Action.ActionType,			 // ActionType (Attack/Ability both scale RawDamage)
		Action.AbilityInfusionLevel, // InfusionLevel
		Action.SelectedSource,		 // SelectedSource
		AbilityBaseBuildup,			 // BaseStatusBuildup
		AbilityPhysicalType,		 // PhysicalDamageType - inherits active weapon
		0.3f,
		Ability->bOverrideStatScaling); // hybrid stat toggle — attack-wide, read from the skill root (physical)

	LogActionDispatch(Action.ActionType, Action.AbilityInfusionLevel, FinalDamage, ValidTargets.Num());
}

// ========================================
// EXECUTION - ITEM ASYNC
// ========================================

void UActionExecutor::ExecuteItemAsync(AActor *Actor, const FAction &Action, UCharacterData *CharData)
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] ExecuteItemAsync - Invalid actor"));
		FinalizeAsyncAction();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Executing item async: %s by %s"),
		   *ItemIdentity::GetDisplayName(Action.ItemData), *Actor->GetName());

	// Face target (if not self)
	AActor *Target = Action.Targets.Num() > 0 ? Action.Targets[0] : Actor;
	bool bIsSelfTarget = (Target == Actor);

	if (!bIsSelfTarget)
	{
		FVector Direction = Target->GetActorLocation() - Actor->GetActorLocation();
		Direction.Z = 0;
		if (!Direction.IsNearlyZero())
		{
			Actor->SetActorRotation(Direction.GetSafeNormal().Rotation());
			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Item user facing toward %s"), *Target->GetName());
		}
	}

	// Get and play animation
	ULoadoutComponent *Loadout = GetLoadoutComponent(Actor);
	UAnimMontage *ItemMontage = Loadout ? Loadout->GetItemUseAnimation(bIsSelfTarget) : nullptr;

	if (ItemMontage)
	{
		BindActionAnimationEnd(Actor);
		PlayActionMontageOnActor(Actor, ItemMontage, 1.0f);
	}

	// Execute item logic (healing, damage, buffs)
	FActionResult Result = ExecuteItem(Actor, Action.ItemData, Action.Targets);

	// Store result for finalization
	if (CurrentExecutionContext.IsSet())
	{
		CurrentExecutionContext->PartialResult = Result;
	}

	// Set timeout as failsafe
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AsyncTimeoutHandle,
			this,
			&UActionExecutor::OnAsyncActionTimeout,
			5.0f,
			false);
	}

	// If no animation, gate on defenses via TryFinalize.
	// Items typically don't open defense windows; if none pending, mark resolved.
	if (!bWaitingForAnimationEnd)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] No item animation"));
		if (CurrentExecutionContext.IsSet() && CurrentExecutionContext->AreAllDefensesResolved())
		{
			bAllDefensesResolved = true;
		}
		TryFinalizeAsyncAction();
	}
	// Otherwise OnActionAnimationEnded will handle finalization
}

// ============================================================
// 6. DEFENSE WINDOW INTEGRATION
// ============================================================

void UActionExecutor::OpenDefenseWindowsForTargets(
	AActor *Attacker,
	const TArray<AActor *> &Targets,
	float AttackSize,
	int32 BaseDamage,
	int32 DamagePerHit,
	int32 HitCount,
	ESpellElement Element,
	bool bCanCrit,
	EActionType ActionType,
	int32 InfusionLevel,
	EInfusionSourceOption SelectedSource,
	int32 BaseStatusBuildup,
	EPhysicalDamageType PhysicalDamageType,
	float WindowDuration,
	bool bOverrideStatScaling)
{
	if (!CurrentExecutionContext.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] No execution context for defense windows"));
		return;
	}

	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (!DefenseSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] DefenseSystem not available - applying damage directly"));

		// Fallback: apply damage without defense
		for (AActor *Target : Targets)
		{
			int32 TotalDamage = ProcessMultiHit(
				Attacker, Target, DamagePerHit, HitCount, Element, bCanCrit,
				CurrentExecutionContext->PartialResult);

			CurrentExecutionContext->PartialResult.TotalDamageDealt += TotalDamage;
			CurrentExecutionContext->PartialResult.DamagePerTarget.Add(Target, TotalDamage);
			CurrentExecutionContext->PartialResult.AffectedTargets.Add(Target);
		}
		return;
	}

	// Count-based close now covers attacks AND abilities (step 1 of the attack/ability
	// merge): both play montages that can carry Impact notifies (the impact counter that
	// drives the close). Abilities WITHOUT authored Impact notifies degrade gracefully —
	// ImpactsLanded stays 0, the reconcile/failsafe tail closes the window and applies the
	// full lumped result (byte-identical to today). Spells keep the timer close
	// (bManualClose=false) — projectile/AOE migrate to the counter at Stage 6.
	// ExpectedImpacts>0 doubles as the "this window is count-based" sentinel read by the
	// Impact-notify handler; 0 leaves the window on its normal timer.
	// Attack collapsed into Ability (Cluster 3) — count-based close is the not-Spell / bPerImpact gate.
	const bool bUseCountBasedClose = (ActionType == EActionType::Ability);

	// Create pending defense context for each target
	for (AActor *Target : Targets)
	{
		FPendingDefenseContext DefenseContext;
		DefenseContext.Attacker = Attacker;
		DefenseContext.Target = Target;
		DefenseContext.BaseDamage = BaseDamage;
		DefenseContext.DamagePerHit = DamagePerHit;
		DefenseContext.AttackSize = AttackSize;
		DefenseContext.Element = Element;
		DefenseContext.HitCount = HitCount;
		DefenseContext.ExpectedImpacts = bUseCountBasedClose ? HitCount : 0;
		DefenseContext.bCanCrit = bCanCrit;
		DefenseContext.WindowDuration = WindowDuration;
		DefenseContext.ActionType = ActionType;
		DefenseContext.InfusionLevel = InfusionLevel;
		DefenseContext.SelectedSource = SelectedSource;
		DefenseContext.BaseStatusBuildup = BaseStatusBuildup;
		DefenseContext.PhysicalDamageType = PhysicalDamageType;
		DefenseContext.bOverrideStatScaling = bOverrideStatScaling;

		CurrentExecutionContext->PendingDefenses.Add(Target, DefenseContext);

		// Open defense window in DefenseSystem. Melee → manual (count-based) close: the
		// last landed hit closes it; the per-window timer is armed at MaxWindowDuration
		// as a failsafe. WindowDuration is still passed as the AI reaction-delay seed.
		DefenseSys->OpenDefenseWindow(
			Attacker,
			Target,
			AttackSize,
			BaseDamage,
			WindowDuration,
			bUseCountBasedClose);

		UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] Opened defense window for %s (Size: %.1f, Damage: %d, CountClose: %s, Expected: %d)"),
			   *Target->GetName(), AttackSize, BaseDamage,
			   bUseCountBasedClose ? TEXT("yes") : TEXT("no"), DefenseContext.ExpectedImpacts);
	}
}

void UActionExecutor::OnDefenseWindowClosed(AActor *Defender, const FDefenseResult &DefenseResult)
{
	UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] OnDefenseWindowClosed CALLBACK FIRED for %s"),
		   Defender ? *Defender->GetName() : TEXT("null"));

	if (!CurrentExecutionContext.IsSet() || !CurrentExecutionContext->bInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Defense window closed but no async action in progress"));
		return;
	}

	// Find the pending defense context for this defender
	FPendingDefenseContext *ContextPtr = CurrentExecutionContext->PendingDefenses.Find(Defender);
	if (!ContextPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Defense resolved for unknown target: %s"),
			   *Defender->GetName());
		return;
	}

	FPendingDefenseContext Context = *ContextPtr;

	// Apply damage based on defense result
	ApplyDamageAfterDefense(
		Context.Attacker.Get(),
		Defender,
		Context,
		DefenseResult);

	// Broken Darkness absorption from defense
	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Defender);
	if (BDManager && BDManager->IsTransformed())
	{
		// Get attack info from pending context
		if (CurrentExecutionContext.IsSet())
		{
			FPendingDefenseContext *BDContext = CurrentExecutionContext->PendingDefenses.Find(Defender);
			if (BDContext)
			{
				// Get spell/ability energy cost for absorption calculation
				float EnergyCost = 0.0f;
				if (CurrentExecutionContext->Action.SpellData)
				{
					EnergyCost = CurrentExecutionContext->Action.SpellData->BaseEnergyCost;
				}
				else if (CurrentExecutionContext->Action.SkillData)
				{
					EnergyCost = CurrentExecutionContext->Action.SkillData->BaseEnergyCost;
				}

				BDManager->OnDefenseResolved(
					DefenseResult.DefenseType,
					DefenseResult,
					BDContext->Element,
					EnergyCost);
			}
		}
	}
	// Remove from pending list
	CurrentExecutionContext->PendingDefenses.Remove(Defender);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Defense resolved for %s - Type: %d, FinalDamage: %d, Pending: %d"),
		   *Defender->GetName(),
		   static_cast<int32>(DefenseResult.DefenseType),
		   DefenseResult.FinalDamage,
		   CurrentExecutionContext->GetPendingCount());

	// Check if all defenses resolved
	CheckAndFinalizeAsyncAction();
}

// OBSOLETE (Stage 3): superseded by ResolveImpactDefense — no live callers. The Stage 2
// model locked ONE action-level result at the first impact; Stage 3 resolves a fresh
// per-impact defense (split-then-reduce + per-impact match-and-consume). Kept un-deleted
// until the per-impact path is PIE-verified, per the incremental-removal rule.
void UActionExecutor::EnsureResultResolved(AActor *Defender)
{
	// Stage 2 ordering crux: the FDefenseResult is normally computed at CLOSE
	// (DefenseSystem::CloseDefenseWindow), but per-impact apply needs it at the FIRST
	// impact — before Skill-end. Compute it here from the current defense state via the
	// public DefenseSystem API (no DefenseSystem change) and STASH it on the context.
	// Idempotent: the single action-level result is LOCKED at the first impact and reused
	// by every later impact (per-impact defense INPUT is Stage 3 — a defender must commit
	// before the first impact for it to count this stage).
	if (!CurrentExecutionContext.IsSet() || !Defender)
	{
		return;
	}
	FPendingDefenseContext *Ctx = CurrentExecutionContext->PendingDefenses.Find(Defender);
	if (!Ctx || Ctx->bResultResolved)
	{
		return;
	}
	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (!DefenseSys)
	{
		return;
	}

	const FDefenseState State = DefenseSys->GetDefenseState(Defender);
	const FDefenseResult Result = DefenseSys->CalculateDefenseResult(
		State.BaseDamage,
		State.DefenseChosen,
		State.bInputReceived);

	Ctx->ResolvedDefenseType = Result.DefenseType;
	Ctx->ResolvedFinalDamage = Result.FinalDamage;
	Ctx->bResolvedSuccess = Result.bSuccess;
	Ctx->bResultResolved = true;

	UE_LOG(LogTemp, Log, TEXT("[Stage2] Defense result resolved for %s — Type: %d, ReducedTotal: %d, Success: %s"),
		   *Defender->GetName(), static_cast<int32>(Result.DefenseType), Result.FinalDamage,
		   Result.bSuccess ? TEXT("yes") : TEXT("no"));
}

void UActionExecutor::ResolveImpactDefense(AActor *Defender, int32 ImpactIndex, double ImpactTime, const FDefenseDifficultyTriple &ImpactDifficulty)
{
	// Stage 3 per-impact resolution (replaces EnsureResultResolved at the impact frame).
	// SPLIT-THEN-REDUCE: this impact takes its slice of the BASE damage FIRST, then the
	// matched defense input reduces that slice. Contrast Stage 2, which reduced the whole
	// total once and sliced the reduced figure — splitting first lets each impact carry its
	// own defense outcome (one parried, the next blocked) instead of one locked decision.
	if (!CurrentExecutionContext.IsSet() || !Defender)
	{
		return;
	}
	FPendingDefenseContext *Ctx = CurrentExecutionContext->PendingDefenses.Find(Defender);
	if (!Ctx)
	{
		return;
	}
	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (!DefenseSys)
	{
		return;
	}

	const FDefenseState State = DefenseSys->GetDefenseState(Defender);

	// This impact's slice of the BASE (pre-defense) damage. Same FloorToInt + 0.01 epsilon
	// as ApplyOneImpact so the split math is identical on both sides; size-mismatched table
	// → even split. The even-fallback also distributes the integer remainder (State.BaseDamage %
	// HitCount) across the first R arrivals (+1 each) so the N spell-burst shares sum EXACTLY to
	// FinalDamage — e.g. 50/3 → 17,17,16. Melee never reaches this fallback: ResolveDamageSplit
	// always returns a table sized HitCount, so bUseSplit is true for melee (its even split + remainder
	// live in ResolveDamageSplit). The fallback fires only for the spell burst (table sized to
	// Spell->HitCount==1, mismatching the runtime Ctx.HitCount=Count) — so this remainder-add is
	// spell-burst-only by construction, melee untouched.
	const int32 HitCount = FMath::Max(1, Ctx->HitCount);
	const TArray<float> &Split = CurrentExecutionContext->ResolvedDamageSplit;
	const bool bUseSplit = (Split.Num() == HitCount) && Split.IsValidIndex(ImpactIndex);
	const int32 EvenRemainder = (ImpactIndex < (State.BaseDamage % HitCount)) ? 1 : 0;
	const int32 BaseSlice = bUseSplit
								? FMath::FloorToInt(State.BaseDamage * (Split[ImpactIndex] / 100.0f) + 0.01f)
								: (State.BaseDamage / HitCount) + EvenRemainder;

	// Per-impact defense difficulty for THIS impact is now CALLER-SUPPLIED (the concrete resolved
	// triple): melee passes ResolvedDifficulty[ImpactIndex], spells pass ResolvedCastDifficulty
	// [CastEntryIndex] (Stage 6 cluster 4). Both guard with IsValidIndex at the call site → an
	// all-Inherit default triple → DefenseDifficultyMultiplier resolves it to Easy ×1.0 (no window
	// change). One resolver body, the difficulty source differs by caller.

	// Match-and-consume this impact against the timestamped input buffer (Stage 3). No
	// in-window press → bMatched=false → resolves as undefended (full slice, no reduction).
	// Ctx->ActionType selects the attacker's speed stat for the window duel (physical →
	// ActionSpeed, spell → SpellSpeed); attacker is read from the live state inside. The duel
	// window is scaled per-press by ImpactDifficulty inside MatchAndConsumeInput.
	const FDefenseInputMatch Match = DefenseSys->MatchAndConsumeInput(Defender, ImpactTime, Ctx->ActionType, ImpactDifficulty);

	const FDefenseResult Result = DefenseSys->CalculateDefenseResult(
		BaseSlice,
		Match.bMatched ? Match.Type : EDefenseType::None,
		Match.bMatched);

	// Stash the PER-IMPACT result — ResolvedFinalDamage now holds the reduced SLICE (not the
	// full total). ApplyOneImpact reads it directly when bDamageIsPerImpact=true. bResultResolved
	// stays true so the non-melee tail in ApplyDamageAfterDefense knows melee already resolved;
	// for melee the tail is empty (ImpactsLanded==HitCount) so the overwrite is harmless.
	Ctx->ResolvedDefenseType = Result.DefenseType;
	Ctx->ResolvedFinalDamage = Result.FinalDamage;
	Ctx->bResolvedSuccess = Result.bSuccess;
	Ctx->bResolvedPerfect = Match.bPerfect;
	Ctx->bResultResolved = true;

	AActor *Attacker = Ctx->Attacker.Get();

	// Piece 5: per-impact parry reflect. Each parried impact reflects ITS OWN slice — the
	// close-time lumped reflect is gated OFF for count-based windows (bCountBasedClose) so this
	// is the only reflect path for melee.
	if (Result.bSuccess && Result.DefenseType == EDefenseType::Parry && Result.ReflectedDamage > 0)
	{
		DefenseSys->ApplyParryReflect(Attacker, Defender, Result.ReflectedDamage);
	}

	// Piece 6: broadcast the per-impact outcome (payoffs are content, later).
	DefenseSys->OnDefenseResolved.Broadcast(Defender, Attacker, Result.DefenseType, Match.bPerfect, ImpactIndex);
	if (Match.bPerfect)
	{
		DefenseSys->OnDefensePerfect.Broadcast(Defender, Attacker, Result.DefenseType, Match.bPerfect, ImpactIndex);
	}

	UE_LOG(LogTemp, Log, TEXT("[Stage3] impact %d defense for %s — Type: %d, Slice: %d → %d, %s"),
		   ImpactIndex, *Defender->GetName(), static_cast<int32>(Result.DefenseType),
		   BaseSlice, Result.FinalDamage, Match.bPerfect ? TEXT("PERFECT") : TEXT("normal"));
}

void UActionExecutor::ApplyOneImpact(AActor *Attacker, AActor *Target, const FPendingDefenseContext &Context, int32 ImpactIndex, bool bDamageIsPerImpact)
{
	if (!CurrentExecutionContext.IsSet() || !Target)
	{
		return;
	}

	// Death guard — a prior impact this combo may have killed the target; mirror the old
	// loop's bTargetDied early-out so impacts after the kill don't apply.
	if (!IsTargetAlive(Target))
	{
		return;
	}

	// Successful dodge — no damage, no buildup, no ApplyHit (parity with the lumped dodge
	// skip). A FAILED dodge (attack too big) has bResolvedSuccess=false and ResolvedFinalDamage
	// = base → falls through to a normal hit.
	if (Context.bResolvedSuccess && Context.ResolvedDefenseType == EDefenseType::Dodge)
	{
		return;
	}

	const int32 HitCount = FMath::Max(1, Context.HitCount);

	// Bounds guard — Impact notifies must be authored 0..HitCount-1 (the authoring contract).
	// A stray index would read the wrong / no split — skip it loudly.
	if (ImpactIndex < 0 || ImpactIndex >= HitCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stage2] impact index %d out of range (HitCount %d) for %s — skipping stray (authoring-contract violation)"),
			   ImpactIndex, HitCount, *Target->GetName());
		return;
	}

	// Per-impact damage. Stage 3 (bDamageIsPerImpact): ResolvedFinalDamage is ALREADY this
	// impact's reduced slice (ResolveImpactDefense split-then-reduced it) — apply directly.
	// Legacy lumped tail (bDamageIsPerImpact=false): ResolvedFinalDamage is the full reduced
	// total — slice it here by the D1 DamageSplit table (even fractions when un-authored).
	// FloorToInt + 0.01 epsilon reproduces the legacy FinalDamage/HitCount truncation and
	// absorbs even-fraction float error (100/3 = 33.333332) so exact quotients don't floor
	// one short. Size-mismatched table → legacy even split.
	const TArray<float> &Split = CurrentExecutionContext->ResolvedDamageSplit;
	const bool bUseSplit = (Split.Num() == HitCount) && Split.IsValidIndex(ImpactIndex);
	const int32 ImpactDamage = bDamageIsPerImpact
								   ? Context.ResolvedFinalDamage
								   : (bUseSplit
										  ? FMath::FloorToInt(Context.ResolvedFinalDamage * (Split[ImpactIndex] / 100.0f) + 0.01f)
										  : (Context.ResolvedFinalDamage / HitCount));

	// Status now distributes per-hit (parallel to damage), not all on hit 0 — each impact applies its
	// SHARE by the SAME fraction the damage slice uses above (Split[i]/100 when authored, else 1/HitCount).
	// Sum of shares ≈ the full BaseStatusBuildup. Block/parry reduce THIS hit's share (per-hit defense →
	// per-hit status); dodge already returned above. Single hit (HitCount==1) → fraction 1.0 → unchanged.
	const float BuildupFraction = bUseSplit ? (Split[ImpactIndex] / 100.0f) : (1.0f / FMath::Max(1, HitCount));
	int32 EffectiveBuildup = FMath::RoundToInt(Context.BaseStatusBuildup * BuildupFraction);
	if (Context.bResolvedSuccess && Context.ResolvedDefenseType == EDefenseType::Block)
	{
		EffectiveBuildup = FMath::RoundToInt(EffectiveBuildup * CombatConstants::BLOCK_BUILDUP_MULTIPLIER);
	}
	else if (Context.bResolvedSuccess && Context.ResolvedDefenseType == EDefenseType::Parry)
	{
		EffectiveBuildup = FMath::RoundToInt(EffectiveBuildup * CombatConstants::PARRY_BUILDUP_MULTIPLIER);
	}

	FActionHitInput Input;
	Input.Attacker = Attacker;
	Input.Target = Target;
	Input.ActionType = Context.ActionType;
	// Two honest sources, one consume slot: spell reads the per-cast stash (bCastOverrideStatScaling, set per
	// CastEntryIndex); physical reads the attack-level context bool (root-sourced, attack-wide, lumped-tail-safe).
	Input.bOverrideStatScaling = (Context.ActionType == EActionType::Spell)
									 ? Context.bCastOverrideStatScaling
									 : Context.bOverrideStatScaling;
	Input.BaseDamage = Context.bIgnoreDamage ? 0 : ImpactDamage; // B1: per-hit no-damage moment
	Input.bCanCrit = Context.bCanCrit;
	Input.Element = Context.Element;
	Input.InfusionLevel = Context.InfusionLevel;
	Input.SelectedSource = Context.SelectedSource;
	Input.BaseStatusBuildup = Context.bIgnoreStatus ? 0 : EffectiveBuildup; // B1: per-hit no-status moment
	// Per-hit physical type, now that status distributes per-hit (B0): each hit's nonzero status share must
	// carry its real PhysicalDamageType so a physical attack's bar-cap trigger resolves on EVERY hit, not
	// just hit 0 (was hit-0-only — harmless when later hits applied zero buildup; now they don't).
	Input.PhysicalDamageType = Context.PhysicalDamageType;
	Input.ActionMods = CurrentExecutionContext->ActionMods;

	const FCombatHitResult HitResult = ApplyHit(Input);

	// Incremental aggregation into the running PartialResult — spread across impact frames +
	// the close-time tail, instead of one synchronous loop.
	FActionResult &PartialResult = CurrentExecutionContext->PartialResult;
	PartialResult.TotalDamageDealt += HitResult.DamageDealt;
	PartialResult.DamagePerTarget.FindOrAdd(Target) += HitResult.DamageDealt;
	PartialResult.AffectedTargets.AddUnique(Target);
	if (HitResult.bWasCritical)
	{
		PartialResult.bWasCritical = true;
	}

	// Death — broadcast EXACTLY ONCE, at the killing impact. The death guard at the top
	// prevents any later impact from re-entering ApplyHit for this target, so bTargetDied
	// can only be true once.
	if (HitResult.bTargetDied)
	{
		PartialResult.bCausedDeath = true;
		OnTargetKilled.Broadcast(Attacker, Target);
	}
}

void UActionExecutor::ApplyDamageAfterDefense(
	AActor *Attacker,
	AActor *Target,
	const FPendingDefenseContext &Context,
	const FDefenseResult &DefenseResult)
{
	if (!CurrentExecutionContext.IsSet() || !Target)
	{
		return;
	}

	// Local mutable copy — callers pass a const-ref snapshot.
	FPendingDefenseContext Ctx = Context;

	// CONSISTENCY: melee resolved + drained every impact per-frame (ResolveImpactDefense) →
	// ImpactsLanded==HitCount → the tail loop below is empty, so this stash is never read for
	// melee. Non-melee (no frames fired, bResultResolved=false) stashes from the close result
	// here — the unchanged lumped path that ApplyOneImpact slices (bDamageIsPerImpact=false).
	if (!Ctx.bResultResolved)
	{
		Ctx.ResolvedDefenseType = DefenseResult.DefenseType;
		Ctx.ResolvedFinalDamage = DefenseResult.FinalDamage;
		Ctx.bResolvedSuccess = DefenseResult.bSuccess;
		Ctx.bResultResolved = true;
	}

	// Apply only the UNDRAINED TAIL [ImpactsLanded, HitCount). Melee: all impacts drained
	// per-frame → ImpactsLanded==HitCount → tail empty (no double-apply). Non-melee:
	// ImpactsLanded==0 → tail = all → unchanged lumped behavior at the timer close.
	const int32 HitCount = FMath::Max(1, Ctx.HitCount);
	for (int32 i = Ctx.ImpactsLanded; i < HitCount; ++i)
	{
		if (!IsTargetAlive(Target))
		{
			break; // death stops the tail (mirror the per-impact death guard)
		}
		ApplyOneImpact(Attacker, Target, Ctx, i);
	}

	// Defense-outcome telemetry (preserved; now logs the tail range applied here).
	if (Ctx.bResolvedSuccess)
	{
		UE_LOG(LogTemp, Verbose,
			   TEXT("[ApplyDamageAfterDefense] %s defended via %d — tail [%d, %d)"),
			   *Target->GetName(), static_cast<int32>(Ctx.ResolvedDefenseType),
			   Ctx.ImpactsLanded, HitCount);
	}
}

// ============================================================
// 7. ASYNC FINALIZATION
// ============================================================

void UActionExecutor::CheckAndFinalizeAsyncAction()
{
	if (!CurrentExecutionContext.IsSet())
	{
		return;
	}

	if (CurrentExecutionContext->AreAllDefensesResolved())
	{
		bAllDefensesResolved = true;
		TryFinalizeAsyncAction();
	}
}

void UActionExecutor::TryFinalizeAsyncAction()
{
	if (bWaitingForAnimationEnd)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] TryFinalize: waiting for animation"));
		return;
	}

	// No-pending fold (turn-hang fix): the bAllDefensesResolved bool is raised by a
	// window-CLOSE event (CheckAndFinalizeAsyncAction) or the no-animation dispatch
	// blocks — both of which a montage-carrying action that opens ZERO defense windows
	// never reaches (the dispatch block is gated on !bWaitingForAnimationEnd). Without
	// this, such an action (e.g. a deferred ritual with no resolving window) strands
	// finalize on gate 2 forever → turn hangs. Animation has already ended here (gate 1
	// passed), so raise the flag when nothing is pending — symmetric with the window path.
	if (CurrentExecutionContext.IsSet() && CurrentExecutionContext->AreAllDefensesResolved())
	{
		bAllDefensesResolved = true;
	}

	if (!bAllDefensesResolved)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] TryFinalize: waiting for defenses"));
		return;
	}

	// Hardening (return-skip fix): MontagePhase is the authoritative chain-progress
	// signal - finalize only once the chain has fully ended (Done) or never ran
	// (None: instant / validation-fail / no-animation paths). Guards against a stray
	// early unbind clearing bWaitingForAnimationEnd while a leg is still mid-chain.
	if (MontagePhase != EMontagePhase::Done && MontagePhase != EMontagePhase::None)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] TryFinalize: chain not complete (phase=%d)"),
			   (int32)MontagePhase);
		return;
	}

	FinalizeAsyncAction();
}

void UActionExecutor::FinalizeAsyncAction()
{
	if (!CurrentExecutionContext.IsSet())
	{
		return;
	}

	// Clear timeout timer + any pending telegraph timers (normally already fired by now, but
	// clear so none can fire into the next action).
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AsyncTimeoutHandle);
		for (FTimerHandle &H : TelegraphTimerHandles)
		{
			World->GetTimerManager().ClearTimer(H);
		}
	}
	TelegraphTimerHandles.Empty();

	// Get final result
	FActionResult FinalResult = CurrentExecutionContext->PartialResult;
	FAction Action = CurrentExecutionContext->Action;
	AActor *Executor = CurrentExecutionContext->Executor.Get();

	// Apply post-action effects (status effects, etc.)
	if (FinalResult.bSuccess && Executor)
	{
		// Apply Effects[] for any action type that carries them.
		// All three data assets (UAbilityData, USpellData, UWeaponAttackData)
		// expose Effects[] in the same shape after Job 2. Runs post-defense,
		// post-damage so Result.TotalDamageDealt / bWasCritical / bCausedDeath
		// are populated for OnHit / OnCrit / OnKill condition checks.
		const TArray<FSkillEffect> *EffectsToApply = nullptr;
		FString SourceName;

		switch (Action.ActionType)
		{
		case EActionType::Ability:
			// Cluster 3: covers attacks (folded into Ability) — reads the merged pointer.
			if (USkillDataBase *Skill = ResolveActionSkill(Action))
			{
				EffectsToApply = &Skill->Effects;
				SourceName = Skill->Name;
			}
			break;

		case EActionType::Spell:
			if (Action.SpellData)
			{
				EffectsToApply = &Action.SpellData->Effects;
				SourceName = Action.SpellData->Name;
			}
			break;

		default:
			break;
		}

		if (EffectsToApply && EffectsToApply->Num() > 0)
		{
			// Resolve the cast's effective element for sweep-4's status-bar
			// manipulation effects (StatusIncrease / StatusDecrease). Spells
			// carry an inherent element; infusion overrides it. Abilities and
			// attacks only have an element when infused. Other (non-gauge-
			// manipulating) effect types stay element=Generic inside the loop
			// to preserve historical behaviour.
			ESpellElement ResolvedElement = ESpellElement::Generic;
			if (Action.ActionType == EActionType::Spell && Action.SpellData)
			{
				ResolvedElement = (Action.SelectedSource != EInfusionSourceOption::None)
									  ? GetElementForSourceOption(Executor, Action.SelectedSource)
									  : Action.SpellData->Element;
			}
			else if (Action.SelectedSource != EInfusionSourceOption::None)
			{
				ResolvedElement = GetElementForSourceOption(Executor, Action.SelectedSource);
			}

			// Physical type for the ability/attack authored-DoT branch — the wielded
			// weapon's declared type (staff=Impact, dagger=Pierce, sword=Slash). None
			// when no weapon resolves (e.g. ring-primary caster): the branch then
			// falls back to the legacy Generic shape.
			EPhysicalDamageType ActionPhysicalType = EPhysicalDamageType::None;
			if (Action.ActionType != EActionType::Spell)
			{
				if (UWeaponManager *WeaponMgr = GetWeaponManager())
				{
					if (UWeaponData *Weapon = WeaponMgr->GetActiveWeapon(Executor))
					{
						ActionPhysicalType = Weapon->PhysicalDamageType;
					}
				}
			}

			ApplySkillEffects(
				Executor,
				FinalResult.AffectedTargets,
				*EffectsToApply,
				SourceName,
				FinalResult,
				FinalResult.bCausedDeath,
				ResolvedElement,
				Action.ActionType,
				ActionPhysicalType);
		}

		// Process post-cast by source (durability wear, etc.)
		if (Action.ActionType == EActionType::Spell && Action.SpellData)
		{
			ProcessPostCastBySource(Executor, Action.SpellData, Action.SpellSource, Action.SpellInfusionLevel);
		}
	}

	// Deferred infusion HP cost — deducted HERE, on the ALWAYS-RUN finalize path
	// (outside the bSuccess block above), so it pays on hit AND miss, mirroring the
	// commit path's unconditional charge. Runs after the infused effect, damage and
	// animation have resolved; a lethal cost kills the caster now, post-action.
	ApplyPendingInfusionHPCost(Executor);

	// Mark complete
	CurrentExecutionContext->bInProgress = false;

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Async action finalized - Success: %s, Damage: %d, Targets: %d"),
		   FinalResult.bSuccess ? TEXT("Yes") : TEXT("No"),
		   FinalResult.TotalDamageDealt,
		   FinalResult.AffectedTargets.Num());

	// Cache result for after return
	PendingFinalResult = FinalResult;
	Executor = CurrentExecutionContext->Executor.Get();

	// Clear context now (action is done, just waiting for return)
	CurrentExecutionContext.Reset();

	// Return already happened (W3): the montage chain's ReturnMontage (via
	// PlayReturnStep + warp) ran INSIDE the animation phase, before finalize —
	// bWaitingForAnimationEnd spanned it. There is no movement travel to wait
	// for and no component state to reset (SC-D), so finalize falls straight to
	// completion.
	CompleteAsyncActionFinal(Executor);
}

void UActionExecutor::CompleteAsyncActionFinal(AActor *Executor)
{
	// Settle facing — the action is fully done (montage → ReturnMontage →
	// return movement all complete); reassert the one rule so the actor ends
	// facing the nearest living enemy (arena center when none alive).
	if (Executor)
	{
		if (UCombatGridSubsystem *Grid = GetWorld()->GetGameInstance()->GetSubsystem<UCombatGridSubsystem>())
		{
			Grid->UpdateActorFacing(Executor, CachedArenaCenter);
		}
	}

	// Snapshot the pending handles BEFORE the callback can re-enter. Execute below
	// can advance the turn and launch the next action (deferred fire), which sets
	// fresh PendingExecutionActor/CharData. Snapshot-compare lets the teardown null
	// only when no re-entrant action replaced them (mirrors the callback guard).
	AActor *ActorAtEntry = PendingExecutionActor;

	// Fire callback — one-shot latch guarantees exactly one completion per action,
	// so a double finalize (timeout failsafe + legitimate finalize) can neither
	// double-fire nor swallow the advance.
	if (!bAsyncCompletionFired)
	{
		bAsyncCompletionFired = true;
		// Execute synchronously advances the turn, which can launch the next action (deferred fire) and RE-BIND
		// AsyncActionCallback. Unbinding AFTER Execute would clobber that fresh binding — so capture a local,
		// clear the member, then run the local. The re-entrant binding then survives to its own finalize.
		FOnActionComplete PendingCallback = AsyncActionCallback;
		AsyncActionCallback.Unbind();
		if (PendingCallback.IsBound())
		{
			PendingCallback.Execute(PendingFinalResult);
		}
	}

	// Broadcast completion
	if (Executor)
	{
		OnAsyncActionCompleted.Broadcast(Executor, PendingFinalResult);
		OnActionCompleted.Broadcast(Executor, PendingFinalResult);
	}

	// Clear pending state — but only if a re-entrant deferred fire didn't already
	// replace the handle. If PendingExecutionActor != ActorAtEntry, a new action
	// owns it now; leave it for that action's own finalize (root fix: VFX/Cast on a
	// deferred ritual fire would otherwise read null here).
	if (PendingExecutionActor == ActorAtEntry)
	{
		PendingExecutionActor = nullptr;
		PendingExecutionCharData = nullptr;
	}
}

void UActionExecutor::OnAsyncActionTimeout()
{
	if (!CurrentExecutionContext.IsSet() || !CurrentExecutionContext->bInProgress)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Async action timed out with %d pending defenses"),
		   CurrentExecutionContext->GetPendingCount());

	// Apply full damage to any remaining targets
	for (auto &Pair : CurrentExecutionContext->PendingDefenses)
	{
		FPendingDefenseContext &Context = Pair.Value;

		// Create failed defense result (full damage)
		FDefenseResult FailedDefense;
		FailedDefense.bSuccess = false;
		FailedDefense.FinalDamage = Context.BaseDamage;

		ApplyDamageAfterDefense(
			Context.Attacker.Get(),
			Context.Target.Get(),
			Context,
			FailedDefense);
	}

	CurrentExecutionContext->PendingDefenses.Empty();
	FinalizeAsyncAction();
}

void UActionExecutor::CancelAsyncAction()
{
	if (!CurrentExecutionContext.IsSet())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Cancelling async action"));

	// Clear timers (failsafe + any pending burst chain + any pending telegraph cues — a cancelled
	// action must not fire telegraphs for impacts that never land).
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AsyncTimeoutHandle);
		World->GetTimerManager().ClearTimer(BurstTimerHandle);
		for (FTimerHandle &H : TelegraphTimerHandles)
		{
			World->GetTimerManager().ClearTimer(H);
		}
	}
	BurstSpawnQueue.Empty();
	TelegraphTimerHandles.Empty();
	ActiveBurstSpell = nullptr;

	// Unbind animation + notify spine bindings
	if (PendingExecutionActor)
	{
		UnbindActionAnimationEnd(PendingExecutionActor);
		UnbindCombatNotify(PendingExecutionActor);
		PendingExecutionActor = nullptr;
		PendingExecutionCharData = nullptr;
	}
	MontagePhase = EMontagePhase::None;
	PendingMontagePlayRate = 1.0f;
	bArmingRitual = false;
	ChainActor = nullptr; // teardown hygiene - drop the chain-owned handles
	ChainSkill = nullptr;

	// Close any open defense windows
	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (DefenseSys)
	{
		for (auto &Pair : CurrentExecutionContext->PendingDefenses)
		{
			if (Pair.Key.IsValid())
			{
				DefenseSys->CloseDefenseWindow(Pair.Key.Get());
			}
		}
	}

	// Mark failed
	CurrentExecutionContext->PartialResult.bSuccess = false;
	CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("Action cancelled");

	FinalizeAsyncAction();
}

bool UActionExecutor::AllInterruptSucceeded() const
{
	if (!CurrentExecutionContext.IsSet())
	{
		return false;
	}
	const auto &Pending = CurrentExecutionContext->PendingDefenses;
	if (Pending.Num() == 0)
	{
		return false;
	}
	for (const auto &Pair : Pending)
	{
		const FPendingDefenseContext &Ctx = Pair.Value;
		if (!Ctx.bResultResolved)
		{
			return false; // not every defender has resolved this hit yet → don't abort
		}
		if (!(Ctx.bResolvedSuccess &&
			  (Ctx.ResolvedDefenseType == EDefenseType::Parry ||
			   Ctx.ResolvedDefenseType == EDefenseType::Dodge)))
		{
			return false; // any defender failed to parry/dodge → no abort, the attack continues
		}
	}
	return true; // every defender resolved AND parried/dodged the interruptable hit
}

bool UActionExecutor::RecordChokeAndShouldAbort(AActor *Target, bool bDefended)
{
	if (!CurrentExecutionContext.IsSet())
	{
		return false;
	}
	FActionExecutionContext &Exec = *CurrentExecutionContext;
	Exec.ChokeOutcomes.Add(Target, bDefended);
	if (Exec.ChokeExpectedCount <= 0 || Exec.ChokeOutcomes.Num() < Exec.ChokeExpectedCount)
	{
		return false; // no active choke, or not every target has resolved this entry yet
	}
	for (const auto &O : Exec.ChokeOutcomes)
	{
		if (!O.Value)
		{
			return false; // some target failed to parry/dodge → no abort, the spell continues
		}
	}
	return true; // every target resolved AND parried/dodged the choke point
}

void UActionExecutor::InterruptAsyncAction(AActor *Attacker, USkillDataBase *Skill)
{
	if (!CurrentExecutionContext.IsSet())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Interrupt — all targets parried/dodged an interruptable hit; aborting the rest"));

	// 1. Stop further spawns + cue timers — no more hits/casts land.
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurstTimerHandle);
		for (FTimerHandle &H : TelegraphTimerHandles)
		{
			World->GetTimerManager().ClearTimer(H);
		}
	}
	BurstSpawnQueue.Empty();
	TelegraphTimerHandles.Empty();
	ActiveBurstSpell = nullptr;

	// 2. Clear the pending contexts FIRST, then close their windows — so the close callbacks find no
	//    context and apply no tail (the aborted remainder must NOT land). The interruptable hit's own
	//    (parried) damage already applied at the resolve site before this call.
	TArray<AActor *> Defenders;
	for (auto &Pair : CurrentExecutionContext->PendingDefenses)
	{
		if (Pair.Key.IsValid())
		{
			Defenders.Add(Pair.Key.Get());
		}
	}
	CurrentExecutionContext->PendingDefenses.Empty();
	if (UDefenseSystem *DefenseSys = GetDefenseSystem())
	{
		for (AActor *D : Defenders)
		{
			DefenseSys->CloseDefenseWindow(D);
		}
	}

	// 3. Mark the action interrupted (defended).
	CurrentExecutionContext->PartialResult.bSuccess = false;
	CurrentExecutionContext->PartialResult.ErrorMessage = TEXT("Action interrupted (defended)");

	// 4. Recover. With a ReturnMontage: PlayReturnStep stops the skill montage and warps the attacker back
	//    — it sets CurrentChainMontage=ReturnMontage BEFORE the stop, so the skill-stop's spurious
	//    OnActionMontageEnded is ignored by the montage-identity guard; the ReturnMontage's own end drives
	//    FinishMontageChain → finalize (single finalize). Without one: unbind, hard-stop, finalize directly.
	if (Skill && Skill->ReturnMontage && Attacker)
	{
		PlayReturnStep(Attacker, Skill);
	}
	else
	{
		UnbindActionAnimationEnd(Attacker);
		UnbindCombatNotify(Attacker);
		if (UCombatAnimInstance *Anim = GetCombatAnimInstance(Attacker))
		{
			Anim->StopActionMontage();
		}
		FinalizeAsyncAction();
	}
}

bool UActionExecutor::IsAsyncActionInProgress() const
{
	return CurrentExecutionContext.IsSet() && CurrentExecutionContext->bInProgress;
}

const FActionExecutionContext *UActionExecutor::GetCurrentExecutionContext() const
{
	return CurrentExecutionContext.IsSet() ? &CurrentExecutionContext.GetValue() : nullptr;
}

// ============================================================
// 8. DEFENSE SYSTEM BINDING
// ============================================================

UDefenseSystem *UActionExecutor::GetDefenseSystem() const
{
	if (DefenseSystemRef)
	{
		return DefenseSystemRef;
	}

	UGameInstance *GI = GetGameInstance();
	if (GI)
	{
		UDefenseSystem *DefenseSys = GI->GetSubsystem<UDefenseSystem>();
		const_cast<UActionExecutor *>(this)->DefenseSystemRef = DefenseSys;
		return DefenseSys;
	}

	return nullptr;
}

void UActionExecutor::BindDefenseSystemEvents()
{
	if (bDefenseEventsBound)
	{
		return;
	}

	UDefenseSystem *DefenseSys = GetDefenseSystem();
	UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] BindDefenseSystemEvents called - DefenseSys: %s"),
		   DefenseSys ? TEXT("VALID") : TEXT("NULL"));

	if (DefenseSys)
	{
		DefenseSys->OnDefenseWindowClosed.AddDynamic(this, &UActionExecutor::OnDefenseWindowClosed);
		bDefenseEventsBound = true;

		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Bound to DefenseSystem events - IsBound: %s"),
			   DefenseSys->OnDefenseWindowClosed.IsBound() ? TEXT("TRUE") : TEXT("FALSE"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] DefenseSystem not yet available - will retry on first action"));
	}
}

void UActionExecutor::UnbindDefenseSystemEvents()
{
	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (DefenseSys)
	{
		DefenseSys->OnDefenseWindowClosed.RemoveDynamic(this, &UActionExecutor::OnDefenseWindowClosed);

		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Unbound from DefenseSystem events"));
	}
	bDefenseEventsBound = false;
}

// ========================================
// EXECUTION - ITEM
// ========================================

FActionResult UActionExecutor::ExecuteItem(
	AActor *User,
	FCrystalId Id,
	const TArray<AActor *> &Targets)
{
	FActionResult Result;
	Result.Executor = User;
	Result.ActionType = EActionType::Item;

	if (!User)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid user");
		return Result;
	}

	// === Find and use item slot in LoadoutComponent ===
	ULoadoutComponent *Loadout = GetLoadoutComponent(User);
	int32 ItemSlotIndex = -1;

	if (Loadout && Loadout->IsReadyForBattle())
	{
		// Find which slot contains this item
		TArray<FItemLoadoutSlot> UsableItems = Loadout->GetUsableItems();
		for (int32 i = 0; i < UsableItems.Num(); ++i)
		{
			if (UsableItems[i].CrystalId == Id)
			{
				ItemSlotIndex = i;
				break;
			}
		}

		if (ItemSlotIndex < 0)
		{
			Result.bSuccess = false;
			Result.ErrorMessage = TEXT("Item not in loadout or no uses remaining");
			return Result;
		}
	}

	// Delegate to ItemExecutor for full item handling
	UItemExecutor *ItemExec = GetItemExecutor();
	if (!ItemExec)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("ItemExecutor not available");
		return Result;
	}

	// Items don't cost energy
	Result.EnergySpent = 0;

	// Determine target (self if not specified)
	AActor *Target = Targets.Num() > 0 ? Targets[0] : User;

	// Execute through ItemExecutor
	if (Targets.Num() > 1)
	{
		FItemUseResult ItemResult = ItemExec->UseItemMultiTarget(User, Id, Targets);

		Result.bSuccess = ItemResult.bSuccess;
		Result.ErrorMessage = ItemResult.ErrorMessage;
		Result.TotalDamageDealt = ItemResult.DamageDealt;
		Result.TotalHealingDone = ItemResult.HealingDone;
		Result.StatusEffectsApplied = ItemResult.BuffsApplied + ItemResult.DebuffsRemoved;

		for (AActor *T : Targets)
		{
			Result.AffectedTargets.Add(T);
		}
	}
	else
	{
		FItemUseResult ItemResult = ItemExec->UseItem(User, Id, Target);

		Result.bSuccess = ItemResult.bSuccess;
		Result.ErrorMessage = ItemResult.ErrorMessage;
		Result.TotalDamageDealt = ItemResult.DamageDealt;
		Result.TotalHealingDone = ItemResult.HealingDone;
		Result.StatusEffectsApplied = ItemResult.BuffsApplied + ItemResult.DebuffsRemoved;
		Result.AffectedTargets.Add(Target);
	}

	// === Consume item use from loadout ===
	if (Result.bSuccess && Loadout && ItemSlotIndex >= 0)
	{
		Loadout->UseItem(ItemSlotIndex);
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Consumed 1 use from item slot %d"), ItemSlotIndex);
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s used item %s - delegated to ItemExecutor"),
		   *User->GetName(), *ItemIdentity::GetDisplayName(Id));

	return Result;
}

// ========================================
// EXECUTION - DEFEND
// ========================================

FActionResult UActionExecutor::ExecuteDefend(AActor *Defender)
{
	FActionResult Result;
	Result.Executor = Defender;
	Result.ActionType = EActionType::Defend;
	Result.EnergySpent = 0;

	if (!Defender)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid defender");
		return Result;
	}

	// Apply defense buff
	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (StatusManager)
	{
		FActiveSkillEffect DefendBuff = FActiveSkillEffect::CreateBuff(
			TEXT("Defending"),
			9999, // Special ID for defend
			ESkillEffectType::DefenseBuff,
			50.0f, // 50% defense boost
			1);	   // Lasts until next turn

		StatusManager->ApplyEffect(Defender, DefendBuff, Defender, TEXT("Defend"), -1);
		Result.StatusEffectsApplied = 1;
	}

	Result.AffectedTargets.Add(Defender);
	Result.bSuccess = true;

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s is defending"), *Defender->GetName());

	return Result;
}
// ========================================
// POST-CAST PROCESSING
// ========================================

void UActionExecutor::ProcessPostCastBySource(AActor *Caster, USpellData *Spell, ESpellSource Source, int32 InfusionLevel)
{
	if (!Caster || !Spell)
		return;

	// Phase 4c: cost-bearing source paths (Ring crystal wear) MOVED to
	// ApplyCommitCosts. This function now handles only post-success consumption
	// (Item) and forward-looking placeholders (Evolution).
	switch (Source)
	{
	case ESpellSource::Innate:
		// No post-cast action; HP cost (if any) was paid at commit.
		break;

	case ESpellSource::RingCrystal:
		// Wear was applied at commit (ApplyCommitCosts). Nothing to do here.
		break;

	case ESpellSource::Evolution:
		// TODO Phase 6: Evolution-specific post-cast effects (if any beyond commit-time backlash)
		break;

	case ESpellSource::Item:
		// TODO: Consume spell item from inventory
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Spell item used - consumption not yet implemented"));
		break;

	case ESpellSource::WeaponCrystal:
		// Wear was applied at commit (ApplyCommitCosts). Nothing to do here.
		break;
	}
}

// ========================================
// DAMAGE APPLICATION
// ========================================

FCombatHitResult UActionExecutor::ApplyHit(const FActionHitInput &Input)
{
	FCombatHitResult Result;
	Result.Target = Input.Target;

	if (!Input.Target)
	{
		return Result;
	}

	UE_LOG(LogTemp, Verbose,
		   TEXT("[ApplyHit] %s -> %s | ActionType=%s Element=%s Dmg=%d Buildup=%d Physical=%s Inf=L%d"),
		   Input.Attacker ? *Input.Attacker->GetName() : TEXT("null"),
		   *Input.Target->GetName(),
		   *UEnum::GetValueAsString(Input.ActionType),
		   *UEnum::GetValueAsString(Input.Element),
		   Input.BaseDamage,
		   Input.BaseStatusBuildup,
		   *UEnum::GetValueAsString(Input.PhysicalDamageType),
		   Input.InfusionLevel);

	UCharacterDataComponent *TargetComp = Input.Target->FindComponentByClass<UCharacterDataComponent>();

	// Damage path — routes through UDamageCalculator so all Phase 1/2/2b math
	// (EActionType-driven stat selection, ActionMods, element interaction,
	// flat defense, grid modifiers, crit, BD multipliers) runs unchanged.
	if (Input.BaseDamage > 0 && TargetComp)
	{
		if (UDamageCalculator *DamageCalc = GetDamageCalculator())
		{
			FDamageCalculationInput DmgInput;
			DmgInput.BaseDamage = Input.BaseDamage;
			DmgInput.ActionType = Input.ActionType;
			DmgInput.bOverrideStatScaling = Input.bOverrideStatScaling;
			DmgInput.Element = Input.Element;
			DmgInput.bCanCrit = Input.bCanCrit;
			DmgInput.bWasInfused = Input.InfusionLevel > 0;
			DmgInput.InfusionLevel = Input.InfusionLevel;
			DmgInput.ActionMods = Input.ActionMods;

			const FDamageCalculationResult CalcResult = DamageCalc->CalculateDamage(Input.Attacker, Input.Target, DmgInput);

			Result.bWasCritical = CalcResult.bWasCritical;

			// Pre-mitigation pass — AbsorbDamage / DamageReflect.
			// Runs after DamageCalculator (so element/crit/defense are already
			// resolved) but before HP is actually decremented. Mutates a local
			// FinalDamage copy; CalcResult itself stays const.
			int32 FinalDamage = CalcResult.FinalDamage;
			if (USkillEffectManager *SEM = GetSkillEffectManager())
			{
				// AbsorbDamage — convert a percent of incoming damage to target EP.
				if (SEM->HasEffectOfType(Input.Target, ESkillEffectType::AbsorbDamage))
				{
					const float AbsorbPct = SEM->GetTotalStatModifier(Input.Target, ESkillEffectType::AbsorbDamage) / 100.0f;
					const int32 Absorbed = FMath::RoundToInt(FinalDamage * AbsorbPct);
					FinalDamage -= Absorbed;
					if (Absorbed > 0)
					{
						TargetComp->ServerGainEnergy(Absorbed);
						UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s absorbed %d damage as EP"),
							   *Input.Target->GetName(), Absorbed);
					}
				}

				// DamageReflect — bounce a percent of incoming damage back to attacker
				// (any ActionType — generic reflect).
				if (Input.Attacker && SEM->HasEffectOfType(Input.Target, ESkillEffectType::DamageReflect))
				{
					const float ReflectPct = SEM->GetTotalStatModifier(Input.Target, ESkillEffectType::DamageReflect) / 100.0f;
					const int32 Reflected = FMath::RoundToInt(FinalDamage * ReflectPct);
					if (Reflected > 0)
					{
						if (UCharacterDataComponent *AC = Input.Attacker->FindComponentByClass<UCharacterDataComponent>())
						{
							AC->ServerTakeDamage(Reflected);
							UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s reflected %d damage back to %s"),
								   *Input.Target->GetName(), Reflected, *Input.Attacker->GetName());
						}
					}
				}

				// ReflectPhysicalDamage — only fires for Ability ActionType (attacks folded in, Cluster 3).
				if (Input.Attacker &&
					(Input.ActionType == EActionType::Ability) &&
					SEM->HasEffectOfType(Input.Target, ESkillEffectType::ReflectPhysicalDamage))
				{
					const float ReflectPct = SEM->GetTotalStatModifier(Input.Target, ESkillEffectType::ReflectPhysicalDamage) / 100.0f;
					const int32 Reflected = FMath::RoundToInt(FinalDamage * ReflectPct);
					if (Reflected > 0)
					{
						if (UCharacterDataComponent *AC = Input.Attacker->FindComponentByClass<UCharacterDataComponent>())
						{
							AC->ServerTakeDamage(Reflected);
							UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s reflected %d physical damage back to %s"),
								   *Input.Target->GetName(), Reflected, *Input.Attacker->GetName());
						}
					}
				}

				// ReflectSpellDamage — only fires for Spell ActionType.
				if (Input.Attacker &&
					Input.ActionType == EActionType::Spell &&
					SEM->HasEffectOfType(Input.Target, ESkillEffectType::ReflectSpellDamage))
				{
					const float ReflectPct = SEM->GetTotalStatModifier(Input.Target, ESkillEffectType::ReflectSpellDamage) / 100.0f;
					const int32 Reflected = FMath::RoundToInt(FinalDamage * ReflectPct);
					if (Reflected > 0)
					{
						if (UCharacterDataComponent *AC = Input.Attacker->FindComponentByClass<UCharacterDataComponent>())
						{
							AC->ServerTakeDamage(Reflected);
							UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] %s reflected %d spell damage back to %s"),
								   *Input.Target->GetName(), Reflected, *Input.Attacker->GetName());
						}
					}
				}

				FinalDamage = FMath::Max(0, FinalDamage);
			}

			const int32 HPBefore = TargetComp->CurrentHP;
			TargetComp->ServerTakeDamage(FinalDamage);
			Result.DamageDealt = HPBefore - TargetComp->CurrentHP;
		}
		else
		{
			// Fallback if DamageCalculator subsystem is unavailable — preserves
			// the existing ApplyDamage degradation path (deal min 1, no math).
			const int32 HPBefore = TargetComp->CurrentHP;
			TargetComp->ServerTakeDamage(FMath::Max(1, Input.BaseDamage));
			Result.DamageDealt = HPBefore - TargetComp->CurrentHP;
		}

		if (!TargetComp->bIsAlive)
		{
			Result.bTargetDied = true;
		}

		// OnDamageDealt fires per-hit. ProcessMultiHit→ApplyDamage did this today;
		// ApplyHit preserves the per-hit broadcast so floating-number widgets,
		// hit-flash VFX, and any other listener see one event per hit landed.
		OnDamageDealt.Broadcast(Input.Attacker, Input.Target, Result.DamageDealt, Result.bWasCritical);

		// DoubleHit — re-run ApplyHit once more at 50% damage when the attacker
		// has the effect. bIsDoubleHit guards against infinite recursion (second
		// invocation skips this block).
		if (!Input.bIsDoubleHit && Input.Attacker)
		{
			if (USkillEffectManager *SEM = GetSkillEffectManager())
			{
				if (SEM->HasEffectOfType(Input.Attacker, ESkillEffectType::DoubleHit))
				{
					FActionHitInput SecondHit = Input;
					SecondHit.BaseDamage = FMath::Max(1, FMath::RoundToInt(Input.BaseDamage * 0.5f));
					SecondHit.bIsDoubleHit = true;
					UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] DoubleHit on %s — second hit %d dmg"),
						   *Input.Attacker->GetName(), SecondHit.BaseDamage);
					ApplyHit(SecondHit);
				}
			}
		}
	}

	// Buildup path — manager resolves the trigger type internally from
	// (Element, PhysicalType) via BarCapTriggerResolver. Manager also runs the
	// attacker StatusMultiplier amplification (Spirit-driven via
	// CalculateStatusMultiplier) and the per-element resistance reduction.
	if (Input.BaseStatusBuildup > 0)
	{
		UStatusBuildupManager *BuildupManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStatusBuildupManager>() : nullptr;
		if (BuildupManager)
		{
			BuildupManager->AddStatusBuildup(
				Input.Attacker,
				Input.Target,
				static_cast<float>(Input.BaseStatusBuildup),
				Input.Element,
				Input.PhysicalDamageType);
		}
	}

	return Result;
}

FCombatHitResult UActionExecutor::ApplyDamage(
	AActor *Attacker,
	AActor *Target,
	int32 BaseDamage,
	ESpellElement Element,
	bool bCanCrit)
{
	FCombatHitResult Result;
	Result.Target = Target;

	if (!Target)
	{
		return Result;
	}

	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
	{
		return Result;
	}

	// Use centralized DamageCalculator
	UDamageCalculator *DamageCalc = GetDamageCalculator();
	if (DamageCalc)
	{
		FDamageCalculationInput Input;
		Input.BaseDamage = BaseDamage;
		Input.Element = Element;
		Input.bCanCrit = bCanCrit;
		// ActionMods + ActionType are the canonical per-action signal path
		// (Reality + Evolution + future buffs). Stashed on context at action start
		// so every ApplyDamage call site (defense resolution, beam ticks, projectile
		// impacts, support spells) reads the same snapshot.
		if (CurrentExecutionContext.IsSet())
		{
			Input.ActionMods = CurrentExecutionContext->ActionMods;
			Input.ActionType = CurrentExecutionContext->Action.ActionType;
		}

		FDamageCalculationResult CalcResult = DamageCalc->CalculateDamage(Attacker, Target, Input);

		Result.bWasCritical = CalcResult.bWasCritical;

		// Apply the calculated damage
		int32 HPBefore = TargetComp->CurrentHP;
		TargetComp->ServerTakeDamage(CalcResult.FinalDamage);
		Result.DamageDealt = HPBefore - TargetComp->CurrentHP;
	}
	else
	{
		// Fallback if DamageCalculator unavailable
		int32 HPBefore = TargetComp->CurrentHP;
		TargetComp->ServerTakeDamage(FMath::Max(1, BaseDamage));
		Result.DamageDealt = HPBefore - TargetComp->CurrentHP;
	}

	// Check for death
	if (!TargetComp->bIsAlive)
	{
		Result.bTargetDied = true;
	}

	// Broadcast damage event
	OnDamageDealt.Broadcast(Attacker, Target, Result.DamageDealt, Result.bWasCritical);

	UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] %s dealt %d damage to %s%s"),
		   Attacker ? *Attacker->GetName() : TEXT("Unknown"),
		   Result.DamageDealt,
		   *Target->GetName(),
		   Result.bWasCritical ? TEXT(" (CRIT)") : TEXT(""));

	return Result;
}

FCombatHitResult UActionExecutor::ApplyHealing(
	AActor *Healer,
	AActor *Target,
	int32 BaseHealing)
{
	FCombatHitResult Result;
	Result.Target = Target;

	if (!Target)
	{
		return Result;
	}

	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
	{
		return Result;
	}

	// Use centralized DamageCalculator for healing
	int32 FinalHealing = BaseHealing;
	UDamageCalculator *DamageCalc = GetDamageCalculator();
	if (DamageCalc)
	{
		FinalHealing = DamageCalc->CalculateHealing(Healer, Target, BaseHealing);
	}

	// Apply healing
	int32 HPBefore = TargetComp->CurrentHP;
	TargetComp->ServerHeal(FinalHealing);
	Result.HealingDone = TargetComp->CurrentHP - HPBefore;

	// Broadcast healing event
	OnHealingDone.Broadcast(Healer, Target, Result.HealingDone);

	UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] %s healed %s for %d"),
		   Healer ? *Healer->GetName() : TEXT("Unknown"),
		   *Target->GetName(),
		   Result.HealingDone);

	return Result;
}

// ========================================
// UTILITY
// ========================================

USkillEffectManager *UActionExecutor::GetSkillEffectManager() const
{
	if (!SkillEffectManagerRef)
	{
		if (UGameInstance *GI = Cast<UGameInstance>(GetGameInstance()))
		{
			const_cast<UActionExecutor *>(this)->SkillEffectManagerRef =
				GI->GetSubsystem<USkillEffectManager>();
		}
	}
	return SkillEffectManagerRef;
}

UDamageCalculator *UActionExecutor::GetDamageCalculator() const
{
	if (UGameInstance *GI = GetGameInstance())
	{
		return GI->GetSubsystem<UDamageCalculator>();
	}
	return nullptr;
}

bool UActionExecutor::IsTargetAlive(AActor *Target) const
{
	if (!Target)
		return false;

	UCharacterDataComponent *Comp = GetCharacterDataComponent(Target);
	return Comp && Comp->bIsAlive;
}

TArray<AActor *> UActionExecutor::FilterValidTargets(const TArray<AActor *> &Targets) const
{
	TArray<AActor *> ValidTargets;
	for (AActor *Target : Targets)
	{
		if (IsTargetAlive(Target))
		{
			ValidTargets.Add(Target);
		}
	}
	return ValidTargets;
}

// ========================================
// INTERNAL HELPERS
// ========================================

UCharacterDataComponent *UActionExecutor::GetCharacterDataComponent(AActor *Actor) const
{
	if (!Actor)
		return nullptr;
	return Actor->FindComponentByClass<UCharacterDataComponent>();
}

UCharacterData *UActionExecutor::GetCharacterData(AActor *Actor) const
{
	UCharacterDataComponent *Comp = GetCharacterDataComponent(Actor);
	return Comp ? Comp->CharacterData : nullptr;
}

int32 UActionExecutor::ProcessMultiHit(
	AActor *Attacker,
	AActor *Target,
	int32 DamagePerHit,
	int32 HitCount,
	ESpellElement Element,
	bool bCanCrit,
	FActionResult &OutResult)
{
	int32 TotalDamage = 0;

	for (int32 i = 0; i < HitCount; i++)
	{
		// Each hit can independently crit
		FCombatHitResult HitResult = ApplyDamage(
			Attacker, Target, DamagePerHit, Element, bCanCrit);

		TotalDamage += HitResult.DamageDealt;

		if (HitResult.bWasCritical)
		{
			OutResult.bWasCritical = true;
		}

		// Stop if target died
		if (HitResult.bTargetDied)
		{
			break;
		}
	}

	return TotalDamage;
}

bool UActionExecutor::SpendEnergy(AActor *Actor, int32 Amount)
{
	if (Amount <= 0)
		return true;

	UCharacterDataComponent *Comp = GetCharacterDataComponent(Actor);
	if (!Comp)
		return false;

	// CurrentEP is the unified spend pool — Broken Darkness (absorption) and
	// non-BD (regenerating EP) characters alike.
	if (Comp->CurrentEP < Amount)
		return false;

	Comp->ServerSpendEnergy(Amount);
	return true;
}

// ========================================
// ANIMATION/VFX STUBS
// ========================================

void UActionExecutor::PlaySpellAnimation(AActor *Caster, USpellData *Spell, float SpellSize, const FActionStatModifiers &ActionMods)
{
	if (!Caster || !Spell)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] PlaySpellAnimation - Invalid caster or spell"));
		return;
	}

	// D2 reader switch: SkillMontage is the unified field (PostLoad mirrored
	// CastAnimation into it, so playback is byte-identical).
	if (!Spell->SkillMontage)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] PlaySpellAnimation - No SkillMontage on %s"),
			   *Spell->Name);
		return;
	}

	// Play rate = BaseAnimSpeed × GetEffectiveSpellSpeed() (Pattern P, cluster 5g — stat ×1.5
	// clamped ALONE, gear/stone/transient multiply toward the ×2.0 ceiling) × per-action ActionMods.
	// BaseAnimSpeed (montage authoring) and ActionMods stay at the call site.
	// TODO(docs/Design/RealTimeDefenseRework.md): SpellSpeed's COMBAT effect (shrinking the
	// defender's reaction window) is still unwired — this speeds the cast animation only.
	float PlayRate = Spell->BaseAnimSpeed;
	if (UCharacterDataComponent *CasterComp = GetCharacterDataComponent(Caster))
	{
		PlayRate *= CasterComp->GetEffectiveSpellSpeed();
	}
	PlayRate = ActionMods.ApplyTo(PlayRate, ESubStat::SpellSpeed);
	PlayRate = FMath::Max(0.1f, PlayRate);

	BeginMontageChain(Caster, Spell, PlayRate);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Playing spell animation %s at %.2fx"),
		   *Spell->SkillMontage->GetName(), PlayRate);
}

void UActionExecutor::SpawnSpellVFX(AActor *Caster, USpellData *Spell, float SpellSize, const TArray<AActor *> &ExplicitTargets, int32 Damage)
{
	if (!Caster || !Spell)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] SpawnSpellVFX - Invalid caster or spell"));
		return;
	}

	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Caster);
	bool bIsBD = BDManager && BDManager->IsTransformed();

	float FinalImpactRadius = Spell->BaseSize * Spell->HitboxRatio * SpellSize;
	float FinalVisualScale = Spell->BaseSize * SpellSize;

	// Use explicit targets if provided, otherwise get from context
	TArray<AActor *> Targets;
	if (ExplicitTargets.Num() > 0)
	{
		Targets = ExplicitTargets;
	}
	else if (CurrentExecutionContext.IsSet())
	{
		for (const auto &Pair : CurrentExecutionContext->PendingDefenses)
		{
			if (Pair.Value.Target.IsValid())
			{
				Targets.Add(Pair.Value.Target.Get());
			}
		}
	}

	int32 FinalDamage = (Damage > 0) ? Damage : (CurrentExecutionContext.IsSet() ? CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense : 0);

	SpawnSpellDelivery(Caster, Targets, Spell, FinalImpactRadius, FinalVisualScale, FinalDamage, bIsBD);
}

void UActionExecutor::SpawnSpellDelivery(
	AActor *Caster,
	const TArray<AActor *> &Targets,
	USpellData *Spell,
	float FinalImpactRadius,
	float FinalVisualScale,
	int32 FinalDamage,
	bool bIsBrokenDarkness)
{
	if (!Spell)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] SpawnSpellDelivery - No spell data"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] SpawnSpellDelivery - Type=%d, Targets=%d, Radius=%.2f"),
		   (int32)Spell->DeliveryType, Targets.Num(), FinalImpactRadius);
	// Check if offensive or supportive spell
	bool bIsOffensive = (Spell->TargetType == ETargetType::SingleEnemy ||
						 Spell->TargetType == ETargetType::AllEnemies);

	if (!bIsOffensive)
	{
		// Self/Ally spells - spawn VFX, apply healing/buff directly (no defense)
		SpawnSupportSpellEffect(Caster, Targets, Spell, FinalVisualScale, bIsBrokenDarkness);
		return;
	}

	switch (Spell->DeliveryType)
	{
	case ESpellDeliveryType::Projectile:
		// Spawn projectile actor for each target
		for (AActor *Target : Targets)
		{
			SpawnProjectileActor(Caster, Target, Spell, FinalImpactRadius, FinalVisualScale, FinalDamage, bIsBrokenDarkness);
		}
		break;

	case ESpellDeliveryType::AOE:
		// Spawn VFX at each target, open defense window immediately
		for (AActor *Target : Targets)
		{
			SpawnAOEEffect(Caster, Target, Spell, FinalImpactRadius, FinalVisualScale, FinalDamage, bIsBrokenDarkness);
		}
		break;

	case ESpellDeliveryType::Instant:
		// No travel time, immediate resolution
		for (AActor *Target : Targets)
		{
			ResolveInstantSpell(Caster, Target, Spell, FinalImpactRadius, FinalDamage, bIsBrokenDarkness);
		}
		break;
	}
}

void UActionExecutor::SpawnSupportSpellEffect(
	AActor *Caster,
	const TArray<AActor *> &Targets,
	USpellData *Spell,
	float FinalVisualScale,
	bool bIsBrokenDarkness)
{
	for (AActor *Target : Targets)
	{
		// Spawn VFX at target
		if (Spell->SpellVFX)
		{
			FHybridSpellColorData Colors = UHybridSpellColors::GetInfusionColors(Spell->Element, bIsBrokenDarkness);

			UNiagaraComponent *NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				Spell->SpellVFX,
				Target->GetActorLocation(),
				FRotator::ZeroRotator,
				FVector(FinalVisualScale),
				true, true);

			if (NiagaraComp)
			{
				NiagaraComp->SetColorParameter(FName("CoreColor"), Colors.PrimaryColor);
			}
		}

		// Apply healing/buff directly - no defense window
		// Healing/buffs handled by existing ExecuteSpell logic
	}

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Support spell VFX spawned for %d targets"), Targets.Num());
}

void UActionExecutor::SpawnProjectileActor(
	AActor *Caster,
	AActor *Target,
	USpellData *Spell,
	float FinalImpactRadius,
	float FinalVisualScale,
	int32 FinalDamage,
	bool bIsBrokenDarkness,
	const FSkillCastEntry *Entry,
	int32 CastEntryIndex)
{
	if (!Caster || !Target || !Spell)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] SpawnProjectileActor - Invalid parameters"));
		return;
	}

	// Check for projectile class — the Cast entry's class wins when set (D6);
	// null entry class = executor default, as today.
	TSubclassOf<ASkillProjectile> ProjectileClass =
		(Entry && Entry->ProjectileClass) ? Entry->ProjectileClass : DefaultProjectileClass;
	if (!ProjectileClass)
	{
		// Fallback to base class if no BP assigned
		ProjectileClass = ASkillProjectile::StaticClass();
	}

	// Spawn projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Caster;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASkillProjectile *Projectile = GetWorld()->SpawnActor<ASkillProjectile>(
		ProjectileClass,
		Caster->GetActorLocation(),
		FRotator::ZeroRotator,
		SpawnParams);

	if (Projectile)
	{
		// 1. Assign VFX assets FIRST — D5/D6 reader switch: muzzle/impact from
		// the VFXArray's role entries (loose fields as fallback, SC5); trail
		// from the Cast entry (loose SpellVFX as fallback, SC3).
		const FSkillVFXEntry *MuzzleEntry = GetVFXEntryByRole(Spell, EVFXRole::Muzzle);
		const FSkillVFXEntry *ImpactEntry = GetVFXEntryByRole(Spell, EVFXRole::Impact);
		Projectile->SetVFXAssets(
			MuzzleEntry ? MuzzleEntry->VFX.LoadSynchronous() : Spell->MuzzleVFX,
			Entry ? Entry->Trail.LoadSynchronous() : Spell->SpellVFX,
			ImpactEntry ? ImpactEntry->VFX.LoadSynchronous() : Spell->ImpactVFX);

		// 2. Initialize with combat data (entry overload feeds the entry's
		// delivery/speed values; legacy feeds the loose fields)
		if (Entry)
		{
			Projectile->InitializeProjectile(
				*Entry, Spell, Caster, Target,
				FinalImpactRadius, FinalVisualScale, FinalDamage, CastEntryIndex);
		}
		else
		{
			Projectile->InitializeProjectile(
				Spell, Caster, Target,
				FinalImpactRadius, FinalVisualScale, FinalDamage);
		}

		// 3. Bind to events
		Projectile->OnSkillImpact.AddDynamic(this, &UActionExecutor::OnProjectileImpact);
		Projectile->OnSkillDodged.AddDynamic(this, &UActionExecutor::OnProjectileDodged);

		// 4. Launch (activates VFX and starts movement)
		Projectile->Launch();

		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Spawned projectile toward %s (Type=%d, Speed=%.1f)"),
			   *Target->GetName(), (int32)(Entry ? Entry->DeliveryType : Spell->DeliveryType),
			   Entry ? Entry->ProjectileSpeed : Spell->ProjectileSpeed);
	}
}

void UActionExecutor::SpawnAOEEffect(
	AActor *Caster,
	AActor *Target,
	USpellData *Spell,
	float FinalImpactRadius,
	float FinalVisualScale,
	int32 FinalDamage,
	bool bIsBrokenDarkness,
	const FSkillCastEntry *Entry,
	int32 CastEntryIndex)
{
	if (!Target || !Spell)
	{
		return;
	}

	FVector SpawnLocation = Target->GetActorLocation();

	// Get colors for VFX
	FHybridSpellColorData Colors = UHybridSpellColors::GetInfusionColors(Spell->Element, bIsBrokenDarkness);

	// Ground visual — entry path: the entry's Trail (doubles as the AOE
	// visual, as the loose SpellVFX does today); legacy: loose SpellVFX.
	UNiagaraSystem *GroundVFX = Entry ? Entry->Trail.LoadSynchronous() : Spell->SpellVFX;

	// Spawn VFX at target location
	if (GroundVFX)
	{
		UNiagaraComponent *NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			GroundVFX,
			SpawnLocation,
			FRotator::ZeroRotator,
			FVector(FinalVisualScale),
			true, // bAutoDestroy
			true, // bAutoActivate
			ENCPoolMethod::None,
			true // bPreCullCheck
		);

		// Apply element colors
		if (NiagaraComp)
		{
			NiagaraComp->SetColorParameter(FName("CoreColor"), Colors.PrimaryColor);
			NiagaraComp->SetColorParameter(FName("EdgeColor"), Colors.BlendedColor);
			NiagaraComp->SetColorParameter(FName("TrailColor"), Colors.SecondaryColor);
		}
	}

	// AOE resolution = NOW (the Cast notify). No travel, so this dispatch IS the impact moment.
	// Resolve THIS defender per-impact against the count-based window the gate opened at action start
	// (ExpectedImpacts=1): read the cast-entry defense difficulty, apply the single full-damage impact,
	// and close. Mirrors OnProjectileImpact's converted branch — the AOE "arrival" is here, now.
	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (DefenseSys && CurrentExecutionContext.IsSet())
	{
		if (FPendingDefenseContext *Ctx = CurrentExecutionContext->PendingDefenses.Find(Target); Ctx && Ctx->ExpectedImpacts > 0)
		{
			const double ImpactTime = FPlatformTime::Seconds(); // same clock as the projectile arrival / Hit notify

			// Difficulty from the spell cast-entry table (guarded): INDEX_NONE / out-of-range → Easy ×1.0.
			const FDefenseDifficultyTriple Diff =
				CurrentExecutionContext->ResolvedCastDifficulty.IsValidIndex(CastEntryIndex)
					? CurrentExecutionContext->ResolvedCastDifficulty[CastEntryIndex]
					: FDefenseDifficultyTriple();

			AActor *Attacker = Ctx->Attacker.Get();

			// One AOE impact per defender → ordinal 0. Drain BEFORE apply so the close-time tail
			// [ImpactsLanded, HitCount) in ApplyDamageAfterDefense is empty — no double-apply on close.
			++Ctx->ImpactsLanded;
			ResolveImpactDefense(Target, 0, ImpactTime, Diff);
			Ctx = CurrentExecutionContext->PendingDefenses.Find(Target); // re-find: ResolveImpactDefense mutated the entry
			if (Ctx)
			{
				StashHitFlags(*Ctx, CurrentExecutionContext->ResolvedCastFlags, CastEntryIndex);
				// Capture the choke outcome BEFORE ApplyOneImpact (which may remove the context on death).
				const bool bChokeEntry = CurrentExecutionContext->ResolvedCastFlags.IsValidIndex(CastEntryIndex) &&
										 CurrentExecutionContext->ResolvedCastFlags[CastEntryIndex].bInterruptable;
				const bool bDefended = Ctx->bResolvedSuccess &&
									   (Ctx->ResolvedDefenseType == EDefenseType::Parry ||
										Ctx->ResolvedDefenseType == EDefenseType::Dodge);
				ApplyOneImpact(Attacker, Target, *Ctx, 0, /*bDamageIsPerImpact=*/true);
				// Cross-target interrupt tally: record this target's outcome; abort once ALL targets have
				// resolved AND all parried/dodged this choke point (the surviving tally replaces the live-
				// context check the per-defender close below defeats).
				if (bChokeEntry && RecordChokeAndShouldAbort(Target, bDefended))
				{
					InterruptAsyncAction(Caster, Spell);
					return; // abort handles teardown — skip the normal close
				}
			}
			DefenseSys->CloseDefenseWindow(Target); // single-and-only arrival → close (empty tail, removes the entry)

			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] AOE resolved per-impact for %s (CastEntryIndex=%d)"),
				   *Target->GetName(), CastEntryIndex);
			return;
		}
	}

	// Fallback — no converted count-based window (unconverted multi-entry AOE keeps the action-start
	// lumped window, which resolves it via OnDefenseWindowClosed; lumped-retire is the next step), or no
	// DefenseSystem at all → apply directly.
	if (!DefenseSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] No DefenseSystem - applying AOE damage directly"));
		ApplyDamage(Caster, Target, FinalDamage, Spell->Element, false);
	}
}

void UActionExecutor::ResolveInstantSpell(
	AActor *Caster,
	AActor *Target,
	USpellData *Spell,
	float FinalImpactRadius,
	int32 FinalDamage,
	bool bIsBrokenDarkness,
	const FSkillCastEntry *Entry,
	int32 CastEntryIndex)
{
	if (!Target || !Spell)
	{
		return;
	}

	// Get colors for VFX
	FHybridSpellColorData Colors = UHybridSpellColors::GetInfusionColors(Spell->Element, bIsBrokenDarkness);

	// Visual — entry path: the entry's Trail; legacy: loose SpellVFX.
	UNiagaraSystem *InstantVFX = Entry ? Entry->Trail.LoadSynchronous() : Spell->SpellVFX;

	// Spawn VFX at target immediately
	if (InstantVFX)
	{
		UNiagaraComponent *NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			InstantVFX,
			Target->GetActorLocation(),
			FRotator::ZeroRotator,
			FVector(1.f),
			true,
			true);

		if (NiagaraComp)
		{
			NiagaraComp->SetColorParameter(FName("CoreColor"), Colors.PrimaryColor);
			NiagaraComp->SetColorParameter(FName("EdgeColor"), Colors.BlendedColor);
		}
	}

	// Instant resolution = NOW (the Cast notify). No travel, so this dispatch IS the impact moment.
	// Instant is now DEFENDABLE per-impact (all three defenses, Hard difficulty): resolve THIS defender
	// against the count-based window the gate opened at action start (ExpectedImpacts=1), read the
	// cast-entry defense difficulty, apply the single full-damage impact through the defense, and close.
	// Byte-identical to SpawnAOEEffect's converted branch — the Instant "arrival" is here, now.
	UDefenseSystem *DefenseSys = GetDefenseSystem();
	if (DefenseSys && CurrentExecutionContext.IsSet())
	{
		if (FPendingDefenseContext *Ctx = CurrentExecutionContext->PendingDefenses.Find(Target); Ctx && Ctx->ExpectedImpacts > 0)
		{
			const double ImpactTime = FPlatformTime::Seconds(); // same clock as the AOE / projectile resolve

			// Difficulty from the spell cast-entry table (guarded): INDEX_NONE / out-of-range → Easy ×1.0.
			const FDefenseDifficultyTriple Diff =
				CurrentExecutionContext->ResolvedCastDifficulty.IsValidIndex(CastEntryIndex)
					? CurrentExecutionContext->ResolvedCastDifficulty[CastEntryIndex]
					: FDefenseDifficultyTriple();

			AActor *Attacker = Ctx->Attacker.Get();

			// One Instant impact per defender → ordinal 0. Drain BEFORE apply so the close-time tail
			// [ImpactsLanded, HitCount) in ApplyDamageAfterDefense is empty — no double-apply on close.
			++Ctx->ImpactsLanded;
			ResolveImpactDefense(Target, 0, ImpactTime, Diff);
			Ctx = CurrentExecutionContext->PendingDefenses.Find(Target); // re-find: ResolveImpactDefense mutated the entry
			if (Ctx)
			{
				StashHitFlags(*Ctx, CurrentExecutionContext->ResolvedCastFlags, CastEntryIndex);
				// Capture the choke outcome BEFORE ApplyOneImpact (which may remove the context on death).
				const bool bChokeEntry = CurrentExecutionContext->ResolvedCastFlags.IsValidIndex(CastEntryIndex) &&
										 CurrentExecutionContext->ResolvedCastFlags[CastEntryIndex].bInterruptable;
				const bool bDefended = Ctx->bResolvedSuccess &&
									   (Ctx->ResolvedDefenseType == EDefenseType::Parry ||
										Ctx->ResolvedDefenseType == EDefenseType::Dodge);
				ApplyOneImpact(Attacker, Target, *Ctx, 0, /*bDamageIsPerImpact=*/true);
				// Cross-target interrupt tally: record this target's outcome; abort once ALL targets have
				// resolved AND all parried/dodged this choke point (the surviving tally replaces the live-
				// context check the per-defender close below defeats).
				if (bChokeEntry && RecordChokeAndShouldAbort(Target, bDefended))
				{
					InterruptAsyncAction(Caster, Spell);
					return; // abort handles teardown — skip the normal close
				}
			}
			DefenseSys->CloseDefenseWindow(Target); // single-and-only arrival → close (empty tail, removes the entry)

			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Instant resolved per-impact for %s (CastEntryIndex=%d)"),
				   *Target->GetName(), CastEntryIndex);
			return;
		}
	}

	// Fallback — no converted count-based window (unconverted multi-entry Instant keeps the action-start
	// lumped window, which resolves it via OnDefenseWindowClosed), or no DefenseSystem at all → apply
	// directly (the legacy unavoidable behavior).
	if (!DefenseSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] No DefenseSystem - applying Instant damage directly"));
		FCombatHitResult Result = ApplyDamage(Caster, Target, FinalDamage, Spell->Element, true);

		if (CurrentExecutionContext.IsSet())
		{
			CurrentExecutionContext->PartialResult.TotalDamageDealt += Result.DamageDealt;
			CurrentExecutionContext->PartialResult.DamagePerTarget.Add(Target, Result.DamageDealt);
			CurrentExecutionContext->PartialResult.AffectedTargets.Add(Target);

			if (Result.bWasCritical)
			{
				CurrentExecutionContext->PartialResult.bWasCritical = true;
			}

			if (Result.bTargetDied)
			{
				CurrentExecutionContext->PartialResult.bCausedDeath = true;
			}
		}
	}
}

// ========================================
// CAST-ENTRY DISPATCH (D6 Stage 12)
// ========================================

void UActionExecutor::DispatchSpellCast(
	AActor *Caster,
	USpellData *Spell,
	const FSkillCastEntry &Entry,
	float SpellSize,
	const TArray<AActor *> &ExplicitTargets,
	int32 Damage,
	int32 CastEntryIndex)
{
	if (!Caster || !Spell)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Runner] DispatchSpellCast - Invalid caster or spell"));
		return;
	}

	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Caster);
	const bool bIsBD = BDManager && BDManager->IsTransformed();

	// Entry-sourced sizing. Migrated entries carry Size = BaseSize×HitboxRatio
	// and VisualScale = BaseSize, so these reproduce the legacy
	// BaseSize×HitboxRatio×SpellSize / BaseSize×SpellSize numbers exactly.
	const float FinalImpactRadius = Entry.Size * SpellSize;
	const float FinalVisualScale = Entry.VisualScale * SpellSize;

	// Targets — same resolution as SpawnSpellVFX.
	TArray<AActor *> Targets;
	if (ExplicitTargets.Num() > 0)
	{
		Targets = ExplicitTargets;
	}
	else if (CurrentExecutionContext.IsSet())
	{
		for (const auto &Pair : CurrentExecutionContext->PendingDefenses)
		{
			if (Pair.Value.Target.IsValid())
			{
				Targets.Add(Pair.Value.Target.Get());
			}
		}
	}

	const int32 FinalDamage = (Damage > 0) ? Damage : (CurrentExecutionContext.IsSet() ? CurrentExecutionContext->PartialResult.BaseDamageBeforeDefense : 0);

	// Support spells keep the legacy effect path (no delivery, no defense).
	const bool bIsOffensive = (Spell->TargetType == ETargetType::SingleEnemy ||
							   Spell->TargetType == ETargetType::AllEnemies);
	if (!bIsOffensive)
	{
		SpawnSupportSpellEffect(Caster, Targets, Spell, FinalVisualScale, bIsBD);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Runner] DispatchCastEntry '%s' - Type=%d, Targets=%d, Radius=%.2f, Count=%d"),
		   *Entry.Label, (int32)Entry.DeliveryType, Targets.Num(), FinalImpactRadius, Entry.Count);

	// Hybrid B2 cross-target interrupt tally: a fresh all-targets round per interruptable cast entry (choke
	// point, first-pass-wins). The per-defender resolve records each outcome (surviving the close-removal)
	// and aborts when ALL targets parry/dodge. Burst (Count>1) is NOT a choke point — the by-target tally
	// can't express per-arrival; only single-impact deliveries record (gated at the resolve site).
	if (CurrentExecutionContext.IsSet() &&
		CurrentExecutionContext->ResolvedCastFlags.IsValidIndex(CastEntryIndex) &&
		CurrentExecutionContext->ResolvedCastFlags[CastEntryIndex].bInterruptable)
	{
		CurrentExecutionContext->ChokeOutcomes.Empty();
		CurrentExecutionContext->ChokeExpectedCount = Targets.Num();
		CurrentExecutionContext->ChokeCastEntryIndex = CastEntryIndex;
	}

	switch (Entry.DeliveryType)
	{
	case ESpellDeliveryType::Projectile:
		for (AActor *Target : Targets)
		{
			// First spawn immediate; Count>1 queues the remainder on the
			// burst chain (BurstInterval stagger, spike-validated).
			SpawnProjectileActor(Caster, Target, Spell, FinalImpactRadius, FinalVisualScale, FinalDamage, bIsBD, &Entry, CastEntryIndex);
			for (int32 i = 1; i < Entry.Count; ++i)
			{
				BurstSpawnQueue.Add(Target);
			}
		}
		if (BurstSpawnQueue.Num() > 0)
		{
			ActiveBurstEntry = Entry;
			ActiveBurstCastEntryIndex = CastEntryIndex;
			ActiveBurstSpell = Spell;
			ActiveBurstCaster = Caster;
			ActiveBurstImpactRadius = FinalImpactRadius;
			ActiveBurstVisualScale = FinalVisualScale;
			ActiveBurstDamage = FinalDamage;
			bActiveBurstIsBD = bIsBD;
			if (UWorld *World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					BurstTimerHandle, this, &UActionExecutor::SpawnNextBurstProjectile,
					FMath::Max(Entry.BurstInterval, 0.01f), true);
			}
		}
		break;

	case ESpellDeliveryType::AOE:
		for (AActor *Target : Targets)
		{
			SpawnAOEEffect(Caster, Target, Spell, FinalImpactRadius, FinalVisualScale, FinalDamage, bIsBD, &Entry, CastEntryIndex);
		}
		break;

	case ESpellDeliveryType::Instant:
		for (AActor *Target : Targets)
		{
			ResolveInstantSpell(Caster, Target, Spell, FinalImpactRadius, FinalDamage, bIsBD, &Entry, CastEntryIndex);
		}
		break;
	}
}

void UActionExecutor::SpawnNextBurstProjectile()
{
	if (BurstSpawnQueue.IsEmpty() || !ActiveBurstCaster.IsValid() || !ActiveBurstSpell)
	{
		if (UWorld *World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BurstTimerHandle);
		}
		BurstSpawnQueue.Empty();
		ActiveBurstSpell = nullptr;
		return;
	}

	const TWeakObjectPtr<AActor> NextTarget = BurstSpawnQueue[0];
	BurstSpawnQueue.RemoveAt(0);

	if (NextTarget.IsValid())
	{
		SpawnProjectileActor(ActiveBurstCaster.Get(), NextTarget.Get(), ActiveBurstSpell,
							 ActiveBurstImpactRadius, ActiveBurstVisualScale,
							 ActiveBurstDamage, bActiveBurstIsBD, &ActiveBurstEntry, ActiveBurstCastEntryIndex);
	}

	if (BurstSpawnQueue.IsEmpty())
	{
		if (UWorld *World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(BurstTimerHandle);
		}
		ActiveBurstSpell = nullptr;
	}
}

// ========================================
// PROJECTILE EVENT HANDLERS
// ========================================

void UActionExecutor::OnProjectileImpact(AActor *Target, FVector ImpactLocation, float ImpactRadius, int32 Damage, int32 CastEntryIndex)
{
	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Projectile impact on %s - Damage=%d, Radius=%.2f, CastEntryIndex=%d"),
		   Target ? *Target->GetName() : TEXT("None"), Damage, ImpactRadius, CastEntryIndex);

	if (!Target)
	{
		return;
	}

	UDefenseSystem *DefenseSys = GetDefenseSystem();

	// Stage 6 cluster 4 — CONVERTED single-projectile path: the dispatch re-opened this defender's
	// window count-based with ExpectedImpacts=1, so the projectile's LANDING is the impact moment.
	// Resolve THIS arrival per-impact (reading the spell cast-entry difficulty) and apply exactly
	// once — mirroring the melee Impact-notify loop (drain counter, resolve, apply slice, close).
	if (DefenseSys && CurrentExecutionContext.IsSet())
	{
		if (FPendingDefenseContext *Ctx = CurrentExecutionContext->PendingDefenses.Find(Target); Ctx && Ctx->ExpectedImpacts > 0)
		{
			const double ImpactTime = FPlatformTime::Seconds(); // same clock as the melee Hit notify

			// Difficulty from the spell cast-entry table (guarded): -1 / out-of-range → Easy ×1.0.
			const FDefenseDifficultyTriple Diff =
				CurrentExecutionContext->ResolvedCastDifficulty.IsValidIndex(CastEntryIndex)
					? CurrentExecutionContext->ResolvedCastDifficulty[CastEntryIndex]
					: FDefenseDifficultyTriple();

			AActor *Attacker = Ctx->Attacker.Get();

			// This arrival's ordinal = the running ImpactsLanded (read BEFORE the increment): 0,1,…,Count-1
			// in arrival order. Distinct ordinals are required — ApplyOneImpact uses ImpactIndex for the
			// bounds guard (< HitCount=Count), buildup-rides-ordinal-0 (status applies ONCE, on the first
			// arrival), and the even-split remainder. Single (Count==1) = ordinal 0, the cluster-4 case.
			const int32 ImpactOrdinal = Ctx->ImpactsLanded;

			// Drain BEFORE apply (melee order, ~line 5081): when ImpactsLanded reaches HitCount the
			// close-time tail [ImpactsLanded, HitCount) in ApplyDamageAfterDefense is EMPTY — no double-
			// apply when CloseDefenseWindow fires OnDefenseWindowClosed below.
			++Ctx->ImpactsLanded;
			// Capture the close decision NOW, while Ctx is valid — ApplyOneImpact can broadcast
			// OnTargetKilled and remove the entry. Close ONLY after the LAST of N staggered arrivals;
			// intermediate burst arrivals leave the count-based window open for the next projectile.
			const bool bLastArrival = (Ctx->ImpactsLanded >= Ctx->ExpectedImpacts);

			// Even-split: ResolveImpactDefense slices State.BaseDamage/HitCount = FinalDamage/Count for
			// this ordinal (+remainder for the first R). Per arrival reduces by its own matched defense.
			ResolveImpactDefense(Target, ImpactOrdinal, ImpactTime, Diff);
			Ctx = CurrentExecutionContext->PendingDefenses.Find(Target); // re-find: ResolveImpactDefense mutated the entry
			if (Ctx)
			{
				StashHitFlags(*Ctx, CurrentExecutionContext->ResolvedCastFlags, CastEntryIndex);
				// Capture the choke outcome BEFORE ApplyOneImpact (may remove the context on death). Only
				// single-impact projectiles (ExpectedImpacts==1) are choke points; burst (Count>1) deferred.
				const bool bChokeEntry = Ctx->ExpectedImpacts == 1 &&
										 CurrentExecutionContext->ResolvedCastFlags.IsValidIndex(CastEntryIndex) &&
										 CurrentExecutionContext->ResolvedCastFlags[CastEntryIndex].bInterruptable;
				const bool bDefended = Ctx->bResolvedSuccess &&
									   (Ctx->ResolvedDefenseType == EDefenseType::Parry ||
										Ctx->ResolvedDefenseType == EDefenseType::Dodge);
				ApplyOneImpact(Attacker, Target, *Ctx, ImpactOrdinal, /*bDamageIsPerImpact=*/true);
				// Cross-target interrupt tally (single-projectile multi-target): record + abort when ALL
				// targets parried/dodged this choke point. Earlier targets already closed normally; the abort
				// on the last prevents the remainder.
				if (bChokeEntry && RecordChokeAndShouldAbort(Target, bDefended))
				{
					InterruptAsyncAction(Attacker, GetCurrentSkillData());
					return; // abort handles teardown — skip the close below
				}
			}
			if (bLastArrival)
			{
				DefenseSys->CloseDefenseWindow(Target); // → OnDefenseWindowClosed: empty tail, removes the entry
			}
			return;
		}
	}

	// Unconverted (lumped) path — barrage/beam, or no pending context. Preserve the legacy orphan
	// behavior (the lumped cast-time window resolves these); cluster 6 retires the lumped path.
	if (DefenseSys)
	{
		DefenseSys->OpenDefenseWindow(nullptr, Target, ImpactRadius, Damage, 0.3f);
	}
	else
	{
		// Fallback: Apply damage directly
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] No DefenseSystem - applying projectile damage directly"));
		ApplyDamage(nullptr, Target, Damage, ESpellElement::Generic, true);
	}
}

void UActionExecutor::OnProjectileDodged(AActor *Target, FVector ImpactLocation)
{
	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Projectile dodged by %s at %s"),
		   Target ? *Target->GetName() : TEXT("None"), *ImpactLocation.ToString());

	// No damage applied - target successfully moved out of impact zone
	// Could broadcast event for UI feedback here

	// Update execution context if tracking
	if (CurrentExecutionContext.IsSet() && Target)
	{
		CurrentExecutionContext->PartialResult.AffectedTargets.Add(Target);
		CurrentExecutionContext->PartialResult.DamagePerTarget.Add(Target, 0); // 0 damage = dodged
	}

	// Stage 6 cluster 6 — CONVERTED path count integrity: a move-dodged projectile is an ARRIVAL that
	// bypasses OnProjectileImpact (and its ++ImpactsLanded), so without this a burst window would hang
	// waiting for a count that never comes. Count the dodged arrival as landed-but-zero (no damage —
	// already recorded above) so the close still reaches ExpectedImpacts. Inert today (move-dodge never
	// fires in fixed-position combat) but correct for robustness / future positional mechanics.
	if (UDefenseSystem *DefenseSys = GetDefenseSystem(); DefenseSys && CurrentExecutionContext.IsSet())
	{
		if (FPendingDefenseContext *Ctx = CurrentExecutionContext->PendingDefenses.Find(Target); Ctx && Ctx->ExpectedImpacts > 0)
		{
			const bool bLastArrival = (++Ctx->ImpactsLanded >= Ctx->ExpectedImpacts);
			if (bLastArrival)
			{
				DefenseSys->CloseDefenseWindow(Target);
			}
		}
	}
}

// Merged skill-animation play (Cluster 2): takes the unified base pointer. Reads SkillMontage /
// BaseAnimSpeed / Name (all on USkillDataBase) and BeginMontageChain (takes USkillDataBase*), so it
// serves attacks and abilities alike. (Log text still says "ability" — cosmetic, harmless.)
void UActionExecutor::PlaySkillAnimation(AActor *User, USkillDataBase *Ability, const FActionStatModifiers &ActionMods)
{
	if (!User || !Ability)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] PlayAbilityAnimation - Invalid user or ability"));
		return;
	}

	// D2 reader switch: SkillMontage is the unified field (PostLoad mirrored
	// ExecutionMontage into it, so playback is byte-identical).
	if (!Ability->SkillMontage)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] PlayAbilityAnimation - No SkillMontage on %s"),
			   *Ability->Name);
		return;
	}

	// Play rate = BaseAnimSpeed × GetEffectiveActionSpeed() (Pattern P, cluster 5g — stat ×1.5
	// clamped ALONE, gear/stone/transient multiply toward the ×2.0 ceiling) × per-action ActionMods.
	// BaseAnimSpeed (montage authoring) and ActionMods stay at the call site.
	// TODO(docs/Design/RealTimeDefenseRework.md): ActionSpeed's COMBAT effect (shrinking the
	// defender's reaction window) is still unwired — this speeds the ability animation only.
	float PlayRate = Ability->BaseAnimSpeed;
	if (UCharacterDataComponent *UserComp = GetCharacterDataComponent(User))
	{
		PlayRate *= UserComp->GetEffectiveActionSpeed();
	}
	PlayRate = ActionMods.ApplyTo(PlayRate, ESubStat::ActionSpeed);
	PlayRate = FMath::Max(0.1f, PlayRate);

	BeginMontageChain(User, Ability, PlayRate);

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Playing ability animation %s for %s at %.2fx"),
		   *Ability->SkillMontage->GetName(), *Ability->Name, PlayRate);
}

// ========================================
// DEBUG
// ========================================

void UActionExecutor::DebugPrintActionResult(const FActionResult &Result) const
{
	UE_LOG(LogTemp, Display, TEXT("=== ACTION RESULT ==="));
	UE_LOG(LogTemp, Display, TEXT("Success: %s"), Result.bSuccess ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("Executor: %s"), Result.Executor ? *Result.Executor->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Display, TEXT("Energy Spent: %d"), Result.EnergySpent);
	UE_LOG(LogTemp, Display, TEXT("Total Damage: %d"), Result.TotalDamageDealt);
	UE_LOG(LogTemp, Display, TEXT("Total Healing: %d"), Result.TotalHealingDone);
	UE_LOG(LogTemp, Display, TEXT("Critical: %s"), Result.bWasCritical ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("Caused Death: %s"), Result.bCausedDeath ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogTemp, Display, TEXT("Status Effects: %d"), Result.StatusEffectsApplied);
	UE_LOG(LogTemp, Display, TEXT("Targets: %d"), Result.AffectedTargets.Num());

	if (!Result.ErrorMessage.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("Error: %s"), *Result.ErrorMessage);
	}

	UE_LOG(LogTemp, Display, TEXT("====================="));
}

// ========================================
// SUBSYSTEM GETTERS
// ========================================

UItemExecutor *UActionExecutor::GetItemExecutor() const
{
	if (!ItemExecutorRef)
	{
		if (UGameInstance *GI = Cast<UGameInstance>(GetGameInstance()))
		{
			const_cast<UActionExecutor *>(this)->ItemExecutorRef =
				GI->GetSubsystem<UItemExecutor>();
		}
	}
	return ItemExecutorRef;
}

UWeaponManager *UActionExecutor::GetWeaponManager() const
{
	if (!WeaponManagerRef)
	{
		if (UGameInstance *GI = Cast<UGameInstance>(GetGameInstance()))
		{
			const_cast<UActionExecutor *>(this)->WeaponManagerRef =
				GI->GetSubsystem<UWeaponManager>();
		}
	}
	return WeaponManagerRef;
}

TArray<EInfusionSourceOption> UActionExecutor::GetAvailableInfusionSources(AActor *Actor) const
{
	TArray<EInfusionSourceOption> Sources;
	Sources.Add(EInfusionSourceOption::None); // Always available
	Sources.Add(EInfusionSourceOption::Raw);  // Always available — HP-cost elementless infusion

	UCharacterData *Data = GetCharacterData(Actor);
	if (!Data)
	{
		return Sources;
	}

	// Innate (Caster only)
	if (Data->IsCaster())
	{
		Sources.Add(EInfusionSourceOption::Innate);
	}

	// Active Ring (Resonator only)
	if (Data->IsResonator())
	{
		URingManager *RM = GetRingManager();
		if (RM && RM->GetActiveRing(Actor) != nullptr)
		{
			Sources.Add(EInfusionSourceOption::ActiveRing);
		}
	}

	// Primary Ring (Generic/Caster with ring in primary slot)
	URingManager *RM = GetRingManager();
	if (RM && RM->GetPrimaryRing(Actor))
	{
		Sources.Add(EInfusionSourceOption::PrimaryRing);
	}

	// Weapon Crystal — read runtime entry, not data asset.
	// Aligns with FCombatCapabilities::BuildFrom path; the data-asset
	// AttachedItem field is the design-time default that diverges from runtime
	// when crystals are slotted/changed at runtime.
	ULoadoutComponent *LC = GetLoadoutComponent(Actor);
	if (LC)
	{
		const FWeaponLoadoutEntry *ActiveWeapon = LC->GetActiveWeaponLoadout();
		if (ActiveWeapon && ActiveWeapon->WeaponEntry.AttachedItem.CanProvideSpells())
		{
			Sources.Add(EInfusionSourceOption::WeaponCrystal);
		}
	}

	// Evolution (any class, if evolved)
	ULoadoutComponent *Loadout = GetLoadoutComponent(Actor);
	if (Loadout && Loadout->IsEvolved())
	{
		Sources.Add(EInfusionSourceOption::Evolution);
	}

	return Sources;
}

TArray<EInfusionSourceOption> UActionExecutor::GetAllowedInfusionSourcesForSpell(AActor *Actor, const USpellData *Spell) const
{
	TArray<EInfusionSourceOption> Allowed;
	if (!Spell)
	{
		return Allowed;
	}

	ULoadoutComponent *LC = GetLoadoutComponent(Actor);
	if (!LC)
	{
		return Allowed;
	}

	// Spells are 1:1 origin-bound — a spell's allowed infusion source IS its origin.
	// ResolveSpellSource is a const, read-only resolver; the non-const param is legacy.
	const ESpellSource Origin = LC->ResolveSpellSource(const_cast<USpellData *>(Spell));

	switch (Origin)
	{
	case ESpellSource::Innate:
		Allowed.Add(EInfusionSourceOption::Innate);
		break;

	case ESpellSource::WeaponCrystal:
		Allowed.Add(EInfusionSourceOption::WeaponCrystal);
		break;

	case ESpellSource::RingCrystal:
	{
		// Resonators hold rings in the dedicated ring loadout (ActiveRing) and can't slot rings in
		// the primary slot; Generic/Caster hold their ring in the primary slot (PrimaryRing).
		// ResolveSpellSource collapses both ring paths to RingCrystal; the class splits them here,
		// mirroring GetAvailableInfusionSources.
		const UCharacterData *Data = GetCharacterData(Actor);
		Allowed.Add((Data && Data->IsResonator())
						? EInfusionSourceOption::ActiveRing
						: EInfusionSourceOption::PrimaryRing);
		break;
	}

	case ESpellSource::Evolution:
	{
		Allowed.Add(EInfusionSourceOption::Evolution);
		// Exception: BD / Reality casters may ALSO route an evolution spell through Innate.
		const UCharacterDataComponent *Comp = GetCharacterDataComponent(Actor);
		const UCharacterData *Data = GetCharacterData(Actor);
		const bool bReality = Data && Data->InnateElement == ESpellElement::Reality;
		const bool bBD = Comp && Comp->IsBrokenDarkness();
		if (bReality || bBD)
		{
			Allowed.Add(EInfusionSourceOption::Innate);
		}
		break;
	}

	case ESpellSource::Item:
	default:
		// Item spells (and any unresolved origin) are consumables, not infusable -> no source.
		break;
	}

	return Allowed;
}

ESpellElement UActionExecutor::GetElementForSourceOption(AActor *Actor, EInfusionSourceOption Option) const
{
	UCharacterData *Data = GetCharacterData(Actor);
	if (!Data)
	{
		return ESpellElement::Generic;
	}

	switch (Option)
	{
	case EInfusionSourceOption::None:
		return ESpellElement::Generic;

	case EInfusionSourceOption::Raw:
		// Raw is elementless infusion — pays HP, no channeled element
		return ESpellElement::Generic;

	case EInfusionSourceOption::Innate:
		return Data->InnateElement;

	case EInfusionSourceOption::ActiveRing:
	{
		URingManager *RM = GetRingManager();
		return RM ? RM->GetActiveElement(Actor) : ESpellElement::Generic;
	}

	case EInfusionSourceOption::PrimaryRing:
	{
		ULoadoutComponent *LC = GetLoadoutComponent(Actor);
		if (LC)
		{
			if (const FRingLoadoutEntry *Entry = LC->GetPrimaryRingLoadout())
			{
				return Entry->RingEntry.AttachedItem.GetElement();
			}
		}
		return ESpellElement::Generic;
	}

	case EInfusionSourceOption::WeaponCrystal:
	{
		ULoadoutComponent *LC = GetLoadoutComponent(Actor);
		if (LC)
		{
			if (const FWeaponLoadoutEntry *Entry = LC->GetActiveWeaponLoadout())
			{
				return Entry->WeaponEntry.AttachedItem.GetElement();
			}
		}
		return ESpellElement::Generic;
	}

	case EInfusionSourceOption::Evolution:
	{
		ULoadoutComponent *Loadout = GetLoadoutComponent(Actor);
		if (Loadout && Loadout->IsEvolved())
		{
			FCombatLoadout ActiveLoadout = Loadout->GetActiveLoadout();
			if (ActiveLoadout.PrimaryEvolution.Item)
			{
				return ActiveLoadout.PrimaryEvolution.Item->GetAssociatedElement();
			}
		}
		return ESpellElement::Generic;
	}

	default:
		return ESpellElement::Generic;
	}
}

bool UActionExecutor::DoWeaponStatsApply(EInfusionSourceOption Option) const
{
	return Option == EInfusionSourceOption::None || Option == EInfusionSourceOption::Raw;
}

float UActionExecutor::GetEffectiveEnergyCostEfficiencyMultiplier(AActor *Actor) const
{
	if (!Actor)
	{
		return 1.0f;
	}

	UCharacterData *CharData = GetCharacterData(Actor);
	if (!CharData)
	{
		return 1.0f;
	}

	// Efficiency multiplier — the unified getter (innate crystal-aware Mind + equipment
	// BonusEfficiency + attached EfficiencyStone, one clamp); the SAME source BD drain
	// and durability use, so all three Efficiency consumers share one shape. The raw
	// asset-formula fallback below is defensive and UNREACHABLE in practice
	// (GetCharacterData above already required the component); innate-only, NOT a
	// live alternate read path — kept purely as a guard.
	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	const float CharMult = CharComp
							   ? CharComp->GetEffectiveEfficiencyMultiplier()
							   : CharData->CalculateEfficiencyMultiplier();

	// Skill-effect-driven cost modifiers: SpellCostBuff reduces cost,
	// SpellCostDebuff increases it, ModifyEnergyCost contributes per the
	// authored sign convention (positive = cheaper, mirroring SpellCostBuff).
	// Clamped to [0.1x, 2.0x] so an actor can never get free spells or be
	// taxed beyond 2x base cost from skill-effect stacking.
	float SkillEffectMult = 1.0f;
	if (USkillEffectManager *SEM = GetSkillEffectManager())
	{
		const float CostBuff = SEM->GetTotalStatModifier(Actor, ESkillEffectType::SpellCostBuff);
		const float CostDebuff = SEM->GetTotalStatModifier(Actor, ESkillEffectType::SpellCostDebuff);
		const float ModifyEnergy = SEM->GetTotalStatModifier(Actor, ESkillEffectType::ModifyEnergyCost);
		SkillEffectMult = FMath::Clamp(
			1.0f - (CostBuff - CostDebuff + ModifyEnergy) / 100.0f,
			0.1f, 2.0f);
	}

	return CharMult * SkillEffectMult;
}

FRuntimeAttachedItem UActionExecutor::ResolveInfusionAttachment(AActor *Actor, const FAction &Action) const
{
	if (!Actor)
	{
		return FRuntimeAttachedItem();
	}

	switch (Action.SelectedSource)
	{
	case EInfusionSourceOption::None:
	case EInfusionSourceOption::Raw:
	case EInfusionSourceOption::Innate:
		// Not crystal-sourced — Raw/Innate pay HP, None means no infusion.
		return FRuntimeAttachedItem();

	case EInfusionSourceOption::ActiveRing:
	{
		ULoadoutComponent *LC = GetLoadoutComponent(Actor);
		if (!LC)
		{
			return FRuntimeAttachedItem();
		}
		const FRingLoadoutEntry *Entry = LC->GetActiveRingLoadout();
		return Entry ? Entry->RingEntry.AttachedItem : FRuntimeAttachedItem();
	}

	case EInfusionSourceOption::PrimaryRing:
	{
		ULoadoutComponent *LC = GetLoadoutComponent(Actor);
		if (!LC)
		{
			return FRuntimeAttachedItem();
		}
		const FRingLoadoutEntry *Entry = LC->GetPrimaryRingLoadout();
		return Entry ? Entry->RingEntry.AttachedItem : FRuntimeAttachedItem();
	}

	case EInfusionSourceOption::WeaponCrystal:
	{
		ULoadoutComponent *LC = GetLoadoutComponent(Actor);
		if (!LC)
		{
			return FRuntimeAttachedItem();
		}
		const FWeaponLoadoutEntry *Entry = LC->GetActiveWeaponLoadout();
		return Entry ? Entry->WeaponEntry.AttachedItem : FRuntimeAttachedItem();
	}

	case EInfusionSourceOption::Evolution:
	{
		ULoadoutComponent *LC = GetLoadoutComponent(Actor);
		if (!LC)
		{
			return FRuntimeAttachedItem();
		}
		const FCombatLoadout Loadout = LC->GetActiveLoadout();
		// Evolution primary slot is its own thing — wrap it as an evolution attachment.
		// Whole-struct copy: the per-instance rolled state (GeneratedStatBonus/
		// GeneratedResistance + pools) must travel with the attachment so the
		// infusion read at the call site sees the roll — a field-by-field copy
		// here previously dropped Generated*, silencing rolled ints for the
		// standalone slot (the only slot path that can carry a roll today).
		FRuntimeAttachedItem Result;
		if (Loadout.PrimaryEvolution.Item)
		{
			Result.Kind = EAttachedItemKind::Evolution;
			Result.Evolution = Loadout.PrimaryEvolution;
		}
		return Result;
	}

	default:
		return FRuntimeAttachedItem();
	}
}

// ============================================================
//  - Get BrokenDarknessManager
// ============================================================

UBrokenDarknessManager *UActionExecutor::GetBrokenDarknessManager(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	return Actor->FindComponentByClass<UBrokenDarknessManager>();
}

// ============================================================
// Check and Roll for Break
// ============================================================
void UActionExecutor::CheckBrokenDarknessBreak(AActor *Actor, const FAction &Action, UCharacterData *CharData)
{
	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Actor);
	if (!BDManager)
	{
		return; // Not a potential BD character
	}

	// Already transformed - no more break checks needed
	if (BDManager->IsTransformed())
	{
		return;
	}

	// Darkness gate: only innate-Darkness characters can break into Broken
	// Darkness. Non-Darkness Casters (and missing CharData) never roll.
	if (!CharData || CharData->InnateElement != ESpellElement::Darkness)
	{
		return;
	}

	// Determine tier and infused state from the action's source data.
	// Spells: use SpellData->Tier. Abilities: use AbilityData->Tier.
	// Other action types (Attack, Defend, Item, etc.) don't trigger break.
	EItemTier Tier = EItemTier::F_Tier;
	int32 InfusionLevel = 0;
	FString TriggerReason;

	if (Action.ActionType == EActionType::Spell && Action.SpellData)
	{
		Tier = Action.SpellData->Tier;
		InfusionLevel = Action.SpellInfusionLevel;
		const bool bInfused = (InfusionLevel >= 1);
		const bool bOverReq =
			UBrokenDarknessManager::DoesSpellExceedRequirements(Action.SpellData, CharData);

		// Spell rolls when over-requirement OR infused at L1/L2. The infusion
		// multiplier is applied inside RollForBreak via InfusionLevel.
		if (!bOverReq && !bInfused)
		{
			return; // Spell within stats and not infused — no roll
		}

		if (bOverReq && bInfused)
		{
			TriggerReason = FString::Printf(TEXT("Requirement deficit + L%d Infusion"), InfusionLevel);
		}
		else if (bOverReq)
		{
			TriggerReason = TEXT("Requirement deficit");
		}
		else
		{
			TriggerReason = FString::Printf(TEXT("L%d Infusion"), InfusionLevel);
		}
	}
	// Attacks fold into Ability (attack/ability merge) but must NOT trigger the break roll — the rule
	// above is "other action types don't trigger break". Gate them out with !IsAttack() so a basic
	// attack falls to the else (no roll), exactly as it did when it was EActionType::Attack.
	else if (Action.ActionType == EActionType::Ability && Action.SkillData && !Action.SkillData->IsAttack())
	{
		Tier = Action.SkillData->Tier;
		InfusionLevel = Action.AbilityInfusionLevel;

		// Ability rolls ONLY when all three hold: the ability is infused, the
		// infusion source resolves to the character's innate (Darkness) element,
		// and the ability exceeds the character's stat requirements.
		if (Action.SelectedSource == EInfusionSourceOption::None)
		{
			return;
		}

		const ESpellElement SourceElement =
			GetElementForSourceOption(Actor, Action.SelectedSource);
		if (SourceElement != CharData->InnateElement)
		{
			return;
		}

		if (!UBrokenDarknessManager::DoesAbilityExceedRequirements(Cast<UAbilityData>(Action.SkillData), CharData))
		{
			return;
		}

		TriggerReason = FString::Printf(
			TEXT("Innate Darkness infused ability over requirements (L%d)"), InfusionLevel);
	}
	else
	{
		// Other action types don't trigger break
		return;
	}

	// One roll per cast with the correct tier and infusion level.
	BDManager->RollForBreak(Tier, InfusionLevel, TriggerReason);
}

// ============================================================
// Process Forbidden Element Cast
// ============================================================
void UActionExecutor::ProcessForbiddenElementCast(AActor *Actor, ESpellElement Element, float BaseDamage)
{
	if (!Actor)
		return;

	// Use IsBrokenDarkness() helper to cover both runtime-transformed
	// and character-created BD characters. Forbidden-element self-damage
	// applies whenever the character is behaviourally BD, regardless of
	// how they got there.
	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp || !CharComp->IsBrokenDarkness())
	{
		return;
	}

	UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Actor);
	if (!BDManager)
	{
		return; // Defensive — character-created BDs should have a manager but verify
	}

	// ProcessForbiddenCast checks if element is forbidden internally
	BDManager->ProcessForbiddenCast(Element, BaseDamage);
}

// ========================================
// RING MANAGER GETTER
// ========================================

URingManager *UActionExecutor::GetRingManager() const
{
	if (!RingManagerRef)
	{
		if (UGameInstance *GI = Cast<UGameInstance>(GetGameInstance()))
		{
			const_cast<UActionExecutor *>(this)->RingManagerRef =
				GI->GetSubsystem<URingManager>();
		}
	}
	return RingManagerRef;
}

// ========================================
// CHARGE INFUSION HELPERS
// ========================================

// Shared upside-only stat surcharge fraction: max(0, stat - 1). Stacking the stat raises
// the infusion COST (6-2 ComputeInfusionCostMultiplier) and the EFFECT (6-3 charge getters)
// by the IDENTICAL fraction — one definition so cost and effect never drift.
static float StatFraction(float Stat)
{
	return FMath::Max(0.0f, Stat - 1.0f);
}

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
	float Base;
	switch (Mode)
	{
	case EInfusionMode::Physical:
		Base = ChargeBand(Level, InfusionConstants::CHARGE_FOCUS_L1, InfusionConstants::CHARGE_FOCUS_L2);
		break;
	case EInfusionMode::Status:
		Base = ChargeBand(Level, InfusionConstants::CHARGE_OFFFOCUS_L1, InfusionConstants::CHARGE_OFFFOCUS_L2);
		break;
	case EInfusionMode::Balanced:
	default:
		Base = ChargeBand(Level, InfusionConstants::CHARGE_BALANCED_L1, InfusionConstants::CHARGE_BALANCED_L2);
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
	float Base;
	switch (Mode)
	{
	case EInfusionMode::Physical:
		Base = ChargeBand(Level, InfusionConstants::CHARGE_OFFFOCUS_L1, InfusionConstants::CHARGE_OFFFOCUS_L2);
		break;
	case EInfusionMode::Status:
		Base = ChargeBand(Level, InfusionConstants::CHARGE_FOCUS_L1, InfusionConstants::CHARGE_FOCUS_L2);
		break;
	case EInfusionMode::Balanced:
	default:
		Base = ChargeBand(Level, InfusionConstants::CHARGE_BALANCED_L1, InfusionConstants::CHARGE_BALANCED_L2);
		break;
	}

	return Base * (1.0f + StatFraction(Comp->GetEffectiveStatusMultiplier()));
}

EInfusionMode UActionExecutor::ResolveInfusionMode(EInfusionSourceOption Source, AActor *Actor) const
{
	switch (Source)
	{
	case EInfusionSourceOption::ActiveRing:
		if (URingManager *RM = GetRingManager())
		{
			if (URingData *Ring = RM->GetActiveRing(Actor))
			{
				return Ring->InfusionMode;
			}
		}
		break;
	case EInfusionSourceOption::PrimaryRing:
		if (URingManager *RM = GetRingManager())
		{
			if (URingData *Ring = RM->GetPrimaryRing(Actor))
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
		if (UWeaponManager *WM = GetWeaponManager())
		{
			if (UWeaponData *Weapon = WM->GetActiveWeapon(Actor))
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
		return InfusionConstants::L1_ENERGY_MULT; // 1.15x
	case 2:
		return InfusionConstants::L2_ENERGY_MULT; // 1.30x
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

// ========================================
// DEBUG
// ========================================

ULoadoutComponent *UActionExecutor::GetLoadoutComponent(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}
	return Actor->FindComponentByClass<ULoadoutComponent>();
}

bool UActionExecutor::CanUseAbility(AActor *Actor, UAbilityData *Ability) const
{
	if (!Actor || !Ability)
	{
		return false;
	}

	// Check LoadoutComponent first (new system)
	ULoadoutComponent *Loadout = GetLoadoutComponent(Actor);
	if (Loadout && Loadout->IsReadyForBattle())
	{
		TArray<UAbilityData *> Available = Loadout->GetAvailableAbilities();
		return Available.Contains(Ability);
	}
	return false;
}

bool UActionExecutor::CanUseSpell(AActor *Actor, USpellData *Spell) const
{
	if (!Actor || !Spell)
	{
		return false;
	}

	// Check LoadoutComponent first (new system)
	ULoadoutComponent *Loadout = GetLoadoutComponent(Actor);
	if (Loadout && Loadout->IsReadyForBattle())
	{
		TArray<USpellData *> Available = Loadout->GetAvailableSpells();
		return Available.Contains(Spell);
	}

	return false;
}

// ==================== MOVEMENT INTEGRATION ====================

float UActionExecutor::GetExecutionRange(const FAction &Action) const
{
	switch (Action.ActionType)
	{
	case EActionType::Ability:
		// Merged (Cluster 2): melee skills warp to ExecutionRange; ranged don't approach. Reads the
		// unified pointer; the gate is on ExecutionType (on USkillDataBase — IsMelee() is ability-only).
		// The old Attack path's unconditional read + 100.0f null-fallback is dropped: attacks default
		// ExecutionType=Melee (so melee attacks are unchanged) and always resolve a skill by dispatch.
		if (USkillDataBase *Skill = ResolveActionSkill(Action))
		{
			if (Skill->ExecutionType == EAbilityExecutionType::Melee)
			{
				return Skill->ExecutionRange;
			}
		}
		return 0.0f;

	case EActionType::Spell:
		// Melee spells warp like melee abilities; ranged spells don't approach.
		// (USpellData has no IsMelee() helper — gate on ExecutionType directly; the
		//  field lives on USkillDataBase, which spells inherit.)
		if (Action.SpellData && Action.SpellData->ExecutionType == EAbilityExecutionType::Melee)
		{
			return Action.SpellData->ExecutionRange;
		}
		return 0.0f; // ranged spells don't approach

	default:
		return 0.0f;
	}
}

void UActionExecutor::BeginSkillExecution(AActor *Actor)
{
	if (!CurrentExecutionContext.IsSet() || !CurrentExecutionContext->bInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] BeginSkillExecution called but no active context"));
		return;
	}

	UCharacterData *CharData = PendingExecutionCharData;
	const FAction &Action = CurrentExecutionContext->Action;

	if (!Actor || !CharData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] BeginSkillExecution - missing actor or char data"));
		CancelAsyncAction();
		return;
	}

	// Execution-start facing — ONE rule: nearest living enemy (same-column
	// first, else closest alive; arena center when none). Movement-independent.
	if (UCombatGridSubsystem *Grid = GetWorld()->GetGameInstance()->GetSubsystem<UCombatGridSubsystem>())
	{
		Grid->UpdateActorFacing(Actor, CachedArenaCenter);
	}

	// Warp approach target (W1, additive — the spike's warp productionized).
	// One named target ("Warp") covers the whole montage chain; only montages
	// authored with a Motion Warping window consume it — in-place content
	// ignores it entirely. Striking position reuses the legacy approach
	// formula: ExecutionRange short of the target, on the caster's Z plane.
	{
		AActor *PrimaryTarget = nullptr;
		for (AActor *Candidate : Action.Targets)
		{
			if (IsValid(Candidate))
			{
				PrimaryTarget = Candidate;
				break;
			}
		}

		if (PrimaryTarget)
		{
			if (UMotionWarpingComponent *Warp = GetOrCreateWarpComponent(Actor))
			{
				const FVector CasterLoc = Actor->GetActorLocation();
				const FVector TargetLoc = PrimaryTarget->GetActorLocation();
				FVector DirToTarget = TargetLoc - CasterLoc;
				DirToTarget.Z = 0;
				DirToTarget.Normalize();

				const float StrikeRange = GetExecutionRange(Action);
				FVector WarpLoc = TargetLoc - (DirToTarget * StrikeRange);
				WarpLoc.Z = CasterLoc.Z;
				const FRotator WarpRot = DirToTarget.IsNearlyZero()
											 ? Actor->GetActorRotation()
											 : DirToTarget.Rotation();

				Warp->AddOrUpdateWarpTargetFromLocationAndRotation(
					CombatConstants::WARP_TARGET_NAME, WarpLoc, WarpRot);
				UE_LOG(LogTemp, Log, TEXT("[Warp] approach target %s (range %.0f)"),
					   *WarpLoc.ToCompactString(), StrikeRange);
			}
		}
	}

	// Bind to action animation end BEFORE executing (so we catch the animation)
	BindActionAnimationEnd(Actor);

	// Runner notify spine (Stage 12): UCombatNotify Family/Index for all three
	// paths. Additive — montages without UCombatNotify instances fire nothing.
	BindCombatNotify(Actor);

	// Execute the action (plays animation)
	switch (Action.ActionType)
	{
	case EActionType::Spell:
		ExecuteSpellAsync(Actor, Action, CharData);
		break;
	case EActionType::Ability:
		// Merged dispatch: attacks fold into Ability (Cluster 3) and route to ExecuteSkillAsync, which
		// reads the unified skill pointer (ResolveActionSkill).
		ExecuteSkillAsync(Actor, Action, CharData);
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] Unexpected action type in BeginSkillExecution"));
		UnbindActionAnimationEnd(Actor);
		UnbindCombatNotify(Actor);
		FinalizeAsyncAction();
		return;
	}

	// Set timeout timer as failsafe
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AsyncTimeoutHandle,
			this,
			&UActionExecutor::OnAsyncActionTimeout,
			CurrentExecutionContext->TimeoutDuration,
			false);
	}

	// Check if animation was actually played.
	// If not waiting for animation (no montage), gate on defenses via TryFinalize.
	if (!bWaitingForAnimationEnd)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] No animation to wait for"));
		if (CurrentExecutionContext.IsSet() && CurrentExecutionContext->AreAllDefensesResolved())
		{
			bAllDefensesResolved = true;
		}
		TryFinalizeAsyncAction();
	}
	// Otherwise, OnActionAnimationEnded will call TryFinalizeAsyncAction
}

UCombatAnimInstance *UActionExecutor::GetCombatAnimInstance(AActor *Actor) const
{
	ACharacter *Character = Cast<ACharacter>(Actor);
	if (Character && Character->GetMesh())
	{
		return Cast<UCombatAnimInstance>(Character->GetMesh()->GetAnimInstance());
	}
	return nullptr;
}

void UActionExecutor::PlayActionMontageOnActor(AActor *Actor, UAnimMontage *Montage, float PlayRate)
{
	if (!Actor || !Montage)
	{
		return;
	}

	UCombatAnimInstance *CombatAnim = GetCombatAnimInstance(Actor);
	if (CombatAnim)
	{
		CombatAnim->PlayActionMontage(Montage, PlayRate);
	}
	else if (ACharacter *Character = Cast<ACharacter>(Actor))
	{
		// Fallback: Direct character montage
		Character->PlayAnimMontage(Montage, PlayRate);
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Playing montage %s via fallback"), *Montage->GetName());
	}

	// Telegraph scan (purely cosmetic — opens no window, gates no input, touches no difficulty).
	// For each authored telegraph on this montage's notifies, schedule its VFX tell to play
	// TelegraphLeadSeconds (wall-clock) BEFORE the notify fires. The notify's montage-local trigger
	// time is divided by the play rate so the lead is rate-INDEPENDENT (a speed buff doesn't shorten
	// the warning). A lead longer than the time-to-notify fires the tell at montage start (clamp).
	// Per-montage: each leg of the Ritual/Skill/Return chain scans its own notifies at its own start.
	const float Rate = FMath::Max(0.1f, PlayRate);
	for (const FAnimNotifyEvent &Event : Montage->Notifies)
	{
		const UCombatNotify *CN = Cast<UCombatNotify>(Event.Notify);
		if (!CN || CN->TelegraphLeadSeconds <= 0.0f || CN->Telegraph.VFX.IsNull())
		{
			continue; // no telegraph authored on this notify
		}

		const float FireDelay = (Event.GetTriggerTime() / Rate) - CN->TelegraphLeadSeconds;
		if (FireDelay <= 0.0f)
		{
			// Lead exceeds the time before the notify — fire the tell now (montage start).
			SpawnVFXArrayEntry(CN->Telegraph, 0);
			continue;
		}

		if (UWorld *World = GetWorld())
		{
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(
				Handle,
				FTimerDelegate::CreateLambda([this, Tell = CN->Telegraph]()
				{
					// Guard a stale fire — a cancelled action clears these timers, but belt-and-suspenders.
					if (CurrentExecutionContext.IsSet() && CurrentExecutionContext->bInProgress)
					{
						SpawnVFXArrayEntry(Tell, 0);
					}
				}),
				FireDelay, false);
			TelegraphTimerHandles.Add(Handle);
		}
	}
}

void UActionExecutor::BindActionAnimationEnd(AActor *Actor)
{
	UCombatAnimInstance *CombatAnim = GetCombatAnimInstance(Actor);
	if (CombatAnim)
	{
		// Remove first to prevent duplicate binding error
		CombatAnim->OnActionMontageEnded.RemoveDynamic(this, &UActionExecutor::OnActionAnimationEnded);
		CombatAnim->OnActionMontageEnded.AddDynamic(this, &UActionExecutor::OnActionAnimationEnded);
		bWaitingForAnimationEnd = true;
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Bound to OnActionMontageEnded for %s"), *Actor->GetName());
	}
}

void UActionExecutor::UnbindActionAnimationEnd(AActor *Actor)
{
	UCombatAnimInstance *CombatAnim = GetCombatAnimInstance(Actor);
	if (CombatAnim)
	{
		CombatAnim->OnActionMontageEnded.RemoveDynamic(this, &UActionExecutor::OnActionAnimationEnded);
	}
	bWaitingForAnimationEnd = false;
}

// Shared reconciliation (Stage 1): mark the action's animation finished and force-close
// every count-based (ExpectedImpacts>0, melee) defense window — the "animation done" half
// of the "later of (impacts-done, animation-done)" close. Damage is lumped at
// CloseDefenseWindow (Stage 2 splits per-impact), so reconcile == close: complete counts
// close normally; incomplete ones (fewer/missing Impact notifies, or an interrupted montage)
// reconcile here instead of hanging on the 8s failsafe. Called from BOTH the clean Skill-end
// branch and FinishMontageChain (which the interrupted path routes through, skipping Skill-end).
// Idempotent: by the time FinishMontageChain runs on the normal path, Skill-end already drained
// the count-based entries, so this is a no-op. Collect-then-close: CloseDefenseWindow
// re-entrantly removes the PendingDefenses entry, so we must not close mid-iteration.
static void ReconcileCountBasedDefenseWindows(FActionExecutionContext &ExecContext, UDefenseSystem *DefenseSys)
{
	ExecContext.bAnimationFinished = true;
	if (!DefenseSys)
	{
		return;
	}

	TArray<AActor *> DefendersToClose;
	for (auto &Pair : ExecContext.PendingDefenses)
	{
		if (Pair.Value.ExpectedImpacts > 0)
		{
			if (AActor *Defender = Pair.Key.Get())
			{
				DefendersToClose.Add(Defender);
			}
		}
	}

	for (AActor *Defender : DefendersToClose)
	{
		UE_LOG(LogTemp, Log, TEXT("[ImpactFrame] animation-end reconcile — closing window for %s"),
			   *GetNameSafe(Defender));
		DefenseSys->CloseDefenseWindow(Defender);
	}
}

void UActionExecutor::OnActionAnimationEnded(UAnimMontage *Montage, bool bInterrupted)
{
	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] OnActionAnimationEnded - bWaitingForAnimationEnd: %s, PendingActor: %s, Phase: %d"),
		   bWaitingForAnimationEnd ? TEXT("TRUE") : TEXT("FALSE"),
		   PendingExecutionActor ? *PendingExecutionActor->GetName() : TEXT("NULL"),
		   (int32)MontagePhase);

	if (!bWaitingForAnimationEnd)
	{
		return;
	}

	// 2b: a deferred ARM turn's ritual cast ended (or was interrupted) — complete
	// the arm and end the turn. Routed BEFORE the chain dispatch AND the interrupt
	// branch: an interrupted ritual cast still completes the arm (costs are paid
	// and the ritual already queued — interrupting the channel doesn't un-arm it).
	if (bArmingRitual)
	{
		FinishArmTurn(PendingExecutionActor);
		return;
	}

	// Montage-identity guard (return-skip fix): ignore an end for ANY montage that
	// is NOT the chain leg currently playing. On an Execution fire the skill montage
	// plays over a residual action montage; that residual's blended-out Montage_Stop
	// fires OnActionMontageEnded(residual, bInterrupted=true). Without this guard the
	// bInterrupted branch below runs FinishMontageChain -> clears bWaitingForAnimationEnd
	// mid-chain -> finalize through the open gate -> the real Return leg is skipped.
	// A genuine interrupt of the actual chain montage (ended == CurrentChainMontage)
	// still falls through and ends the chain, as it should.
	if (Montage != CurrentChainMontage)
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Ignoring spurious montage end %s (chain leg is %s)"),
			   Montage ? *Montage->GetName() : TEXT("<null>"),
			   CurrentChainMontage ? *CurrentChainMontage->GetName() : TEXT("<null>"));
		return;
	}

	// Chain-owned handles (return-skip fix A): advance off ChainActor/ChainSkill,
	// NOT PendingExecutionActor/GetCurrentSkillData() — on a deferred fire finalize
	// nulls PendingExecutionActor mid-Skill; reading it here would strand Return.
	AActor *Actor = ChainActor.Get();
	USkillDataBase *Skill = ChainSkill;

	// Lost actor/skill or interrupted → close the chain immediately (finalize with
	// the facing reassert). Matches today's interrupted/dropped-actor paths, which
	// skipped the remaining legs and fell straight through to finalize.
	if (bInterrupted || !Actor || !Skill)
	{
		FinishMontageChain(Actor);
		return;
	}

	// Advance the explicit chain. Chained montages play NEXT-TICK (not same-frame):
	// the anim instance resumes stance on montage end and would stomp a same-frame
	// action montage (spike finding). The first montage played immediately from
	// BeginMontageChain; only these transitions are deferred.
	switch (MontagePhase)
	{
	case EMontagePhase::Ritual:
	{
		UE_LOG(LogTemp, Log, TEXT("[Montage] Ritual ended - advancing to Skill"));
		TWeakObjectPtr<AActor> WeakActor = Actor;
		GetWorld()->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this, WeakActor]()
											 {
				if (bWaitingForAnimationEnd && WeakActor.IsValid())
				{
					PlaySkillStep(WeakActor.Get(), ChainSkill);
				} }));
		return;
	}

	case EMontagePhase::Skill:
	{
		UE_LOG(LogTemp, Log, TEXT("[Montage] Skill ended - advancing to Return"));

		// Stage 1 "later of" close: the hitting animation has ended. Reconcile/close the
		// count-based melee windows here (the animation is normally the LATER signal, so
		// this is what actually closes them) BEFORE the warp-back Return leg. An attack
		// with fewer Impact notifies than ExpectedImpacts still closes here, not at 8s.
		if (CurrentExecutionContext.IsSet())
		{
			ReconcileCountBasedDefenseWindows(*CurrentExecutionContext, GetDefenseSystem());
		}

		TWeakObjectPtr<AActor> WeakActor = Actor;
		GetWorld()->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this, WeakActor]()
											 {
				if (bWaitingForAnimationEnd && WeakActor.IsValid())
				{
					PlayReturnStep(WeakActor.Get(), ChainSkill);
				} }));
		return;
	}

	case EMontagePhase::Return:
	default:
		FinishMontageChain(Actor);
		return;
	}
}

void UActionExecutor::BindSpellNotify(AActor *Actor)
{
	UCombatAnimInstance *AnimInst = GetCombatAnimInstance(Actor);
	if (AnimInst)
	{
		AnimInst->OnActionNotify.AddDynamic(this, &UActionExecutor::OnSpellAnimNotify);
	}
}

void UActionExecutor::UnbindSpellNotify(AActor *Actor)
{
	UCombatAnimInstance *AnimInst = GetCombatAnimInstance(Actor);
	if (AnimInst)
	{
		AnimInst->OnActionNotify.RemoveDynamic(this, &UActionExecutor::OnSpellAnimNotify);
	}
}

void UActionExecutor::ClearPendingSpellData()
{
	PendingSpellCaster = nullptr;
	PendingSpellData = nullptr;
	PendingSpellTargets.Empty();
	PendingSpellSize = 1.0f;
	PendingSpellDamage = 0;
	bPendingSpellIsBrokenDarkness = false;
}

void UActionExecutor::OnSpellAnimNotify(FName NotifyName)
{
	if (!PendingSpellCaster || !PendingSpellData)
	{
		return;
	}

	if (NotifyName == FName("SpellCastStart"))
	{
		// D5 reader switch (SC5): the Muzzle-role VFXArray entry is
		// authoritative when present; loose MuzzleVFX is the fallback. Entry
		// Scale multiplies the action size (migrated entries carry Scale=1.0
		// → byte-identical) and bElementTinted gates the tint. The spawn
		// stays caster-anchored — "the muzzle" — exotic attach modes are the
		// index-driven notify path's job (SC2).
		const FSkillVFXEntry *MuzzleEntry = GetVFXEntryByRole(PendingSpellData, EVFXRole::Muzzle);
		UNiagaraSystem *MuzzleSystem = MuzzleEntry ? MuzzleEntry->VFX.LoadSynchronous()
												   : PendingSpellData->MuzzleVFX;
		if (MuzzleSystem)
		{
			FVector SpawnLocation = PendingSpellCaster->GetActorLocation();
			// TODO: Get hand socket location if available

			const float EntryScale = MuzzleEntry ? MuzzleEntry->Scale : 1.0f;
			const bool bTint = !MuzzleEntry || MuzzleEntry->bElementTinted;

			FHybridSpellColorData Colors = UHybridSpellColors::GetInfusionColors(
				PendingSpellData->Element, bPendingSpellIsBrokenDarkness);

			UNiagaraComponent *MuzzleComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(),
				MuzzleSystem,
				SpawnLocation,
				PendingSpellCaster->GetActorRotation(),
				FVector(PendingSpellSize * EntryScale),
				true, true);

			if (MuzzleComp && bTint)
			{
				MuzzleComp->SetColorParameter(FName("CoreColor"), Colors.PrimaryColor);
			}

			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] SpellCastStart - Muzzle VFX spawned%s"),
				   MuzzleEntry ? TEXT(" (VFXArray)") : TEXT(" (loose)"));
		}
	}
	else if (NotifyName == FName("SpellRelease"))
	{
		// D6 bridge: a spell WITH Cast entries fires its PRIMARY entry through
		// the same dispatch a UCombatNotify Family=Cast would — one spawn site
		// for both trigger paths, so current content (SpellRelease-authored
		// montages) delivers via the entry path. Empty CastArray → loose path.
		if (PendingSpellData->CastArray.Num() > 0)
		{
			DispatchSpellCast(PendingSpellCaster, PendingSpellData,
							  PendingSpellData->CastArray[0], PendingSpellSize,
							  PendingSpellTargets, PendingSpellDamage, 0);
			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] SpellRelease - dispatched CastArray[0]"));
		}
		else
		{
			SpawnSpellVFX(PendingSpellCaster, PendingSpellData, PendingSpellSize,
						  PendingSpellTargets, PendingSpellDamage);
			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] SpellRelease - Main spell VFX spawned (loose path)"));
		}
	}
}

// ==================== FUSED-MONTAGE RUNNER (Stage 12) ====================

void UActionExecutor::BindCombatNotify(AActor *Actor)
{
	if (UCombatAnimInstance *CombatAnim = GetCombatAnimInstance(Actor))
	{
		// Remove first to prevent duplicate binding error
		CombatAnim->OnCombatNotify.RemoveDynamic(this, &UActionExecutor::OnCombatNotifyReceived);
		CombatAnim->OnCombatNotify.AddDynamic(this, &UActionExecutor::OnCombatNotifyReceived);
	}
}

void UActionExecutor::UnbindCombatNotify(AActor *Actor)
{
	if (UCombatAnimInstance *CombatAnim = GetCombatAnimInstance(Actor))
	{
		CombatAnim->OnCombatNotify.RemoveDynamic(this, &UActionExecutor::OnCombatNotifyReceived);
	}
}

UMotionWarpingComponent *UActionExecutor::GetOrCreateWarpComponent(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UMotionWarpingComponent *Existing = Actor->FindComponentByClass<UMotionWarpingComponent>())
	{
		return Existing;
	}

	// Runtime fallback (the spike's pattern) — characters without an authored
	// persistent component still warp.
	UMotionWarpingComponent *Warp = NewObject<UMotionWarpingComponent>(Actor, TEXT("CombatWarp"));
	if (Warp)
	{
		Warp->RegisterComponent();
	}
	return Warp;
}

void UActionExecutor::BeginMontageChain(AActor *Actor, USkillDataBase *Skill, float PlayRate)
{
	if (!Actor || !Skill)
	{
		return;
	}

	// The live chain advances off its OWN handles, never PendingExecutionActor /
	// GetCurrentSkillData() — on a deferred fire finalize nulls PendingExecutionActor
	// mid-Skill, which would otherwise strand the Return leg (return-skip fix A).
	ChainActor = Actor;
	ChainSkill = Skill;

	// Entry to the explicit chain (Option B). The first montage plays IMMEDIATELY
	// (here, via the entry step, which presence-skips through the remaining legs);
	// every montage-end transition is scheduled NEXT-TICK by the dispatcher (the
	// stance-resume stomp avoidance).
	//
	// Deferred FIRE (2a): the ritual cast already played as its own arm turn — the
	// fire skips the ritual leg and starts at Skill (→ Return). A normal cast
	// (delay==0 or non-ritual) runs the full chain from Ritual.
	const bool bDeferredFire = CurrentExecutionContext.IsSet() &&
							   CurrentExecutionContext->Action.bIsDeferredFire;
	PendingMontagePlayRate = PlayRate;
	if (bDeferredFire)
	{
		PlaySkillStep(Actor, Skill);
	}
	else
	{
		PlayRitualStep(Actor, Skill);
	}
}

void UActionExecutor::PlayRitualStep(AActor *Actor, USkillDataBase *Skill)
{
	if (!Actor || !Skill)
	{
		return;
	}

	// Presence-driven: a null RitualCastMontage skips straight to the skill leg.
	if (Skill->RitualCastMontage)
	{
		MontagePhase = EMontagePhase::Ritual;
		CurrentChainMontage = Skill->RitualCastMontage; // tracked-leg identity (return-skip guard)
		UE_LOG(LogTemp, Log, TEXT("[Montage] PlayRitual %s"), *Skill->RitualCastMontage->GetName());
		PlayActionMontageOnActor(Actor, Skill->RitualCastMontage, PendingMontagePlayRate);
		return;
	}

	PlaySkillStep(Actor, Skill);
}

void UActionExecutor::PlaySkillStep(AActor *Actor, USkillDataBase *Skill)
{
	if (!Actor || !Skill)
	{
		return;
	}

	// Always SkillMontage — bIsDeferredFire gates validation/cost, not montage
	// choice; there is no ResolveActiveMontage.
	MontagePhase = EMontagePhase::Skill;
	CurrentChainMontage = Skill->SkillMontage; // tracked-leg identity (return-skip guard)
	UE_LOG(LogTemp, Log, TEXT("[Montage] PlaySkill %s"),
		   Skill->SkillMontage ? *Skill->SkillMontage->GetName() : TEXT("None"));
	PlayActionMontageOnActor(Actor, Skill->SkillMontage, PendingMontagePlayRate);
}

void UActionExecutor::PlayReturnStep(AActor *Actor, USkillDataBase *Skill)
{
	if (!Actor || !Skill)
	{
		return;
	}

	// Presence-driven: no ReturnMontage → finalize now (finalize-after-skill).
	if (!Skill->ReturnMontage)
	{
		FinishMontageChain(Actor);
		return;
	}

	MontagePhase = EMontagePhase::Return;

	// Warp return leg (W1): retarget WarpTarget to the origin snapshot — a
	// ReturnMontage with a warp window travels back to the grid slot; without one
	// it plays in place. Must run BEFORE the montage plays. The snapshot is read
	// from the runner's own fields now (SC-D) — same values, same timing as the
	// component round-trip it replaces.
	if (bHasGridPosition)
	{
		if (UMotionWarpingComponent *Warp = GetOrCreateWarpComponent(Actor))
		{
			Warp->AddOrUpdateWarpTargetFromLocationAndRotation(
				CombatConstants::WARP_TARGET_NAME, GridPosition, GridRotation);
			UE_LOG(LogTemp, Log, TEXT("[Warp] return target %s"),
				   *GridPosition.ToCompactString());
		}
	}

	CurrentChainMontage = Skill->ReturnMontage; // tracked-leg identity (return-skip guard)
	UE_LOG(LogTemp, Log, TEXT("[Montage] PlayReturn %s"), *Skill->ReturnMontage->GetName());
	PlayActionMontageOnActor(Actor, Skill->ReturnMontage, 1.0f);
}

void UActionExecutor::FinishArmTurn(AActor *Actor)
{
	// 2b arm completion — the ritual cast (the arm turn) has ended or was
	// interrupted. NO skill effects, NO damage, NO defense gating, NO chain: the
	// costs were paid and the ritual queued at arm. Unbind the spine, release the
	// context the arm held open (same end-state the synchronous arm produced so
	// the next-action guard is clean), and fire the stashed armed-success callback
	// ONCE — that ends the turn.
	bArmingRitual = false;

	if (Actor)
	{
		UnbindActionAnimationEnd(Actor); // clears bWaitingForAnimationEnd
		UnbindCombatNotify(Actor);
	}

	CurrentExecutionContext.Reset();

	if (AsyncActionCallback.IsBound())
	{
		AsyncActionCallback.Execute(PendingFinalResult);
		AsyncActionCallback.Unbind();
	}
}

void UActionExecutor::FinishMontageChain(AActor *Actor)
{
	// Last leg done — close the animation phase. bWaitingForAnimationEnd is cleared
	// HERE (inside UnbindActionAnimationEnd) and nowhere else, so finalize can only
	// run after the final montage. The notify spine is unbound here too.
	MontagePhase = EMontagePhase::Done;
	CurrentChainMontage = nullptr; // chain over - drop the tracked-leg identity
	ChainActor = nullptr;		   // chain over - drop the chain-owned handles
	ChainSkill = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Action animation ended - finalizing"));

	// Stage 1 "later of" close — catch-all for the INTERRUPTED path: an interrupted Skill
	// montage routes here (via the bInterrupted branch) WITHOUT passing the Skill-end case,
	// so without this an interrupted attack's count-based windows would hang to the 8s
	// failsafe. Idempotent on the normal path: Skill-end already drained the entries.
	if (CurrentExecutionContext.IsSet())
	{
		ReconcileCountBasedDefenseWindows(*CurrentExecutionContext, GetDefenseSystem());
	}

	// Montage-end facing reassert — nearest living enemy (arena center when none).
	if (Actor)
	{
		UCombatGridSubsystem *Grid = GetWorld()->GetGameInstance()->GetSubsystem<UCombatGridSubsystem>();
		if (Grid)
		{
			Grid->UpdateActorFacing(Actor, CachedArenaCenter);
			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Restored facing for %s after animation"),
				   *Actor->GetName());
		}
	}

	// Unbind action animation + runner notify spine
	if (Actor)
	{
		UnbindActionAnimationEnd(Actor);
		UnbindCombatNotify(Actor);
	}

	// Cleanup spell notify binding
	if (PendingSpellCaster)
	{
		UnbindSpellNotify(PendingSpellCaster);
		ClearPendingSpellData();
	}

	// Animation done — gate on defenses too. TryFinalize fires only if defenses already resolved.
	TryFinalizeAsyncAction();
}

const FSkillVFXEntry *UActionExecutor::GetVFXEntryByRole(const USkillDataBase *Skill, EVFXRole Role)
{
	if (!Skill)
	{
		return nullptr;
	}
	for (const FSkillVFXEntry &Entry : Skill->VFXArray)
	{
		// Null-asset entries don't mask the loose-field fallback.
		if (Entry.Role == Role && !Entry.VFX.IsNull())
		{
			return &Entry;
		}
	}
	return nullptr;
}

USkillDataBase *UActionExecutor::GetCurrentSkillData() const
{
	if (!CurrentExecutionContext.IsSet())
	{
		return nullptr;
	}

	const FAction &Action = CurrentExecutionContext->Action;
	switch (Action.ActionType)
	{
	case EActionType::Spell:
		return Action.SpellData;
	case EActionType::Ability:
		return ResolveActionSkill(Action);
	default:
		return nullptr;
	}
}

void UActionExecutor::OnCombatNotifyReceived(ECombatNotifyFamily Family, int32 Index)
{
	if (!CurrentExecutionContext.IsSet() || !CurrentExecutionContext->bInProgress)
	{
		return; // stale notify outside an action
	}

	switch (Family)
	{
	case ECombatNotifyFamily::VFX:
	{
		USkillDataBase *Skill = GetCurrentSkillData();
		if (!Skill)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Runner] VFX notify %d with no skill data — skipping"), Index);
			return;
		}
		if (!Skill->VFXArray.IsValidIndex(Index))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Runner] VFX index %d out of range (%d entries)"),
				   Index, Skill->VFXArray.Num());
			return;
		}
		SpawnVFXArrayEntry(Skill->VFXArray[Index], Index);
		return;
	}

	case ECombatNotifyFamily::Impact:
	{
		// Read ChainActor, not PendingExecutionActor: the latter is nulled by the
		// prior action's CompleteAsyncActionFinal during a deferred fire (re-entrant
		// clobber), so it can be NULL while this action's skill montage is still
		// firing hits. ChainActor is the stable handle, valid through the whole
		// montage chain — same source OnActionAnimationEnded advances off.
		AActor *Executor = ChainActor.Get();
		USkillDataBase *Skill = GetCurrentSkillData();
		const int32 HitCount = Skill ? Skill->HitCount : 0;
		// Fires into the void this stage — Stage 2's per-impact resolver binds OnImpactFrame.
		UE_LOG(LogTemp, Log, TEXT("[ImpactFrame] impact %d of %d for %s"),
			   Index, HitCount, *GetNameSafe(Executor));
		OnImpactFrame.Broadcast(Executor, Index);

		// Per-impact apply (Stage 2) + count-based "later of" close (Stage 1), melee.
		// Each landed impact APPLIES ITS DAMAGE at its frame (ResolvedDamageSplit[Index] of
		// the stashed reduced total) and increments the count. A defender closes only once it
		// has taken ALL its impacts AND the hitting animation has finished (bAnimationFinished);
		// for melee the animation is normally the LATER signal, so the close usually happens at
		// Skill-end (ReconcileCountBasedDefenseWindows) — this branch handles the symmetric
		// "impact after animation" case. ExpectedImpacts==0 ⇒ timer-close window (non-attack) →
		// left untouched. One swing hits all current defenders (melee cleave).
		// Three phases, all DEFERRED out of the iteration: ApplyOneImpact can broadcast
		// OnTargetKilled (subscribers may mutate PendingDefenses) and CloseDefenseWindow
		// re-entrantly removes the entry — neither is safe mid-iteration.
		if (UDefenseSystem *DefenseSys = GetDefenseSystem())
		{
			const bool bAnimationFinished = CurrentExecutionContext->bAnimationFinished;
			TArray<AActor *> DefendersToApply;
			TArray<AActor *> DefendersToClose;
			for (auto &Pair : CurrentExecutionContext->PendingDefenses)
			{
				FPendingDefenseContext &Ctx = Pair.Value;
				if (Ctx.ExpectedImpacts <= 0)
				{
					continue; // timer-close window — not count-based
				}
				AActor *Defender = Pair.Key.Get();
				if (!Defender)
				{
					continue;
				}
				// In-order sanity — the close-time tail [ImpactsLanded, HitCount) assumes
				// contiguous in-order notifies (authoring contract: 0..HitCount-1).
				if (Index != Ctx.ImpactsLanded)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Stage2] impact index %d != drain pointer %d for %s — out-of-order/gapped notify (authoring-contract)"),
						   Index, Ctx.ImpactsLanded, *Defender->GetName());
				}
				DefendersToApply.Add(Defender);
				if (++Ctx.ImpactsLanded >= Ctx.ExpectedImpacts && bAnimationFinished)
				{
					DefendersToClose.Add(Defender);
				}
			}
			// Resolve THIS impact's defense (split-then-reduce + match-and-consume) and apply
			// its reduced slice. One impact frame = one moment, so all defenders share a single
			// ImpactTime. Done after iteration — see above.
			const double ImpactTime = FPlatformTime::Seconds();
			for (AActor *Defender : DefendersToApply)
			{
				FPendingDefenseContext *Ctx = CurrentExecutionContext->PendingDefenses.Find(Defender);
				if (!Ctx)
				{
					continue; // removed by a prior impact's death broadcast
				}
				// Melee difficulty source: the per-impact-ordinal table (IsValidIndex guard → Easy ×1.0).
				const FDefenseDifficultyTriple MeleeDifficulty =
					CurrentExecutionContext->ResolvedDifficulty.IsValidIndex(Index)
						? CurrentExecutionContext->ResolvedDifficulty[Index]
						: FDefenseDifficultyTriple();
				ResolveImpactDefense(Defender, Index, ImpactTime, MeleeDifficulty);
				StashHitFlags(*Ctx, CurrentExecutionContext->ResolvedHitFlags, Index);
				ApplyOneImpact(Ctx->Attacker.Get(), Defender, *Ctx, Index, /*bDamageIsPerImpact=*/true);
			}
			// Hybrid B2 interrupt: if THIS impact was authored interruptable and EVERY targeted defender
			// parried/dodged it, abort the rest of the attack. Checked HERE — after all defenders resolved
			// this impact, BEFORE the closes remove their contexts — so AllInterruptSucceeded sees the full
			// set. Single-target degenerates to the one defender's outcome (the grab).
			if (CurrentExecutionContext->ResolvedHitFlags.IsValidIndex(Index) &&
				CurrentExecutionContext->ResolvedHitFlags[Index].bInterruptable &&
				AllInterruptSucceeded())
			{
				InterruptAsyncAction(ChainActor.Get(), ChainSkill);
				return;
			}
			for (AActor *Defender : DefendersToClose)
			{
				UE_LOG(LogTemp, Log, TEXT("[ImpactFrame] impacts + animation done for %s — closing window"),
					   *GetNameSafe(Defender));
				DefenseSys->CloseDefenseWindow(Defender);
			}
		}
		return;
	}

	case ECombatNotifyFamily::Cast:
	{
		// Spell-context only this stage: the dispatch reads the pending-spell
		// stash (caster/targets/size/damage). Ability/attack Cast entries are
		// future authoring (TODO: non-spell dispatch context).
		if (!PendingSpellData || !PendingSpellCaster)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Runner] Cast notify %d outside a spell action — non-spell Cast entries not yet supported"), Index);
			return;
		}
		if (PendingSpellData->CastArray.IsEmpty())
		{
			// Empty-CastArray fallback: the loose-field dispatch (delta-
			// serialization limit — fully-default spells migrated no entry).
			SpawnSpellVFX(PendingSpellCaster, PendingSpellData, PendingSpellSize,
						  PendingSpellTargets, PendingSpellDamage);
			return;
		}
		if (!PendingSpellData->CastArray.IsValidIndex(Index))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Runner] Cast index %d out of range (%d entries)"),
				   Index, PendingSpellData->CastArray.Num());
			return;
		}
		DispatchSpellCast(PendingSpellCaster, PendingSpellData,
						  PendingSpellData->CastArray[Index], PendingSpellSize,
						  PendingSpellTargets, PendingSpellDamage, Index);
		return;
	}

	case ECombatNotifyFamily::SFX:
		// No production SFX array yet (deferred).
		UE_LOG(LogTemp, Verbose, TEXT("[Runner] SFX notify %d — no SFX array (deferred)"), Index);
		return;
	}
}

void UActionExecutor::SpawnVFXArrayEntry(const FSkillVFXEntry &Entry, int32 Index)
{
	AActor *Caster = PendingExecutionActor;
	if (!Caster)
	{
		return;
	}

	// Soft ref — the runner resolves at spawn time.
	UNiagaraSystem *System = Entry.VFX.LoadSynchronous();
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Runner] VFX[%d] '%s' has no asset"), Index, *Entry.Label);
		return;
	}

	UNiagaraComponent *Spawned = nullptr;

	// CasterSocket attaches (follows the hand through the montage) — spike-proven.
	if (Entry.Attach == EVFXAttach::CasterSocket)
	{
		ACharacter *Character = Cast<ACharacter>(Caster);
		if (Character && Character->GetMesh() && Character->GetMesh()->DoesSocketExist(Entry.SocketName))
		{
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(
				System, Character->GetMesh(), Entry.SocketName,
				FVector::ZeroVector, FRotator::ZeroRotator, FVector(Entry.Scale),
				EAttachLocation::SnapToTarget, true, ENCPoolMethod::None);
		}
	}

	if (!Spawned)
	{
		FVector SpawnLoc;
		if (!ResolveVFXAttachLocation(Entry.Attach, Entry.SocketName, Index, SpawnLoc))
		{
			return; // warned inside
		}
		Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), System, SpawnLoc, FRotator::ZeroRotator, FVector(Entry.Scale),
			true, true);
	}

	// Element tint — the action's resolved element (set by all three paths),
	// BD-aware like the legacy spell VFX spawns.
	if (Spawned && Entry.bElementTinted)
	{
		UBrokenDarknessManager *BDManager = GetBrokenDarknessManager(Caster);
		const bool bIsBD = BDManager && BDManager->IsTransformed();
		const ESpellElement Element = CurrentExecutionContext->PartialResult.AttackElement;
		const FHybridSpellColorData Colors = UHybridSpellColors::GetInfusionColors(Element, bIsBD);
		Spawned->SetColorParameter(FName("CoreColor"), Colors.PrimaryColor);
		Spawned->SetColorParameter(FName("EdgeColor"), Colors.BlendedColor);
	}

	UE_LOG(LogTemp, Log, TEXT("[Runner] VFX[%d] '%s' spawned (attach=%d, scale=%.2f)"),
		   Index, *Entry.Label, (int32)Entry.Attach, Entry.Scale);
}

bool UActionExecutor::ResolveVFXAttachLocation(EVFXAttach Attach, FName SocketName, int32 Index, FVector &OutLoc) const
{
	AActor *Caster = PendingExecutionActor;
	if (!Caster)
	{
		return false;
	}

	// First valid action target (Target/ImpactPoint modes).
	AActor *Target = nullptr;
	if (CurrentExecutionContext.IsSet())
	{
		for (AActor *Candidate : CurrentExecutionContext->Action.Targets)
		{
			if (IsValid(Candidate))
			{
				Target = Candidate;
				break;
			}
		}
	}

	switch (Attach)
	{
	case EVFXAttach::Target:
		if (!Target)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Runner] VFX[%d] wants Target, none available — skipping"), Index);
			return false;
		}
		OutLoc = Target->GetActorLocation();
		return true;

	case EVFXAttach::ImpactPoint:
		// No warp yet — impacts land on the target, not under the caster
		// (the spike mapped this to caster loc because it WAS warped onto
		// the target; production isn't until the movement arc).
		OutLoc = Target ? Target->GetActorLocation() : Caster->GetActorLocation();
		return true;

	case EVFXAttach::CasterSocket:
		// Attached spawn already failed (missing socket) — actor-location fallback.
		UE_LOG(LogTemp, Warning, TEXT("[Runner] VFX[%d] socket '%s' invalid, using caster location"),
			   Index, *SocketName.ToString());
		OutLoc = Caster->GetActorLocation();
		return true;

	case EVFXAttach::Caster:
	case EVFXAttach::World: // TODO: no authored world point yet — caster loc
	default:
		OutLoc = Caster->GetActorLocation();
		return true;
	}
}

// ==================== ABILITY EFFECT SYSTEM ====================

int32 UActionExecutor::GetUniqueEffectID()
{
	return ++EffectIDCounter;
}

TArray<AActor *> UActionExecutor::GetAllEnemies(AActor *User, int32 UserTeam)
{
	TArray<AActor *> Enemies;

	UGameInstance *GI = GetGameInstance();
	if (!GI)
		return Enemies;

	UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
	if (!TurnMgr)
		return Enemies;

	// Get combatants from both teams and filter
	for (int32 TeamIdx = 0; TeamIdx <= 1; TeamIdx++)
	{
		if (TeamIdx != UserTeam)
		{
			TArray<AActor *> TeamMembers = TurnMgr->GetTeamMembers(TeamIdx);
			Enemies.Append(TeamMembers);
		}
	}

	return Enemies;
}

TArray<AActor *> UActionExecutor::GetAllAllies(AActor *User, int32 UserTeam)
{
	TArray<AActor *> Allies;

	UGameInstance *GI = GetGameInstance();
	if (!GI)
		return Allies;

	UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
	if (!TurnMgr)
		return Allies;

	Allies = TurnMgr->GetTeamMembers(UserTeam);

	return Allies;
}

TArray<AActor *> UActionExecutor::GetAllCombatants()
{
	TArray<AActor *> All;

	UGameInstance *GI = GetGameInstance();
	if (!GI)
		return All;

	UTurnManager *TurnMgr = GI->GetSubsystem<UTurnManager>();
	if (!TurnMgr)
		return All;

	// Get both teams
	All.Append(TurnMgr->GetTeamMembers(0));
	All.Append(TurnMgr->GetTeamMembers(1));

	return All;
}

void UActionExecutor::GetEffectTargets(
	AActor *User,
	const TArray<AActor *> &ActionTargets,
	ETargetType TargetType,
	int32 UserTeam,
	TArray<AActor *> &OutTargets)
{
	OutTargets.Empty();

	switch (TargetType)
	{
	case ETargetType::Self:
		OutTargets.Add(User);
		break;

	case ETargetType::SingleEnemy:
		// Use first action target (the enemy we attacked)
		if (ActionTargets.Num() > 0)
		{
			OutTargets.Add(ActionTargets[0]);
		}
		break;

	case ETargetType::AllEnemies:
		OutTargets = GetAllEnemies(User, UserTeam);
		break;

	case ETargetType::SingleAlly:
		// For abilities, SingleAlly typically means self
		// Could expand for ally selection in future
		OutTargets.Add(User);
		break;

	case ETargetType::AllAllies:
		OutTargets = GetAllAllies(User, UserTeam);
		break;

	case ETargetType::Everyone:
		OutTargets = GetAllCombatants();
		break;
	}
}

void UActionExecutor::ApplySkillEffects(
	AActor *User,
	const TArray<AActor *> &Targets,
	const TArray<FSkillEffect> &Effects,
	const FString &SourceName,
	FActionResult &Result,
	bool bCausedDeath,
	ESpellElement ResolvedCastElement,
	EActionType ActionKind,
	EPhysicalDamageType PhysicalType)
{
	if (Effects.Num() == 0)
	{
		return;
	}

	USkillEffectManager *StatusMgr = GetSkillEffectManager();
	if (!StatusMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] ApplySkillEffects - No SkillEffectManager"));
		return;
	}

	// Get user's team for targeting
	UGameInstance *GI = GetGameInstance();
	UTurnManager *TurnMgr = GI ? GI->GetSubsystem<UTurnManager>() : nullptr;
	int32 UserTeam = TurnMgr ? TurnMgr->GetActorTeam(User) : 0;

	for (const FSkillEffect &Effect : Effects)
	{
		if (!Effect.IsValid())
		{
			continue;
		}

		// Check condition
		bool bConditionMet = false;
		switch (Effect.Condition)
		{
		case ESkillTrigger::Always:
			bConditionMet = true;
			break;

		case ESkillTrigger::OnHit:
			bConditionMet = Result.TotalDamageDealt > 0;
			break;

		case ESkillTrigger::OnCrit:
			bConditionMet = Result.bWasCritical;
			break;

		case ESkillTrigger::OnKill:
			bConditionMet = bCausedDeath;
			break;

		default:
			// Other triggers not applicable to ability effects
			bConditionMet = false;
			break;
		}

		if (!bConditionMet)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[ActionExecutor] Effect %s condition not met"),
				   *UEnum::GetValueAsString(Effect.EffectType));
			continue;
		}

		// Determine effect targets
		TArray<AActor *> EffectTargets;
		GetEffectTargets(User, Targets, Effect.Target, UserTeam, EffectTargets);

		if (EffectTargets.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] No targets found for effect %s"),
				   *UEnum::GetValueAsString(Effect.EffectType));
			continue;
		}

		// Handle drain effects specially
		if (Effect.IsDrain() && Effect.Condition == ESkillTrigger::OnHit)
		{
			int32 DrainAmount = FMath::RoundToInt(Result.TotalDamageDealt * Effect.DrainPercent);

			if (Effect.EffectType == ESkillEffectType::HealthRestore)
			{
				// Heal the user
				UCharacterDataComponent *CharComp = User->FindComponentByClass<UCharacterDataComponent>();
				if (CharComp)
				{
					CharComp->ServerHeal(DrainAmount);

					OnHealingDone.Broadcast(User, User, DrainAmount);
					UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Drain healed %s for %d HP (%.0f%% of %d damage)"),
						   *User->GetName(), DrainAmount, Effect.DrainPercent * 100.0f, Result.TotalDamageDealt);
				}
			}
			else if (Effect.EffectType == ESkillEffectType::EnergyRestore)
			{
				// Restore energy to user
				UCharacterDataComponent *CharComp = User->FindComponentByClass<UCharacterDataComponent>();
				if (CharComp)
				{
					CharComp->ServerGainEnergy(DrainAmount);
					UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Drain restored %s for %d EP (%.0f%% of %d damage)"),
						   *User->GetName(), DrainAmount, Effect.DrainPercent * 100.0f, Result.TotalDamageDealt);
				}
			}
			continue;
		}

		// Per-effect element: status-bar manipulation effects (sweep-4
		// StatusIncrease/StatusDecrease) use the resolved cast element so the
		// gauge fills/drains in the correct element. SPELL-authored DOTs now
		// inherit the cast element too (feature/authored-skill-dots — deliberate
		// change from the historical always-Generic invariant): a Fire spell's
		// authored burn is Fire. Ability/attack DOTs route through the
		// physical-type mapping below instead. Every OTHER effect type stays
		// Generic to preserve historical behaviour.
		const ESpellElement EffectElement =
			(Effect.EffectType == ESkillEffectType::StatusIncrease ||
			 Effect.EffectType == ESkillEffectType::StatusDecrease ||
			 (ActionKind == EActionType::Spell && Effect.EffectType == ESkillEffectType::DOT))
				? ResolvedCastElement
				: ESpellElement::Generic;

		// For instant gauge manipulators (Value > 0, no need to persist the
		// effect on the target's active list), the runtime Value field carries
		// the absolute buildup amount. DOTs also pass authored Value through —
		// FSkillEffect documents Value as the flat per-tick amount, and the
		// factory's (Value != 0 ? Value : Magnitude×100) fallback keeps the
		// Magnitude-authored shape working. Stat-modifier effects keep the
		// percentage conversion.
		const int32 RuntimeValue =
			(Effect.EffectType == ESkillEffectType::StatusIncrease ||
			 Effect.EffectType == ESkillEffectType::StatusDecrease ||
			 Effect.EffectType == ESkillEffectType::DOT)
				? Effect.Value
				: FMath::RoundToInt(Effect.Magnitude * 100.0f); // existing percentage shape

		// ABILITY/ATTACK authored DoT → physical-type status (Slash→Bleed,
		// Pierce→ArmorBreak, Impact→Stun) via the existing weapon mapping — NOT
		// an elemental DoT. The factory owns the status shape (durations 3/2/1,
		// value derivation); the authored value feeds its buildup input. None
		// (no weapon resolved) falls through to the legacy Generic shape below.
		if (ActionKind != EActionType::Spell &&
			Effect.EffectType == ESkillEffectType::DOT &&
			PhysicalType != EPhysicalDamageType::None)
		{
			const int32 PhysValue = (Effect.Value != 0)
										? Effect.Value
										: FMath::RoundToInt(Effect.Magnitude * 100.0f);
			for (AActor *EffectTarget : EffectTargets)
			{
				// Authored path: ALWAYS pass a >0 override so the factory's canonical
				// per-status defaults (passive-proc territory) can never apply here.
				// Authored Duration=0 resolves to 1 turn — a visibly-wrong nub that
				// surfaces the authoring mistake instead of masking it as a plausible
				// 3-turn bleed. Mirrors the spell path's (Duration > 0 ? Duration : 1).
				FActiveSkillEffect PhysEffect = FActiveSkillEffect::CreateFromPhysicalDamageType(
					SourceName,
					GetUniqueEffectID(),
					static_cast<uint8>(PhysicalType),
					PhysValue,
					/*InfusionMultiplier*/ 1.0f,
					/*HitCount*/ 1,
					/*DurationOverride*/ FMath::Max(1, Effect.Duration));

				StatusMgr->ApplyEffect(EffectTarget, PhysEffect, User, SourceName, UserTeam);
				Result.StatusEffectsApplied++;

				UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Applied %s (physical-type DoT) to %s"),
					   *PhysEffect.EffectName, *EffectTarget->GetName());
			}
			continue;
		}

		// Apply effect to each target as a status effect
		for (AActor *EffectTarget : EffectTargets)
		{
			FActiveSkillEffect StatusEffect = FActiveSkillEffect::CreateFromSpellEffect(
				SourceName + TEXT(" Effect"),
				GetUniqueEffectID(),
				Effect.EffectType,
				Effect.Magnitude,
				RuntimeValue,
				Effect.Duration,
				EffectElement,
				ESkillEffectTiming::StartOfOwnTurn,
				Effect.Condition,
				Effect.ConditionThreshold,
				Effect.TargetCondition,
				Effect.TargetThreshold);

			StatusMgr->ApplyEffect(EffectTarget, StatusEffect, User, SourceName, UserTeam);
			Result.StatusEffectsApplied++;

			UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] Applied %s to %s"),
				   *Effect.GetDescription(), *EffectTarget->GetName());
		}
	}
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
		if (bIsSpellAction)
		{
			if (Action.SpellData)
			{
				BaseCost = Action.SpellData->CalculateEnergyCost(CommitCharData);
			}
		}
		else if (USkillDataBase *Skill = ResolveActionSkill(Action))
		{
			const bool bIsInfused = (Action.SelectedSource != EInfusionSourceOption::None);
			BaseCost = Skill->CalculateEnergyCost(CommitCharData, bIsInfused);
		}
		PreEffInfusedEP = FMath::RoundToInt(BaseCost * ComputeInfusionCostMultiplier(Level, bIsSpellAction, CommitComp));
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
		URingManager *RingMgr = GetRingManager();
		if (!RingMgr)
		{
			break;
		}

		URingData *Ring = RingMgr->GetActiveRing(Actor);
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
			ActionTier = Action.SpellData->Tier;
		}
		else
		{
			// Ability or attack: action tier inherits from active weapon.
			if (UWeaponManager *WeaponMgr = GetWeaponManager())
			{
				if (UWeaponData *Weapon = WeaponMgr->GetActiveWeapon(Actor))
				{
					ActionTier = Weapon->Tier;
				}
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
		URingManager *RingMgr = GetRingManager();
		if (!RingMgr)
		{
			break;
		}

		URingData *Ring = RingMgr->GetPrimaryRing(Actor);
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
			ActionTier = Action.SpellData->Tier;
		}
		else
		{
			if (UWeaponManager *WeaponMgr = GetWeaponManager())
			{
				if (UWeaponData *Weapon = WeaponMgr->GetActiveWeapon(Actor))
				{
					ActionTier = Weapon->Tier;
				}
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
		UWeaponManager *WeaponMgr = GetWeaponManager();
		if (!WeaponMgr)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ActionExecutor] %s: WeaponCrystal infusion but WeaponManager unavailable"),
				   *Actor->GetName());
			break;
		}

		UWeaponData *Weapon = WeaponMgr->GetActiveWeapon(Actor);
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
		EItemTier ActionTier = Weapon->Tier;
		if (bIsSpell && Action.SpellData)
		{
			ActionTier = Action.SpellData->Tier;
		}

		CrystalMgr->ProcessPostCastWear(
			Actor, Weapon, *Attachment, ActionTier, Level, bIsSpell);

		// Per-action stat modifiers live on the execution context (action-start), not here.
		break;
	}

	case EInfusionSourceOption::Evolution:
	{
		// Phase 6 — Evolution backlash:
		//   L1: 5% HP + 15 self-status build (using evolution element)
		//   L2: 10% HP + 25 self-status build
		// HP percentages match Innate/Raw; the cost is deferred to finalize too.
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

		// 2. HP cost — same percentages as Innate/Raw (5% L1, 10% L2). Computed now
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
			EItemTier ActionTier = EItemTier::F_Tier;
			if (UWeaponManager *WeaponMgr = GetWeaponManager())
			{
				if (UWeaponData *Weapon = WeaponMgr->GetActiveWeapon(Actor))
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
FActionStatModifiers UActionExecutor::ComputeActionStatModifiers(const FAction &Action, AActor *Actor) const
{
	FActionStatModifiers Result;

	if (!Actor)
		return Result;

	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp || !CharComp->CharacterData)
		return Result;

	// 1. Reality innate — Refractor (Caster + InnateElement = Reality)
	if (CharComp->CharacterData->CharacterClass == ECharacterClass::Caster &&
		CharComp->CharacterData->InnateElement == ESpellElement::Reality)
	{
		Result.AddFlatPercent(RealityBoost::INNATE_PERCENT);
	}

	// 2. Evolution crystal slotted as primary
	UEvolutionItemData *PrimaryEvolutionCrystal = nullptr;
	if (ULoadoutComponent *LC = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		const FCombatLoadout Loadout = LC->GetActiveLoadout();
		if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
		{
			PrimaryEvolutionCrystal = Loadout.PrimaryEvolution.Item;
		}
	}

	if (PrimaryEvolutionCrystal)
	{
		// TODO: Apply Crystal->StatBonus persistent bonuses via the equipment-bonus
		// channel (ApplyEvolutionPillarModifier for the pillar percent fields;
		// per-action substat flow for primary-slotted crystals is GONE in the
		// StatBonus migration — substat values now apply via infusion only,
		// see GetInfusionStatModifiers below). This block previously called
		// GetActionModifiers(1.0f) which mixed both channels.

		// If the slotted Evolution crystal is Reality-element, add the slotted bonus.
		if (PrimaryEvolutionCrystal->GetAssociatedElement() == ESpellElement::Reality)
		{
			Result.AddFlatPercent(RealityBoost::SLOTTED_PERCENT);
		}
	}

	// 3. Infusion contributions — only when an infusion is active on this action.
	const int32 InfusionLevel = (Action.ActionType == EActionType::Spell)
									? Action.SpellInfusionLevel
									: Action.AbilityInfusionLevel;

	if (InfusionLevel > 0 && Action.SelectedSource != EInfusionSourceOption::None)
	{
		const ESpellElement SourceElement = GetElementForSourceOption(Actor, Action.SelectedSource);

		// Reality crystal infused — flat percent, scaled by infusion level.
		if (SourceElement == ESpellElement::Reality)
		{
			const float Pct = (InfusionLevel == 1)
								  ? RealityBoost::L1_PERCENT
								  : RealityBoost::L2_PERCENT;
			Result.AddFlatPercent(Pct);
		}

		// Evolution-infused authored stats. Source attachment resolves via
		// ResolveInfusionAttachment (WeaponCrystal/ActiveRing/PrimaryRing/Evolution
		// → the appropriate slotted/equipped attachment). InfusionMultiplier
		// follows the locked design: L1 = 0.5, L2 = 1.0. Reality flat-bump
		// above still applies independently for Reality-element infusion.
		const FRuntimeAttachedItem InfusionAttachment = ResolveInfusionAttachment(Actor, Action);
		if (InfusionAttachment.IsEvolution() && InfusionAttachment.Evolution.Item)
		{
			const float InfusionMultiplier = (InfusionLevel == 1) ? 0.5f : 1.0f;
			// Asset Base ints (authored, stays) + the attachment's per-instance
			// GeneratedStatBonus ints — both through the SAME mapping/multiplier,
			// so rolled ints inherit the infusion-conditional split (U3c).
			// All-zero Generated adds nothing (no rolled instance slotted).
			Result.Accumulate(InfusionAttachment.Evolution.Item->GetInfusionStatModifiers(InfusionMultiplier));
			Result.Accumulate(UEvolutionItemData::MapToInfusionModifiers(InfusionAttachment.Evolution.GeneratedStatBonus, InfusionMultiplier));
		}
	}

	return Result;
}

void UActionExecutor::DebugAsyncState()
{
	if (CurrentExecutionContext.IsSet())
	{
		const FActionExecutionContext &Ctx = CurrentExecutionContext.GetValue();
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] ASYNC STATE:"));
		UE_LOG(LogTemp, Warning, TEXT("  InProgress: %s"), Ctx.bInProgress ? TEXT("YES") : TEXT("NO"));
		UE_LOG(LogTemp, Warning, TEXT("  Executor: %s"), Ctx.Executor.IsValid() ? *Ctx.Executor->GetName() : TEXT("NULL"));
		UE_LOG(LogTemp, Warning, TEXT("  Action: %s"), *Ctx.Action.GetActionName());
		UE_LOG(LogTemp, Warning, TEXT("  Duration: %.2fs"), FPlatformTime::Seconds() - Ctx.StartTime);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[ActionExecutor] No async action in progress"));
	}
}

void UActionExecutor::DebugForceResetAsync()
{
	if (CurrentExecutionContext.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActionExecutor] FORCE RESETTING stuck async action"));
		CurrentExecutionContext.Reset();
		PendingExecutionActor = nullptr;
		PendingExecutionCharData = nullptr;
		bWaitingForAnimationEnd = false;
	}
}