// CrystalData.cpp
// Implementation for crystal slot data asset

#include "CrystalData.h"

void UCrystalData::AddAbsorption(int32 DamageBlocked, ERefractionElement DamageElement)
{
	if (CrystalType != ECrystalSlotType::Quartz || bHasTransformed)
	{
		return;
	}

	CurrentAbsorption += DamageBlocked;

	// Track the element that's contributing most
	// For now, last element absorbed wins - could implement tracking later
	if (DamageElement != ERefractionElement::Generic &&
		DamageElement != ERefractionElement::BrokenDarkness)
	{
		AbsorbedElement = DamageElement;
	}

	// Check for automatic transformation
	if (HasReachedThreshold())
	{
		TriggerTransformation();
	}
}

float UCrystalData::GetAbsorptionProgress() const
{
	if (CrystalType != ECrystalSlotType::Quartz || AbsorptionThreshold <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(
		static_cast<float>(CurrentAbsorption) / static_cast<float>(AbsorptionThreshold),
		0.0f,
		1.0f
	);
}

void UCrystalData::TriggerTransformation()
{
	if (CrystalType != ECrystalSlotType::Quartz || bHasTransformed)
	{
		return;
	}

	bHasTransformed = true;

	// Quartz now has the absorbed element permanently
	// The absorbed element was set during AddAbsorption calls

	UE_LOG(LogTemp, Display, TEXT("Quartz '%s' transformed! Absorbed element: %s"),
		*CrystalName,
		*UEnum::GetValueAsString(AbsorbedElement));
}

void UCrystalData::ResetQuartzState()
{
	if (CrystalType != ECrystalSlotType::Quartz)
	{
		return;
	}

	CurrentAbsorption = 0;
	AbsorbedElement = ERefractionElement::Generic;
	bHasTransformed = false;
}

#if WITH_EDITOR
EDataValidationResult UCrystalData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// All crystals need a name
	if (CrystalName.IsEmpty() || CrystalName == TEXT("Unnamed Crystal"))
	{
		Context.AddWarning(FText::FromString(TEXT("Crystal should have a meaningful name")));
	}

	// Type-specific validation
	switch (CrystalType)
	{
	case ECrystalSlotType::Ilodite:
		if (EnhancementMultiplier <= 1.0f && BonusDamage == 0 && BonusStatusBuildup == 0)
		{
			Context.AddWarning(FText::FromString(
				TEXT("Ilodite crystal has no enhancement effect")));
		}
		break;

	case ECrystalSlotType::Quartz:
		if (AbsorptionThreshold < 100)
		{
			Context.AddError(FText::FromString(
				TEXT("Quartz absorption threshold should be at least 100")));
			Result = EDataValidationResult::Invalid;
		}
		break;

	case ECrystalSlotType::Evolution:
		if (EvolutionSpells.Num() == 0 && EvolutionAbilities.Num() == 0 && !StatBonuses.HasModifiers())
		{
			Context.AddWarning(FText::FromString(
				TEXT("Evolution crystal provides no spells, abilities, or stat bonuses")));
		}
		// Evolution crystals should specify an element
		if (Element == ERefractionElement::Generic)
		{
			Context.AddWarning(FText::FromString(
				TEXT("Evolution crystal should have a specific element, not Generic")));
		}
		break;

	case ECrystalSlotType::None:
		Context.AddError(FText::FromString(
			TEXT("Crystal type cannot be None")));
		Result = EDataValidationResult::Invalid;
		break;
	}

	// Icon check
	if (Icon == nullptr)
	{
		Context.AddWarning(FText::FromString(TEXT("Crystal has no icon assigned")));
	}

	return Result;
}
#endif
