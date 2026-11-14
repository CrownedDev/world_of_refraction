// ApparelBonusLineDebug.cpp

#include "Debug/Equipment/ApparelBonusLineDebug.h"
#include "Equipment/Apparel/ApparelBonusLine.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Pool/PoolSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float APPAREL_BONUS_SCREEN_SECONDS = 10.0f;

	FAutoConsoleCommandWithWorld GPrintApparelBonusLinesCmd(
		TEXT("wor.Apparel.PrintBonusLines"),
		TEXT("DEBUG (B-475): every owned apparel piece and its rolled bonus line (D-208)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { ApparelBonusLineDebug::PrintBonusLines(World); }));
}

FString ApparelBonusLineDebug::GetBonusLineString(const FApparelEntry &Entry)
{
	const FText Line = ApparelBonusLine::GetText(Entry.BonusLine);
	return FString::Printf(TEXT("%s (%s) %s: %s"), Entry.Apparel ? *Entry.Apparel->GetDisplayName() : TEXT("-"), *UEnum::GetValueAsString(Entry.Tier),
						   *Entry.PersistentID.ToString(), Line.IsEmpty() ? TEXT("no bonus line") : *Line.ToString());
}

void ApparelBonusLineDebug::PrintBonusLines(UWorld *World)
{
	UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
	const UPoolSubsystem *Pool = GameInstance ? GameInstance->GetSubsystem<UPoolSubsystem>() : nullptr;
	if (!Pool)
	{
		UE_LOG(LogWoR, Warning, TEXT("[ApparelBonusLine] no pool subsystem"));
		return;
	}
	UE_LOG(LogWoR, Display, TEXT("[ApparelBonusLine] %d owned apparel piece(s)"), Pool->GetOwnedApparel().Num());
	for (const FApparelEntry &Entry : Pool->GetOwnedApparel())
	{
		const FString Line = GetBonusLineString(Entry);
		UE_LOG(LogWoR, Display, TEXT("[ApparelBonusLine] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, APPAREL_BONUS_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}
}
