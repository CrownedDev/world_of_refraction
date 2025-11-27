// BrokenDarknessManager.cpp
// Implementation for BrokenDarkness transformation and absorption system

#include "BrokenDarknessManager.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "CharacterData.h"
#include "CharacterDataComponent.h"
#include "DefenseSystem.h"
#include "WorldStatRequirements.h"

namespace BrokenDarknessConstants
{
	// Break System
	constexpr float BREAK_CHANCE = 0.03f;  // 3% chance to transform

	// Absorption
	constexpr float PARRY_ABSORPTION_MULT = 0.30f;   // 30% of spell cost on parry
	constexpr float BLOCK_ABSORPTION_MULT = 0.15f;   // 15% of spell cost on block

	// Stack Multipliers
	constexpr float STACK_0_MULT = 1.0f;
	constexpr float STACK_1_MULT = 1.0f;
	constexpr float STACK_2_MULT = 2.0f;
	constexpr float STACK_3_MULT = 4.0f;
}

UBrokenDarknessManager::UBrokenDarknessManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBrokenDarknessManager::BeginPlay()
{
	Super::BeginPlay();

	// Initialize state
	AbsorptionEnergy = 0.0f;
	bIsTransformed = false;
	bIsOverloaded = false;
	CurrentAlignmentElement = ESpellElement::Generic;
	CurrentAbsorptionStacks = 0;
	ConsecutiveAbsorptions = 0;
}

// ==================== BREAK SYSTEM ====================

bool UBrokenDarknessManager::RollForBreak(const FString& TriggerReason)
{
	if (bIsTransformed)
	{
		return false;  // Already transformed
	}

	float Roll = FMath::FRand();
	bool bBreaks = Roll < BrokenDarknessConstants::BREAK_CHANCE;

	AActor* Owner = GetOwner();

	if (bBreaks)
	{
		UE_LOG(LogTemp, Warning, TEXT("BrokenDarkness: %s BROKE! (Trigger: %s, Roll: %.3f < %.3f)"),
			Owner ? *Owner->GetName() : TEXT("Unknown"),
			*TriggerReason,
			Roll,
			BrokenDarknessConstants::BREAK_CHANCE);

		TriggerTransformation();
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: %s survived break check (Trigger: %s, Roll: %.3f >= %.3f)"),
			Owner ? *Owner->GetName() : TEXT("Unknown"),
			*TriggerReason,
			Roll,
			BrokenDarknessConstants::BREAK_CHANCE);

		return false;
	}
}

bool UBrokenDarknessManager::DoesSpellExceedRequirements(USpellData* Spell, UCharacterData* Character)
{
	if (!Spell || !Character)
	{
		return false;
	}

	// Uses FWorldStatRequirements - if there's a deficit, character doesn't meet requirements
	return Spell->Requirements.GetTotalDeficit(Character) > 0;
}

bool UBrokenDarknessManager::DoesAbilityExceedRequirements(UAbilityData* Ability, UCharacterData* Character)
{
	if (!Ability || !Character)
	{
		return false;
	}

	// Uses FWorldStatRequirements - if there's a deficit, character doesn't meet requirements
	return Ability->Requirements.GetTotalDeficit(Character) > 0;
}

void UBrokenDarknessManager::ForceTransformation()
{
	if (!bIsTransformed)
	{
		UE_LOG(LogTemp, Warning, TEXT("BrokenDarkness: Force transformation triggered"));
		TriggerTransformation();
	}
}

void UBrokenDarknessManager::TriggerTransformation()
{
	if (bIsTransformed)
	{
		return;
	}

	bIsTransformed = true;

	// Start with 0 absorption energy - must absorb to cast
	AbsorptionEnergy = 0.0f;

	// Reset alignment
	CurrentAlignmentElement = ESpellElement::Generic;
	CurrentAbsorptionStacks = 0;
	ConsecutiveAbsorptions = 0;

	AActor* Owner = GetOwner();
	UE_LOG(LogTemp, Warning, TEXT("BrokenDarkness: %s has TRANSFORMED into Broken Darkness!"),
		Owner ? *Owner->GetName() : TEXT("Unknown"));

	// Broadcast event
	OnTransformed.Broadcast(Owner);

	// VFX/Audio trigger point
}

// ==================== FORBIDDEN ELEMENTS ====================

bool UBrokenDarknessManager::IsForbiddenElement(ESpellElement Element)
{
	return Element == ESpellElement::Light || Element == ESpellElement::Void;
}

bool UBrokenDarknessManager::CanAbsorbElement(ESpellElement Element)
{
	// Cannot absorb: Generic (nothing to absorb), Reality (too powerful), BrokenDarkness (self)
	if (Element == ESpellElement::Generic ||
		Element == ESpellElement::Reality ||
		Element == ESpellElement::BrokenDarkness)
	{
		return false;
	}

	return true;
}

float UBrokenDarknessManager::CalculateForbiddenCastDamage(float SpellBaseDamage) const
{
	return SpellBaseDamage * ForbiddenCastSelfDamagePercent;
}

bool UBrokenDarknessManager::ProcessForbiddenCast(ESpellElement SpellElement, float SpellBaseDamage)
{
	if (!bIsTransformed)
	{
		return false;
	}

	if (!IsForbiddenElement(SpellElement))
	{
		return false;
	}

	float SelfDamage = CalculateForbiddenCastDamage(SpellBaseDamage);
	
	AActor* Owner = GetOwner();
	ApplyDamageToActor(Owner, SelfDamage);

	UE_LOG(LogTemp, Warning, TEXT("BrokenDarkness: Forbidden element %s dealt %.1f self-damage to %s!"),
		*UEnum::GetValueAsString(SpellElement),
		SelfDamage,
		Owner ? *Owner->GetName() : TEXT("Unknown"));

	// Broadcast for VFX/UI feedback
	OnOverloadDamage.Broadcast(Owner, Owner, SelfDamage);

	return true;
}

// ==================== ABSORPTION ====================

void UBrokenDarknessManager::OnSuccessfulParry(float DamageBlocked, ESpellElement DamageElement)
{
	if (!bIsTransformed)
	{
		return;
	}

	// Check if element can be absorbed
	if (!CanAbsorbElement(DamageElement))
	{
		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Cannot absorb %s element"),
			*UEnum::GetValueAsString(DamageElement));
		return;
	}

	float EnergyGained = DamageBlocked * ParryAbsorptionRate;
	AddAbsorptionEnergy(EnergyGained);
	RecordAbsorbedElement(DamageElement);

	OnEnergyAbsorbed.Broadcast(GetOwner(), EnergyGained, DamageElement);

	UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Parry absorbed %.1f energy from %s"),
		EnergyGained,
		*UEnum::GetValueAsString(DamageElement));
}

void UBrokenDarknessManager::OnSuccessfulBlock(float DamageBlocked, ESpellElement DamageElement)
{
	if (!bIsTransformed)
	{
		return;
	}

	// Check if element can be absorbed
	if (!CanAbsorbElement(DamageElement))
	{
		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Cannot absorb %s element"),
			*UEnum::GetValueAsString(DamageElement));
		return;
	}

	float EnergyGained = DamageBlocked * BlockAbsorptionRate;
	AddAbsorptionEnergy(EnergyGained);
	RecordAbsorbedElement(DamageElement);

	OnEnergyAbsorbed.Broadcast(GetOwner(), EnergyGained, DamageElement);

	UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Block absorbed %.1f energy from %s"),
		EnergyGained,
		*UEnum::GetValueAsString(DamageElement));
}

bool UBrokenDarknessManager::SpendAbsorptionEnergy(float Amount)
{
	if (!bIsTransformed)
	{
		return false;
	}
	if (AbsorptionEnergy < Amount)
	{
		return false;
	}

	AbsorptionEnergy -= Amount;

	// Check if exiting overload
	UpdateOverloadState();

	return true;
}

void UBrokenDarknessManager::AddAbsorptionEnergy(float Amount)
{
	float OldEnergy = AbsorptionEnergy;

	// Allow exceeding max by OverloadCapacity
	float AbsoluteMax = MaxAbsorptionEnergy + OverloadCapacity;
	AbsorptionEnergy = FMath::Min(AbsorptionEnergy + Amount, AbsoluteMax);

	// Check for overload state change
	UpdateOverloadState();

	UE_LOG(LogTemp, Verbose, TEXT("BrokenDarkness: Energy %.1f -> %.1f (Max: %.1f, Overload: %s)"),
		OldEnergy, AbsorptionEnergy, MaxAbsorptionEnergy, bIsOverloaded ? TEXT("Yes") : TEXT("No"));
}

void UBrokenDarknessManager::RecordAbsorbedElement(ESpellElement Element)
{
	// Check if element can be absorbed
	if (!CanAbsorbElement(Element))
	{
		return;
	}

	// Track for hybrid spell availability
	if (!AbsorbedElements.Contains(Element))
	{
		AbsorbedElements.Add(Element);
	}

	// Process stacks and alignment
	ProcessElementAbsorption(Element);
}

// ==================== OVERLOAD STATE ====================

float UBrokenDarknessManager::GetOverloadEnergy() const
{
	if (AbsorptionEnergy > MaxAbsorptionEnergy)
	{
		return AbsorptionEnergy - MaxAbsorptionEnergy;
	}
	return 0.0f;
}

void UBrokenDarknessManager::UpdateOverloadState()
{
	bool bShouldBeOverloaded = AbsorptionEnergy > MaxAbsorptionEnergy;

	if (bShouldBeOverloaded && !bIsOverloaded)
	{
		EnterOverload();
	}
	else if (!bShouldBeOverloaded && bIsOverloaded)
	{
		ExitOverload();
	}
}

void UBrokenDarknessManager::EnterOverload()
{
	if (bIsOverloaded)
	{
		return;
	}

	bIsOverloaded = true;

	AActor* Owner = GetOwner();
	UE_LOG(LogTemp, Warning, TEXT("BrokenDarkness: %s entered OVERLOAD! (Energy: %.1f/%.1f)"),
		Owner ? *Owner->GetName() : TEXT("Unknown"),
		AbsorptionEnergy, MaxAbsorptionEnergy);

	OnOverloadStateChanged.Broadcast(Owner, true);

	// VFX/Audio trigger point
}

void UBrokenDarknessManager::ExitOverload()
{
	if (!bIsOverloaded)
	{
		return;
	}

	bIsOverloaded = false;

	AActor* Owner = GetOwner();
	UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: %s exited overload (Energy: %.1f/%.1f)"),
		Owner ? *Owner->GetName() : TEXT("Unknown"),
		AbsorptionEnergy, MaxAbsorptionEnergy);

	OnOverloadStateChanged.Broadcast(Owner, false);
}

void UBrokenDarknessManager::ProcessOverloadTick(const TArray<AActor*>& NearbyEnemies,
	float EffectDamageMultiplier, float EfficiencyPercent)
{
	if (!bIsOverloaded || !bIsTransformed)
	{
		return;
	}

	AActor* Owner = GetOwner();

	// 1. Apply aura damage to nearby enemies
	float AuraDamage = BaseOverloadAuraDamage * EffectDamageMultiplier;
	for (AActor* Enemy : NearbyEnemies)
	{
		if (Enemy && Enemy != Owner)
		{
			ApplyDamageToActor(Enemy, AuraDamage);
			OnOverloadDamage.Broadcast(Owner, Enemy, AuraDamage);

			UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: Overload aura dealt %.1f damage to %s"),
				AuraDamage, *Enemy->GetName());
		}
	}

	// 2. Apply self-damage
	float SelfDamage = BaseOverloadSelfDamage * EffectDamageMultiplier;
	ApplyDamageToActor(Owner, SelfDamage);
	OnOverloadDamage.Broadcast(Owner, Owner, SelfDamage);

	UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: Overload self-damage %.1f"), SelfDamage);

	// 3. Drain energy (efficiency reduces drain)
	float DrainMultiplier = 1.0f - (EfficiencyPercent * 0.01f);
	float EnergyDrain = BaseEnergyDrain * FMath::Max(0.1f, DrainMultiplier);

	AbsorptionEnergy = FMath::Max(0.0f, AbsorptionEnergy - EnergyDrain);

	UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: Energy drained %.1f (Efficiency: %.0f%%), now at %.1f"),
		EnergyDrain, EfficiencyPercent, AbsorptionEnergy);

	// 4. Check if exiting overload
	UpdateOverloadState();
}

void UBrokenDarknessManager::ApplyDamageToActor(AActor* Target, float Damage)
{
	if (!Target)
	{
		return;
	}

	UCharacterDataComponent* CharComp = Target->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp)
	{
		CharComp->ServerTakeDamage(FMath::RoundToInt(Damage));
	}
}

// ==================== ABSORPTION STACKS ====================

float UBrokenDarknessManager::GetStackStatusMultiplier() const
{
	switch (CurrentAbsorptionStacks)
	{
	case 0:
		return BrokenDarknessConstants::STACK_0_MULT;
	case 1:
		return BrokenDarknessConstants::STACK_1_MULT;
	case 2:
		return BrokenDarknessConstants::STACK_2_MULT;
	case 3:
	default:
		return BrokenDarknessConstants::STACK_3_MULT;
	}
}

void UBrokenDarknessManager::ProcessElementAbsorption(ESpellElement Element)
{
	AActor* Owner = GetOwner();
	ESpellElement OldAlignment = CurrentAlignmentElement;

	// Check if same element as current alignment
	if (Element == CurrentAlignmentElement && CurrentAlignmentElement != ESpellElement::Generic)
	{
		// Same element - increment toward next stack
		ConsecutiveAbsorptions++;

		// Stacks: Absorption 1 = Stack 0, Absorption 2 = Stack 1, etc.
		int32 NewStacks = FMath::Min(ConsecutiveAbsorptions - 1, MaxAbsorptionStacks);

		if (NewStacks != CurrentAbsorptionStacks)
		{
			CurrentAbsorptionStacks = NewStacks;
			OnStacksChanged.Broadcast(Owner, Element, CurrentAbsorptionStacks);

			UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: %s stacks increased to %d (x%.1f status mult)"),
				*UEnum::GetValueAsString(Element),
				CurrentAbsorptionStacks,
				GetStackStatusMultiplier());
		}
	}
	else
	{
		// Different element - reset stacks, change alignment
		ResetStacks();

		CurrentAlignmentElement = Element;
		ConsecutiveAbsorptions = 1;
		CurrentAbsorptionStacks = 0;

		OnAlignmentChanged.Broadcast(Owner, OldAlignment, Element);
		OnStacksChanged.Broadcast(Owner, Element, 0);

		UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: Alignment changed %s -> %s (stacks reset)"),
			*UEnum::GetValueAsString(OldAlignment),
			*UEnum::GetValueAsString(Element));
	}

	// Update last absorbed for hybrid spells
	LastAbsorbedElement = Element;
}

void UBrokenDarknessManager::ResetStacks()
{
	CurrentAbsorptionStacks = 0;
	ConsecutiveAbsorptions = 0;
}

// ==================== HYBRID SPELLS ====================

bool UBrokenDarknessManager::HasAbsorbedElement(ESpellElement Element) const
{
	return AbsorbedElements.Contains(Element);
}

bool UBrokenDarknessManager::CanCastHybridSpell(ESpellElement SecondaryElement) const
{
	if (!bIsTransformed)
	{
		return false;
	}

	// Must have absorbed this element
	return HasAbsorbedElement(SecondaryElement);
}

// ==================== DEFENSE SYSTEM INTEGRATION ====================

void UBrokenDarknessManager::OnDefenseResolved(EDefenseType DefenseType,
	const FDefenseResult& DefenseResult, ESpellElement AttackElement, float AttackEnergyCost)
{
	if (!bIsTransformed)
	{
		return;
	}

	// Dodge doesn't absorb - you avoided it entirely
	if (DefenseType == EDefenseType::Dodge)
	{
		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Dodge - nothing to absorb"));
		return;
	}

	// Only Block and Parry absorb
	if (DefenseType != EDefenseType::Block && DefenseType != EDefenseType::Parry)
	{
		return;
	}

	// Must be successful defense
	if (!DefenseResult.bSuccess && !DefenseResult.bWasInWindow)
	{
		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Defense failed - no absorption"));
		return;
	}

	// Check if element can be absorbed
	if (!CanAbsorbElement(AttackElement))
	{
		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Cannot absorb %s element"),
			*UEnum::GetValueAsString(AttackElement));
		return;
	}

	// Calculate energy gained
	float EnergyGained = CalculateAbsorptionEnergy(DefenseType, AttackEnergyCost);

	// Add energy
	AddAbsorptionEnergy(EnergyGained);

	// Record element (handles stacks and alignment)
	RecordAbsorbedElement(AttackElement);

	// Broadcast
	OnEnergyAbsorbed.Broadcast(GetOwner(), EnergyGained, AttackElement);

	UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: %s absorbed %.1f energy from %s (Stacks: %d)"),
		DefenseType == EDefenseType::Parry ? TEXT("Parry") : TEXT("Block"),
		EnergyGained,
		*UEnum::GetValueAsString(AttackElement),
		CurrentAbsorptionStacks);
}

float UBrokenDarknessManager::CalculateAbsorptionEnergy(EDefenseType DefenseType, float AttackEnergyCost) const
{
	float AbsorptionMult = 0.0f;

	switch (DefenseType)
	{
	case EDefenseType::Parry:
		AbsorptionMult = BrokenDarknessConstants::PARRY_ABSORPTION_MULT;
		break;
	case EDefenseType::Block:
		AbsorptionMult = BrokenDarknessConstants::BLOCK_ABSORPTION_MULT;
		break;
	default:
		return 0.0f;
	}

	return AttackEnergyCost * AbsorptionMult;
}
