// ReviveVoteSpec.cpp
// B-187e (Q-187e changed by Crown, 2026-11-08): reviving a mini-boss is a unanimous vote of every player outside
// battle. All yes passes; one no fails; a player in battle isn't asked; one who enters a fight leaves the list; the
// timeout fails it; solo the starter's start passes at once.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Trial/ReviveVote.h"

namespace
{
	constexpr int32 VOTE_TEST_HOST = 1;
	constexpr int32 VOTE_TEST_FRIEND = 2;
	constexpr int32 VOTE_TEST_THIRD = 3;
	constexpr double VOTE_TEST_NOW = 100.0;
}

BEGIN_DEFINE_SPEC(FReviveVoteSpec, "WoR.Trial.ReviveVote", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	static TArray<int32> Everyone() { return {VOTE_TEST_HOST, VOTE_TEST_FRIEND, VOTE_TEST_THIRD}; }
END_DEFINE_SPEC(FReviveVoteSpec)

void FReviveVoteSpec::Define()
{
	It("passes when every player outside battle says yes", [this]()
	{
		FReviveVote Vote;
		TestTrue(TEXT("open"), ReviveVote::Start(Vote, VOTE_TEST_HOST, Everyone(), VOTE_TEST_NOW) == EReviveVoteResult::Pending);
		TestTrue(TEXT("the starter is not asked again"), !ReviveVote::IsWaitingFor(Vote, VOTE_TEST_HOST));
		TestTrue(TEXT("one more yes: still waiting"), ReviveVote::Cast(Vote, VOTE_TEST_FRIEND, true) == EReviveVoteResult::Pending);
		TestTrue(TEXT("the last yes passes it"), ReviveVote::Cast(Vote, VOTE_TEST_THIRD, true) == EReviveVoteResult::Passed);
		TestFalse(TEXT("closed"), Vote.bOpen);
	});

	It("fails on one no", [this]()
	{
		FReviveVote Vote;
		ReviveVote::Start(Vote, VOTE_TEST_FRIEND, Everyone(), VOTE_TEST_NOW);
		ReviveVote::Cast(Vote, VOTE_TEST_HOST, true);
		TestTrue(TEXT("a no fails it"), ReviveVote::Cast(Vote, VOTE_TEST_THIRD, false) == EReviveVoteResult::Failed);
		TestTrue(TEXT("and it stays failed"), ReviveVote::Cast(Vote, VOTE_TEST_HOST, true) == EReviveVoteResult::Failed);
	});

	It("doesn't ask a player in battle, and drops one who enters a fight", [this]()
	{
		const TArray<int32> Voters = ReviveVote::SelectVoters(Everyone(), [](int32 Id) { return Id == VOTE_TEST_THIRD; });
		TestEqual(TEXT("two voters"), Voters.Num(), 2);
		TestFalse(TEXT("the fighter isn't asked"), Voters.Contains(VOTE_TEST_THIRD));
		FReviveVote Vote;
		ReviveVote::Start(Vote, VOTE_TEST_HOST, Voters, VOTE_TEST_NOW);
		TestFalse(TEXT("nor waited for"), ReviveVote::IsWaitingFor(Vote, VOTE_TEST_THIRD));
		TestTrue(TEXT("the friend enters a fight: the rest all said yes, so it passes"),
				 ReviveVote::DropVoter(Vote, VOTE_TEST_FRIEND) == EReviveVoteResult::Passed);
	});

	It("refuses a starter in battle, and fails on the timeout", [this]()
	{
		FReviveVote Refused;
		TestTrue(TEXT("a starter in battle can't start it"),
				 ReviveVote::Start(Refused, VOTE_TEST_THIRD, {VOTE_TEST_HOST, VOTE_TEST_FRIEND}, VOTE_TEST_NOW) == EReviveVoteResult::Failed);
		TestFalse(TEXT("nothing opened"), Refused.bOpen);
		FReviveVote Vote;
		ReviveVote::Start(Vote, VOTE_TEST_HOST, Everyone(), VOTE_TEST_NOW);
		TestTrue(TEXT("before the deadline: waiting"), ReviveVote::CheckTimeout(Vote, VOTE_TEST_NOW + 1.0) == EReviveVoteResult::Pending);
		TestTrue(TEXT("past it: failed"),
				 ReviveVote::CheckTimeout(Vote, VOTE_TEST_NOW + ReviveVote::TIMEOUT_SECONDS) == EReviveVoteResult::Failed);
	});

	It("solo: no vote, the start revives at once", [this]()
	{
		FReviveVote Vote;
		TestTrue(TEXT("passes"), ReviveVote::Start(Vote, VOTE_TEST_HOST, {VOTE_TEST_HOST}, VOTE_TEST_NOW) == EReviveVoteResult::Passed);
		TestFalse(TEXT("nothing left open"), Vote.bOpen);
		TestFalse(TEXT("the debug string says so"), ReviveVote::GetVoteString(Vote, VOTE_TEST_NOW).IsEmpty());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
