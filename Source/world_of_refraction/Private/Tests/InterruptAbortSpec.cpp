// InterruptAbortSpec.cpp
// B-431 (D-161, B-367 / B-367b): an interrupt point aborts the rest of an attack only when every defender has
// resolved the hit with a Perfect Guard (a luck dodge or a normal guard stops nothing). On the abort each defender
// whose hits so far were all perfect gets one Flawless Parry broadcast. A spell's choke point aborts only once every
// expected target (ChokeExpectedCount) has resolved it, all Perfect Guarded. Driven through the friend access on a
// hand-built execution context: no montage, no projectile.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "FlawlessParryRecorder.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/FlawlessParry.h"

namespace
{
	constexpr int32 INTERRUPT_TEST_HITS = 3;
	constexpr int32 INTERRUPT_TEST_TWO_TARGETS = 2;
}

BEGIN_DEFINE_SPEC(FInterruptAbortSpec, "WoR.Defense.InterruptAbort",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UActionExecutor *Executor = nullptr;
	UFlawlessParryRecorder *Recorder = nullptr;
	AActor *Attacker = nullptr;
	AActor *First = nullptr;
	AActor *Second = nullptr;

	/** Records Defender's resolved hit at the interrupt point on the live context (the tally counts it too). */
	void Resolve(AActor *Defender, EDefenseType Type, bool bSuccess, bool bPerfect, bool bLuckDodge = false)
	{
		FPendingDefenseContext &Ctx = Executor->CurrentExecutionContext->PendingDefenses.FindOrAdd(Defender);
		Ctx.Attacker = Attacker;
		Ctx.ResolvedDefenseType = Type;
		Ctx.bResolvedSuccess = bSuccess;
		Ctx.bResolvedPerfect = bPerfect;
		Ctx.bResolvedLuckDodge = bLuckDodge;
		Ctx.bResultResolved = true;
		// As the executor counts a hit for the Flawless Parry: a Perfect Guard that wasn't a luck dodge.
		Ctx.FlawlessTally.AddHit(FlawlessParry::IsPerfectHit(Type, bSuccess, bPerfect) && !bLuckDodge, INTERRUPT_TEST_HITS);
	}

	/** Defender's window is open but the hit isn't resolved yet (as an opened defense window leaves it). */
	void Pending(AActor *Defender)
	{
		FPendingDefenseContext &Ctx = Executor->CurrentExecutionContext->PendingDefenses.FindOrAdd(Defender);
		Ctx.Attacker = Attacker;
		Ctx.bResultResolved = false;
	}

	void StartAttack()
	{
		Executor->CurrentExecutionContext.Emplace();
		Executor->CurrentExecutionContext->Executor = Attacker;
	}
END_DEFINE_SPEC(FInterruptAbortSpec)

void FInterruptAbortSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Executor = TestWorld->GetSubsystem<UActionExecutor>();
		Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		First = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Second = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		UDefenseSystem *Defense = TestWorld->GetSubsystem<UDefenseSystem>();
		Recorder = NewObject<UFlawlessParryRecorder>();
		Recorder->AddToRoot();
		Recorder->Listen(Defense);
		if (!TestNotNull(TEXT("ActionExecutor"), Executor) || !TestNotNull(TEXT("DefenseSystem"), Defense) ||
			!TestTrue(TEXT("Combatants"), Attacker && First && Second))
		{
			Executor = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		if (Executor)
		{
			Executor->CurrentExecutionContext.Reset();
		}
		if (Recorder)
		{
			Recorder->RemoveFromRoot();
		}
		Recorder = nullptr;
		Executor = nullptr;
		Attacker = First = Second = nullptr;
		TestWorld.Reset();
	});

	It("doesn't abort when only one of two defenders Perfect Guards", [this]()
	{
		if (!Executor) return;
		StartAttack();
		Pending(Second);
		Resolve(First, EDefenseType::Block, true, true);
		TestFalse(TEXT("The second defender hasn't resolved yet"), Executor->AllInterruptSucceeded());
		Resolve(Second, EDefenseType::Block, true, false);
		TestFalse(TEXT("A normal guard doesn't stop the attack"), Executor->AllInterruptSucceeded());
	});

	It("doesn't abort on a luck dodge or a normal dodge", [this]()
	{
		if (!Executor) return;
		StartAttack();
		Resolve(First, EDefenseType::Block, true, true);
		Resolve(Second, EDefenseType::Block, true, true, /*bLuckDodge*/ true);
		TestFalse(TEXT("A luck-dodge success stops nothing, even on a Guard"), Executor->AllInterruptSucceeded());
		Resolve(Second, EDefenseType::Dodge, true, true);
		TestFalse(TEXT("A Perfect Dodge isn't a Perfect Guard (B-367b)"), Executor->AllInterruptSucceeded());
	});

	It("aborts when both defenders Perfect Guard, with one Flawless Parry broadcast each", [this]()
	{
		if (!Executor) return;
		StartAttack();
		Resolve(First, EDefenseType::Block, true, true);
		Resolve(Second, EDefenseType::Parry, true, true);
		if (!TestTrue(TEXT("Both Perfect Guarded: abort"), Executor->AllInterruptSucceeded())) return;

		Executor->InterruptAsyncAction(Attacker, nullptr);
		TestEqual(TEXT("One Flawless Parry for the first"), Recorder->CountFor(First), 1);
		TestEqual(TEXT("One Flawless Parry for the second"), Recorder->CountFor(Second), 1);
		TestEqual(TEXT("No others"), Recorder->Defenders.Num(), INTERRUPT_TEST_TWO_TARGETS);
		TestFalse(TEXT("The action is finalised"), Executor->CurrentExecutionContext.IsSet());
	});

	It("aborts a choke point only once every expected target resolved it, all defended", [this]()
	{
		if (!Executor) return;
		StartAttack();
		TestFalse(TEXT("No active choke: never"), Executor->RecordChokeAndShouldAbort(First, true));

		Executor->CurrentExecutionContext->ChokeOutcomes.Reset();
		Executor->CurrentExecutionContext->ChokeExpectedCount = INTERRUPT_TEST_TWO_TARGETS;
		TestFalse(TEXT("One of two resolved: wait"), Executor->RecordChokeAndShouldAbort(First, true));
		TestTrue(TEXT("Both resolved, both defended: abort"), Executor->RecordChokeAndShouldAbort(Second, true));

		Executor->CurrentExecutionContext->ChokeOutcomes.Reset();
		TestFalse(TEXT("First fails"), Executor->RecordChokeAndShouldAbort(First, false));
		TestFalse(TEXT("One failure: no abort"), Executor->RecordChokeAndShouldAbort(Second, true));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
