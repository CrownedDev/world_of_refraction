// BalanceSimReport.cpp
// B-293: see BalanceSimReport.h. The bands are BalanceSimulator.md §3's table; the boss win-rate band and the
// hits-to-kill bands beyond the Decided list are reference-backed defaults (Q-293).

#include "Balance/BalanceSimReport.h"
#include "Balance/BalanceArchetypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// ---- Targets (BalanceSimulator.md §3) ----
	constexpr float NORMAL_PARTY_TURNS_MIN = 2.5f;
	constexpr float NORMAL_PARTY_TURNS_MAX = 5.5f;
	constexpr float NORMAL_PARTY_TURNS_P90_MAX = 7.0f;
	constexpr float MINIBOSS_ROUNDS_MIN = 7.0f;
	constexpr float MINIBOSS_ROUNDS_MAX = 11.0f;
	constexpr float BOSS_ROUNDS_MIN = 11.0f;
	constexpr float BOSS_ROUNDS_MAX = 15.5f;
	constexpr float BOSS_ROUNDS_P90_MAX = 18.0f;
	constexpr float NORMAL_HITS_TO_KILL_MIN = 2.5f;
	constexpr float NORMAL_HITS_TO_KILL_MAX = 5.5f;
	/** Elite 5-7, Mini-boss 10-14, Boss 25-35, each +-20%. */
	constexpr float HITS_BAND_TOLERANCE = 0.2f;
	constexpr float ELITE_HITS_MIN = 5.0f;
	constexpr float ELITE_HITS_MAX = 7.0f;
	constexpr float MINIBOSS_HITS_MIN = 10.0f;
	constexpr float MINIBOSS_HITS_MAX = 14.0f;
	constexpr float BOSS_HITS_MIN = 25.0f;
	constexpr float BOSS_HITS_MAX = 35.0f;
	/** The §3 worked examples' basic attacks (F / C / S), +-10% on the uncrit median. The sim's Mid and Late
	 *  archetypes clamp Mind to 7, so these are warn-only until the examples are redone at the cap. */
	constexpr float EXAMPLE_HIT_EARLY = 44.0f;
	constexpr float EXAMPLE_HIT_MID = 132.0f;
	constexpr float EXAMPLE_HIT_LATE = 382.0f;
	constexpr float EXAMPLE_HIT_TOLERANCE = 0.1f;
	constexpr float CRIT_RATE_MIN = 0.03f;
	constexpr float CRIT_RATE_MAX = 0.08f;
	constexpr float CRIT_RATIO_MIN = 1.2f;
	constexpr float CRIT_RATIO_MAX = 1.3f;
	constexpr float PILLAR6_WIN_MIN = 0.35f;
	constexpr float PILLAR6_WIN_MAX = 0.65f;
	constexpr float NORMAL_WIN_MIN = 0.85f;
	constexpr float NORMAL_WIN_MAX = 0.99f;
	constexpr float BOSS_WIN_MIN = 0.5f;
	constexpr float BOSS_WIN_MAX = 0.8f;
	constexpr float ANY_WIN_MIN = 0.01f;
	constexpr float SHARE_MAX = 1.0f;
	constexpr float NO_STALLS = 0.0f;

	const TCHAR *REPORT_DIR_NAME = TEXT("Balance");
	const TCHAR *REPORT_STAMP_FORMAT = TEXT("%Y%m%d_%H%M%S");

	bool StartsWith(FName Id, const TCHAR *Prefix)
	{
		return Id.ToString().StartsWith(Prefix);
	}

	FBalanceTargetResult Target(FName Matchup, const TCHAR *Name, float Value, float Min, float Max, bool bError)
	{
		FBalanceTargetResult Out;
		Out.MatchupId = Matchup;
		Out.Target = Name;
		Out.Value = Value;
		Out.Min = Min;
		Out.Max = Max;
		Out.bError = bError;
		return Out;
	}

	FString Quoted(const FString &Text)
	{
		return TEXT("\"") + Text.Replace(TEXT("\""), TEXT("\\\"")) + TEXT("\"");
	}

	FString Stamp()
	{
		return FDateTime::Now().ToString(REPORT_STAMP_FORMAT);
	}
}

namespace BalanceSimReport
{
	TArray<FBalanceTargetResult> EvaluateTargets(const FBalanceMatchup &Matchup, const FBalanceAggregate &A)
	{
		TArray<FBalanceTargetResult> Out;
		const FName Id = Matchup.Id;
		const bool bMedium = Matchup.Difficulty == EAIDifficulty::Medium;
		const bool bPillar6 = StartsWith(Id, TEXT("Pillar6-"));
		const bool bPillar6Reverse = StartsWith(Id, TEXT("Pillar6Reverse-"));

		Out.Add(Target(Id, TEXT("stalled fights"), static_cast<float>(A.Stalled), NO_STALLS, NO_STALLS, true));
		if (bPillar6)
		{
			Out.Add(Target(Id, TEXT("pillar 6 win rate (both sides win sometimes)"), A.PartyWinRate, PILLAR6_WIN_MIN, PILLAR6_WIN_MAX, true));
			return Out;
		}
		if (bPillar6Reverse)
		{
			Out.Add(Target(Id, TEXT("pillar 6 reverse still clears"), A.PartyWinRate, ANY_WIN_MIN, SHARE_MAX, true));
			return Out;
		}
		switch (Matchup.Rank)
		{
		case EEncounterRank::Normal:
		{
			// B-293c: BaselineNumbers §2.9's "3-5 player turns" are party ROUNDS ("a group of 2-3 dies in about 3-5 party
			// rounds, 3 actors"), so the member turns are divided by the party size.
			const float PartySize = FMath::Max(1, Matchup.Party.Num());
			Out.Add(Target(Id, TEXT("party rounds to clear a normal fight"), A.MeanPartyTurns / PartySize, NORMAL_PARTY_TURNS_MIN, NORMAL_PARTY_TURNS_MAX, true));
			Out.Add(Target(Id, TEXT("party rounds p90"), A.P90PartyTurns / PartySize, 0.0f, NORMAL_PARTY_TURNS_P90_MAX, false));
		}
			Out.Add(Target(Id, TEXT("hits to kill a normal enemy"), A.MeanHitsToKill, NORMAL_HITS_TO_KILL_MIN, NORMAL_HITS_TO_KILL_MAX, false));
			if (bMedium)
			{
				Out.Add(Target(Id, TEXT("win rate vs normal at Medium"), A.PartyWinRate, NORMAL_WIN_MIN, NORMAL_WIN_MAX, true));
				const float Example = StartsWith(Id, TEXT("Early-")) ? EXAMPLE_HIT_EARLY : StartsWith(Id, TEXT("Mid-")) ? EXAMPLE_HIT_MID : EXAMPLE_HIT_LATE;
				// Every party hit (attacks, abilities, spells), not basic attacks alone: a rough guide only.
				Out.Add(Target(Id, TEXT("uncrit party hit median (all skills) vs the worked example's basic attack"), A.MedianUncritHit,
							   Example * (1.0f - EXAMPLE_HIT_TOLERANCE), Example * (1.0f + EXAMPLE_HIT_TOLERANCE), false));
			}
			break;
		case EEncounterRank::Elite:
			Out.Add(Target(Id, TEXT("hits to kill an elite"), A.MeanHitsToKill, ELITE_HITS_MIN * (1.0f - HITS_BAND_TOLERANCE),
						   ELITE_HITS_MAX * (1.0f + HITS_BAND_TOLERANCE), false));
			break;
		case EEncounterRank::MiniBoss:
			Out.Add(Target(Id, TEXT("rounds for a mini-boss"), A.MeanRounds, MINIBOSS_ROUNDS_MIN, MINIBOSS_ROUNDS_MAX, true));
			Out.Add(Target(Id, TEXT("hits to kill a mini-boss"), A.MeanHitsToKill, MINIBOSS_HITS_MIN * (1.0f - HITS_BAND_TOLERANCE),
						   MINIBOSS_HITS_MAX * (1.0f + HITS_BAND_TOLERANCE), false));
			break;
		case EEncounterRank::Boss:
			Out.Add(Target(Id, TEXT("rounds for a boss"), A.MeanRounds, BOSS_ROUNDS_MIN, BOSS_ROUNDS_MAX, true));
			Out.Add(Target(Id, TEXT("rounds p90"), A.P90Rounds, 0.0f, BOSS_ROUNDS_P90_MAX, false));
			Out.Add(Target(Id, TEXT("hits to kill a boss"), A.MeanHitsToKill, BOSS_HITS_MIN * (1.0f - HITS_BAND_TOLERANCE),
						   BOSS_HITS_MAX * (1.0f + HITS_BAND_TOLERANCE), false));
			if (bMedium)
			{
				// Reference-backed default (Q-293): a first boss attempt is lost more often than a normal fight, never a coin flip.
				Out.Add(Target(Id, TEXT("win rate vs boss at Medium"), A.PartyWinRate, BOSS_WIN_MIN, BOSS_WIN_MAX, false));
			}
			break;
		default:
			break;
		}
		// Crit targets hold for un-invested Luck; the Late party invests Luck, so these stay warn-only everywhere.
		Out.Add(Target(Id, TEXT("party crit rate"), A.CritRate, CRIT_RATE_MIN, CRIT_RATE_MAX, false));
		if (A.MedianUncritHit > 0.0f && A.MedianCritHit > 0.0f)
		{
			Out.Add(Target(Id, TEXT("crit / uncrit median"), A.MedianCritHit / A.MedianUncritHit, CRIT_RATIO_MIN, CRIT_RATIO_MAX, false));
		}
		return Out;
	}

	FString ToCSV(FName Preset, const TArray<FBalanceFightRecord> &Records)
	{
		static const TCHAR *OutcomeNames[] = {TEXT("PartyWon"), TEXT("EnemiesWon"), TEXT("Draw"), TEXT("Stalled")};
		FString Out = TEXT("preset,matchup,seed,outcome,turns,party_turns,rounds,ticks,party_damage,enemy_damage,hits,crits,status_breaks,hits_to_kill,stalled\n");
		for (const FBalanceFightRecord &R : Records)
		{
			Out += FString::Printf(TEXT("%s,%s,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%d\n"), *Preset.ToString(), *R.MatchupId.ToString(), R.Seed,
								   OutcomeNames[static_cast<int32>(R.Outcome)], R.Turns, R.PartyTurns, R.Rounds, R.Ticks, R.PartyDamage,
								   R.EnemyDamage, R.Hits, R.Crits, R.StatusBreaks,
								   *FString::JoinBy(R.HitsToKill, TEXT(";"), [](int32 V) { return FString::FromInt(V); }),
								   R.Outcome == EBalanceFightOutcome::Stalled ? 1 : 0);
		}
		return Out;
	}

	FString ToJSON(FName Preset, const TArray<FBalanceAggregate> &Aggregates, const TArray<FBalanceTargetResult> &Results)
	{
		// Hand-written: the module doesn't link Json, and the shape is small.
		FString Out = FString::Printf(TEXT("{\n  \"preset\": %s,\n  \"written\": %s,\n  \"matchups\": [\n"), *Quoted(Preset.ToString()), *Quoted(Stamp()));
		for (int32 Index = 0; Index < Aggregates.Num(); ++Index)
		{
			const FBalanceAggregate &A = Aggregates[Index];
			Out += FString::Printf(TEXT("    {\"id\": %s, \"fights\": %d, \"stalled\": %d, \"party_win_rate\": %.4f, \"mean_turns\": %.3f, "
										 "\"mean_party_turns\": %.3f, \"p90_party_turns\": %.1f, \"mean_rounds\": %.3f, \"p90_rounds\": %.1f, "
										 "\"mean_hits_to_kill\": %.3f, \"median_uncrit_hit\": %.1f, \"median_crit_hit\": %.1f, \"crit_rate\": %.4f}%s\n"),
								   *Quoted(A.MatchupId.ToString()), A.Fights, A.Stalled, A.PartyWinRate, A.MeanTurns, A.MeanPartyTurns, A.P90PartyTurns,
								   A.MeanRounds, A.P90Rounds, A.MeanHitsToKill, A.MedianUncritHit, A.MedianCritHit, A.CritRate,
								   Index + 1 < Aggregates.Num() ? TEXT(",") : TEXT(""));
		}
		Out += TEXT("  ],\n  \"targets\": [\n");
		for (int32 Index = 0; Index < Results.Num(); ++Index)
		{
			const FBalanceTargetResult &T = Results[Index];
			Out += FString::Printf(TEXT("    {\"matchup\": %s, \"target\": %s, \"value\": %.4f, \"min\": %.4f, \"max\": %.4f, \"severity\": %s, \"passed\": %s}%s\n"),
								   *Quoted(T.MatchupId.ToString()), *Quoted(T.Target), T.Value, T.Min, T.Max, T.bError ? TEXT("\"error\"") : TEXT("\"warn\""),
								   T.Passed() ? TEXT("true") : TEXT("false"), Index + 1 < Results.Num() ? TEXT(",") : TEXT(""));
		}
		Out += TEXT("  ]\n}\n");
		return Out;
	}

	FString ToMarkdown(FName Preset, const TArray<FBalanceAggregate> &Aggregates, const TArray<FBalanceTargetResult> &Results)
	{
		int32 Failed = 0;
		int32 Warned = 0;
		for (const FBalanceTargetResult &T : Results)
		{
			if (!T.Passed())
			{
				(T.bError ? Failed : Warned) += 1;
			}
		}
		FString Out = FString::Printf(TEXT("# Balance report: %s (%s)\n\n%d matchup(s), %d target(s): %d failed, %d warned.\n\n"), *Preset.ToString(),
									  *Stamp(), Aggregates.Num(), Results.Num(), Failed, Warned);
		Out += TEXT("## Matchups\n\n| Matchup | Fights | Stalled | Party wins | Party turns (p90) | Rounds (p90) | Hits to kill | Uncrit hit | Crit hit | Crit rate |\n|---|---|---|---|---|---|---|---|---|---|\n");
		for (const FBalanceAggregate &A : Aggregates)
		{
			Out += FString::Printf(TEXT("| %s | %d | %d | %.0f%% | %.1f (%.0f) | %.1f (%.0f) | %.1f | %.0f | %.0f | %.1f%% |\n"), *A.MatchupId.ToString(),
								   A.Fights, A.Stalled, A.PartyWinRate * 100.0f, A.MeanPartyTurns, A.P90PartyTurns, A.MeanRounds, A.P90Rounds,
								   A.MeanHitsToKill, A.MedianUncritHit, A.MedianCritHit, A.CritRate * 100.0f);
		}
		Out += TEXT("\n## Targets\n\n| Matchup | Target | Value | Band | Result |\n|---|---|---|---|---|\n");
		for (const FBalanceTargetResult &T : Results)
		{
			Out += FString::Printf(TEXT("| %s | %s | %.2f | [%.2f, %.2f] | %s |\n"), *T.MatchupId.ToString(), *T.Target, T.Value, T.Min, T.Max,
								   T.Passed() ? TEXT("ok") : (T.bError ? TEXT("**FAIL**") : TEXT("warn")));
		}
		return Out;
	}

	FString GetReportDirectory()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), REPORT_DIR_NAME);
	}

	TArray<FString> WriteReport(FName Preset, const TArray<FBalanceMatchup> &Matchups, const TArray<FBalanceFightRecord> &Records,
								TArray<FBalanceAggregate> &OutAggregates, TArray<FBalanceTargetResult> &OutResults)
	{
		OutAggregates.Reset();
		OutResults.Reset();
		for (const FBalanceMatchup &Matchup : Matchups)
		{
			const TArray<FBalanceFightRecord> Own = Records.FilterByPredicate([&Matchup](const FBalanceFightRecord &R) { return R.MatchupId == Matchup.Id; });
			if (Own.IsEmpty())
			{
				continue;
			}
			const FBalanceAggregate Aggregate = FBalanceAggregate::From(Own);
			OutAggregates.Add(Aggregate);
			OutResults.Append(EvaluateTargets(Matchup, Aggregate));
		}
		const FString Directory = GetReportDirectory();
		IFileManager::Get().MakeDirectory(*Directory, true);
		// The quick preset runs in every suite: it overwrites one report instead of piling up stamped ones.
		const bool bEverySuite = Preset == BalanceArchetypes::PresetNames()[0];
		const FString Base = FPaths::Combine(Directory, FString::Printf(TEXT("%s_%s"), *Preset.ToString(), bEverySuite ? TEXT("latest") : *Stamp()));
		TArray<FString> Paths;
		const TPair<FString, FString> Files[] = {{Base + TEXT(".csv"), ToCSV(Preset, Records)},
												 {Base + TEXT(".json"), ToJSON(Preset, OutAggregates, OutResults)},
												 {Base + TEXT(".md"), ToMarkdown(Preset, OutAggregates, OutResults)}};
		for (const TPair<FString, FString> &File : Files)
		{
			if (FFileHelper::SaveStringToFile(File.Value, *File.Key))
			{
				Paths.Add(File.Key);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[BalanceSim] couldn't write %s"), *File.Key);
			}
		}
		return Paths;
	}

	FString GetReportString(const TArray<FBalanceAggregate> &Aggregates, const TArray<FBalanceTargetResult> &Results)
	{
		FString Out = TEXT("[BalanceSim] report");
		for (const FBalanceAggregate &A : Aggregates)
		{
			Out += TEXT("\n  ") + A.ToString();
		}
		for (const FBalanceTargetResult &T : Results)
		{
			Out += TEXT("\n  target ") + T.ToString();
		}
		return Out;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
