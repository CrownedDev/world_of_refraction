// CharacterFormDebug.cpp

#include "Debug/Character/CharacterFormDebug.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float FORMS_SCREEN_SECONDS = 8.0f;

	FAutoConsoleCommandWithWorld GPrintFormsCmd(
		TEXT("wor.Forms.Print"),
		TEXT("DEBUG: print every character form and its loadout-match meter."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				UCharacterFormDebug::PrintForms(World);
			}));

	UCharacterFormSubsystem *GetFormsFor(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UCharacterFormSubsystem>() : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GUnlockFormCmd(
		TEXT("wor.Forms.Unlock"),
		TEXT("DEBUG (B-279b): fill a form's meter to 100%. Usage: wor.Forms.Unlock <FormAssetName>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				UCharacterFormSubsystem *Forms = GetFormsFor(World);
				const UCharacterFormData *Form = Forms && Args.Num() > 0 ? Forms->FindFormByName(FName(*Args[0])) : nullptr;
				if (!Form)
				{
					UE_LOG(LogWoR, Warning, TEXT("[CharacterForms] wor.Forms.Unlock: no form named '%s'"), Args.Num() > 0 ? *Args[0] : TEXT(""));
					return;
				}
				TMap<FName, float> Progress = Forms->GetProgressByName();
				Progress.Add(Form->GetFName(), CharacterFormMeter::FULL);
				Forms->RestoreProgress(Progress);
				UCharacterFormDebug::PrintForms(World);
			}));

	FAutoConsoleCommandWithWorldAndArgs GAtRestSpotCmd(
		TEXT("wor.Forms.AtRestSpot"),
		TEXT("DEBUG (B-279b): pretend the player is (1) or isn't (0) at a rest spot, until B-187's rest spots exist."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				if (UCharacterFormSubsystem *Forms = GetFormsFor(World))
				{
					const bool bAtRestSpot = Args.Num() == 0 || Args[0] != TEXT("0");
					Forms->SetAtRestSpot(bAtRestSpot);
					UE_LOG(LogWoR, Display, TEXT("[CharacterForms] at a rest spot: %s"), bAtRestSpot ? TEXT("yes") : TEXT("no"));
				}
			}));
}

FString UCharacterFormDebug::GetFormsString(const UObject *WorldContextObject)
{
	const UWorld *World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UCharacterFormSubsystem *Forms = GI ? GI->GetSubsystem<UCharacterFormSubsystem>() : nullptr;
	return Forms ? Forms->GetFormsString() : TEXT("[CharacterForms] no game instance");
}

void UCharacterFormDebug::PrintForms(const UObject *WorldContextObject)
{
	const FString State = GetFormsString(WorldContextObject);
	UE_LOG(LogWoR, Display, TEXT("%s"), *State);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, FORMS_SCREEN_SECONDS, FColor::Cyan, State);
	}
}
