// MemoryJournalSubsystem.cpp

#include "Memory/MemoryJournalSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryData.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

bool UMemoryJournalSubsystem::RecordMemory(UMemoryData *Memory, EMemorySource Source)
{
	if (!Memory || Memories.Contains(Memory))
	{
		return false;
	}
	Memories.Add(Memory);
	Sources.Add(Source);
	UE_LOG(LogWoR, Log, TEXT("[MemoryJournal] Recorded %s from %s (%d in the journal)"), *Memory->GetMemoryId().ToString(),
		   *UEnum::GetDisplayValueAsText(Source).ToString(), Memories.Num());
	return true;
}

TOptional<EMemorySource> UMemoryJournalSubsystem::GetSource(const UMemoryData *Memory) const
{
	const int32 Index = Memories.IndexOfByKey(Memory);
	return Sources.IsValidIndex(Index) ? TOptional<EMemorySource>(Sources[Index]) : TOptional<EMemorySource>();
}

bool UMemoryJournalSubsystem::HasMemoryId(FName MemoryId) const
{
	return Memories.ContainsByPredicate([MemoryId](const UMemoryData *Memory) { return Memory && Memory->GetMemoryId() == MemoryId; });
}

bool UMemoryJournalSubsystem::RecordLore(FName LoreId, const FText &Title, const FText &Body)
{
	if (Lore.ContainsByPredicate([LoreId](const FJournalLoreEntry &Entry) { return Entry.LoreId == LoreId; }))
	{
		return false;
	}
	Lore.Add({LoreId, Title, Body});
	UE_LOG(LogWoR, Log, TEXT("[MemoryJournal] Lore %s kept (%d in the journal)"), *LoreId.ToString(), Lore.Num());
	return true;
}

FString UMemoryJournalSubsystem::GetJournalString() const
{
	FString Out = FString::Printf(TEXT("Memory journal: %d memories, %d lore, %d V line(s) heard, %d conversation(s) played"),
								  Memories.Num(), Lore.Num(), HeardVLines.Num(), PlayedConversations.Num());
	for (int32 Index = 0; Index < Memories.Num(); ++Index)
	{
		if (const UMemoryData *Memory = Memories[Index])
		{
			const FString Source = Sources.IsValidIndex(Index) ? UEnum::GetDisplayValueAsText(Sources[Index]).ToString() : TEXT("?");
			Out += FString::Printf(TEXT("\n  [%s] "), *Source) + Memory->GetMemoryString();
		}
	}
	for (const FJournalLoreEntry &Entry : Lore)
	{
		Out += FString::Printf(TEXT("\n  Lore %s: %s"), *Entry.LoreId.ToString(), *Entry.Title.ToString());
	}
	if (!PlayedConversations.IsEmpty())
	{
		Out += TEXT("\n  Played: ") + FString::JoinBy(PlayedConversations, TEXT(", "), [](FName Id) { return Id.ToString(); });
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
