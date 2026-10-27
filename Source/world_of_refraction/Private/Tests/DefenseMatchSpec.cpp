// DefenseMatchSpec.cpp
// B-135: how a defense press is matched to an impact today (UDefenseSystem::MatchAndConsumeInput).
// A press counts inside a two-sided window around the impact: up to the duel window before it, up
// to DEFENSE_AFTER_GRACE_SECONDS after; inside PerfectThreshold it is perfect. Each press is used
// once; with several in range the latest wins, so a multi-hit attack takes one press per impact. A
// harder difficulty shrinks the window. The clock itself is WoR.Defense.Clock.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Combat/CombatConstants.h"
#include "Combat/Defense/DefenseSystem.h"

namespace
{
	constexpr float MATCH_TEST_WINDOW_SECONDS = 0.5f;
	constexpr int32 MATCH_TEST_BASE_DAMAGE = 10;
	constexpr float MATCH_TEST_ATTACK_SIZE = 1.0f;
	/** Impacts sit well ahead of the clock, so no press time is 0 (0 means "now" to SubmitDefenseInput). */
	constexpr double MATCH_TEST_IMPACT_LEAD_SECONDS = 10.0;
	constexpr EActionType MATCH_TEST_ATTACK_TYPE = EActionType::Ability;
	constexpr double MATCH_TEST_HALF = 0.5;
	/** A press half a window past the window's edge. */
	constexpr double MATCH_TEST_OUTSIDE = 1.5;
}

BEGIN_DEFINE_SPEC(FDefenseMatchSpec, "WoR.Defense.Match",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UDefenseSystem *DefenseSystem = nullptr;
	AActor *Attacker = nullptr;
	AActor *Defender = nullptr;
	double Impact = 0.0;
	/** The duel window before an Easy impact (base + Reflex − Speed, floored). */
	double BeforeWindow = 0.0;

	void Press(EDefenseType Type, double InputTime)
	{
		DefenseSystem->SubmitDefenseInput(Defender, Type, EDefenseDirection::None, InputTime);
	}

	FDefenseInputMatch Match(double ImpactTime, const FDefenseDifficultyTriple &Difficulty = FDefenseDifficultyTriple())
	{
		return DefenseSystem->MatchAndConsumeInput(Defender, ImpactTime, MATCH_TEST_ATTACK_TYPE, Difficulty);
	}

	/** Halfway between the perfect band and the window's edge: a normal (not perfect) press. */
	static double Between(double PerfectBand, double Edge)
	{
		return PerfectBand + (Edge - PerfectBand) * MATCH_TEST_HALF;
	}
END_DEFINE_SPEC(FDefenseMatchSpec)

void FDefenseMatchSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		DefenseSystem = TestWorld->GetSubsystem<UDefenseSystem>();
		Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		Defender = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("DefenseSystem"), DefenseSystem) || !TestNotNull(TEXT("Attacker"), Attacker) ||
			!TestNotNull(TEXT("Defender"), Defender))
		{
			DefenseSystem = nullptr; // each case returns early
			return;
		}
		DefenseSystem->OpenDefenseWindow(Attacker, Defender, MATCH_TEST_ATTACK_SIZE, MATCH_TEST_BASE_DAMAGE, MATCH_TEST_WINDOW_SECONDS, true);
		Impact = UDefenseSystem::GetDefenseClockSeconds(DefenseSystem) + MATCH_TEST_IMPACT_LEAD_SECONDS;
		BeforeWindow = DefenseSystem->GetEffectiveDefenseInputWindow(Defender, Attacker, MATCH_TEST_ATTACK_TYPE);
		TestTrue(TEXT("The window is wider than the perfect band"), BeforeWindow > DefenseSystem->PerfectThreshold);
		TestTrue(TEXT("The after-grace is wider than the perfect band"),
				 CombatConstants::DEFENSE_AFTER_GRACE_SECONDS > DefenseSystem->PerfectThreshold);
	});

	AfterEach([this]()
	{
		if (DefenseSystem && Defender)
		{
			DefenseSystem->CloseDefenseWindow(Defender);
		}
		DefenseSystem = nullptr;
		Attacker = nullptr;
		Defender = nullptr;
		TestWorld.Reset();
	});

	It("calls a dead-on press perfect and a press inside the window a normal match", [this]()
	{
		if (!DefenseSystem) return;
		Press(EDefenseType::Block, Impact);
		const FDefenseInputMatch DeadOn = Match(Impact);
		TestTrue(TEXT("Dead-on matches"), DeadOn.bMatched);
		TestTrue(TEXT("Dead-on is perfect"), DeadOn.bPerfect);

		Press(EDefenseType::Block, Impact - Between(DefenseSystem->PerfectThreshold, BeforeWindow));
		const FDefenseInputMatch Early = Match(Impact);
		TestTrue(TEXT("Early, inside the lead-in, matches"), Early.bMatched);
		TestFalse(TEXT("but is not perfect"), Early.bPerfect);

		Press(EDefenseType::Block, Impact + Between(DefenseSystem->PerfectThreshold, CombatConstants::DEFENSE_AFTER_GRACE_SECONDS));
		const FDefenseInputMatch Late = Match(Impact);
		TestTrue(TEXT("Late, inside the after-grace, matches"), Late.bMatched);
		TestFalse(TEXT("but is not perfect"), Late.bPerfect);
	});

	It("misses a press outside the window on either side", [this]()
	{
		if (!DefenseSystem) return;
		Press(EDefenseType::Block, Impact - BeforeWindow * MATCH_TEST_OUTSIDE);
		Press(EDefenseType::Block, Impact + CombatConstants::DEFENSE_AFTER_GRACE_SECONDS * MATCH_TEST_OUTSIDE);
		TestFalse(TEXT("Neither press counts"), Match(Impact).bMatched);
	});

	It("uses each press once", [this]()
	{
		if (!DefenseSystem) return;
		Press(EDefenseType::Block, Impact);
		TestTrue(TEXT("First impact takes the press"), Match(Impact).bMatched);
		TestFalse(TEXT("A second impact at the same moment finds nothing"), Match(Impact).bMatched);
	});

	It("gives a multi-hit attack one press per impact, the latest in range first", [this]()
	{
		if (!DefenseSystem) return;
		const double SecondImpact = Impact + BeforeWindow + CombatConstants::DEFENSE_AFTER_GRACE_SECONDS;
		// The dodge is dead-on; the block is later but further away, so "latest" and "closest" differ.
		Press(EDefenseType::Dodge, Impact);
		Press(EDefenseType::Block, Impact + DefenseSystem->PerfectThreshold * MATCH_TEST_HALF);
		Press(EDefenseType::Block, SecondImpact);

		const FDefenseInputMatch First = Match(Impact);
		TestTrue(TEXT("First impact matched"), First.bMatched);
		TestEqual(TEXT("The latest press in range wins, not the closest"), First.Type, EDefenseType::Block);
		const FDefenseInputMatch Second = Match(SecondImpact);
		TestTrue(TEXT("Second impact matched by its own press"), Second.bMatched && Second.bPerfect);
		const FDefenseInputMatch Again = Match(Impact);
		TestTrue(TEXT("The older press is still there for a third impact"), Again.bMatched);
		TestEqual(TEXT("and it is the dodge"), Again.Type, EDefenseType::Dodge);
	});

	It("shrinks the window for a harder difficulty", [this]()
	{
		if (!DefenseSystem) return;
		const double HardWindow = FMath::Max(CombatConstants::MINIMUM_DEFENSE_WINDOW, BeforeWindow * CombatConstants::DEFENSE_DIFFICULTY_HARD);
		if (!TestTrue(TEXT("Hard is narrower than Easy here"), HardWindow < BeforeWindow)) return;
		const double PressTime = Impact - (HardWindow + BeforeWindow) * MATCH_TEST_HALF;
		FDefenseDifficultyTriple Hard;
		Hard.Block = EDefenseDifficulty::Hard;

		Press(EDefenseType::Block, PressTime);
		TestFalse(TEXT("Hard: outside its window"), Match(Impact, Hard).bMatched);
		TestTrue(TEXT("Easy: the same press counts"), Match(Impact).bMatched);
	});

	It("treats a Parry press as a Guard (Block) press", [this]()
	{
		if (!DefenseSystem) return;
		Press(EDefenseType::Parry, Impact);
		TestEqual(TEXT("Stored as Block (D-084)"), Match(Impact).Type, EDefenseType::Block);
	});

	It("ignores presses once the window is closed", [this]()
	{
		if (!DefenseSystem) return;
		DefenseSystem->CloseDefenseWindow(Defender);
		Press(EDefenseType::Block, Impact);
		TestFalse(TEXT("No window, no match"), Match(Impact).bMatched);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
