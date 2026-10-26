// CombatCameraSpec.cpp
// Combat camera rules (B-156, Design/CombatCamera.md): the mode the viewer sees, the template a
// skill falls back to, and where each mode puts the camera.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Camera/CombatCameraDirector.h"
#include "Combat/Camera/CombatCameraComponent.h"

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
			const FTransform View = FCombatCameraRules::ComputeView(ECombatCameraMode::Turn, ETurnViewStyle::Tactical, CombatCameraTestFrame());
			TestTrue(TEXT("side-on"), FMath::Abs(View.GetLocation().Y) > FMath::Abs(View.GetLocation().X - 500.0f));
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
