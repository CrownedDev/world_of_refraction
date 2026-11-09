// CombatCameraSpec.cpp
// Combat camera rules (B-156, Design/CombatCamera.md): the mode the viewer sees, the template a
// skill falls back to, and where each mode puts the camera. B-370: the cinematic window and the Defence view.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "Combat/Camera/CameraFramingChecks.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Camera/CombatCameraDirector.h"
#include "Combat/Camera/CombatCameraComponent.h"
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

		Describe("the side-on Defence view (B-402, R10, R10b)", [this]()
		{
			// Defender(s) around the origin, the attacker 1000 cm ahead on +X, facing back along -X (its right is -Y).
			const FVector Attacker(1000.0f, 0.0f, 0.0f);
			const float HalfHeight = 90.0f; // a character's head and feet above and below its centre
			const auto WholeOnScreen = [this, HalfHeight](const FTransform &View, const FVector &Who, const TCHAR *Name)
			{
				for (const float Z : {-HalfHeight, 0.0f, HalfHeight})
				{
					TestTrue(FString::Printf(TEXT("%s on screen at %+.0f cm"), Name, Z),
							 CameraFraming::IsOnScreen(View, FCombatCameraRules::DefaultFieldOfView, CameraFraming::DEFAULT_ASPECT, Who + FVector(0.0f, 0.0f, Z)));
				}
			};

			It("one target: a low side-on two-shot with both fighters whole on screen, from the weapon side", [this, Attacker, WholeOnScreen]()
			{
				const FVector Defender = FVector::ZeroVector;
				const FTransform View = FCombatCameraRules::ComputeDefenceView({Defender}, Attacker);
				WholeOnScreen(View, Defender, TEXT("defender"));
				WholeOnScreen(View, Attacker, TEXT("attacker"));
				const FVector Mid = (Defender + Attacker) * 0.5f;
				TestTrue(TEXT("side-on: further across the axis than along it"),
						 FMath::Abs(View.GetLocation().Y) > FMath::Abs(View.GetLocation().X - Mid.X));
				TestTrue(TEXT("the attacker's right (weapon) side"), View.GetLocation().Y < 0.0f);
				const FTransform Turn = FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, ETurnViewStyle::CentredWide, CombatCameraTestFrame());
				TestTrue(TEXT("low: below the turn view"), View.GetLocation().Z < Turn.GetLocation().Z);
				TestTrue(TEXT("nearer the defender than the attacker"),
						 FVector::Dist(View.GetLocation(), Defender) < FVector::Dist(View.GetLocation(), Attacker));
			});

			It("several targets: a wide side-on shot that holds every target and the attacker", [this, Attacker, WholeOnScreen]()
			{
				const TArray<FVector> Party = {FVector(0.0f, -300.0f, 0.0f), FVector::ZeroVector, FVector(0.0f, 300.0f, 0.0f)};
				const FTransform Wide = FCombatCameraRules::ComputeDefenceView(Party, Attacker);
				for (int32 Index = 0; Index < Party.Num(); ++Index)
				{
					WholeOnScreen(Wide, Party[Index], *FString::Printf(TEXT("target %d"), Index));
				}
				WholeOnScreen(Wide, Attacker, TEXT("attacker"));
				const FTransform Two = FCombatCameraRules::ComputeDefenceView({FVector::ZeroVector}, Attacker);
				const FVector Mid = (FVector::ZeroVector + Attacker) * 0.5f;
				TestTrue(TEXT("wider than the two-shot"), FVector::Dist2D(Wide.GetLocation(), Mid) > FVector::Dist2D(Two.GetLocation(), Mid));
			});

			It("keeps both fighters on screen when the attacker has closed in to melee range", [this, WholeOnScreen]()
			{
				const FVector Close(150.0f, 0.0f, 0.0f);
				const FTransform View = FCombatCameraRules::ComputeDefenceView({FVector::ZeroVector}, Close);
				WholeOnScreen(View, FVector::ZeroVector, TEXT("defender"));
				WholeOnScreen(View, Close, TEXT("attacker"));
			});
		});

		It("keeps a shot still while the viewer defends: no shake and no FOV punch (B-402, R10, R11)", [this]()
		{
			FCameraShotContext Context;
			Context.Targets = {FVector(1000.0f, 0.0f, 0.0f)};
			Context.bCritical = true;
			const FCameraShot Punch = FCombatCameraRules::ComputeShot(ECameraShotMoment::Impact, Context, FSkillCameraTuning());
			const FCameraShot Steady = FCombatCameraRules::SteadyForDefence(Punch, true);
			TestEqual(TEXT("defending: no shake"), Steady.ShakeScale, 0.0f);
			TestEqual(TEXT("defending: no punch"), Steady.FieldOfView, FCombatCameraRules::DefaultFieldOfView);
			TestTrue(TEXT("defending: same framing"), Steady.End.Equals(Punch.End));
			const FCameraShot Free = FCombatCameraRules::SteadyForDefence(Punch, false);
			TestEqual(TEXT("not defending: the shake stays"), Free.ShakeScale, Punch.ShakeScale);
			TestEqual(TEXT("not defending: the punch stays"), Free.FieldOfView, Punch.FieldOfView);
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
}

#endif // WITH_DEV_AUTOMATION_TESTS
