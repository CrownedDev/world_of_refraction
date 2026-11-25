// SkillCameraStyleSpec.cpp
// B-600 (Q-600, Crown 2026-11-24): each skill picks a camera style. The shot order per style: which moments play a
// shot, which cut, which hold still, the Ranged Release creep and punch, and that Standard is today's camera. Rule
// (Persona 5, Expedition 33; study R5, R6, R8): never change size along one axis; a size change comes with a cut.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "Combat/Camera/CameraFramingChecks.h"
#include "Skills/Definitions/AbilityData.h"
#include "Combat/Actions/ApproachWarp.h"
#include "Animation/AnimMontage.h"

namespace
{
	const FVector STYLE_TEST_TARGET(800.0f, 0.0f, 0.0f);
	const FVector STYLE_TEST_SECOND_TARGET(800.0f, 400.0f, 0.0f);
	constexpr float STYLE_TEST_MAX_CREEP = 30.0f;
	constexpr float STYLE_TEST_PUNCH_RANGE = 1.0f;
	constexpr float STYLE_TEST_LOW_CAMERA_HEIGHT = 100.0f; // a low camera sits below the fighters' heads
	// B-600h: a medium shot of the target (a fighter 2 x 90 cm tall) fills about a third to a half of the frame.
	constexpr float STYLE_TEST_HALF_HEIGHT = 90.0f;
	constexpr float STYLE_TEST_MEDIUM_MIN_SHARE = 0.3f;
	constexpr float STYLE_TEST_MEDIUM_MAX_SHARE = 0.6f;
	// B-600i: Melee Rush's close three-quarter on the target: a bigger share, 30 to 60 degrees off the fighters' line.
	constexpr float STYLE_TEST_CLOSE_MIN_SHARE = 0.4f;
	constexpr float STYLE_TEST_CLOSE_MAX_SHARE = 0.75f;
	constexpr float STYLE_TEST_THREE_QUARTER_MIN_DEGREES = 30.0f;
	constexpr float STYLE_TEST_THREE_QUARTER_MAX_DEGREES = 60.0f;
	// B-600i: Area Sweep and Support's wide sit low to mid height (cm above the caster), under the old 450.
	constexpr float STYLE_TEST_MID_HEIGHT = 300.0f;
	// B-600i: the allies for Support, the enemy line ahead of them.
	const FVector STYLE_TEST_ALLY(0.0f, -300.0f, 0.0f);
	const FVector STYLE_TEST_SECOND_ALLY(0.0f, 300.0f, 0.0f);
	const FVector STYLE_TEST_ENEMY_CENTRE(1000.0f, 0.0f, 0.0f);
	// B-600h (B-595c QA): Heavy Attack, a ritual that dashes about 7 m on its fire turn.
	const TCHAR *const STYLE_TEST_HEAVY_ATTACK = TEXT("/Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_HeavyStrike.DA_Sword_Abilities_HeavyStrike");
	const FVector STYLE_TEST_DASH_TARGET(850.0f, 0.0f, 0.0f);
	// B-600i (B-595b): Over Under, a two-hit sword skill.
	const TCHAR *const STYLE_TEST_OVER_UNDER = TEXT("/Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_OverUnder.DA_Sword_Abilities_OverUnder");
	const FVector STYLE_TEST_STRIKE_SPOT(650.0f, 0.0f, 0.0f);	   // B-600c: where a melee dash ends, in reach of the target
	constexpr float STYLE_TEST_MIN_LENS_DISTANCE = 250.0f;		   // B-600c: the attacker never fills the lens
	// B-604b: the test trial enemy's real skills (BP_Enemy_Generic in the B-604 check).
	const TCHAR *const STYLE_TEST_WRATH_CUT = TEXT("/Game/Data/Pool/Generic/Weapons/Sword/Slash/Abilities/DA_Sword_Ability_WrathCut.DA_Sword_Ability_WrathCut");
	const TCHAR *const STYLE_TEST_LUNGE = TEXT("/Game/Data/Skills/Weapons/Sword/Abilities/DA_Sword_Abilities_Lunge.DA_Sword_Abilities_Lunge");
	constexpr float STYLE_TEST_TURN_BEFORE_HIT = 0.25f; // B-604b check: the next turn started up to 0.25 s before the hit
	// B-604d: the defend camera stands this far off the fighters' line at least; a fighter's height and its weapon's reach (cm).
	constexpr float STYLE_TEST_SIDE_ON_DEGREES = 45.0f;
	constexpr float STYLE_TEST_BODY_HEIGHT = 180.0f;
	constexpr float STYLE_TEST_WEAPON_REACH = 120.0f;

	FCameraShotContext StyleTestContext()
	{
		FCameraShotContext Context;
		Context.Targets.Add(STYLE_TEST_TARGET);
		return Context;
	}

	bool IsStill(const FCameraShot &Shot) { return Shot.Start.Equals(Shot.End); }

	/** B-600h/i: where a fighter at Target sits in Shot's end view, and how much of the frame's height it fills. */
	bool FrameTarget(const FCameraShot &Shot, const FVector &Target, FVector2D &OutCentre, float &OutShare)
	{
		const FVector Up = FVector::UpVector * STYLE_TEST_HALF_HEIGHT;
		FVector2D Head, Feet;
		const bool bProjected = CameraFraming::ProjectToScreen(Shot.End, Shot.FieldOfView, CameraFraming::DEFAULT_ASPECT, Target, OutCentre) &&
								CameraFraming::ProjectToScreen(Shot.End, Shot.FieldOfView, CameraFraming::DEFAULT_ASPECT, Target + Up, Head) &&
								CameraFraming::ProjectToScreen(Shot.End, Shot.FieldOfView, CameraFraming::DEFAULT_ASPECT, Target - Up, Feet);
		OutShare = static_cast<float>(Head.Y - Feet.Y) * 0.5f;
		return bProjected;
	}

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

	It("Melee Rush: rushes in, no Cast shot, cuts to a held close three-quarter on the target on the hit (B-600i, E33)", [this, Tuning]()
	{
		const FCameraShotContext Context = StyleTestContext();
		TestTrue(TEXT("precondition: camera movement on (the rush is allowed)"), Context.bAllowRush);
		const TOptional<FCameraShot> WindUp = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::MeleeRush, ECameraShotMoment::WindUp, Context, Tuning);
		TestTrue(TEXT("a rush (motion blur), one way in"), WindUp.IsSet() && WindUp->MotionBlur > 0.0f && Travel(*WindUp) > 0.0f);
		TestFalse(TEXT("no Cast pull-back"), FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::MeleeRush, ECameraShotMoment::Cast, Context, Tuning).IsSet());
		FCameraShotContext Arrived = Context; // the hit lands at the strike spot
		Arrived.Caster = STYLE_TEST_STRIKE_SPOT;
		const TOptional<FCameraShot> Hit = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::MeleeRush, ECameraShotMoment::Impact, Arrived, Tuning);
		TestTrue(TEXT("the hit cuts and holds"), Hit.IsSet() && Hit->bCut && IsStill(*Hit));
		if (!Hit.IsSet())
		{
			return;
		}
		FVector2D Centre;
		float Share = 0.0f;
		const bool bFramed = FrameTarget(*Hit, STYLE_TEST_TARGET, Centre, Share);
		const FString Where = FString::Printf(TEXT("target %s, %.2f of the frame"), *CameraFraming::GetThirdsCell(Centre), Share);
		TestTrue(FString::Printf(TEXT("the target in the middle band (%s)"), *Where),
				 bFramed && FMath::Abs(Centre.X) <= CameraFraming::THIRD_LINE && FMath::Abs(Centre.Y) <= CameraFraming::THIRD_LINE);
		TestTrue(FString::Printf(TEXT("close: %.2f to %.2f of the frame (%s)"), STYLE_TEST_CLOSE_MIN_SHARE, STYLE_TEST_CLOSE_MAX_SHARE, *Where),
				 bFramed && Share >= STYLE_TEST_CLOSE_MIN_SHARE && Share <= STYLE_TEST_CLOSE_MAX_SHARE);
		const FVector Off = (Hit->End.GetLocation() - STYLE_TEST_TARGET).GetSafeNormal2D();
		const float Degrees = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Off, (STYLE_TEST_STRIKE_SPOT - STYLE_TEST_TARGET).GetSafeNormal2D())));
		TestTrue(FString::Printf(TEXT("three-quarter: %.0f degrees off the fighters' line, on the attacker's side"), Degrees),
				 Degrees >= STYLE_TEST_THREE_QUARTER_MIN_DEGREES && Degrees <= STYLE_TEST_THREE_QUARTER_MAX_DEGREES);
		TestTrue(TEXT("the attacker still in frame"), CameraFraming::IsOnScreen(Hit->End, Hit->FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_STRIKE_SPOT));
		TestTrue(TEXT("its punch passes (FOV back to the default)"), Hit->FovPunch > 0.0f && FMath::IsNearlyEqual(Hit->FieldOfView, FCombatCameraRules::DefaultFieldOfView));
	});

	It("B-600i Melee Rush combo: each later hit re-cuts to the next of three angles; hits on one frame cut once", [this, Tuning]()
	{
		FCameraShotContext Arrived = StyleTestContext();
		Arrived.Caster = STYLE_TEST_STRIKE_SPOT;
		TArray<FTransform> Views;
		for (int32 Hit = 0; Hit < FCombatCameraRules::ComboRecutAngles; ++Hit)
		{
			const FCameraShot Shot = FCombatCameraRules::ComputeComboHitShot(Arrived, Tuning, Hit);
			Views.Add(Shot.End);
			const FString Name = FString::Printf(TEXT("hit %d"), Hit + 1);
			TestTrue(Name + TEXT(": a cut that holds, its punch passing"), Shot.bCut && IsStill(Shot) && Shot.FovPunch > 0.0f);
			TestTrue(Name + TEXT(": the target in frame"), CameraFraming::IsOnScreen(Shot.End, Shot.FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_TARGET));
			TestTrue(FString::Printf(TEXT("%s: the attacker at least %.0f cm from the lens"), *Name, STYLE_TEST_MIN_LENS_DISTANCE),
					 FVector::Dist(Shot.End.GetLocation(), STYLE_TEST_STRIKE_SPOT) >= STYLE_TEST_MIN_LENS_DISTANCE);
		}
		TestTrue(TEXT("three different angles"), Views.Num() == 3 && !Views[0].Equals(Views[1]) && !Views[1].Equals(Views[2]) && !Views[0].Equals(Views[2]));
		const FVector Line = (STYLE_TEST_TARGET - STYLE_TEST_STRIKE_SPOT).GetSafeNormal2D();
		const FVector Across = FVector::CrossProduct(FVector::UpVector, Line);
		TestTrue(TEXT("the second from the other side"),
				 FVector::DotProduct(Views[0].GetLocation() - STYLE_TEST_TARGET, Across) * FVector::DotProduct(Views[1].GetLocation() - STYLE_TEST_TARGET, Across) < 0.0f);
		TestTrue(TEXT("hit 1 is the Melee Rush hit shot"),
				 Views[0].Equals(FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::MeleeRush, ECameraShotMoment::Impact, Arrived, Tuning)->End));
		TestTrue(TEXT("the fourth hit comes round to the first angle"),
				 FCombatCameraRules::ComputeComboHitShot(Arrived, Tuning, FCombatCameraRules::ComboRecutAngles).End.Equals(Views[0]));

		TestTrue(TEXT("on by default (E33: almost every hit)"), Tuning.bRecutEachHit);
		TestTrue(TEXT("Melee Rush, a later frame: re-cuts"), FCombatCameraRules::RecutsOnLaterHit(ESkillCameraStyle::MeleeRush, Tuning, false));
		TestFalse(TEXT("the same frame (several targets at once): no"), FCombatCameraRules::RecutsOnLaterHit(ESkillCameraStyle::MeleeRush, Tuning, true));
		FSkillCameraTuning Off = Tuning;
		Off.bRecutEachHit = false;
		TestFalse(TEXT("switched off: no"), FCombatCameraRules::RecutsOnLaterHit(ESkillCameraStyle::MeleeRush, Off, false));
		for (const ESkillCameraStyle Style : {ESkillCameraStyle::Standard, ESkillCameraStyle::RangedRelease, ESkillCameraStyle::HeavyWindUp, ESkillCameraStyle::AreaSweep,
											  ESkillCameraStyle::Cinematic, ESkillCameraStyle::Support})
		{
			TestFalse(FString::Printf(TEXT("%s: never"), *UEnum::GetValueAsString(Style)), FCombatCameraRules::RecutsOnLaterHit(Style, Tuning, false));
		}
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

	It("B-604c: with the enemy's real Wrath Cut timing, the defend camera holds until the hit lands and is seen", [this]()
	{
		// B-595c: Heavy Attack left this case: it now has an Impact notify on its contact frame.
		for (const TCHAR *Path : {STYLE_TEST_WRATH_CUT})
		{
			const USkillDataBase *Skill = LoadObject<USkillDataBase>(nullptr, Path);
			if (!TestNotNull(FString::Printf(TEXT("%s loads (renamed? point this case at the enemy's skill)"), Path), Skill) ||
				!TestNotNull(FString::Printf(TEXT("%s has its swing"), Path), Skill->SkillMontage))
			{
				continue;
			}
			const FString Name = Skill->Name;
			// The real path (B-604b check): no Impact notify and no window, so no Defence hand-off, and the hit lands by the
			// B-032d fallback at the swing's end, after the turn system has started the next fighter's turn.
			const FCinematicWindowTiming Timing = FCombatCameraRules::ReadWindowTiming(Skill, Skill->BaseAnimSpeed, false);
			float WindowEnd = 0.0f;
			TestFalse(Name + TEXT(": no Defence window to hand off at (if this fails, its swing gained an Impact notify: re-check the timing)"),
					  FCombatCameraRules::ResolveWindowEnd(Timing, WindowEnd));
			const float HitAt = Skill->SkillMontage->GetPlayLength() / FMath::Max(Skill->BaseAnimSpeed, UE_KINDA_SMALL_NUMBER);
			const float TurnStartAt = HitAt - STYLE_TEST_TURN_BEFORE_HIT;

			const TOptional<float> AtTurnStart = FCombatCameraRules::GetDefendTurnWait(true, /*bHitLanded=*/false, 0.0f);
			TestFalse(Name + TEXT(": at the next turn's start the hit hasn't landed: the camera waits for it"), AtTurnStart.IsSet());
			TestTrue(Name + TEXT(": the wait limit covers the gap to the hit"),
					 TurnStartAt + FCombatCameraRules::DefendHitWaitLimitSeconds > HitAt);
			const TOptional<float> AtHit = FCombatCameraRules::GetDefendTurnWait(true, /*bHitLanded=*/true, 0.0f);
			const float BeatAt = HitAt + AtHit.Get(0.0f);
			TestTrue(FString::Printf(TEXT("%s: the beat at %.2f s, after the hit at %.2f s and its %.2f s hold"), *Name, BeatAt, HitAt,
									 FCombatCameraRules::DefendHitHoldSeconds),
					 BeatAt >= HitAt + FCombatCameraRules::DefendHitHoldSeconds - UE_KINDA_SMALL_NUMBER);
		}
		TestTrue(TEXT("a hit seen long enough before the turn start (the enemy's Quick Strike): no wait"),
				 FMath::IsNearlyZero(FCombatCameraRules::GetDefendTurnWait(true, true, FCombatCameraRules::DefendHitHoldSeconds).Get(-1.0f)));
		TestTrue(TEXT("no defend camera (a party member's attack): no wait"),
				 FMath::IsNearlyZero(FCombatCameraRules::GetDefendTurnWait(false, false, 0.0f).Get(-1.0f)));
	});

	It("B-604d (Q-604d): the enemy's real Wrath Cut and Lunge play the defend camera side-on: both whole bodies and the weapon in view, clear of the lens", [this]()
	{
		// The enemy at its start (StyleTestContext's caster), the one party member it attacks at STYLE_TEST_TARGET.
		const FVector Enemy = StyleTestContext().Caster;
		const FVector Defender = STYLE_TEST_TARGET;
		const FVector Axis = (Enemy - Defender).GetSafeNormal2D();
		for (const TCHAR *Path : {STYLE_TEST_WRATH_CUT, STYLE_TEST_LUNGE})
		{
			const USkillDataBase *Skill = LoadObject<USkillDataBase>(nullptr, Path);
			if (!TestNotNull(FString::Printf(TEXT("%s loads (renamed? point this case at the enemy's skill)"), Path), Skill))
			{
				continue;
			}
			// StartShotPattern's path for an enemy caster with party targets only.
			const ECameraShotMoment Opening = FCombatCameraRules::OpeningMomentFor(Skill->CameraTuning, /*bCasterIsEnemy=*/true);
			TestTrue(FString::Printf(TEXT("%s: the enemy's attack plays the defend camera"), *Skill->Name),
					 FCombatCameraRules::UsesDefendCamera(/*bCasterIsEnemy=*/true, Opening, /*bOnlyPartyTargets=*/true));

			const FCameraShot Shot = FCombatCameraRules::ComputeDefendShot({Defender}, Enemy);
			const FVector Lens = Shot.End.GetLocation();
			const FVector Strike = ApproachWarp::GetStrikePosition(Enemy, Defender, Skill->ExecutionRange); // where a dash would end
			const FString Name = Skill->Name;
			TestTrue(Name + TEXT(": a cut that holds (through the hit), no rush blur"), Shot.bCut && IsStill(Shot) && FMath::IsNearlyZero(Shot.MotionBlur));
			const FVector FromMiddle = (Lens - (Defender + Strike) * 0.5f).GetSafeNormal2D();
			TestTrue(FString::Printf(TEXT("%s: side-on, not behind either fighter (at least %.0f degrees off their line)"), *Name, STYLE_TEST_SIDE_ON_DEGREES),
					 FMath::Abs(FVector::DotProduct(FromMiddle, Axis)) < FMath::Cos(FMath::DegreesToRadians(STYLE_TEST_SIDE_ON_DEGREES)));
			TestTrue(FString::Printf(TEXT("%s: low (below %.0f cm over the defender)"), *Name, STYLE_TEST_LOW_CAMERA_HEIGHT), Lens.Z < Defender.Z + STYLE_TEST_LOW_CAMERA_HEIGHT);
			for (const FVector &EnemyAt : {Enemy, Strike})
			{
				const FVector Weapon = EnemyAt - Axis * STYLE_TEST_WEAPON_REACH + FVector::UpVector * STYLE_TEST_BODY_HEIGHT * 0.5f; // swung at the defender
				TestTrue(FString::Printf(TEXT("%s: the enemy at %s at least %.0f cm from the lens"), *Name, *EnemyAt.ToCompactString(), STYLE_TEST_MIN_LENS_DISTANCE),
						 FVector::Dist(Lens, EnemyAt) >= STYLE_TEST_MIN_LENS_DISTANCE);
				bool bWhole = CameraFraming::IsOnScreen(Shot.End, Shot.FieldOfView, CameraFraming::DEFAULT_ASPECT, Weapon);
				for (const FVector &Feet : {EnemyAt, Defender})
				{
					bWhole &= CameraFraming::IsOnScreen(Shot.End, Shot.FieldOfView, CameraFraming::DEFAULT_ASPECT, Feet) &&
							  CameraFraming::IsOnScreen(Shot.End, Shot.FieldOfView, CameraFraming::DEFAULT_ASPECT, Feet + FVector::UpVector * STYLE_TEST_BODY_HEIGHT);
				}
				TestTrue(FString::Printf(TEXT("%s: the enemy at %s, its weapon and the defender all whole in frame"), *Name, *EnemyAt.ToCompactString()), bWhole);
			}
		}
		TestFalse(TEXT("a party member's attack keeps the attack camera (its style)"),
				  FCombatCameraRules::UsesDefendCamera(false, ECameraShotMoment::WindUp, true));
		TestFalse(TEXT("an enemy's named attack keeps its front shot (B-406)"), FCombatCameraRules::UsesDefendCamera(true, ECameraShotMoment::NamedAttack, true));
		TestFalse(TEXT("an enemy at its own side: today's shots"), FCombatCameraRules::UsesDefendCamera(true, ECameraShotMoment::WindUp, false));
	});

	It("B-604d (Q-604d): an enemy's hit on the whole party goes wide: every party member and the enemy whole in frame", [this]()
	{
		const FVector Enemy = StyleTestContext().Caster;
		const TArray<FVector> Party = {STYLE_TEST_TARGET, STYLE_TEST_SECOND_TARGET, STYLE_TEST_TARGET - (STYLE_TEST_SECOND_TARGET - STYLE_TEST_TARGET)};
		const FCameraShot Wide = FCombatCameraRules::ComputeDefendShot(Party, Enemy);
		const FCameraShot Two = FCombatCameraRules::ComputeDefendShot({Party[0]}, Enemy);
		TestTrue(TEXT("a cut that holds"), Wide.bCut && IsStill(Wide));
		TestTrue(TEXT("the Defence hand-off's own wide view (one framing for the whole attack)"),
				 Wide.End.Equals(FCombatCameraRules::ComputeDefenceView(Party, Enemy)));
		bool bWhole = true;
		TArray<FVector> Fighters = Party;
		Fighters.Add(Enemy);
		for (const FVector &Feet : Fighters)
		{
			bWhole &= CameraFraming::IsOnScreen(Wide.End, Wide.FieldOfView, CameraFraming::DEFAULT_ASPECT, Feet) &&
					  CameraFraming::IsOnScreen(Wide.End, Wide.FieldOfView, CameraFraming::DEFAULT_ASPECT, Feet + FVector::UpVector * STYLE_TEST_BODY_HEIGHT);
		}
		TestTrue(TEXT("every party member and the enemy whole in frame"), bWhole);
		FVector WideCentre = FVector::ZeroVector;
		for (const FVector &Fighter : Fighters)
		{
			WideCentre += Fighter / Fighters.Num();
		}
		const FVector TwoCentre = (Party[0] + Enemy) * 0.5f;
		TestTrue(TEXT("further back from its fighters than the one-defender two-shot from its two"),
				 FVector::Dist2D(Wide.End.GetLocation(), WideCentre) > FVector::Dist2D(Two.End.GetLocation(), TwoCentre));
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
			const FTurnChangePlan Beat = FCombatCameraRules::PlanTurnChange(true, ECombatCameraMode::Turn, Style, HomeFrame);
			TestTrue(Name + TEXT(": the next beat's own view, a hard cut (B-600g, E33)"),
					 Leap.WideView.Equals(Beat.WideView) && FMath::IsNearlyZero(Leap.BeatBlendSeconds) && FMath::IsNearlyZero(Beat.BeatBlendSeconds));
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

	It("B-600f (Q-600f): the leap home is followed into the attacker's own turn view at home: low behind it, the whole leap in frame", [this]()
	{
		const FVector Home = FVector::ZeroVector; // StyleTestContext's caster
		FCombatCameraFrame HomeFrame;
		HomeFrame.Focus = Home;
		HomeFrame.TowardEnemies = (STYLE_TEST_TARGET - Home).GetSafeNormal2D();
		HomeFrame.EnemyCentre = STYLE_TEST_TARGET;
		HomeFrame.ArenaCentre = (Home + STYLE_TEST_TARGET) * 0.5f;
		for (const ETurnViewStyle Style : {ETurnViewStyle::CentredWide, ETurnViewStyle::OverTheShoulder, ETurnViewStyle::Tactical})
		{
			const FString Name = UEnum::GetValueAsString(Style);
			const FTransform View = FCombatCameraRules::ComputeTurnView(Style, HomeFrame); // what the director follows into
			const float Behind = FVector::DotProduct(Home - View.GetLocation(), HomeFrame.TowardEnemies);
			if (Style == ETurnViewStyle::Tactical)
			{
				TestTrue(Name + TEXT(": side-on (the leap crosses the frame, never toward the lens)"),
						 FMath::Abs(Behind) < FVector::Dist2D(Home, View.GetLocation()) * 0.5f);
			}
			else
			{
				TestTrue(Name + TEXT(": the lens behind the home spot (the leap comes toward it but stops short)"), Behind > 0.0f);
			}
			bool bLeapInFrame = true;
			constexpr float LEAP_SAMPLE_STEP = 0.25f; // strike spot, three points on the way, home
			for (float Alpha = 0.0f; Alpha <= 1.0f; Alpha += LEAP_SAMPLE_STEP)
			{
				const FVector Leaping = FMath::Lerp(STYLE_TEST_STRIKE_SPOT, Home, Alpha);
				bLeapInFrame &= CameraFraming::IsOnScreen(View, FCombatCameraRules::DefaultFieldOfView, CameraFraming::DEFAULT_ASPECT, Leaping) &&
								FVector::Dist(View.GetLocation(), Leaping) >= STYLE_TEST_MIN_LENS_DISTANCE;
			}
			TestTrue(Name + TEXT(": the attacker in frame from the strike spot to home, never at the lens"), bLeapInFrame);
		}
	});

	It("B-600f: the follow lasts the real return leg (its length at its rate); a fallback with no montage", [this]()
	{
		for (const TCHAR *Path : {STYLE_TEST_WRATH_CUT, STYLE_TEST_LUNGE, STYLE_TEST_HEAVY_ATTACK})
		{
			const USkillDataBase *Skill = LoadObject<USkillDataBase>(nullptr, Path);
			const UAnimMontage *Montage = Skill ? (Skill->ReturnMontage ? Skill->ReturnMontage : Skill->SkillMontage) : nullptr;
			if (!TestNotNull(FString::Printf(TEXT("%s has a montage"), Path), Montage))
			{
				continue;
			}
			const float Length = Montage->GetPlayLength();
			TestTrue(FString::Printf(TEXT("%s: its length at rate 1"), Path),
					 Length > 0.0f && FMath::IsNearlyEqual(FCombatCameraRules::GetFollowHomeSeconds(Montage, 1.0f), Length));
			TestTrue(FString::Printf(TEXT("%s: half at rate 2"), Path), FMath::IsNearlyEqual(FCombatCameraRules::GetFollowHomeSeconds(Montage, 2.0f), Length * 0.5f));
		}
		TestEqual(TEXT("no return montage playing: the fallback"), FCombatCameraRules::GetFollowHomeSeconds(nullptr, 1.0f),
				  FCombatCameraRules::FollowHomeFallbackSeconds);
	});

	It("B-600f: the next fighter's view waits ActionEndHoldSeconds after a party member's skill with shots, else not at all", [this]()
	{
		TestTrue(TEXT("a party member's skill with shots: the hold"),
				 FCombatCameraRules::GetActionEndHold(/*bPartyCaster=*/true, /*bPlayedShots=*/true) == FCombatCameraRules::ActionEndHoldSeconds &&
					 FCombatCameraRules::ActionEndHoldSeconds > 0.0f);
		TestEqual(TEXT("an enemy's skill: the defend camera's own wait (B-604c), not this"), FCombatCameraRules::GetActionEndHold(false, true), 0.0f);
		TestEqual(TEXT("no shots (an item, End Turn, cinematics or movement off): none"), FCombatCameraRules::GetActionEndHold(true, false), 0.0f);
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

	It("Heavy Wind-Up: a slow one-way push, no Cast shot, a cut to a held medium shot of the target on the hit (B-600h, E33)", [this, Tuning]()
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
		if (!Hit.IsSet())
		{
			return;
		}
		// B-600h (E33 section 7: a spell's hit cuts to a medium shot of the target; it replaces B-600's low hero angle).
		const FVector Up = FVector::UpVector * STYLE_TEST_HALF_HEIGHT;
		FVector2D Centre, Head, Feet;
		const bool bProjected = CameraFraming::ProjectToScreen(Hit->End, Hit->FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_TARGET, Centre) &&
								CameraFraming::ProjectToScreen(Hit->End, Hit->FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_TARGET + Up, Head) &&
								CameraFraming::ProjectToScreen(Hit->End, Hit->FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_TARGET - Up, Feet);
		const float Share = static_cast<float>(Head.Y - Feet.Y) * 0.5f;
		const FString Where = FString::Printf(TEXT("target %s, %.2f of the frame"), *CameraFraming::GetThirdsCell(Centre), Share);
		TestTrue(FString::Printf(TEXT("the target in the middle band (%s)"), *Where),
				 bProjected && FMath::Abs(Centre.X) <= CameraFraming::THIRD_LINE && FMath::Abs(Centre.Y) <= CameraFraming::THIRD_LINE);
		TestTrue(FString::Printf(TEXT("a medium shot: the whole target, %.2f to %.2f of the frame (%s)"), STYLE_TEST_MEDIUM_MIN_SHARE, STYLE_TEST_MEDIUM_MAX_SHARE, *Where),
				 bProjected && Share >= STYLE_TEST_MEDIUM_MIN_SHARE && Share <= STYLE_TEST_MEDIUM_MAX_SHARE);
		const FVector ToCaster = (Context.Caster - STYLE_TEST_TARGET).GetSafeNormal2D();
		TestTrue(TEXT("from the caster's side of the target"), FVector::DotProduct(Hit->End.GetLocation() - STYLE_TEST_TARGET, ToCaster) > 0.0f);
		TestTrue(TEXT("about chest height"), FMath::Abs(Hit->End.GetLocation().Z - STYLE_TEST_TARGET.Z) < STYLE_TEST_LOW_CAMERA_HEIGHT);
		TestTrue(TEXT("its punch passes (FOV back to the default)"), Hit->FovPunch > 0.0f && FMath::IsNearlyEqual(Hit->FieldOfView, FCombatCameraRules::DefaultFieldOfView));
	});

	It("B-600h: a Heavy Wind-Up blow that dashes in plays as Melee Rush; nothing else changes", [this]()
	{
		TestEqual(TEXT("Heavy Wind-Up that dashes: Melee Rush (the rush, the cut as the dash ends)"),
				  FCombatCameraRules::GetStyleForDash(ESkillCameraStyle::HeavyWindUp, true), ESkillCameraStyle::MeleeRush);
		TestEqual(TEXT("Heavy Wind-Up in place: its own push and target shot"), FCombatCameraRules::GetStyleForDash(ESkillCameraStyle::HeavyWindUp, false),
				  ESkillCameraStyle::HeavyWindUp);
		for (const ESkillCameraStyle Style : {ESkillCameraStyle::Standard, ESkillCameraStyle::MeleeRush, ESkillCameraStyle::RangedRelease, ESkillCameraStyle::AreaSweep,
											  ESkillCameraStyle::Cinematic})
		{
			for (const bool bDashes : {false, true})
			{
				TestEqual(FString::Printf(TEXT("%s, dash %d: unchanged"), *UEnum::GetValueAsString(Style), bDashes), FCombatCameraRules::GetStyleForDash(Style, bDashes), Style);
			}
		}
	});

	It("B-600h: the real Heavy Attack is a Heavy Wind-Up ritual whose fire-turn montage dashes, so its fire turn plays as Melee Rush", [this]()
	{
		const USkillDataBase *Skill = LoadObject<USkillDataBase>(nullptr, STYLE_TEST_HEAVY_ATTACK);
		if (!TestNotNull(TEXT("Heavy Attack loads (renamed? point this case at it)"), Skill) || !TestNotNull(TEXT("its fire-turn montage"), Skill->SkillMontage))
		{
			return;
		}
		TestEqual(TEXT("Heavy Wind-Up style"), Skill->CameraStyle, ESkillCameraStyle::HeavyWindUp);
		TestNotNull(TEXT("a ritual: the wind-up turn plays its own montage first"), Skill->RitualCastMontage);
		const FVector Strike = ApproachWarp::GetStrikePosition(FVector::ZeroVector, STYLE_TEST_DASH_TARGET, Skill->ExecutionRange);
		const FApproachBurst Burst = ApproachWarp::Measure(Skill->SkillMontage, static_cast<float>(FVector::Dist2D(FVector::ZeroVector, Strike)), 1.0f,
														   ApproachWarp::GetApproachBurstSeconds());
		TestTrue(FString::Printf(TEXT("its fire-turn montage dashes %.0f cm (%s)"), FVector::Dist2D(FVector::ZeroVector, Strike), *Burst.ToString()), Burst.bHasWindow);
		TestEqual(TEXT("so its fire turn plays as Melee Rush"), FCombatCameraRules::GetStyleForDash(Skill->CameraStyle, Burst.bHasWindow), ESkillCameraStyle::MeleeRush);
	});

	It("B-600i: the real Over Under (two hits) is a Melee Rush with the re-cut on, so its second cut gets a new angle", [this]()
	{
		const USkillDataBase *Skill = LoadObject<USkillDataBase>(nullptr, STYLE_TEST_OVER_UNDER);
		if (!TestNotNull(TEXT("Over Under loads (renamed? point this case at it)"), Skill))
		{
			return;
		}
		TestEqual(TEXT("Melee Rush"), Skill->CameraStyle, ESkillCameraStyle::MeleeRush);
		TestTrue(TEXT("its re-cut on (the default)"), Skill->CameraTuning.bRecutEachHit);
	});

	It("B-600i Ranged Release, hold over the shoulder: no release cut, the hit punches in the shoulder shot; off by default", [this, Tuning]()
	{
		TestFalse(TEXT("off by default: the release two-shot as before"), Tuning.bHoldShoulderThroughHit);
		FSkillCameraTuning Hold = Tuning;
		Hold.bHoldShoulderThroughHit = true;
		const FCameraShotContext Context = StyleTestContext();
		const TOptional<FCameraShot> WindUp = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::RangedRelease, ECameraShotMoment::WindUp, Context, Hold);
		TestFalse(TEXT("no release shot: the shoulder shot holds as the projectile flies away"),
				  FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::RangedRelease, ECameraShotMoment::Cast, Context, Hold).IsSet());
		const TOptional<FCameraShot> Hit = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::RangedRelease, ECameraShotMoment::Impact, Context, Hold);
		TestTrue(TEXT("the hit: the shoulder shot's own view, no cut, a punch and a shake"),
				 WindUp.IsSet() && Hit.IsSet() && Hit->End.Equals(WindUp->End) && IsStill(*Hit) && !Hit->bCut && Hit->FovPunch > 0.0f && Hit->ShakeScale > 0.0f);
		TestTrue(TEXT("the target in frame from behind the thrower"),
				 Hit.IsSet() && CameraFraming::IsOnScreen(Hit->End, Hit->FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_TARGET) &&
					 FVector::DotProduct(Hit->End.GetLocation() - Context.Caster, (STYLE_TEST_TARGET - Context.Caster).GetSafeNormal2D()) < 0.0f);
	});

	It("B-600i Support: a front shot of the caster at home, then a wide of the allies with no enemy in frame", [this, Tuning]()
	{
		FCameraShotContext Context;
		Context.Targets = {STYLE_TEST_ALLY, STYLE_TEST_SECOND_ALLY};
		Context.EnemyCentre = STYLE_TEST_ENEMY_CENTRE;
		const TOptional<FCameraShot> Front = FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::Support, ECameraShotMoment::WindUp, Context, Tuning);
		if (!TestTrue(TEXT("a front shot"), Front.IsSet()))
		{
			return;
		}
		const FVector TowardEnemies = (STYLE_TEST_ENEMY_CENTRE - Context.Caster).GetSafeNormal2D();
		TestTrue(TEXT("a cut, then a slight one-way orbit over SupportCasterSeconds"),
				 Front->bCut && Travel(*Front) > 0.0f && FMath::IsNearlyEqual(Front->BlendSeconds, FCombatCameraRules::SupportCasterSeconds));
		TestTrue(TEXT("in front of the caster (toward the enemies), looking back at it"),
				 FVector::DotProduct(Front->End.GetLocation() - Context.Caster, TowardEnemies) > 0.0f &&
					 CameraFraming::IsOnScreen(Front->End, Front->FieldOfView, CameraFraming::DEFAULT_ASPECT, Context.Caster));
		TestFalse(TEXT("no enemy in the front shot"), CameraFraming::IsOnScreen(Front->End, Front->FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_ENEMY_CENTRE));
		for (const ECameraShotMoment Moment : {ECameraShotMoment::Cast, ECameraShotMoment::Impact, ECameraShotMoment::Spread})
		{
			TestFalse(TEXT("no other style shot: the front shot holds"), FCombatCameraRules::ComputeStyledShot(ESkillCameraStyle::Support, Moment, Context, Tuning).IsSet());
		}

		const FCameraShot Wide = FCombatCameraRules::ComputeSupportPartyShot(Context, Tuning);
		TestTrue(TEXT("the allies' wide: a cut that holds, no punch"), Wide.bCut && IsStill(Wide) && FMath::IsNearlyZero(Wide.FovPunch) && FMath::IsNearlyZero(Wide.ShakeScale));
		for (const FVector &Member : {Context.Caster, STYLE_TEST_ALLY, STYLE_TEST_SECOND_ALLY})
		{
			TestTrue(TEXT("every ally and the caster in frame"), CameraFraming::IsOnScreen(Wide.End, Wide.FieldOfView, CameraFraming::DEFAULT_ASPECT, Member));
		}
		TestFalse(TEXT("no enemy in frame"), CameraFraming::IsOnScreen(Wide.End, Wide.FieldOfView, CameraFraming::DEFAULT_ASPECT, STYLE_TEST_ENEMY_CENTRE));
		TestTrue(TEXT("low to mid height"), Wide.End.GetLocation().Z - Context.Caster.Z < STYLE_TEST_MID_HEIGHT);

		FCameraShotContext Close = Context;
		Close.EnemyCentre = FVector(STYLE_TEST_ENEMY_CENTRE.X * 0.5f, 0.0f, 0.0f); // the enemies right in front
		const FCameraShot CloseWide = FCombatCameraRules::ComputeSupportPartyShot(Close, Tuning);
		TestFalse(TEXT("enemies close by: still none in frame (never past them)"),
				  CameraFraming::IsOnScreen(CloseWide.End, CloseWide.FieldOfView, CameraFraming::DEFAULT_ASPECT, Close.EnemyCentre.GetValue()));
		TestTrue(TEXT("Support plays its shots, not a sequence"), !FCombatCameraRules::PlaysStandardShots(ESkillCameraStyle::Support));
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
			// B-600i (E33 area spells): low to mid height, behind and beside the attacker, the attacker near-left.
			FVector2D Attacker;
			TestTrue(TEXT("low to mid height"), WindUp->End.GetLocation().Z - Context.Caster.Z < STYLE_TEST_MID_HEIGHT);
			TestTrue(TEXT("behind the attacker"),
					 FVector::DotProduct(WindUp->End.GetLocation() - Context.Caster, (STYLE_TEST_TARGET - Context.Caster).GetSafeNormal2D()) < 0.0f);
			TestTrue(TEXT("the attacker on the left"),
					 CameraFraming::ProjectToScreen(WindUp->End, WindUp->FieldOfView, CameraFraming::DEFAULT_ASPECT, Context.Caster, Attacker) && Attacker.X < 0.0f);
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
