// CombatCameraSpec.cpp
// Combat camera rules (B-156, Design/CombatCamera.md): the mode the viewer sees, the template a
// skill falls back to, and where each mode puts the camera. B-370: the cinematic window and the Defence view.
// B-398: the turn-view setting's layouts and its save. B-399: the menu-depth step and the menu driving it.
// B-400: the target view (target cut with several enemies), the highlight's cycling, and the director following it.

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
#include "Combat/Camera/CameraFramingChecks.h"
#include "Save/SaveSubsystem.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "UI/Combat/PieMenuButtonData.h"
#include "Loadout/LoadoutComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"
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

	// B-399: frames for the orchestrator's first turn to open the menu (as the command-menu specs wait).
	constexpr int32 MENU_CAMERA_TEST_SETTLE_FRAMES = 3;

	/** B-399: the command menu needs a loadout to open. */
	void MenuCameraTestAddLoadout(AActor *Actor)
	{
		ULoadoutComponent *Loadout = NewObject<ULoadoutComponent>(Actor);
		Actor->AddInstanceComponent(Loadout);
		Loadout->RegisterComponent();
	}

	// B-400: target heights (a person, a tall golem) and how many targets the picker offers.
	constexpr float TARGET_TEST_PERSON_HEIGHT = 180.0f;
	constexpr float TARGET_TEST_GOLEM_HEIGHT = 400.0f;
	constexpr int32 TARGET_TEST_SEVERAL = 3;
	constexpr int32 TARGET_TEST_ONE = 1;
	/** B-400: an ally beside the acting character (a heal's target). */
	const FVector TARGET_TEST_ALLY(0.0f, 250.0f, 0.0f);

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

	Describe("calm menus (B-399, study R2, R3)", [this]()
	{
		It("gives the base turn view at menu depth 0 and one step closer and lower at depth 1, aimed the same", [this]()
		{
			const FCombatCameraFrame Frame = TurnViewTestFrame(FVector::ZeroVector, {TURN_TEST_ONE_ENEMY});
			for (const ETurnViewStyle Style : {ETurnViewStyle::CentredWide, ETurnViewStyle::OverTheShoulder, ETurnViewStyle::Tactical})
			{
				const FString Name = UEnum::GetValueAsString(Style);
				const FTransform Base = FCombatCameraRules::ComputeTurnView(Style, Frame);
				const FTransform Main = FCombatCameraRules::ComputeTurnView(Style, Frame, CommandMenuRules::MAIN_MENU_DEPTH);
				const FTransform Sub = FCombatCameraRules::ComputeTurnView(Style, Frame, CommandMenuRules::SUBMENU_DEPTH);
				TestTrue(FString::Printf(TEXT("%s: depth 0 is the base view"), *Name), Main.Equals(Base));
				TestTrue(FString::Printf(TEXT("%s: ComputeView's turn passes the depth"), *Name),
						 Sub.Equals(FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, Style, Frame, CommandMenuRules::SUBMENU_DEPTH)));
				TestTrue(FString::Printf(TEXT("%s: closer to the character"), *Name),
						 FVector::Dist2D(Sub.GetLocation(), Frame.Focus) < FVector::Dist2D(Base.GetLocation(), Frame.Focus));
				TestTrue(FString::Printf(TEXT("%s: lower"), *Name), Sub.GetLocation().Z < Base.GetLocation().Z);
				const FVector2D BaseCentre = ProjectInTurnView(Base, Frame.EnemyCentre);
				const FVector2D SubCentre = ProjectInTurnView(Sub, Frame.EnemyCentre);
				TestTrue(FString::Printf(TEXT("%s: the enemy centre keeps its side of the frame, in the middle band"), *Name),
						 BaseCentre.X * SubCentre.X >= -TURN_TEST_LAYOUT_TOLERANCE && FMath::Abs(SubCentre.Y) <= CameraFraming::THIRD_LINE);
			}
			const FTransform WideSub = FCombatCameraRules::ComputeTurnView(ETurnViewStyle::CentredWide, Frame, CommandMenuRules::SUBMENU_DEPTH);
			TestTrue(TEXT("centred wide: still aimed at the enemy centre"),
					 ProjectInTurnView(WideSub, Frame.EnemyCentre).Equals(FVector2D::ZeroVector, TURN_TEST_LAYOUT_TOLERANCE));
		});

		It("keeps the over-the-shoulder layout one step in: hero lower-left, enemy centre mid-frame", [this]()
		{
			const FCombatCameraFrame Frame = TurnViewTestFrame(FVector::ZeroVector, TurnTestThreeEnemies());
			const FTransform Sub = FCombatCameraRules::ComputeTurnView(ETurnViewStyle::OverTheShoulder, Frame, CommandMenuRules::SUBMENU_DEPTH);
			const FVector2D Hero = ProjectInTurnView(Sub, Frame.Focus);
			const FVector2D Centre = ProjectInTurnView(Sub, Frame.EnemyCentre);
			const FString Where = FString::Printf(TEXT("hero %s, enemy centre %s"), *CameraFraming::GetThirdsCell(Hero), *CameraFraming::GetThirdsCell(Centre));
			TestTrue(FString::Printf(TEXT("hero on screen (%s)"), *Where),
					 CameraFraming::IsOnScreen(Sub, FCombatCameraRules::DefaultFieldOfView, CameraFraming::DEFAULT_ASPECT, Frame.Focus));
			TestTrue(FString::Printf(TEXT("hero lower-left (%s)"), *Where), Hero.X < -CameraFraming::THIRD_LINE && Hero.Y < -CameraFraming::THIRD_LINE);
			TestTrue(FString::Printf(TEXT("enemy centre mid-frame (%s)"), *Where), Centre.Equals(FVector2D::ZeroVector, TURN_TEST_LAYOUT_TOLERANCE));
		});

		It("holds the one step for deeper menus and reads a negative depth as the main menu", [this]()
		{
			const FCombatCameraFrame Frame = CombatCameraTestFrame();
			const FTransform Sub = FCombatCameraRules::ComputeTurnView(ETurnViewStyle::CentredWide, Frame, CommandMenuRules::SUBMENU_DEPTH);
			TestTrue(TEXT("deeper holds the step"),
					 Sub.Equals(FCombatCameraRules::ComputeTurnView(ETurnViewStyle::CentredWide, Frame, FCombatCameraRules::MaxMenuDepth + 1)));
			TestTrue(TEXT("negative is the base"),
					 FCombatCameraRules::ComputeTurnView(ETurnViewStyle::CentredWide, Frame, -1).Equals(FCombatCameraRules::ComputeTurnView(ETurnViewStyle::CentredWide, Frame)));
		});

		It("follows a sub-menu, and keeps a target picker at the depth it was opened from (Q-399)", [this]()
		{
			using namespace CommandMenuRules;
			TestEqual(TEXT("closed"), CameraDepthFor(ECombatMenuDepth::Closed, ECombatMenuDepth::Main), MAIN_MENU_DEPTH);
			TestEqual(TEXT("main menu"), CameraDepthFor(ECombatMenuDepth::Main, ECombatMenuDepth::Main), MAIN_MENU_DEPTH);
			TestEqual(TEXT("a sub-menu"), CameraDepthFor(ECombatMenuDepth::Submenu, ECombatMenuDepth::Main), SUBMENU_DEPTH);
			TestEqual(TEXT("Attack's target picker"), CameraDepthFor(ECombatMenuDepth::TargetSelection, ECombatMenuDepth::Main), MAIN_MENU_DEPTH);
			TestEqual(TEXT("a skill's target picker"), CameraDepthFor(ECombatMenuDepth::TargetSelection, ECombatMenuDepth::Submenu), SUBMENU_DEPTH);
			TestEqual(TEXT("a skill's Allies / Enemies step"), CameraDepthFor(ECombatMenuDepth::TargetCategory, ECombatMenuDepth::Submenu), SUBMENU_DEPTH);
		});
	});

	Describe("target cut with several enemies (B-400, study R3 / §9, Q-400)", [this]()
	{
		It("keeps each target's feet, head and HP bar on screen, in every style, for several positions and sizes", [this]()
		{
			const TArray<FVector> Enemies = TurnTestThreeEnemies();
			const FCombatCameraFrame Frames[] = {TurnViewTestFrame(FVector::ZeroVector, Enemies), TurnViewTestFrame(TURN_TEST_SIDE_MEMBER, Enemies)};
			TArray<FVector> Targets = Enemies;
			Targets.Add(TARGET_TEST_ALLY);
			for (const FCombatCameraFrame &Frame : Frames)
			{
				for (const ETurnViewStyle Style : {ETurnViewStyle::CentredWide, ETurnViewStyle::OverTheShoulder, ETurnViewStyle::Tactical})
				{
					for (const FVector &Feet : Targets)
					{
						for (const float Height : {TARGET_TEST_PERSON_HEIGHT, TARGET_TEST_GOLEM_HEIGHT})
						{
							const FTransform View = FCombatCameraRules::ComputeTargetView(Style, Frame, Feet, Height, CommandMenuRules::MAIN_MENU_DEPTH, TARGET_TEST_SEVERAL);
							const FVector Head = Feet + FVector::UpVector * Height;
							const FVector Bar = FCombatCameraRules::GetTargetHealthBarPoint(Feet, Height);
							const FString Name = FString::Printf(TEXT("%s, target %s, %.0f cm"), *UEnum::GetValueAsString(Style), *Feet.ToString(), Height);
							for (const FVector &Point : {Feet, Head, Bar})
							{
								TestTrue(FString::Printf(TEXT("%s: %s on screen"), *Name, *CameraFraming::GetThirdsCell(ProjectInTurnView(View, Point))),
										 CameraFraming::IsOnScreen(View, FCombatCameraRules::DefaultFieldOfView, CameraFraming::DEFAULT_ASPECT, Point));
							}
						}
					}
				}
			}
		});

		It("gives the turn view itself for one target, in every style and at either menu depth", [this]()
		{
			const FCombatCameraFrame Frame = TurnViewTestFrame(FVector::ZeroVector, {TURN_TEST_ONE_ENEMY});
			for (const ETurnViewStyle Style : {ETurnViewStyle::CentredWide, ETurnViewStyle::OverTheShoulder, ETurnViewStyle::Tactical})
			{
				for (const int32 Depth : {CommandMenuRules::MAIN_MENU_DEPTH, CommandMenuRules::SUBMENU_DEPTH})
				{
					const FTransform View = FCombatCameraRules::ComputeTargetView(Style, Frame, TURN_TEST_ONE_ENEMY, TARGET_TEST_PERSON_HEIGHT, Depth, TARGET_TEST_ONE);
					TestTrue(FString::Printf(TEXT("%s depth %d: no move"), *UEnum::GetValueAsString(Style), Depth),
							 View.Equals(FCombatCameraRules::ComputeTurnView(Style, Frame, Depth)));
				}
			}
		});

		It("over the shoulder: a front medium shot, the target centred and larger than in the turn view; a cut", [this]()
		{
			const TArray<FVector> Enemies = TurnTestThreeEnemies();
			const FCombatCameraFrame Frame = TurnViewTestFrame(FVector::ZeroVector, Enemies);
			const FTransform Turn = FCombatCameraRules::ComputeTurnView(ETurnViewStyle::OverTheShoulder, Frame);
			for (const FVector &Feet : Enemies)
			{
				const FTransform View = FCombatCameraRules::ComputeTargetView(ETurnViewStyle::OverTheShoulder, Frame, Feet, TARGET_TEST_PERSON_HEIGHT,
																			  CommandMenuRules::MAIN_MENU_DEPTH, TARGET_TEST_SEVERAL);
				const FVector Head = Feet + FVector::UpVector * TARGET_TEST_PERSON_HEIGHT;
				const float ShotHeight = ProjectInTurnView(View, Head).Y - ProjectInTurnView(View, Feet).Y;
				const float TurnHeight = ProjectInTurnView(Turn, Head).Y - ProjectInTurnView(Turn, Feet).Y;
				TestTrue(TEXT("larger than in the turn view"), ShotHeight > TurnHeight);
				TestTrue(TEXT("centred left to right"), FMath::Abs(ProjectInTurnView(View, Feet).X) <= CameraFraming::THIRD_LINE);
				TestTrue(TEXT("from the party's side (front)"), View.GetLocation().X < Feet.X);
			}
			TestEqual(TEXT("a cut"), FCombatCameraRules::GetTargetViewBlendSeconds(ETurnViewStyle::OverTheShoulder), 0.0f);
		});

		It("centred wide: the same place, re-aimed toward the target; a short blend", [this]()
		{
			const TArray<FVector> Enemies = TurnTestThreeEnemies();
			const FCombatCameraFrame Frame = TurnViewTestFrame(FVector::ZeroVector, Enemies);
			const FTransform Turn = FCombatCameraRules::ComputeTurnView(ETurnViewStyle::CentredWide, Frame);
			const FVector Side = Enemies[0];
			const FTransform View = FCombatCameraRules::ComputeTargetView(ETurnViewStyle::CentredWide, Frame, Side, TARGET_TEST_PERSON_HEIGHT,
																		  CommandMenuRules::MAIN_MENU_DEPTH, TARGET_TEST_SEVERAL);
			TestTrue(TEXT("same place"), View.GetLocation().Equals(Turn.GetLocation(), TURN_TEST_LOCATION_TOLERANCE));
			TestTrue(TEXT("the target nearer the middle"), FMath::Abs(ProjectInTurnView(View, Side).X) < FMath::Abs(ProjectInTurnView(Turn, Side).X));
			TestTrue(TEXT("a short blend"), FCombatCameraRules::GetTargetViewBlendSeconds(ETurnViewStyle::CentredWide) > 0.0f);
		});

		It("cycles the highlight forward and back, wrapping at both ends", [this]()
		{
			using CommandMenuRules::CycleIndex;
			TestEqual(TEXT("next"), CycleIndex(0, 1, TARGET_TEST_SEVERAL), 1);
			TestEqual(TEXT("wraps forward"), CycleIndex(TARGET_TEST_SEVERAL - 1, 1, TARGET_TEST_SEVERAL), 0);
			TestEqual(TEXT("wraps back"), CycleIndex(0, -1, TARGET_TEST_SEVERAL), TARGET_TEST_SEVERAL - 1);
			TestEqual(TEXT("a big step is one step"), CycleIndex(0, TARGET_TEST_SEVERAL + 1, TARGET_TEST_SEVERAL), 1);
			TestEqual(TEXT("no step"), CycleIndex(1, 0, TARGET_TEST_SEVERAL), 1);
			TestEqual(TEXT("out of range: the first"), CycleIndex(INDEX_NONE, 1, TARGET_TEST_SEVERAL), 0);
			TestEqual(TEXT("one target stays"), CycleIndex(0, 1, TARGET_TEST_ONE), 0);
			TestEqual(TEXT("none"), CycleIndex(0, 1, 0), INDEX_NONE);
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
			TestTrue(TEXT("the test shot has a shake to drop"), Punch.ShakeScale > 0.0f);
			const FCameraShot Free = FCombatCameraRules::SteadyForDefence(Punch, false);
			TestEqual(TEXT("not defending: the shake stays"), Free.ShakeScale, Punch.ShakeScale);
			TestEqual(TEXT("not defending: the punch stays"), Free.FieldOfView, Punch.FieldOfView);

			// B-403 review: an enemy's melee rush seen while defending has no blur (so the director doesn't cut to it).
			Context.bRanged = false;
			Context.bAllowRush = true;
			const FCameraShot Rush = FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Context, FSkillCameraTuning());
			TestTrue(TEXT("the test rush has blur to drop"), Rush.MotionBlur > 0.0f);
			TestEqual(TEXT("defending: no rush blur"), FCombatCameraRules::SteadyForDefence(Rush, true).MotionBlur, 0.0f);
			TestEqual(TEXT("not defending: the rush blur stays"), FCombatCameraRules::SteadyForDefence(Rush, false).MotionBlur, Rush.MotionBlur);
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

		It("rushes in fast with motion blur on a melee wind-up; a spell keeps the slow push (B-403, R5)", [this, Context]()
		{
			const FCameraShot Melee = FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Context(), FSkillCameraTuning());
			FCameraShotContext SpellContext = Context();
			SpellContext.bRanged = true;
			const FCameraShot Spell = FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, SpellContext, FSkillCameraTuning());
			TestTrue(TEXT("melee: a short rush (0.3 s or less)"), Melee.BlendSeconds > 0.0f && Melee.BlendSeconds <= 0.3f);
			TestTrue(TEXT("melee: motion blur"), Melee.MotionBlur > 0.0f);
			TestTrue(TEXT("melee: moves toward the target"), Melee.End.GetLocation().X - Melee.Start.GetLocation().X > 0.0f);
			TestTrue(TEXT("spell: slower than the rush"), Spell.BlendSeconds > Melee.BlendSeconds);
			TestEqual(TEXT("spell: no blur"), Spell.MotionBlur, 0.0f);
			FCameraShotContext Calm = Context();
			Calm.bAllowRush = false;
			const FCameraShot Still = FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Calm, FSkillCameraTuning());
			TestEqual(TEXT("movement off: no blur"), Still.MotionBlur, 0.0f);
			TestTrue(TEXT("movement off: the slow push"), Still.BlendSeconds > Melee.BlendSeconds);
		});

		It("frames caster and target side-on on impact, low, with a small FOV punch and a light shake, larger on a crit (B-403, R6)", [this, Context]()
		{
			const FCameraShot Hit = FCombatCameraRules::ComputeShot(ECameraShotMoment::Impact, Context(), FSkillCameraTuning());
			const FVector Caster = FVector::ZeroVector;
			const FVector Target(1000.0f, 0.0f, 0.0f);
			const FVector Look = Hit.End.GetRotation().GetForwardVector();
			TestTrue(TEXT("side-on: looks across the caster-target line"), FMath::Abs(Look.X) < 0.5f);
			TestTrue(TEXT("three-quarter front: on the caster's side of the middle"), Hit.End.GetLocation().X < 500.0f);
			TestTrue(TEXT("low"), Hit.End.GetLocation().Z < 100.0f);
			TestTrue(TEXT("caster on screen"), CameraFraming::IsOnScreen(Hit.End, Hit.FieldOfView, CameraFraming::DEFAULT_ASPECT, Caster));
			TestTrue(TEXT("target on screen"), CameraFraming::IsOnScreen(Hit.End, Hit.FieldOfView, CameraFraming::DEFAULT_ASPECT, Target));
			FCameraShotContext Close = Context({FVector(150.0f, 0.0f, 0.0f)});
			const FCameraShot Melee = FCombatCameraRules::ComputeShot(ECameraShotMoment::Impact, Close, FSkillCameraTuning());
			TestTrue(TEXT("melee distance: both on screen"),
					 CameraFraming::IsOnScreen(Melee.End, Melee.FieldOfView, CameraFraming::DEFAULT_ASPECT, Caster)
						 && CameraFraming::IsOnScreen(Melee.End, Melee.FieldOfView, CameraFraming::DEFAULT_ASPECT, FVector(150.0f, 0.0f, 0.0f)));
			const float Punch = FCombatCameraRules::DefaultFieldOfView - Hit.FieldOfView;
			TestTrue(TEXT("a small FOV punch (10 degrees or less)"), Punch > 0.0f && Punch <= 10.0f);
			TestTrue(TEXT("no big shake on a normal hit"), Hit.ShakeScale <= 0.5f);
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

		It("holds a spread still for the whole effect: no move after the pull-out, no shake, no punch (B-403, R8)", [this, Context]()
		{
			const TArray<FVector> Targets = {FVector(1000.0f, -600.0f, 0.0f), FVector(1000.0f, 600.0f, 0.0f)};
			FCameraShotContext CritContext = Context(Targets);
			CritContext.bCritical = true;
			const FCameraShot Spread = FCombatCameraRules::ComputeShot(ECameraShotMoment::Spread, CritContext, FSkillCameraTuning());
			TestTrue(TEXT("one transform: nothing to blend"), Spread.Start.Equals(Spread.End));
			TestTrue(TEXT("held long after its blend"), FCombatCameraRules::BlendShot(Spread, 10.0f).Equals(Spread.End));
			TestEqual(TEXT("no shake, even on a crit"), Spread.ShakeScale, 0.0f);
			TestEqual(TEXT("no FOV punch"), Spread.FieldOfView, FCombatCameraRules::DefaultFieldOfView);
			for (const FVector &Target : Targets)
			{
				TestTrue(TEXT("every target on screen"), CameraFraming::IsOnScreen(Spread.End, Spread.FieldOfView, CameraFraming::DEFAULT_ASPECT, Target));
			}
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

	It("moves the camera once per menu-depth change and never while browsing one depth (B-399)", [this]()
	{
		UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
		UCombatCommandMenuSubsystem *Menu = TestWorld->GetSubsystem<UCombatCommandMenuSubsystem>();
		// The turn view needs a viewer; a headless controller has no local player, so no HUD is built for it.
		APlayerController *Viewer = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<APlayerController>() : nullptr;
		if (!TestTrue(TEXT("setup"), Director && Menu && Viewer && Hero && Foe))
		{
			return;
		}
		Director->SetViewerForTests(Viewer); // dev's viewer is the local player only (B-405 review)
		MenuCameraTestAddLoadout(Hero);
		MenuCameraTestAddLoadout(Foe);
		Menu->SetCombatOrchestrator(Orchestrator);
		Orchestrator->StartCombat({Hero}, {Foe});
		TestWorld->Tick(MENU_CAMERA_TEST_SETTLE_FRAMES);
		ON_SCOPE_EXIT
		{
			Menu->ClearCombatOrchestrator();
			// The bare controller is not a local player: restoring its own view mid-blend (EndCombat) makes the engine's
			// camera manager call ClientSetViewTarget, which runs locally with no connection and recurses (stack overflow).
			Director->SetViewerForTests(nullptr);
			if (Orchestrator->GetCombatState() != ECombatState::Idle)
			{
				Orchestrator->ForceEndCombat(ECombatState::Victory);
			}
		};
		if (!TestTrue(TEXT("the hero's menu is open on the hero's turn view"), Menu->IsOpen() && Menu->GetCurrentActor() == Hero &&
																				   Director->GetViewedActor() == Hero &&
																				   Director->GetCurrentMode() == ECombatCameraMode::Turn))
		{
			return;
		}
		TestEqual(TEXT("main menu: depth 0"), Director->GetMenuDepth(), CommandMenuRules::MAIN_MENU_DEPTH);
		TestEqual(TEXT("main menu: no menu move"), Director->GetMenuStepCount(), 0);

		FPieMenuButtonData Abilities;
		Abilities.Category = EPieMenuCategory::Abilities;
		FPieMenuButtonData Items;
		Items.Category = EPieMenuCategory::Items;
		Menu->HandleSelection(Abilities);
		TestEqual(TEXT("a sub-menu: depth 1"), Director->GetMenuDepth(), CommandMenuRules::SUBMENU_DEPTH);
		TestEqual(TEXT("a sub-menu: one move"), Director->GetMenuStepCount(), 1);

		Menu->HandleSelection(Items); // another sub-menu: browsing at the same depth
		TestWorld->Tick(MENU_CAMERA_TEST_SETTLE_FRAMES);
		TestEqual(TEXT("browsing: no further move"), Director->GetMenuStepCount(), 1);

		Menu->HandleBack();
		TestEqual(TEXT("cancel: depth 0"), Director->GetMenuDepth(), CommandMenuRules::MAIN_MENU_DEPTH);
		TestEqual(TEXT("cancel: one move back"), Director->GetMenuStepCount(), 2);

		Director->SetCameraMovementOff(true);
		Menu->HandleSelection(Abilities);
		TestEqual(TEXT("movement off: the depth is kept"), Director->GetMenuDepth(), CommandMenuRules::SUBMENU_DEPTH);
		TestEqual(TEXT("movement off: the static view holds (R15)"), Director->GetMenuStepCount(), 2);
		Director->SetCameraMovementOff(false);
	});

	It("cuts to each highlighted target with two enemies, back on cancel, and holds with movement off (B-400)", [this]()
	{
		UCombatCameraDirector *Director = Orchestrator ? Orchestrator->GetCameraDirector() : nullptr;
		UCombatCommandMenuSubsystem *Menu = TestWorld->GetSubsystem<UCombatCommandMenuSubsystem>();
		APlayerController *Viewer = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<APlayerController>() : nullptr;
		AActor *SecondFoe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("setup"), Director && Menu && Viewer && Hero && Foe && SecondFoe))
		{
			return;
		}
		for (AActor *Actor : {Hero, Foe, SecondFoe})
		{
			MenuCameraTestAddLoadout(Actor);
		}
		Menu->SetCombatOrchestrator(Orchestrator);
		Orchestrator->StartCombat({Hero}, {Foe, SecondFoe});
		TestWorld->Tick(MENU_CAMERA_TEST_SETTLE_FRAMES);
		ON_SCOPE_EXIT
		{
			Menu->ClearCombatOrchestrator();
			if (Orchestrator->GetCombatState() != ECombatState::Idle)
			{
				Orchestrator->ForceEndCombat(ECombatState::Victory);
			}
		};
		if (!TestTrue(TEXT("the hero's menu is open on the hero's turn view"), Menu->IsOpen() && Menu->GetCurrentActor() == Hero &&
																				   Director->GetViewedActor() == Hero &&
																				   Director->GetCurrentMode() == ECombatCameraMode::Turn))
		{
			return;
		}
		FPieMenuButtonData Attack;
		Attack.Category = EPieMenuCategory::Attack;
		Menu->HandleSelection(Attack);
		if (!TestEqual(TEXT("the picker offers both enemies"), Menu->GetHighlightTargetCount(), 2))
		{
			return;
		}
		AActor *First = Menu->GetHighlightedTarget();
		TestNotNull(TEXT("the first target is highlighted on open"), First);
		TestTrue(TEXT("the director follows it"), Director->GetHighlightedTarget() == First && Director->IsTargetViewShowing());
		TestEqual(TEXT("one move"), Director->GetTargetMoveCount(), 1);

		Menu->CycleHighlightedTarget(1);
		AActor *Second = Menu->GetHighlightedTarget();
		TestTrue(TEXT("cycling moves to the other enemy"), Second && Second != First && Director->GetHighlightedTarget() == Second);
		TestEqual(TEXT("one move per press"), Director->GetTargetMoveCount(), 2);
		Menu->CycleHighlightedTarget(1);
		TestTrue(TEXT("and wraps"), Menu->GetHighlightedTarget() == First);
		Menu->HighlightTarget(First);
		TestEqual(TEXT("hovering the highlighted target: no move"), Director->GetTargetMoveCount(), 3);

		Menu->HandleBack();
		TestNull(TEXT("cancel clears the highlight"), Menu->GetHighlightedTarget());
		TestFalse(TEXT("cancel: back to the turn view"), Director->IsTargetViewShowing());
		TestEqual(TEXT("cancel: one move back"), Director->GetTargetMoveCount(), 4);
		TestEqual(TEXT("the menu depth is untouched (Q-399)"), Director->GetMenuDepth(), CommandMenuRules::MAIN_MENU_DEPTH);

		Director->SetCameraMovementOff(true);
		Menu->HandleSelection(Attack);
		Menu->CycleHighlightedTarget(1);
		TestFalse(TEXT("movement off: no target view"), Director->IsTargetViewShowing());
		TestEqual(TEXT("movement off: the static view holds (R15)"), Director->GetTargetMoveCount(), 4);
		Menu->HandleBack();
		Director->SetCameraMovementOff(false);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
