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

UCharacterDataComponent *WoRLevelDrops::FindSpawnedMember(const UWorld *World, const UCharacterData *Member)
{
	if (!World || !Member)
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(const_cast<UWorld *>(World)); It; ++It)
	{
		UCharacterDataComponent *Comp = It->FindComponentByClass<UCharacterDataComponent>();
		if (Comp && Comp->CharacterData == Member)
		{
			return Comp;
		}
	}
	return nullptr;
}

UCharacterDataComponent *WoRLevelDrops::FindSpawnedMember(const UWorld *World, const FPartyMember &Member)
{
	if (!World || !Member.CharacterData)
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(const_cast<UWorld *>(World)); It; ++It)
	{
		UCharacterDataComponent *Comp = It->FindComponentByClass<UCharacterDataComponent>();
		if (!Comp || Comp->CharacterData != Member.CharacterData)
		{
			continue;
		}
		const bool bStampedWithThisMember = Comp->PartyMemberId.IsValid() && Comp->PartyMemberId == Member.MemberId;
		const bool bUnstampedFirstCopy = !Comp->PartyMemberId.IsValid() && !Member.bKeyedByMember;
		if (bStampedWithThisMember || bUnstampedFirstCopy)
		{
			return Comp;
		}
	}
	return nullptr;
}

namespace
{
	/** One member's live level in Pillar: the spawned component's, else its asset level plus the run's earned levels
	 *  under Key (head-start lives on the spawned component). */
	int32 LiveLevelOf(const UWorld *World, const UCharacterData &Data, FName Key, const UCharacterDataComponent *Spawned, EWorldPillar Pillar)
	{
		if (Spawned)
		{
			return Spawned->GetWorldStat(Pillar);
		}
		const UTrialRunSubsystem *Run = LevelDropRun(World);
		const int32 Earned = Run ? Run->GetRunEarnedLevels(Key, Pillar) : 0;
		return FMath::Min(AssetLevel(Data, Pillar) + Earned, WorldStatConstants::LIVE_MAX);
	}

	/** Places one unspent level banked under Key on Data's Pillar. Spawned is the member's pawn component, if any. */
	FText PlaceLevelOf(UWorld *World, UCharacterData *Data, FName Key, UCharacterDataComponent *Spawned, EWorldPillar Pillar)
	{
		UTrialRunSubsystem *Run = LevelDropRun(World);
		if (!Run || !Run->IsRunActive() || !Data)
		{
			return LOCTEXT("NoRun", "No run to place a level in");
		}
		const FText Name = FText::FromString(Data->Name);
		// B-223b: a character's levels go only to its own pillars.
		if (Run->GetUnspentLevels(Key) <= 0)
		{
			return FText::Format(LOCTEXT("NoneWaiting", "{0} has no pillar levels to place"), Name);
		}
		if (LiveLevelOf(World, *Data, Key, Spawned, Pillar) >= WorldStatConstants::LIVE_MAX)
		{
			return FText::Format(LOCTEXT("AtCap", "{0}'s {1} is already at {2}"), Name, WoRLevelDrops::GetPillarName(Pillar),
								 FText::AsNumber(WorldStatConstants::LIVE_MAX));
		}
		// A spawned member records into the run itself (AddEarnedWorldStat); one not in the level reads it at its next spawn.
		if (Spawned)
		{
			Spawned->AddEarnedWorldStat(Pillar, 1);
		}
		else
		{
			Run->AddRunEarnedLevels(Key, Pillar, 1);
		}
		Run->TakeUnspentLevel(Key);
		UE_LOG(LogTemp, Log, TEXT("[LevelDrops] %s +1 %s (now %d; %d left to place)"), *Data->Name, *WoRLevelDrops::GetPillarName(Pillar).ToString(),
			   LiveLevelOf(World, *Data, Key, Spawned, Pillar), Run->GetUnspentLevels(Key));
		return FText::GetEmpty();
	}
}

int32 WoRLevelDrops::GetLiveLevel(const UWorld *World, const UCharacterData *Member, EWorldPillar Pillar)
{
	return Member ? LiveLevelOf(World, *Member, UPoolSubsystem::GetCharacterKey(Member), FindSpawnedMember(World, Member), Pillar) : 0;
}

int32 WoRLevelDrops::GetLiveLevel(const UWorld *World, const FPartyMember &Member, EWorldPillar Pillar)
{
	return Member.CharacterData
			   ? LiveLevelOf(World, *Member.CharacterData, UPoolSubsystem::GetCharacterKey(Member), FindSpawnedMember(World, Member), Pillar)
			   : 0;
}

FText WoRLevelDrops::TryPlaceLevel(UWorld *World, UCharacterData *Member, EWorldPillar Pillar)
{
	return PlaceLevelOf(World, Member, UPoolSubsystem::GetCharacterKey(Member), FindSpawnedMember(World, Member), Pillar);
}

FText WoRLevelDrops::TryPlaceLevel(UWorld *World, const FPartyMember &Member, EWorldPillar Pillar)
{
	return PlaceLevelOf(World, Member.CharacterData, UPoolSubsystem::GetCharacterKey(Member), FindSpawnedMember(World, Member), Pillar);
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
			Out += FString::Printf(TEXT("\n  live %s [%s]: Mind %d, Body %d, Spirit %d%s"), *Member.CharacterData->Name,
								   *UPoolSubsystem::GetCharacterKey(Member).ToString(), // B-360c: a copy shows its own key
								   GetLiveLevel(World, Member, EWorldPillar::Mind), GetLiveLevel(World, Member, EWorldPillar::Body),
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
