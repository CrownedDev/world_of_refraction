// BrokenDarknessManager.cpp
// Implementation for BrokenDarkness transformation and absorption system

#include "BrokenDarknessManager.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "CharacterData.h"
#include "CharacterDataComponent.h"
#include "LoadoutComponent.h"
#include "DefenseSystem.h"
#include "WorldStatRequirements.h"
#include "StatConstants.h"
#include "ElementHelpers.h"

namespace BrokenDarknessConstants
{
	// Break System — tier-keyed, infusion-multiplied (L1 ×1.5, L2 ×2.0)
	// Per locked design (May 2026):
	// S=1.5%, A=1.0%, B=0.6%, C=0.3%, D=0.1%, E/F=0%.
	constexpr float BREAK_CHANCE_S_TIER = 0.015f;
	constexpr float BREAK_CHANCE_A_TIER = 0.010f;
	constexpr float BREAK_CHANCE_B_TIER = 0.006f;
	constexpr float BREAK_CHANCE_C_TIER = 0.003f;
	constexpr float BREAK_CHANCE_D_TIER = 0.001f;
	constexpr float BREAK_CHANCE_E_TIER = 0.0f;
	constexpr float BREAK_CHANCE_F_TIER = 0.0f;
	constexpr float BREAK_CHANCE_L1_MULTIPLIER = 1.5f;
	constexpr float BREAK_CHANCE_L2_MULTIPLIER = 2.0f;

	// Absorption
	constexpr float PARRY_ABSORPTION_MULT = 0.30f; // 30% of spell cost on parry
	constexpr float BLOCK_ABSORPTION_MULT = 0.15f; // 15% of spell cost on block

	// Stack Multipliers
	constexpr float STACK_0_MULT = 1.0f;
	constexpr float STACK_1_MULT = 1.0f;
	constexpr float STACK_2_MULT = 2.0f;
	constexpr float STACK_3_MULT = 4.0f;

	// Overload — CurrentEP may exceed MaxEP by this fraction of MaxEP before the
	// hard cap, so the overload window scales with the BD's stat-derived pool.
	constexpr float OVERLOAD_CAPACITY_FRACTION = 0.30f;

	// Aura Range (scales with MaxEnergy points)
	constexpr float AURA_RANGE_MIN = 2.0f; // 0 points - no coverage
	constexpr float AURA_RANGE_MAX = 4.5f; // 21 points - full 3x3 coverage
}

namespace
{
	/** Look up base break chance for a tier. Returns 0 for E/F. */
	float GetBaseBreakChance(EItemTier Tier)
	{
		switch (Tier)
		{
		case EItemTier::S_Tier: return BrokenDarknessConstants::BREAK_CHANCE_S_TIER;
		case EItemTier::A_Tier: return BrokenDarknessConstants::BREAK_CHANCE_A_TIER;
		case EItemTier::B_Tier: return BrokenDarknessConstants::BREAK_CHANCE_B_TIER;
		case EItemTier::C_Tier: return BrokenDarknessConstants::BREAK_CHANCE_C_TIER;
		case EItemTier::D_Tier: return BrokenDarknessConstants::BREAK_CHANCE_D_TIER;
		case EItemTier::E_Tier: return BrokenDarknessConstants::BREAK_CHANCE_E_TIER;
		case EItemTier::F_Tier: return BrokenDarknessConstants::BREAK_CHANCE_F_TIER;
		default: return 0.0f;
		}
	}

	/** Look up infusion-level multiplier. L0 = 1.0x, L1 = 1.5x, L2 = 2.0x. */
	float GetInfusionMultiplier(int32 InfusionLevel)
	{
		switch (InfusionLevel)
		{
		case 1: return BrokenDarknessConstants::BREAK_CHANCE_L1_MULTIPLIER;
		case 2: return BrokenDarknessConstants::BREAK_CHANCE_L2_MULTIPLIER;
		default: return 1.0f;
		}
	}
}

UBrokenDarknessManager::UBrokenDarknessManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBrokenDarknessManager::BeginPlay()
{
	Super::BeginPlay();

	// Initialize state
	bIsTransformed = false;
	bIsOverloaded = false;
	CurrentAlignmentElement = ESpellElement::Generic;
	CurrentAbsorptionStacks = 0;
	ConsecutiveAbsorptions = 0;

	if (AActor *Owner = GetOwner())
	{
		if (UCharacterDataComponent *CharComp = Owner->FindComponentByClass<UCharacterDataComponent>())
		{
			// Overload is re-evaluated on every owner energy change. Absorption
			// gain, cast spend, and overload drain all broadcast OnEPChanged,
			// so this binding is the single overload trigger point.
			CharComp->OnEPChanged.AddDynamic(this, &UBrokenDarknessManager::HandleOwnerEnergyChanged);

			// Character-created BD path: align the manager's internal flag with
			// CharacterDataComponent::IsBrokenDarkness(). Without this,
			// character-created BDs silently short-circuit absorption methods
			// (OnSuccessfulParry, OnSuccessfulBlock, ProcessForbiddenCast, etc.)
			// because they all check bIsTransformed.
			if (CharComp->IsBrokenDarkness())
			{
				bIsTransformed = true;
				UE_LOG(LogTemp, Log, TEXT("[BrokenDarkness] %s: auto-flipped bIsTransformed for character-created BD"),
					   *Owner->GetName());
			}
		}
	}
}

void UBrokenDarknessManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCharacterDataComponent *CharComp = GetCharComp())
	{
		CharComp->OnEPChanged.RemoveDynamic(this, &UBrokenDarknessManager::HandleOwnerEnergyChanged);
	}

	Super::EndPlay(EndPlayReason);
}

// ==================== BREAK SYSTEM ====================

bool UBrokenDarknessManager::RollForBreak(EItemTier Tier, int32 InfusionLevel, const FString &TriggerReason)
{
	if (bIsTransformed)
	{
		return false; // Already transformed
	}

	const float BaseChance = GetBaseBreakChance(Tier);
	const float Multiplier = GetInfusionMultiplier(InfusionLevel);
	const float Chance = BaseChance * Multiplier;

	AActor *Owner = GetOwner();

	// E/F tier (or any unmapped tier) — chance is 0, skip the roll entirely
	if (Chance <= 0.0f)
	{
		UE_LOG(LogTemp, Verbose, TEXT("BrokenDarkness: %s skipped break check (Tier too low, Reason: %s)"),
			   Owner ? *Owner->GetName() : TEXT("Unknown"),
			   *TriggerReason);
		return false;
	}

	const float Roll = FMath::FRand();
	const bool bBreaks = Roll < Chance;

	if (bBreaks)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("BrokenDarkness: %s BROKE! (Reason: %s, Tier: %s, Level: L%d, Roll: %.4f < %.4f)"),
			   Owner ? *Owner->GetName() : TEXT("Unknown"),
			   *TriggerReason,
			   *UEnum::GetValueAsString(Tier),
			   InfusionLevel,
			   Roll, Chance);

		TriggerTransformation();
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Display,
			   TEXT("BrokenDarkness: %s survived break check (Reason: %s, Tier: %s, Level: L%d, Roll: %.4f >= %.4f)"),
			   Owner ? *Owner->GetName() : TEXT("Unknown"),
			   *TriggerReason,
			   *UEnum::GetValueAsString(Tier),
			   InfusionLevel,
			   Roll, Chance);

		return false;
	}
}

bool UBrokenDarknessManager::DoesSpellExceedRequirements(USpellData *Spell, UCharacterData *Character)
{
	if (!Spell || !Character)
	{
		return false;
	}

	// Uses FWorldStatRequirements - if there's a deficit, character doesn't meet requirements
	return Spell->Requirements.GetTotalDeficit(Character) > 0;
}

bool UBrokenDarknessManager::DoesAbilityExceedRequirements(UAbilityData *Ability, UCharacterData *Character)
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

	// Energy carries over: whatever CurrentEP the character held at the moment
	// of transformation becomes their starting absorption buffer.

	// Reset alignment
	CurrentAlignmentElement = ESpellElement::Generic;
	CurrentAbsorptionStacks = 0;
	ConsecutiveAbsorptions = 0;

	AActor *Owner = GetOwner();

	// Update the runtime flag on CharacterDataComponent. This zeros regular
	// EP and is the canonical "this character is now BD" signal that
	// IsBrokenDarkness() reads.
	if (Owner)
	{
		if (UCharacterDataComponent *CharComp = Owner->FindComponentByClass<UCharacterDataComponent>())
		{
			CharComp->ServerSetBrokenDarkness(true);
		}
	}

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

	AActor *Owner = GetOwner();
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

void UBrokenDarknessManager::GrantAbsorptionEnergy(float Amount)
{
	// Public entry point for non-defense absorption sources (e.g. ItemExecutor
	// when a crystal is used on a BD). Defense absorption reaches the same path
	// via OnDefenseResolved -> AddAbsorptionEnergy.
	if (!bIsTransformed)
	{
		return;
	}

	AddAbsorptionEnergy(Amount);
}

void UBrokenDarknessManager::AddAbsorptionEnergy(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	UCharacterDataComponent *CharComp = GetCharComp();
	if (!CharComp)
	{
		return;
	}

	// BD energy is stored on CurrentEP. Absorption may push it above MaxEP into
	// overload — the ceiling is MaxEP + GetOverloadCapacity() (30% of MaxEP).
	// The OnEPChanged broadcast inside ServerGainBrokenDarknessEnergy drives
	// UpdateOverloadState via HandleOwnerEnergyChanged.
	const int32 AbsoluteMax = CharComp->MaxEP + FMath::RoundToInt(GetOverloadCapacity());
	CharComp->ServerGainBrokenDarknessEnergy(FMath::RoundToInt(Amount), AbsoluteMax);

	UE_LOG(LogTemp, Verbose, TEXT("BrokenDarkness: absorbed %.1f energy -> CurrentEP %d/%d (Overload: %s)"),
		   Amount, CharComp->CurrentEP, CharComp->MaxEP, bIsOverloaded ? TEXT("Yes") : TEXT("No"));
}

void UBrokenDarknessManager::RecordAbsorbedElement(ESpellElement Element)
{
	// Check if element can be absorbed
	if (!CanAbsorbElement(Element))
	{
		return;
	}

	// Single active slot: the most recent absorption is always Last(). Re-absorbing
	// a prior element moves it to the end rather than duplicating, so the array
	// stays a distinct, recency-ordered history and Last() is the active element.
	AbsorbedElements.Remove(Element);
	AbsorbedElements.Add(Element);

	// Process stacks and alignment
	ProcessElementAbsorption(Element);
}

// ==================== OVERLOAD STATE ====================

float UBrokenDarknessManager::GetOverloadEnergy() const
{
	const UCharacterDataComponent *CharComp = GetCharComp();
	if (CharComp && CharComp->CurrentEP > CharComp->MaxEP)
	{
		return static_cast<float>(CharComp->CurrentEP - CharComp->MaxEP);
	}
	return 0.0f;
}

float UBrokenDarknessManager::GetOverloadCapacity() const
{
	// Derived: 30% of the stat-derived MaxEP — no stored field. The overload
	// window scales with the BD's energy pool rather than a flat buffer.
	const UCharacterDataComponent *CharComp = GetCharComp();
	const int32 MaxEP = CharComp ? CharComp->MaxEP : 0;
	return MaxEP * BrokenDarknessConstants::OVERLOAD_CAPACITY_FRACTION;
}

void UBrokenDarknessManager::UpdateOverloadState()
{
	const UCharacterDataComponent *CharComp = GetCharComp();
	if (!CharComp)
	{
		return;
	}

	const bool bShouldBeOverloaded = CharComp->CurrentEP > CharComp->MaxEP;

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
	const UCharacterDataComponent *CharComp = GetCharComp();
	UE_LOG(LogTemp, Warning, TEXT("BrokenDarkness: %s entered OVERLOAD! (Energy: %d/%d)"),
		   Owner ? *Owner->GetName() : TEXT("Unknown"),
		   CharComp ? CharComp->CurrentEP : 0, CharComp ? CharComp->MaxEP : 0);

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
	const UCharacterDataComponent *CharComp = GetCharComp();
	UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: %s exited overload (Energy: %d/%d)"),
		   Owner ? *Owner->GetName() : TEXT("Unknown"),
		   CharComp ? CharComp->CurrentEP : 0, CharComp ? CharComp->MaxEP : 0);

	OnOverloadStateChanged.Broadcast(Owner, false);
}

void UBrokenDarknessManager::ProcessOverloadTick(const TArray<AActor *> &NearbyEnemies,
												 float StatusMultiplierBonus, float EfficiencyPercent)
{
	if (!bIsOverloaded || !bIsTransformed)
	{
		return;
	}

	AActor *Owner = GetOwner();

	// 1. Apply aura damage to nearby enemies
	float AuraDamage = BaseOverloadAuraDamage * StatusMultiplierBonus;
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
	float SelfDamage = BaseOverloadSelfDamage * StatusMultiplierBonus;
	ApplyDamageToActor(Owner, SelfDamage);
	OnOverloadDamage.Broadcast(Owner, Owner, SelfDamage);

	UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: Overload self-damage %.1f"), SelfDamage);

	// 3. Drain energy (efficiency reduces drain). The ServerSpendEnergy
	// broadcast re-evaluates overload via HandleOwnerEnergyChanged.
	float DrainMultiplier = 1.0f - (EfficiencyPercent * 0.01f);
	float EnergyDrain = BaseEnergyDrain * FMath::Max(0.1f, DrainMultiplier);

	if (UCharacterDataComponent *CharComp = GetCharComp())
	{
		CharComp->ServerSpendEnergy(FMath::RoundToInt(EnergyDrain));

		UE_LOG(LogTemp, Log, TEXT("BrokenDarkness: Energy drained %.1f (Efficiency: %.0f%%), now at %d"),
			   EnergyDrain, EfficiencyPercent, CharComp->CurrentEP);
	}
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

UCharacterDataComponent *UBrokenDarknessManager::GetCharComp() const
{
	AActor *Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UCharacterDataComponent>() : nullptr;
}

void UBrokenDarknessManager::HandleOwnerEnergyChanged(int32 InCurrentEP, int32 InMaxEP)
{
	// Overload is a Broken-Darkness mechanic — non-BD energy changes are ignored.
	if (!bIsTransformed)
	{
		return;
	}

	UpdateOverloadState();
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
	// Single active slot: only the most recent absorption is "active". Earlier
	// entries in AbsorbedElements are historical and do not count here.
	return AbsorbedElements.Num() > 0 && AbsorbedElements.Last() == Element;
}

bool UBrokenDarknessManager::IsElementCastable(AActor *Actor,
											   UCharacterDataComponent *CharComp,
											   UBrokenDarknessManager *BDManager,
											   ESpellElement Element)
{
	// Cannot resolve the character — do not block.
	if (!CharComp)
	{
		return true;
	}

	// Broken Darkness: Darkness (the BD default) is always castable; every other
	// element requires a session absorption. AbsorbedElements never contains
	// Darkness, so the Darkness case is checked explicitly here. An equipped
	// crystal channelling the element also unlocks the cast.
	if (CharComp->IsBrokenDarkness() && BDManager)
	{
		return Element == ESpellElement::Darkness
			|| BDManager->HasAbsorbedElement(Element)
			|| ULoadoutComponent::HasEquippedSourceForElement(Actor, Element);
	}

	// Non-BD: innate-element match, an innate any-element source
	// (Reality / BrokenDarkness), or an equipped crystal channelling the element.
	const UCharacterData *CharData = CharComp->CharacterData;
	if (!CharData)
	{
		return true;
	}
	return Element == CharData->InnateElement
		|| ElementHelpers::IsAnySpellSource(CharData->InnateElement)
		|| ULoadoutComponent::HasEquippedSourceForElement(Actor, Element);
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

float UBrokenDarknessManager::CalculateAuraRange() const
{
	if (!bIsOverloaded)
	{
		return 0.0f;
	}

	// Get character data for MaxEnergy points
	AActor *Owner = GetOwner();
	if (!Owner)
	{
		return BrokenDarknessConstants::AURA_RANGE_MIN;
	}

	UCharacterDataComponent *CharComp = Owner->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp || !CharComp->CharacterData)
	{
		return BrokenDarknessConstants::AURA_RANGE_MIN;
	}

	int32 MaxEnergyPoints = CharComp->CharacterData->GetTotalMaxEnergy();

	// Linear scaling: 0 points = MIN, 21 points = MAX
	float PointRatio = FMath::Clamp(
		(float)MaxEnergyPoints / (float)StatConstants::MAX_SUBSTAT_POINTS_PER_PILLAR,
		0.0f,
		1.0f);

	return BrokenDarknessConstants::AURA_RANGE_MIN +
		   (PointRatio * (BrokenDarknessConstants::AURA_RANGE_MAX - BrokenDarknessConstants::AURA_RANGE_MIN));
}