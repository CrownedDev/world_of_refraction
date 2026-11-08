// HiddenQuestData.cpp

#include "Memory/HiddenQuestData.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Conditions/WoRConditions.h"
#include "WoRLog.h"

FName UHiddenQuestData::GetQuestId() const
{
	return QuestId.IsNone() ? GetFName() : QuestId;
}

FText UHiddenQuestData::GetDisplayTitle() const
{
	return Title.IsEmpty() ? FText::FromName(GetQuestId()) : Title;
}

FText UHiddenQuestData::GetGoalAfter(int32 StepsDone) const
{
	return Steps.IsValidIndex(StepsDone) ? Steps[StepsDone].Goal : FText::GetEmpty();
}

bool UHiddenQuestData::StartsFrom(const UMemoryData *Memory, EMemoryDepth Depth) const
{
	return Memory && Starts.ContainsByPredicate([Memory, Depth](const FHiddenQuestStart &Start)
	{
		return Start.Memory == Memory && (Start.Depth == EMemoryDepth::None || Start.Depth == Depth);
	});
}

TArray<UHiddenQuestData *> UHiddenQuestData::GatherAll()
{
	const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UHiddenQuestData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
	Assets.Sort([](const FAssetData &A, const FAssetData &B) { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });
	TArray<UHiddenQuestData *> Quests;
	for (const FAssetData &Asset : Assets)
	{
		if (UHiddenQuestData *Quest = Cast<UHiddenQuestData>(Asset.GetAsset()))
		{
			Quests.Add(Quest);
		}
	}
	return Quests;
}

FString UHiddenQuestData::GetQuestString() const
{
	TArray<FString> StartText;
	for (const FHiddenQuestStart &Start : Starts)
	{
		StartText.Add(FString::Printf(TEXT("%s (%s)"), Start.Memory ? *Start.Memory->GetMemoryId().ToString() : TEXT("<none>"),
									  *UEnum::GetDisplayValueAsText(Start.Depth).ToString()));
	}
	TArray<FString> StepText;
	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		const FHiddenQuestStep &Step = Steps[Index];
		StepText.Add(FString::Printf(TEXT("%d: %s x%d"), Index + 1,
									 *FString::JoinBy(Step.Conditions.Conditions, TEXT(" + "), [](const FWoRCondition &Condition)
									 {
										 return WoRConditions::GetConditionString(Condition);
									 }),
									 Step.TimesInOneRun) +
					 (Step.Goal.IsEmpty() ? FString() : FString::Printf(TEXT(" (goal: %s)"), *Step.Goal.ToString())));
	}
	const FString KindText = IsVisible() ? FString::Printf(TEXT(" [%s \"%s\"]"), *UEnum::GetDisplayValueAsText(Kind).ToString(),
														   *GetDisplayTitle().ToString())
										 : FString();
	return FString::Printf(TEXT("%s%s: starts [%s]; steps [%s]; ends %s"), *GetQuestId().ToString(), *KindText, *FString::Join(StartText, TEXT(", ")),
						   *FString::Join(StepText, TEXT("; ")), EndMemory ? *EndMemory->GetMemoryId().ToString() : TEXT("<none>"));
}

void UHiddenQuestData::PrintQuest() const
{
	UE_LOG(LogWoR, Display, TEXT("[HiddenQuest] %s"), *GetQuestString());
}
