// BrokenDarknessManager.cpp
// Implementation for BrokenDarkness transformation system

#include "BrokenDarknessManager.h"
#include "SpellData.h"
#include "CharacterData.h"

// Corruption constants
namespace BrokenDarknessConstants
{
	constexpr float CORRUPTION_THRESHOLD = 100.0f;
	constexpr float BASE_CORRUPTION_CHANCE = 0.05f; // 5% per corrupting spell
	constexpr float HIGH_POWER_CORRUPTION_BONUS = 0.10f; // +10% for powerful spells
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
}

// ==================== TRANSFORMATION ====================

bool UBrokenDarknessManager::CanSpellCorrupt(USpellData* Spell) const
{
	if (!Spell) return false;

	// Only Darkness spells can corrupt
	if (Spell->Element != ERefractionElement::Darkness) return false;

	// Could add: specific corruption flag on SpellData
	// For now, all Darkness spells have small corruption chance

	return true;
}

void UBrokenDarknessManager::ProcessSpellCast(USpellData* Spell)
{
	if (bIsTransformed) return; // Already transformed
	if (!CanSpellCorrupt(Spell)) return;

	using namespace BrokenDarknessConstants;

	// Calculate corruption chance
	float CorruptionChance = BASE_CORRUPTION_CHANCE;

	// Higher tier spells have higher corruption chance
	// TODO: When SpellData has Tier, use it here
	// if (Spell->Tier >= ETier::A) CorruptionChance += HIGH_POWER_CORRUPTION_BONUS;

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
	if (bIsTransformed) return;

	CorruptionBuildup = FMath::Min(CorruptionBuildup + Amount,
		BrokenDarknessConstants::CORRUPTION_THRESHOLD);

	if (CorruptionBuildup >= BrokenDarknessConstants::CORRUPTION_THRESHOLD)
	{
		TriggerTransformation();
	}
}

void UBrokenDarknessManager::TriggerTransformation()
{
	if (bIsTransformed) return;

	bIsTransformed = true;

	// Start with 0 absorption energy - must absorb to cast
	AbsorptionEnergy = 0.0f;

	// Log transformation
	AActor* Owner = GetOwner();
	UE_LOG(LogTemp, Warning, TEXT("BrokenDarkness: %s has TRANSFORMED!"),
		Owner ? *Owner->GetName() : TEXT("Unknown"));

	// Broadcast event
	OnTransformed.Broadcast(Owner);

	// Could trigger: VFX, sound, UI notification, character model change, etc.
}

// ==================== ABSORPTION ====================

void UBrokenDarknessManager::OnSuccessfulParry(float DamageBlocked, ERefractionElement DamageElement)
{
	if (!bIsTransformed) return;

	// Physical attacks give nothing to absorb
	if (DamageElement == ERefractionElement::Generic)
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

void UBrokenDarknessManager::OnSuccessfulBlock(float DamageBlocked, ERefractionElement DamageElement)
{
	if (!bIsTransformed) return;

	// Physical attacks give nothing to absorb
	if (DamageElement == ERefractionElement::Generic)
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
	if (!bIsTransformed) return false;
	if (AbsorptionEnergy < Amount) return false;

	AbsorptionEnergy -= Amount;
	return true;
}

void UBrokenDarknessManager::AddAbsorptionEnergy(float Amount)
{
	AbsorptionEnergy = FMath::Min(AbsorptionEnergy + Amount, MaxAbsorptionEnergy);
}

void UBrokenDarknessManager::RecordAbsorbedElement(ERefractionElement Element)
{
	if (Element == ERefractionElement::Generic ||
		Element == ERefractionElement::BrokenDarkness)
	{
		return;
	}

	LastAbsorbedElement = Element;

	if (!AbsorbedElements.Contains(Element))
	{
		AbsorbedElements.Add(Element);
	}
}

// ==================== HYBRID SPELLS ====================

bool UBrokenDarknessManager::HasAbsorbedElement(ERefractionElement Element) const
{
	return AbsorbedElements.Contains(Element);
}

bool UBrokenDarknessManager::CanCastHybridSpell(ERefractionElement SecondaryElement) const
{
	if (!bIsTransformed) return false;

	// Must have absorbed this element
	return HasAbsorbedElement(SecondaryElement);
}
