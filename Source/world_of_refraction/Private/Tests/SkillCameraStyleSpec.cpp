// SkillCameraStyleSpec.cpp
// B-600 (Q-600, Crown 2026-11-24): each skill picks a camera style. The shot order per style: which moments play a
// shot, which cut, which hold still, the Ranged Release creep and punch, and that Standard is today's camera. Rule
// (Persona 5, Expedition 33; study R5, R6, R8): never change size along one axis; a size change comes with a cut.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "Combat/Camera/CameraFramingChecks.h"
#include "Skills/Definitions/AbilityData.h"

namespace
{
	const FVector STYLE_TEST_TARGET(800.0f, 0.0f, 0.0f);
	const FVector STYLE_TEST_SECOND_TARGET(800.0f, 400.0f, 0.0f);
	constexpr float STYLE_TEST_MAX_CREEP = 30.0f;
	constexpr float STYLE_TEST_PUNCH_RANGE = 1.0f;
	constexpr float STYLE_TEST_LOW_CAMERA_HEIGHT = 100.0f; // a hero angle sits below the fighters' chests
	const FVector STYLE_TEST_STRIKE_SPOT(650.0f, 0.0f, 0.0f);	   // B-600c: where a melee dash ends, in reach of the target
	constexpr float STYLE_TEST_MIN_LENS_DISTANCE = 250.0f;		   // B-600c: the attacker never fills the lens

	FCameraShotContext StyleTestContext()
	{
		FCameraShotContext Context;
		Context.Targets.Add(STYLE_TEST_TARGET);
		return Context;
	}

	bool IsStill(const FCameraShot &Shot) { return Shot.Start.Equals(Shot.End); }

	float Travel(const FCameraShot &Shot) { return static_cast<float>(FVector::Dist(Shot.Start.GetLocation(), Shot.End.GetLocation())); }
}

BEGIN_DEFINE_SPEC(FSkillCameraStyleSpec, "WoR.Combat.Camera.Styles", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FSkillCameraStyleSpec)

void FSkillCameraStyleSpec::Define()
{
	const FSkillCameraTuning Tuning;

	It("defaults a skill to Standard, today's camera, and keeps Standard for an enemy", [this]()
	{
		UAbilityData *Skill = NewObject<UAbilityData>(GetTransientPackage());
		TestEqual(TEXT("a new skill is Standard"), Skill->CameraStyle, ESkillCameraStyle::Standard);
		Skill->CameraStyle = ESkillCameraStyle::RangedRelease;
		TestEqual(TEXT("a party member's skill plays its style"), FCombatCameraRules::GetCameraStyle(Skill, false), ESkillCameraStyle::RangedRelease);
		TestEqual(TEXT("an enemy's plays Standard"), FCombatCameraRules::GetCameraStyle(Skill, true), ESkillCameraStyle::Standard);
		TestEqual(TEXT("no skill: Standard"), FCombatCameraRules::GetCameraStyle(nullptr, false), ESkillCameraStyle::Standard);
		TestTrue(TEXT("a styled skill plays shots"), FCombatCameraRules::UsesShotPatterns(Skill));
	});

	It("plays Standard exactly as ComputeShot (Cinematic too, with no sequence)", [this, Tuning]()
	{
		const FCameraShotContext Context = StyleTestContext();
		for (const ESkillCameraStyle Style : {ESkillCameraStyle::Standard, ESkillCameraStyle::Cinematic})
		{
			for (const ECameraShotMoment Moment : {ECameraShotMoment::WindUp, ECameraShotMoment::Cast, ECameraShotMoment::Impact})
			{
				const TOptional<FCameraShot> Styled = FCombatCameraRules::ComputeStyledShot(Style, Moment, Context, Tuning);
				const FCameraShot Plain = FCombatCameraRules::ComputeShot(Moment, Context, Tuning);
				TestTrue(FString::Printf(TEXT("%s %s is today's shot"), *UEnum::GetValueAsString(Style), *UEnum::GetValueAsString(Moment)),
						 Styled.IsSet() && Styled->End.Equals(Plain.End) && Styled->Start.Equals(Plain.Start) && !Styled->bCut &&
							 FMath::IsNearlyEqual(Styled->BlendSeconds, Plain.BlendSeconds));
			}
		}
	});

	It("Melee Rush: rushes in, no Cast shot, cuts to a held side two-shot on the hit", [this, Tuning]()
	{
		const FCameraShotContext Context = StyleTestContext();
		TestTrue(TEXT("precondition: camera movement on (the rush is allowed)"), Context.bAllowRush);
		const TOptional<FCameraShot> WindUp = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::MeleeRush, ECameraShotMoment::WindUp, Context, Tuning);
		TestTrue(TEXT("a rush (motion blur), one way in"), WindUp.IsSet() && WindUp->MotionBlur > 0.0f && Travel(*WindUp) > 0.0f);
		TestFalse(TEXT("no Cast pull-back"), FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::MeleeRush, ECameraShotMoment::Cast, Context, Tuning).IsSet());
		const TOptional<FCameraShot> Hit = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::MeleeRush, ECameraShotMoment::Impact, Context, Tuning);
		TestTrue(TEXT("the hit cuts and holds"), Hit.IsSet() && Hit->bCut && IsStill(*Hit));
		TestTrue(TEXT("both fighters in the two-shot"), Hit.IsSet() && CameraFraming::IsOnScreen(Hit->End, Hit->FieldOfView, CameraFraming::DEFAULT_ASPECT, Context.Caster) &&
															CameraFraming::IsOnScreen(Hit->End, Hit->FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_TARGET));
		TestTrue(TEXT("its punch passes (FOV back to the default)"), Hit.IsSet() && FMath::IsNearlyEqual(Hit->FieldOfView, FCombatCameraRules::DefaultFieldOfView));
	});

	It("B-600c Melee Rush: the rush stays behind the attacker; the dash's end cuts to the side view, the attacker clear of the lens", [this, Tuning]()
	{
		const FCameraShotContext Context = StyleTestContext();
		const TOptional<FCameraShot> WindUp = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::MeleeRush, ECameraShotMoment::WindUp, Context, Tuning);
		const FVector ToTarget = (STYLE_TEST_TARGET - Context.Caster).GetSafeNormal2D();
		TestTrue(TEXT("the rush ends behind the attacker's start (it never leads the dash)"),
				 WindUp.IsSet() && FVector::DotProduct(WindUp->End.GetLocation() - Context.Caster, ToTarget) < 0.0f);

		FCameraShotContext Arrived = Context;
		Arrived.Caster = STYLE_TEST_STRIKE_SPOT;
		const TOptional<FCameraShot> Arrival = FCombatCameraRules::ComputeArrivalShot(ESkillCameraStyle::MeleeRush, /*bCasterIsEnemy=*/false, Arrived, Tuning);
		TestTrue(TEXT("on arrival: a cut that holds, no punch or shake yet"), Arrival.IsSet() && Arrival->bCut && IsStill(*Arrival) &&
																					 FMath::IsNearlyZero(Arrival->FovPunch) && FMath::IsNearlyZero(Arrival->ShakeScale));
		TestTrue(FString::Printf(TEXT("the attacker at least %.0f cm from the lens"), STYLE_TEST_MIN_LENS_DISTANCE),
				 Arrival.IsSet() && FVector::Dist(Arrival->End.GetLocation(), STYLE_TEST_STRIKE_SPOT) >= STYLE_TEST_MIN_LENS_DISTANCE);
		TestTrue(TEXT("both fighters in frame"), Arrival.IsSet() && CameraFraming::IsOnScreen(Arrival->End, Arrival->FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_STRIKE_SPOT) &&
													 CameraFraming::IsOnScreen(Arrival->End, Arrival->FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_TARGET));
		const TOptional<FCameraShot> Hit = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::MeleeRush, ECameraShotMoment::Impact, Arrived, Tuning);
		TestTrue(TEXT("the hit's view is the arrival's (it punches there, no second cut)"), Hit.IsSet() && Arrival.IsSet() && Hit->End.Equals(Arrival->End));
		for (const ESkillCameraStyle Style : {ESkillCameraStyle::Standard, ESkillCameraStyle::RangedRelease, ESkillCameraStyle::HeavyWindUp, ESkillCameraStyle::AreaSweep,
											  ESkillCameraStyle::Cinematic})
		{
			TestFalse(FString::Printf(TEXT("%s: no arrival cut"), *UEnum::GetValueAsString(Style)), FCombatCameraRules::ComputeArrivalShot(Style, /*bCasterIsEnemy=*/false, Arrived, Tuning).IsSet());
		}
	});

	It("B-604: an enemy's melee rush holds behind its start and cuts to the side view on arrival; party Standard skills unchanged", [this, Tuning]()
	{
		// The enemy at its start (StyleTestContext's caster), rushing the party member at STYLE_TEST_TARGET.
		const FCameraShotContext Context = StyleTestContext();
		const ESkillCameraStyle EnemyStyle = FCombatCameraRules::GetCameraStyle(nullptr, /*bCasterIsEnemy=*/true);
		const TOptional<FCameraShot> Rush = FCombatCameraRules::ComputeStyledShot(EnemyStyle, ECameraShotMoment::WindUp, Context, Tuning);
		const FVector ToTarget = (STYLE_TEST_TARGET - Context.Caster).GetSafeNormal2D();
		TestTrue(TEXT("the rush ends behind the enemy's start: it runs away from the lens, never through it"),
				 Rush.IsSet() && FVector::DotProduct(Rush->End.GetLocation() - Context.Caster, ToTarget) < 0.0f);

		FCameraShotContext Arrived = Context;
		Arrived.Caster = STYLE_TEST_STRIKE_SPOT;
		const TOptional<FCameraShot> Arrival = FCombatCameraRules::ComputeArrivalShot(EnemyStyle, /*bCasterIsEnemy=*/true, Arrived, Tuning);
		TestTrue(TEXT("on arrival: a cut that holds, no punch or shake yet"), Arrival.IsSet() && Arrival->bCut && IsStill(*Arrival) &&
																				 FMath::IsNearlyZero(Arrival->FovPunch) && FMath::IsNearlyZero(Arrival->ShakeScale));
		TestTrue(FString::Printf(TEXT("the enemy at least %.0f cm from the lens (it never fills the frame)"), STYLE_TEST_MIN_LENS_DISTANCE),
				 Arrival.IsSet() && FVector::Dist(Arrival->End.GetLocation(), STYLE_TEST_STRIKE_SPOT) >= STYLE_TEST_MIN_LENS_DISTANCE);
		TestTrue(TEXT("the enemy and its target both in frame"),
				 Arrival.IsSet() && CameraFraming::IsOnScreen(Arrival->End, Arrival->FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_STRIKE_SPOT) &&
					 CameraFraming::IsOnScreen(Arrival->End, Arrival->FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_TARGET));
		TestFalse(TEXT("a party member's Standard skill keeps its timed rush (no arrival cut)"),
				  FCombatCameraRules::ComputeArrivalShot(ESkillCameraStyle::Standard, /*bCasterIsEnemy=*/false, Arrived, Tuning).IsSet());
		TestTrue(TEXT("a party member's Melee Rush still cuts (B-600c)"),
				 FCombatCameraRules::ComputeArrivalShot(ESkillCameraStyle::MeleeRush, /*bCasterIsEnemy=*/false, Arrived, Tuning).IsSet());
	});

	It("B-600d: a party member's leap home leaves its shot; an enemy's keeps its shot", [this]()
	{
		TestTrue(TEXT("a party member leaving its own shot: the camera follows it, never the target alone"),
				 FCombatCameraRules::FollowsLeapHome(/*bShotPlaying=*/true, /*bIsShotCaster=*/true, /*bEnemyCaster=*/false));
		TestFalse(TEXT("an enemy leaving: the shot holds on the party member it hit, as before"), FCombatCameraRules::FollowsLeapHome(true, true, true));
		TestFalse(TEXT("someone else's return: not this shot's"), FCombatCameraRules::FollowsLeapHome(true, false, false));
		TestFalse(TEXT("no shot playing (cinematics off, a sequence): the view is the turn view's or the sequence's already"),
				  FCombatCameraRules::FollowsLeapHome(false, true, false));
	});

	It("B-600e: the leap home goes to the wide beat's view from the home spot: the whole leap and both sides in frame", [this]()
	{
		const FVector Home = FVector::ZeroVector; // StyleTestContext's caster
		const FVector Ally(0.0f, -300.0f, 0.0f);
		FCombatCameraFrame HomeFrame;
		HomeFrame.Focus = Home;
		HomeFrame.TowardEnemies = (STYLE_TEST_TARGET - Home).GetSafeNormal2D();
		HomeFrame.EnemyCentre = STYLE_TEST_TARGET;
		HomeFrame.ArenaCentre = (Home + STYLE_TEST_TARGET) * 0.5f;
		for (const ETurnViewStyle Style : {ETurnViewStyle::CentredWide, ETurnViewStyle::OverTheShoulder, ETurnViewStyle::Tactical})
		{
			const FString Name = UEnum::GetValueAsString(Style);
			const FTurnChangePlan Leap = FCombatCameraRules::PlanLeapHome(Style, HomeFrame);
			const FTurnChangePlan Beat = FCombatCameraRules::PlanTurnChange(true, false, ECombatCameraMode::Turn, Style, HomeFrame);
			TestTrue(Name + TEXT(": the next beat's own view and blend"),
					 Leap.WideView.Equals(Beat.WideView) && FMath::IsNearlyEqual(Leap.BeatBlendSeconds, Beat.BeatBlendSeconds) && Leap.BeatBlendSeconds > 0.0f);
			const FTransform &View = Leap.WideView;
			TestTrue(Name + TEXT(": the lens behind the home spot (the leap comes toward it but never past it)"),
					 FVector::DotProduct(Home - View.GetLocation(), HomeFrame.TowardEnemies) > STYLE_TEST_MIN_LENS_DISTANCE);
			bool bLeapInFrame = true;
			constexpr float LEAP_SAMPLE_STEP = 0.25f; // strike spot, three points on the way, home
			for (float Alpha = 0.0f; Alpha <= 1.0f; Alpha += LEAP_SAMPLE_STEP)
			{
				const FVector Leaping = FMath::Lerp(STYLE_TEST_STRIKE_SPOT, Home, Alpha);
				bLeapInFrame &= CameraFraming::IsOnScreen(View, FCombatCameraRules::DefaultFieldOfView, CameraFraming::DEFAULT_ASPECT, Leaping) &&
								FVector::Dist(View.GetLocation(), Leaping) >= STYLE_TEST_MIN_LENS_DISTANCE;
			}
			TestTrue(Name + TEXT(": the attacker in frame from the strike spot to home"), bLeapInFrame);
			TestTrue(Name + TEXT(": both sides in frame (the enemy and the other party member), never the enemy alone"),
					 CameraFraming::IsOnScreen(View, FCombatCameraRules::DefaultFieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_TARGET) &&
						 CameraFraming::IsOnScreen(View, FCombatCameraRules::DefaultFieldOfView, CameraFraming::DEFAULT_ASPECT, Ally));
		}
	});

	It("Ranged Release: a held shoulder shot, a cut to a still two-shot at the release, a punch in place on the hit", [this, Tuning]()
	{
		const FCameraShotContext Context = StyleTestContext();
		const TOptional<FCameraShot> WindUp = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::RangedRelease, ECameraShotMoment::WindUp, Context, Tuning);
		TestTrue(TEXT("a cut to the shoulder"), WindUp.IsSet() && WindUp->bCut);
		TestTrue(FString::Printf(TEXT("it creeps no more than %.0f cm"), STYLE_TEST_MAX_CREEP), WindUp.IsSet() && Travel(*WindUp) <= STYLE_TEST_MAX_CREEP + KINDA_SMALL_NUMBER);
		TestTrue(TEXT("FOV 90"), WindUp.IsSet() && FMath::IsNearlyEqual(WindUp->FieldOfView, FCombatCameraRules::DefaultFieldOfView));
		TestTrue(TEXT("no rush blur"), WindUp.IsSet() && FMath::IsNearlyZero(WindUp->MotionBlur));

		FCameraShotContext Flying = Context;
		Flying.Projectile = FVector(400.0f, 0.0f, 100.0f);
		const TOptional<FCameraShot> Cast = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::RangedRelease, ECameraShotMoment::Cast, Flying, Tuning);
		TestTrue(TEXT("the release cuts to a dead-still shot"), Cast.IsSet() && Cast->bCut && IsStill(*Cast) && FMath::IsNearlyZero(Cast->ShakeScale));
		TestTrue(TEXT("the projectile's path is in it"), Cast.IsSet() && CameraFraming::IsOnScreen(Cast->End, Cast->FieldOfView, CameraFraming::DEFAULT_ASPECT, Flying.Projectile.GetValue()));

		const TOptional<FCameraShot> Hit = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::RangedRelease, ECameraShotMoment::Impact, Context, Tuning);
		TestTrue(TEXT("the hit stays in the same shot (no swing)"), Hit.IsSet() && Cast.IsSet() && Hit->End.Equals(Cast->End) && !Hit->bCut && IsStill(*Hit));
		TestTrue(TEXT("a small shake"), Hit.IsSet() && Hit->ShakeScale > 0.0f);
		if (Hit.IsSet())
		{
			const float Middle = FCombatCameraRules::GetPunchedFieldOfView(*Hit, Hit->FovPunchSeconds * 0.5f);
			const float After = FCombatCameraRules::GetPunchedFieldOfView(*Hit, Hit->FovPunchSeconds);
			TestTrue(FString::Printf(TEXT("FOV 90 -> %.1f -> %.1f"), Middle, After), Hit->FovPunch > 0.0f && Middle < FCombatCameraRules::DefaultFieldOfView - STYLE_TEST_PUNCH_RANGE &&
																						FMath::IsNearlyEqual(After, FCombatCameraRules::DefaultFieldOfView));
		}
	});

	It("Heavy Wind-Up: a slow one-way push, no Cast shot, a cut to a held low hero angle on the hit", [this, Tuning]()
	{
		const FCameraShotContext Context = StyleTestContext();
		const TOptional<FCameraShot> WindUp = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::HeavyWindUp, ECameraShotMoment::WindUp, Context, Tuning);
		const FCameraShot Plain = FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Context, Tuning);
		TestTrue(TEXT("precondition: camera movement on (so no rush is a real choice)"), Context.bAllowRush);
		TestTrue(TEXT("a push, no rush, slower than the plain wind-up"), WindUp.IsSet() && FMath::IsNearlyZero(WindUp->MotionBlur) && Travel(*WindUp) > 0.0f &&
																			 WindUp->BlendSeconds > Plain.BlendSeconds);
		TestFalse(TEXT("no Cast shot"), FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::HeavyWindUp, ECameraShotMoment::Cast, Context, Tuning).IsSet());
		const TOptional<FCameraShot> Hit = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::HeavyWindUp, ECameraShotMoment::Impact, Context, Tuning);
		TestTrue(TEXT("the hit cuts and holds"), Hit.IsSet() && Hit->bCut && IsStill(*Hit));
		TestTrue(TEXT("low, looking up"), Hit.IsSet() && Hit->End.GetLocation().Z - Context.Caster.Z < STYLE_TEST_LOW_CAMERA_HEIGHT && Hit->End.GetRotation().Rotator().Pitch > 0.0f);
		TestTrue(TEXT("both fighters in frame"), Hit.IsSet() && CameraFraming::IsOnScreen(Hit->End, Hit->FieldOfView, CameraFraming::DEFAULT_ASPECT, Context.Caster) &&
													 CameraFraming::IsOnScreen(Hit->End, Hit->FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_TARGET));
	});

	It("Area Sweep: one wide still shot of the attacker and every target, held through the hit", [this, Tuning]()
	{
		FCameraShotContext Context = StyleTestContext();
		Context.Targets.Add(STYLE_TEST_SECOND_TARGET);
		const TOptional<FCameraShot> WindUp = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::AreaSweep, ECameraShotMoment::WindUp, Context, Tuning);
		TestTrue(TEXT("a cut to a still shot"), WindUp.IsSet() && WindUp->bCut && IsStill(*WindUp) && FMath::IsNearlyZero(WindUp->ShakeScale));
		if (WindUp.IsSet())
		{
			TestTrue(TEXT("the attacker in frame"), CameraFraming::IsOnScreen(WindUp->End, WindUp->FieldOfView, CameraFraming::DEFAULT_ASPECT, Context.Caster));
			for (const FVector &Target : Context.Targets)
			{
				TestTrue(TEXT("every target in frame"), CameraFraming::IsOnScreen(WindUp->End, WindUp->FieldOfView, CameraFraming::DEFAULT_ASPECT, Target));
			}
		}
		for (const ECameraShotMoment Moment : {ECameraShotMoment::Cast, ECameraShotMoment::Impact, ECameraShotMoment::Spread})
		{
			TestFalse(TEXT("then it holds"), FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::AreaSweep, Moment, Context, Tuning).IsSet());
		}
	});

	It("drops a style's punch while the viewer defends", [this, Tuning]()
	{
		const TOptional<FCameraShot> Hit = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::RangedRelease, ECameraShotMoment::Impact, StyleTestContext(), Tuning);
		if (!TestTrue(TEXT("hit shot"), Hit.IsSet()))
		{
			return;
		}
		const FCameraShot Steady = FCombatCameraRules::SteadyForDefence(*Hit, true);
		TestTrue(TEXT("no punch, no shake"), FMath::IsNearlyZero(Steady.FovPunch) && FMath::IsNearlyZero(Steady.FovPunchSeconds) && FMath::IsNearlyZero(Steady.ShakeScale));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
