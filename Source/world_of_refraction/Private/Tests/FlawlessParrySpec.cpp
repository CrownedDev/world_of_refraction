// FlawlessParrySpec.cpp
// B-365 (D-158): an attack whose every hit is perfectly guarded or dodged is a Flawless Parry, reported once at its
// last hit; one hit not perfect and it is not; a one-hit attack counts; the condition reads true through WoRConditions.
// B-367 (D-161): a perfect defense that ends the attack at an interrupt point, every hit so far perfect, reports one;
// the debug print lists a skill's interrupt points.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Defense/FlawlessParry.h"
#include "Conditions/WoRConditions.h"
#include "Debug/Skills/SkillCastDebug.h"
#include "Skills/Definitions/SpellData.h"
#include "WoRGameNames.h"

namespace
{
	constexpr int32 FLAWLESS_TEST_HITS = 3;

	/** Feeds a hit pattern through one tally; returns how many times it reported a Flawless Parry. */
	int32 FlawlessReports(const TArray<bool> &PerfectHits)
	{
		FlawlessParry::FTally Tally;
		int32 Reports = 0;
		for (const bool bPerfect : PerfectHits)
		{
			Reports += Tally.AddHit(bPerfect, PerfectHits.Num()) ? 1 : 0;
		}
		return Reports;
	}
}

BEGIN_DEFINE_SPEC(FFlawlessParrySpec, "WoR.Defense.FlawlessParry", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FFlawlessParrySpec)

void FFlawlessParrySpec::Define()
{
	It("counts a Perfect Guard or a Perfect Dodge for a hit, nothing else", [this]()
	{
		TestTrue(TEXT("Perfect Guard"), FlawlessParry::IsPerfectHit(EDefenseType::Block, true, true));
		TestTrue(TEXT("Perfect Dodge"), FlawlessParry::IsPerfectHit(EDefenseType::Dodge, true, true));
		TestFalse(TEXT("a normal Guard"), FlawlessParry::IsPerfectHit(EDefenseType::Block, true, false));
		TestFalse(TEXT("a failed defense"), FlawlessParry::IsPerfectHit(EDefenseType::Dodge, false, true));
		TestFalse(TEXT("undefended"), FlawlessParry::IsPerfectHit(EDefenseType::None, true, true));
	});

	It("fires once at the last hit when every hit is perfect, and not with one hit missed", [this]()
	{
		TestEqual(TEXT("three perfect hits: once"), FlawlessReports({true, true, true}), 1);
		TestEqual(TEXT("the last hit missed: never"), FlawlessReports({true, true, false}), 0);
		TestEqual(TEXT("the first hit missed: never"), FlawlessReports({false, true, true}), 0);
		TestEqual(TEXT("a one-hit attack counts"), FlawlessReports({true}), 1);
		TestEqual(TEXT("a one-hit miss does not"), FlawlessReports({false}), 0);

		FlawlessParry::FTally Tally;
		TestFalse(TEXT("not before the last hit"), Tally.AddHit(true, FLAWLESS_TEST_HITS));
		TestFalse(TEXT("still not"), Tally.AddHit(true, FLAWLESS_TEST_HITS));
		TestTrue(TEXT("at the last hit"), Tally.AddHit(true, FLAWLESS_TEST_HITS));
		TestTrue(TEXT("the debug line counts the hits"), FlawlessParry::GetTallyString(Tally, FLAWLESS_TEST_HITS).Contains(TEXT("3 perfect")));
	});

	It("fires once when a perfect defense ends the attack at an interrupt point, every hit so far perfect (B-367)", [this]()
	{
		FlawlessParry::FTally Ended;
		Ended.AddHit(true, FLAWLESS_TEST_HITS);
		Ended.AddHit(true, FLAWLESS_TEST_HITS); // the interrupt point: perfect, the attack ends here
		TestTrue(TEXT("ended at the interrupt point: flawless"), Ended.EndAtInterrupt());
		TestFalse(TEXT("only once"), Ended.EndAtInterrupt());

		FlawlessParry::FTally Missed;
		Missed.AddHit(false, FLAWLESS_TEST_HITS);
		Missed.AddHit(true, FLAWLESS_TEST_HITS);
		TestFalse(TEXT("an earlier hit not perfect: none"), Missed.EndAtInterrupt());

		FlawlessParry::FTally Last;
		Last.AddHit(true, 1); // the interrupt point was the last hit: already reported there
		TestFalse(TEXT("no second report"), Last.EndAtInterrupt());

		FlawlessParry::FTally Unjudged;
		TestFalse(TEXT("nothing judged: none"), Unjudged.EndAtInterrupt());
	});

	It("lists a skill's interrupt points (B-367 debug)", [this]()
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		TestTrue(TEXT("none authored"), USkillCastDebug::GetInterruptPointsString(Spell).EndsWith(TEXT("none")));
		FDamageSplitEntry &Hit = Spell->DamageSplit.AddDefaulted_GetRef();
		Hit.HitNumber = 2;
		Hit.bInterruptable = true;
		Spell->CastArray.AddDefaulted();
		Spell->CastArray.AddDefaulted_GetRef().bInterruptable = true;
		TestTrue(TEXT("hit and cast points"), USkillCastDebug::GetInterruptPointsString(Spell).EndsWith(TEXT("hit 2, cast [1]")));
	});

	It("reads true through the shared conditions on a Flawless Parry, false elsewhere", [this]()
	{
		FWoRCondition OnFlawless;
		OnFlawless.Trigger = EWoRConditionTrigger::FlawlessParry;
		OnFlawless.Subject = EWoRConditionSubject::FlawlessParry;
		const FWoRFighterState Defender{1, 1};
		const FWoRConditionContext Flawless = WoRConditions::MakeFlawlessParryContext(&Defender, nullptr);
		TestTrue(TEXT("on a Flawless Parry"), WoRConditions::IsMet(OnFlawless, Flawless));

		FWoRCondition SubjectOnly;
		SubjectOnly.Subject = EWoRConditionSubject::FlawlessParry;
		TestTrue(TEXT("the subject alone holds on its event"), WoRConditions::IsMet(SubjectOnly, Flawless));
		TestFalse(TEXT("and not at another event"), WoRConditions::IsMet(SubjectOnly, FWoRConditionContext()));

		FWoRConditionContext FightWon;
		FightWon.Trigger = EWoRConditionTrigger::FightWon;
		TestFalse(TEXT("the trigger must match"), WoRConditions::IsMet(OnFlawless, FightWon));
		TestTrue(TEXT("the debug line names it"), WoRConditions::GetConditionString(OnFlawless).Contains(TEXT("FlawlessParry")));
	});

	It("has its name in the game-names table", [this]()
	{
		TestEqual(TEXT("name"), WoRGameNames::GetFlawlessParryName().ToString(), FString(TEXT("Flawless Parry")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
