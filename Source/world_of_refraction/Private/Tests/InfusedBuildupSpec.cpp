// InfusedBuildupSpec.cpp
// B-113 / Q-113: an infused hit's status buildup is authored × charge band × (1 + StatFraction(M))
// in the executor, then × M again in AddStatusBuildup. That second use is the documented infusion
// design (InfusionSystem.md "stat-scaled" bands, like damage and cost), kept by Q-113 — these
// cases pin both layers so a change to either is a deliberate one.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Infusion/EInfusionMode.h"
#include "Skills/Definitions/EScalingTier.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	constexpr int32 INFUSED_TEST_EFFECT_ID = 113000;
	constexpr int32 INFUSED_TEST_DURATION_TURNS = 3;
	constexpr int32 INFUSED_TEST_CHARGE_LEVEL = 1;
	constexpr float INFUSED_TEST_STATUS_BUFF_PERCENT = 50.0f;
	constexpr float INFUSED_TEST_BUILDUP = 20.0f;
	constexpr float INFUSED_TEST_TOLERANCE = 1.e-3f;
}

BEGIN_DEFINE_SPEC(FInfusedBuildupSpec, "WoR.Status.InfusedBuildup",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UStatusBuildupManager *Buildup = nullptr;
	UActionExecutor *Executor = nullptr;

	AActor *Spawn()
	{
		return TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	}

	static UCharacterDataComponent *CharOf(AActor *Actor)
	{
		return Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	}

	void BuffStatusMultiplier(AActor *Actor)
	{
		FActiveSkillEffect Effect = FActiveSkillEffect::CreateBuff(TEXT("Infused buildup test"), INFUSED_TEST_EFFECT_ID,
			ESkillEffectType::StatusMultiplierBuff, INFUSED_TEST_STATUS_BUFF_PERCENT, INFUSED_TEST_DURATION_TURNS);
		EffectManager->ApplyEffect(Actor, Effect);
	}

	/** Bar filled on a fresh target by INFUSED_TEST_BUILDUP from Source (no element, no physical type resolves no trigger). */
	float BuildupFrom(AActor *Source)
	{
		AActor *Target = Spawn();
		Buildup->AddStatusBuildup(Source, Target, INFUSED_TEST_BUILDUP, ESpellElement::None, EPhysicalDamageType::None);
		return Buildup->GetStatusBarBuildup(Target);
	}
END_DEFINE_SPEC(FInfusedBuildupSpec)

void FInfusedBuildupSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Buildup = TestWorld->GetSubsystem<UStatusBuildupManager>();
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		TestNotNull(TEXT("SkillEffectManager"), EffectManager);
		TestNotNull(TEXT("StatusBuildupManager"), Buildup);
		TestNotNull(TEXT("ActionExecutor"), Executor);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
		EffectManager = nullptr;
		Buildup = nullptr;
		Executor = nullptr;
	});

	It("scales the infusion charge band by the source's Status Multiplier surcharge", [this]()
	{
		if (!EffectManager || !Executor) return;
		AActor *Source = Spawn();
		UCharacterDataComponent *Comp = CharOf(Source);
		if (!TestNotNull(TEXT("Source"), Comp)) return;

		const float BareMult = Comp->GetEffectiveStatusMultiplier();
		const float BareCharge = Executor->GetChargeStatusMultiplier(INFUSED_TEST_CHARGE_LEVEL, EInfusionMode::Status, Comp);
		BuffStatusMultiplier(Source);
		const float BuffedMult = Comp->GetEffectiveStatusMultiplier();
		if (!TestTrue(FString::Printf(TEXT("Buff raised Status Multiplier (%f -> %f)"), BareMult, BuffedMult), BuffedMult > BareMult)) return;

		const float ExpectedRatio = (1.0f + StatFraction(BuffedMult)) / (1.0f + StatFraction(BareMult));
		TestEqual(TEXT("Charge band grows by the stat surcharge"),
				  Executor->GetChargeStatusMultiplier(INFUSED_TEST_CHARGE_LEVEL, EInfusionMode::Status, Comp) / BareCharge,
				  ExpectedRatio, INFUSED_TEST_TOLERANCE);
	});

	It("applies the source's Status Multiplier again when the buildup lands (Q-113, by design)", [this]()
	{
		if (!EffectManager || !Buildup) return;
		AActor *Source = Spawn();
		UCharacterDataComponent *Comp = CharOf(Source);
		if (!TestNotNull(TEXT("Source"), Comp)) return;

		const float BareMult = Comp->GetEffectiveStatusMultiplier();
		const float BareBuildup = BuildupFrom(Source);
		if (!TestTrue(TEXT("Bare buildup lands"), BareBuildup > 0.0f)) return;
		BuffStatusMultiplier(Source);
		const float BuffedMult = Comp->GetEffectiveStatusMultiplier();

		TestEqual(TEXT("Landed buildup scales by the Status Multiplier"),
				  BuildupFrom(Source) / BareBuildup, BuffedMult / BareMult, INFUSED_TEST_TOLERANCE);
	});
}

#endif
