// BalanceSimReport.h
// B-293 (docs/Design/BalanceSimulator.md §3): the balance targets with their tolerances, checked per matchup over an
// aggregate, and the three report files a run writes: Saved/Balance/<preset>_<stamp>.csv (one row per fight),
// .json (aggregates and target results) and .md (the summary Crown reads). Debug: GetReportString.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Balance/BalanceSimTypes.h"

/** One target checked against one matchup's aggregate. An error target fails the nightly / full spec; a warn target
 *  is reported only (its band is a reference-backed default, or the sim's archetypes don't match the worked example). */
struct FBalanceTargetResult
{
	FName MatchupId;
	FString Target;
	float Value = 0.0f;
	float Min = 0.0f;
	float Max = 0.0f;
	bool bError = false;

	bool Passed() const { return Value >= Min && Value <= Max; }
	FString ToString() const
	{
		return FString::Printf(TEXT("%s %s: %.2f in [%.2f, %.2f] %s%s"), *MatchupId.ToString(), *Target, Value, Min, Max,
							   Passed() ? TEXT("ok") : (bError ? TEXT("FAIL") : TEXT("warn")), bError ? TEXT("") : TEXT(" (warn only)"));
	}
};

namespace BalanceSimReport
{
	/** The targets that apply to Matchup (by rank, party and difficulty), each with the aggregate's value. */
	TArray<FBalanceTargetResult> EvaluateTargets(const FBalanceMatchup &Matchup, const FBalanceAggregate &Aggregate);

	/** The three report texts. */
	FString ToCSV(FName Preset, const TArray<FBalanceFightRecord> &Records);
	FString ToJSON(FName Preset, const TArray<FBalanceAggregate> &Aggregates, const TArray<FBalanceTargetResult> &Results);
	FString ToMarkdown(FName Preset, const TArray<FBalanceAggregate> &Aggregates, const TArray<FBalanceTargetResult> &Results);

	/** Where a preset's reports go: Saved/Balance. */
	FString GetReportDirectory();

	/** Writes <Preset>_<stamp>.csv / .json / .md under GetReportDirectory; returns the paths written (empty on failure). */
	TArray<FString> WriteReport(FName Preset, const TArray<FBalanceMatchup> &Matchups, const TArray<FBalanceFightRecord> &Records,
								TArray<FBalanceAggregate> &OutAggregates, TArray<FBalanceTargetResult> &OutResults);

	/** Debug: every aggregate and target result, one line each. */
	FString GetReportString(const TArray<FBalanceAggregate> &Aggregates, const TArray<FBalanceTargetResult> &Results);
}

#endif // WITH_DEV_AUTOMATION_TESTS
