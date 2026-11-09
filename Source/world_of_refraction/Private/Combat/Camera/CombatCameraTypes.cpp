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
	// B-403 (study R5): a melee confirm rushes in fast with motion blur (0.2-0.3 s in the study). TODO: tune.
	constexpr float RushStartBack = 420.0f;
	constexpr float RushSeconds = 0.25f;
	constexpr float RushMotionBlur = 0.8f;
	// B-403 (study R6): the impact two-shot stands out to the side of the caster-target line, a little toward the
	// caster (three-quarter front of the target), low. Out = (ImpactSide + gap * ImpactSidePerGap) * scale, never
	// under gap * ImpactFitPerGap so both fighters stay in frame at any style. TODO: tune.
	constexpr float ImpactSide = 260.0f;
	constexpr float ImpactSidePerGap = 0.6f;
	constexpr float ImpactFitPerGap = 0.9f;
	constexpr float ImpactThreeQuarterBack = 120.0f;
	constexpr float ImpactHeight = 40.0f;
	constexpr float ImpactBlendSeconds = 0.15f;
	// B-403 (R6): a small punch and a light shake on a normal hit; a crit stays bigger (FeelGuide 4). TODO: tune.
	constexpr float ImpactFovPunch = 5.0f;
	constexpr float CritFovPunch = 10.0f;
	constexpr float ImpactShake = 0.25f;
	constexpr float CritShake = 0.6f;
	constexpr float SpreadBack = 600.0f;
	constexpr float SpreadHeight = 380.0f;
	/** Extra distance back per cm the targets stand from their centre, so every one stays in frame. */
	constexpr float SpreadPullback = 1.6f;
	constexpr float SpreadBlendSeconds = 0.4f;
	constexpr float CloseScale = 0.75f;
	constexpr float WideScale = 1.4f;
	constexpr float BlendEaseExponent = 2.0f;
	// B-406 (study R9): the named-attack front shot, a medium shot of the enemy from in front, pushing in.
	constexpr float NamedAttackStartFront = 520.0f;
	constexpr float NamedAttackEndFront = 380.0f;
	constexpr float NamedAttackSide = 60.0f;
	constexpr float NamedAttackHeight = 110.0f;
	constexpr float NamedAttackLookHeight = 90.0f;
	constexpr float NamedAttackSeconds = 1.0f;
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

namespace CombatCameraDefence
{
	// B-402 (study R10, R10b; Q-402). TODO: tune in PIE (B-157). Distances in cm, angles in degrees.
	/** Camera height above the fighters' mean centre (their actor locations): low, about hip height. */
	constexpr float CameraHeight = 20.0f;
	/** Where the camera looks, above the same centre: chest height, so the shot tilts up a little. */
	constexpr float AimHeight = 50.0f;
	/** How far the camera swings from square side-on toward the defenders' backs, so they sit nearer. */
	constexpr float SwingTowardDefendersDegrees = 20.0f;
	/** 1: the camera stands on the attacker's right (the weapon hand of most rigs); -1: its left. */
	constexpr float WeaponSideSign = 1.0f;
	/** Room kept beside each fighter's centre across the frame, so bodies and weapons stay whole. */
	constexpr float TwoShotPadding = 150.0f;
	constexpr float WidePadding = 220.0f;
	/** The nearest the camera stands to the shot's centre. */
	constexpr float TwoShotMinDistance = 450.0f;
	constexpr float WideMinDistance = 700.0f;
	/** Part of the half-width the fit keeps free: more than CameraFraming's margin, as the fit ignores the small tilt. */
	constexpr float FitMargin = 0.15f;
}

FTransform FCombatCameraRules::ComputeDefenceView(const TArray<FVector> &Defenders, const FVector &Attacker)
{
	using namespace CombatCameraDefence;
	FVector DefenderCentre = Attacker - FVector::ForwardVector * TwoShotMinDistance; // none listed: the attacker alone
	if (!Defenders.IsEmpty())
	{
		DefenderCentre = FVector::ZeroVector;
		for (const FVector &Defender : Defenders)
		{
			DefenderCentre += Defender;
		}
		DefenderCentre /= Defenders.Num();
	}
	const FVector Axis = (Attacker - DefenderCentre).GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	// The attacker faces back along the axis, so its right is Up x (-Axis).
	const FVector WeaponSide = FVector::CrossProduct(FVector::UpVector, -Axis) * WeaponSideSign;
	const float Swing = FMath::DegreesToRadians(SwingTowardDefendersDegrees);
	// From the camera toward the fight: square across the axis, swung so the camera stands nearer the defenders' backs.
	const FVector Look = (Axis * FMath::Sin(Swing) - WeaponSide * FMath::Cos(Swing)).GetSafeNormal2D(UE_SMALL_NUMBER, FVector::RightVector);
	const FVector Across = FVector::CrossProduct(FVector::UpVector, Look);

	TArray<FVector> Points = Defenders;
	Points.Add(Attacker);
	// Centre the frame on the fighters' extremes across the screen, so none is pushed to one edge.
	FVector Mid = (DefenderCentre + Attacker) * 0.5f;
	float MinAcross = TNumericLimits<float>::Max();
	float MaxAcross = TNumericLimits<float>::Lowest();
	for (const FVector &Point : Points)
	{
		const float Offset = FVector::DotProduct(Point - Mid, Across);
		MinAcross = FMath::Min(MinAcross, Offset);
		MaxAcross = FMath::Max(MaxAcross, Offset);
	}
	Mid += Across * ((MinAcross + MaxAcross) * 0.5f);

	// Stand back until every fighter (plus padding) fits the half-width: nearer fighters need more room.
	const bool bWide = Defenders.Num() > 1;
	const float Padding = bWide ? WidePadding : TwoShotPadding;
	const float HalfTan = FMath::Tan(FMath::DegreesToRadians(DefaultFieldOfView * 0.5f)) * (1.0f - FitMargin);
	float Distance = bWide ? WideMinDistance : TwoShotMinDistance;
	for (const FVector &Point : Points)
	{
		const FVector Offset = Point - Mid;
		const float AcrossDistance = FMath::Abs(FVector::DotProduct(Offset, Across));
		const float Depth = FVector::DotProduct(Offset, Look);
		Distance = FMath::Max(Distance, (AcrossDistance + Padding) / HalfTan - Depth);
	}
	return LookFrom(Mid - Look * Distance + FVector::UpVector * CameraHeight, Mid + FVector::UpVector * AimHeight);
}

FCameraShot FCombatCameraRules::SteadyForDefence(const FCameraShot &Shot, bool bViewerDefending)
{
	FCameraShot Out = Shot;
	if (bViewerDefending)
	{
		Out.ShakeScale = 0.0f;
		Out.FieldOfView = DefaultFieldOfView;
	}
	return Out;
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
		// B-403 (R5): a melee confirm is the loud moment: a short fast rush in with motion blur. Spells and ranged
		// skills keep the slow push (and so does camera movement off, R15).
		const bool bRush = !Context.bRanged && Context.bAllowRush;
		Shot.Start = LookFrom(Context.Caster - Forward * (bRush ? RushStartBack : WindUpStartBack) * Scale + Shoulder, TargetCentre);
		Shot.End = LookFrom(Context.Caster - Forward * WindUpEndBack * Scale + Shoulder, TargetCentre);
		Shot.BlendSeconds = bRush ? RushSeconds : WindUpSeconds;
		Shot.MotionBlur = bRush ? RushMotionBlur : 0.0f;
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
		// B-403 (R6): a side-on two-shot, low: out to the side of the caster-target line, a little toward the
		// caster (three-quarter front of the target), far enough out that both stay in frame.
		const FVector Line = Target - Context.Caster;
		const FVector Along = Line.GetSafeNormal2D(UE_SMALL_NUMBER, Forward);
		const FVector Across = FVector::CrossProduct(FVector::UpVector, Along);
		const float Gap = static_cast<float>(Line.Size2D());
		const float Out = FMath::Max((ImpactSide + Gap * ImpactSidePerGap) * Scale, Gap * ImpactFitPerGap);
		const FVector Middle = (Context.Caster + Target) * 0.5f;
		Shot.End = LookFrom(Middle + Across * Out * SideSign - Along * ImpactThreeQuarterBack * Scale + FVector::UpVector * ImpactHeight, Middle);
		Shot.Start = Shot.End;
		Shot.BlendSeconds = ImpactBlendSeconds;
		Shot.FieldOfView = DefaultFieldOfView - (Context.bCritical ? CritFovPunch : ImpactFovPunch) * Tuning.ShakeScale;
		Shot.ShakeScale = (Context.bCritical ? CritShake : ImpactShake) * Tuning.ShakeScale;
		break;
	}
	case ECameraShotMoment::NamedAttack:
	{
		// In front of the caster (between it and its targets), a little to the tuned side, looking back at it.
		const FVector LookAt = Context.Caster + FVector::UpVector * NamedAttackLookHeight * Scale;
		const FVector Offset = Right * NamedAttackSide * SideSign * Scale + FVector::UpVector * NamedAttackHeight * Scale;
		Shot.Start = LookFrom(Context.Caster + Forward * NamedAttackStartFront * Scale + Offset, LookAt);
		Shot.End = LookFrom(Context.Caster + Forward * NamedAttackEndFront * Scale + Offset, LookAt);
		Shot.BlendSeconds = NamedAttackSeconds;
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
		// B-403 (R8): the area effect gets room: the shot holds still to the end (no shake, no FOV punch), so the
		// whole effect and anything shown beside a target stay in frame.
		Shot.ShakeScale = 0.0f;
		break;
	}
	}
	return Shot;
}

ECameraShotMoment FCombatCameraRules::OpeningMomentFor(const FSkillCameraTuning &Tuning, bool bCasterIsEnemy)
{
	return Tuning.bNamedAttack && bCasterIsEnemy ? ECameraShotMoment::NamedAttack : ECameraShotMoment::WindUp;
}

bool FCombatCameraRules::ShouldPlayBossCinematic(bool bHasSequence, bool bHasLocalViewer, ESkillCinematics Setting, bool bMovementOff)
{
	return bHasSequence && bHasLocalViewer && ShouldPlayCinematic(Setting, bMovementOff, false, true);
}

FTransform FCombatCameraRules::BlendShot(const FCameraShot &Shot, float Alpha)
{
	const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f), CombatCameraShots::BlendEaseExponent);
	FTransform Out;
	Out.Blend(Shot.Start, Shot.End, Eased);
	return Out;
}

namespace CombatCameraTurnChange
{
	// B-404 (Q-404, study §9: Expedition 33 goes wide, then blends to the next hero within about 0.5 s).
	// TODO: tune in PIE (B-157). Times in s.
	constexpr float WideBeatBlendSeconds = 0.3f;
	constexpr float WideBeatHoldSeconds = 0.2f;
	constexpr float HandoverBlendSeconds = 0.5f;
}

FTurnChangePlan FCombatCameraRules::PlanTurnChange(bool bSomeoneActed, bool bSameActor, ECombatCameraMode NextMode, ETurnViewStyle Style,
												   const FCombatCameraFrame &Frame)
{
	using namespace CombatCameraTurnChange;
	FTurnChangePlan Plan;
	Plan.TurnView = ComputeView(NextMode, Style, Frame);
	Plan.bWideBeat = bSomeoneActed && !bSameActor && NextMode == ECombatCameraMode::Turn;
	if (Plan.bWideBeat)
	{
		Plan.WideView = ComputeView(ECombatCameraMode::NotYourTurn, Style, Frame);
		Plan.BeatBlendSeconds = WideBeatBlendSeconds;
		Plan.BeatSeconds = WideBeatBlendSeconds + WideBeatHoldSeconds;
		Plan.HandoverBlendSeconds = HandoverBlendSeconds;
	}
	return Plan;
}

namespace CombatCameraSameTurn
{
	// B-401 (Q-401, study R4): Expedition 33 settles straight back behind the same hero between that hero's actions;
	// shorter than the turn blend (0.6 s) and the handover (0.5 s). TODO: tune in PIE (B-157).
	constexpr float ReturnBlendSeconds = 0.35f;
}

FSameTurnReturnPlan FCombatCameraRules::PlanSameTurnReturn(bool bActedSinceAsked, ECombatCameraMode Mode, ETurnViewStyle Style,
														   const FCombatCameraFrame &Frame)
{
	FSameTurnReturnPlan Plan;
	Plan.TurnView = ComputeView(Mode, Style, Frame);
	Plan.bReturn = bActedSinceAsked && Mode == ECombatCameraMode::Turn;
	Plan.BlendSeconds = Plan.bReturn ? CombatCameraSameTurn::ReturnBlendSeconds : 0.0f;
	return Plan;
}
