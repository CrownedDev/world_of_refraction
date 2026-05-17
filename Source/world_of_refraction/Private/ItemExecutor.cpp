// Copyright Epic Games, Inc. All Rights Reserved.

#include "ItemExecutor.h"
#include "ItemData.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
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

	// Apply character-specific bonuses (all items give these)
	ApplyGenericBonus(User, Item, Result);
	ApplyBrokenDarknessBonus(User, Item, Result);

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
	// Garnet - percentage-based fire DOT (Phase 2). Damage per turn is a percent
	// of the target's MaxHP; all tiers apply the DOT (no instant-damage path).
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

	// Apply fire DOT via SkillEffectManager
	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager unavailable");
		return;
	}

	const int32 EffectID = static_cast<int32>(GetTypeHash(User)) ^ static_cast<int32>(GetTypeHash(Target)) ^ static_cast<int32>(ESkillEffectType::DOT);
	FActiveSkillEffect DOT = FActiveSkillEffect::CreateDOT(
		TEXT("Burn"), EffectID, DamagePerTurnInt, Duration, ESpellElement::Fire);

	SEM->ApplyEffect(Target, DOT, User, TEXT("Garnet"), -1);
	OutResult.bSuccess = true;

	// Garnet also builds the Fire status bar on application (not per tick),
	// using the shared elemental-buildup table.
	UStatusBuildupManager *SBM = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
	if (SBM)
	{
		const float BarMax = SBM->GetStatusBarBuildup(Target) + SBM->GetBuildupToTrigger(Target);
		const float BuildupAmount = BarMax * Item->GetElementalBuildupPercent() / 100.0f;
		SBM->AddStatusBuildup(User, Target, FMath::RoundToInt(BuildupAmount),
							   ESpellElement::Fire, EPhysicalDamageType::None);
	}

	UE_LOG(LogTemp, Log,
		   TEXT("[ItemExecutor] Garnet: Applied Burn DOT to %s (%d dmg/turn x %d turns)"),
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
	// Onyx - F-A apply a percentage energy-lock (EnergyDrain); S-rank applies a
	// binary Silenced effect (blocks spellcasting via USkillEffectManager::IsSilenced).
	// All tiers additionally build the Darkness status bar on the target.
	USkillEffectManager *SEM = GetSkillEffectManager();
	if (!SEM)
	{
		OutResult.ErrorMessage = TEXT("SkillEffectManager not available");
		return;
	}

	const float SilencePercent = Item->GetSilencePercentage();
	const bool bSRank = (Item->Tier == EItemTier::S_Tier);

	if (bSRank)
	{
		// S-rank: full binary silence for 1 turn.
		FActiveSkillEffect Silence = FActiveSkillEffect::CreateBuff(
			FString::Printf(TEXT("%s Silence"), *Item->GetFullItemName()),
			Item->GetUniqueID(), ESkillEffectType::Silenced, 1.0f, 1);
		Silence.Element = Item->GetAssociatedElement();
		SEM->ApplyEffect(Target, Silence, User, Item->GetFullItemName(), -1);
	}
	else
	{
		// F-A: percentage energy-lock via the EnergyDrain effect type.
		const int32 Duration = Item->GetSilenceDurationNew();
		FActiveSkillEffect EnergyLock = FActiveSkillEffect::CreateBuff(
			FString::Printf(TEXT("%s Silence"), *Item->GetFullItemName()),
			Item->GetUniqueID(), ESkillEffectType::EnergyDrain, SilencePercent, Duration);
		EnergyLock.Element = Item->GetAssociatedElement();
		SEM->ApplyEffect(Target, EnergyLock, User, Item->GetFullItemName(), -1);
	}
	OutResult.BuffsApplied++;
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

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Onyx: Silenced %s (%s)"),
		   *Target->GetName(), bSRank ? TEXT("S-rank binary, 1 turn") : TEXT("energy-lock"));
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

void UItemExecutor::ApplyGenericBonus(AActor *User, UItemData *Item, FItemUseResult &OutResult)
{
	// Generic characters gain elemental resistance from items
	if (!IsGenericCharacter(User))
		return;

	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (!StatusManager)
		return;

	float Resistance = Item->GetGenericResistanceBonus();
	int32 Duration = Item->GetGenericResistanceDuration();
	ESpellElement Element = Item->GetAssociatedElement();

	if (Resistance <= 0 || Element == ESpellElement::Generic)
		return;

	// Apply resistance buff (element-specific via Element field)
	FActiveSkillEffect ResistBuff = FActiveSkillEffect::CreateBuff(
		FString::Printf(TEXT("%s Resistance"), *UEnum::GetValueAsString(Element)),
		Item->GetUniqueID() + 1000, // Offset to avoid ID collision
		ESkillEffectType::ResistanceBuff,
		Resistance,
		Duration);
	ResistBuff.Element = Element;

	StatusManager->ApplyEffect(User, ResistBuff, User, TEXT("Generic Bonus"), -1);
	OutResult.GenericResistanceApplied = FMath::RoundToInt(Resistance);

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Generic bonus: +%.0f%% %s resistance for %d turns"),
		   Resistance, *UEnum::GetValueAsString(Element), Duration);
}

void UItemExecutor::ApplyBrokenDarknessBonus(AActor *User, UItemData *Item, FItemUseResult &OutResult)
{
	// Broken Darkness characters gain bonus energy from ALL items
	if (!IsBrokenDarknessCharacter(User))
		return;

	UCharacterDataComponent *UserComp = GetCharacterDataComponent(User);
	if (!UserComp)
		return;

	int32 BonusEnergy = Item->GetBrokenDarknessEnergyBonus();
	if (BonusEnergy <= 0)
		return;

	int32 EPBefore = UserComp->CurrentEP;
	UserComp->ServerGainEnergy(BonusEnergy);
	OutResult.BrokenDarknessEnergyGained = UserComp->CurrentEP - EPBefore;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Broken Darkness bonus: +%d energy"),
		   OutResult.BrokenDarknessEnergyGained);
}

void UItemExecutor::ApplySecondaryEffect(AActor *Target, UItemData *Item, AActor *Source, FItemUseResult &OutResult)
{
	// S-tier secondary effects (burn DOT for Garnet)
	if (!Item->HasSecondaryEffect())
		return;

	USkillEffectManager *StatusManager = GetSkillEffectManager();
	if (!StatusManager)
		return;

	int32 DamagePerTurn = Item->GetSecondaryDamagePerTurn();
	int32 Duration = Item->GetSecondaryDuration();

	if (DamagePerTurn <= 0 || Duration <= 0)
		return;

	ESpellElement ItemElement = Item->GetAssociatedElement();
	FActiveSkillEffect DOT = FActiveSkillEffect::CreateDOT(
		FString::Printf(TEXT("%s Burn"), *Item->GetFullItemName()),
		Item->GetUniqueID() + 2000,
		static_cast<float>(DamagePerTurn),
		Duration,
		ItemElement);

	StatusManager->ApplyEffect(Target, DOT, Source, Item->GetFullItemName(), -1);
	OutResult.bAppliedSecondaryEffect = true;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Secondary effect: %d damage/turn for %d turns"),
		   DamagePerTurn, Duration);
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

UCharacterData *UItemExecutor::GetCharacterData(AActor *Actor) const
{
	UCharacterDataComponent *Comp = GetCharacterDataComponent(Actor);
	return Comp ? Comp->CharacterData : nullptr;
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

bool UItemExecutor::IsGenericCharacter(AActor *Actor) const
{
	UCharacterData *Data = GetCharacterData(Actor);
	return Data && Data->InnateElement == ESpellElement::Generic;
}

bool UItemExecutor::IsBrokenDarknessCharacter(AActor *Actor) const
{
	UCharacterData *Data = GetCharacterData(Actor);
	return Data && Data->InnateElement == ESpellElement::BrokenDarkness;
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
