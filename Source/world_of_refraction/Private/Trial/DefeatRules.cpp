// DefeatRules.cpp

#include "Trial/DefeatRules.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Party/PartySessionSubsystem.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	UTrialRunSubsystem *DefeatRun(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	}
}

namespace WoRDefeat
{
	FWipeResult ApplyWipe(UWorld *World, EEncounterRank Rank, const TArray<FName> &MemberKeys)
	{
		FWipeResult Result;
		UTrialRunSubsystem *Run = DefeatRun(World);
		if (!Run || !Run->IsRunActive())
		{
			UE_LOG(LogTemp, Log, TEXT("[Defeat] A wipe outside a run costs nothing"));
			return Result;
		}
		Result.bRunEnds = Run->RecordWipe(Rank);
		Result.Wipes = Run->GetRunWipes();
		Result.MainBossLosses = Run->GetRunMainBossLosses();
		const int32 MaxPoints = DefeatRules::GetPointsLost(Rank);
		for (const FName Key : MemberKeys)
		{
			Result.PointsTaken += Run->LoseRunPoints(Key, MaxPoints);
		}
		if (!Result.bRunEnds)
		{
			// D-043: the rest spot revives the fallen; the whole party fell, so all of it comes back at full.
			UGameInstance *GI = World->GetGameInstance();
			UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
			Result.Revived = PartySession ? PartySession->ReviveFallenMembers() : 0;
		}
		UE_LOG(LogTemp, Log, TEXT("[Defeat] Wipe by a %s enemy: %d run point(s) taken from %d member(s), %d revived, rest spot '%s'%s"),
			   *UEnum::GetValueAsString(Rank), Result.PointsTaken, MemberKeys.Num(), Result.Revived,
			   *Run->GetLastRestSpot().ToString(), Result.bRunEnds ? TEXT("; the run ends") : TEXT(""));
		return Result;
	}

	FString GetDefeatString(const UWorld *World)
	{
		const UTrialRunSubsystem *Run = DefeatRun(World);
		if (!Run)
		{
			return TEXT("[Defeat] no trial run subsystem");
		}
		return FString::Printf(TEXT("[Defeat] wipes %d of %d, main-boss losses %d of %d, last rest spot '%s' (None = the trial's start)"),
							   Run->GetRunWipes(), DefeatRules::MAX_WIPES_PER_RUN, Run->GetRunMainBossLosses(),
							   DefeatRules::MAX_MAIN_BOSS_LOSSES_PER_RUN, *Run->GetLastRestSpot().ToString());
	}
}

static FAutoConsoleCommandWithWorld GTrialPrintLossesCommand(
	TEXT("wor.Trial.PrintLosses"),
	TEXT("B-362: the run's wipe and main-boss-loss counters against their limits, and the last rest spot used."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UE_LOG(LogTemp, Display, TEXT("%s"), *WoRDefeat::GetDefeatString(World)); }));
