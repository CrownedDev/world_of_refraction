// LoadoutV2Debug.cpp

#include "Debug/UI/LoadoutV2Debug.h"
#include "UI/Menus/LoadoutV2Model.h"
#include "Loadout/LoadoutComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float LOADOUT_V2_SCREEN_SECONDS = 10.0f;

	FAutoConsoleCommandWithWorld GPrintLoadoutV2Cmd(
		TEXT("wor.Loadout.PrintV2"),
		TEXT("DEBUG (B-476): per character, the v2 loadout screen's Weapons tab as its model builds it."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { LoadoutV2Debug::PrintWeaponsPages(World); }));
}

void LoadoutV2Debug::PrintWeaponsPages(UWorld *World)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const ULoadoutComponent *Loadout = It->FindComponentByClass<ULoadoutComponent>();
		if (!Loadout)
		{
			continue;
		}
		const FString Line = FString::Printf(TEXT("%s: %s"), *GetNameSafe(*It),
											 *WoRLoadoutV2::GetWeaponsPageString(WoRLoadoutV2::BuildWeaponsPage(Loadout->GetActiveLoadout(), Loadout->GetCharacterClass())));
		UE_LOG(LogWoRUI, Display, TEXT("[LoadoutV2] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, LOADOUT_V2_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}
}
