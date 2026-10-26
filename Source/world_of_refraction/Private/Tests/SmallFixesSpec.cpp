// SmallFixesSpec.cpp
// B-067 regression tests: a DoubleHit's second hit counts toward the hit's DamageDealt, and the
// turn scheduler keeps running when a combatant's actor is destroyed mid-fight.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/TurnManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 SMALL_FIXES_BASE_DAMAGE = 20;
	constexpr int32 SMALL_FIXES_DOUBLE_HIT_ID = 67001;
	constexpr int32 SMALL_FIXES_DURATION_TURNS = 3;
	constexpr int32 SMALL_FIXES_TURNS_TO_ADVANCE = 3;
}

BEGIN_DEFINE_SPEC(FSmallFixesSpec, "WoR.Combat.SmallFixes",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;

	AActor *Spawn()
	{
		return TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	}
END_DEFINE_SPEC(FSmallFixesSpec)

void FSmallFixesSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		if (UTurnManager *TurnManager = TestWorld->GetSubsystem<UTurnManager>())
		{
			if (TurnManager->IsCombatActive())
			{
				TurnManager->EndCombat();
			}
		}
		TestWorld.Reset();
	});

	It("counts a DoubleHit's second hit in the hit's damage dealt", [this]()
	{
		UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!TestNotNull(TEXT("ActionExecutor"), Executor) || !TestNotNull(TEXT("SkillEffectManager"), Effects)) return;
		AActor *Attacker = Spawn();
		AActor *SingleTarget = Spawn();
		AActor *Target = Spawn();

		FActionHitInput Hit;
		Hit.Attacker = Attacker;
		Hit.ActionType = EActionType::Ability;
		Hit.BaseDamage = SMALL_FIXES_BASE_DAMAGE;
		Hit.bCanCrit = false;

		// One plain hit first, as the yardstick.
		Hit.Target = SingleTarget;
		UCharacterDataComponent *SingleComp = SingleTarget->FindComponentByClass<UCharacterDataComponent>();
		const int32 SingleHPBefore = SingleComp->CurrentHP;
		Executor->ApplyHit(Hit);
		const int32 SingleHitHPLost = SingleHPBefore - SingleComp->CurrentHP;

		Effects->ApplyEffect(Attacker, FActiveSkillEffect::CreateBuff(TEXT("Double hit test"), SMALL_FIXES_DOUBLE_HIT_ID,
			ESkillEffectType::DoubleHit, 0.0f, SMALL_FIXES_DURATION_TURNS));
		TestTrue(TEXT("Attacker has DoubleHit"), Effects->HasEffectOfType(Attacker, ESkillEffectType::DoubleHit));

		Hit.Target = Target;
		UCharacterDataComponent *TargetComp = Target->FindComponentByClass<UCharacterDataComponent>();
		const int32 HPBefore = TargetComp->CurrentHP;
		const FCombatHitResult Result = Executor->ApplyHit(Hit);
		const int32 HPLost = HPBefore - TargetComp->CurrentHP;
		TestTrue(TEXT("Target survived both hits"), TargetComp->CurrentHP > 0);
		TestTrue(TEXT("The second hit landed (more HP lost than one hit)"), HPLost > SingleHitHPLost);
		TestEqual(TEXT("Damage dealt is all the HP lost (both hits)"), Result.DamageDealt, HPLost);
	});

	It("keeps scheduling turns after a combatant's actor is destroyed", [this]()
	{
		UTurnManager *TurnManager = TestWorld->GetSubsystem<UTurnManager>();
		if (!TestNotNull(TEXT("TurnManager"), TurnManager)) return;
		AActor *Local = Spawn();
		AActor *Survivor = Spawn();
		AActor *Doomed = Spawn();
		TurnManager->InitializeCombat({Local}, {Survivor, Doomed});
		Doomed->Destroy();
		// Collect it so the TurnManager's reflected references to it are nulled (the crash path).
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		TestWorld->Tick(1);

		for (int32 Turn = 0; Turn < SMALL_FIXES_TURNS_TO_ADVANCE; ++Turn)
		{
			TurnManager->AdvanceToNextTurn();
		}
		AActor *Current = TurnManager->GetCurrentActor();
		TestTrue(TEXT("A living combatant has the turn"), Current == Local || Current == Survivor);
	});
}

#endif
