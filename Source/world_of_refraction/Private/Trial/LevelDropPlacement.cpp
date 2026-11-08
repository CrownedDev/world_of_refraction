// LevelDropPlacement.cpp

#include "Trial/LevelDropPlacement.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/TrialRunSubsystem.h"

#define LOCTEXT_NAMESPACE "LevelDrops"

namespace
{
	UTrialRunSubsystem *LevelDropRun(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	}

	int32 AssetLevel(const UCharacterData &Member, EWorldPillar Pillar)
	{
		switch (Pillar)
		{
		case EWorldPillar::Body:   return Member.WorldBodyLevel;
		case EWorldPillar::Spirit: return Member.WorldSpiritLevel;
		case EWorldPillar::Mind:
		default:                   return Member.WorldMindLevel;
		}
	}
}

FText WoRLevelDrops::GetPillarName(EWorldPillar Pillar)
{
	switch (Pillar)
	{
	case EWorldPillar::Body:   return LOCTEXT("Body", "Body");
	case EWorldPillar::Spirit: return LOCTEXT("Spirit", "Spirit");
	case EWorldPillar::Mind:
	default:                   return LOCTEXT("Mind", "Mind");
	}
}

UCharacterDataComponent *WoRLevelDrops::FindSpawnedMember(const UWorld *World, const FPartyMember &Member)
{
	const FName MemberKey = UPoolSubsystem::GetCharacterKey(Member);
	if (!World || MemberKey.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(const_cast<UWorld *>(World)); It; ++It)
	{
		UCharacterDataComponent *Comp = It->FindComponentByClass<UCharacterDataComponent>();
		if (Comp && UPoolSubsystem::GetCharacterKey(Comp) == MemberKey)
		{
			return Comp;
		}
	}
	return nullptr;
}

int32 WoRLevelDrops::GetLiveLevel(const UWorld *World, const FPartyMember &Member, EWorldPillar Pillar)
{
	if (!Member.CharacterData)
	{
		return 0;
	}
	if (const UCharacterDataComponent *Comp = FindSpawnedMember(World, Member))
	{
		return Comp->GetWorldStat(Pillar);
	}
	const UTrialRunSubsystem *Run = LevelDropRun(World);
	const int32 Earned = Run ? Run->GetRunEarnedLevels(UPoolSubsystem::GetCharacterKey(Member), Pillar) : 0;
	// B-348b: the member's saved head-start is part of its live level too (UCharacterDataComponent::SeedWorldStats).
	const int32 HeadStart = Pillar == EWorldPillar::Mind ? Member.HeadStartMind : Pillar == EWorldPillar::Body ? Member.HeadStartBody : Member.HeadStartSpirit;
	return FMath::Min(AssetLevel(*Member.CharacterData, Pillar) + HeadStart + Earned, WorldStatConstants::LIVE_MAX);
}

FText WoRLevelDrops::TryPlaceLevel(UWorld *World, const FPartyMember &Member, EWorldPillar Pillar)
{
	UTrialRunSubsystem *Run = LevelDropRun(World);
	if (!Run || !Run->IsRunActive() || !Member.CharacterData)
	{
		return LOCTEXT("NoRun", "No run to place a level in");
	}
	const FName MemberKey = UPoolSubsystem::GetCharacterKey(Member);
	const FText Name = FText::FromString(Member.CharacterData->Name);
	// B-223b: a character's levels go only to its own pillars.
	if (Run->GetUnspentLevels(MemberKey) <= 0)
	{
		return FText::Format(LOCTEXT("NoneWaiting", "{0} has no pillar levels to place"), Name);
	}
	if (GetLiveLevel(World, Member, Pillar) >= WorldStatConstants::LIVE_MAX)
	{
		return FText::Format(LOCTEXT("AtCap", "{0}'s {1} is already at {2}"), Name, GetPillarName(Pillar),
							 FText::AsNumber(WorldStatConstants::LIVE_MAX));
	}
	// A spawned member records into the run itself (AddEarnedWorldStat); one not in the level reads it at its next spawn.
	if (UCharacterDataComponent *Comp = FindSpawnedMember(World, Member))
	{
		Comp->AddEarnedWorldStat(Pillar, 1);
	}
	else
	{
		Run->AddRunEarnedLevels(MemberKey, Pillar, 1);
	}
	Run->TakeUnspentLevel(MemberKey);
	UE_LOG(LogTemp, Log, TEXT("[LevelDrops] %s +1 %s (now %d; %d left to place)"), *MemberKey.ToString(), *GetPillarName(Pillar).ToString(),
		   GetLiveLevel(World, Member, Pillar), Run->GetUnspentLevels(MemberKey));
	return FText::GetEmpty();
}

FString WoRLevelDrops::GetLevelDropString(const UWorld *World)
{
	const UTrialRunSubsystem *Run = LevelDropRun(World);
	FString Out = Run ? Run->GetRunLevelsString() : TEXT("[TrialRun] no run subsystem");
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	if (!Party)
	{
		return Out;
	}
	for (const FPartyMember &Member : Party->Members)
	{
		if (Member.CharacterData)
		{
			Out += FString::Printf(TEXT("\n  live %s: Mind %d, Body %d, Spirit %d%s"), *Member.CharacterData->Name,
								   GetLiveLevel(World, Member, EWorldPillar::Mind),
								   GetLiveLevel(World, Member, EWorldPillar::Body),
								   GetLiveLevel(World, Member, EWorldPillar::Spirit),
								   FindSpawnedMember(World, Member) ? TEXT("") : TEXT(" (not spawned: no head-start)"));
		}
	}
	return Out;
}

static FAutoConsoleCommandWithWorld GWorldStatPrintCommand(
	TEXT("wor.WorldStat.Print"),
	TEXT("B-223: the run's pillar levels (earned, bought, waiting to be placed) and each party member's live pillars."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UE_LOG(LogTemp, Display, TEXT("%s"), *WoRLevelDrops::GetLevelDropString(World)); }));

#undef LOCTEXT_NAMESPACE
