// BrokenDarknessManager.cpp
// Implementation for BrokenDarkness transformation system

#include "BrokenDarknessManager.h"
#include "SpellData.h"
#include "CharacterData.h"
#include "EDefenseType.h"
#include "DefenseSystem.h"
#include "CharacterDataComponent.h"

// Corruption constants
namespace BrokenDarknessConstants
{
	constexpr float CORRUPTION_THRESHOLD = 100.0f;
	constexpr float BASE_CORRUPTION_CHANCE = 0.05f;		 // 5% per corrupting spell
	constexpr float HIGH_POWER_CORRUPTION_BONUS = 0.10f; // +10% for powerful spells

	/ Absorption constexpr float PARRY_ABSORPTION_MULT = 0.30f; // 30% of spell cost on parry
	constexpr float BLOCK_ABSORPTION_MULT = 0.15f;				// 15% of spell cost on block

	// Stack Multipliers
	constexpr float STACK_0_MULT = 1.0f;
	constexpr float STACK_1_MULT = 1.0f;
	constexpr float STACK_2_MULT = 2.0f;
	constexpr float STACK_3_MULT = 4.0f;

	// Overload
	constexpr float DEFAULT_OVERLOAD_CAPACITY = 30.0f;
	constexpr float BASE_AURA_DAMAGE = 15.0f;
	constexpr float BASE_SELF_DAMAGE = 15.0f;
	constexpr float BASE_ENERGY_DRAIN = 15.0f;
}

UBrokenDarknessManager::UBrokenDarknessManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBrokenDarknessManager::BeginPlay()
{
	Super::BeginPlay();

	// Initialize at 0
	AbsorptionEnergy = 0.0f;
	CorruptionBuildup = 0.0f;
	bIsTransformed = false;
	IsOverloaded = false;
	CurrentAlignmentElement = ESpellElement::Generic;
	CurrentAbsorptionStacks = 0;
	ConsecutiveAbsorptions = 0;
}

// ==================== TRANSFORMATION ====================

bool UBrokenDarknessManager::CanSpellCorrupt(USpellData *Spell) const
{
	if (!Spell)
		return false;

	// Only Darkness spells can corrupt
	if (Spell->Element != ESpellElement::Darkness)
		return false;

	// Could add: specific corruption flag on SpellData
	// For now, all Darkness spells have small corruption chance

	return true;
}

void UBrokenDarknessManager::ProcessSpellCast(USpellData *Spell)
{
	if (bIsTransformed)
		return; // Already transformed
	if (!CanSpellCorrupt(Spell))
		return;

	using namespace BrokenDarknessConstants;

	// Calculate corruption chance
	float CorruptionChance = BASE_CORRUPTION_CHANCE;

	// Higher tier spells have higher corruption chance
	if (Spell->Tier >= EItemTier::A_Tier)
	{
		CorruptionChance += HIGH_POWER_CORRUPTION_BONUS;
	};

	// Roll for corruption
	if (FMath::FRand() < CorruptionChance)
	{
		// Add corruption buildup
		float CorruptionAmount = FMath::RandRange(5.0f, 15.0f);
		AddCorruption(CorruptionAmount);

		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Corruption +%.1f (Total: %.1f)"),
			   CorruptionAmount, CorruptionBuildup);
	}
}

void UBrokenDarknessManager::AddCorruption(float Amount)
{
	if (bIsTransformed)
		return;

	CorruptionBuildup = FMath::Min(CorruptionBuildup + Amount,
								   BrokenDarknessConstants::CORRUPTION_THRESHOLD);

	if (CorruptionBuildup >= BrokenDarknessConstants::CORRUPTION_THRESHOLD)
	{
		TriggerTransformation();
	}
}

void UBrokenDarknessManager::TriggerTransformation()
{
	if (bIsTransformed)
		return;

	bIsTransformed = true;

	// Start with 0 absorption energy - must absorb to cast
	AbsorptionEnergy = 0.0f;

	// Log transformation
	AActor *Owner = GetOwner();
	UE_LOG(LogTemp, Warning, TEXT("BrokenDarkness: %s has TRANSFORMED!"),
		   Owner ? *Owner->GetName() : TEXT("Unknown"));

	// Broadcast event
	OnTransformed.Broadcast(Owner);

	// Could trigger: VFX, sound, UI notification, character model change, etc.
}

// ==================== ABSORPTION ====================

void UBrokenDarknessManager::OnSuccessfulParry(float DamageBlocked, ESpellElement DamageElement)
{
	if (!bIsTransformed)
		return;

	// Physical attacks give nothing to absorb
	if (DamageElement == ESpellElement::Generic)
	{
		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Cannot absorb physical damage"));
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
		return;

	// Physical attacks give nothing to absorb
	if (DamageElement == ESpellElement::Generic)
	{
		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Cannot absorb physical damage"));
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
		return false;
	if (AbsorptionEnergy < Amount)
		return false;

	AbsorptionEnergy -= Amount;
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

	UE_LOG(LogTemp, Verbose, TEXT("BrokenDarkness: Energy %.1f → %.1f (Max: %.1f, Overload: %s)"),
		   OldEnergy, AbsorptionEnergy, MaxAbsorptionEnergy, bIsOverloaded ? TEXT("Yes") : TEXT("No"));
}

void UBrokenDarknessManager::RecordAbsorbedElement(ESpellElement Element)
{
	if (Element == ESpellElement::Generic ||
		Element == ESpellElement::BrokenDarkness)
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

// ==================== HYBRID SPELLS ====================

bool UBrokenDarknessManager::HasAbsorbedElement(ESpellElement Element) const
{
	return AbsorbedElements.Contains(Element);
}

bool UBrokenDarknessManager::CanCastHybridSpell(ESpellElement SecondaryElement) const
{
	if (!bIsTransformed)
		return false;

	// Must have absorbed this element
	return HasAbsorbedElement(SecondaryElement);
}

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

	AActor *Owner = GetOwner();
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

	AActor *Owner = GetOwner();
	UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: %s exited overload (Energy: %.1f/%.1f)"),
		   Owner ? *Owner->GetName() : TEXT("Unknown"),
		   AbsorptionEnergy, MaxAbsorptionEnergy);

	OnOverloadStateChanged.Broadcast(Owner, false);
}

void UBrokenDarknessManager::ProcessOverloadTick(const TArray<AActor *> &NearbyEnemies,
												 float EffectDamageMultiplier, float EfficiencyPercent)
{
	if (!bIsOverloaded || !bIsTransformed)
	{
		return;
	}

	AActor *Owner = GetOwner();

	// 1. Apply aura damage to nearby enemies
	float AuraDamage = BaseOverloadAuraDamage * EffectDamageMultiplier;
	for (AActor *Enemy : NearbyEnemies)
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
	// Efficiency 0% = full drain, Efficiency 100% = no drain
	float DrainMultiplier = 1.0f - (EfficiencyPercent * 0.01f);
	float EnergyDrain = BaseEnergyDrain * FMath::Max(0.1f, DrainMultiplier); // Minimum 10% drain

	AbsorptionEnergy = FMath::Max(0.0f, AbsorptionEnergy - EnergyDrain);

	UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: Energy drained %.1f (Efficiency: %.0f%%), now at %.1f"),
		   EnergyDrain, EfficiencyPercent, AbsorptionEnergy);

	// 4. Check if exiting overload
	UpdateOverloadState();
}

void UBrokenDarknessManager::ApplyDamageToActor(AActor *Target, float Damage)
{
	if (!Target)
	{
		return;
	}

	UCharacterDataComponent *CharComp = Target->FindComponentByClass<UCharacterDataComponent>();
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
	AActor *Owner = GetOwner();
	ESpellElement OldAlignment = CurrentAlignmentElement;
	int32 OldStacks = CurrentAbsorptionStacks;

	// Check if same element as current alignment
	if (Element == CurrentAlignmentElement && CurrentAlignmentElement != ESpellElement::Generic)
	{
		// Same element - increment stacks
		ConsecutiveAbsorptions++;

		// Stacks increase after certain absorption counts
		// Absorption 1 = Stack 0, Absorption 2 = Stack 1, Absorption 3 = Stack 2, Absorption 4+ = Stack 3
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

		UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: Alignment changed %s → %s (stacks reset)"),
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

// ==================== DEFENSE SYSTEM INTEGRATION ====================

void UBrokenDarknessManager::OnDefenseResolved(EDefenseType DefenseType,
											   const FDefenseResult &DefenseResult, ESpellElement AttackElement, float AttackEnergyCost)
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

	// Physical attacks give nothing to absorb
	if (AttackElement == ESpellElement::Generic)
	{
		UE_LOG(LogTemp, Display, TEXT("BrokenDarkness: Cannot absorb physical damage"));
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

// ============================================================
// DEBUG HELPER (optional - add to class)
// ============================================================
void UBrokenDarknessManager::DebugPrintState() const
{
	AActor *Owner = GetOwner();

	UE_LOG(LogTemp, Warning, TEXT("=== BrokenDarkness State: %s ==="), Owner ? *Owner->GetName() : TEXT("Unknown"));
	UE_LOG(LogTemp, Warning, TEXT("Transformed: %s"), bIsTransformed ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogTemp, Warning, TEXT("Energy: %.1f / %.1f"), AbsorptionEnergy, MaxAbsorptionEnergy);
	UE_LOG(LogTemp, Warning, TEXT("Overloaded: %s (Overflow: %.1f)"), bIsOverloaded ? TEXT("Yes") : TEXT("No"), GetOverloadEnergy());
	UE_LOG(LogTemp, Warning, TEXT("Alignment: %s"), *UEnum::GetValueAsString(CurrentAlignmentElement));
	UE_LOG(LogTemp, Warning, TEXT("Stacks: %d (x%.1f status)"), CurrentAbsorptionStacks, GetStackStatusMultiplier());
	UE_LOG(LogTemp, Warning, TEXT("Absorbed Elements: %d"), AbsorbedElements.Num());
	UE_LOG(LogTemp, Warning, TEXT("====================================="));
}
