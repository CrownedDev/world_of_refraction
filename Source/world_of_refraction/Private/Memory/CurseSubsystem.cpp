// CurseSubsystem.cpp

#include "Memory/CurseSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
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
	UE_LOG(LogWoR, Log, TEXT("[Curse] %s now held (%d)"), *CurseId.ToString(), HeldCurses.Num());
	return true;
}

bool UCurseSubsystem::RemoveCurse(FName CurseId)
{
	if (HeldCurses.Remove(CurseId) == 0)
	{
		return false;
	}
	UE_LOG(LogWoR, Log, TEXT("[Curse] %s lifted (%d left)"), *CurseId.ToString(), HeldCurses.Num());
	return true;
}

int32 UCurseSubsystem::ApplyRewards(const UMemoryData &Memory)
{
	int32 Changed = 0;
	for (const FMemoryReward &Reward : Memory.Rewards)
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
	return Saved;
}

void UCurseSubsystem::Restore(const FSavedCurses &Saved)
{
	HeldCurses.Reset();
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
	if (HeldCurses.IsEmpty())
	{
		return TEXT("no curses held");
	}
	TArray<FString> Names;
	for (const FName &CurseId : HeldCurses)
	{
		Names.Add(CurseId.ToString());
	}
	Names.Sort();
	return FString::Printf(TEXT("curses held: %s"), *FString::Join(Names, TEXT(", ")));
}

#if !UE_BUILD_SHIPPING
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
