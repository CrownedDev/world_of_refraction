// Copyright Epic Games, Inc. All Rights Reserved.

#include "ItemExecutor.h"
#include "ItemData.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "BrokenDarknessManager.h"
#include "SkillEffectManager.h"
#include "StatusBuildupManager.h"
#include "TurnManager.h"
#include "ActiveSkillEffect.h"

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

FItemUseResult UItemExecutor::UseItem(AActor *User, UItemData *Item, AActor *Target)
{
	FItemUseResult Result;

	if (!User || !Item)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid user or item");
		return Result;
	}

	// Default target to user if not specified (for self-target items)
	if (!Target)
	{
		Target = User;
	}

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] %s using %s on %s"),
		   *User->GetName(), *Item->GetFullItemName(), *Target->GetName());

	// Execute based on effect type
	EItemEffectType EffectType = Item->GetPrimaryEffectType();

	switch (EffectType)
	{
	case EItemEffectType::Damage:
		ExecuteDamageEffect(User, Target, Item, Result);
		break;

	case EItemEffectType::Healing:
		ExecuteHealingEffect(User, Target, Item, Result);
		break;

	case EItemEffectType::EnergyRestore:
		ExecuteEnergyRestoreEffect(User, Target, Item, Result);
		break;

	case EItemEffectType::BuffSpeed:
		ExecuteSpeedBuffEffect(User, Target, Item, Result);
		break;

	case EItemEffectType::BuffDefense:
		ExecuteDefenseBuffEffect(User, Target, Item, Result);
		break;

	case EItemEffectType::BuffCrit:
		ExecuteCritBuffEffect(User, Target, Item, Result);
		break;

	case EItemEffectType::Silence:
		ExecuteSilenceEffect(User, Target, Item, Result);
		break;

	case EItemEffectType::Gamble:
		ExecuteGambleEffect(User, Target, Item, Result);
		break;

	case EItemEffectType::Cleanse:
		ExecuteCleanseEffect(User, Target, Item, Result);
		break;

	case EItemEffectType::StatusClear:
		ExecuteStatusClearEffect(User, Target, Item, Result);
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
		ApplyBrokenDarknessBonus(Target, Item, Result);
	}

	Result.bSuccess = true;

	// Broadcast event
	OnItemUsed.Broadcast(User, Item, Result);

	return Result;
}

FItemUseResult UItemExecutor::UseItemMultiTarget(AActor *User, UItemData *Item, const TArray<AActor *> &Targets)
{
	FItemUseResult CombinedResult;
	CombinedResult.bSuccess = true;

	for (AActor *Target : Targets)
	{
		if (!Target)
			continue;

		FItemUseResult SingleResult = UseItem(User, Item, Target);

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

void UItemExecutor::ExecuteDamageEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
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

	const float DamagePercent = Item->GetDOTDamagePercent();
	const int32 Duration = Item->GetDOTDuration();

	if (DamagePercent <= 0.0f || Duration <= 0)
	{
		OutResult.ErrorMessage = TEXT("Invalid Garnet DOT values");
		return;
	}

	const float DamagePerTurn = TargetComp->MaxHP * DamagePercent / 100.0f;
	const int32 DamagePerTurnInt = FMath::Max(1, FMath::RoundToInt(DamagePerTurn));

	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager unavailable");
		return;
	}

	// Immediate Fire hit on use — equal to one DOT tick. Unlike DOT ticks (which
	// SkillEffectManager clamps to leave 1 HP, "DOTs can't kill"), this is a
	// direct damage event and CAN drop the target to 0.
	const int32 HPBefore = TargetComp->CurrentHP;
	TargetComp->ServerTakeDamage(DamagePerTurnInt);
	OutResult.DamageDealt = HPBefore - TargetComp->CurrentHP;

	// Apply the fire DOT tail via SkillEffectManager — registration is unchanged;
	// first tick lands at the end of the target's next turn.
	const int32 EffectID = static_cast<int32>(GetTypeHash(User)) ^ static_cast<int32>(GetTypeHash(Target)) ^ static_cast<int32>(ESkillEffectType::DOT);
	FActiveSkillEffect DOT = FActiveSkillEffect::CreateDOT(
		TEXT("Burn"), EffectID, DamagePerTurnInt, Duration, ESpellElement::Fire);

	SEM->ApplyEffect(Target, DOT, User, TEXT("Garnet"), -1);
	OutResult.bSuccess = true;

	// Fire status buildup for the immediate hit's damage event, using the shared
	// elemental-buildup table. NOTE: DOT ticks do not yet contribute buildup —
	// that path lives in SkillEffectManager (out of scope for this change).
	float BuildupApplied = 0.0f;
	UStatusBuildupManager *SBM = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
	if (SBM)
	{
		const float BarMax = SBM->GetStatusBarBuildup(Target) + SBM->GetBuildupToTrigger(Target);
		BuildupApplied = BarMax * Item->GetElementalBuildupPercent() / 100.0f;
		SBM->AddStatusBuildup(User, Target, FMath::RoundToInt(BuildupApplied),
							   ESpellElement::Fire, EPhysicalDamageType::None);
	}

	UE_LOG(LogTemp, Log,
		   TEXT("[ItemExecutor] Garnet: %s took %d immediate Fire damage + %.1f buildup"),
		   *Target->GetName(), OutResult.DamageDealt, BuildupApplied);
	UE_LOG(LogTemp, Log,
		   TEXT("[ItemExecutor] Garnet: Applied Burn DOT tail to %s (%d dmg/turn x %d turns)"),
		   *Target->GetName(), DamagePerTurnInt, Duration);
}

void UItemExecutor::ExecuteHealingEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Sapphire - percentage-based water healing (Phase 2). S-tier additionally
	// revives a dead target at 30% MaxHP; lower tiers heal living targets only.
	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
	{
		OutResult.ErrorMessage = TEXT("Target has no character data");
		return;
	}

	const float HealPercent = Item->GetHealPercent();
	if (HealPercent <= 0.0f)
	{
		OutResult.ErrorMessage = TEXT("Invalid Sapphire heal value");
		return;
	}

	const int32 HealAmount = FMath::Max(1, FMath::RoundToInt(TargetComp->MaxHP * HealPercent / 100.0f));

	// S-rank: revive a dead target at 30% MaxHP
	if (!TargetComp->bIsAlive && Item->Tier == EItemTier::S_Tier)
	{
		const int32 ReviveHP = FMath::RoundToInt(TargetComp->MaxHP * 0.3f);
		TargetComp->ServerResurrect(ReviveHP);
		OutResult.HealingDone = ReviveHP;
		OutResult.bSuccess = true;
		UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Sapphire S: Revived %s at %d HP"),
			   *Target->GetName(), ReviveHP);
		return;
	}

	// Non-S Sapphire cannot affect a dead target
	if (!TargetComp->bIsAlive)
	{
		OutResult.ErrorMessage = TEXT("Cannot heal dead target with non-S Sapphire");
		return;
	}

	// Living target — heal a percent of MaxHP
	const int32 HPBefore = TargetComp->CurrentHP;
	TargetComp->ServerHeal(HealAmount);
	OutResult.HealingDone = TargetComp->CurrentHP - HPBefore;
	OutResult.bSuccess = true;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Sapphire: Healed %s for %d HP (%.0f%%)"),
		   *Target->GetName(), OutResult.HealingDone, HealPercent);
}

void UItemExecutor::ExecuteEnergyRestoreEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Citrine (Phase 2) - restores a percent of the target's MaxEP and builds
	// Lightning status on the target (no HP cost anymore).
	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
	{
		OutResult.ErrorMessage = TEXT("Target has no character data");
		return;
	}

	const float EPPercent = Item->GetEPRestorePercent();
	if (EPPercent <= 0.0f)
	{
		OutResult.ErrorMessage = TEXT("Invalid Citrine EP value");
		return;
	}

	const int32 EnergyAmount = FMath::Max(1, FMath::RoundToInt(TargetComp->MaxEP * EPPercent / 100.0f));
	const int32 EPBefore = TargetComp->CurrentEP;
	TargetComp->ServerGainEnergy(EnergyAmount);
	OutResult.EnergyRestored = TargetComp->CurrentEP - EPBefore;

	// Lightning buildup is applied to the TARGET; the user remains the source.
	UStatusBuildupManager *SBM = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
	if (SBM)
	{
		const float BuildupPercent = Item->GetElementalBuildupPercent();
		const float BarMax = SBM->GetStatusBarBuildup(Target) + SBM->GetBuildupToTrigger(Target);
		const float BuildupAmount = BarMax * BuildupPercent / 100.0f;
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

void UItemExecutor::ExecuteSpeedBuffEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Emerald (Phase 2) - F-A apply a turn-speed buff; S grants an extra turn.
	if (Item->Tier == EItemTier::S_Tier)
	{
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
		TurnMgr->RequestExtraTurn(Target);
		OutResult.bSuccess = true;
		UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Emerald S: Granted %s an extra turn"), *Target->GetName());
		return;
	}

	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
		return;
	}

	const float BuffPercent = Item->GetSpeedBuffPercent();
	const int32 Duration = Item->GetCrystalDuration();

	FActiveSkillEffect SpeedBuff = FActiveSkillEffect::CreateBuff(
		FString::Printf(TEXT("%s Speed"), *Item->GetFullItemName()),
		Item->GetUniqueID(), ESkillEffectType::TurnSpeedBuff, BuffPercent, Duration);
	SpeedBuff.Element = Item->GetAssociatedElement();

	SEM->ApplyEffect(Target, SpeedBuff, User, Item->GetFullItemName(), -1);
	OutResult.BuffsApplied++;
	OutResult.bSuccess = true;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Emerald: Applied %.0f%% turn-speed buff for %d turns to %s"),
		   BuffPercent, Duration, *Target->GetName());
}

void UItemExecutor::ExecuteDefenseBuffEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Amber (Phase 2) - buff an ally's defense, or debuff an enemy's.
	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
		return;
	}

	const bool bAlly = IsAlly(User, Target);
	const ESkillEffectType EffectType = bAlly ? ESkillEffectType::DefenseBuff : ESkillEffectType::DefenseDebuff;
	const float Magnitude = Item->GetBuffPercentage();
	const int32 Duration = Item->GetCrystalDuration();

	FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(
		FString::Printf(TEXT("%s Defense"), *Item->GetFullItemName()),
		Item->GetUniqueID(), EffectType, Magnitude, Duration);
	Effect.Element = Item->GetAssociatedElement();

	SEM->ApplyEffect(Target, Effect, User, Item->GetFullItemName(), -1);
	OutResult.BuffsApplied++;
	OutResult.bSuccess = true;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Amber: Applied %s %.0f%% for %d turns to %s"),
		   bAlly ? TEXT("DefenseBuff") : TEXT("DefenseDebuff"), Magnitude, Duration, *Target->GetName());
}

void UItemExecutor::ExecuteCritBuffEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
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
	const float Magnitude = Item->GetCritBuffPercent();
	const int32 Duration = Item->GetCrystalDuration();

	FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(
		FString::Printf(TEXT("%s Crit"), *Item->GetFullItemName()),
		Item->GetUniqueID(), EffectType, Magnitude, Duration);
	Effect.Element = Item->GetAssociatedElement();

	SEM->ApplyEffect(Target, Effect, User, Item->GetFullItemName(), -1);
	OutResult.BuffsApplied++;
	OutResult.bSuccess = true;

	// TODO: S-rank stat reveal — implement in UI pass

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Opal: Applied %s %.0f%% for %d turns to %s"),
		   bAlly ? TEXT("CritChanceBuff") : TEXT("CritChanceDebuff"), Magnitude, Duration, *Target->GetName());
}

void UItemExecutor::ExecuteSilenceEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Onyx - F-A is a one-shot energy drain (a percent of the target's MaxEP,
	// spent immediately on use, no tick); S-rank applies a binary Silenced gate
	// for 1 turn (blocks spellcasting via USkillEffectManager::IsSilenced).
	// All tiers additionally build the Darkness status bar on the target.
	const bool bSRank = (Item->Tier == EItemTier::S_Tier);

	if (bSRank)
	{
		USkillEffectManager *SEM = GetSkillEffectManager();
		if (!SEM)
		{
			OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
			return;
		}

		// S-rank: binary silence gate for 1 turn.
		FActiveSkillEffect Silence = FActiveSkillEffect::CreateBuff(
			FString::Printf(TEXT("%s Silence"), *Item->GetFullItemName()),
			Item->GetUniqueID(), ESkillEffectType::Silenced, 1.0f, 1);
		Silence.Element = Item->GetAssociatedElement();
		SEM->ApplyEffect(Target, Silence, User, Item->GetFullItemName(), -1);
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

		const float SilencePercent = Item->GetSilencePercentage();
		const int32 DrainAmount = FMath::RoundToInt(TargetComp->MaxEP * SilencePercent / 100.0f);
		const int32 EPBefore = TargetComp->CurrentEP;
		TargetComp->ServerSpendEnergy(DrainAmount);

		UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Onyx: %s took %d EP drain (%.0f%% of MaxEP)"),
			   *Target->GetName(), EPBefore - TargetComp->CurrentEP, SilencePercent);
	}

	OutResult.bSuccess = true;

	// All tiers build the Darkness status bar on the target.
	UStatusBuildupManager *SBM = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
	if (SBM)
	{
		const float BarMax = SBM->GetStatusBarBuildup(Target) + SBM->GetBuildupToTrigger(Target);
		const float BuildupAmount = BarMax * Item->GetElementalBuildupPercent() / 100.0f;
		if (BuildupAmount > 0.0f)
		{
			SBM->AddStatusBuildup(User, Target, FMath::RoundToInt(BuildupAmount),
								   ESpellElement::Darkness, EPhysicalDamageType::None);
		}
	}
}

void UItemExecutor::ExecuteGambleEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
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

	const bool bIsBuff = FMath::FRand() < (Item->GetBuffChancePercent() / 100.0f);
	OutResult.bGambleWon = bIsBuff;

	const TArray<ESkillEffectType> &Pool = bIsBuff ? BuffPool : DebuffPool;
	const ESkillEffectType ChosenType = Pool[FMath::RandRange(0, Pool.Num() - 1)];

	const float Magnitude = Item->GetGambleMagnitudePercent();
	const int32 Duration = Item->GetGambleDuration();

	FActiveSkillEffect GambleEffect = FActiveSkillEffect::CreateBuff(
		TEXT("Gamble Result"), Item->GetUniqueID(), ChosenType, Magnitude, Duration);
	GambleEffect.Element = ESpellElement::Void;

	SEM->ApplyEffect(Target, GambleEffect, User, Item->GetFullItemName(), -1);
	OutResult.BuffsApplied++;
	OutResult.bSuccess = true;

	// Amethyst also builds the Void status bar on the target.
	UStatusBuildupManager *SBM = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
	if (SBM)
	{
		const float BarMax = SBM->GetStatusBarBuildup(Target) + SBM->GetBuildupToTrigger(Target);
		const float BuildupAmount = BarMax * Item->GetElementalBuildupPercent() / 100.0f;
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

void UItemExecutor::ExecuteCleanseEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
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
	const int32 Count = Item->GetEffectsToRemoveCount();

	// GetActiveEffects returns a copy — safe to iterate while removing by ID.
	const TArray<FActiveSkillEffect> Effects = SEM->GetActiveEffects(Target);
	int32 Removed = 0;
	for (const FActiveSkillEffect &Effect : Effects)
	{
		if (Count < 99 && Removed >= Count)
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

void UItemExecutor::ExecuteStatusClearEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
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

	const float ClearFraction = Item->GetStatusClearPercent() / 100.0f;
	const ESpellElement ResistElement = SBM->GetPendingElement(Target);
	SBM->ReduceStatusBuildup(Target, ClearFraction);

	// Grant protection for the cleared element (skip Generic — no element to resist).
	bool bAppliedImmunity = false;
	if (ResistElement != ESpellElement::Generic)
	{
		if (USkillEffectManager *SEM = GetSkillEffectManager())
		{
			const int32 Duration = Item->GetResistanceDuration();
			const ESkillEffectType ImmunityType = GetElementImmunityType(ResistElement);
			const bool bUseImmunity = (Item->Tier == EItemTier::S_Tier) && (ImmunityType != ESkillEffectType::None);

			const ESkillEffectType EffectType = bUseImmunity ? ImmunityType : ESkillEffectType::ResistanceBuff;
			const float Magnitude = bUseImmunity ? 1.0f : 30.0f;
			const int32 EffectID = static_cast<int32>(GetTypeHash(Target)) ^ static_cast<int32>(EffectType);

			FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(
				bUseImmunity ? TEXT("Elemental Immunity") : TEXT("Elemental Resistance"),
				EffectID, EffectType, Magnitude, Duration);
			Effect.Element = ResistElement;
			SEM->ApplyEffect(Target, Effect, User, TEXT("Quartz"), -1);
			OutResult.BuffsApplied++;
			bAppliedImmunity = bUseImmunity;
		}
	}

	OutResult.bSuccess = true;
	UE_LOG(LogTemp, Log,
		   TEXT("[ItemExecutor] Quartz: Cleared %.0f%% status bar on %s, applied %s %s for %d turns"),
		   Item->GetStatusClearPercent(), *Target->GetName(),
		   *UEnum::GetValueAsString(ResistElement),
		   bAppliedImmunity ? TEXT("immunity") : TEXT("resistance"),
		   Item->GetResistanceDuration());
}

// ========================================
// BONUS HANDLERS
// ========================================

void UItemExecutor::ApplyBrokenDarknessBonus(AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Broken Darkness character absorbs the crystal's elemental energy when one
	// is used on them — gains EP scaled by the crystal's tier. The caller has
	// already confirmed Target is a Broken Darkness character.
	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
		return;

	int32 BonusEnergy = Item->GetBrokenDarknessEnergyBonus();
	if (BonusEnergy <= 0)
		return;

	// Route through the BD manager's absorption path. ServerGainEnergy is
	// suppressed for BD characters (passive regen is excluded), so a direct
	// call there grants nothing — GrantAbsorptionEnergy is the BD gain path.
	UBrokenDarknessManager *BDManager = Target->FindComponentByClass<UBrokenDarknessManager>();
	if (!BDManager)
		return;

	int32 EPBefore = TargetComp->CurrentEP;
	BDManager->GrantAbsorptionEnergy(static_cast<float>(BonusEnergy));
	OutResult.BrokenDarknessEnergyGained = TargetComp->CurrentEP - EPBefore;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Broken Darkness absorption: %s gained %d energy"),
		   *Target->GetName(), OutResult.BrokenDarknessEnergyGained);
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
	case ESpellElement::BrokenDarkness:
	default:
		return ESkillEffectType::None;
	}
}
