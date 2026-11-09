// CombatCameraTypes.cpp

#include "Combat/Camera/CombatCameraTypes.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Combat/Camera/CombatCinematicWindow.h"
#include "Combat/CombatNotify.h"
#include "Animation/AnimMontage.h"
#include "Combat/Camera/CameraFramingChecks.h"

namespace CombatCameraOffsets
{
	// TODO: tune in PIE (B-157 camera rig pass). Distances in cm.
	constexpr float CentredWideBack = 650.0f;
	constexpr float CentredWideHeight = 320.0f;
	// B-398 (R1): over-the-shoulder offsets as parts of the character-to-enemy-centre distance, so the layout (character
	// lower-left third, enemy centre in the middle) holds at any range. TODO: tune in PIE.
	constexpr float ShoulderBackPerRange = 0.35f;
	constexpr float ShoulderSidePerRange = 0.25f;
	constexpr float ShoulderHeightPerRange = 0.2f;
	/** The shortest range the offsets scale from, so a close enemy doesn't put the camera inside the character.
	 *  TODO: tune in PIE. */
	constexpr float ShoulderMinRange = 500.0f;
	// B-399 (R2): a sub-menu's step in, as parts of the turn camera's horizontal distance from the acting character
	// and of its height above it. TODO: tune in PIE.
	constexpr float MenuStepCloser = 0.25f;
	constexpr float MenuStepLower = 0.3f;
	constexpr float TacticalSide = 1400.0f;
	constexpr float TacticalHeight = 550.0f;
	constexpr float NotYourTurnBack = 1300.0f;
	constexpr float NotYourTurnHeight = 750.0f;
	constexpr float StaticBack = 1600.0f;
	constexpr float StaticHeight = 950.0f;
	constexpr float DefenceBack = 320.0f;
	constexpr float DefenceSide = 110.0f;
	constexpr float DefenceHeight = 170.0f;
	/** Extra distance back per cm the defenders stand from their centre (several targeted, Q-370 (3)). */
	constexpr float DefenceSpreadPullback = 1.2f;
}

namespace CombatCameraShots
{
	// TODO: tune in PIE (B-157: Crown approves the patterns' look). Distances in cm, times in s, angles in degrees.
	constexpr float WindUpStartBack = 300.0f;
	constexpr float WindUpEndBack = 200.0f;
	constexpr float WindUpSide = 80.0f;
	constexpr float WindUpHeight = 120.0f;
	constexpr float WindUpSeconds = 1.2f;
	constexpr float CastBack = 380.0f;
	constexpr float CastHeight = 180.0f;
	constexpr float CastSide = 140.0f;
	constexpr float CastBlendSeconds = 0.35f;
	constexpr float ImpactBack = 330.0f;
	constexpr float ImpactSide = 160.0f;
	constexpr float ImpactHeight = 140.0f;
	constexpr float ImpactBlendSeconds = 0.15f;
	constexpr float ImpactFovPunch = 8.0f;
	constexpr float CritFovPunch = 14.0f;
	constexpr float ImpactShake = 0.6f;
	constexpr float CritShake = 1.0f;
	constexpr float SpreadBack = 600.0f;
	constexpr float SpreadHeight = 380.0f;
	/** Extra distance back per cm the targets stand from their centre, so every one stays in frame. */
	constexpr float SpreadPullback = 1.6f;
	constexpr float SpreadBlendSeconds = 0.4f;
	constexpr float CloseScale = 0.75f;
	constexpr float WideScale = 1.4f;
	constexpr float BlendEaseExponent = 2.0f;
}

namespace CombatCameraTargetView
{
	// B-400 (study R3 / §9, Q-400). TODO: tune in PIE.
	/** Room above a target's head for its HP bar, as a part of its height. */
	constexpr float HealthBarHeadroom = 0.25f;
	/** The shortest target the shot frames, in cm, so a missing or tiny bound still gives a sensible shot. */
	constexpr float MinTargetHeight = 100.0f;
	/** The part of the screen's height the target and its HP bar fill in the over-the-shoulder medium shot. */
	constexpr float MediumShotFill = 0.6f;
	/** The closest the medium shot stands to the target, in cm. */
	constexpr float MediumShotMinDistance = 250.0f;
	/** The medium shot's step to the side and up, as parts of its distance: a three-quarter front view, slightly high. */
	constexpr float MediumShotSide = 0.35f;
	constexpr float MediumShotRise = 0.15f;
	/** Centred Wide / Tactical: how far the aim turns from the turn view's toward the target (0 none, 1 all the way). */
	constexpr float ReaimFraction = 0.35f;
	/** Centred Wide / Tactical: the re-aim's blend (Over-the-Shoulder cuts). */
	constexpr float ReaimBlendSeconds = 0.15f;
}

namespace CombatCameraWindow
{
	constexpr float MinPlayRate = FCombatCameraRules::MinPlayRate;
	constexpr float ReturnLegRate = FCombatCameraRules::ReturnLegRate;
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

bool FCombatCameraRules::ShouldPlayCinematic(ESkillCinematics Setting, bool bMovementOff, bool bCasterIsEnemy, bool bEnemyCinematicsOn)
{
	const bool bCinematicsOn = Setting == ESkillCinematics::On || (Setting == ESkillCinematics::FollowCameraMovement && !bMovementOff);
	return bCinematicsOn && (!bCasterIsEnemy || bEnemyCinematicsOn);
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

FTransform FCombatCameraRules::ComputeView(ECombatCameraMode Mode, ETurnViewStyle Style, const FCombatCameraFrame &Frame, int32 MenuDepth)
{
	using namespace CombatCameraOffsets;
	const FVector Forward = Frame.TowardEnemies.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);

	switch (Mode)
	{
	case ECombatCameraMode::Turn:
		return ComputeTurnView(Style, Frame, MenuDepth);
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

FTransform FCombatCameraRules::ComputeTurnView(ETurnViewStyle Style, const FCombatCameraFrame &Frame, int32 MenuDepth)
{
	using namespace CombatCameraOffsets;
	const FVector Forward = Frame.TowardEnemies.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);

	FVector Location = Frame.Focus; // every style sets it below
	FVector LookAt = Frame.EnemyCentre;
	switch (Style)
	{
	case ETurnViewStyle::OverTheShoulder:
	{
		// Aimed along this character's own line to the enemy centre (not the party's), and scaled by its length, so the
		// character lands in the same screen spot whoever acts. Behind and right of it: it sits left, below the aim.
		const FVector ToEnemies = Frame.EnemyCentre - Frame.Focus;
		const FVector Aim = ToEnemies.GetSafeNormal2D(UE_SMALL_NUMBER, Forward);
		const FVector AimRight = FVector::CrossProduct(FVector::UpVector, Aim);
		const float Range = FMath::Max(static_cast<float>(ToEnemies.Size2D()), ShoulderMinRange);
		const FVector Offset = -Aim * ShoulderBackPerRange + AimRight * ShoulderSidePerRange + FVector::UpVector * ShoulderHeightPerRange;
		Location = Frame.Focus + Offset * Range;
		break;
	}
	case ETurnViewStyle::Tactical:
		LookAt = (Frame.Focus + Frame.EnemyCentre) * 0.5f;
		Location = LookAt + Right * TacticalSide + FVector::UpVector * TacticalHeight;
		break;
	case ETurnViewStyle::CentredWide:
	default:
		Location = Frame.Focus - Forward * CentredWideBack + FVector::UpVector * CentredWideHeight;
		break;
	}

	if (FMath::Clamp(MenuDepth, 0, MaxMenuDepth) > 0)
	{
		// B-399 (study R2): one step in toward the acting character and down, still aimed where the view aims, so the
		// layout holds and the character grows in the frame.
		const FVector FromCharacter = Location - Frame.Focus;
		const FVector Horizontal(FromCharacter.X, FromCharacter.Y, 0.0f);
		Location = Frame.Focus + Horizontal * (1.0f - MenuStepCloser) + FVector::UpVector * FromCharacter.Z * (1.0f - MenuStepLower);
	}
	return LookFrom(Location, LookAt);
}

FVector FCombatCameraRules::GetTargetHealthBarPoint(const FVector &TargetFeet, float TargetHeight)
{
	using namespace CombatCameraTargetView;
	return TargetFeet + FVector::UpVector * FMath::Max(TargetHeight, MinTargetHeight) * (1.0f + HealthBarHeadroom);
}

float FCombatCameraRules::GetTargetViewBlendSeconds(ETurnViewStyle Style)
{
	return Style == ETurnViewStyle::OverTheShoulder ? 0.0f : CombatCameraTargetView::ReaimBlendSeconds;
}

FTransform FCombatCameraRules::ComputeTargetView(ETurnViewStyle Style, const FCombatCameraFrame &Frame, const FVector &TargetFeet, float TargetHeight,
												 int32 MenuDepth, int32 TargetCount)
{
	using namespace CombatCameraTargetView;
	const FTransform TurnView = ComputeTurnView(Style, Frame, MenuDepth);
	if (TargetCount < MinTargetsForTargetView)
	{
		return TurnView; // one target: the camera holds (study R3)
	}
	// The body and the HP bar above it, aimed at their middle.
	const float FramedHeight = FMath::Max(TargetHeight, MinTargetHeight) * (1.0f + HealthBarHeadroom);
	const FVector AimPoint = TargetFeet + FVector::UpVector * FramedHeight * 0.5f;

	if (Style != ETurnViewStyle::OverTheShoulder)
	{
		// A small re-aim from where the turn view stands: part of the way from its aim toward the target.
		const FVector Location = TurnView.GetLocation();
		const FVector TurnAim = TurnView.GetRotation().Vector();
		const FVector ToTarget = (AimPoint - Location).GetSafeNormal(UE_SMALL_NUMBER, TurnAim);
		const FVector Aim = (TurnAim * (1.0f - ReaimFraction) + ToTarget * ReaimFraction).GetSafeNormal(UE_SMALL_NUMBER, TurnAim);
		return FTransform(Aim.Rotation(), Location);
	}

	// A front medium shot: the camera on the side of the fight the target faces (an enemy faces the party, an ally the
	// enemies), far enough back that the framed height fills MediumShotFill of the screen's height.
	const FVector Forward = Frame.TowardEnemies.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const bool bOnEnemySide = FVector::DotProduct(TargetFeet - Frame.ArenaCentre, Forward) >= 0.0f;
	const FVector Facing = bOnEnemySide ? -Forward : Forward;
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Facing);
	const float VerticalHalfTan = FMath::Tan(FMath::DegreesToRadians(DefaultFieldOfView * 0.5f)) / CameraFraming::DEFAULT_ASPECT;
	const float Distance = FMath::Max(FramedHeight * 0.5f / (VerticalHalfTan * MediumShotFill), MediumShotMinDistance);
	const FVector Location = AimPoint + (Facing + Side * MediumShotSide + FVector::UpVector * MediumShotRise) * Distance;
	return LookFrom(Location, AimPoint);
}

#define LOCTEXT_NAMESPACE "CombatCameraSettings"

FText FCombatCameraRules::GetTurnViewSettingName()
{
	return LOCTEXT("TurnViewSetting", "Turn View");
}

FText FCombatCameraRules::GetTurnViewStyleName(ETurnViewStyle Style)
{
	switch (Style)
	{
	case ETurnViewStyle::OverTheShoulder:
		return LOCTEXT("TurnViewOverTheShoulder", "Over the Shoulder");
	case ETurnViewStyle::Tactical:
		return LOCTEXT("TurnViewTactical", "Tactical");
	case ETurnViewStyle::CentredWide:
	default:
		return LOCTEXT("TurnViewCentredWide", "Centred Wide");
	}
}

#undef LOCTEXT_NAMESPACE

FCinematicWindowTiming FCombatCameraRules::ReadWindowTiming(const USkillDataBase *Skill, float PlayRate, bool bSkipRitualLeg)
{
	FCinematicWindowTiming Timing;
	if (!Skill || !Skill->SkillMontage)
	{
		return Timing;
	}
	// TODO: per-action ActionMods also scale the runner's rate; the caller passes BaseAnimSpeed × the speed stat.
	const float Rate = FMath::Max(CombatCameraWindow::MinPlayRate, PlayRate);
	struct FLeg
	{
		const UAnimMontage *Montage;
		float Rate;
	};
	const FLeg Legs[] = {{bSkipRitualLeg ? nullptr : Skill->RitualCastMontage, Rate},
						 {Skill->SkillMontage, Rate},
						 {Skill->ReturnMontage, CombatCameraWindow::ReturnLegRate}};
	float Offset = 0.0f;
	for (const FLeg &Leg : Legs)
	{
		if (!Leg.Montage)
		{
			continue;
		}
		for (const FAnimNotifyEvent &Event : Leg.Montage->Notifies)
		{
			if (!Timing.bHasAuthoredWindow && Cast<UCombatCinematicWindow>(Event.NotifyStateClass))
			{
				Timing.bHasAuthoredWindow = true;
				Timing.AuthoredStart = Offset + Event.GetTriggerTime() / Leg.Rate;
				Timing.AuthoredEnd = Offset + Event.GetEndTriggerTime() / Leg.Rate;
			}
			const UCombatNotify *Notify = Cast<UCombatNotify>(Event.Notify);
			if (Notify && (Notify->Family == ECombatNotifyFamily::Impact || Notify->Family == ECombatNotifyFamily::Cast))
			{
				const float Defendable = FMath::Max(0.0f, Offset + Event.GetTriggerTime() / Leg.Rate - Notify->TelegraphLeadSeconds);
				Timing.FirstDefendable = Timing.bHasDefendable ? FMath::Min(Timing.FirstDefendable, Defendable) : Defendable;
				Timing.bHasDefendable = true;
			}
		}
		Offset += Leg.Montage->GetPlayLength() / Leg.Rate;
		if (Leg.Montage == Skill->RitualCastMontage && !bSkipRitualLeg)
		{
			Timing.bHasWindUpEnd = true;
			Timing.WindUpEnd = Offset;
		}
	}
	return Timing;
}

bool FCombatCameraRules::ResolveWindowEnd(const FCinematicWindowTiming &Timing, float &OutWindowEnd)
{
	if (!Timing.bHasAuthoredWindow && !Timing.bHasDefendable)
	{
		return false;
	}
	float End = Timing.bHasAuthoredWindow ? Timing.AuthoredEnd
			  : Timing.bHasWindUpEnd	  ? Timing.WindUpEnd
										  : Timing.FirstDefendable;
	if (Timing.bHasDefendable)
	{
		End = FMath::Min(End, Timing.FirstDefendable - MinDefenseReadSeconds);
	}
	OutWindowEnd = FMath::Max(0.0f, End);
	return true;
}

ECombatCameraMode FCombatCameraRules::SelectHandOffMode(bool bViewerDefends, bool bSequencePlaying, bool bMovementOff)
{
	if (bMovementOff)
	{
		return ECombatCameraMode::Static;
	}
	if (bViewerDefends)
	{
		return ECombatCameraMode::Defence;
	}
	return bSequencePlaying ? ECombatCameraMode::Action : ECombatCameraMode::NotYourTurn;
}

FTransform FCombatCameraRules::ComputeDefenceView(const TArray<FVector> &Defenders, const FVector &Attacker)
{
	using namespace CombatCameraOffsets;
	if (Defenders.IsEmpty())
	{
		return LookFrom(Attacker - FVector::ForwardVector * DefenceBack + FVector::UpVector * DefenceHeight, Attacker);
	}
	FVector Centre = FVector::ZeroVector;
	for (const FVector &Defender : Defenders)
	{
		Centre += Defender;
	}
	Centre /= Defenders.Num();
	float Spread = 0.0f;
	for (const FVector &Defender : Defenders)
	{
		Spread = FMath::Max(Spread, FVector::Dist2D(Defender, Centre));
	}
	const FVector Forward = (Attacker - Centre).GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const float Back = DefenceBack + Spread * DefenceSpreadPullback;
	return LookFrom(Centre - Forward * Back + Right * DefenceSide + FVector::UpVector * DefenceHeight, Attacker);
}

bool FCombatCameraRules::UsesShotPatterns(const USkillDataBase *Skill)
{
	return Skill && Skill->CameraSequence.IsNull();
}

ECameraShotMoment FCombatCameraRules::ImpactMomentFor(int32 TargetCount)
{
	return TargetCount > 1 ? ECameraShotMoment::Spread : ECameraShotMoment::Impact;
}

FCameraShot FCombatCameraRules::ComputeShot(ECameraShotMoment Moment, const FCameraShotContext &Context, const FSkillCameraTuning &Tuning)
{
	using namespace CombatCameraShots;
	FVector TargetCentre = Context.Caster + FVector::ForwardVector;
	if (!Context.Targets.IsEmpty())
	{
		TargetCentre = FVector::ZeroVector;
		for (const FVector &Target : Context.Targets)
		{
			TargetCentre += Target;
		}
		TargetCentre /= Context.Targets.Num();
	}
	const FVector Forward = (TargetCentre - Context.Caster).GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const float SideSign = Tuning.Side == ECameraShotSide::Left ? -1.0f : 1.0f;
	const bool bWide = Tuning.Style == ECameraShotStyle::Wide || (Tuning.Style == ECameraShotStyle::Default && Context.bRanged);
	const float StyleScale = Tuning.Style == ECameraShotStyle::Close ? CloseScale : bWide ? WideScale : 1.0f;
	const float Scale = FMath::Max(Tuning.DistanceScale, UE_KINDA_SMALL_NUMBER) * StyleScale;

	FCameraShot Shot;
	Shot.FieldOfView = DefaultFieldOfView;
	switch (Moment)
	{
	case ECameraShotMoment::WindUp:
	{
		// B-372 review: the height scales with the distance, so a Close wind-up still frames the caster.
		const FVector Shoulder = Right * WindUpSide * SideSign * Scale + FVector::UpVector * WindUpHeight * Scale;
		Shot.Start = LookFrom(Context.Caster - Forward * WindUpStartBack * Scale + Shoulder, TargetCentre);
		Shot.End = LookFrom(Context.Caster - Forward * WindUpEndBack * Scale + Shoulder, TargetCentre);
		Shot.BlendSeconds = WindUpSeconds;
		break;
	}
	case ECameraShotMoment::Cast:
	{
		const FVector LookAt = Tuning.bFollowProjectile && Context.Projectile.IsSet() ? Context.Projectile.GetValue() : TargetCentre;
		Shot.End = LookFrom(Context.Caster - Forward * CastBack * Scale + Right * CastSide * SideSign * Scale + FVector::UpVector * CastHeight, LookAt);
		Shot.Start = Shot.End;
		Shot.BlendSeconds = CastBlendSeconds;
		break;
	}
	case ECameraShotMoment::Impact:
	{
		const FVector Target = Context.Targets.IsEmpty() ? TargetCentre : Context.Targets[0];
		// From the caster's side of the target, beside it, so the hit reads toward the camera.
		Shot.End = LookFrom(Target - Forward * ImpactBack * Scale + Right * ImpactSide * SideSign * Scale + FVector::UpVector * ImpactHeight, Target);
		Shot.Start = Shot.End;
		Shot.BlendSeconds = ImpactBlendSeconds;
		Shot.FieldOfView = DefaultFieldOfView - (Context.bCritical ? CritFovPunch : ImpactFovPunch) * Tuning.ShakeScale;
		Shot.ShakeScale = (Context.bCritical ? CritShake : ImpactShake) * Tuning.ShakeScale;
		break;
	}
	case ECameraShotMoment::Spread:
	default:
	{
		float Spread = 0.0f;
		for (const FVector &Target : Context.Targets)
		{
			Spread = FMath::Max(Spread, FVector::Dist2D(Target, TargetCentre));
		}
		const float Back = (SpreadBack + Spread * SpreadPullback) * Scale;
		Shot.End = LookFrom(TargetCentre - Forward * Back + FVector::UpVector * SpreadHeight * Scale, TargetCentre);
		Shot.Start = Shot.End;
		Shot.BlendSeconds = SpreadBlendSeconds;
		Shot.ShakeScale = (Context.bCritical ? CritShake : ImpactShake) * Tuning.ShakeScale;
		break;
	}
	}
	return Shot;
}

FTransform FCombatCameraRules::BlendShot(const FCameraShot &Shot, float Alpha)
{
	const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f), CombatCameraShots::BlendEaseExponent);
	FTransform Out;
	Out.Blend(Shot.Start, Shot.End, Eased);
	return Out;
}
