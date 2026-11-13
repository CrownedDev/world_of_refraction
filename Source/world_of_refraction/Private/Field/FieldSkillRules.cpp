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
		return Type == EFieldMovementType::Dash || Type == EFieldMovementType::EnhancedSprint ||
			   Type == EFieldMovementType::Glide || Type == EFieldMovementType::HighJump || Type == EFieldMovementType::AirDash ||
			   Type == EFieldMovementType::Flight;
	}

	bool NeedsElementalSource(EFieldMovementType Type)
	{
		return Type != EFieldMovementType::Dash;
	}

	EFieldSkillRefusal CheckPick(EFieldMovementType Type, ESpellElement SourceElement, bool bUnlocked)
	{
		if (!IsBuilt(Type))
		{
			return EFieldSkillRefusal::NotBuiltYet;
		}
		if (NeedsElementalSource(Type) && !IsElemental(SourceElement))
		{
			return EFieldSkillRefusal::NeedsElementalSource;
		}
		if (NeedsUnlock(Type) && !bUnlocked)
		{
			return EFieldSkillRefusal::NotUnlocked;
		}
		return EFieldSkillRefusal::None;
	}

	EFieldSkillRefusal CheckEnhancedSprintStart(ESpellElement SourceElement, int32 CurrentEP, bool bUnlocked)
	{
		const EFieldSkillRefusal PickRefusal = CheckPick(EFieldMovementType::EnhancedSprint, SourceElement, bUnlocked);
		if (PickRefusal != EFieldSkillRefusal::None)
		{
			return PickRefusal;
		}
		return CurrentEP < ENHANCED_SPRINT_EP_PER_SECOND ? EFieldSkillRefusal::NotEnoughEP : EFieldSkillRefusal::None;
	}

	int32 TierToRank(EItemTier Tier)
	{
		return static_cast<int32>(Tier) + 1;
	}

	bool NeedsUnlock(EFieldMovementType Type)
	{
		return Type != EFieldMovementType::Dash;
	}

	TArray<EFieldAura> GetUnlockAuras(EFieldMovementType Type)
	{
		switch (Type)
		{
		case EFieldMovementType::Dash:
		case EFieldMovementType::AirDash:
			return {EFieldAura::Elemental};
		case EFieldMovementType::EnhancedSprint:
		case EFieldMovementType::HighJump:
			return {EFieldAura::Enhancement};
		case EFieldMovementType::Glide:
		case EFieldMovementType::Flight:
			return {EFieldAura::Manipulation};
		case EFieldMovementType::Teleport:
		case EFieldMovementType::PhaseStep:
		case EFieldMovementType::MarkAndRecall:
			return {EFieldAura::Elemental, EFieldAura::Manipulation};
		case EFieldMovementType::SuperLaunch:
			return {EFieldAura::Enhancement, EFieldAura::Elemental};
		case EFieldMovementType::Regroup:
			return {EFieldAura::Elemental, EFieldAura::Enhancement, EFieldAura::Manipulation};
		}
		return {};
	}

	int32 GetUnlockRank(EFieldMovementType Type)
	{
		switch (Type)
		{
		case EFieldMovementType::Dash:
		case EFieldMovementType::EnhancedSprint:
		case EFieldMovementType::Glide:
		case EFieldMovementType::HighJump:
		case EFieldMovementType::AirDash:
			return SIMPLE_UNLOCK_RANK;
		default:
			return ADVANCED_UNLOCK_RANK;
		}
	}

	bool CanElementUnlock(EFieldMovementType Type, ESpellElement Element)
	{
		return IsElemental(Element) && (Type != EFieldMovementType::Teleport || Element == TELEPORT_ELEMENT);
	}

	bool IsUnlockedByRanks(EFieldMovementType Type, ESpellElement Element, TFunctionRef<int32(EFieldAura)> BestRank)
	{
		if (!NeedsUnlock(Type))
		{
			return true;
		}
		if (!CanElementUnlock(Type, Element))
		{
			return false;
		}
		const int32 Needed = GetUnlockRank(Type);
		for (const EFieldAura Aura : GetUnlockAuras(Type))
		{
			if (BestRank(Aura) < Needed)
			{
				return false;
			}
		}
		return true;
	}

	EFieldSkillRefusal CheckFlightStart(bool bInNoFlyZone, int32 CurrentEP, const FFieldFlightLimits &Limits)
	{
		if (bInNoFlyZone && !Limits.bIgnoreNoFly)
		{
			return EFieldSkillRefusal::NoFlyZone;
		}
		return !Limits.bUnlimitedEP && CurrentEP < FLIGHT_EP_PER_SECOND ? EFieldSkillRefusal::NotEnoughEP : EFieldSkillRefusal::None;
	}

	double GetFlightCeilingZ(double TakeOffGroundZ, const FFieldFlightLimits &Limits)
	{
		return Limits.bNoCeiling ? TNumericLimits<double>::Max() : TakeOffGroundZ + FLIGHT_MAX_HEIGHT + Limits.ExtraCeiling;
	}

	EFieldSkillRefusal CheckGlideStart(bool bFalling, int32 CurrentEP)
	{
		if (!bFalling)
		{
			return EFieldSkillRefusal::NeedsAir;
		}
		return CurrentEP < GLIDE_EP_COST ? EFieldSkillRefusal::NotEnoughEP : EFieldSkillRefusal::None;
	}

	EFieldSkillRefusal CheckJump(bool bOnGround, bool bAirJumpUsed, int32 CurrentEP)
	{
		if (!bOnGround && bAirJumpUsed)
		{
			return EFieldSkillRefusal::AlreadyUsedInAir;
		}
		return CurrentEP < HIGH_JUMP_EP_COST ? EFieldSkillRefusal::NotEnoughEP : EFieldSkillRefusal::None;
	}

	EFieldSkillRefusal CheckAirDash(bool bFalling, bool bAirDashUsed, int32 CurrentEP)
	{
		if (!bFalling)
		{
			return EFieldSkillRefusal::NeedsAir;
		}
		if (bAirDashUsed)
		{
			return EFieldSkillRefusal::AlreadyUsedInAir;
		}
		return CurrentEP < AIR_DASH_EP_COST ? EFieldSkillRefusal::NotEnoughEP : EFieldSkillRefusal::None;
	}

	bool CanWallRun(bool bEnhancedSprintActive, bool bFalling, float GroundSpeed, bool bWallBeside, float SecondsOnWall)
	{
		return bEnhancedSprintActive && bFalling && IsMovingAtSprintSpeed(GroundSpeed) && bWallBeside &&
			   SecondsOnWall < WALL_RUN_MAX_SECONDS;
	}

	bool IsBlinkAirDash(ESpellElement Element)
	{
		return Element == ESpellElement::Lightning;
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

	FString AuraName(EFieldAura Aura)
	{
		return StaticEnum<EFieldAura>()->GetNameStringByValue(static_cast<int64>(Aura));
	}
}
