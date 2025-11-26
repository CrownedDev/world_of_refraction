// Copyright Epic Games, Inc. All Rights Reserved.

#include "WeaponManager.h"
#include "WeaponData.h"
#include "WeaponAttackData.h"
#include "BaseAttackData.h"
#include "AbilityData.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "StatusEffectManager.h"
#include "StatusEffect.h"

void UWeaponManager::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] Initialized"));
}

void UWeaponManager::Deinitialize()
{
	WeaponStates.Empty();
	StatusEffectManagerRef = nullptr;
	Super::Deinitialize();
}

// ========================================
// STATE MANAGEMENT
// ========================================

void UWeaponManager::InitializeWeaponState(AActor *Actor)
{
	if (!Actor)
		return;

	UCharacterData *CharData = GetCharacterData(Actor);
	if (!CharData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WeaponManager] Cannot initialize - no CharacterData for %s"),
			   *Actor->GetName());
		return;
	}

	FWeaponState State;

	if (IsGenericCharacter(Actor))
	{
		// Generic: Start with primary or secondary based on bUsePrimary
		State.ActiveSlot = CharData->bUsePrimary ? EWeaponSlot::Primary : EWeaponSlot::Secondary;
		State.bInfusionActive = false;
	}
	else
	{
		// Elemental: Start armed or unarmed based on bUsePrimary
		State.ActiveSlot = CharData->bUsePrimary ? EWeaponSlot::Primary : EWeaponSlot::Unarmed;
	}

	State.ConjuredWeapon = nullptr;
	State.bSpellsSealed = false;

	WeaponStates.Add(Actor, State);

	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] Initialized weapon state for %s: Slot=%d"),
		   *Actor->GetName(), static_cast<int32>(State.ActiveSlot));
}

void UWeaponManager::ClearWeaponState(AActor *Actor)
{
	WeaponStates.Remove(Actor);
	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] Cleared weapon state for %s"),
		   Actor ? *Actor->GetName() : TEXT("Unknown"));
}

FWeaponState UWeaponManager::GetWeaponState(AActor *Actor) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);
	return State ? *State : FWeaponState();
}

UWeaponData *UWeaponManager::GetActiveWeapon(AActor *Actor) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);
	if (!State)
		return nullptr;

	return GetWeaponInSlot(Actor, State->ActiveSlot);
}

UBaseAttackData *UWeaponManager::GetActiveAttack(AActor *Actor) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);
	if (!State)
		return nullptr;

	// If conjured, use conjured weapon attack
	if (State->ActiveSlot == EWeaponSlot::Conjured && State->ConjuredWeapon)
	{
		return State->ConjuredWeapon->WeaponAttack;
	}

	// If armed, use weapon attack
	UWeaponData *Weapon = GetWeaponInSlot(Actor, State->ActiveSlot);
	if (Weapon && Weapon->WeaponAttack)
	{
		return Weapon->WeaponAttack;
	}

	// Unarmed - use base attack
	UCharacterData *CharData = GetCharacterData(Actor);
	return CharData ? CharData->BaseAttack : nullptr;
}

TArray<UAbilityData *> UWeaponManager::GetActiveAbilities(AActor *Actor) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);
	if (!State)
	{
		return TArray<UAbilityData *>();
	}

	// Conjured weapon abilities (includes Dispel in slot)
	if (State->ActiveSlot == EWeaponSlot::Conjured && State->ConjuredWeapon)
	{
		return State->ConjuredWeapon->PresetAbilities;
	}

	// Armed - weapon abilities
	UWeaponData *Weapon = GetWeaponInSlot(Actor, State->ActiveSlot);
	if (Weapon)
	{
		return Weapon->PresetAbilities;
	}

	// Unarmed - no base abilities on CharacterData
	// TODO: Implement base ability lookup - CharacterData doesn't store abilities directly
	// For now, return empty array - abilities come from weapon loadout
	return TArray<UAbilityData *>();
}

// ========================================
// WEAPON SWITCHING
// ========================================

bool UWeaponManager::SwitchWeapon(AActor *Actor, EWeaponSlot TargetSlot)
{
	if (!CanSwitchWeapon(Actor, TargetSlot))
	{
		return false;
	}

	FWeaponState *State = WeaponStates.Find(Actor);
	if (!State)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WeaponManager] No weapon state for %s"),
			   *Actor->GetName());
		return false;
	}

	EWeaponSlot OldSlot = State->ActiveSlot;
	State->ActiveSlot = TargetSlot;

	// Turn off infusion when switching (must re-enable)
	if (State->bInfusionActive)
	{
		State->bInfusionActive = false;
		OnInfusionToggled.Broadcast(Actor, false);
	}

	OnWeaponSwitched.Broadcast(Actor, OldSlot, TargetSlot);

	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] %s switched from %d to %d"),
		   *Actor->GetName(), static_cast<int32>(OldSlot), static_cast<int32>(TargetSlot));

	return true;
}

bool UWeaponManager::CanSwitchWeapon(AActor *Actor, EWeaponSlot TargetSlot) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);
	if (!State)
		return false;

	// Can't switch while conjured (must dispel first)
	if (State->ActiveSlot == EWeaponSlot::Conjured)
	{
		return false;
	}

	// Can't switch to conjured directly (must use ConjureWeapon)
	if (TargetSlot == EWeaponSlot::Conjured)
	{
		return false;
	}

	// Already in target slot
	if (State->ActiveSlot == TargetSlot)
	{
		return false;
	}

	TArray<EWeaponSlot> Available = GetAvailableSlots(Actor);
	return Available.Contains(TargetSlot);
}

TArray<EWeaponSlot> UWeaponManager::GetAvailableSlots(AActor *Actor) const
{
	TArray<EWeaponSlot> Slots;

	UCharacterData *CharData = GetCharacterData(Actor);
	if (!CharData)
		return Slots;

	if (IsGenericCharacter(Actor))
	{
		// Generic: Primary and Secondary always available
		if (CharData->PrimaryWeapon)
		{
			Slots.Add(EWeaponSlot::Primary);
		}
		if (CharData->SecondaryWeapon)
		{
			Slots.Add(EWeaponSlot::Secondary);
		}
		// Generic can also go unarmed
		Slots.Add(EWeaponSlot::Unarmed);
	}
	else
	{
		// Elemental: Unarmed always available
		Slots.Add(EWeaponSlot::Unarmed);

		// Armed if has primary weapon
		if (CharData->PrimaryWeapon)
		{
			Slots.Add(EWeaponSlot::Primary);
		}
	}

	return Slots;
}

// ========================================
// INFUSION (GENERIC ONLY)
// ========================================

bool UWeaponManager::ToggleInfusion(AActor *Actor)
{
	FWeaponState *State = WeaponStates.Find(Actor);
	if (!State || !CanUseInfusion(Actor))
	{
		return false;
	}

	State->bInfusionActive = !State->bInfusionActive;
	OnInfusionToggled.Broadcast(Actor, State->bInfusionActive);

	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] %s infusion: %s"),
		   *Actor->GetName(), State->bInfusionActive ? TEXT("ON") : TEXT("OFF"));

	return true;
}

bool UWeaponManager::SetInfusion(AActor *Actor, bool bEnabled)
{
	FWeaponState *State = WeaponStates.Find(Actor);
	if (!State || !CanUseInfusion(Actor))
	{
		return false;
	}

	if (State->bInfusionActive != bEnabled)
	{
		State->bInfusionActive = bEnabled;
		OnInfusionToggled.Broadcast(Actor, bEnabled);
	}

	return true;
}

bool UWeaponManager::IsInfusionActive(AActor *Actor) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);
	return State && State->bInfusionActive;
}

bool UWeaponManager::CanUseInfusion(AActor *Actor) const
{
	// Only Generic characters can use infusion
	if (!IsGenericCharacter(Actor))
	{
		return false;
	}

	// Must have a weapon equipped
	UWeaponData *Weapon = GetActiveWeapon(Actor);
	if (!Weapon)
	{
		return false;
	}

	// Weapon must support infusion
	return Weapon->bCanBeInfused;
}

// ========================================
// CONJURATION (ELEMENTAL ONLY)
// ========================================

bool UWeaponManager::ConjureWeapon(AActor *Actor, UWeaponData *WeaponToConjure)
{
	if (!Actor || !WeaponToConjure)
	{
		return false;
	}

	// Only Elemental characters can conjure
	if (!IsElementalCharacter(Actor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WeaponManager] Only Elemental characters can conjure weapons"));
		return false;
	}

	FWeaponState *State = WeaponStates.Find(Actor);
	if (!State)
	{
		return false;
	}

	// Already conjured
	if (State->ActiveSlot == EWeaponSlot::Conjured)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WeaponManager] Already has conjured weapon"));
		return false;
	}

	// Store previous state for dispel
	State->PreConjuredSlot = State->ActiveSlot;
	State->ConjuredWeapon = WeaponToConjure;
	State->ActiveSlot = EWeaponSlot::Conjured;
	State->bSpellsSealed = true;

	OnConjurationStarted.Broadcast(Actor, WeaponToConjure);

	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] %s conjured %s - spells SEALED"),
		   *Actor->GetName(), *WeaponToConjure->WeaponName);

	return true;
}

bool UWeaponManager::DispelConjuredWeapon(AActor *Actor)
{
	FWeaponState *State = WeaponStates.Find(Actor);
	if (!State || State->ActiveSlot != EWeaponSlot::Conjured)
	{
		return false;
	}

	// Return to previous state
	State->ActiveSlot = State->PreConjuredSlot;
	State->ConjuredWeapon = nullptr;
	State->bSpellsSealed = false;

	OnConjurationEnded.Broadcast(Actor);

	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] %s dispelled conjured weapon - spells UNSEALED"),
		   *Actor->GetName());

	return true;
}

bool UWeaponManager::HasConjuredWeapon(AActor *Actor) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);
	return State && State->ActiveSlot == EWeaponSlot::Conjured && State->ConjuredWeapon != nullptr;
}

bool UWeaponManager::AreSpellsSealed(AActor *Actor) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);
	return State && State->bSpellsSealed;
}

// ========================================
// ATTACK EXECUTION
// ========================================

FWeaponAttackResult UWeaponManager::ExecuteAttack(AActor *Attacker, const TArray<AActor *> &Targets)
{
	bool bUseInfusion = IsInfusionActive(Attacker);
	return ExecuteAttackWithInfusion(Attacker, Targets, bUseInfusion);
}

FWeaponAttackResult UWeaponManager::ExecuteAttackWithInfusion(AActor *Attacker, const TArray<AActor *> &Targets, bool bUseInfusion)
{
	FWeaponAttackResult Result;

	if (!Attacker)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid attacker");
		return Result;
	}

	UBaseAttackData *Attack = GetActiveAttack(Attacker);
	if (!Attack)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("No attack available");
		return Result;
	}

	UCharacterData *AttackerData = GetCharacterData(Attacker);
	UCharacterDataComponent *AttackerComp = GetCharacterDataComponent(Attacker);
	if (!AttackerData || !AttackerComp)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Attacker missing character data");
		return Result;
	}

	// Get weapon for infusion/status calculations
	UWeaponData *Weapon = GetActiveWeapon(Attacker);
	UWeaponAttackData *WeaponAttack = Cast<UWeaponAttackData>(Attack);

	// Infusion costs energy
	if (bUseInfusion)
	{
		if (AttackerComp->CurrentEP < INFUSED_ATTACK_ENERGY_COST)
		{
			Result.bSuccess = false;
			Result.ErrorMessage = TEXT("Not enough energy for infused attack");
			return Result;
		}

		AttackerComp->ServerSpendEnergy(INFUSED_ATTACK_ENERGY_COST);
		Result.EnergySpent = INFUSED_ATTACK_ENERGY_COST;
		Result.bWasInfused = true;
		Result.InfusedElement = AttackerData->InnateElement;
	}

	// Calculate base damage - attacks use character's RawDamageMultiplier
	// Base damage is 100, scaled by character stats
	float DamageMultiplier = AttackerData->CalculateRawDamageMultiplier();
	int32 BaseDamage = FMath::RoundToInt(100.0f * DamageMultiplier);

	// Infusion penalty
	if (bUseInfusion)
	{
		BaseDamage = FMath::RoundToInt(BaseDamage * (1.0f - INFUSION_DAMAGE_PENALTY));
	}

	// Get physical damage type if weapon attack
	if (WeaponAttack)
	{
		Result.PhysicalType = WeaponAttack->PhysicalDamageType;
	}

	// Process each target
	int32 HitCount = Attack->HitCount > 0 ? Attack->HitCount : 1;
	int32 DamagePerHit = BaseDamage / HitCount;

	for (AActor *Target : Targets)
	{
		if (!Target)
			continue;

		int32 TotalDamageToTarget = 0;

		// Multi-hit processing
		for (int32 Hit = 0; Hit < HitCount; Hit++)
		{
			int32 HitDamage = ApplyWeaponDamage(
				Attacker, Target, DamagePerHit,
				bUseInfusion, bUseInfusion ? AttackerData->InnateElement : ERefractionElement::Generic,
				true, Result);

			TotalDamageToTarget += HitDamage;

			// Check if target died
			UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
			if (TargetComp && !TargetComp->bIsAlive)
			{
				Result.bCausedDeath = true;
				break;
			}
		}

		Result.TotalDamageDealt += TotalDamageToTarget;
		Result.DamagePerTarget.Add(Target, TotalDamageToTarget);

		// Physical status buildup (only for weapon attacks, not infused)
		if (WeaponAttack && !bUseInfusion)
		{
			float Buildup = CalculateStatusBuildup(Weapon, WeaponAttack, HitCount);
			Result.StatusBuildupApplied += Buildup;

			FWeaponState *State = WeaponStates.Find(Attacker);
			if (State)
			{
				State->AddBuildup(Target, Buildup);
				float TotalBuildup = State->GetBuildup(Target);
				float Threshold = GetStatusThreshold(WeaponAttack->PhysicalDamageType);

				if (TotalBuildup >= Threshold)
				{
					TriggerPhysicalStatus(Attacker, Target, WeaponAttack->PhysicalDamageType);
					State->ResetBuildup(Target);
					Result.bTriggeredPhysicalStatus = true;
				}
			}
		}
	}

	Result.bSuccess = true;

	// Broadcast event
	OnWeaponAttackExecuted.Broadcast(Attacker, Result, Weapon);

	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] %s attacked with %s%s - %d total damage"),
		   *Attacker->GetName(),
		   *Attack->AttackName,
		   bUseInfusion ? TEXT(" (Infused)") : TEXT(""),
		   Result.TotalDamageDealt);

	return Result;
}

// ========================================
// PHYSICAL STATUS
// ========================================

float UWeaponManager::GetStatusBuildup(AActor *Attacker, AActor *Target) const
{
	const FWeaponState *State = WeaponStates.Find(Attacker);
	return State ? State->GetBuildup(Target) : 0.0f;
}

float UWeaponManager::GetStatusThreshold(EPhysicalDamageType DamageType) const
{
	switch (DamageType)
	{
	case EPhysicalDamageType::Slash:
		return STATUS_THRESHOLD_BLEED;
	case EPhysicalDamageType::Pierce:
		return STATUS_THRESHOLD_ARMOR_BREAK;
	case EPhysicalDamageType::Blunt:
		return STATUS_THRESHOLD_STUN;
	default:
		return 100.0f;
	}
}

bool UWeaponManager::WouldTriggerStatus(AActor *Attacker, AActor *Target, float AdditionalBuildup) const
{
	const FWeaponState *State = WeaponStates.Find(Attacker);
	if (!State)
		return false;

	UWeaponAttackData *WeaponAttack = Cast<UWeaponAttackData>(GetActiveAttack(Attacker));
	if (!WeaponAttack)
		return false;

	float CurrentBuildup = State->GetBuildup(Target);
	float Threshold = GetStatusThreshold(WeaponAttack->PhysicalDamageType);

	return (CurrentBuildup + AdditionalBuildup) >= Threshold;
}

void UWeaponManager::ResetStatusBuildup(AActor *Attacker, AActor *Target)
{
	FWeaponState *State = WeaponStates.Find(Attacker);
	if (State)
	{
		State->ResetBuildup(Target);
	}
}

// ========================================
// HELPERS
// ========================================

UCharacterDataComponent *UWeaponManager::GetCharacterDataComponent(AActor *Actor) const
{
	if (!Actor)
		return nullptr;
	return Actor->FindComponentByClass<UCharacterDataComponent>();
}

UCharacterData *UWeaponManager::GetCharacterData(AActor *Actor) const
{
	UCharacterDataComponent *Comp = GetCharacterDataComponent(Actor);
	return Comp ? Comp->CharacterData : nullptr;
}

UStatusEffectManager *UWeaponManager::GetStatusEffectManager() const
{
	if (!StatusEffectManagerRef)
	{
		if (UGameInstance *GI = Cast<UGameInstance>(GetGameInstance()))
		{
			const_cast<UWeaponManager *>(this)->StatusEffectManagerRef =
				GI->GetSubsystem<UStatusEffectManager>();
		}
	}
	return StatusEffectManagerRef;
}

bool UWeaponManager::IsGenericCharacter(AActor *Actor) const
{
	UCharacterData *Data = GetCharacterData(Actor);
	return Data && Data->InnateElement == ERefractionElement::Generic;
}

bool UWeaponManager::IsElementalCharacter(AActor *Actor) const
{
	UCharacterData *Data = GetCharacterData(Actor);
	if (!Data)
		return false;

	return Data->InnateElement != ERefractionElement::Generic &&
		   Data->InnateElement != ERefractionElement::BrokenDarkness;
}

UWeaponData *UWeaponManager::GetWeaponInSlot(AActor *Actor, EWeaponSlot Slot) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);
	UCharacterData *CharData = GetCharacterData(Actor);

	if (!CharData)
		return nullptr;

	switch (Slot)
	{
	case EWeaponSlot::Unarmed:
		return nullptr;

	case EWeaponSlot::Primary:
		// Both Generic and Elemental characters use PrimaryWeapon
		return CharData->PrimaryWeapon;

	case EWeaponSlot::Secondary:
		if (IsGenericCharacter(Actor))
		{
			return CharData->SecondaryWeapon;
		}
		return nullptr;

	case EWeaponSlot::Conjured:
		return State ? State->ConjuredWeapon : nullptr;

	default:
		return nullptr;
	}
}

float UWeaponManager::CalculateStatusBuildup(UWeaponData *Weapon, UWeaponAttackData *Attack, int32 HitCount) const
{
	if (!Attack)
		return 0.0f;

	float BaseBuildup = Attack->StatusBuildup;
	float Multiplier = Weapon ? Weapon->InfusionStatusMultiplier : 1.0f;

	return BaseBuildup * Multiplier * HitCount;
}

void UWeaponManager::TriggerPhysicalStatus(AActor *Attacker, AActor *Target, EPhysicalDamageType DamageType)
{
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
		return;

	FStatusEffect Effect;

	switch (DamageType)
	{
	case EPhysicalDamageType::Slash:
		// Bleed - DOT (use Generic element for physical)
		Effect = FStatusEffect::CreateDOT(
			TEXT("Bleed"),
			FMath::Rand(),				  // Unique ID
			15.0f,						  // Damage per turn
			3,							  // Duration
			ERefractionElement::Generic); // Physical, no element
		break;

	case EPhysicalDamageType::Pierce:
		// Armor Break - Defense debuff (CreateBuff works for debuffs too)
		Effect = FStatusEffect::CreateBuff(
			TEXT("Armor Break"),
			FMath::Rand(),
			EAbilityEffectType::DefenseDebuff,
			30.0f, // 30% defense reduction
			3);
		Effect.Element = ERefractionElement::Generic;
		break;

	case EPhysicalDamageType::Blunt:
		// Stun - Skip turn (using AttackSpeedDebuff as placeholder)
		// TODO: Add proper Stun type to EAbilityEffectType
		Effect = FStatusEffect::CreateBuff(
			TEXT("Stun"),
			FMath::Rand(),
			EAbilityEffectType::AttackSpeedDebuff, // Placeholder for stun
			100.0f,								   // 100% speed reduction = skip turn
			1);									   // 1 turn
		Effect.Element = ERefractionElement::Generic;
		break;

	default:
		return;
	}

	StatusManager->ApplyEffect(Target, Effect, Attacker, TEXT("Physical Status"), -1);
	OnPhysicalStatusTriggered.Broadcast(Target, DamageType, Attacker);

	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] Physical status triggered: %s on %s"),
		   *Effect.EffectName, *Target->GetName());
}

int32 UWeaponManager::ApplyWeaponDamage(AActor *Attacker, AActor *Target, int32 BaseDamage,
										bool bIsElemental, ERefractionElement Element, bool bCanCrit, FWeaponAttackResult &OutResult)
{
	UCharacterDataComponent *TargetComp = GetCharacterDataComponent(Target);
	if (!TargetComp)
		return 0;

	UCharacterData *TargetData = GetCharacterData(Target);
	UCharacterData *AttackerData = GetCharacterData(Attacker);

	// Calculate defense - different mechanics for physical vs elemental
	int32 FinalDamage = BaseDamage;

	if (TargetData)
	{
		// Step 1: Apply flat defense (subtraction) - always applied
		int32 FlatDefense = TargetData->CalculateFlatDefense();

		// Apply status effect modifiers to flat defense
		UStatusEffectManager *StatusManager = GetStatusEffectManager();
		if (StatusManager)
		{
			float DefenseBuffPercent = StatusManager->GetTotalStatModifier(Target, EAbilityEffectType::DefenseBuff);
			float DefenseDebuffPercent = StatusManager->GetTotalStatModifier(Target, EAbilityEffectType::DefenseDebuff);
			float DefenseModifier = 1.0f + (DefenseBuffPercent - DefenseDebuffPercent) / 100.0f;
			FlatDefense = FMath::RoundToInt(FlatDefense * FMath::Max(0.0f, DefenseModifier));
		}

		FinalDamage = FMath::Max(0, FinalDamage - FlatDefense);

		// Step 2: Apply resistance (percentage) - elemental damage only
		if (bIsElemental && FinalDamage > 0)
		{
			float Resistance = TargetData->CalculateElementalResistance(); // Returns 0.0-1.0
			Resistance = FMath::Clamp(Resistance, 0.0f, 0.8f);			   // Cap at 80%
			FinalDamage = FMath::RoundToInt(FinalDamage * (1.0f - Resistance));
		}
	}

	// Critical hit check
	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (bCanCrit && AttackerData)
	{
		float CritChance = AttackerData->CalculateCritChance() * 100.0f; // Convert to percentage
		if (StatusManager)
		{
			CritChance += StatusManager->GetTotalStatModifier(Attacker, EAbilityEffectType::CritChanceBuff);
			CritChance -= StatusManager->GetTotalStatModifier(Attacker, EAbilityEffectType::CritChanceDebuff);
		}
		CritChance = FMath::Clamp(CritChance, 0.0f, 100.0f);

		if (FMath::FRand() * 100.0f < CritChance)
		{
			// Crit damage is fixed at 1.5x (no method exists on CharacterData)
			constexpr float CritMultiplier = 1.5f;
			FinalDamage = FMath::RoundToInt(FinalDamage * CritMultiplier);
			OutResult.bWasCritical = true;
		}
	}

	// Ensure minimum damage
	FinalDamage = FMath::Max(1, FinalDamage);

	// Apply damage
	TargetComp->ServerTakeDamage(FinalDamage);

	return FinalDamage;
}

// ========================================
// DEBUG
// ========================================

void UWeaponManager::DebugPrintWeaponStates() const
{
	UE_LOG(LogTemp, Display, TEXT("=== WEAPON STATES ==="));

	for (const auto &Pair : WeaponStates)
	{
		AActor *Actor = Pair.Key.Get();
		const FWeaponState &State = Pair.Value;

		UE_LOG(LogTemp, Display, TEXT("Actor: %s"),
			   Actor ? *Actor->GetName() : TEXT("Invalid"));
		UE_LOG(LogTemp, Display, TEXT("  Slot: %d, Infusion: %s, Spells Sealed: %s"),
			   static_cast<int32>(State.ActiveSlot),
			   State.bInfusionActive ? TEXT("ON") : TEXT("OFF"),
			   State.bSpellsSealed ? TEXT("Yes") : TEXT("No"));

		if (State.ConjuredWeapon)
		{
			UE_LOG(LogTemp, Display, TEXT("  Conjured: %s"),
				   *State.ConjuredWeapon->WeaponName);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("====================="));
}

void UWeaponManager::DebugPrintActorWeaponState(AActor *Actor) const
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Display, TEXT("Invalid actor"));
		return;
	}

	const FWeaponState *State = WeaponStates.Find(Actor);
	if (!State)
	{
		UE_LOG(LogTemp, Display, TEXT("No weapon state for %s"), *Actor->GetName());
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("=== WEAPON STATE: %s ==="), *Actor->GetName());
	UE_LOG(LogTemp, Display, TEXT("Active Slot: %d"), static_cast<int32>(State->ActiveSlot));
	UE_LOG(LogTemp, Display, TEXT("Infusion Active: %s"), State->bInfusionActive ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogTemp, Display, TEXT("Spells Sealed: %s"), State->bSpellsSealed ? TEXT("Yes") : TEXT("No"));

	UWeaponData *ActiveWeapon = GetActiveWeapon(Actor);
	if (ActiveWeapon)
	{
		UE_LOG(LogTemp, Display, TEXT("Active Weapon: %s"), *ActiveWeapon->WeaponName);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("Active Weapon: Unarmed"));
	}

	UBaseAttackData *Attack = GetActiveAttack(Actor);
	if (Attack)
	{
		UE_LOG(LogTemp, Display, TEXT("Active Attack: %s"), *Attack->AttackName);
	}

	TArray<UAbilityData *> Abilities = GetActiveAbilities(Actor);
	UE_LOG(LogTemp, Display, TEXT("Active Abilities: %d"), Abilities.Num());
	for (UAbilityData *Ability : Abilities)
	{
		if (Ability)
		{
			UE_LOG(LogTemp, Display, TEXT("  - %s"), *Ability->AbilityName);
		}
	}

	UE_LOG(LogTemp, Display, TEXT("================================"));
}
