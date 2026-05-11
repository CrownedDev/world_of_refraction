// Copyright Epic Games, Inc. All Rights Reserved.

#include "ItemExecutor.h"
#include "ItemData.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "SkillEffectManager.h"
#include "StatusEffect.h"

void UItemExecutor::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Initialized"));
}

void UItemExecutor::Deinitialize()
{
	QuartzStates.Empty();
	QuartzItems.Empty();
	StatusEffectManagerRef = nullptr;
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
		ExecuteEnergyRestoreEffect(User, Item, Result);
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

	case EItemEffectType::Transform:
		ExecuteTransformEffect(User, Item, Result);
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
	// Garnet - Fire damage
	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
	{
		OutResult.ErrorMessage = TEXT("Target has no character data");
		return;
	}

	float Damage = Item->GetDamageValue();
	int32 HPBefore = TargetComp->CurrentHP;

	TargetComp->ServerTakeDamage(FMath::RoundToInt(Damage));

	OutResult.DamageDealt = HPBefore - TargetComp->CurrentHP;

	// S-tier Garnet has burn DOT
	if (Item->HasSecondaryEffect())
	{
		ApplySecondaryEffect(Target, Item, User, OutResult);
	}

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Garnet dealt %d damage to %s"),
		   OutResult.DamageDealt, *Target->GetName());
}

void UItemExecutor::ExecuteHealingEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Sapphire - Water healing
	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
	{
		OutResult.ErrorMessage = TEXT("Target has no character data");
		return;
	}

	float Healing = Item->GetDamageValue(); // Same getter for healing
	int32 HPBefore = TargetComp->CurrentHP;

	TargetComp->ServerHeal(FMath::RoundToInt(Healing));

	OutResult.HealingDone = TargetComp->CurrentHP - HPBefore;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Sapphire healed %s for %d"),
		   *Target->GetName(), OutResult.HealingDone);
}

void UItemExecutor::ExecuteEnergyRestoreEffect(AActor *User, UItemData *Item, FItemUseResult &OutResult)
{
	// Citrine - Lightning energy with self-damage cost
	UCharacterDataComponent *UserComp = GetCharacterDataComponent(User);
	if (!UserComp)
	{
		OutResult.ErrorMessage = TEXT("User has no character data");
		return;
	}

	int32 Energy = Item->GetEnergyValue();
	int32 SelfDamage = Item->GetSelfDamage();

	// Take self-damage first
	if (SelfDamage > 0)
	{
		int32 HPBefore = UserComp->CurrentHP;
		UserComp->ServerTakeDamage(SelfDamage);
		OutResult.SelfDamageTaken = HPBefore - UserComp->CurrentHP;
	}

	// Restore energy
	int32 EPBefore = UserComp->CurrentEP;
	UserComp->ServerGainEnergy(Energy);
	OutResult.EnergyRestored = UserComp->CurrentEP - EPBefore;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Citrine: %s gained %d energy, took %d self-damage"),
		   *User->GetName(), OutResult.EnergyRestored, OutResult.SelfDamageTaken);
}

void UItemExecutor::ExecuteSpeedBuffEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Emerald - Wind speed buff
	USkillEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
	{
		OutResult.ErrorMessage = TEXT("StatusEffectManager not available");
		return;
	}

	float BuffPercent = Item->GetBuffPercentage();
	int32 Duration = Item->GetBuffDuration();

	FStatusEffect SpeedBuff = FStatusEffect::CreateBuff(
		FString::Printf(TEXT("%s Speed"), *Item->GetFullItemName()),
		Item->GetUniqueID(),
		EStatusType::SpeedBuff,
		BuffPercent,
		Duration);
	SpeedBuff.Element = Item->GetAssociatedElement();

	StatusManager->ApplyEffect(Target, SpeedBuff, User, Item->GetFullItemName(), -1);
	OutResult.BuffsApplied++;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Emerald: Applied %.0f%% speed buff for %d turns"),
		   BuffPercent, Duration);
}

void UItemExecutor::ExecuteDefenseBuffEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Amber - Earth defense buff
	USkillEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
	{
		OutResult.ErrorMessage = TEXT("StatusEffectManager not available");
		return;
	}

	float BuffPercent = Item->GetBuffPercentage();
	int32 Duration = Item->GetBuffDuration();

	FStatusEffect DefenseBuff = FStatusEffect::CreateBuff(
		FString::Printf(TEXT("%s Defense"), *Item->GetFullItemName()),
		Item->GetUniqueID(),
		EStatusType::DefenseBuff,
		BuffPercent,
		Duration);
	DefenseBuff.Element = Item->GetAssociatedElement();

	StatusManager->ApplyEffect(Target, DefenseBuff, User, Item->GetFullItemName(), -1);
	OutResult.BuffsApplied++;

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Amber: Applied %.0f%% defense buff for %d turns"),
		   BuffPercent, Duration);
}

void UItemExecutor::ExecuteCritBuffEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Opal - Light crit buff (+ S-tier reveals enemy stats)
	USkillEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
	{
		OutResult.ErrorMessage = TEXT("StatusEffectManager not available");
		return;
	}

	float BuffPercent = Item->GetBuffPercentage();
	int32 Duration = Item->GetBuffDuration();

	FStatusEffect CritBuff = FStatusEffect::CreateBuff(
		FString::Printf(TEXT("%s Crit"), *Item->GetFullItemName()),
		Item->GetUniqueID(),
		EStatusType::CritChanceBuff,
		BuffPercent,
		Duration);
	CritBuff.Element = Item->GetAssociatedElement();

	StatusManager->ApplyEffect(Target, CritBuff, User, Item->GetFullItemName(), -1);
	OutResult.BuffsApplied++;

	// S-tier Opal reveals enemy info
	if (Item->GetRevealsHP() || Item->GetRevealsStats())
	{
		// TODO: Broadcast reveal event for UI to show enemy HP/stats
		UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Opal S-tier: Revealing enemy info (HP: %s, Stats: %s)"),
			   Item->GetRevealsHP() ? TEXT("Yes") : TEXT("No"),
			   Item->GetRevealsStats() ? TEXT("Yes") : TEXT("No"));
	}

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Opal: Applied %.0f%% crit buff for %d turns"),
		   BuffPercent, Duration);
}

void UItemExecutor::ExecuteSilenceEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Onyx - Darkness silence (prevents spell casting)
	USkillEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
	{
		OutResult.ErrorMessage = TEXT("StatusEffectManager not available");
		return;
	}

	int32 Duration = Item->GetSilenceDuration();

	// NOTE: Silence effect type not yet in enum - using EnergyDrain as placeholder
	// This prevents energy gain which effectively limits spell casting
	FStatusEffect Silence = FStatusEffect::CreateBuff(
		FString::Printf(TEXT("%s Silence"), *Item->GetFullItemName()),
		Item->GetUniqueID(),
		EStatusType::EnergyDrain, // TODO: Add proper Silence type
		1.0f,							 // Binary effect
		Duration);
	Silence.Element = Item->GetAssociatedElement();

	StatusManager->ApplyEffect(Target, Silence, User, Item->GetFullItemName(), -1);
	OutResult.BuffsApplied++; // Counts as applied effect

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Onyx: Applied silence for %d turns to %s"),
		   Duration, *Target->GetName());
}

void UItemExecutor::ExecuteGambleEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Amethyst - Void gamble (random buff or debuff)
	USkillEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
	{
		OutResult.ErrorMessage = TEXT("StatusEffectManager not available");
		return;
	}

	OutResult.bWasGamble = true;

	// 50/50 chance for buff or debuff
	bool bIsPositive = FMath::RandBool();
	OutResult.bGambleWon = bIsPositive;

	// Random effect type
	TArray<EStatusType> BuffTypes = {
		EStatusType::DamageBuff,
		EStatusType::DefenseBuff,
		EStatusType::SpeedBuff,
		EStatusType::CritChanceBuff};

	TArray<EStatusType> DebuffTypes = {
		EStatusType::DamageDebuff,
		EStatusType::DefenseDebuff,
		EStatusType::SpeedDebuff,
		EStatusType::CritChanceDebuff};

	EStatusType ChosenType;
	if (bIsPositive)
	{
		ChosenType = BuffTypes[FMath::RandRange(0, BuffTypes.Num() - 1)];
	}
	else
	{
		ChosenType = DebuffTypes[FMath::RandRange(0, DebuffTypes.Num() - 1)];
	}

	// Random magnitude (scaled by tier)
	float BaseMagnitude = Item->GetBuffPercentage(); // Use tier scaling
	float Magnitude = BaseMagnitude * FMath::FRandRange(0.5f, 1.5f);
	int32 Duration = FMath::RandRange(2, 5);

	// CreateBuff works for both - EffectType determines if it's buff or debuff
	FStatusEffect GambleEffect = FStatusEffect::CreateBuff(
		TEXT("Gamble Result"), Item->GetUniqueID(), ChosenType, Magnitude, Duration);
	GambleEffect.Element = ESpellElement::Void;

	// Apply to user (gamble affects self)
	StatusManager->ApplyEffect(User, GambleEffect, User, Item->GetFullItemName(), -1);

	OutResult.GambleOutcome = FString::Printf(TEXT("%s %.0f%% for %d turns"),
											  bIsPositive ? TEXT("Won") : TEXT("Lost"),
											  Magnitude, Duration);

	// Broadcast gamble result
	OnGambleResult.Broadcast(User, bIsPositive, OutResult.GambleOutcome);

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Amethyst gamble: %s"), *OutResult.GambleOutcome);
}

void UItemExecutor::ExecuteCleanseEffect(AActor *User, AActor *Target, UItemData *Item, FItemUseResult &OutResult)
{
	// Iolite - Reality cleanse (tiered removal + immunity)
	USkillEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
	{
		OutResult.ErrorMessage = TEXT("StatusEffectManager not available");
		return;
	}

	int32 DebuffsToRemove = Item->GetDebuffsToRemove();
	bool bGrantsImmunity = Item->GetGrantsImmunity();
	int32 ImmunityDuration = Item->GetImmunityDuration();

	// Get current debuff count
	int32 DebuffsBefore = StatusManager->GetDebuffCount(Target);

	if (DebuffsToRemove >= 99) // High value = remove all
	{
		StatusManager->RemoveAllDebuffs(Target);
	}
	else
	{
		// Remove specific number of debuffs (oldest first)
		TArray<FStatusEffect> Debuffs = StatusManager->GetActiveEffects(Target);
		int32 Removed = 0;

		for (const FStatusEffect &Effect : Debuffs)
		{
			if (Removed >= DebuffsToRemove)
				break;

			if (!Effect.IsBuff())
			{
				StatusManager->RemoveEffectByID(Target, Effect.EffectID);
				Removed++;
			}
		}
	}

	int32 DebuffsAfter = StatusManager->GetDebuffCount(Target);
	OutResult.DebuffsRemoved = DebuffsBefore - DebuffsAfter;

	// Apply immunity if granted (B-tier and above)
	// NOTE: Using high ResistanceBuff as pseudo-immunity since DebuffImmunity doesn't exist
	if (bGrantsImmunity && ImmunityDuration > 0)
	{
		FStatusEffect Immunity = FStatusEffect::CreateBuff(
			TEXT("Cleanse Immunity"),
			Item->GetUniqueID(),
			EStatusType::ResistanceBuff, // 100% = immunity
			100.0f,
			ImmunityDuration);
		Immunity.Element = ESpellElement::Reality;

		StatusManager->ApplyEffect(Target, Immunity, User, Item->GetFullItemName(), -1);
		OutResult.BuffsApplied++;
	}

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Iolite: Removed %d debuffs, immunity: %s"),
		   OutResult.DebuffsRemoved, bGrantsImmunity ? TEXT("Yes") : TEXT("No"));
}

void UItemExecutor::ExecuteTransformEffect(AActor *User, UItemData *Item, FItemUseResult &OutResult)
{
	// Quartz - Transform (activates if threshold met)
	if (!CanQuartzTransform(User))
	{
		OutResult.ErrorMessage = TEXT("Quartz cannot transform yet");
		UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Quartz: Not enough damage absorbed to transform"));
		return;
	}

	ESpellElement NewElement = TransformQuartz(User);
	if (NewElement != ESpellElement::Generic)
	{
		OutResult.bQuartzTransformed = true;
		OutResult.QuartzNewElement = NewElement;

		UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Quartz transformed into %s element!"),
			   *UEnum::GetValueAsString(NewElement));
	}
}

// ========================================
// BONUS HANDLERS
// ========================================

void UItemExecutor::ApplyGenericBonus(AActor *User, UItemData *Item, FItemUseResult &OutResult)
{
	// Generic characters gain elemental resistance from items
	if (!IsGenericCharacter(User))
		return;

	USkillEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
		return;

	float Resistance = Item->GetGenericResistanceBonus();
	int32 Duration = Item->GetGenericResistanceDuration();
	ESpellElement Element = Item->GetAssociatedElement();

	if (Resistance <= 0 || Element == ESpellElement::Generic)
		return;

	// Apply resistance buff (element-specific via Element field)
	FStatusEffect ResistBuff = FStatusEffect::CreateBuff(
		FString::Printf(TEXT("%s Resistance"), *UEnum::GetValueAsString(Element)),
		Item->GetUniqueID() + 1000, // Offset to avoid ID collision
		EStatusType::ResistanceBuff,
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

	USkillEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
		return;

	int32 DamagePerTurn = Item->GetSecondaryDamagePerTurn();
	int32 Duration = Item->GetSecondaryDuration();

	if (DamagePerTurn <= 0 || Duration <= 0)
		return;

	ESpellElement ItemElement = Item->GetAssociatedElement();
	FStatusEffect DOT = FStatusEffect::CreateDOT(
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
// QUARTZ SYSTEM
// ========================================

void UItemExecutor::RegisterQuartz(AActor *Owner, UItemData *QuartzItem)
{
	if (!Owner || !QuartzItem)
		return;

	if (QuartzItem->GetPrimaryEffectType() != EItemEffectType::Transform)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ItemExecutor] Attempted to register non-Quartz item"));
		return;
	}

	FQuartzAbsorptionState State;
	State.TransformThreshold = QuartzItem->GetTransformThreshold();

	QuartzStates.Add(Owner, State);
	QuartzItems.Add(Owner, QuartzItem);

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Registered Quartz for %s (Threshold: %.0f)"),
		   *Owner->GetName(), State.TransformThreshold);
}

void UItemExecutor::UnregisterQuartz(AActor *Owner)
{
	QuartzStates.Remove(Owner);
	QuartzItems.Remove(Owner);

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Unregistered Quartz for %s"),
		   Owner ? *Owner->GetName() : TEXT("Unknown"));
}

void UItemExecutor::NotifyQuartzDamage(AActor *Owner, ESpellElement Element, float Damage)
{
	FQuartzAbsorptionState *State = QuartzStates.Find(Owner);
	if (!State || State->bHasTransformed)
		return;

	State->AbsorbDamage(Element, Damage);

	UE_LOG(LogTemp, Verbose, TEXT("[ItemExecutor] Quartz absorbed %.0f %s damage (Total: %.0f/%.0f)"),
		   Damage, *UEnum::GetValueAsString(Element), State->TotalAbsorbed, State->TransformThreshold);
}

bool UItemExecutor::CanQuartzTransform(AActor *Owner) const
{
	const FQuartzAbsorptionState *State = QuartzStates.Find(Owner);
	return State && State->CanTransform();
}

ESpellElement UItemExecutor::TransformQuartz(AActor *Owner)
{
	FQuartzAbsorptionState *State = QuartzStates.Find(Owner);
	if (!State || !State->CanTransform())
	{
		return ESpellElement::Generic;
	}

	ESpellElement NewElement = State->GetDominantElement();
	State->bHasTransformed = true;
	State->TransformedElement = NewElement;

	// Get the Quartz item
	TWeakObjectPtr<UItemData> *ItemPtr = QuartzItems.Find(Owner);
	UItemData *Item = ItemPtr ? ItemPtr->Get() : nullptr;

	// Broadcast transformation
	OnQuartzTransformed.Broadcast(Owner, NewElement, Item);

	UE_LOG(LogTemp, Log, TEXT("[ItemExecutor] Quartz transformed! Dominant element: %s"),
		   *UEnum::GetValueAsString(NewElement));

	return NewElement;
}

FQuartzAbsorptionState UItemExecutor::GetQuartzState(AActor *Owner) const
{
	const FQuartzAbsorptionState *State = QuartzStates.Find(Owner);
	return State ? *State : FQuartzAbsorptionState();
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

USkillEffectManager *UItemExecutor::GetStatusEffectManager() const
{
	if (!StatusEffectManagerRef)
	{
		if (UGameInstance *GI = Cast<UGameInstance>(GetGameInstance()))
		{
			const_cast<UItemExecutor *>(this)->StatusEffectManagerRef =
				GI->GetSubsystem<USkillEffectManager>();
		}
	}
	return StatusEffectManagerRef;
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

// ========================================
// DEBUG
// ========================================

void UItemExecutor::DebugPrintQuartzStates() const
{
	UE_LOG(LogTemp, Display, TEXT("=== QUARTZ ABSORPTION STATES ==="));

	for (const auto &Pair : QuartzStates)
	{
		AActor *Owner = Pair.Key.Get();
		const FQuartzAbsorptionState &State = Pair.Value;

		UE_LOG(LogTemp, Display, TEXT("Owner: %s"), Owner ? *Owner->GetName() : TEXT("Invalid"));
		UE_LOG(LogTemp, Display, TEXT("  Total Absorbed: %.0f / %.0f"),
			   State.TotalAbsorbed, State.TransformThreshold);
		UE_LOG(LogTemp, Display, TEXT("  Transformed: %s"), State.bHasTransformed ? TEXT("Yes") : TEXT("No"));

		if (State.bHasTransformed)
		{
			UE_LOG(LogTemp, Display, TEXT("  Result Element: %s"),
				   *UEnum::GetValueAsString(State.TransformedElement));
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("  Dominant Element: %s"),
				   *UEnum::GetValueAsString(State.GetDominantElement()));
		}

		for (const auto &ElementPair : State.AbsorbedDamage)
		{
			UE_LOG(LogTemp, Display, TEXT("    %s: %.0f"),
				   *UEnum::GetValueAsString(ElementPair.Key), ElementPair.Value);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("================================"));
}
