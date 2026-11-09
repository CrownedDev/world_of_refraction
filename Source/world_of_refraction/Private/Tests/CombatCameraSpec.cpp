// CombatCameraSpec.cpp
// Combat camera rules (B-156, Design/CombatCamera.md): the mode the viewer sees, the template a
// skill falls back to, and where each mode puts the camera. B-370: the cinematic window and the Defence view.
// B-406: boss cinematics (opening before turn 1, phase change) with the HUD broadcast, and the named-attack shot.
// B-406b: the skip key's action (ACombatPlayerController::SkipBossCinematic) ends one and the fight goes on.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Camera/CombatCameraDirector.h"
#include "Combat/Camera/CombatCameraComponent.h"
#include "LevelSequence.h"
#include "Combat/Boss/BossData.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "BossCinematicRecorder.h"
#include "Combat/CombatPlayerController.h"

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

	Describe("boss cinematics and named attacks (B-406)", [this]()
	{
		It("plays a boss cinematic only with a sequence, a local viewer and cinematics on", [this]()
		{
			using R = FCombatCameraRules;
			TestFalse(TEXT("empty field: nothing"), R::ShouldPlayBossCinematic(false, true, ESkillCinematics::FollowCameraMovement, false));
			TestFalse(TEXT("no viewer (headless): nothing"), R::ShouldPlayBossCinematic(true, false, ESkillCinematics::FollowCameraMovement, false));
			TestTrue(TEXT("set and watched: plays"), R::ShouldPlayBossCinematic(true, true, ESkillCinematics::FollowCameraMovement, false));
			TestFalse(TEXT("camera movement off: skipped by default (D-078)"), R::ShouldPlayBossCinematic(true, true, ESkillCinematics::FollowCameraMovement, true));
			TestTrue(TEXT("cinematics forced on: plays"), R::ShouldPlayBossCinematic(true, true, ESkillCinematics::On, true));
			TestFalse(TEXT("cinematics off: nothing"), R::ShouldPlayBossCinematic(true, true, ESkillCinematics::Off, false));
			TestTrue(TEXT("a cap so a broken sequence can't hang the fight"), R::BossCinematicMaxSeconds > 0.0f);
		});

		It("opens an enemy's named attack on the front shot, every other skill on the wind-up", [this]()
		{
			FSkillCameraTuning Named;
			Named.bNamedAttack = true;
			TestEqual(TEXT("enemy, named"), FCombatCameraRules::OpeningMomentFor(Named, true), ECameraShotMoment::NamedAttack);
			TestEqual(TEXT("party member, named"), FCombatCameraRules::OpeningMomentFor(Named, false), ECameraShotMoment::WindUp);
			TestEqual(TEXT("enemy, not named"), FCombatCameraRules::OpeningMomentFor(FSkillCameraTuning(), true), ECameraShotMoment::WindUp);
		});

		It("frames the named attack from in front of the enemy, looking back at it, pushing in", [this]()
		{
			FCameraShotContext Context;
			Context.Targets = {FVector(1000.0f, 0.0f, 0.0f)};
			const FCameraShot Shot = FCombatCameraRules::ComputeShot(ECameraShotMoment::NamedAttack, Context, FSkillCameraTuning());
			TestTrue(TEXT("in front of the caster, toward its targets"), Shot.End.GetLocation().X > 0.0f);
			TestTrue(TEXT("short of the targets"), Shot.Start.GetLocation().X < 1000.0f);
			TestTrue(TEXT("looks back at the caster"), Shot.End.GetRotation().GetForwardVector().X < 0.0f);
			TestTrue(TEXT("pushes in"), Shot.End.GetLocation().X < Shot.Start.GetLocation().X);
			TestTrue(TEXT("over time"), Shot.BlendSeconds > 0.0f);
			TestEqual(TEXT("no shake"), Shot.ShakeScale, 0.0f);
		});

		It("keeps each phase's cinematic with its phase", [this]()
		{
			UBossData *Data = NewObject<UBossData>(GetTransientPackage());
			FBossPhase First;
			First.CinematicSequence = TSoftObjectPtr<ULevelSequence>(FSoftObjectPath(TEXT("/Game/Test/LS_BossPhaseTest.LS_BossPhaseTest")));
			Data->Phases = {First, FBossPhase()};
			UBossPhaseComponent *Phases = NewObject<UBossPhaseComponent>(GetTransientPackage());
			Phases->Setup(Data);
			TestFalse(TEXT("phase 0: its sequence"), Phases->GetPhaseCinematic(0).IsNull());
			TestTrue(TEXT("phase 1: empty"), Phases->GetPhaseCinematic(1).IsNull());
			TestTrue(TEXT("out of range: empty"), Phases->GetPhaseCinematic(2).IsNull());
			TestTrue(TEXT("its boss data"), Phases->GetBossData() == Data);
		});
	});
}

BEGIN_DEFINE_SPEC(FCombatCameraDirectorSpec, "WoR.Combat.Camera.Director",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
ACombatOrchestrator *Orchestrator = nullptr;
AActor *Hero = nullptr;
AActor *Foe = nullptr;

/** B-406: makes Foe a boss with one phase (and Opening as its opening sequence, empty by default). */
UBossPhaseComponent *MakeFoeABoss(const TSoftObjectPtr<ULevelSequence> &Opening = TSoftObjectPtr<ULevelSequence>())
{
	UBossData *Data = NewObject<UBossData>(GetTransientPackage());
	Data->OpeningSequence = Opening;
	Data->Phases.Add(FBossPhase());
	UBossPhaseComponent *Phases = NewObject<UBossPhaseComponent>(Foe);
	Foe->AddInstanceComponent(Phases);
	Phases->RegisterComponent();
	Phases->Setup(Data);
	return Phases;
}
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

	Describe("boss cinematics (B-406)", [this]()
	{
		It("plays nothing for a boss with empty fields: turn 1 starts at once", [this]()
		{
			UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
			if (!TestTrue(TEXT("setup"), Director && Hero && Foe))
			{
				return;
			}
			MakeFoeABoss();
			UBossCinematicRecorder *Recorder = NewObject<UBossCinematicRecorder>();
			Recorder->Listen(Director);
			Orchestrator->StartCombat({Hero}, {Foe});
			TestFalse(TEXT("nothing plays"), Director->IsBossCinematicPlaying());
			TestNotNull(TEXT("turn 1 started"), Director->GetViewedActor());
			TestEqual(TEXT("no HUD broadcast"), Recorder->HudHidden.Num(), 0);
			Orchestrator->ForceEndCombat(ECombatState::Victory);
		});

		It("starts at once with an opening set but no viewer (headless)", [this]()
		{
			UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
			if (!TestTrue(TEXT("setup"), Director && Hero && Foe))
			{
				return;
			}
			MakeFoeABoss(TSoftObjectPtr<ULevelSequence>(FSoftObjectPath(TEXT("/Game/Test/LS_BossOpeningTest.LS_BossOpeningTest"))));
			Orchestrator->StartCombat({Hero}, {Foe});
			TestFalse(TEXT("nothing plays without a viewer"), Director->IsBossCinematicPlaying());
			TestNotNull(TEXT("turn 1 started"), Director->GetViewedActor());
			Orchestrator->ForceEndCombat(ECombatState::Victory);
		});

		It("runs an opening before the first turn, the HUD hidden, then starts turn 1", [this]()
		{
			UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
			if (!TestTrue(TEXT("setup"), Director && Hero && Foe))
			{
				return;
			}
			MakeFoeABoss();
			Director->BossCinematicPreviewSeconds = 1.0f; // the sequence-free hold: a headless world has no viewer
			UBossCinematicRecorder *Recorder = NewObject<UBossCinematicRecorder>();
			Recorder->Listen(Director);
			Orchestrator->StartCombat({Hero}, {Foe});
			TestEqual(TEXT("the opening plays"), Director->GetBossCinematicKind(), EBossCinematicKind::Opening);
			TestNull(TEXT("no turn yet"), Director->GetViewedActor());
			TestTrue(TEXT("the fight waits"), Orchestrator->IsWaitingForAsyncAction());
			TestTrue(TEXT("HUD hidden"), Recorder->HudHidden.Num() == 1 && Recorder->HudHidden[0]);

			Director->FinishBossCinematic();
			TestFalse(TEXT("ended"), Director->IsBossCinematicPlaying());
			TestTrue(TEXT("HUD back"), Recorder->HudHidden.Num() == 2 && !Recorder->HudHidden[1]);
			TestTrue(TEXT("finished: Opening"), Recorder->Finished.Num() == 1 && Recorder->Finished[0] == EBossCinematicKind::Opening);
			TestNotNull(TEXT("turn 1 started after it"), Director->GetViewedActor());
			Orchestrator->ForceEndCombat(ECombatState::Victory);
		});

		It("plays a phase cinematic on the phase event, the HUD hidden around it", [this]()
		{
			UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
			if (!TestTrue(TEXT("setup"), Director && Hero && Foe))
			{
				return;
			}
			UBossPhaseComponent *Phases = MakeFoeABoss();
			UBossCinematicRecorder *Recorder = NewObject<UBossCinematicRecorder>();
			Recorder->Listen(Director);
			Orchestrator->StartCombat({Hero}, {Foe});
			Director->BossCinematicPreviewSeconds = 1.0f; // after the start: no opening hold
			Phases->EnterNextPhase();
			TestEqual(TEXT("the phase cinematic plays"), Director->GetBossCinematicKind(), EBossCinematicKind::Phase);
			TestTrue(TEXT("HUD hidden"), Recorder->HudHidden.Num() == 1 && Recorder->HudHidden[0]);
			Director->FinishBossCinematic();
			TestTrue(TEXT("HUD back"), Recorder->HudHidden.Num() == 2 && !Recorder->HudHidden[1]);
			TestTrue(TEXT("finished: Phase"), Recorder->Finished.Num() == 1 && Recorder->Finished[0] == EBossCinematicKind::Phase);
			Orchestrator->ForceEndCombat(ECombatState::Victory);
		});

		It("never plays one for a normal enemy", [this]()
		{
			UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
			if (!TestTrue(TEXT("setup"), Director && Hero && Foe))
			{
				return;
			}
			Director->BossCinematicPreviewSeconds = 1.0f;
			Orchestrator->StartCombat({Hero}, {Foe});
			TestFalse(TEXT("no opening"), Director->IsBossCinematicPlaying());
			TestFalse(TEXT("no preview either"), Director->PreviewBossCinematic(EBossCinematicKind::Phase, 0));
			Orchestrator->ForceEndCombat(ECombatState::Victory);
		});
	});

	Describe("the boss cinematic skip (B-406b)", [this]()
	{
		It("skips an opening: the HUD comes back and turn 1 starts", [this]()
		{
			UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
			if (!TestTrue(TEXT("setup"), Director && Hero && Foe))
			{
				return;
			}
			TestFalse(TEXT("nothing to skip outside a fight"), ACombatPlayerController::SkipBossCinematic(TestWorld->GetWorld()));
			MakeFoeABoss();
			Director->BossCinematicPreviewSeconds = 1.0f; // the sequence-free hold: a headless world has no viewer
			UBossCinematicRecorder *Recorder = NewObject<UBossCinematicRecorder>();
			Recorder->Listen(Director);
			Orchestrator->StartCombat({Hero}, {Foe});
			TestNull(TEXT("turn 1 waits for the opening"), Director->GetViewedActor());

			TestTrue(TEXT("skipped"), ACombatPlayerController::SkipBossCinematic(TestWorld->GetWorld()));
			TestFalse(TEXT("ended"), Director->IsBossCinematicPlaying());
			TestTrue(TEXT("HUD back"), Recorder->HudHidden.Num() == 2 && !Recorder->HudHidden[1]);
			TestTrue(TEXT("finished: Opening"), Recorder->Finished.Num() == 1 && Recorder->Finished[0] == EBossCinematicKind::Opening);
			TestNotNull(TEXT("turn 1 started"), Director->GetViewedActor());
			TestFalse(TEXT("the fight goes on"), Orchestrator->IsWaitingForAsyncAction());
			TestFalse(TEXT("nothing left to skip"), ACombatPlayerController::SkipBossCinematic(TestWorld->GetWorld()));
			Orchestrator->ForceEndCombat(ECombatState::Victory);
		});

		It("skips a phase cinematic: the held action request goes out", [this]()
		{
			UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
			if (!TestTrue(TEXT("setup"), Director && Hero && Foe))
			{
				return;
			}
			Orchestrator->StartCombat({Hero}, {Foe});
			const int32 FirstTurn = Orchestrator->GetCurrentTurnNumber();
			Director->HoldForBossCinematic(EBossCinematicKind::Phase, Foe, FCombatCameraRules::BossCinematicMaxSeconds);
			Orchestrator->OnActionCompleted();
			const int32 HeldTurn = Orchestrator->GetCurrentTurnNumber();
			if (!TestTrue(TEXT("the next turn started"), HeldTurn > FirstTurn))
			{
				return;
			}
			TestTrue(TEXT("its action request is held"), Orchestrator->IsWaitingForAsyncAction());

			TestTrue(TEXT("skipped"), ACombatPlayerController::SkipBossCinematic(TestWorld->GetWorld()));
			TestFalse(TEXT("ended"), Director->IsBossCinematicPlaying());
			TestFalse(TEXT("the held request went out"), Orchestrator->IsWaitingForAsyncAction());
			TestEqual(TEXT("on that same turn"), Orchestrator->GetCurrentTurnNumber(), HeldTurn);
			Orchestrator->ForceEndCombat(ECombatState::Victory);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
