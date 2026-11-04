// DefeatRules.cpp

#include "Trial/DefeatRules.h"

#include "Currency/CurrencyComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRPlayerState.h"
#include "Trial/TrialRunSubsystem.h"

namespace
{
	UTrialRunSubsystem *DefeatRun(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	}

	/** The local player's wallet: the run balance lives there (as USaveSubsystem carries it). */
	UCurrencyComponent *DefeatLocalWallet(const UWorld *World)
	{
		const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		const AWoRPlayerState *PS = PC && PC->IsLocalController() ? PC->GetPlayerState<AWoRPlayerState>() : nullptr;
		return PS ? PS->GetWallet() : nullptr;
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
		// D-160: the run's Prisms drop here instead of pillar points (every rank alike, the main boss too).
		Result.PrismsDropped = DropPrisms(*Run, DefeatLocalWallet(World), Result.PrismsLostForGood);
		if (!Result.bRunEnds)
		{
			// D-043: the rest spot revives the fallen; the whole party fell, so all of it comes back at full.
			UGameInstance *GI = World->GetGameInstance();
			UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
			Result.Revived = PartySession ? PartySession->ReviveFallenMembers() : 0;
		}
		UE_LOG(LogTemp, Log, TEXT("[Defeat] Wipe by a %s enemy: %d Prism(s) dropped (%d lost for good), %d member(s), %d revived, rest spot '%s'%s"),
			   *UEnum::GetValueAsString(Rank), Result.PrismsDropped, Result.PrismsLostForGood, MemberKeys.Num(), Result.Revived,
			   *Run->GetLastRestSpot().ToString(), Result.bRunEnds ? TEXT("; the run ends") : TEXT(""));
		return Result;
	}

	int32 DropPrisms(UTrialRunSubsystem &Run, UCurrencyComponent *Wallet, int32 &OutLostForGood)
	{
		const int32 Balance = Wallet ? Wallet->GetBalance(ECurrencyType::Gold) : 0;
		// Only what really left the wallet drops (Spend refuses without authority): nothing is doubled.
		const int32 Dropped = Balance > 0 && Wallet->Spend(ECurrencyType::Gold, Balance) ? Balance : 0;
		OutLostForGood = Run.DropPrisms(Dropped);
		return Dropped;
	}

	int32 RecoverPrisms(UTrialRunSubsystem &Run, UCurrencyComponent *Wallet)
	{
		if (!Wallet)
		{
			return 0; // the drop waits: nowhere to put it
		}
		const int32 Recovered = Run.TakeDropIfPendingFight();
		if (Recovered > 0)
		{
			Wallet->Add(ECurrencyType::Gold, Recovered);
		}
		return Recovered;
	}

	int32 RecoverPrisms(UWorld *World)
	{
		UTrialRunSubsystem *Run = DefeatRun(World);
		return Run && Run->IsRunActive() ? RecoverPrisms(*Run, DefeatLocalWallet(World)) : 0;
	}

	FString GetDefeatString(const UWorld *World)
	{
		const UTrialRunSubsystem *Run = DefeatRun(World);
		if (!Run)
		{
			return TEXT("[Defeat] no trial run subsystem");
		}
		return FString::Printf(TEXT("[Defeat] wipes %d of %d, main-boss losses %d of %d, %d Prism(s) dropped at [%s], last rest spot '%s' (None = the trial's start)"),
							   Run->GetRunWipes(), DefeatRules::MAX_WIPES_PER_RUN, Run->GetRunMainBossLosses(),
							   DefeatRules::MAX_MAIN_BOSS_LOSSES_PER_RUN, Run->GetDroppedPrisms(),
							   *FString::JoinBy(Run->GetDropEncounters(), TEXT(", "), [](FName Id) { return Id.ToString(); }),
							   *Run->GetLastRestSpot().ToString());
	}
}

static FAutoConsoleCommandWithWorld GTrialPrintLossesCommand(
	TEXT("wor.Trial.PrintLosses"),
	TEXT("B-362: the run's wipe and main-boss-loss counters against their limits, the dropped Prisms (B-362b), and the last rest spot used."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UE_LOG(LogTemp, Display, TEXT("%s"), *WoRDefeat::GetDefeatString(World)); }));
