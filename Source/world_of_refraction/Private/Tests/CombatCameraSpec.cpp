// CombatCameraSpec.cpp
// Combat camera rules (B-156, Design/CombatCamera.md): the mode the viewer sees, the template a
// skill falls back to, and where each mode puts the camera. B-370: the cinematic window and the Defence view.
// B-398: the turn-view setting's layouts and its save.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Camera/CombatCameraDirector.h"
#include "Combat/Camera/CombatCameraComponent.h"
#include "Combat/Camera/CameraFramingChecks.h"
#include "Save/SaveSubsystem.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Actions/ActionStructs.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"

namespace
{
	/** Viewer's character at the origin, enemies 1000 cm ahead on +X. */
	FCombatCameraFrame CombatCameraTestFrame()
	{
		FCombatCameraFrame Frame;
		Frame.Focus = FVector::ZeroVector;
		Frame.TowardEnemies = FVector::ForwardVector;
		Frame.EnemyCentre = FVector(1000.0f, 0.0f, 0.0f);
		Frame.ArenaCentre = FVector(500.0f, 0.0f, 0.0f);
		return Frame;
	}

	// B-398: turn-view layouts. One enemy ahead; three in a shallow arc; a second party member beside the first; a
	// closer enemy. Screen positions run -1..1 (CameraFraming::ProjectToScreen).
	const FVector TURN_TEST_ONE_ENEMY(1000.0f, 0.0f, 0.0f);
	const FVector TURN_TEST_SIDE_MEMBER(0.0f, -250.0f, 0.0f);
	const FVector TURN_TEST_NEAR_ENEMY(600.0f, 0.0f, 0.0f);
	constexpr float TURN_TEST_LAYOUT_TOLERANCE = 0.01f;
	/** B-156's centred-wide turn view for CombatCameraTestFrame: 650 back, 320 up. B-398 must not move it. */
	const FVector TURN_TEST_CENTRED_WIDE_LOCATION(-650.0f, 0.0f, 320.0f);
	constexpr float TURN_TEST_LOCATION_TOLERANCE = 0.1f;
	/** B-404: world ticks well past a wide beat (0.5 s) at the test wrapper's 0.01 s step. */
	constexpr int32 BEAT_TEST_TICKS_PAST_BEAT = 200;
	/** A byte past the last ETurnViewStyle (a save from a later build). */
	constexpr uint8 TURN_TEST_UNKNOWN_STYLE = 7;

	TArray<FVector> TurnTestThreeEnemies()
	{
		return {FVector(900.0f, -300.0f, 0.0f), FVector(1100.0f, 0.0f, 0.0f), FVector(900.0f, 300.0f, 0.0f)};
	}

	/** The acting character at Focus, the enemy centre over Enemies; the party faces +X, as BuildFrame frames it. */
	FCombatCameraFrame TurnViewTestFrame(const FVector &Focus, const TArray<FVector> &Enemies)
	{
		FCombatCameraFrame Frame;
		Frame.Focus = Focus;
		Frame.TowardEnemies = FVector::ForwardVector;
		for (const FVector &Enemy : Enemies)
		{
			Frame.EnemyCentre += Enemy;
		}
		Frame.EnemyCentre /= FMath::Max(1, Enemies.Num());
		Frame.ArenaCentre = (Focus + Frame.EnemyCentre) * 0.5f;
		return Frame;
	}

	FVector2D ProjectInTurnView(const FTransform &View, const FVector &Point)
	{
		FVector2D Screen(TNumericLimits<float>::Max(), TNumericLimits<float>::Max()); // stays off screen if behind
		CameraFraming::ProjectToScreen(View, FCombatCameraRules::DefaultFieldOfView, CameraFraming::DEFAULT_ASPECT, Point, Screen);
		return Screen;
	}
}

BEGIN_DEFINE_SPEC(FCombatCameraSpec, "WoR.Combat.Camera",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCombatCameraSpec)

void FCombatCameraSpec::Define()
{
	Describe("SelectMode", [this]()
	{
		It("shows the turn view on your own turn and pans out on others", [this]()
		{
			TestEqual(TEXT("own turn"), FCombatCameraRules::SelectMode(true, false, false), ECombatCameraMode::Turn);
			TestEqual(TEXT("other turn"), FCombatCameraRules::SelectMode(false, false, false), ECombatCameraMode::NotYourTurn);
		});

		It("lets a playing sequence own the view, whoever acts", [this]()
		{
			TestEqual(TEXT("own action"), FCombatCameraRules::SelectMode(true, true, false), ECombatCameraMode::Action);
			TestEqual(TEXT("enemy action"), FCombatCameraRules::SelectMode(false, true, false), ECombatCameraMode::Action);
		});

		It("stays static whenever camera movement is off", [this]()
		{
			TestEqual(TEXT("own turn"), FCombatCameraRules::SelectMode(true, false, true), ECombatCameraMode::Static);
			TestEqual(TEXT("sequence"), FCombatCameraRules::SelectMode(false, true, true), ECombatCameraMode::Static);
		});
	});

	Describe("ShouldPlayCinematic (D-078)", [this]()
	{
		It("by default follows camera movement", [this]()
		{
			TestTrue(TEXT("movement on: plays"), FCombatCameraRules::ShouldPlayCinematic(ESkillCinematics::FollowCameraMovement, false, false, true));
			TestFalse(TEXT("movement off: skipped"), FCombatCameraRules::ShouldPlayCinematic(ESkillCinematics::FollowCameraMovement, true, false, true));
		});

		It("can be forced on or off whatever the camera movement", [this]()
		{
			TestTrue(TEXT("on, movement off"), FCombatCameraRules::ShouldPlayCinematic(ESkillCinematics::On, true, false, true));
			TestFalse(TEXT("off, movement on"), FCombatCameraRules::ShouldPlayCinematic(ESkillCinematics::Off, false, false, true));
		});

		It("plays enemy cinematics unless that setting is off", [this]()
		{
			TestTrue(TEXT("enemy, setting on"), FCombatCameraRules::ShouldPlayCinematic(ESkillCinematics::FollowCameraMovement, false, true, true));
			TestFalse(TEXT("enemy, setting off"), FCombatCameraRules::ShouldPlayCinematic(ESkillCinematics::FollowCameraMovement, false, true, false));
			TestTrue(TEXT("own side, enemy setting off"), FCombatCameraRules::ShouldPlayCinematic(ESkillCinematics::FollowCameraMovement, false, false, false));
		});
	});

	Describe("TemplateKindFor", [this]()
	{
		It("picks Spell for spells and the execution type for other skills", [this]()
		{
			USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
			UAbilityData *Melee = NewObject<UAbilityData>(GetTransientPackage());
			Melee->ExecutionType = EAbilityExecutionType::Melee;
			UAbilityData *Ranged = NewObject<UAbilityData>(GetTransientPackage());
			Ranged->ExecutionType = EAbilityExecutionType::Ranged;

			TestEqual(TEXT("spell"), FCombatCameraRules::TemplateKindFor(Spell, EActionType::Spell), ECameraTemplateKind::Spell);
			TestEqual(TEXT("melee ability"), FCombatCameraRules::TemplateKindFor(Melee, EActionType::Ability), ECameraTemplateKind::Melee);
			TestEqual(TEXT("ranged ability"), FCombatCameraRules::TemplateKindFor(Ranged, EActionType::Ability), ECameraTemplateKind::Ranged);
			TestEqual(TEXT("no skill"), FCombatCameraRules::TemplateKindFor(nullptr, EActionType::Ability), ECameraTemplateKind::Melee);
		});
	});

	Describe("ComputeView", [this]()
	{
		It("puts the centred-wide turn view behind and above the character, facing the enemies", [this]()
		{
			const FTransform View = FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, ETurnViewStyle::CentredWide, CombatCameraTestFrame());
			TestTrue(TEXT("behind"), View.GetLocation().X < 0.0f);
			TestTrue(TEXT("above"), View.GetLocation().Z > 0.0f);
			TestTrue(TEXT("centred"), FMath::IsNearlyZero(View.GetLocation().Y));
			TestTrue(TEXT("faces the enemies"), View.GetRotation().GetForwardVector().X > 0.0f);
		});

		It("puts the over-the-shoulder view closer than centred-wide and off to one side", [this]()
		{
			const FCombatCameraFrame Frame = CombatCameraTestFrame();
			const FVector Wide = FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, ETurnViewStyle::CentredWide, Frame).GetLocation();
			const FVector Shoulder = FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, ETurnViewStyle::OverTheShoulder, Frame).GetLocation();
			TestTrue(TEXT("closer"), Shoulder.Size2D() < Wide.Size2D());
			TestFalse(TEXT("to one side"), FMath::IsNearlyZero(Shoulder.Y));
		});

		It("views the tactical turn from the side of the fight", [this]()
		{
			const FCombatCameraFrame Frame = CombatCameraTestFrame();
			const FTransform View = FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, ETurnViewStyle::Tactical, Frame);
			TestTrue(TEXT("side-on"), FMath::Abs(View.GetLocation().Y) > FMath::Abs(View.GetLocation().X - Frame.ArenaCentre.X));
		});

		It("pans further out on someone else's turn than on your own", [this]()
		{
			const FCombatCameraFrame Frame = CombatCameraTestFrame();
			const FVector Turn = FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, ETurnViewStyle::CentredWide, Frame).GetLocation();
			const FVector Wide = FCombatCameraRules::ComputeView(ECombatCameraMode::NotYourTurn, ETurnViewStyle::CentredWide, Frame).GetLocation();
			TestTrue(TEXT("further from the character"), FVector::Dist(Wide, Frame.Focus) > FVector::Dist(Turn, Frame.Focus));
		});

		It("keeps the static view in one place whoever acts", [this]()
		{
			FCombatCameraFrame Frame = CombatCameraTestFrame();
			const FVector First = FCombatCameraRules::ComputeView(ECombatCameraMode::Static, ETurnViewStyle::CentredWide, Frame).GetLocation();
			Frame.Focus = FVector(0.0f, 400.0f, 0.0f);
			const FVector Second = FCombatCameraRules::ComputeView(ECombatCameraMode::Static, ETurnViewStyle::Tactical, Frame).GetLocation();
			TestTrue(TEXT("same place"), First.Equals(Second));
		});
	});

	Describe("the turn-view setting (B-398, study R1)", [this]()
	{
		It("over the shoulder puts the hero in the lower-left third and the enemy centre in the middle band, 1 or 3 enemies", [this]()
		{
			const TArray<FVector> Three = TurnTestThreeEnemies();
			const FCombatCameraFrame Frames[] = {TurnViewTestFrame(FVector::ZeroVector, {TURN_TEST_ONE_ENEMY}),
												 TurnViewTestFrame(FVector::ZeroVector, Three)};
			for (const FCombatCameraFrame &Frame : Frames)
			{
				const FTransform View = FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, ETurnViewStyle::OverTheShoulder, Frame);
				const FVector2D Hero = ProjectInTurnView(View, Frame.Focus);
				const FVector2D Centre = ProjectInTurnView(View, Frame.EnemyCentre);
				const FString Where = FString::Printf(TEXT("hero %s, enemy centre %s"), *CameraFraming::GetThirdsCell(Hero), *CameraFraming::GetThirdsCell(Centre));
				TestTrue(FString::Printf(TEXT("hero on screen (%s)"), *Where),
						 CameraFraming::IsOnScreen(View, FCombatCameraRules::DefaultFieldOfView, CameraFraming::DEFAULT_ASPECT, Frame.Focus));
				TestTrue(FString::Printf(TEXT("hero in the left third (%s)"), *Where), Hero.X < -CameraFraming::THIRD_LINE);
				TestTrue(FString::Printf(TEXT("hero in the lower third (%s)"), *Where), Hero.Y < -CameraFraming::THIRD_LINE);
				TestTrue(FString::Printf(TEXT("enemy centre in the middle band (%s)"), *Where),
						 FMath::Abs(Centre.X) <= CameraFraming::THIRD_LINE && FMath::Abs(Centre.Y) <= CameraFraming::THIRD_LINE);
			}
			const FTransform ThreeView = FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, ETurnViewStyle::OverTheShoulder, Frames[1]);
			for (const FVector &Enemy : Three)
			{
				TestTrue(TEXT("every one of three enemies on screen"),
						 CameraFraming::IsOnScreen(ThreeView, FCombatCameraRules::DefaultFieldOfView, CameraFraming::DEFAULT_ASPECT, Enemy));
			}
		});

		It("gives every party member the same over-the-shoulder layout, at any range", [this]()
		{
			const FCombatCameraFrame First = TurnViewTestFrame(FVector::ZeroVector, {TURN_TEST_ONE_ENEMY});
			const FCombatCameraFrame Beside = TurnViewTestFrame(TURN_TEST_SIDE_MEMBER, TurnTestThreeEnemies());
			const FCombatCameraFrame Near = TurnViewTestFrame(FVector::ZeroVector, {TURN_TEST_NEAR_ENEMY});
			const FVector2D FirstHero = ProjectInTurnView(FCombatCameraRules::ComputeTurnView(ETurnViewStyle::OverTheShoulder, First), First.Focus);
			const FVector2D BesideHero = ProjectInTurnView(FCombatCameraRules::ComputeTurnView(ETurnViewStyle::OverTheShoulder, Beside), Beside.Focus);
			const FVector2D NearHero = ProjectInTurnView(FCombatCameraRules::ComputeTurnView(ETurnViewStyle::OverTheShoulder, Near), Near.Focus);
			TestTrue(TEXT("a member beside the first, same spot"), FirstHero.Equals(BesideHero, TURN_TEST_LAYOUT_TOLERANCE));
			TestTrue(TEXT("a closer enemy, same spot"), FirstHero.Equals(NearHero, TURN_TEST_LAYOUT_TOLERANCE));
		});

		It("keeps the centred-wide turn view where B-156 put it", [this]()
		{
			const FCombatCameraFrame Frame = CombatCameraTestFrame();
			const FTransform View = FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, ETurnViewStyle::CentredWide, Frame);
			TestTrue(TEXT("same place"), View.GetLocation().Equals(TURN_TEST_CENTRED_WIDE_LOCATION, TURN_TEST_LOCATION_TOLERANCE));
			TestTrue(TEXT("ComputeView's turn is ComputeTurnView"),
					 View.Equals(FCombatCameraRules::ComputeTurnView(ETurnViewStyle::CentredWide, Frame)));
		});

		It("names the setting and each style for the player", [this]()
		{
			TestFalse(TEXT("setting name"), FCombatCameraRules::GetTurnViewSettingName().IsEmpty());
			const FText Wide = FCombatCameraRules::GetTurnViewStyleName(ETurnViewStyle::CentredWide);
			const FText Shoulder = FCombatCameraRules::GetTurnViewStyleName(ETurnViewStyle::OverTheShoulder);
			TestFalse(TEXT("named"), Wide.IsEmpty() || Shoulder.IsEmpty());
			TestFalse(TEXT("distinct"), Wide.EqualTo(Shoulder));
		});
	});

	Describe("the cinematic window (B-370, D-162, Q-370)", [this]()
	{
		const float Read = FCombatCameraRules::MinDefenseReadSeconds;

		It("cuts a placed window short so it ends the read time before the first defendable moment", [this, Read]()
		{
			FCinematicWindowTiming Timing;
			Timing.bHasAuthoredWindow = true;
			Timing.AuthoredStart = 0.0f;
			Timing.AuthoredEnd = 3.0f;
			Timing.bHasDefendable = true;
			Timing.FirstDefendable = 2.0f;
			float End = 0.0f;
			TestTrue(TEXT("a window"), FCombatCameraRules::ResolveWindowEnd(Timing, End));
			TestEqual(TEXT("clamped"), End, 2.0f - Read, KINDA_SMALL_NUMBER);
			Timing.AuthoredEnd = 0.5f;
			FCombatCameraRules::ResolveWindowEnd(Timing, End);
			TestEqual(TEXT("an early placed end stays"), End, 0.5f, KINDA_SMALL_NUMBER);
			Timing.FirstDefendable = Read * 0.5f;
			FCombatCameraRules::ResolveWindowEnd(Timing, End);
			TestEqual(TEXT("a hit too soon: no cinematic"), End, 0.0f, KINDA_SMALL_NUMBER);
		});

		It("runs to the wind-up end when no range is placed, still clamped", [this, Read]()
		{
			FCinematicWindowTiming Timing;
			Timing.bHasWindUpEnd = true;
			Timing.WindUpEnd = 1.0f;
			Timing.bHasDefendable = true;
			Timing.FirstDefendable = 3.0f;
			float End = 0.0f;
			FCombatCameraRules::ResolveWindowEnd(Timing, End);
			TestEqual(TEXT("the wind-up end"), End, 1.0f, KINDA_SMALL_NUMBER);
			Timing.bHasWindUpEnd = false;
			FCombatCameraRules::ResolveWindowEnd(Timing, End);
			TestEqual(TEXT("no wind-up leg: the read time alone"), End, 3.0f - Read, KINDA_SMALL_NUMBER);
			TestFalse(TEXT("nothing placed, nothing to defend: no hand-off"), FCombatCameraRules::ResolveWindowEnd(FCinematicWindowTiming(), End));
		});

		It("hands the defending viewer to Defence and everyone else to the watch view", [this]()
		{
			TestEqual(TEXT("defending"), FCombatCameraRules::SelectHandOffMode(true, true, false), ECombatCameraMode::Defence);
			TestEqual(TEXT("defending, no sequence"), FCombatCameraRules::SelectHandOffMode(true, false, false), ECombatCameraMode::Defence);
			TestEqual(TEXT("not defending keeps the cinematic"), FCombatCameraRules::SelectHandOffMode(false, true, false), ECombatCameraMode::Action);
			TestEqual(TEXT("not defending, no sequence: watch"), FCombatCameraRules::SelectHandOffMode(false, false, false), ECombatCameraMode::NotYourTurn);
			TestEqual(TEXT("movement off: static (D-078)"), FCombatCameraRules::SelectHandOffMode(true, true, true), ECombatCameraMode::Static);
		});

		It("puts the Defence view behind the defender, facing the attacker; further back for several", [this]()
		{
			const FVector Attacker(1000.0f, 0.0f, 0.0f);
			const FTransform One = FCombatCameraRules::ComputeDefenceView({FVector::ZeroVector}, Attacker);
			TestTrue(TEXT("behind the defender"), One.GetLocation().X < 0.0f);
			TestTrue(TEXT("above it"), One.GetLocation().Z > 0.0f);
			TestTrue(TEXT("facing the attacker"), One.GetRotation().GetForwardVector().X > 0.0f);
			const FTransform Two = FCombatCameraRules::ComputeDefenceView({FVector(0.0f, -300.0f, 0.0f), FVector(0.0f, 300.0f, 0.0f)}, Attacker);
			TestTrue(TEXT("two spread out: further back"), Two.GetLocation().X < One.GetLocation().X);
		});

		It("reads nothing from a skill without a montage", [this]()
		{
			const FCinematicWindowTiming Timing = FCombatCameraRules::ReadWindowTiming(NewObject<UAbilityData>(GetTransientPackage()), 1.0f, false);
			TestFalse(TEXT("no window"), Timing.bHasAuthoredWindow);
			TestFalse(TEXT("nothing to defend"), Timing.bHasDefendable);
		});
	});

	Describe("shot patterns (B-371, Q-371)", [this]()
	{
		// Caster at the origin, one target 1000 cm ahead on +X (Right is +Y).
		const auto Context = [](TArray<FVector> Targets = {FVector(1000.0f, 0.0f, 0.0f)})
		{
			FCameraShotContext Out;
			Out.Targets = MoveTemp(Targets);
			return Out;
		};

		It("winds up over the caster's shoulder and pushes in", [this, Context]()
		{
			const FCameraShot Shot = FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Context(), FSkillCameraTuning());
			TestTrue(TEXT("behind the caster"), Shot.End.GetLocation().X < 0.0f);
			TestTrue(TEXT("pushes in"), Shot.Start.GetLocation().X < Shot.End.GetLocation().X);
			TestTrue(TEXT("right shoulder"), Shot.End.GetLocation().Y > 0.0f);
			TestTrue(TEXT("over time"), Shot.BlendSeconds > 0.0f);
			FSkillCameraTuning Left;
			Left.Side = ECameraShotSide::Left;
			TestTrue(TEXT("left shoulder when tuned"), FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Context(), Left).End.GetLocation().Y < 0.0f);
		});

		It("frames the target on impact with a FOV punch and a shake, larger on a crit", [this, Context]()
		{
			const FCameraShot Hit = FCombatCameraRules::ComputeShot(ECameraShotMoment::Impact, Context(), FSkillCameraTuning());
			const FVector ToTarget = (FVector(1000.0f, 0.0f, 0.0f) - Hit.End.GetLocation()).GetSafeNormal();
			TestTrue(TEXT("looks at the target"), FVector::DotProduct(Hit.End.GetRotation().GetForwardVector(), ToTarget) > 0.99f);
			TestTrue(TEXT("FOV punch"), Hit.FieldOfView < FCombatCameraRules::DefaultFieldOfView);
			TestTrue(TEXT("a shake"), Hit.ShakeScale > 0.0f);
			FCameraShotContext CritContext = Context();
			CritContext.bCritical = true;
			const FCameraShot Crit = FCombatCameraRules::ComputeShot(ECameraShotMoment::Impact, CritContext, FSkillCameraTuning());
			TestTrue(TEXT("crit: a bigger punch"), Crit.FieldOfView < Hit.FieldOfView);
			TestTrue(TEXT("crit: a bigger shake"), Crit.ShakeScale > Hit.ShakeScale);
			FSkillCameraTuning Calm;
			Calm.ShakeScale = 0.0f;
			const FCameraShot Still = FCombatCameraRules::ComputeShot(ECameraShotMoment::Impact, CritContext, Calm);
			TestEqual(TEXT("no shake when tuned off"), Still.ShakeScale, 0.0f);
			TestEqual(TEXT("no punch when tuned off"), Still.FieldOfView, FCombatCameraRules::DefaultFieldOfView);
		});

		It("pulls out for several targets, further the wider they stand", [this, Context]()
		{
			TestEqual(TEXT("one target: Impact"), FCombatCameraRules::ImpactMomentFor(1), ECameraShotMoment::Impact);
			TestEqual(TEXT("two: Spread"), FCombatCameraRules::ImpactMomentFor(2), ECameraShotMoment::Spread);
			const FCameraShot Close = FCombatCameraRules::ComputeShot(ECameraShotMoment::Spread,
				Context({FVector(1000.0f, -100.0f, 0.0f), FVector(1000.0f, 100.0f, 0.0f)}), FSkillCameraTuning());
			const FCameraShot Far = FCombatCameraRules::ComputeShot(ECameraShotMoment::Spread,
				Context({FVector(1000.0f, -600.0f, 0.0f), FVector(1000.0f, 600.0f, 0.0f)}), FSkillCameraTuning());
			TestTrue(TEXT("wider spread: further back"), Far.End.GetLocation().X < Close.End.GetLocation().X);
		});

		It("follows the projectile on the Cast shot, or holds on the target", [this, Context]()
		{
			FCameraShotContext Flying = Context();
			Flying.Projectile = FVector(400.0f, 0.0f, 600.0f);
			const FCameraShot Follow = FCombatCameraRules::ComputeShot(ECameraShotMoment::Cast, Flying, FSkillCameraTuning());
			const FVector ToProjectile = (Flying.Projectile.GetValue() - Follow.End.GetLocation()).GetSafeNormal();
			TestTrue(TEXT("looks at the projectile"), FVector::DotProduct(Follow.End.GetRotation().GetForwardVector(), ToProjectile) > 0.99f);
			FSkillCameraTuning Hold;
			Hold.bFollowProjectile = false;
			const FCameraShot Held = FCombatCameraRules::ComputeShot(ECameraShotMoment::Cast, Flying, Hold);
			const FVector ToTarget = (FVector(1000.0f, 0.0f, 0.0f) - Held.End.GetLocation()).GetSafeNormal();
			TestTrue(TEXT("tuned off: looks at the target"), FVector::DotProduct(Held.End.GetRotation().GetForwardVector(), ToTarget) > 0.99f);
		});

		It("changes with the tuning: distance and style", [this, Context]()
		{
			const float Base = FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Context(), FSkillCameraTuning()).End.GetLocation().X;
			FSkillCameraTuning Twice;
			Twice.DistanceScale = 2.0f;
			TestEqual(TEXT("twice as far back"), FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Context(), Twice).End.GetLocation().X, Base * 2.0, 0.01);
			FSkillCameraTuning Close, Wide;
			Close.Style = ECameraShotStyle::Close;
			Wide.Style = ECameraShotStyle::Wide;
			TestTrue(TEXT("Wide sits further back than Close"),
					 FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Context(), Wide).End.GetLocation().X
						 < FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Context(), Close).End.GetLocation().X);
			FCameraShotContext Ranged = Context();
			Ranged.bRanged = true;
			TestTrue(TEXT("Default frames a ranged skill wide"),
					 FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Ranged, FSkillCameraTuning()).End.GetLocation().X < Base);
		});

		It("blends from start to end and holds at the end", [this, Context]()
		{
			const FCameraShot Shot = FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Context(), FSkillCameraTuning());
			TestTrue(TEXT("at the start"), FCombatCameraRules::BlendShot(Shot, 0.0f).GetLocation().Equals(Shot.Start.GetLocation(), 0.1f));
			TestTrue(TEXT("at the end"), FCombatCameraRules::BlendShot(Shot, 1.0f).GetLocation().Equals(Shot.End.GetLocation(), 0.1f));
			TestTrue(TEXT("held past the end"), FCombatCameraRules::BlendShot(Shot, 2.0f).GetLocation().Equals(Shot.End.GetLocation(), 0.1f));
		});

		It("plays a skill's own CameraSequence instead (a signature move)", [this]()
		{
			UAbilityData *Plain = NewObject<UAbilityData>(GetTransientPackage());
			TestTrue(TEXT("no sequence: the patterns"), FCombatCameraRules::UsesShotPatterns(Plain));
			UAbilityData *Signature = NewObject<UAbilityData>(GetTransientPackage());
			Signature->CameraSequence = TSoftObjectPtr<ULevelSequence>(FSoftObjectPath(TEXT("/Game/Test/LS_SignatureTest.LS_SignatureTest")));
			TestFalse(TEXT("its own sequence wins"), FCombatCameraRules::UsesShotPatterns(Signature));
			TestFalse(TEXT("no skill: nothing"), FCombatCameraRules::UsesShotPatterns(nullptr));
		});
	});

	Describe("wide beats and the party handover (B-404, study R12, R16)", [this]()
	{
		It("passes a turn change after an action through the wide view, then blends to the next hero", [this]()
		{
			const FCombatCameraFrame Frame = TurnViewTestFrame(TURN_TEST_SIDE_MEMBER, {TURN_TEST_ONE_ENEMY});
			const FTurnChangePlan Plan = FCombatCameraRules::PlanTurnChange(true, false, ECombatCameraMode::Turn, ETurnViewStyle::OverTheShoulder, Frame);
			const FTransform Wide = FCombatCameraRules::ComputeView(ECombatCameraMode::NotYourTurn, ETurnViewStyle::OverTheShoulder, Frame);
			const FTransform Turn = FCombatCameraRules::ComputeTurnView(ETurnViewStyle::OverTheShoulder, Frame);
			TestTrue(TEXT("a wide beat"), Plan.bWideBeat);
			TestTrue(TEXT("through the wide (NotYourTurn) view"), Plan.WideView.GetLocation().Equals(Wide.GetLocation(), TURN_TEST_LOCATION_TOLERANCE));
			TestTrue(TEXT("ends on the next hero's turn view"), Plan.TurnView.GetLocation().Equals(Turn.GetLocation(), TURN_TEST_LOCATION_TOLERANCE));
			TestTrue(TEXT("the wide is higher than the turn view"), Plan.WideView.GetLocation().Z > Plan.TurnView.GetLocation().Z);
			TestTrue(TEXT("the beat holds after blending in"), Plan.BeatSeconds > Plan.BeatBlendSeconds && Plan.BeatBlendSeconds > 0.0f);
			TestTrue(TEXT("the handover is a blend, not a cut"), Plan.HandoverBlendSeconds > 0.0f);
		});

		It("adds no wide beat between actions of one actor, on the first turn, with movement off or before an enemy's turn", [this]()
		{
			const FCombatCameraFrame Frame = CombatCameraTestFrame();
			const FTurnChangePlan SameActor = FCombatCameraRules::PlanTurnChange(true, true, ECombatCameraMode::Turn, ETurnViewStyle::CentredWide, Frame);
			const FTurnChangePlan NobodyActed = FCombatCameraRules::PlanTurnChange(false, false, ECombatCameraMode::Turn, ETurnViewStyle::CentredWide, Frame);
			const FTurnChangePlan MovementOff = FCombatCameraRules::PlanTurnChange(true, false, ECombatCameraMode::Static, ETurnViewStyle::CentredWide, Frame);
			const FTurnChangePlan EnemyTurn = FCombatCameraRules::PlanTurnChange(true, false, ECombatCameraMode::NotYourTurn, ETurnViewStyle::CentredWide, Frame);
			TestFalse(TEXT("same actor keeps acting (B-401)"), SameActor.bWideBeat);
			TestFalse(TEXT("first turn: nobody acted yet"), NobodyActed.bWideBeat);
			TestFalse(TEXT("movement off: the static view"), MovementOff.bWideBeat);
			TestFalse(TEXT("an enemy's watch view is already the wide"), EnemyTurn.bWideBeat);
			const FTransform Static = FCombatCameraRules::ComputeView(ECombatCameraMode::Static, ETurnViewStyle::CentredWide, Frame);
			TestTrue(TEXT("movement off still ends on the static view"), MovementOff.TurnView.GetLocation().Equals(Static.GetLocation(), TURN_TEST_LOCATION_TOLERANCE));
		});
	});

	Describe("no reset between actions (B-401, study R4)", [this]()
	{
		It("sends the second action of a turn back to the same turn transform, in one short blend", [this]()
		{
			const FCombatCameraFrame Frame = TurnViewTestFrame(TURN_TEST_SIDE_MEMBER, {TURN_TEST_ONE_ENEMY});
			for (const ETurnViewStyle Style : {ETurnViewStyle::CentredWide, ETurnViewStyle::OverTheShoulder, ETurnViewStyle::Tactical})
			{
				const FTurnChangePlan TurnStart = FCombatCameraRules::PlanTurnChange(true, false, ECombatCameraMode::Turn, Style, Frame);
				const FSameTurnReturnPlan Return = FCombatCameraRules::PlanSameTurnReturn(true, ECombatCameraMode::Turn, Style, Frame);
				const FString Name = UEnum::GetValueAsString(Style);
				TestTrue(Name + TEXT(": a return"), Return.bReturn);
				TestTrue(Name + TEXT(": the turn start's own transform"), Return.TurnView.Equals(TurnStart.TurnView, TURN_TEST_LOCATION_TOLERANCE));
				TestTrue(Name + TEXT(": a blend, not a cut"), Return.BlendSeconds > 0.0f);
				TestTrue(Name + TEXT(": shorter than the handover from wide"), Return.BlendSeconds < TurnStart.HandoverBlendSeconds);
			}
		});

		It("leaves the camera alone before any action, on an enemy's watch view and with movement off", [this]()
		{
			const FCombatCameraFrame Frame = CombatCameraTestFrame();
			TestFalse(TEXT("asked again without acting (a refused pick)"),
					  FCombatCameraRules::PlanSameTurnReturn(false, ECombatCameraMode::Turn, ETurnViewStyle::CentredWide, Frame).bReturn);
			TestFalse(TEXT("an enemy's turn"),
					  FCombatCameraRules::PlanSameTurnReturn(true, ECombatCameraMode::NotYourTurn, ETurnViewStyle::CentredWide, Frame).bReturn);
			TestFalse(TEXT("movement off: the static view"),
					  FCombatCameraRules::PlanSameTurnReturn(true, ECombatCameraMode::Static, ETurnViewStyle::CentredWide, Frame).bReturn);
		});
	});
}

BEGIN_DEFINE_SPEC(FCombatCameraDirectorSpec, "WoR.Combat.Camera.Director",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
ACombatOrchestrator *Orchestrator = nullptr;
AActor *Hero = nullptr;
AActor *Foe = nullptr;
END_DEFINE_SPEC(FCombatCameraDirectorSpec)

void FCombatCameraDirectorSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	It("gives every combatant a camera for the fight and removes them after", [this]()
	{
		UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
		if (!TestTrue(TEXT("setup"), Director && Hero && Foe))
		{
			return;
		}

		Orchestrator->StartCombat({Hero}, {Foe});
		TestTrue(TEXT("in combat"), Director->IsInCombat());
		TestEqual(TEXT("one camera per combatant"), Director->GetCameraCount(), 2);
		TestNotNull(TEXT("hero has a camera"), Hero->FindComponentByClass<UCombatCameraComponent>());
		TestNotNull(TEXT("follows the first turn"), Director->GetViewedActor());

		Orchestrator->ForceEndCombat(ECombatState::Victory);
		TestFalse(TEXT("out of combat"), Director->IsInCombat());
		TestEqual(TEXT("no cameras left"), Director->GetCameraCount(), 0);
		TestNull(TEXT("hero's camera removed"), Hero->FindComponentByClass<UCombatCameraComponent>());
	});

	It("fires the turn handover once per turn start, whoever acts (B-404, study R16)", [this]()
	{
		UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
		AActor *SecondHero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("setup"), Director && Hero && SecondHero && Foe))
		{
			return;
		}
		Director->BeginCombat({Hero, SecondHero}, {Foe}, nullptr);
		TestEqual(TEXT("none before the first turn"), Director->GetHandoverCount(), 0);
		// Hero -> hero, hero -> enemy, enemy -> hero, and a second turn in a row for one hero.
		const TArray<AActor *> TurnOrder = {Hero, SecondHero, Foe, Hero, Hero};
		for (int32 Turn = 0; Turn < TurnOrder.Num(); ++Turn)
		{
			Orchestrator->OnActorTurnStarted.Broadcast(TurnOrder[Turn], Turn + 1);
			TestEqual(FString::Printf(TEXT("one handover after turn start %d"), Turn + 1), Director->GetHandoverCount(), Turn + 1);
		}
		TestTrue(TEXT("follows the last turn"), Director->GetViewedActor() == Hero);
		Director->EndCombat();
		Director->BeginCombat({Hero, SecondHero}, {Foe}, nullptr);
		TestEqual(TEXT("a new fight counts from zero"), Director->GetHandoverCount(), 0);
		Director->EndCombat();
	});

	It("fires a pending handover exactly once when a turn, or an action, starts inside the beat (B-404)", [this]()
	{
		UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
		UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
		APlayerController *Viewer = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<APlayerController>() : nullptr;
		AActor *SecondHero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("setup"), Director && Executor && Viewer && Hero && SecondHero && Foe))
		{
			return;
		}
		Director->BeginCombat({Hero, SecondHero}, {Foe}, Executor);
		Orchestrator->OnActorTurnStarted.Broadcast(Hero, 1);
		TestEqual(TEXT("first turn: at once"), Director->GetHandoverCount(), 1);

		// A turn starting inside the beat.
		Executor->OnActionStarted.Broadcast(Hero, FAction(), 0);
		Orchestrator->OnActorTurnStarted.Broadcast(SecondHero, 2);
		TestTrue(TEXT("hero -> hero after an action: a wide beat"), Director->IsHandoverPending());
		TestEqual(TEXT("the beat holds the handover"), Director->GetHandoverCount(), 1);
		Orchestrator->OnActorTurnStarted.Broadcast(Hero, 3); // nobody acted in turn 2: no beat of its own
		TestFalse(TEXT("the next turn ran the pending one"), Director->IsHandoverPending());
		TestEqual(TEXT("the pending one once, then turn 3's own"), Director->GetHandoverCount(), 3);

		// An action starting inside the beat.
		Executor->OnActionStarted.Broadcast(Hero, FAction(), 0);
		Orchestrator->OnActorTurnStarted.Broadcast(SecondHero, 4);
		TestTrue(TEXT("a wide beat again"), Director->IsHandoverPending());
		Executor->OnActionStarted.Broadcast(SecondHero, FAction(), 0);
		TestFalse(TEXT("the action ran the pending one"), Director->IsHandoverPending());
		TestEqual(TEXT("once for turn 4"), Director->GetHandoverCount(), 4);
		Executor->OnActionStarted.Broadcast(SecondHero, FAction(), 0);
		TestEqual(TEXT("a second action of the turn adds none"), Director->GetHandoverCount(), 4);
		Director->EndCombat();
	});

	It("returns to the same hero's turn view between actions of one turn, with no wide beat (B-401, study R4)", [this]()
	{
		UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
		UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
		APlayerController *Viewer = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<APlayerController>() : nullptr;
		AActor *SecondHero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("setup"), Director && Executor && Viewer && Hero && SecondHero && Foe))
		{
			return;
		}
		Director->BeginCombat({Hero, SecondHero}, {Foe}, Executor);
		Orchestrator->OnActorTurnStarted.Broadcast(Hero, 1);
		Orchestrator->OnActionRequested.Broadcast(Hero);
		TestEqual(TEXT("the turn start's own ask is no return"), Director->GetSameTurnReturnCount(), 0);

		// Two actions in one turn (B-170c): asked again after each.
		Executor->OnActionStarted.Broadcast(Hero, FAction(), 0);
		Orchestrator->OnActionRequested.Broadcast(Hero);
		TestEqual(TEXT("asked again after an action: a return"), Director->GetSameTurnReturnCount(), 1);
		TestFalse(TEXT("no wide beat between actions of one turn"), Director->IsHandoverPending());
		TestEqual(TEXT("no handover between actions of one turn"), Director->GetHandoverCount(), 1);
		TestEqual(TEXT("the turn view again"), Director->GetCurrentMode(), ECombatCameraMode::Turn);
		Orchestrator->OnActionRequested.Broadcast(Hero); // a refused pick asked again
		TestEqual(TEXT("no action since the last ask: none"), Director->GetSameTurnReturnCount(), 1);
		Executor->OnActionStarted.Broadcast(Hero, FAction(), 0);
		Orchestrator->OnActionRequested.Broadcast(Hero);
		TestEqual(TEXT("a third action's ask: a return"), Director->GetSameTurnReturnCount(), 2);

		// Only a turn change does the full move.
		Orchestrator->OnActorTurnStarted.Broadcast(SecondHero, 2);
		TestTrue(TEXT("hero -> hero after acting: the wide beat"), Director->IsHandoverPending());
		Orchestrator->OnActionRequested.Broadcast(SecondHero);
		TestEqual(TEXT("a new turn's ask is no return"), Director->GetSameTurnReturnCount(), 2);

		// Movement off: the static view stays.
		Director->SetCameraMovementOff(true);
		Orchestrator->OnActorTurnStarted.Broadcast(Hero, 3);
		Executor->OnActionStarted.Broadcast(Hero, FAction(), 0);
		Orchestrator->OnActionRequested.Broadcast(Hero);
		TestEqual(TEXT("movement off: no return"), Director->GetSameTurnReturnCount(), 2);
		TestEqual(TEXT("movement off: static"), Director->GetCurrentMode(), ECombatCameraMode::Static);
		Director->SetCameraMovementOff(false);
		Director->EndCombat();
		Director->BeginCombat({Hero, SecondHero}, {Foe}, Executor);
		TestEqual(TEXT("a new fight counts from zero"), Director->GetSameTurnReturnCount(), 0);
		Director->EndCombat();
	});

	It("drops a pending handover when the fight ends, and never fires one for a preview (B-404)", [this]()
	{
		UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
		UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
		APlayerController *Viewer = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<APlayerController>() : nullptr;
		AActor *SecondHero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("setup"), Director && Executor && Viewer && Hero && SecondHero && Foe))
		{
			return;
		}
		Director->BeginCombat({Hero, SecondHero}, {Foe}, Executor);
		Orchestrator->OnActorTurnStarted.Broadcast(Hero, 1);
		Executor->OnActionStarted.Broadcast(Hero, FAction(), 0);
		Orchestrator->OnActorTurnStarted.Broadcast(SecondHero, 2);
		TestTrue(TEXT("a wide beat"), Director->IsHandoverPending());
		Director->EndCombat();
		TestFalse(TEXT("the fight's end dropped it"), Director->IsHandoverPending());
		TestWorld->Tick(BEAT_TEST_TICKS_PAST_BEAT);
		TestEqual(TEXT("its timer was cleared: nothing fires later"), Director->GetHandoverCount(), 1);

		Director->BeginCombat({Hero, SecondHero}, {Foe}, Executor);
		Orchestrator->OnActorTurnStarted.Broadcast(Hero, 1);
		TestTrue(TEXT("a preview plays a beat"), Director->PreviewHandover(1));
		TestEqual(TEXT("a preview fires nothing at its start"), Director->GetHandoverCount(), 1);
		TestWorld->Tick(BEAT_TEST_TICKS_PAST_BEAT);
		TestEqual(TEXT("nor at its end"), Director->GetHandoverCount(), 1);
		TestTrue(TEXT("the turn is unchanged"), Director->GetViewedActor() == Hero);
		TestTrue(TEXT("a second preview"), Director->PreviewHandover(1));
		Orchestrator->OnActorTurnStarted.Broadcast(Hero, 2); // a turn inside the preview: only its own handover
		TestEqual(TEXT("the flushed preview fires nothing"), Director->GetHandoverCount(), 2);
		Director->EndCombat();
	});

	It("keeps the turn-view setting across a fight restart and on a new orchestrator (B-398, saved)", [this]()
	{
		UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
		USaveSubsystem *Save = TestWorld->GetSubsystem<USaveSubsystem>();
		if (!TestTrue(TEXT("setup"), Director && Save && Hero && Foe))
		{
			return;
		}
		Save->SetTurnViewStyle(ETurnViewStyle::CentredWide); // the default (D-074), whatever a loaded slot held

		Orchestrator->StartCombat({Hero}, {Foe});
		TestEqual(TEXT("the first fight starts Centred Wide"), Director->GetTurnViewStyle(), ETurnViewStyle::CentredWide);
		Director->SetTurnViewStyle(ETurnViewStyle::OverTheShoulder);
		TestEqual(TEXT("the choice is in the save"), Save->GetTurnViewStyle(), ETurnViewStyle::OverTheShoulder);
		Orchestrator->ForceEndCombat(ECombatState::Victory);

		// The restart: what StartCombat calls, on a director whose own field was reset.
		Director->TurnViewStyle = ETurnViewStyle::CentredWide;
		Director->BeginCombat({Hero}, {Foe}, nullptr);
		TestEqual(TEXT("a restarted fight keeps it"), Director->GetTurnViewStyle(), ETurnViewStyle::OverTheShoulder);
		Director->EndCombat();

		// A new level's orchestrator (its director has never been set).
		ACombatOrchestrator *Next = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
		UCombatCameraDirector *NextDirector = Next ? Next->GetCameraDirector() : nullptr;
		if (!TestNotNull(TEXT("second orchestrator"), NextDirector))
		{
			return;
		}
		NextDirector->BeginCombat({Hero}, {Foe}, nullptr);
		TestEqual(TEXT("a new orchestrator starts with it"), NextDirector->GetTurnViewStyle(), ETurnViewStyle::OverTheShoulder);
		NextDirector->EndCombat();

		Save->SetTurnViewStyle(static_cast<ETurnViewStyle>(TURN_TEST_UNKNOWN_STYLE));
		TestEqual(TEXT("an unknown saved value reads as the default"), Save->GetTurnViewStyle(), ETurnViewStyle::CentredWide);
		Save->SetTurnViewStyle(ETurnViewStyle::CentredWide);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
