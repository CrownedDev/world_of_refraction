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
