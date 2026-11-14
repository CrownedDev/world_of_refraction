// MemoryJournalSubsystem.cpp

#include "Memory/MemoryJournalSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "MemoryJournalSubsystem"

bool UMemoryJournalSubsystem::RecordMemory(UMemoryData *Memory, EMemorySource Source)
{
	if (!Memory || Memories.Contains(Memory))
	{
		return false;
	}
	Memories.Add(Memory);
	Sources.Add(Source);
	Places.Add(GetCurrentPlace(GetGameInstance()));
	NewEntries.Add(Memory->GetMemoryId());
	UE_LOG(LogWoR, Log, TEXT("[MemoryJournal] Recorded %s from %s (%d in the journal)"), *Memory->GetMemoryId().ToString(),
		   *UEnum::GetDisplayValueAsText(Source).ToString(), Memories.Num());
	OnMemoryRecorded.Broadcast(Memory, Source); // B-377: hidden quests start here
	return true;
}

FSavedJournal UMemoryJournalSubsystem::Capture() const
{
	FSavedJournal Saved;
	for (int32 Index = 0; Index < Memories.Num(); ++Index)
	{
		if (Memories[Index])
		{
			Saved.Memories.Add(FSavedAssetRef::Make(Memories[Index]));
			Saved.Sources.Add(static_cast<uint8>(Sources.IsValidIndex(Index) ? Sources[Index] : EMemorySource::Fragment));
			Saved.MemoryPlaces.Add(Places.IsValidIndex(Index) ? Places[Index] : FText::GetEmpty());
		}
	}
	for (const FJournalLoreEntry &Entry : Lore)
	{
		FSavedLore &Out = Saved.Lore.AddDefaulted_GetRef();
		Out.LoreId = Entry.LoreId;
		Out.Title = Entry.Title;
		Out.Body = Entry.Body;
		Out.Place = Entry.Place;
		Out.Kind = static_cast<uint8>(Entry.Kind);
	}
	Saved.NewEntries = NewEntries.Array();
	Saved.HeardVLines = HeardVLines.Array();
	Saved.PlayedConversations = PlayedConversations;
	return Saved;
}

void UMemoryJournalSubsystem::Restore(const FSavedJournal &Saved)
{
	Memories.Reset();
	Sources.Reset();
	Places.Reset();
	for (int32 Index = 0; Index < Saved.Memories.Num(); ++Index)
	{
		UMemoryData *Memory = Saved.Memories[Index].ResolveAs<UMemoryData>();
		if (!Memory)
		{
			UE_LOG(LogWoR, Warning, TEXT("[MemoryJournal] a saved memory no longer resolves - dropped"));
			continue;
		}
		Memories.Add(Memory);
		const uint8 Source = Saved.Sources.IsValidIndex(Index) ? Saved.Sources[Index] : 0;
		Sources.Add(static_cast<EMemorySource>(Source));
		Places.Add(Saved.MemoryPlaces.IsValidIndex(Index) ? Saved.MemoryPlaces[Index] : FText::GetEmpty()); // empty before B-463c
	}
	Lore.Reset();
	for (const FSavedLore &Entry : Saved.Lore)
	{
		FJournalLoreEntry &Kept = Lore.AddDefaulted_GetRef();
		Kept.LoreId = Entry.LoreId;
		Kept.Title = Entry.Title;
		Kept.Body = Entry.Body;
		Kept.Place = Entry.Place;
		Kept.Kind = StaticEnum<ELoreKind>()->IsValidEnumValue(Entry.Kind) ? static_cast<ELoreKind>(Entry.Kind) : ELoreKind::Inscription;
	}
	NewEntries.Reset();
	NewEntries.Append(Saved.NewEntries); // a save from before B-463c has none: nothing shows NEW
	HeardVLines.Reset();
	HeardVLines.Append(Saved.HeardVLines);
	PlayedConversations = Saved.PlayedConversations;
	UE_LOG(LogWoR, Log, TEXT("[MemoryJournal] Restored %d memory(ies), %d lore entr(ies)"), Memories.Num(), Lore.Num());
}

FText UMemoryJournalSubsystem::GetPlace(const UMemoryData *Memory) const
{
	const int32 Index = Memories.IndexOfByKey(Memory);
	return Places.IsValidIndex(Index) ? Places[Index] : FText::GetEmpty();
}

FText UMemoryJournalSubsystem::GetCurrentPlace(const UGameInstance *GameInstance)
{
	const UTrialRunSubsystem *Run = GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	const UTrialData *Trial = Run && Run->IsRunActive() ? Run->GetActiveTrial() : nullptr;
	if (!Trial)
	{
		return LOCTEXT("HubPlace", "Void Realm"); // the hub's name (the game menu's header, PauseMenuModel)
	}
	const FText TrialName = Trial->Name.IsEmpty() ? LOCTEXT("TrialPlace", "a trial") : Trial->Name;
	return Trial->Variations.IsValidIndex(Run->GetVariationIndex())
			   ? FText::Format(LOCTEXT("TrialAndVariation", "{0}, {1}"), TrialName, Trial->Variations[Run->GetVariationIndex()].Name)
			   : TrialName;
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

bool UMemoryJournalSubsystem::RecordLore(FName LoreId, const FText &Title, const FText &Body, ELoreKind Kind)
{
	if (Lore.ContainsByPredicate([LoreId](const FJournalLoreEntry &Entry) { return Entry.LoreId == LoreId; }))
	{
		return false;
	}
	Lore.Add({LoreId, Title, Body, GetCurrentPlace(GetGameInstance()), Kind});
	NewEntries.Add(LoreId);
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
		// B-209: each memory with unlock conditions, met or not for the party now.
		const FMemoryEvent Now = MemoryConditions::MakeEvent(GameInstance, EMemorySource::Fragment);
		for (const UMemoryData *Memory : MemoryConditions::GatherConditional())
		{
			UE_LOG(LogWoR, Display, TEXT("  Unlock %s"), *MemoryConditions::GetConditionsString(*Memory, Now, Journal));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
