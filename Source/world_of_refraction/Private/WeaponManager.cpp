// Copyright Epic Games, Inc. All Rights Reserved.

#include "WeaponManager.h"
#include "WeaponData.h"
#include "WeaponAttackData.h"
#include "AbilityData.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "StatusEffectManager.h"
#include "StatusEffect.h"
#include "LoadoutComponent.h"
#include "ItemData.h"
#include "BreakCalculator.h"

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

	ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
	UCharacterData *CharData = GetCharacterData(Actor);

	if (!LoadoutComp && !CharData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WeaponManager] Cannot initialize - no LoadoutComponent or CharacterData for %s"),
			   *Actor->GetName());
		return;
	}

	FWeaponState State;

	// Determine initial state from LoadoutComponent or CharacterData
	bool bIsGeneric = CharData ? CharData->IsGeneric() : (LoadoutComp && LoadoutComp->CharacterClass == ECharacterClass::Generic);
	bool bUsingPrimary = LoadoutComp ? LoadoutComp->IsUsingPrimary() : true;

	if (bIsGeneric)
	{
		State.ActiveSlot = bUsingPrimary ? EWeaponSlot::Primary : EWeaponSlot::Secondary;
		State.bInfusionActive = false;
	}
	else
	{
		State.ActiveSlot = bUsingPrimary ? EWeaponSlot::Primary : EWeaponSlot::Unarmed;
	}

	State.ConjuredWeapon = nullptr;
	State.bSpellsSealed = false;

	WeaponStates.Add(Actor, State);

	// Subscribe to OnCrystalBroken for every slotted crystal on the actor's weapons
	SubscribeToActorWeaponCrystals(Actor);

	UE_LOG(LogTemp, Log, TEXT("[WeaponManager] Initialized weapon state for %s: Slot=%d"),
		   *Actor->GetName(), static_cast<int32>(State.ActiveSlot));
}

void UWeaponManager::ClearWeaponState(AActor *Actor)
{
	// Unsubscribe BEFORE removing state so the helper can iterate the loadout
	UnsubscribeFromActorWeaponCrystals(Actor);

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
	ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
	if (LoadoutComp)
	{
		return LoadoutComp->GetActiveWeapon();
	}
	return nullptr;
}

UWeaponAttackData *UWeaponManager::GetActiveAttack(AActor *Actor) const
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

	// Unarmed - no base attack (attacks come from weapons only)
	return nullptr;
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

	ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
	UCharacterData *CharData = GetCharacterData(Actor);

	if (!LoadoutComp && !CharData)
		return Slots;

	Slots.Add(EWeaponSlot::Unarmed);

	// Check primary weapon availability
	if (LoadoutComp)
	{
		if (LoadoutComp->GetPrimaryWeapon())
			Slots.Add(EWeaponSlot::Primary);

		if (LoadoutComp->GetSecondaryWeapon())
			Slots.Add(EWeaponSlot::Secondary);
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
	return true; // All weapons can be infused //return Weapon->bCanBeInfused;
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
	if (!IsCasterCharacter(Actor))
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

	UWeaponAttackData *Attack = GetActiveAttack(Attacker);
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
	float DamageMultiplier = AttackerData->CalculateRawDamage();
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
				bUseInfusion, bUseInfusion ? AttackerData->InnateElement : ESpellElement::Generic,
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

		// Apply unified status buildup (both physical and elemental)
		if (WeaponAttack)
		{
			// Weapons don't have charge levels yet (just toggle), so InfusionLevel = 0
			int32 InfusionLevel = 0;

			ApplyWeaponStatusBuildup(Attacker, Target, Weapon, WeaponAttack, InfusionLevel);
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
	case EPhysicalDamageType::Impact:
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
ULoadoutComponent *UWeaponManager::GetLoadoutComponent(AActor *Actor) const
{
	if (!Actor)
		return nullptr;
	return Actor->FindComponentByClass<ULoadoutComponent>();
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
	return Data && Data->IsGeneric();
}

bool UWeaponManager::IsCasterCharacter(AActor *Actor) const
{
	UCharacterData *Data = GetCharacterData(Actor);
	return Data && Data->IsCaster();
}

bool UWeaponManager::IsResonatorCharacter(AActor *Actor) const
{
	UCharacterData *Data = GetCharacterData(Actor);
	return Data && Data->IsResonator();
}

UWeaponData *UWeaponManager::GetWeaponInSlot(AActor *Actor, EWeaponSlot Slot) const
{
	const FWeaponState *State = WeaponStates.Find(Actor);

	switch (Slot)
	{
	case EWeaponSlot::Unarmed:
		return nullptr;

	case EWeaponSlot::Primary:
	{
		ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
		if (LoadoutComp)
		{
			return LoadoutComp->GetPrimaryWeapon();
		}
		return nullptr;
	}

	case EWeaponSlot::Secondary:
	{
		ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
		if (LoadoutComp)
		{
			return LoadoutComp->GetSecondaryWeapon();
		}
		return nullptr;
	}

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
			FMath::Rand(),			 // Unique ID
			15.0f,					 // Damage per turn
			3,						 // Duration
			ESpellElement::Generic); // Physical, no element
		break;

	case EPhysicalDamageType::Pierce:
		// Armor Break - Defense debuff (CreateBuff works for debuffs too)
		Effect = FStatusEffect::CreateBuff(
			TEXT("Armor Break"),
			FMath::Rand(),
			EStatusType::DefenseDebuff,
			30.0f, // 30% defense reduction
			3);
		Effect.Element = ESpellElement::Generic;
		break;

	case EPhysicalDamageType::Impact:
		// Stun - Skip turn (using AttackSpeedDebuff as placeholder)
		// TODO: Add proper Stun type to EStatusType
		Effect = FStatusEffect::CreateBuff(
			TEXT("Stun"),
			FMath::Rand(),
			EStatusType::AttackSpeedDebuff, // Placeholder for stun
			100.0f,							// 100% speed reduction = skip turn
			1);								// 1 turn
		Effect.Element = ESpellElement::Generic;
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
										bool bIsElemental, ESpellElement Element, bool bCanCrit, FWeaponAttackResult &OutResult)
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
			float DefenseBuffPercent = StatusManager->GetTotalStatModifier(Target, EStatusType::DefenseBuff);
			float DefenseDebuffPercent = StatusManager->GetTotalStatModifier(Target, EStatusType::DefenseDebuff);
			float DefenseModifier = 1.0f + (DefenseBuffPercent - DefenseDebuffPercent) / 100.0f;
			FlatDefense = FMath::RoundToInt(FlatDefense * FMath::Max(0.0f, DefenseModifier));
		}

		FinalDamage = FMath::Max(0, FinalDamage - FlatDefense);

		// Step 2: Apply resistance (percentage) - elemental damage only
		if (bIsElemental && FinalDamage > 0)
		{
			float Resistance = TargetData->CalculateResistance(); // Returns 0.0-1.0
			Resistance = FMath::Clamp(Resistance, 0.0f, 0.8f);	  // Cap at 80%
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
			CritChance += StatusManager->GetTotalStatModifier(Attacker, EStatusType::CritChanceBuff);
			CritChance -= StatusManager->GetTotalStatModifier(Attacker, EStatusType::CritChanceDebuff);
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

URingData *UWeaponManager::GetPrimaryRing(AActor *Actor) const
{
	ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
	if (LoadoutComp)
	{
		return LoadoutComp->GetPrimaryRing();
	}
	return nullptr;
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

	UWeaponAttackData *Attack = GetActiveAttack(Actor);
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

void UWeaponManager::ApplyWeaponStatusBuildup(AActor *Attacker, AActor *Target, UWeaponData *Weapon, UWeaponAttackData *Attack, int32 InfusionLevel)
{
	if (!Attacker || !Target || !Weapon || !Attack)
		return;

	UStatusEffectManager *StatusManager = GetStatusEffectManager();
	if (!StatusManager)
		return;

	// Determine status type from weapon's physical damage type
	EStatusType StatusType = EStatusType::None;

	switch (Attack->PhysicalDamageType)
	{
	case EPhysicalDamageType::Slash:
	case EPhysicalDamageType::Pierce:
		StatusType = EStatusType::DOT; // Bleed/Pierce bleed
		break;
	case EPhysicalDamageType::Impact:
		StatusType = EStatusType::DefenseDebuff; // Armor break
		break;
	default:
		StatusType = EStatusType::None;
	}

	// Calculate buildup
	float Buildup = Attack->StatusBuildup;

	// L1 infusion: +50% buildup
	if (InfusionLevel == 1)
		Buildup *= 1.5f;

	// Determine element (for display name)
	FWeaponState *State = WeaponStates.Find(Attacker);
	ESpellElement Element = ESpellElement::Generic; // Default to physical

	if (State && State->bInfusionActive)
	{
		// Get character's infusion element
		UCharacterDataComponent *CharComp = Attacker->FindComponentByClass<UCharacterDataComponent>();
		if (CharComp && CharComp->CharacterData)
		{
			Element = CharComp->CharacterData->InnateElement;
		}
	}

	// Add to status bar
	bool bTriggered = StatusManager->AddStatusBuildup(
		Attacker,
		Target,
		Buildup,
		StatusType,
		Element);

	if (bTriggered)
	{
		UE_LOG(LogTemp, Log, TEXT("[WeaponManager] %s triggered status %s on %s"),
			   *Weapon->WeaponName, *UEnum::GetValueAsString(StatusType), *Target->GetName());
	}

	// Apply immediate status (change to public method first)
	if (StatusType != EStatusType::None)
	{
		StatusManager->ApplyImmediateStatus(Attacker, Target, StatusType, Element);
	}
}

// ============================================================
// DURABILITY (Phase 4d)
// ============================================================

int32 UWeaponManager::ProcessPostCastWear(AActor *Actor, EItemTier ActionTier, int32 InfusionLevel, bool bIsSpell)
{
	if (!Actor)
	{
		return 0;
	}

	UWeaponData *Weapon = GetActiveWeapon(Actor);
	if (!Weapon || !Weapon->SlottedCrystal)
	{
		return 0;
	}

	UItemData *Crystal = Weapon->SlottedCrystal;
	if (!Crystal->bIsRefined || Crystal->bImmuneToBreaking)
	{
		// Evolution crystals or unrefined crystals: no wear, nothing to do
		return 0;
	}

	if (Crystal->IsBroken())
	{
		// Already broken — should never reach here (commit-time gate should have
		// excluded broken crystal from the source options). Defensive: don't double-process.
		UE_LOG(LogTemp, Warning,
			   TEXT("[WeaponManager] ProcessPostCastWear called on already-broken crystal '%s' for %s"),
			   *Crystal->GetFullItemName(), *Actor->GetName());
		return 0;
	}

	const int32 Wear = UBreakCalculator::CalculateDurabilityWear(
		Crystal->Tier,
		ActionTier,
		InfusionLevel,
		bIsSpell);

	if (Wear <= 0)
	{
		return 0;
	}

	UE_LOG(LogTemp, Verbose,
		   TEXT("[WeaponManager] %s applies %d wear to weapon crystal '%s' (%d/%d) [ActionTier=%d L%d bIsSpell=%d]"),
		   *Actor->GetName(), Wear,
		   *Crystal->GetFullItemName(),
		   Crystal->CurrentDurability, Crystal->MaxDurability,
		   static_cast<int32>(ActionTier), InfusionLevel, bIsSpell ? 1 : 0);

	Crystal->ApplyWear(Wear);
	// Note: ApplyWear fires OnCrystalBroken if it hits 0; we handle that in HandleWeaponCrystalBroken

	return Wear;
}

// ============================================================
// CRYSTAL SUBSCRIPTION & BREAK HANDLING
// ============================================================

void UWeaponManager::SubscribeToActorWeaponCrystals(AActor *Actor)
{
	if (!Actor)
	{
		return;
	}

	ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
	if (!LoadoutComp)
	{
		return;
	}

	// Iterate every weapon in the loadout — subscribe to crystals on each.
	// Currently uses UWeaponData::SlottedCrystal (asset-side); see TODO note in header.
	auto SubscribeToWeapon = [this](UWeaponData *Weapon)
	{
		if (!Weapon || !Weapon->SlottedCrystal)
		{
			return;
		}

		UItemData *Crystal = Weapon->SlottedCrystal;
		if (!Crystal->bIsRefined || Crystal->bImmuneToBreaking)
		{
			return;
		}

		if (!Crystal->OnCrystalBroken.IsAlreadyBound(this, &UWeaponManager::HandleWeaponCrystalBroken))
		{
			Crystal->OnCrystalBroken.AddDynamic(this, &UWeaponManager::HandleWeaponCrystalBroken);
		}
	};

	// Primary, Secondary, and conjured weapons all need monitoring.
	// LoadoutComponent's accessors are the source of truth.
	if (UWeaponData *Primary = LoadoutComp->GetPrimaryWeapon())
	{
		SubscribeToWeapon(Primary);
	}
	if (UWeaponData *Secondary = LoadoutComp->GetSecondaryWeapon())
	{
		SubscribeToWeapon(Secondary);
	}

	// Conjured weapon is runtime-spawned during combat; if a Caster has one
	// active we'd subscribe to it then. For now, conjured-time subscription
	// is a TODO when the conjure pipeline lands.
}

void UWeaponManager::UnsubscribeFromActorWeaponCrystals(AActor *Actor)
{
	if (!Actor)
	{
		return;
	}

	ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
	if (!LoadoutComp)
	{
		return;
	}

	auto UnsubscribeFromWeapon = [this](UWeaponData *Weapon)
	{
		if (!Weapon || !Weapon->SlottedCrystal)
		{
			return;
		}

		UItemData *Crystal = Weapon->SlottedCrystal;
		if (Crystal->OnCrystalBroken.IsAlreadyBound(this, &UWeaponManager::HandleWeaponCrystalBroken))
		{
			Crystal->OnCrystalBroken.RemoveDynamic(this, &UWeaponManager::HandleWeaponCrystalBroken);
		}
	};

	if (UWeaponData *Primary = LoadoutComp->GetPrimaryWeapon())
	{
		UnsubscribeFromWeapon(Primary);
	}
	if (UWeaponData *Secondary = LoadoutComp->GetSecondaryWeapon())
	{
		UnsubscribeFromWeapon(Secondary);
	}

	// Conjured: same TODO as in subscribe.
}

void UWeaponManager::HandleWeaponCrystalBroken(UItemData *BrokenCrystal)
{
	if (!BrokenCrystal)
	{
		return;
	}

	AActor *OwnerActor = nullptr;
	UWeaponData *OwnerWeapon = nullptr;
	if (!FindWeaponOwnerOfCrystal(BrokenCrystal, OwnerActor, OwnerWeapon))
	{
		// Crystal isn't on any weapon we're tracking — could be from a ring
		// (which has its own handler in URingManager) or an unrelated event
		return;
	}

	UE_LOG(LogTemp, Log,
		   TEXT("[WeaponManager] Crystal '%s' broke on %s's weapon '%s' — weapon stays equipped, downgrades to physical"),
		   *BrokenCrystal->GetFullItemName(),
		   *OwnerActor->GetName(),
		   *OwnerWeapon->WeaponName);

	// Broadcast the crystal-broken event for any listener (UI, VFX, audio)
	OnWeaponCrystalBroken.Broadcast(OwnerActor, OwnerWeapon, BrokenCrystal);

	// Per locked design: NO auto-switch. Weapon stays equipped, just loses
	// crystal-derived effects (element, Iolite enhancement, Quartz absorption).
	// WeaponData::GetWeaponElement already returns Generic when crystal IsBroken.
}

bool UWeaponManager::FindWeaponOwnerOfCrystal(UItemData *Crystal, AActor *&OutActor, UWeaponData *&OutWeapon) const
{
	OutActor = nullptr;
	OutWeapon = nullptr;

	if (!Crystal)
	{
		return false;
	}

	// Iterate every actor we have weapon state for; check their loadout's weapons.
	for (const auto &Pair : WeaponStates)
	{
		AActor *Actor = Pair.Key.Get();
		if (!Actor)
		{
			continue;
		}

		ULoadoutComponent *LoadoutComp = GetLoadoutComponent(Actor);
		if (!LoadoutComp)
		{
			continue;
		}

		auto CheckWeapon = [&](UWeaponData *Weapon) -> bool
		{
			if (Weapon && Weapon->SlottedCrystal == Crystal)
			{
				OutActor = Actor;
				OutWeapon = Weapon;
				return true;
			}
			return false;
		};

		if (CheckWeapon(LoadoutComp->GetPrimaryWeapon()))
		{
			return true;
		}
		if (CheckWeapon(LoadoutComp->GetSecondaryWeapon()))
		{
			return true;
		}
	}

	return false;
}