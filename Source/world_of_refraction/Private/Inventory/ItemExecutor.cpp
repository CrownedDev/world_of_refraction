// Copyright Epic Games, Inc. All Rights Reserved.

#include "Inventory/ItemExecutor.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Combat/TurnManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Combat/CombatConstants.h"
#include "Inventory/ItemConstants.h"

void UItemExecutor::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Initialized"));
}

void UItemExecutor::Deinitialize()
{
	SkillEffectManagerRef = nullptr;
	Super::Deinitialize();
}

// ========================================
// MAIN EXECUTION
// ========================================

FItemUseResult UItemExecutor::UseItem(AActor *User, FCrystalId Id, AActor *Target)
{
	FItemUseResult Result;

	if (!User)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid user");
		return Result;
	}

	// Default target to user if not specified (for self-target items)
	if (!Target)
	{
		Target = User;
	}

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] %s using %s on %s"),
		   *User->GetName(), *ItemIdentity::GetDisplayName(Id), *Target->GetName());

	// Execute based on effect type. AbilityStone maps to EItemEffectType::None
	// (attach-only) — no case matches, so it falls to the default failure arm.
	EItemEffectType EffectType = ItemIdentity::GetItemEffectType(Id);

	switch (EffectType)
	{
	case EItemEffectType::Damage:
		ExecuteDamageEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::Healing:
		ExecuteHealingEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::EnergyRestore:
		ExecuteEnergyRestoreEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::GrantBonusTurn:
		ExecuteBonusTurnEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffDefense:
		ExecuteDefenseBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffCrit:
		ExecuteCritBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffTurnSpeed:
		ExecuteTurnSpeedStoneEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffStatusMultiplier:
		ExecuteStatusBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffEfficiency:
		ExecuteEfficiencyBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffMaxHP:
		ExecuteMaxHPBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffMaxEP:
		ExecuteMaxEPBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffRawDamage:
		ExecuteRawDamageBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffSpellDamage:
		ExecuteSpellDamageBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffResistance:
		ExecuteResistanceBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffSpellSpeed:
		ExecuteSpellSpeedBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffActionSpeed:
		ExecuteActionSpeedBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffCritDamage:
		ExecuteCritDamageBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffLuck:
		ExecuteLuckBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::BuffReflex:
		ExecuteReflexBuffEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::Silence:
		ExecuteSilenceEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::Gamble:
		ExecuteGambleEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::Cleanse:
		ExecuteCleanseEffect(User, Target, Id, Result);
		break;

	case EItemEffectType::StatusClear:
		ExecuteStatusClearEffect(User, Target, Id, Result);
		break;

	default:
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Unknown item effect type");
		return Result;
	}

	// Broken Darkness absorption — when a crystal is used on a BD character,
	// they absorb its elemental energy (EP scaled by the crystal's tier).
	if (IsBrokenDarknessCharacter(Target))
	{
		ApplyBrokenDarknessBonus(Target, Id, Result);
	}

	Result.bSuccess = true;

	// Broadcast event
	OnItemUsed.Broadcast(User, Id, Result);

	return Result;
}

FItemUseResult UItemExecutor::UseItemMultiTarget(AActor *User, FCrystalId Id, const TArray<AActor *> &Targets)
{
	FItemUseResult CombinedResult;
	CombinedResult.bSuccess = true;

	for (AActor *Target : Targets)
	{
		if (!Target)
			continue;

		FItemUseResult SingleResult = UseItem(User, Id, Target);

		// Accumulate results
		CombinedResult.DamageDealt += SingleResult.DamageDealt;
		CombinedResult.HealingDone += SingleResult.HealingDone;
		CombinedResult.BuffsApplied += SingleResult.BuffsApplied;
		CombinedResult.DebuffsRemoved += SingleResult.DebuffsRemoved;

		if (!SingleResult.bSuccess)
		{
			CombinedResult.bSuccess = false;
			CombinedResult.ErrorMessage = SingleResult.ErrorMessage;
		}
	}

	return CombinedResult;
}

// ========================================
// EFFECT HANDLERS
// ========================================

void UItemExecutor::ExecuteDamageEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Garnet - percentage-based fire damage (Phase 2). One immediate Fire hit on
	// use, then a DOT tail for the configured duration ("throw a grenade" feel).
	// The immediate hit and each DOT tick are equal: a percent of the target's MaxHP.
	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
	{
		OutResult.ErrorMessage = TEXT("Target has no character data");
		return;
	}

	const float DamagePercent = CrystalEffectTable::GetDOTDamagePercent(Id);
	const int32 Duration = CrystalEffectTable::GetDOTDuration(Id);

	if (DamagePercent <= 0.0f || Duration <= 0)
	{
		OutResult.ErrorMessage = TEXT("Invalid Garnet DOT values");
		return;
	}

	const float DamagePerTurn = TargetComp->MaxHP * DamagePercent / CombatConstants::STAT_PERCENT_DIVISOR;
	const int32 DamagePerTurnInt = FMath::Max(1, FMath::RoundToInt(DamagePerTurn));

	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager unavailable");
		return;
	}

	// Garnet's per-event Fire buildup, from the shared elemental-buildup table.
	// The same amount accompanies the immediate hit and every DOT tick — it is
	// carried on the DOT via BuildupPerTick (see below).
	float BuildupPerEvent = 0.0f;
	UStatusBuildupManager *SBM = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
	if (SBM)
	{
		const float BarMax = SBM->GetStatusBarBuildup(Target) + SBM->GetBuildupToTrigger(Target);
		BuildupPerEvent = BarMax * CrystalEffectTable::GetElementalBuildupPercent(Id) / CombatConstants::STAT_PERCENT_DIVISOR;
	}

	// Immediate Fire hit on use — equal to one DOT tick.
	// Immediate Garnet hit is unclamped — can secure kills. Only DOT ticks have
	// the can't-kill clamp (SkillEffectManager leaves the target at 1 HP).
	const int32 HPBefore = TargetComp->CurrentHP;
	TargetComp->ServerTakeDamage(DamagePerTurnInt);
	OutResult.DamageDealt = HPBefore - TargetComp->CurrentHP;

	// Fire status buildup for the immediate hit's damage event.
	if (SBM && BuildupPerEvent > 0.0f)
	{
		SBM->AddStatusBuildup(User, Target, FMath::RoundToInt(BuildupPerEvent),
							   ESpellElement::Fire, EPhysicalDamageType::None);
	}

	// Apply the fire DOT tail via SkillEffectManager — first tick lands at the
	// end of the target's next turn. BuildupPerTick carries the per-event buildup
	// so each tick contributes the same amount the immediate hit did.
	//
	// Element-keyed, tier-agnostic: one Burn per target; the strength policy
	// (SkillEffectManager re-apply) decides which tier's values win. Mirrors the
	// on-hit Burn/Chill element-token convention. DOTElement feeds BOTH the ID and
	// CreateDOT from the crystal (Fire for Garnet) so they can never drift.
	const ESpellElement DOTElement = ItemIdentity::GetElement(Id);
	const int32 EffectID = static_cast<int32>(GetTypeHash(DOTElement)) ^ static_cast<int32>(ESkillEffectType::DOT);
	FActiveSkillEffect DOT = FActiveSkillEffect::CreateDOT(
		TEXT("Burn"), EffectID, DamagePerTurnInt, Duration, DOTElement);
	DOT.BuildupPerTick = BuildupPerEvent;

	SEM->ApplyEffect(Target, DOT, User, TEXT("Garnet"), -1);
	OutResult.bSuccess = true;

	UE_LOG(LogTemp, Log,
		   TEXT("[ItemExecutor] Garnet: %s took %d immediate Fire damage + %.1f buildup"),
		   *Target->GetName(), OutResult.DamageDealt, BuildupPerEvent);
	UE_LOG(LogTemp, Log,
		   TEXT("[ItemExecutor] Garnet: Applied Burn DOT tail to %s (%d dmg/turn x %d turns)"),
		   *Target->GetName(), DamagePerTurnInt, Duration);
}

void UItemExecutor::ExecuteHealingEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Sapphire (reworked) — the "defy death" crystal. Branches by TARGET STATE:
	//   DEAD  → revive (ServerResurrect, the genuine resurrection; any tier now, no longer S-gated).
	//   ALIVE → grant a LastStand ward (the C2a CheckDeath intercept revives if HP hits 0 inside a
	//           tier-scaled window). Sapphire no longer heals — the heal moves to the Healing stone
	//           (C2c). NOTE: the function name ExecuteHealingEffect is now a misnomer; kept this
	//           cluster to avoid touching the dispatch site — rename in a later cleanup pass.
	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
	{
		OutResult.ErrorMessage = TEXT("Target has no character data");
		return;
	}

	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
		return;
	}

	const FString DisplayName = ItemIdentity::GetDisplayName(Id);

	// DEAD target → revive. ServerResurrect itself guards on !bIsAlive, so the bIsAlive check here
	// is what routes a dead target down this branch (and a living one to Last Stand below). Flat
	// REVIVE_HP_PERCENT (30%), not tier-scaled — tier scales the Last Stand window, not the revive.
	if (!TargetComp->bIsAlive)
	{
		const int32 ReviveHP = FMath::RoundToInt(TargetComp->MaxHP * ItemConstants::REVIVE_HP_PERCENT);
		TargetComp->ServerResurrect(ReviveHP);
		OutResult.HealingDone = ReviveHP;
		OutResult.bSuccess = true;
		UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Sapphire: Revived dead %s at %d HP"),
			   *Target->GetName(), ReviveHP);
		return;
	}

	// LIVING target → grant Last Stand: a dormant ward (LastStand effect-type) that sits idle until
	// either the bearer's HP hits 0 within the window — the C2a CheckDeath intercept consumes the
	// charge and restores HP to LAST_STAND_HP_PERCENT — or the window expires (TickDurations removes
	// it at duration 0, after which CheckDeath finds nothing and death is permanent). Window > 0 so
	// the effect is stored (a duration-0 effect would take the no-store instant lane and never arm).
	const int32 WindowTurns = CrystalEffectTable::GetLastStandWindow(Id);
	FActiveSkillEffect LastStand = FActiveSkillEffect::CreateBuff(
		FString::Printf(TEXT("%s Last Stand"), *DisplayName),
		ItemIdentity::GetEffectSourceID(Id), ESkillEffectType::LastStand,
		/*EffectValue = revive HP%*/ ItemConstants::LAST_STAND_HP_PERCENT,
		/*Duration = protection window*/ WindowTurns);
	LastStand.Charges = 1; // one death absorbed; bCanStack stays false (equal re-apply refreshes window)
	SEM->ApplyEffect(Target, LastStand, User, DisplayName, -1);

	OutResult.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Sapphire: Last Stand on %s (%.0f%% revive, %d-turn window)"),
		   *Target->GetName(), ItemConstants::LAST_STAND_HP_PERCENT, WindowTurns);
}

void UItemExecutor::ExecuteEnergyRestoreEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Citrine (Phase 2) - restores a percent of the target's MaxEP and builds
	// Lightning status on the target (no HP cost anymore).
	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
	{
		OutResult.ErrorMessage = TEXT("Target has no character data");
		return;
	}

	const float EPPercent = CrystalEffectTable::GetEPRestorePercent(Id);
	if (EPPercent <= 0.0f)
	{
		OutResult.ErrorMessage = TEXT("Invalid Citrine EP value");
		return;
	}

	const int32 EnergyAmount = FMath::Max(1, FMath::RoundToInt(TargetComp->MaxEP * EPPercent / CombatConstants::STAT_PERCENT_DIVISOR));
	const int32 EPBefore = TargetComp->CurrentEP;

	// Route through the instant lane (duration-0 EnergyRestore): ApplyEffectLogic restores via
	// ServerGainEnergy once, no store. No gate blocks EP today; future +energy mods can hook here.
	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
		return;
	}
	const FString DisplayName = ItemIdentity::GetDisplayName(Id);
	FActiveSkillEffect Restore = FActiveSkillEffect::CreateBuff(
		FString::Printf(TEXT("%s Energy"), *DisplayName),
		ItemIdentity::GetEffectSourceID(Id), ESkillEffectType::EnergyRestore,
		static_cast<float>(EnergyAmount), /*Duration*/ 0);
	SEM->ApplyEffect(Target, Restore, User, DisplayName, -1);
	OutResult.EnergyRestored = TargetComp->CurrentEP - EPBefore;

	// Lightning buildup is applied to the TARGET; the user remains the source.
	UStatusBuildupManager *SBM = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
	if (SBM)
	{
		const float BuildupPercent = CrystalEffectTable::GetElementalBuildupPercent(Id);
		const float BarMax = SBM->GetStatusBarBuildup(Target) + SBM->GetBuildupToTrigger(Target);
		const float BuildupAmount = BarMax * BuildupPercent / CombatConstants::STAT_PERCENT_DIVISOR;
		if (BuildupAmount > 0.0f)
		{
			SBM->AddStatusBuildup(User, Target, FMath::RoundToInt(BuildupAmount),
								   ESpellElement::Lightning, EPhysicalDamageType::None);
		}
	}

	OutResult.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Citrine: Restored %d EP to %s (Lightning buildup applied)"),
		   OutResult.EnergyRestored, *Target->GetName());
}

void UItemExecutor::ApplyStoneBuffEffect(AActor *User, AActor *Target, FCrystalId Id,
										 ESkillEffectType BuffType, ESkillEffectType DebuffType,
										 const FString &DisplaySuffix, FItemUseResult &OutResult)
{
	// Shared body for the standard directional stone-consumable buffs. Ally → BuffType,
	// enemy → DebuffType (IsAlly direction), at the stone's GetStoneBasePercent magnitude
	// for the flat AUGMENT_STONE_CONSUMABLE_DURATION. Byte-identical to each delegating
	// handler's prior inline body (same effect construction); only the diagnostic log line
	// is now generic. Outliers do NOT route here (different magnitude/duration source,
	// sign-encoded type, or extra-turn branch).
	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
		return;
	}

	const bool bAlly = IsAlly(User, Target);
	const ESkillEffectType EffectType = bAlly ? BuffType : DebuffType;
	const float Magnitude = CrystalEffectTable::GetStoneBasePercent(Id.Type, Id.Tier);
	const int32 Duration = CombatConstants::AUGMENT_STONE_CONSUMABLE_DURATION;

	const FString DisplayName = ItemIdentity::GetDisplayName(Id);
	FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(
		FString::Printf(TEXT("%s %s"), *DisplayName, *DisplaySuffix),
		ItemIdentity::GetEffectSourceID(Id), EffectType, Magnitude, Duration);
	Effect.Element = ItemIdentity::GetElement(Id);

	SEM->ApplyEffect(Target, Effect, User, DisplayName, -1);
	OutResult.BuffsApplied++;
	OutResult.bSuccess = true;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] %s stone: Applied %s %.0f%% for %d turns to %s"),
		   *DisplaySuffix, bAlly ? TEXT("buff") : TEXT("debuff"), Magnitude, Duration, *Target->GetName());
}

void UItemExecutor::ExecuteBonusTurnEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Emerald: grant the TARGET a PINNED bonus turn after a tier-scaled delay in normal
	// turns (F=6 … S=0). Self-target = tempo; enemy-target = force their turn (their DoTs
	// tick + they act — the gamble). Emerald itself applies NOTHING else: the DoT is
	// whatever the player already stacked. The user forfeits the current turn for free —
	// using the item IS the turn-ending action (OnActionCompleted → AdvanceToNextTurn).
	// All tiers route through ScheduleBonusTurn; S (delay 0) fires on the very next
	// scheduler step and grants TWO back-to-back turns.
	if (!Target)
	{
		OutResult.ErrorMessage = TEXT("Emerald: no target");
		return;
	}

	UTurnManager *TurnMgr = nullptr;
	if (UGameInstance *GI = GetGameInstance())
	{
		TurnMgr = GI->GetSubsystem<UTurnManager>();
	}
	if (!TurnMgr)
	{
		OutResult.ErrorMessage = TEXT("TurnManager not available");
		return;
	}

	// S is the delay-0 entry of EMERALD_BONUS_TURN_DELAY (the existing tier identification
	// in this handler) and is the only tier granting more than one turn. NOTE: delay==0 is
	// the trigger for count 2 — if a future tier were ever given delay 0, it would also
	// grant two back-to-back turns.
	constexpr int32 SRankBonusCount = 2;
	const int32 DelayTurns = CrystalEffectTable::GetEmeraldBonusTurnDelay(Id);
	const int32 Count = (DelayTurns == 0) ? SRankBonusCount : 1;

	TurnMgr->ScheduleBonusTurn(Target, DelayTurns, Count);
	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Emerald: scheduled %d pinned bonus turn(s) for %s after %d normal turn(s)"),
		   Count, *Target->GetName(), DelayTurns);

	OutResult.bSuccess = true;
}

void UItemExecutor::ExecuteTurnSpeedStoneEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// TurnSpeedStone consumable - DIRECTIONAL pure stat: buff an ally's turn speed, or
	// debuff an enemy's, at the stone's 3-15 magnitude for the flat stone duration. NO
	// turn mechanic (no extra-turn/skip) — that lives in Emerald's ExecuteBonusTurnEffect.
	ApplyStoneBuffEffect(User, Target, Id,
						 ESkillEffectType::TurnSpeedBuff, ESkillEffectType::TurnSpeedDebuff,
						 TEXT("Turn Speed"), OutResult);
}

void UItemExecutor::ExecuteStatusBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// StatusStone consumable - DIRECTIONAL: buff an ally's status multiplier, or debuff
	// an enemy's, at the stone's 3-15 magnitude for the flat stone duration. This is a
	// TRANSIENT StatusMultiplierBuff/Debuff (the Step-5b layer in AddStatusBuildup) —
	// SEPARATE from the attached StatusStone's base-stat getter (B3). Applied via SEM.
	ApplyStoneBuffEffect(User, Target, Id,
						 ESkillEffectType::StatusMultiplierBuff, ESkillEffectType::StatusMultiplierDebuff,
						 TEXT("Status Multiplier"), OutResult);
}

void UItemExecutor::ExecuteEfficiencyBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// EfficiencyStone consumable - DIRECTIONAL: buff an ally's Efficiency (cheaper casts /
	// slower BD drain / less crystal wear) or debuff an enemy's (the inverse), at the
	// stone's 3-15 magnitude for the flat stone duration. Applies the TRANSIENT
	// EfficiencyBuff/Debuff (H0); GetEffectiveEfficiencyMultiplier folds it in, so it
	// reaches BD drain + EP cost + durability with no per-consumer wiring here. The
	// getter's sign handles direction; this handler just picks buff-ally / debuff-enemy.
	ApplyStoneBuffEffect(User, Target, Id,
						 ESkillEffectType::EfficiencyBuff, ESkillEffectType::EfficiencyDebuff,
						 TEXT("Efficiency"), OutResult);
}

void UItemExecutor::ExecuteMaxHPBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// MaxHPStone consumable - DIRECTIONAL: raise an ally's MaxHP, or lower an enemy's, at
	// the stone's 3-15 magnitude for the flat stone duration. Applies the TRANSIENT
	// MaxHPBuff/Debuff (P2a); RecomputeMaxPools folds it in and the P2b subscribe trigger
	// recomputes the pool on apply/expire — no per-consumer pool wiring here. Current HP is
	// untouched (raise = no heal; expiry = overcap, no damage).
	ApplyStoneBuffEffect(User, Target, Id,
						 ESkillEffectType::MaxHPBuff, ESkillEffectType::MaxHPDebuff,
						 TEXT("Max HP"), OutResult);
}

void UItemExecutor::ExecuteMaxEPBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// MaxEPStone consumable - DIRECTIONAL: raise an ally's MaxEP, or lower an enemy's, at
	// the stone's 3-15 magnitude for the flat stone duration. Applies the TRANSIENT
	// MaxEnergyBuff/Debuff; RecomputeMaxPools folds it in via the P2b trigger. Current EP
	// untouched; a raised MaxEP also lifts the BD overload entry/ceiling (intended).
	ApplyStoneBuffEffect(User, Target, Id,
						 ESkillEffectType::MaxEnergyBuff, ESkillEffectType::MaxEnergyDebuff,
						 TEXT("Max EP"), OutResult);
}

void UItemExecutor::ExecuteRawDamageBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Damage stone consumable - DIRECTIONAL (Cluster 4): buff an ally's raw
	// (physical) damage, or debuff an enemy's, for a fixed duration. Magnitude is a
	// whole-number percent, consumed physical-only by GetStatusEffectDamageModifier.
	// CHANGED in C4: was a self-only buff before — shipped-behaviour modification.
	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
		return;
	}

	const bool bAlly = IsAlly(User, Target);
	const ESkillEffectType EffectType = bAlly ? ESkillEffectType::RawDamageBuff : ESkillEffectType::RawDamageDebuff;
	const float Magnitude = CrystalEffectTable::GetDamageStoneBasePercent(Id);
	const int32 Duration = CombatConstants::AUGMENT_STONE_CONSUMABLE_DURATION;

	const FString DisplayName = ItemIdentity::GetDisplayName(Id);
	FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(
		FString::Printf(TEXT("%s Raw Damage"), *DisplayName),
		ItemIdentity::GetEffectSourceID(Id), EffectType, Magnitude, Duration);
	Effect.Element = ItemIdentity::GetElement(Id);

	SEM->ApplyEffect(Target, Effect, User, DisplayName, -1);
	OutResult.BuffsApplied++;
	OutResult.bSuccess = true;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Damage stone: Applied %s %.0f%% raw-damage for %d turns to %s"),
		   bAlly ? TEXT("RawDamageBuff") : TEXT("RawDamageDebuff"), Magnitude, Duration, *Target->GetName());
}

void UItemExecutor::ExecuteSpellDamageBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Spell-damage stone consumable - DIRECTIONAL: buff an ally's spell (magical)
	// damage, or debuff an enemy's, for a fixed duration. Magnitude is a whole-number
	// percent, consumed SPELL-only by GetStatusEffectDamageModifier (== Spell gate).
	// The magical mirror of ExecuteRawDamageBuffEffect.
	ApplyStoneBuffEffect(User, Target, Id,
						 ESkillEffectType::SpellDamageBuff, ESkillEffectType::SpellDamageDebuff,
						 TEXT("Spell Damage"), OutResult);
}

void UItemExecutor::ExecuteResistanceBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Resistance stone consumable - DIRECTIONAL, BLANKET: raise an ally's status
	// resistance, or curse an enemy into amplified vulnerability. Uses ModifyStatusResist
	// (element-agnostic, summed into StatusBuildupManager's resistance aggregate) so it
	// matches the attached form's blanket behaviour. Direction is encoded by SIGN: ally =
	// +magnitude (more resist), enemy = -magnitude (drives resist below 0 -> amplified
	// buildup, capped at RESISTANCE_MIN = 2x). ModifyStatusResist is sign-aware classified
	// (+ -> buff, - -> debuff) so cleanse/UI read the direction correctly.
	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
		return;
	}

	const bool bAlly = IsAlly(User, Target);
	const float StonePct = CrystalEffectTable::GetStoneBasePercent(Id.Type, Id.Tier);
	const float Magnitude = bAlly ? StonePct : -StonePct;
	const int32 Duration = CombatConstants::AUGMENT_STONE_CONSUMABLE_DURATION;

	const FString DisplayName = ItemIdentity::GetDisplayName(Id);
	FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(
		FString::Printf(TEXT("%s Status Resistance"), *DisplayName),
		ItemIdentity::GetEffectSourceID(Id), ESkillEffectType::ModifyStatusResist, Magnitude, Duration);
	Effect.Element = ItemIdentity::GetElement(Id);

	SEM->ApplyEffect(Target, Effect, User, DisplayName, -1);
	OutResult.BuffsApplied++;
	OutResult.bSuccess = true;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Resistance stone: Applied ModifyStatusResist %+.0f%% for %d turns to %s (%s)"),
		   Magnitude, Duration, *Target->GetName(), bAlly ? TEXT("ally buff") : TEXT("enemy vulnerability"));
}

void UItemExecutor::ExecuteSpellSpeedBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Spell-speed stone consumable - DIRECTIONAL: buff an ally's cast speed, or debuff an
	// enemy's. Read on the cast-montage PlayRate path (PlaySpellAnimation, SpellSpeedBuff/
	// Debuff). Visual now; the COMBAT effect (defender reaction window) lands with the
	// Real-Time Defense Rework (docs/Design/RealTimeDefenseRework.md).
	ApplyStoneBuffEffect(User, Target, Id,
						 ESkillEffectType::SpellSpeedBuff, ESkillEffectType::SpellSpeedDebuff,
						 TEXT("Spell Speed"), OutResult);
}

void UItemExecutor::ExecuteActionSpeedBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Action-speed stone consumable - DIRECTIONAL: buff an ally's action speed, or debuff
	// an enemy's. Read on the ability/attack montage PlayRate path (ActionSpeedBuff/Debuff;
	// read-site added in PlayAbility/PlayAttackAnimation). Visual now; the COMBAT effect
	// lands with the Real-Time Defense Rework (docs/Design/RealTimeDefenseRework.md).
	ApplyStoneBuffEffect(User, Target, Id,
						 ESkillEffectType::ActionSpeedBuff, ESkillEffectType::ActionSpeedDebuff,
						 TEXT("Action Speed"), OutResult);
}

void UItemExecutor::ExecuteDefenseBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Directional defense consumable: buff an ally's defense, or debuff an enemy's.
	// Shared by Amber (crystal) and DefenseStone (augment stone) — both map to
	// EItemEffectType::BuffDefense.
	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
		return;
	}

	const bool bAlly = IsAlly(User, Target);
	const ESkillEffectType EffectType = bAlly ? ESkillEffectType::DefenseBuff : ESkillEffectType::DefenseDebuff;

	// DefenseStone sources the shared stone curve + flat stone duration; Amber keeps
	// its own GetBuffPercentage + GetCrystalDuration. Direction (IsAlly) is identical.
	const bool bIsStone = CrystalTypeHelpers::IsAugmentStoneType(Id.Type);
	const float Magnitude = bIsStone
		? CrystalEffectTable::GetStoneBasePercent(Id.Type, Id.Tier)
		: CrystalEffectTable::GetBuffPercentage(Id);
	const int32 Duration = bIsStone
		? CombatConstants::AUGMENT_STONE_CONSUMABLE_DURATION
		: CrystalEffectTable::GetCrystalDuration(Id);

	const FString DisplayName = ItemIdentity::GetDisplayName(Id);
	FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(
		FString::Printf(TEXT("%s Defense"), *DisplayName),
		ItemIdentity::GetEffectSourceID(Id), EffectType, Magnitude, Duration);
	Effect.Element = ItemIdentity::GetElement(Id);

	SEM->ApplyEffect(Target, Effect, User, DisplayName, -1);
	OutResult.BuffsApplied++;
	OutResult.bSuccess = true;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Amber: Applied %s %.0f%% for %d turns to %s"),
		   bAlly ? TEXT("DefenseBuff") : TEXT("DefenseDebuff"), Magnitude, Duration, *Target->GetName());
}

void UItemExecutor::ExecuteCritBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Opal (Phase 2) - buff an ally's crit chance, or debuff an enemy's.
	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
		return;
	}

	const bool bAlly = IsAlly(User, Target);
	const ESkillEffectType EffectType = bAlly ? ESkillEffectType::CritChanceBuff : ESkillEffectType::CritChanceDebuff;

	// CritStone sources the shared stone curve + flat stone duration; Opal keeps its own
	// GetCritBuffPercent + GetCrystalDuration. Direction (IsAlly) is identical — mirrors
	// the DefenseStone/Amber branch in ExecuteDefenseBuffEffect.
	const bool bIsStone = CrystalTypeHelpers::IsAugmentStoneType(Id.Type);
	const float Magnitude = bIsStone
		? CrystalEffectTable::GetStoneBasePercent(Id.Type, Id.Tier)
		: CrystalEffectTable::GetCritBuffPercent(Id);
	const int32 Duration = bIsStone
		? CombatConstants::AUGMENT_STONE_CONSUMABLE_DURATION
		: CrystalEffectTable::GetCrystalDuration(Id);

	const FString DisplayName = ItemIdentity::GetDisplayName(Id);
	FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(
		FString::Printf(TEXT("%s Crit"), *DisplayName),
		ItemIdentity::GetEffectSourceID(Id), EffectType, Magnitude, Duration);
	Effect.Element = ItemIdentity::GetElement(Id);

	SEM->ApplyEffect(Target, Effect, User, DisplayName, -1);
	OutResult.BuffsApplied++;
	OutResult.bSuccess = true;

	// TODO: S-rank stat reveal — implement in UI pass

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Opal: Applied %s %.0f%% for %d turns to %s"),
		   bAlly ? TEXT("CritChanceBuff") : TEXT("CritChanceDebuff"), Magnitude, Duration, *Target->GetName());
}

void UItemExecutor::ExecuteCritDamageBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// CritDamage stone consumable (5f-C, revised) — DIRECTIONAL like every other stone: buff an ally's
	// crit damage or debuff an enemy's, at the stone's 3-15 magnitude for the flat stone duration.
	// Ally → ModifyCritDamage ("Crit Damage Up", the buff GetCritDamageMultiplier already reads); enemy →
	// CritDamageDebuff (the paired debuff, SUBTRACTED there, final-clamped at CRIT_DMG_BASE so a crit
	// never drops below a normal hit). Matches the attached CritStone's crit-DAMAGE form and mirrors the
	// crit-CHANCE / Luck consumables' buff/debuff shape via the shared helper.
	ApplyStoneBuffEffect(User, Target, Id,
						 ESkillEffectType::ModifyCritDamage, ESkillEffectType::CritDamageDebuff,
						 TEXT("Crit Damage"), OutResult);
}

void UItemExecutor::ExecuteLuckBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// LuckStone consumable (5f-C) — DIRECTIONAL: buff an ally's Luck or debuff an enemy's, at the
	// stone's 3-15 magnitude for the flat stone duration. Applies the transient LuckBuff/LuckDebuff,
	// which GetEquipmentModifiedLuck reads as (LuckBuff - LuckDebuff) — so it lifts/lowers EVERY luck
	// consumer uniformly (crit chance, break-skip, future dodge/drops). The getter's sign handles
	// direction; this just picks buff-ally / debuff-enemy via the shared helper.
	ApplyStoneBuffEffect(User, Target, Id,
						 ESkillEffectType::LuckBuff, ESkillEffectType::LuckDebuff,
						 TEXT("Luck"), OutResult);
}

void UItemExecutor::ExecuteReflexBuffEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// ReflexStone consumable (Cluster B-6) — DIRECTIONAL: buff an ally's Reflex or debuff an enemy's, at
	// the stone's 3-15 magnitude for the flat stone duration. Applies the transient ReflexBuff/ReflexDebuff,
	// which UDefenseSystem::GetEffectiveDefenseInputWindow (B-5) reads as (ReflexBuff - ReflexDebuff) through
	// ReflexWindowGearFactor — so an ally's defense window widens, an enemy's narrows. The window read's sign
	// handles direction; this just picks buff-ally / debuff-enemy via the shared helper.
	ApplyStoneBuffEffect(User, Target, Id,
						 ESkillEffectType::ReflexBuff, ESkillEffectType::ReflexDebuff,
						 TEXT("Reflex"), OutResult);
}

void UItemExecutor::ExecuteSilenceEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Onyx - F-A is a one-shot energy drain (a percent of the target's MaxEP,
	// spent immediately on use, no tick); S-rank applies a binary Silenced gate
	// for 1 turn (blocks spellcasting via USkillEffectManager::IsSilenced).
	// All tiers additionally build the Darkness status bar on the target.
	const bool bSRank = (Id.Tier == EItemTier::S_Tier);

	if (bSRank)
	{
		USkillEffectManager *SEM = GetSkillEffectManager();
		if (!SEM)
		{
			OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
			return;
		}

		// S-rank: binary silence gate for 1 turn.
		const FString DisplayName = ItemIdentity::GetDisplayName(Id);
		FActiveSkillEffect Silence = FActiveSkillEffect::CreateBuff(
			FString::Printf(TEXT("%s Silence"), *DisplayName),
			ItemIdentity::GetEffectSourceID(Id), ESkillEffectType::Silenced, 1.0f, 1);
		Silence.Element = ItemIdentity::GetElement(Id);
		SEM->ApplyEffect(Target, Silence, User, DisplayName, -1);
		OutResult.BuffsApplied++;

		UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Onyx S: Silenced %s (binary gate, 1 turn)"),
			   *Target->GetName());
	}
	else
	{
		// F-A: one-shot energy drain. SilencePercent is a percentage of the
		// target's MaxEP — resolve it to a flat amount and spend it immediately.
		// No FActiveSkillEffect, no duration, no tick (mirrors Sapphire/Citrine).
		UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
		if (!TargetComp)
		{
			OutResult.ErrorMessage = TEXT("Target has no character data");
			return;
		}

		const float SilencePercent = CrystalEffectTable::GetSilencePercentage(Id);
		const int32 DrainAmount = FMath::RoundToInt(TargetComp->MaxEP * SilencePercent / CombatConstants::STAT_PERCENT_DIVISOR);
		const int32 EPBefore = TargetComp->CurrentEP;

		// Route the one-shot energy-lock through the instant lane (duration-0 EnergyDrain):
		// ApplyEffectLogic spends via ServerSpendEnergy once, no store. (S-rank lingering Silence
		// above is unchanged — it stays a stored, duration-1 effect.)
		USkillEffectManager *SEM = GetSkillEffectManager();
		if (!SEM)
		{
			OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
			return;
		}
		const FString DisplayName = ItemIdentity::GetDisplayName(Id);
		FActiveSkillEffect Drain = FActiveSkillEffect::CreateBuff(
			FString::Printf(TEXT("%s Energy Lock"), *DisplayName),
			ItemIdentity::GetEffectSourceID(Id), ESkillEffectType::EnergyDrain,
			static_cast<float>(DrainAmount), /*Duration*/ 0);
		SEM->ApplyEffect(Target, Drain, User, DisplayName, -1);

		UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Onyx: %s took %d EP drain (%.0f%% of MaxEP)"),
			   *Target->GetName(), EPBefore - TargetComp->CurrentEP, SilencePercent);
	}

	OutResult.bSuccess = true;

	// All tiers build the Darkness status bar on the target.
	UStatusBuildupManager *SBM = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
	if (SBM)
	{
		const float BarMax = SBM->GetStatusBarBuildup(Target) + SBM->GetBuildupToTrigger(Target);
		const float BuildupAmount = BarMax * CrystalEffectTable::GetElementalBuildupPercent(Id) / CombatConstants::STAT_PERCENT_DIVISOR;
		if (BuildupAmount > 0.0f)
		{
			SBM->AddStatusBuildup(User, Target, FMath::RoundToInt(BuildupAmount),
								   ESpellElement::Darkness, EPhysicalDamageType::None);
		}
	}
}

void UItemExecutor::ExecuteGambleEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Amethyst (Phase 2) - roll buff vs debuff against the tier's buff chance,
	// then apply a random effect from the matching pool to the target.
	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
		return;
	}

	OutResult.bWasGamble = true;

	const TArray<ESkillEffectType> BuffPool = {
		ESkillEffectType::SpellDamageBuff,
		ESkillEffectType::RawDamageBuff,
		ESkillEffectType::DefenseBuff,
		ESkillEffectType::CritChanceBuff,
		ESkillEffectType::TurnSpeedBuff,
		ESkillEffectType::ResistanceBuff,
		ESkillEffectType::LuckBuff};
	const TArray<ESkillEffectType> DebuffPool = {
		ESkillEffectType::SpellDamageDebuff,
		ESkillEffectType::RawDamageDebuff,
		ESkillEffectType::DefenseDebuff,
		ESkillEffectType::CritChanceDebuff,
		ESkillEffectType::TurnSpeedDebuff,
		ESkillEffectType::ResistanceDebuff,
		ESkillEffectType::LuckDebuff};

	const bool bIsBuff = FMath::FRand() < (CrystalEffectTable::GetBuffChancePercent(Id) / CombatConstants::STAT_PERCENT_DIVISOR);
	OutResult.bGambleWon = bIsBuff;

	const TArray<ESkillEffectType> &Pool = bIsBuff ? BuffPool : DebuffPool;
	const ESkillEffectType ChosenType = Pool[FMath::RandRange(0, Pool.Num() - 1)];

	const float Magnitude = CrystalEffectTable::GetGambleMagnitudePercent(Id);
	const int32 Duration = CrystalEffectTable::GetGambleDuration(Id);

	FActiveSkillEffect GambleEffect = FActiveSkillEffect::CreateBuff(
		TEXT("Gamble Result"), ItemIdentity::GetEffectSourceID(Id), ChosenType, Magnitude, Duration);

	// Resistance rolls target a RANDOM category across all 12 buildup categories
	// (9 elements + 3 physical types) — replacing the old hard-coded Void, which
	// made a gambled resistance buff/debuff only ever apply to Void attacks.
	// Element-keyed effects leave PhysicalType None; physical-keyed effects set
	// PhysicalType and leave Element Generic (unused). Non-resistance stat rolls
	// don't read Element/PhysicalType, so they keep CreateBuff's defaults.
	if (ChosenType == ESkillEffectType::ResistanceBuff || ChosenType == ESkillEffectType::ResistanceDebuff)
	{
		static const ESpellElement kGambleElements[9] = {
			ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth,
			ESpellElement::Wind, ESpellElement::Light, ESpellElement::Darkness,
			ESpellElement::Lightning, ESpellElement::Void, ESpellElement::Reality}; // 9 — excl. the non-element sentinels Generic/None
		static const EPhysicalDamageType kGamblePhysicals[3] = {
			EPhysicalDamageType::Slash, EPhysicalDamageType::Pierce, EPhysicalDamageType::Impact};

		const int32 cat = FMath::RandRange(0, 11);
		if (cat < 9)
		{
			GambleEffect.Element = kGambleElements[cat]; // element-keyed; PhysicalType stays None
		}
		else
		{
			GambleEffect.PhysicalType = kGamblePhysicals[cat - 9]; // physical-keyed
			GambleEffect.Element = ESpellElement::None;            // Element unused for physical effects
		}
	}

	SEM->ApplyEffect(Target, GambleEffect, User, ItemIdentity::GetDisplayName(Id), -1);
	OutResult.BuffsApplied++;
	OutResult.bSuccess = true;

	// Amethyst also builds the Void status bar on the target.
	UStatusBuildupManager *SBM = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
	if (SBM)
	{
		const float BarMax = SBM->GetStatusBarBuildup(Target) + SBM->GetBuildupToTrigger(Target);
		const float BuildupAmount = BarMax * CrystalEffectTable::GetElementalBuildupPercent(Id) / CombatConstants::STAT_PERCENT_DIVISOR;
		if (BuildupAmount > 0.0f)
		{
			SBM->AddStatusBuildup(User, Target, FMath::RoundToInt(BuildupAmount),
								   ESpellElement::Void, EPhysicalDamageType::None);
		}
	}

	OutResult.GambleOutcome = FString::Printf(TEXT("%s %.0f%% for %d turns"),
											  bIsBuff ? TEXT("Buff") : TEXT("Debuff"), Magnitude, Duration);
	OnGambleResult.Broadcast(User, bIsBuff, OutResult.GambleOutcome);

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Amethyst gamble on %s: %s"),
		   *Target->GetName(), *OutResult.GambleOutcome);
}

void UItemExecutor::ExecuteCleanseEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Iolite (Phase 2) - cleanse debuffs from an ally, or strip buffs from an enemy.
	// GetEffectsToRemoveCount() == 99 means "remove all" (fixes the old S-rank bug).
	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
		return;
	}

	const bool bRemoveDebuffs = IsAlly(User, Target);
	const int32 Count = CrystalEffectTable::GetEffectsToRemoveCount(Id);

	// GetActiveEffects returns a copy — safe to iterate while removing by ID.
	const TArray<FActiveSkillEffect> Effects = SEM->GetActiveEffects(Target);
	int32 Removed = 0;
	for (const FActiveSkillEffect &Effect : Effects)
	{
		if (Count < ItemConstants::IOLITE_REMOVE_ALL && Removed >= Count)
		{
			break;
		}
		const bool bMatch = bRemoveDebuffs ? Effect.IsDebuff() : Effect.IsBuff();
		if (bMatch)
		{
			SEM->RemoveEffectByID(Target, Effect.EffectID);
			Removed++;
		}
	}

	OutResult.DebuffsRemoved = Removed;
	OutResult.bSuccess = true;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Iolite: Removed %d %s from %s"),
		   Removed, bRemoveDebuffs ? TEXT("debuff(s)") : TEXT("buff(s)"), *Target->GetName());
}

void UItemExecutor::ExecuteStatusClearEffect(AActor *User, AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Quartz (Phase 2) - clears a percent of the target's status bar, then grants
	// element-specific protection for the bar's pending (last-hit) element:
	//   F-A : ResistanceBuff   — reduces further buildup of that element
	//   S   : GrantXxxImmunity — fully absorbs further buildup of that element
	UStatusBuildupManager *SBM = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!SBM || !TargetComp)
	{
		OutResult.ErrorMessage = TEXT("StatusBuildupManager or target unavailable");
		return;
	}

	const float StatusClearPercent = CrystalEffectTable::GetStatusClearPercent(Id);
	const int32 ResistanceDurationTurns = CrystalEffectTable::GetResistanceDuration(Id);
	const float ClearFraction = StatusClearPercent / CombatConstants::STAT_PERCENT_DIVISOR;
	const ESpellElement ResistElement = SBM->GetPendingElement(Target);
	SBM->ReduceStatusBuildup(Target, ClearFraction);

	// Grant protection for the cleared element (skip None — no element to resist).
	bool bAppliedImmunity = false;
	if (ResistElement != ESpellElement::None)
	{
		if (USkillEffectManager *SEM = GetSkillEffectManager())
		{
			const ESkillEffectType ImmunityType = GetElementImmunityType(ResistElement);
			const bool bUseImmunity = (Id.Tier == EItemTier::S_Tier) && (ImmunityType != ESkillEffectType::None);

			const ESkillEffectType EffectType = bUseImmunity ? ImmunityType : ESkillEffectType::ResistanceBuff;
			const float Magnitude = bUseImmunity ? 1.0f : ItemConstants::QUARTZ_RESIST_PERCENT;
			// ResistElement (the per-target, runtime protected element) is folded into the ID
			// so protections for DIFFERENT elements coexist on one target instead of colliding.
			// For F–A tiers EffectType is the generic ResistanceBuff for every element, so without
			// ResistElement a Fire-resist and a Water-resist would share an ID and overwrite.
			const int32 EffectID = static_cast<int32>(GetTypeHash(Target))
								 ^ static_cast<int32>(GetTypeHash(ResistElement))
								 ^ static_cast<int32>(EffectType);

			FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(
				bUseImmunity ? TEXT("Elemental Immunity") : TEXT("Elemental Resistance"),
				EffectID, EffectType, Magnitude, ResistanceDurationTurns);
			Effect.Element = ResistElement;
			SEM->ApplyEffect(Target, Effect, User, TEXT("Quartz"), -1);
			OutResult.BuffsApplied++;
			bAppliedImmunity = bUseImmunity;
		}
	}

	OutResult.bSuccess = true;
	UE_LOG(LogTemp, Log,
		   TEXT("[ItemExecutor] Quartz: Cleared %.0f%% status bar on %s, applied %s %s for %d turns"),
		   StatusClearPercent, *Target->GetName(),
		   *UEnum::GetValueAsString(ResistElement),
		   bAppliedImmunity ? TEXT("immunity") : TEXT("resistance"),
		   ResistanceDurationTurns);
}

// ========================================
// BONUS HANDLERS
// ========================================

void UItemExecutor::ApplyBrokenDarknessBonus(AActor *Target, FCrystalId Id, FItemUseResult &OutResult)
{
	// Broken Darkness character absorbs the crystal's elemental energy when one
	// is used on them — gains EP scaled as a percentage of MaxEP by the crystal's
	// tier. The caller has already confirmed Target is a Broken Darkness character.
	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
		return;

	const float Percent = CrystalEffectTable::GetBrokenDarknessEnergyPercent(Id);
	if (Percent <= 0.0f)
		return;

	const float BonusEnergy = TargetComp->MaxEP * Percent;
	if (BonusEnergy <= 0.0f)
		return;

	// Route through the BD manager's absorption path. ServerGainEnergy is
	// suppressed for BD characters (passive regen is excluded), so a direct
	// call there grants nothing — GrantAbsorptionEnergy is the BD gain path.
	UBrokenDarknessManager *BDManager = Target->FindComponentByClass<UBrokenDarknessManager>();
	if (!BDManager)
		return;

	// Three behaviours by the crystal's element:
	//   - Reality (Iolite) → DRAIN the tier-scaled energy + revert the active pool to base
	//     Darkness (cleanse — unmake the stolen element); character stays BD.
	//   - None (Quartz / non-elemental) → NO absorption interaction at all (no energy, no
	//     rotation, no drain) — skip entirely.
	//   - Real element → grant energy + rotate the active pool to it.
	const ESpellElement Elem = ItemIdentity::GetElement(Id);
	const int32 EPBefore = TargetComp->CurrentEP;

	if (Elem == ESpellElement::Reality)
	{
		BDManager->DrainAndRevertToBase(BonusEnergy);
	}
	else if (Elem == ESpellElement::None)
	{
		// Non-elemental crystal: intentionally no-op (EP unchanged → gained stays 0).
	}
	else
	{
		BDManager->GrantAbsorptionEnergy(BonusEnergy, Elem);
	}

	// Honest delta: positive = granted, negative = drained (Reality), 0 = no interaction (None).
	OutResult.BrokenDarknessEnergyGained = TargetComp->CurrentEP - EPBefore;

	UE_LOG(LogTemp, Log,
		   TEXT("[ItemExecutor] Broken Darkness crystal (%s): %s energy delta %d (%.0f%% of MaxEP %d)"),
		   *UEnum::GetValueAsString(Elem), *Target->GetName(), OutResult.BrokenDarknessEnergyGained,
		   Percent * 100.0f, TargetComp->MaxEP);
}

// ========================================
// HELPERS
// ========================================

UCharacterDataComponent *UItemExecutor::GetCharacterDataComponent(AActor *Actor) const
{
	if (!Actor)
		return nullptr;
	return Actor->FindComponentByClass<UCharacterDataComponent>();
}

USkillEffectManager *UItemExecutor::GetSkillEffectManager() const
{
	if (!SkillEffectManagerRef)
	{
		if (UGameInstance *GI = Cast<UGameInstance>(GetGameInstance()))
		{
			const_cast<UItemExecutor *>(this)->SkillEffectManagerRef =
				GI->GetSubsystem<USkillEffectManager>();
		}
	}
	return SkillEffectManagerRef;
}

bool UItemExecutor::IsBrokenDarknessCharacter(AActor *Actor) const
{
	UCharacterDataComponent *Comp = GetCharacterDataComponent(Actor);
	if (!Comp)
		return false;
	return Comp->IsBrokenDarkness();
}

bool UItemExecutor::IsAlly(AActor *User, AActor *Target) const
{
	if (!User || !Target)
	{
		return false;
	}
	if (User == Target)
	{
		return true;
	}

	UTurnManager *TurnMgr = nullptr;
	if (UGameInstance *GI = GetGameInstance())
	{
		TurnMgr = GI->GetSubsystem<UTurnManager>();
	}
	if (!TurnMgr)
	{
		return false;
	}

	const int32 UserTeam = TurnMgr->GetActorTeam(User);
	const int32 TargetTeam = TurnMgr->GetActorTeam(Target);
	return UserTeam >= 0 && UserTeam == TargetTeam;
}

ESkillEffectType UItemExecutor::GetElementImmunityType(ESpellElement Element)
{
	switch (Element)
	{
	case ESpellElement::Fire:      return ESkillEffectType::GrantFireImmunity;
	case ESpellElement::Water:     return ESkillEffectType::GrantWaterImmunity;
	case ESpellElement::Earth:     return ESkillEffectType::GrantEarthImmunity;
	case ESpellElement::Wind:      return ESkillEffectType::GrantWindImmunity;
	case ESpellElement::Light:     return ESkillEffectType::GrantLightImmunity;
	case ESpellElement::Darkness:  return ESkillEffectType::GrantDarknessImmunity;
	case ESpellElement::Lightning: return ESkillEffectType::GrantLightningImmunity;
	case ESpellElement::Void:      return ESkillEffectType::GrantVoidImmunity;
	case ESpellElement::Reality:   return ESkillEffectType::GrantRealityImmunity;
	case ESpellElement::Generic:
	default:
		return ESkillEffectType::None;
	}
}
