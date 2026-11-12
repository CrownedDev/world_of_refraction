// FieldSkillRules.cpp

#include "Field/FieldSkillTypes.h"

namespace FieldSkillRules
{
	bool IsElemental(ESpellElement Element)
	{
		return Element != ESpellElement::Generic && Element != ESpellElement::None;
	}

	bool IsBuilt(EFieldMovementType Type)
	{
		return Type == EFieldMovementType::Dash || Type == EFieldMovementType::EnhancedSprint;
	}

	bool NeedsElementalSource(EFieldMovementType Type)
	{
		return Type != EFieldMovementType::Dash;
	}

	EFieldSkillRefusal CheckPick(EFieldMovementType Type, ESpellElement SourceElement)
	{
		if (!IsBuilt(Type))
		{
			return EFieldSkillRefusal::NotBuiltYet;
		}
		if (NeedsElementalSource(Type) && !IsElemental(SourceElement))
		{
			return EFieldSkillRefusal::NeedsElementalSource;
		}
		return EFieldSkillRefusal::None;
	}

	EFieldSkillRefusal CheckEnhancedSprintStart(ESpellElement SourceElement, int32 CurrentEP)
	{
		const EFieldSkillRefusal PickRefusal = CheckPick(EFieldMovementType::EnhancedSprint, SourceElement);
		if (PickRefusal != EFieldSkillRefusal::None)
		{
			return PickRefusal;
		}
		return CurrentEP < ENHANCED_SPRINT_EP_PER_SECOND ? EFieldSkillRefusal::NotEnoughEP : EFieldSkillRefusal::None;
	}

	bool IsMovingAtSprintSpeed(float GroundSpeed)
	{
		return GroundSpeed > ENHANCED_SPRINT_MIN_MOVING_SPEED;
	}

	float GetEnhancedSprintMultiplier(ESpellElement Element)
	{
		switch (Element)
		{
		case ESpellElement::Lightning:
			return ENHANCED_SPRINT_LIGHTNING_MULTIPLIER;
		case ESpellElement::Wind:
			return ENHANCED_SPRINT_WIND_MULTIPLIER;
		default:
			return IsElemental(Element) ? ENHANCED_SPRINT_OTHER_MULTIPLIER : NO_SPRINT_BOOST;
		}
	}

	FString TypeName(EFieldMovementType Type)
	{
		return StaticEnum<EFieldMovementType>()->GetNameStringByValue(static_cast<int64>(Type));
	}

	FString SourceName(EFieldSkillSource Source)
	{
		return StaticEnum<EFieldSkillSource>()->GetNameStringByValue(static_cast<int64>(Source));
	}

	FString RefusalName(EFieldSkillRefusal Refusal)
	{
		return StaticEnum<EFieldSkillRefusal>()->GetNameStringByValue(static_cast<int64>(Refusal));
	}
}
