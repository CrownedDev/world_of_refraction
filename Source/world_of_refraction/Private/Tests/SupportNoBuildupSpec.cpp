// SupportNoBuildupSpec.cpp
// Rules page 03 (Crown, 2026-10-26; B-174): heals, buffs and support skills never build status,
// on anyone. Unit cases for SupportSkillRules, then a real fight through ACombatOrchestrator.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatOrchestrator.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/SupportSkillRules.h"
#include "Skills/Effects/EffectDefinition.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	constexpr int32 SUPPORT_TEST_BUILDUP = 40;
	constexpr int32 SUPPORT_TEST_DAMAGE = 10;
	constexpr float SUPPORT_TEST_HEAL = 20.0f;
	/** Test-world ticks are 0.01s; spans the executor's 10s timeout, which covers the defense
	 *  window's 8s failsafe a montage-less ability waits on. */
	constexpr int32 SUPPORT_SETTLE_FRAMES = 1100;

	UEffectDefinition *SupportHealEffect()
	{
		UEffectDefinition *Def = NewObject<UEffectDefinition>(GetTransientPackage());
		FSkillEffectPayload Heal;
		Heal.EffectType = ESkillEffectType::HealthRestore;
		Heal.Magnitude = SUPPORT_TEST_HEAL;
		FSkillEffect Effect;
		Effect.Payloads.Add(Heal);
		Def->Effects.Add(Effect);
		return Def;
	}

	UAbilityData *SupportTestAbility(int32 BaseDamage, bool bWithHeal)
	{
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = TEXT("Support Test Ability");
		Ability->BaseDamage = BaseDamage;
		Ability->StatusBuildup = SUPPORT_TEST_BUILDUP;
		if (bWithHeal)
		{
			Ability->ReferencedEffects.Add(SupportHealEffect());
		}
		return Ability;
	}

	USpellData *SupportTestSpell(ESpellSchool School)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Name = TEXT("Support Test Spell");
		Spell->School = School;
		Spell->BaseDamage = SUPPORT_TEST_DAMAGE;
		Spell->StatusBuildup = SUPPORT_TEST_BUILDUP;
		return Spell;
	}
}

BEGIN_DEFINE_SPEC(FSupportNoBuildupSpec, "WoR.Skills.SupportNoBuildup",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UStatusBuildupManager *BuildupManager = nullptr;
	AActor *Hero = nullptr;
	AActor *Ally = nullptr;
	AActor *Foe = nullptr;

	/** Hero and Ally against Foe; returns the local-party member whose turn it is (nullptr on failure). */
	AActor *StartFight()
	{
		Orchestrator = TestWorld->GetWorld() ? TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>() : nullptr;
		BuildupManager = TestWorld->GetSubsystem<UStatusBuildupManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Ally = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestTrue(TEXT("Fight set up"), Orchestrator && BuildupManager && Hero && Ally && Foe))
		{
			return nullptr;
		}
		Orchestrator->StartCombat({Hero, Ally}, {Foe});
		AActor *Current = Orchestrator->GetCurrentActor();
		return TestTrue(TEXT("A local-party member acts first"), Current == Hero || Current == Ally) ? Current : nullptr;
	}

	/** Uses Ability from the acting member on Target and lets it resolve. */
	void UseAbility(UAbilityData *Ability, AActor *Target, ETargetType TargetType)
	{
		Ability->TargetType = TargetType;
		Ability->AddToRoot();
		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Ability;
		Action.Targets.Add(Target);
		Orchestrator->SubmitActionAsync(Action);
		TestWorld->Tick(SUPPORT_SETTLE_FRAMES);
		Ability->RemoveFromRoot();
	}
END_DEFINE_SPEC(FSupportNoBuildupSpec)

void FSupportNoBuildupSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		BuildupManager = nullptr;
		Hero = Ally = Foe = nullptr;
		TestWorld.Reset();
	});

	Describe("the rule", [this]()
	{
		It("counts Restoration and Enhancement spells as support", [this]()
		{
			TestTrue(TEXT("Restoration"), SupportSkillRules::IsSupportSkill(SupportTestSpell(ESpellSchool::Restoration)));
			TestTrue(TEXT("Enhancement"), SupportSkillRules::IsSupportSkill(SupportTestSpell(ESpellSchool::Enhancement)));
			TestEqual(TEXT("no buildup"), SupportSkillRules::GetAuthoredStatusBuildup(SupportTestSpell(ESpellSchool::Restoration)), 0);
		});

		It("leaves a Destruction spell's buildup alone", [this]()
		{
			USpellData *Spell = SupportTestSpell(ESpellSchool::Destruction);
			TestFalse(TEXT("not support"), SupportSkillRules::IsSupportSkill(Spell));
			TestEqual(TEXT("authored buildup"), SupportSkillRules::GetAuthoredStatusBuildup(Spell), SUPPORT_TEST_BUILDUP);
		});

		It("counts a no-damage ability with a heal as support, not a damaging one", [this]()
		{
			TestTrue(TEXT("heal ability"), SupportSkillRules::IsSupportSkill(SupportTestAbility(0, true)));
			TestFalse(TEXT("damaging ability with a heal"), SupportSkillRules::IsSupportSkill(SupportTestAbility(SUPPORT_TEST_DAMAGE, true)));
			TestFalse(TEXT("damaging ability"), SupportSkillRules::IsSupportSkill(SupportTestAbility(SUPPORT_TEST_DAMAGE, false)));
		});

		It("gives a support spell no buildup in its own calculation", [this]()
		{
			TestEqual(TEXT("CalculateStatusBuildup"), SupportTestSpell(ESpellSchool::Restoration)->CalculateStatusBuildup(FWoRTestWorld::NewCharacterData()), 0);
		});
	});

	Describe("in a fight", [this]()
	{
		It("healing an ally adds no buildup", [this]()
		{
			AActor *Actor = StartFight();
			if (!Actor)
			{
				return;
			}
			AActor *Target = Actor == Hero ? Ally : Hero;
			UseAbility(SupportTestAbility(0, true), Target, ETargetType::Ally);
			TestEqual(TEXT("ally's bar empty"), BuildupManager->GetStatusBarBuildup(Target), 0.0f);
		});

		It("an attack still builds status", [this]()
		{
			AActor *Actor = StartFight();
			if (!Actor)
			{
				return;
			}
			UseAbility(SupportTestAbility(SUPPORT_TEST_DAMAGE, false), Foe, ETargetType::Enemy);
			TestTrue(TEXT("foe's bar filled"), BuildupManager->GetStatusBarBuildup(Foe) > 0.0f);
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
