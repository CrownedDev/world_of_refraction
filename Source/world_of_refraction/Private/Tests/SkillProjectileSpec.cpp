// SkillProjectileSpec.cpp
// B-350 (D-140): what the manual ASkillProjectileTestActor exercised, as a headless spec. A projectile launched with
// no spell data flies at the default speed to the target's cast-time location and reports an impact with its damage;
// a target that moved out of the impact radius reports a dodge instead; nothing moves before Launch.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "ProjectileEventRecorder.h"
#include "Combat/Projectile/SkillProjectile.h"
#include "Components/SceneComponent.h"

namespace
{
	constexpr float PROJECTILE_TEST_RANGE = 1000.0f;
	constexpr float PROJECTILE_TEST_DODGE_STEP = 500.0f;
	constexpr float PROJECTILE_TEST_IMPACT_RADIUS = 1.0f; // x100 units in the dodge check
	constexpr float PROJECTILE_TEST_VISUAL_SCALE = 1.0f;
	constexpr int32 PROJECTILE_TEST_DAMAGE = 50;
	/** Test-world frames are 0.01 s. 1000 units at the default 1500 units/s (arrival within 50 units) take about
	 *  64 frames; this budget is several times that. */
	constexpr int32 PROJECTILE_TEST_MAX_FRAMES = 300;
	/** A few frames: enough to see that an unlaunched projectile stays put. */
	constexpr int32 PROJECTILE_TEST_IDLE_FRAMES = 5;
}

BEGIN_DEFINE_SPEC(FSkillProjectileSpec, "WoR.Combat.SkillProjectile", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *Caster = nullptr;
	AActor *Target = nullptr;
	ASkillProjectile *Projectile = nullptr;
	UProjectileEventRecorder *Recorder = nullptr;

	/** A bare actor with a scene root, so it has a location. */
	AActor *SpawnAt(const FVector &Location)
	{
		AActor *Actor = TestWorld->GetWorld()->SpawnActor<AActor>();
		if (Actor)
		{
			USceneComponent *Root = NewObject<USceneComponent>(Actor);
			Actor->SetRootComponent(Root);
			Root->RegisterComponent();
			Actor->SetActorLocation(Location);
		}
		return Actor;
	}

	/** Ticks the world until the projectile resolves (impact or dodge) or the frame budget runs out. */
	void FlyToTarget()
	{
		for (int32 Frame = 0; Frame < PROJECTILE_TEST_MAX_FRAMES && !Projectile->HasImpacted(); ++Frame)
		{
			TestWorld->Tick();
		}
	}
END_DEFINE_SPEC(FSkillProjectileSpec)

void FSkillProjectileSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		UWorld *World = TestWorld->GetWorld();
		if (!World) return;
		Caster = SpawnAt(FVector::ZeroVector);
		Target = SpawnAt(FVector(PROJECTILE_TEST_RANGE, 0.0f, 0.0f));
		Projectile = World->SpawnActor<ASkillProjectile>();
		Recorder = NewObject<UProjectileEventRecorder>(GetTransientPackage());
		if (Recorder)
		{
			Recorder->AddToRoot(); // kept alive across the world ticks; released in AfterEach
		}
		if (!TestTrue(TEXT("Spawned"), Caster && Target && Projectile && Recorder)) return;
		Projectile->InitializeProjectile(nullptr, Caster, Target, PROJECTILE_TEST_IMPACT_RADIUS, PROJECTILE_TEST_VISUAL_SCALE,
										 PROJECTILE_TEST_DAMAGE, ESpellElement::None);
		Recorder->Listen(Projectile);
	});

	AfterEach([this]()
	{
		if (Recorder)
		{
			Recorder->RemoveFromRoot();
		}
		Caster = Target = nullptr;
		Projectile = nullptr;
		Recorder = nullptr;
		TestWorld.Reset();
	});

	It("starts at the caster, as a projectile, aimed at the target", [this]()
	{
		if (!Projectile) return;
		TestEqual(TEXT("delivery"), Projectile->GetDeliveryType(), ESpellDeliveryType::Projectile);
		TestTrue(TEXT("at the caster"), Projectile->GetActorLocation().Equals(Caster->GetActorLocation()));
		TestEqual(TEXT("target"), Projectile->GetTarget(), Target);
		TestEqual(TEXT("caster"), Projectile->GetCaster(), Caster);
	});

	It("does not move before Launch", [this]()
	{
		if (!Projectile) return;
		TestWorld->Tick(PROJECTILE_TEST_IDLE_FRAMES);
		TestFalse(TEXT("no impact"), Projectile->HasImpacted());
		TestTrue(TEXT("still at the caster"), Projectile->GetActorLocation().Equals(Caster->GetActorLocation()));
	});

	It("reaches a target that stayed put and reports an impact with its damage", [this]()
	{
		if (!Projectile) return;
		Projectile->Launch();
		FlyToTarget();
		TestTrue(TEXT("resolved"), Projectile->HasImpacted());
		TestEqual(TEXT("one impact"), Recorder->Impacts, 1);
		TestEqual(TEXT("no dodge"), Recorder->Dodges, 0);
		TestEqual(TEXT("its damage"), Recorder->LastDamage, PROJECTILE_TEST_DAMAGE);
		TestEqual(TEXT("on the target"), Recorder->LastTarget, Target);
	});

	It("reports a dodge when the target moved out of the impact radius", [this]()
	{
		if (!Projectile) return;
		Projectile->Launch();
		Target->SetActorLocation(FVector(PROJECTILE_TEST_RANGE, PROJECTILE_TEST_DODGE_STEP, 0.0f));
		FlyToTarget();
		TestTrue(TEXT("resolved"), Projectile->HasImpacted());
		TestEqual(TEXT("one dodge"), Recorder->Dodges, 1);
		TestEqual(TEXT("no impact"), Recorder->Impacts, 0);
	});
}

#endif
