// NpcTalkDebug.cpp

#include "Debug/UI/NpcTalkDebug.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Merchant/MerchantInteractable.h"
#include "Story/StoryLinesData.h"
#include "UI/Hub/NpcTalkModel.h"
#include "WoRLog.h"

namespace
{
	constexpr float NPC_TALK_SCREEN_SECONDS = 10.0f;

	FAutoConsoleCommandWithWorld GPrintNpcTalkCmd(
		TEXT("wor.Talk.Print"),
		TEXT("DEBUG (B-471c): the talk box every vendor in the level would open (role, Shop / Talk / Leave, V's topics, NEW, the log)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { NpcTalkDebug::PrintTalkPages(World); }));
}

void NpcTalkDebug::PrintTalkPages(UWorld *World)
{
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UMemoryJournalSubsystem *Journal = GI ? GI->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
	const TArray<UStoryLinesData *> Story = StoryUnlocks::GatherStoryData();
	int32 Vendors = 0;
	for (TActorIterator<AMerchantInteractable> It(World); It; ++It)
	{
		++Vendors;
		const FString Line = FString::Printf(TEXT("%s: %s"), *It->GetName(), *WoRNpcTalk::GetTalkString(WoRNpcTalk::Build(It->Merchant, Story, Journal)));
		UE_LOG(LogWoRUI, Display, TEXT("[NpcTalk] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, NPC_TALK_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}
	if (Vendors == 0)
	{
		UE_LOG(LogWoRUI, Display, TEXT("[NpcTalk] No vendor in this level (%d story asset(s))."), Story.Num());
	}
}
