// MemoryJournalSubsystem.cpp

#include "Memory/MemoryJournalSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryData.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

bool UMemoryJournalSubsystem::RecordMemory(UMemoryData *Memory)
{
	if (!Memory || Memories.Contains(Memory))
	{
		return false;
	}
	Memories.Add(Memory);
	UE_LOG(LogWoR, Log, TEXT("[MemoryJournal] Recorded %s (%d in the journal)"), *Memory->GetMemoryId().ToString(),
		   Memories.Num());
	return true;
}

FString UMemoryJournalSubsystem::GetJournalString() const
{
	FString Out = FString::Printf(TEXT("Memory journal: %d memories"), Memories.Num());
	for (const UMemoryData *Memory : Memories)
	{
		if (Memory)
		{
			Out += TEXT("\n  ") + Memory->GetMemoryString();
		}
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GMemoryPrintCommand(
	TEXT("wor.Memory.Print"),
	TEXT("Print the memories collected this run and the memory journal, with each memory's character depth (B-190)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
		const UTrialRunSubsystem *TrialRun = GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
		if (!Journal || !TrialRun)
		{
			UE_LOG(LogWoR, Warning, TEXT("[MemoryJournal] wor.Memory.Print: no subsystem (no PIE world?)."));
			return;
		}
		TArray<FString> RunMemories;
		for (const UMemoryData *Memory : TrialRun->GetRunMemories())
		{
			RunMemories.Add(Memory ? Memory->GetMemoryId().ToString() : TEXT("<null>"));
		}
		UE_LOG(LogWoR, Display, TEXT("This run: %d memories [%s]"), RunMemories.Num(), *FString::Join(RunMemories, TEXT(", ")));
		UE_LOG(LogWoR, Display, TEXT("%s"), *Journal->GetJournalString());
	}));
#endif
