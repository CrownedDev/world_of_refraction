// CurseSubsystem.cpp

#include "Memory/CurseSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "WoRLog.h"

void UCurseSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	if (UMemoryJournalSubsystem *Journal = Collection.InitializeDependency<UMemoryJournalSubsystem>())
	{
		Journal->OnMemoryRecorded.AddDynamic(this, &UCurseSubsystem::HandleMemoryRecorded);
	}
}

void UCurseSubsystem::Deinitialize()
{
	if (UMemoryJournalSubsystem *Journal = GetGameInstance()->GetSubsystem<UMemoryJournalSubsystem>())
	{
		Journal->OnMemoryRecorded.RemoveAll(this);
	}
	Super::Deinitialize();
}

bool UCurseSubsystem::AddCurse(FName CurseId)
{
	if (CurseId.IsNone() || HeldCurses.Contains(CurseId))
	{
		return false;
	}
	HeldCurses.Add(CurseId);
	UE_LOG(LogWoR, Log, TEXT("[Curse] %s now held%s (%d)"), *CurseId.ToString(), LiftedCurses.Contains(CurseId) ? TEXT(" again") : TEXT(""),
		   HeldCurses.Num());
	return true;
}

bool UCurseSubsystem::RemoveCurse(FName CurseId)
{
	if (HeldCurses.Remove(CurseId) == 0)
	{
		return false;
	}
	LiftedCurses.Add(CurseId);
	LiftedThisEvent.Add(CurseId);
	UE_LOG(LogWoR, Log, TEXT("[Curse] %s lifted (%d left)"), *CurseId.ToString(), HeldCurses.Num());
	return true;
}

bool UCurseSubsystem::ClearCurse(FName CurseId)
{
	if (!RemoveCurse(CurseId))
	{
		return false;
	}
	UE_LOG(LogWoR, Log, TEXT("[Curse] %s cleared by the player (it comes back if its giver's conditions are met again)"),
		   *CurseId.ToString());
	return true;
}

int32 UCurseSubsystem::ApplyRewards(const TArray<FMemoryReward> &Rewards)
{
	int32 Changed = 0;
	for (const FMemoryReward &Reward : Rewards)
	{
		if (Reward.Kind == EMemoryRewardKind::Curse)
		{
			Changed += AddCurse(Reward.RewardId) ? 1 : 0;
		}
		else if (Reward.Kind == EMemoryRewardKind::RemoveCurse)
		{
			Changed += RemoveCurse(Reward.RewardId) ? 1 : 0;
		}
	}
	return Changed;
}

int32 UCurseSubsystem::RegainFrom(const TArray<FMemoryReward> &Rewards)
{
	int32 Regained = 0;
	for (const FMemoryReward &Reward : Rewards)
	{
		if (Reward.Kind == EMemoryRewardKind::Curse && !LiftedThisEvent.Contains(Reward.RewardId) && AddCurse(Reward.RewardId))
		{
			++Regained;
		}
	}
	return Regained;
}

bool UCurseSubsystem::CouldRegain(const TArray<FMemoryReward> &Rewards) const
{
	return Rewards.ContainsByPredicate([this](const FMemoryReward &Reward)
	{
		return Reward.Kind == EMemoryRewardKind::Curse && !Reward.RewardId.IsNone() && !HeldCurses.Contains(Reward.RewardId) &&
			   !LiftedThisEvent.Contains(Reward.RewardId);
	});
}

int32 UCurseSubsystem::CheckRegain(const FMemoryEvent &Event, const TArray<UMemoryData *> &Candidates)
{
	const UMemoryJournalSubsystem *Journal = GetGameInstance()->GetSubsystem<UMemoryJournalSubsystem>();
	int32 Regained = 0;
	for (const UMemoryData *Memory : Candidates)
	{
		if (Memory && Journal && Journal->HasMemory(Memory) && CouldRegain(Memory->Rewards) && MemoryConditions::AreMet(*Memory, Event, Journal))
		{
			const int32 Back = RegainFrom(Memory->Rewards);
			Regained += Back;
			UE_LOG(LogWoR, Log, TEXT("[Curse] %s met again: %d curse(s) back"), *Memory->GetMemoryId().ToString(), Back);
		}
	}
	return Regained;
}

void UCurseSubsystem::HandleMemoryRecorded(UMemoryData *Memory, EMemorySource /*Source*/)
{
	if (Memory)
	{
		ApplyRewards(*Memory);
	}
}

FSavedCurses UCurseSubsystem::Capture() const
{
	FSavedCurses Saved;
	Saved.Held = HeldCurses.Array();
	Saved.Held.Sort([](const FName &A, const FName &B) { return A.LexicalLess(B); });
	Saved.Lifted = LiftedCurses.Array();
	Saved.Lifted.Sort([](const FName &A, const FName &B) { return A.LexicalLess(B); });
	return Saved;
}

void UCurseSubsystem::Restore(const FSavedCurses &Saved)
{
	HeldCurses.Reset();
	LiftedCurses.Reset();
	LiftedCurses.Append(Saved.Lifted);
	for (const FName &CurseId : Saved.Held)
	{
		if (!CurseId.IsNone())
		{
			HeldCurses.Add(CurseId);
		}
	}
}

FString UCurseSubsystem::GetCursesString() const
{
	TArray<FString> Lifted;
	for (const FName &CurseId : LiftedCurses)
	{
		if (!HeldCurses.Contains(CurseId))
		{
			Lifted.Add(CurseId.ToString());
		}
	}
	Lifted.Sort();
	const FString LiftedText = Lifted.IsEmpty() ? FString() : FString::Printf(TEXT("; lifted or cleared (can come back): %s"), *FString::Join(Lifted, TEXT(", ")));
	if (HeldCurses.IsEmpty())
	{
		return TEXT("no curses held") + LiftedText;
	}
	TArray<FString> Names;
	for (const FName &CurseId : HeldCurses)
	{
		Names.Add(CurseId.ToString());
	}
	Names.Sort();
	return FString::Printf(TEXT("curses held: %s"), *FString::Join(Names, TEXT(", "))) + LiftedText;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GCurseClearCommand(
	TEXT("wor.Curse.Clear"),
	TEXT("Q-378: clear a held curse as the player would (wor.Curse.Clear <curse id>)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		UCurseSubsystem *Curses = GameInstance ? GameInstance->GetSubsystem<UCurseSubsystem>() : nullptr;
		if (!Curses || Args.IsEmpty())
		{
			UE_LOG(LogWoR, Warning, TEXT("[Curse] usage: wor.Curse.Clear <curse id>"));
			return;
		}
		const bool bCleared = Curses->ClearCurse(FName(*Args[0]));
		UE_LOG(LogWoR, Display, TEXT("[Curse] %s: %s"), *Args[0], bCleared ? TEXT("cleared") : TEXT("not held"));
	}));

static FAutoConsoleCommandWithWorld GCursePrintCommand(
	TEXT("wor.Curse.Print"),
	TEXT("B-378: the curses this player holds."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const UCurseSubsystem *Curses = GameInstance ? GameInstance->GetSubsystem<UCurseSubsystem>() : nullptr;
		UE_LOG(LogWoR, Display, TEXT("[Curse] %s"), Curses ? *Curses->GetCursesString() : TEXT("no curse subsystem"));
	}));
#endif
