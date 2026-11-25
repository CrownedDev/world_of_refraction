// HubTrainingDummy.cpp

#include "Hub/HubTrainingDummy.h"

#include "Character/CharacterDataComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Hub/HubHUDModel.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "HubTrainingDummy"

void WoRPractice::MarkFighter(AActor *Fighter, bool bDummy)
{
	UCharacterDataComponent *Data = Fighter ? Fighter->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!Data)
	{
		return;
	}
	Data->bCannotFall = true;
	Data->bTrainingDummy = bDummy;
	UE_LOG(LogWoR, Log, TEXT("[Practice] %s marked: can't fall%s"), *GetNameSafe(Fighter), bDummy ? TEXT(", passes its turns") : TEXT(""));
}

FString WoRPractice::GetPracticeFightString(const UObject *WorldContext)
{
	FString Out = FString::Printf(TEXT("Practice fight: %s"), UTrialRunSubsystem::IsPracticeFight(WorldContext) ? TEXT("yes") : TEXT("no"));
	UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return Out;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const UCharacterDataComponent *Data = It->FindComponentByClass<UCharacterDataComponent>();
		if (Data && Data->bCannotFall)
		{
			Out += FString::Printf(TEXT("\n  %s  HP %d/%d%s"), *It->GetName(), Data->CurrentHP, Data->MaxHP, Data->bTrainingDummy ? TEXT("  [dummy]") : TEXT(""));
		}
	}
	return Out;
}

void AHubTrainingDummy::Interact(APawn *Pawn)
{
	UTrialRunSubsystem *TrialRun = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (!TrialRun || !TrialRun->EnterPracticeEncounter())
	{
		UE_LOG(LogWoR, Warning, TEXT("[TrainingDummy] %s: the practice fight didn't start (see the line above)"), *GetNameSafe(Pawn));
	}
}

FInteractPrompt AHubTrainingDummy::GetPrompt() const
{
	return FInteractPrompt(EInteractVerb::Challenge, LOCTEXT("DummyPromptName", "Training Dummy"));
}

FString AHubTrainingDummy::GetHintText() const
{
	return WoRHubHUD::FormatPromptLine(GetPrompt()).ToString(); // "E  Challenge · Training Dummy"
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GHubTrainingDummyCommand(
    TEXT("wor.Hub.TrainingDummy"),
    TEXT("Start the training dummy's practice fight without walking to it (B-611). In a fight, prints the practice state."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
    {
        UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
        UTrialRunSubsystem *TrialRun = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
        if (TrialRun && TrialRun->IsPracticeFight())
        {
            UE_LOG(LogWoR, Display, TEXT("%s"), *WoRPractice::GetPracticeFightString(World));
            return;
        }
        if (!TrialRun || !TrialRun->EnterPracticeEncounter())
        {
            UE_LOG(LogWoR, Warning, TEXT("[TrainingDummy] wor.Hub.TrainingDummy: the practice fight didn't start"));
        }
    }));
#endif

#undef LOCTEXT_NAMESPACE
