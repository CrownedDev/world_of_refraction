// HiddenQuestData.cpp

#include "Memory/HiddenQuestData.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Conditions/WoRConditions.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "HiddenQuestData"

/** B-397: shown in place of a hidden quest's text not written yet. TODO(Crown): content. */
namespace HiddenQuestPlaceholder
{
	static FText Title() { return LOCTEXT("UntitledHiddenQuest", "Unnamed hidden quest"); }
	static FText Goal() { return LOCTEXT("UnwrittenGoal", "The way on is not yet clear."); }
}

FName UHiddenQuestData::GetQuestId() const
{
	return QuestId.IsNone() ? GetFName() : QuestId;
}

FText UHiddenQuestData::GetDisplayTitle() const
{
	if (!Title.IsEmpty())
	{
		return Title;
	}
	return IsVisible() ? FText::FromName(GetQuestId()) : HiddenQuestPlaceholder::Title();
}

FText UHiddenQuestData::GetGoalAfter(int32 StepsDone) const
{
	if (!Steps.IsValidIndex(StepsDone))
	{
		return FText::GetEmpty();
	}
	const FText &Goal = Steps[StepsDone].Goal;
	return Goal.IsEmpty() && !IsVisible() ? HiddenQuestPlaceholder::Goal() : Goal;
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

bool UHiddenQuestData::IsCatalogueReady()
{
	return !FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().IsLoadingAssets();
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
	// B-397: a hidden quest has a shown title too, once found.
	const FString KindText = FString::Printf(TEXT(" [%s \"%s\"]"), *UEnum::GetDisplayValueAsText(Kind).ToString(), *GetDisplayTitle().ToString());
	return FString::Printf(TEXT("%s%s: starts [%s]; steps [%s]; ends %s"), *GetQuestId().ToString(), *KindText, *FString::Join(StartText, TEXT(", ")),
						   *FString::Join(StepText, TEXT("; ")), EndMemory ? *EndMemory->GetMemoryId().ToString() : TEXT("<none>"));
}

void UHiddenQuestData::PrintQuest() const
{
	UE_LOG(LogWoR, Display, TEXT("[HiddenQuest] %s"), *GetQuestString());
}

#if WITH_EDITOR
EDataValidationResult UHiddenQuestData::IsDataValid(FDataValidationContext &Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);
	if (IsVisible())
	{
		return Result;
	}
	if (Title.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("NoTitleWarning", "Hidden quest has no Title: once found it shows placeholder text (D-171)."));
	}
	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		if (Steps[Index].Goal.IsEmpty())
		{
			Context.AddWarning(FText::Format(LOCTEXT("NoGoalWarning", "Hidden quest step {0} has no Goal: it shows placeholder text (D-171)."),
											 FText::AsNumber(Index + 1)));
		}
	}
	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
