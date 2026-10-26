// StoryLinesData.cpp

#include "Story/StoryLinesData.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "WoRLog.h"

FString UStoryLinesData::GetStoryString() const
{
	FString Out = FString::Printf(TEXT("Story %s: %d V line(s), %d conversation(s)"), *GetName(), VLines.Num(), Conversations.Num());
	for (const FVLine &Line : VLines)
	{
		Out += FString::Printf(TEXT("\n  V %s needs [%s]: %s"), *Line.LineId.ToString(),
							   *FString::JoinBy(Line.RequiredMemories, TEXT(", "), [](FName Id) { return Id.ToString(); }),
							   *Line.Text.ToString());
	}
	for (const FStoryConversation &Talk : Conversations)
	{
		Out += FString::Printf(TEXT("\n  Talk %s (%s) needs [%s]: %d line(s), speakers [%s]"), *Talk.ConversationId.ToString(),
							   *Talk.Title.ToString(),
							   *FString::JoinBy(Talk.RequiredMemories, TEXT(", "), [](FName Id) { return Id.ToString(); }),
							   Talk.Lines.Num(),
							   *FString::JoinBy(Talk.Speakers, TEXT(", "), [](const TObjectPtr<UCharacterData> &Speaker) { return GetNameSafe(Speaker); }));
	}
	return Out;
}

void UStoryLinesData::PrintStory() const
{
	UE_LOG(LogWoR, Log, TEXT("%s"), *GetStoryString());
}

TArray<UStoryLinesData *> StoryUnlocks::GatherStoryData()
{
	const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UStoryLinesData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
	Assets.Sort([](const FAssetData &A, const FAssetData &B) { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });

	TArray<UStoryLinesData *> Story;
	for (const FAssetData &Asset : Assets)
	{
		if (UStoryLinesData *Data = Cast<UStoryLinesData>(Asset.GetAsset()))
		{
			Story.Add(Data);
		}
	}
	return Story;
}

bool StoryUnlocks::AreUnlocked(const TArray<FName> &RequiredMemories, const UMemoryJournalSubsystem &Journal)
{
	for (const FName Id : RequiredMemories)
	{
		if (!Journal.HasMemoryId(Id))
		{
			return false;
		}
	}
	return true;
}

const FVLine *StoryUnlocks::FindNextVLine(const TArray<UStoryLinesData *> &Story, const UMemoryJournalSubsystem &Journal)
{
	// Pointers into the assets: iterate their arrays directly, never a copy.
	for (const UStoryLinesData *Data : Story)
	{
		if (!Data)
		{
			continue;
		}
		for (const FVLine &Line : Data->VLines)
		{
			if (!Journal.HasHeardVLine(Line.LineId) && AreUnlocked(Line.RequiredMemories, Journal))
			{
				return &Line;
			}
		}
	}
	return nullptr;
}

TArray<const FStoryConversation *> StoryUnlocks::GetUnlockedConversations(const TArray<UStoryLinesData *> &Story,
																		 const UMemoryJournalSubsystem &Journal)
{
	TArray<const FStoryConversation *> Unlocked;
	for (const UStoryLinesData *Data : Story)
	{
		if (!Data)
		{
			continue;
		}
		for (const FStoryConversation &Talk : Data->Conversations)
		{
			if (AreUnlocked(Talk.RequiredMemories, Journal))
			{
				Unlocked.Add(&Talk);
			}
		}
	}
	return Unlocked;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GStoryPrintCommand(
	TEXT("wor.Story.Print"),
	TEXT("Print every V line and conversation, and which are unlocked by the memory journal now (B-194)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const TArray<UStoryLinesData *> Story = StoryUnlocks::GatherStoryData();
		for (const UStoryLinesData *Data : Story)
		{
			UE_LOG(LogWoR, Display, TEXT("%s"), *Data->GetStoryString());
		}
		UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
		if (!Journal)
		{
			UE_LOG(LogWoR, Display, TEXT("%d story asset(s); no journal (no PIE world?)."), Story.Num());
			return;
		}
		const FVLine *Next = StoryUnlocks::FindNextVLine(Story, *Journal);
		UE_LOG(LogWoR, Display, TEXT("%d story asset(s). V's next line: %s. Unlocked conversations: %d"), Story.Num(),
			   Next ? *Next->LineId.ToString() : TEXT("none"), StoryUnlocks::GetUnlockedConversations(Story, *Journal).Num());
	}));
#endif
