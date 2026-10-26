// CombatCameraTypes.cpp

#include "Combat/Camera/CombatCameraTypes.h"
#include "Skills/Definitions/SkillDataBase.h"

namespace CombatCameraOffsets
{
	// TODO: tune in PIE (B-157 camera rig pass). Distances in cm.
	constexpr float CentredWideBack = 650.0f;
	constexpr float CentredWideHeight = 320.0f;
	constexpr float ShoulderBack = 260.0f;
	constexpr float ShoulderHeight = 130.0f;
	constexpr float ShoulderSide = 90.0f;
	constexpr float TacticalSide = 1400.0f;
	constexpr float TacticalHeight = 550.0f;
	constexpr float NotYourTurnBack = 1300.0f;
	constexpr float NotYourTurnHeight = 750.0f;
	constexpr float StaticBack = 1600.0f;
	constexpr float StaticHeight = 950.0f;
}

namespace
{
	FTransform LookFrom(const FVector &Location, const FVector &Target)
	{
		return FTransform((Target - Location).Rotation(), Location);
	}
}

ECombatCameraMode FCombatCameraRules::SelectMode(bool bActorIsViewers, bool bSequencePlaying, bool bMovementOff)
{
	if (bMovementOff)
	{
		return ECombatCameraMode::Static;
	}
	if (bSequencePlaying)
	{
		return ECombatCameraMode::Action;
	}
	return bActorIsViewers ? ECombatCameraMode::Turn : ECombatCameraMode::NotYourTurn;
}

ECameraTemplateKind FCombatCameraRules::TemplateKindFor(const USkillDataBase *Skill, EActionType ActionType)
{
	if (ActionType == EActionType::Spell)
	{
		return ECameraTemplateKind::Spell;
	}
	return (Skill && Skill->ExecutionType == EAbilityExecutionType::Ranged) ? ECameraTemplateKind::Ranged
																			  : ECameraTemplateKind::Melee;
}

FTransform FCombatCameraRules::ComputeView(ECombatCameraMode Mode, ETurnViewStyle Style, const FCombatCameraFrame &Frame)
{
	using namespace CombatCameraOffsets;
	const FVector Forward = Frame.TowardEnemies.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);

	switch (Mode)
	{
	case ECombatCameraMode::Turn:
		switch (Style)
		{
		case ETurnViewStyle::OverTheShoulder:
			return LookFrom(Frame.Focus - Forward * ShoulderBack + Right * ShoulderSide + FVector::UpVector * ShoulderHeight,
							Frame.EnemyCentre);
		case ETurnViewStyle::Tactical:
		{
			const FVector Middle = (Frame.Focus + Frame.EnemyCentre) * 0.5f;
			return LookFrom(Middle + Right * TacticalSide + FVector::UpVector * TacticalHeight, Middle);
		}
		case ETurnViewStyle::CentredWide:
		default:
			return LookFrom(Frame.Focus - Forward * CentredWideBack + FVector::UpVector * CentredWideHeight,
							Frame.EnemyCentre);
		}
	case ECombatCameraMode::NotYourTurn:
		return LookFrom(Frame.Focus - Forward * NotYourTurnBack + FVector::UpVector * NotYourTurnHeight, Frame.ArenaCentre);
	case ECombatCameraMode::Static:
		return LookFrom(Frame.ArenaCentre - Forward * StaticBack + FVector::UpVector * StaticHeight, Frame.ArenaCentre);
	case ECombatCameraMode::Action:
	case ECombatCameraMode::None:
	default:
		return LookFrom(Frame.Focus - Forward * CentredWideBack + FVector::UpVector * CentredWideHeight, Frame.EnemyCentre);
	}
}
