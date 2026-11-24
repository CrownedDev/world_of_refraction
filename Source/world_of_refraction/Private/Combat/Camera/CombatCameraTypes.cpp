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
	// B-600g (§6, E33): the over-the-shoulder camera a fixed distance from the acting character (B-398 scaled it by the
	// range): over the right shoulder at about chest height (above the actor's centre), so the character sits in the
	// left third, about 60% of the frame tall, at 5 to 14 m from the enemy centre. TODO: tune in PIE.
	constexpr float ShoulderBack = 250.0f;
	constexpr float ShoulderSide = 150.0f;
	constexpr float ShoulderHeight = 45.0f;
	// B-399 (R2): a sub-menu's step in, as parts of the turn camera's horizontal distance from the acting character
	// and of its height above it. TODO: tune in PIE.
	constexpr float MenuStepCloser = 0.25f;
	constexpr float MenuStepLower = 0.3f;
	constexpr float TacticalSide = 1400.0f;
	constexpr float TacticalHeight = 550.0f;
	// B-600g (§6): E33's hand-over wide (ComputeWideView), behind and a little above the viewer's side's centre.
	constexpr float HomeWideBack = 600.0f;
	constexpr float HomeWideHeight = 120.0f;
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
	// B-403 (R6): a small punch and a light shake on a normal hit; a crit stays bigger (FeelGuide 4). The softer punch
	// (5 / 10, was 8 / 14) is kept by Crown (Q-403, 2026-11-12). TODO: tune.
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

namespace CombatCameraStyles
{
	// B-600 (Q-600; Persona 5, Expedition 33, study R5/R6/R8). TODO: tune in PIE (Crown). Distances in cm, times in s.
	/** Every punch narrows and comes back this fast (90 -> 85 -> 90). */
	constexpr float HitFovPunchSeconds = 0.1f;
	// Ranged Release: the low over-the-shoulder hold, creeping in no more than ReleaseCreep.
	constexpr float ReleaseShoulderBack = 250.0f;
	constexpr float ReleaseShoulderSide = 70.0f;
	constexpr float ReleaseShoulderHeight = 90.0f;
	constexpr float ReleaseCreep = 30.0f;
	constexpr float ReleaseCreepSeconds = 1.5f;
	constexpr float ReleaseHitShake = 0.15f;
	// Heavy Wind-Up: the charge's push is slower than the plain wind-up's.
	constexpr float HeavyPushSeconds = 2.0f;
	// Heavy Wind-Up (B-600h, E33 section 7): the hit cuts to a medium shot of the target, from the caster's side of it
	// and off to the tuned side, at chest height, looking at the target's chest.
	constexpr float HeavyTargetBack = 350.0f;
	constexpr float HeavyTargetSide = 150.0f;
	constexpr float HeavyTargetHeight = 40.0f;
	constexpr float HeavyTargetLookHeight = 20.0f;
	// Area Sweep: wide and higher, back from everyone's centre past the attacker.
	constexpr float AreaBack = 700.0f;
	constexpr float AreaHeight = 450.0f;
	/** Extra distance back per cm the farthest fighter stands from the centre. */
	constexpr float AreaPullback = 1.6f;
}

namespace CombatCameraTargetView
{
	// B-400 (study R3 / §9, Q-400). TODO: tune in PIE.
	/** Room above a target's head for its HP bar, as a part of its height. */
	constexpr float HealthBarHeadroom = 0.25f;
	/** The shortest target the shot frames, in cm, so a missing or tiny bound still gives a sensible shot. */
	constexpr float MinTargetHeight = 100.0f;
	/** The part of the screen's height the target and its HP bar fill in Intel's medium shot; B-600f: the most they fill
	 *  in the over-the-shoulder target view (a nearer target steps the camera back). */
	constexpr float MediumShotFill = 0.6f;
	/** The closest the medium shot stands to the target, in cm. */
	constexpr float MediumShotMinDistance = 250.0f;
	/** The medium shot's step to the side and up, as parts of its distance: a three-quarter front view, slightly high. */
	constexpr float MediumShotSide = 0.35f;
	constexpr float MediumShotRise = 0.15f;
	/** Centred Wide / Tactical: how far the aim turns from the turn view's toward the target (0 none, 1 all the way). */
	constexpr float ReaimFraction = 0.35f;
	/** B-600f (Q-600f): Over the Shoulder turns all the way to face the target. */
	constexpr float ShoulderTurnFraction = 1.0f;
	/** The re-aim's blend, in every style (B-600f: Over the Shoulder blends too). */
	constexpr float ReaimBlendSeconds = 0.15f;
}

namespace CombatCameraIntelView
{
	// B-587 (D-262 c). TODO: tune in PIE.
	/** The close-up's distance as a part of the target view's front shot distance. */
	constexpr float CloserThanTargetView = 0.75f;
	/** The closest the close-up stands to the character's middle, in cm. */
	constexpr float MinDistance = 200.0f;
	/** Where the character sits across the screen (-1 left edge, 1 right edge): the middle of the left two-thirds (D-262 a). */
	constexpr float SubjectScreenX = -CameraFraming::THIRD_LINE;
	// B-587 QA: another fighter between the camera and the character tries the other three-quarter side, then each side
	// from higher up, and keeps the clearest. TODO: tune in PIE.
	/** The higher tries' extra rise, as a part of the shot's distance. */
	constexpr float HigherRise = 0.4f;
	/** How near the character's place across the screen (in -1..1 screen units) a fighter in front counts as on it. */
	constexpr float OnSubjectWidth = 0.3f;
	/** A fighter in front on the character weighs this many in front elsewhere in the free part of the screen. */
	constexpr int32 OnSubjectWeight = 3;

	/** How much the fighters at OtherBodies (their middles) stand in front of the character in View's free part. */
	int32 ScoreForeground(const FTransform &View, float SubjectDepth, const TArray<FVector> &OtherBodies)
	{
		int32 Score = 0;
		for (const FVector &Body : OtherBodies)
		{
			const float Depth = FVector::DotProduct(Body - View.GetLocation(), View.GetRotation().GetForwardVector());
			FVector2D Screen;
			if (Depth >= SubjectDepth || !CameraFraming::ProjectToScreen(View, FCombatCameraRules::DefaultFieldOfView, CameraFraming::DEFAULT_ASPECT, Body, Screen))
			{
				continue; // behind the character, or behind the camera
			}
			if (Screen.X < -1.0f || Screen.X >= CameraFraming::THIRD_LINE || FMath::Abs(Screen.Y) > 1.0f)
			{
				continue; // off screen, or under the panel
			}
			Score += FMath::Abs(Screen.X - SubjectScreenX) < OnSubjectWidth ? OnSubjectWeight : 1;
		}
		return Score;
	}
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
	{
		// B-600g (§6): the end of the enemy turn's slow push-in from the hand-over wide toward the acting enemy.
		const FVector Wide = ComputeWideView(Frame).GetLocation();
		return LookFrom(FMath::Lerp(Wide, Frame.Focus, EnemyPushInFraction), Frame.Focus);
	}
	case ECombatCameraMode::Static:
		return LookFrom(Frame.ArenaCentre - Forward * StaticBack + FVector::UpVector * StaticHeight, Frame.ArenaCentre);
	case ECombatCameraMode::Action:
	case ECombatCameraMode::None:
	default:
		return LookFrom(Frame.Focus - Forward * CentredWideBack + FVector::UpVector * CentredWideHeight, Frame.EnemyCentre);
	}
}

FTransform FCombatCameraRules::ComputeWideView(const FCombatCameraFrame &Frame)
{
	using namespace CombatCameraOffsets;
	const FVector Forward = Frame.TowardEnemies.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector ViewerSide = Frame.ArenaCentre * 2.0f - Frame.EnemyCentre; // BuildFrame: the arena centre is the sides' midpoint
	return LookFrom(ViewerSide - Forward * HomeWideBack + FVector::UpVector * HomeWideHeight, Frame.EnemyCentre);
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
		// Along this character's own line to the enemy centre (not the party's). Behind and right of it, aimed at the
		// enemy centre: it sits left, a little below the aim.
		const FVector Aim = (Frame.EnemyCentre - Frame.Focus).GetSafeNormal2D(UE_SMALL_NUMBER, Forward);
		const FVector AimRight = FVector::CrossProduct(FVector::UpVector, Aim);
		Location = Frame.Focus - Aim * ShoulderBack + AimRight * ShoulderSide + FVector::UpVector * ShoulderHeight;
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
	return CombatCameraTargetView::ReaimBlendSeconds; // B-600f: every style re-aims on the spot
}

namespace
{
	// B-400's front medium shot of a target: the camera on the side of the fight the target faces (an enemy faces the
	// party, an ally the enemies), far enough back that the framed height fills MediumShotFill of the screen's height.
	// B-600f: only Intel's close-up starts from it now.
	FTransform ComputeFrontMediumShot(const FCombatCameraFrame &Frame, const FVector &TargetFeet, float TargetHeight)
	{
		using namespace CombatCameraTargetView;
		const float FramedHeight = FMath::Max(TargetHeight, MinTargetHeight) * (1.0f + HealthBarHeadroom);
		const FVector AimPoint = TargetFeet + FVector::UpVector * FramedHeight * 0.5f;
		const FVector Forward = Frame.TowardEnemies.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
		const bool bOnEnemySide = FVector::DotProduct(TargetFeet - Frame.ArenaCentre, Forward) >= 0.0f;
		const FVector Facing = bOnEnemySide ? -Forward : Forward;
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Facing);
		const float VerticalHalfTan = FMath::Tan(FMath::DegreesToRadians(FCombatCameraRules::DefaultFieldOfView * 0.5f)) / CameraFraming::DEFAULT_ASPECT;
		const float Distance = FMath::Max(FramedHeight * 0.5f / (VerticalHalfTan * MediumShotFill), MediumShotMinDistance);
		const FVector Location = AimPoint + (Facing + Side * MediumShotSide + FVector::UpVector * MediumShotRise) * Distance;
		return LookFrom(Location, AimPoint);
	}
}

FTransform FCombatCameraRules::ComputeTargetView(ETurnViewStyle Style, const FCombatCameraFrame &Frame, const FVector &TargetFeet, float TargetHeight,
												 int32 MenuDepth, int32 TargetCount)
{
	return ComputeTargetViewFrom(Style, Frame, ComputeTurnView(Style, Frame, MenuDepth), TargetFeet, TargetHeight, TargetCount);
}

FTransform FCombatCameraRules::ComputeTargetViewFrom(ETurnViewStyle Style, const FCombatCameraFrame &Frame, const FTransform &TurnView,
													 const FVector &TargetFeet, float TargetHeight, int32 TargetCount)
{
	using namespace CombatCameraTargetView;
	if (TargetCount < MinTargetsForTargetView)
	{
		return TurnView; // one target: the camera holds (study R3)
	}
	// The body and the HP bar above it, aimed at their middle.
	const float FramedHeight = FMath::Max(TargetHeight, MinTargetHeight) * (1.0f + HealthBarHeadroom);
	const FVector AimPoint = TargetFeet + FVector::UpVector * FramedHeight * 0.5f;

	// Re-aim from where the turn view stands (B-600f: Over the Shoulder turns all the way to the target, as Expedition 33
	// and Persona 5 keep the shoulder view while you pick; the others part of the way, so the wide view stays wide).
	const float TurnFraction = Style == ETurnViewStyle::OverTheShoulder ? ShoulderTurnFraction : ReaimFraction;
	const FVector Location = TurnView.GetLocation();
	const FVector TurnAim = TurnView.GetRotation().Vector();
	const FVector ToTarget = (AimPoint - Location).GetSafeNormal(UE_SMALL_NUMBER, TurnAim);
	const FVector Aim = (TurnAim * (1.0f - TurnFraction) + ToTarget * TurnFraction).GetSafeNormal(UE_SMALL_NUMBER, TurnAim);
	if (Style != ETurnViewStyle::OverTheShoulder)
	{
		return FTransform(Aim.Rotation(), Location);
	}
	// A target too near to fit (a tall ally beside the character): step back along the aim until it fills MediumShotFill.
	const float VerticalHalfTan = FMath::Tan(FMath::DegreesToRadians(DefaultFieldOfView * 0.5f)) / CameraFraming::DEFAULT_ASPECT;
	const float FitDistance = FramedHeight * 0.5f / (VerticalHalfTan * MediumShotFill);
	const float StepBack = FMath::Max(0.0f, FitDistance - static_cast<float>(FVector::Dist(Location, AimPoint)));
	return FTransform(Aim.Rotation(), Location - Aim * StepBack);
}

FTransform FCombatCameraRules::ComputeIntelView(const FCombatCameraFrame &Frame, const FVector &SubjectFeet, float SubjectHeight,
												  const TArray<FVector> &OtherBodies)
{
	using namespace CombatCameraIntelView;
	const FTransform Front = ComputeFrontMediumShot(Frame, SubjectFeet, SubjectHeight);
	const float FramedHeight = FMath::Max(SubjectHeight, CombatCameraTargetView::MinTargetHeight) * (1.0f + CombatCameraTargetView::HealthBarHeadroom);
	const FVector AimPoint = SubjectFeet + FVector::UpVector * FramedHeight * 0.5f; // where the front shot aims
	const FVector FromAim = Front.GetLocation() - AimPoint;
	const float Distance = FMath::Max(FromAim.Size() * CloserThanTargetView, MinDistance);
	const float HorizontalHalfTan = FMath::Tan(FMath::DegreesToRadians(DefaultFieldOfView * 0.5f));

	// The front shot's direction first, then the other three-quarter side, then each from higher up.
	const FVector Forward = Frame.TowardEnemies.GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
	const FVector Mirrored = FromAim - Side * FVector::DotProduct(FromAim, Side) * 2.0f;
	const FVector Higher = FVector::UpVector * FromAim.Size() * HigherRise;
	FTransform Best;
	int32 BestScore = TNumericLimits<int32>::Max();
	for (const FVector &Offset : {FromAim, Mirrored, FromAim + Higher, Mirrored + Higher})
	{
		const FVector Back = Offset.GetSafeNormal(UE_SMALL_NUMBER, FromAim.GetSafeNormal());
		const FTransform Aimed = LookFrom(AimPoint + Back * Distance, AimPoint);
		// The same aim, slid to the camera's right so the character lands at SubjectScreenX.
		const FVector Slide = Aimed.GetRotation().GetRightVector() * (-SubjectScreenX * Distance * HorizontalHalfTan);
		const FTransform View(Aimed.GetRotation(), Aimed.GetLocation() + Slide);
		const int32 Score = ScoreForeground(View, Distance, OtherBodies);
		if (Score < BestScore)
		{
			Best = View;
			BestScore = Score;
		}
		if (Score == 0)
		{
			break; // the first clear one
		}
	}
	return Best;
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

namespace ScreenShakeScales
{
	/** B-453a: the factor each level plays a shot's ShakeScale at (Reduced is half, as The Last of Us Part II's
	 *  camera shake slider halfway). */
	constexpr float FULL = 1.0f;
	constexpr float REDUCED = 0.5f;
	constexpr float OFF = 0.0f;
}

FText FCombatCameraRules::GetScreenShakeSettingName()
{
	return LOCTEXT("ScreenShakeSetting", "Screen shake");
}

FText FCombatCameraRules::GetScreenShakeLevelName(EScreenShakeLevel Level)
{
	switch (Level)
	{
	case EScreenShakeLevel::Reduced:
		return LOCTEXT("ScreenShakeReduced", "Reduced");
	case EScreenShakeLevel::Off:
		return LOCTEXT("ScreenShakeOff", "Off");
	case EScreenShakeLevel::Full:
	default:
		return LOCTEXT("ScreenShakeFull", "Full");
	}
}

float FCombatCameraRules::GetScreenShakeScale(EScreenShakeLevel Level)
{
	switch (Level)
	{
	case EScreenShakeLevel::Reduced:
		return ScreenShakeScales::REDUCED;
	case EScreenShakeLevel::Off:
		return ScreenShakeScales::OFF;
	case EScreenShakeLevel::Full:
	default:
		return ScreenShakeScales::FULL;
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

FCameraShot FCombatCameraRules::ComputeDefendShot(const TArray<FVector> &Defenders, const FVector &Attacker)
{
	// B-604d (Q-604d, camera study §5c): side-on, never tight behind a defender; tuned in CombatCameraDefence.
	FCameraShot Shot;
	Shot.End = ComputeDefenceView(Defenders, Attacker);
	Shot.Start = Shot.End; // a cut that holds through the hit
	Shot.BlendSeconds = 0.0f;
	Shot.bCut = true;
	Shot.FieldOfView = DefaultFieldOfView;
	return Shot;
}

bool FCombatCameraRules::UsesDefendCamera(bool bCasterIsEnemy, ECameraShotMoment Opening, bool bOnlyPartyTargets)
{
	return bCasterIsEnemy && bOnlyPartyTargets && Opening != ECameraShotMoment::NamedAttack;
}

TOptional<float> FCombatCameraRules::GetDefendTurnWait(bool bDefendShowing, bool bHitLanded, float SecondsSinceHit)
{
	if (!bDefendShowing)
	{
		return 0.0f;
	}
	if (!bHitLanded)
	{
		return {};
	}
	return FMath::Max(0.0f, DefendHitHoldSeconds - SecondsSinceHit);
}

FCameraShot FCombatCameraRules::SteadyForDefence(const FCameraShot &Shot, bool bViewerDefending)
{
	FCameraShot Out = Shot;
	if (bViewerDefending)
	{
		Out.ShakeScale = 0.0f;
		Out.FieldOfView = DefaultFieldOfView;
		Out.MotionBlur = 0.0f; // B-403 review: no melee rush either (no blur, and the director keeps the camera where it is)
		Out.FovPunch = 0.0f;   // B-600
		Out.FovPunchSeconds = 0.0f;
	}
	return Out;
}

bool FCombatCameraRules::UsesShotPatterns(const USkillDataBase *Skill, bool bCasterIsEnemy)
{
	if (!Skill)
	{
		return false;
	}
	return Skill->CameraSequence.IsNull() || !PlaysStandardShots(GetCameraStyle(Skill, bCasterIsEnemy));
}

bool FCombatCameraRules::PlaysStandardShots(ESkillCameraStyle Style)
{
	return Style == ESkillCameraStyle::Standard || Style == ESkillCameraStyle::Cinematic;
}

bool FCombatCameraRules::FollowsLeapHome(bool bShotPlaying, bool bIsShotCaster, bool bEnemyCaster)
{
	return bShotPlaying && bIsShotCaster && !bEnemyCaster;
}

FTurnChangePlan FCombatCameraRules::PlanLeapHome(ETurnViewStyle Style, const FCombatCameraFrame &HomeFrame)
{
	return PlanTurnChange(/*bSomeoneActed=*/true, ECombatCameraMode::Turn, Style, HomeFrame);
}

float FCombatCameraRules::GetFollowHomeSeconds(const UAnimMontage *ReturnMontage, float PlayRate)
{
	return ReturnMontage ? ReturnMontage->GetPlayLength() / FMath::Max(MinPlayRate, PlayRate) : FollowHomeFallbackSeconds;
}

float FCombatCameraRules::GetActionEndHold(bool bPartyCaster, bool bPlayedShots)
{
	return bPartyCaster && bPlayedShots ? ActionEndHoldSeconds : 0.0f;
}

ESkillCameraStyle FCombatCameraRules::GetCameraStyle(const USkillDataBase *Skill, bool bCasterIsEnemy)
{
	return Skill && !bCasterIsEnemy ? Skill->CameraStyle : ESkillCameraStyle::Standard;
}

ESkillCameraStyle FCombatCameraRules::GetStyleForDash(ESkillCameraStyle Style, bool bDashes)
{
	return Style == ESkillCameraStyle::HeavyWindUp && bDashes ? ESkillCameraStyle::MeleeRush : Style;
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

namespace
{
	/** A shot that cuts to Base's end and holds there. */
	FCameraShot CutAndHold(FCameraShot Shot)
	{
		Shot.Start = Shot.End;
		Shot.BlendSeconds = 0.0f;
		Shot.bCut = true;
		return Shot;
	}

	/** Impact's static FOV narrowing as a punch that comes back (B-600: the shot holds, the punch passes). */
	FCameraShot WithPassingPunch(FCameraShot Shot)
	{
		Shot.FovPunch = FCombatCameraRules::DefaultFieldOfView - Shot.FieldOfView;
		Shot.FovPunchSeconds = Shot.FovPunch > 0.0f ? CombatCameraStyles::HitFovPunchSeconds : 0.0f;
		Shot.FieldOfView = FCombatCameraRules::DefaultFieldOfView;
		return Shot;
	}
}

TOptional<FCameraShot> FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle Style, ECameraShotMoment Moment, const FCameraShotContext &Context,
															 const FSkillCameraTuning &Tuning)
{
	using namespace CombatCameraStyles;
	const bool bHit = Moment == ECameraShotMoment::Impact || Moment == ECameraShotMoment::Spread;
	const FVector Caster = Context.Caster;
	FVector TargetCentre = Caster + FVector::ForwardVector;
	if (!Context.Targets.IsEmpty())
	{
		TargetCentre = FVector::ZeroVector;
		for (const FVector &Target : Context.Targets)
		{
			TargetCentre += Target;
		}
		TargetCentre /= Context.Targets.Num();
	}
	const FVector Forward = (TargetCentre - Caster).GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const float SideSign = Tuning.Side == ECameraShotSide::Left ? -1.0f : 1.0f;
	const float Scale = FMath::Max(Tuning.DistanceScale, UE_KINDA_SMALL_NUMBER);

	switch (Style)
	{
	case ESkillCameraStyle::MeleeRush:
	{
		if (Moment == ECameraShotMoment::WindUp)
		{
			FCameraShotContext Melee = Context;
			Melee.bRanged = false; // the rush (unless camera movement is off); it holds behind the attacker's start, and
								   // the director cuts to ComputeArrivalShot when the dash ends (B-600c)
			return ComputeShot(ECameraShotMoment::WindUp, Melee, Tuning);
		}
		if (bHit)
		{
			return CutAndHold(WithPassingPunch(ComputeShot(ECameraShotMoment::Impact, Context, Tuning)));
		}
		return {}; // no Cast pull-back
	}
	case ESkillCameraStyle::RangedRelease:
	{
		if (Moment == ECameraShotMoment::WindUp || Moment == ECameraShotMoment::NamedAttack)
		{
			FCameraShot Shot;
			Shot.FieldOfView = DefaultFieldOfView;
			const FVector Shoulder = Caster - Forward * ReleaseShoulderBack * Scale + Right * ReleaseShoulderSide * SideSign * Scale +
									 FVector::UpVector * ReleaseShoulderHeight * Scale;
			Shot.Start = LookFrom(Shoulder, TargetCentre);
			Shot.End = LookFrom(Shoulder + Forward * ReleaseCreep, TargetCentre);
			Shot.BlendSeconds = ReleaseCreepSeconds;
			Shot.bCut = true;
			return Shot;
		}
		// The release and the hit share one still side two-shot: the projectile crosses it, the hit punches in it.
		FCameraShotContext Still = Context;
		Still.Projectile.Reset();
		FCameraShot TwoShot = CutAndHold(ComputeShot(ECameraShotMoment::Impact, Still, Tuning));
		TwoShot.FieldOfView = DefaultFieldOfView;
		TwoShot.ShakeScale = 0.0f;
		if (bHit)
		{
			TwoShot.bCut = false; // already there: the punch and the shake only
			TwoShot.FovPunch = (Context.bCritical ? CombatCameraShots::CritFovPunch : CombatCameraShots::ImpactFovPunch) * Tuning.ShakeScale;
			TwoShot.FovPunchSeconds = HitFovPunchSeconds;
			TwoShot.ShakeScale = ReleaseHitShake * Tuning.ShakeScale;
		}
		return TwoShot;
	}
	case ESkillCameraStyle::HeavyWindUp:
	{
		if (Moment == ECameraShotMoment::WindUp || Moment == ECameraShotMoment::NamedAttack)
		{
			FCameraShotContext Charge = Context;
			Charge.bAllowRush = false; // the slow push, one way
			FCameraShot Push = ComputeShot(ECameraShotMoment::WindUp, Charge, Tuning);
			Push.BlendSeconds = HeavyPushSeconds;
			return Push;
		}
		if (bHit)
		{
			// B-600h (E33: a spell's hit cuts to the target): a medium shot of the target, from the caster's side.
			FCameraShot OnTarget = WithPassingPunch(ComputeShot(ECameraShotMoment::Impact, Context, Tuning)); // its punch and shake
			const FVector Target = Context.Targets.IsEmpty() ? TargetCentre : Context.Targets[0];
			const FVector Back = (Caster - Target).GetSafeNormal2D(UE_SMALL_NUMBER, -Forward);
			const FVector Across = FVector::CrossProduct(FVector::UpVector, Back);
			OnTarget.End = LookFrom(Target + Back * HeavyTargetBack * Scale - Across * HeavyTargetSide * SideSign * Scale + FVector::UpVector * HeavyTargetHeight,
									Target + FVector::UpVector * HeavyTargetLookHeight);
			return CutAndHold(OnTarget);
		}
		return {};
	}
	case ESkillCameraStyle::AreaSweep:
	{
		if (Moment != ECameraShotMoment::WindUp && Moment != ECameraShotMoment::NamedAttack)
		{
			return {}; // the wide shot holds to the end
		}
		FVector Centre = Caster;
		for (const FVector &Target : Context.Targets)
		{
			Centre += Target;
		}
		Centre /= Context.Targets.Num() + 1;
		float Spread = static_cast<float>(FVector::Dist2D(Caster, Centre));
		for (const FVector &Target : Context.Targets)
		{
			Spread = FMath::Max(Spread, static_cast<float>(FVector::Dist2D(Target, Centre)));
		}
		FCameraShot Wide;
		Wide.FieldOfView = DefaultFieldOfView;
		Wide.End = LookFrom(Centre - Forward * (AreaBack + Spread * AreaPullback) * Scale + FVector::UpVector * AreaHeight * Scale, Centre);
		return CutAndHold(Wide);
	}
	case ESkillCameraStyle::Cinematic:
	case ESkillCameraStyle::Standard:
	default:
		return ComputeShot(Moment, Context, Tuning);
	}
}

float FCombatCameraRules::GetPunchedFieldOfView(const FCameraShot &Shot, float Elapsed)
{
	if (Shot.FovPunchSeconds <= 0.0f || Elapsed < 0.0f || Elapsed >= Shot.FovPunchSeconds)
	{
		return Shot.FieldOfView;
	}
	return Shot.FieldOfView - Shot.FovPunch * FMath::Sin(UE_PI * Elapsed / Shot.FovPunchSeconds);
}

TOptional<FCameraShot> FCombatCameraRules::ComputeArrivalShot(ESkillCameraStyle Style, bool bCasterIsEnemy, const FCameraShotContext &Context,
															   const FSkillCameraTuning &Tuning)
{
	if (Style != ESkillCameraStyle::MeleeRush && !bCasterIsEnemy)
	{
		return {};
	}
	// B-600c: the side two-shot, cut to as the attacker arrives (before the swing), still: the hit punches in it later.
	FCameraShot Side = CutAndHold(ComputeShot(ECameraShotMoment::Impact, Context, Tuning));
	Side.FieldOfView = DefaultFieldOfView;
	Side.ShakeScale = 0.0f;
	return Side;
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

FCameraShot FCombatCameraRules::FollowApproachBurst(const FCameraShot &Rush, const FCameraShot &WindUpAtStrike, float WindowStart, float WindowEnd,
													 bool bCut)
{
	FCameraShot Out = Rush;
	Out.End = WindUpAtStrike.End;
	if (bCut)
	{
		Out.Start = Rush.End; // no blur: the director keeps the view it has until the cut
		Out.MotionBlur = 0.0f;
		Out.DelaySeconds = FMath::Max(WindowEnd, 0.0f);
		Out.BlendSeconds = 0.0f;
		return Out;
	}
	Out.DelaySeconds = FMath::Max(WindowStart, 0.0f);
	Out.BlendSeconds = FMath::Max(WindowEnd - WindowStart, 0.0f);
	return Out;
}

FTurnChangePlan FCombatCameraRules::PlanTurnChange(bool bSomeoneActed, ECombatCameraMode NextMode, ETurnViewStyle Style, const FCombatCameraFrame &Frame,
												   float SecondsOnWide)
{
	FTurnChangePlan Plan;
	Plan.TurnView = ComputeView(NextMode, Style, Frame);
	const bool bNextIsTurnView = NextMode == ECombatCameraMode::Turn || NextMode == ECombatCameraMode::NotYourTurn;
	Plan.bWideBeat = bSomeoneActed && bNextIsTurnView;
	if (Plan.bWideBeat)
	{
		// B-600g (§6): a hard cut to the hand-over wide, a hold, then the sweep (or an enemy's slow push-in).
		Plan.WideView = ComputeWideView(Frame);
		Plan.BeatBlendSeconds = 0.0f;
		Plan.BeatSeconds = FMath::Max(0.0f, WideMinSeconds - FMath::Max(0.0f, SecondsOnWide));
		Plan.HandoverBlendSeconds = NextMode == ECombatCameraMode::NotYourTurn ? EnemyPushInSeconds : SweepSeconds;
	}
	return Plan;
}

FSameTurnReturnPlan FCombatCameraRules::PlanSameTurnReturn(bool bActedSinceAsked, ECombatCameraMode Mode, ETurnViewStyle Style,
														   const FCombatCameraFrame &Frame)
{
	FSameTurnReturnPlan Plan;
	Plan.TurnView = ComputeView(Mode, Style, Frame);
	Plan.bReturn = bActedSinceAsked && Mode == ECombatCameraMode::Turn;
	// B-401; B-600g (§6): the director plays it as a turn change back to the same fighter (the wide, then the sweep).
	Plan.BlendSeconds = Plan.bReturn ? SweepSeconds : 0.0f;
	return Plan;
}
