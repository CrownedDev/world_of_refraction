// BattleTransitionDebug.cpp

#include "Debug/Combat/BattleTransitionDebug.h"

#include "Combat/Transition/BattleTransitionData.h"
#include "Combat/Transition/BattleTransitionSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "WoRLog.h"

namespace
{
	/** The preview holds as long as an encounter's join window does by default. */
	constexpr float DEFAULT_PREVIEW_HOLD_SECONDS = 2.0f;
}

FString WoRBattleTransitionDebug::GetStateString(const UBattleTransitionSubsystem *Transition)
{
	if (!Transition)
	{
		return TEXT("no battle transition subsystem");
	}
	const BattleTransition::FClock &Clock = Transition->GetClock();
	return FString::Printf(TEXT("%s: %.2fs (blow-away %.2fs), %d shards, %s, %s"), BattleTransition::GetStateName(Clock.State),
						   Clock.Elapsed, Clock.ReleaseElapsed, Transition->GetShardCount(),
						   Transition->HasFrame() ? TEXT("frame captured") : TEXT("no frame"),
						   Transition->HasPicture() ? TEXT("held picture shown") : TEXT("no picture"));
}

FString WoRBattleTransitionDebug::GetDataString(const UBattleTransitionData &Data)
{
	return FString::Printf(
		TEXT("crack %.2fs, break %.2fs (ease %.1f), settle %.2fs, blow away %.2fs (ease %.1f), picture fade %.2fs, max hold %.1fs | %dx%d cells, "
			 "wall %.0f cm, push in %.2f | bloom %.1f, motion blur %.1f (max %.0f%%), f/%.1f | material %s"),
		Data.CrackSeconds, Data.BreakSeconds, Data.BreakEase, Data.ArrivalSettleSeconds, Data.BlowAwaySeconds, Data.BlowAwayEase,
		Data.PictureFadeSeconds, Data.MaxHoldSeconds,
		Data.ShardColumns, Data.ShardRows, Data.WallDistance, Data.PushIn, Data.BloomIntensity, Data.MotionBlurAmount,
		Data.MotionBlurMaxPercent, Data.FocusFstop,
		Data.ShardMaterial.IsNull() ? TEXT("none (plain tinted shards)") : *Data.ShardMaterial.GetAssetName());
}

void WoRBattleTransitionDebug::PlayPreview(const TArray<FString> &Args, UWorld *World)
{
	UBattleTransitionSubsystem *Transition = UBattleTransitionSubsystem::Get(World);
	if (!Transition)
	{
		UE_LOG(LogWoR, Warning, TEXT("[BattleTransition] No game running - start PIE first"));
		return;
	}
	const float HoldSeconds = Args.Num() > 0 ? FCString::Atof(*Args[0]) : DEFAULT_PREVIEW_HOLD_SECONDS;
	const bool bWasIdle = Transition->GetClock().State == BattleTransition::EState::Idle;
	Transition->PlayAtScreen(FVector2D(0.5, 0.5));
	if (!bWasIdle || Transition->GetClock().State != BattleTransition::EState::Playing)
	{
		UE_LOG(LogWoR, Warning, TEXT("[BattleTransition] Preview not started: %s"), *GetStateString(Transition));
		return; // one is already running, or there is no view: don't arm a release for it
	}
	FTimerHandle ReleaseTimer;
	World->GetTimerManager().SetTimer(ReleaseTimer, FTimerDelegate::CreateUObject(Transition, &UBattleTransitionSubsystem::Release),
									  FMath::Max(HoldSeconds, UE_KINDA_SMALL_NUMBER), false);
	UE_LOG(LogWoR, Display, TEXT("[BattleTransition] Preview, holding %.1fs: %s"), HoldSeconds, *GetStateString(Transition));
}

void WoRBattleTransitionDebug::PrintState(UWorld *World)
{
	UE_LOG(LogWoR, Display, TEXT("[BattleTransition] %s"), *GetStateString(UBattleTransitionSubsystem::Get(World)));
	UE_LOG(LogWoR, Display, TEXT("[BattleTransition] %s"), *GetDataString(UBattleTransitionData::Get()));
}

static FAutoConsoleCommandWithWorldAndArgs GBattleTransitionPlayCommand(
	TEXT("wor.Transition.Play"),
	TEXT("B-500 DEBUG: wor.Transition.Play [hold seconds]: plays the battle transition on this map with no fight: the frame cracks "
		 "from the screen centre, breaks into shards, holds (default 2 s), then the shards blow away."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WoRBattleTransitionDebug::PlayPreview));

static FAutoConsoleCommandWithWorld GBattleTransitionStateCommand(
	TEXT("wor.Transition.State"),
	TEXT("B-500 DEBUG: logs the battle transition's state (phase, time, shards, held picture) and the values it plays with."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&WoRBattleTransitionDebug::PrintState));
