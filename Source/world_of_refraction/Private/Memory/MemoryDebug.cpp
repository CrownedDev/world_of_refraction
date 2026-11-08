// MemoryDebug.cpp

#include "Memory/MemoryDebug.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Character/CharacterData.h"
#include "Conditions/WoRConditions.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Memory/MemoryUnlock.h"
#include "Player/WoRPlayerState.h"
#include "WoRLog.h"

namespace
{
	FString CharactersString(const TArray<UCharacterData *> &Characters)
	{
		return Characters.IsEmpty() ? FString(TEXT("no characters"))
									: FString::JoinBy(Characters, TEXT(", "), [](const UCharacterData *Character) { return GetNameSafe(Character); });
	}
}

UMemoryData *MemoryDebug::FindMemory(FName MemoryId)
{
	const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UMemoryData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
	// Most memories use their asset name as the id (GetMemoryId): try that without loading every memory first.
	for (const FAssetData &Asset : Assets)
	{
		if (Asset.AssetName == MemoryId)
		{
			UMemoryData *Memory = Cast<UMemoryData>(Asset.GetAsset());
			if (Memory && Memory->GetMemoryId() == MemoryId)
			{
				return Memory;
			}
		}
	}
	for (const FAssetData &Asset : Assets)
	{
		UMemoryData *Memory = Asset.AssetName == MemoryId ? nullptr : Cast<UMemoryData>(Asset.GetAsset());
		if (Memory && Memory->GetMemoryId() == MemoryId)
		{
			return Memory; // an authored MemoryId
		}
	}
	if (Registry.IsLoadingAssets())
	{
		UE_LOG(LogWoR, Display, TEXT("[MemoryDebug] the asset registry is still scanning; try again in a moment"));
	}
	return nullptr;
}

FString MemoryDebug::GetScopeString(const UMemoryData &Memory)
{
	if (Memory.CharacterDepth.IsEmpty())
	{
		return TEXT("for every character");
	}
	TArray<FString> Entries;
	for (const TPair<TObjectPtr<UCharacterData>, EMemoryDepth> &Entry : Memory.CharacterDepth)
	{
		Entries.Add(FString::Printf(TEXT("%s %s"), *GetNameSafe(Entry.Key.Get()), *UEnum::GetDisplayValueAsText(Entry.Value).ToString()));
	}
	return FString::Printf(TEXT("for %s"), *FString::Join(Entries, TEXT(", ")));
}

FString MemoryDebug::GetRoutesString(const UMemoryData &Memory, const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal,
									 const TSet<FName> *KnownMemoryIds)
{
	FWoRConditionContext Context = WoRConditions::MakeContext(Event, Journal);
	Context.KnownMemoryIds = KnownMemoryIds;
	TArray<FString> Routes;
	for (const TArray<FMemoryCondition> &Set : Memory.GetConditionSets())
	{
		Routes.Add(WoRConditions::GetConditionSetString(WoRConditions::FromMemory(Set), Context));
	}
	if (Routes.IsEmpty())
	{
		return TEXT("by its source only");
	}
	const bool bMet = MemoryConditions::AreMet(Memory, Event, Journal, KnownMemoryIds);
	return FString::Printf(TEXT("%s -> %s"), *FString::Join(Routes, TEXT(" OR ")), bMet ? TEXT("MET") : TEXT("not met"));
}

FString MemoryDebug::GetWhyString(const UGameInstance *GameInstance, const UMemoryData &Memory, const FMemoryEvent &Event)
{
	const UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
	const TArray<AWoRPlayerState *> Friends = MemoryUnlock::GetRemotePlayers(GameInstance);
	TArray<const TSet<FName> *> FriendsKnown;
	TArray<UCharacterData *> FriendsCharacters;
	for (const AWoRPlayerState *Friend : Friends)
	{
		FriendsKnown.Add(&Friend->GetKnownMemoryIds());
		FriendsCharacters.Append(Friend->GetOwnCharacters());
	}
	const TSet<FName> TeamKnown = MemoryConditions::GetTeamMemoryIds(Journal, FriendsKnown);
	const TSet<FName> *Team = Friends.IsEmpty() ? nullptr : &TeamKnown;

	TArray<FString> Lines;
	const bool bHostHas = Journal && Journal->HasMemory(&Memory);
	Lines.Add(FString::Printf(TEXT("%s (%s)%s"), *Memory.GetMemoryId().ToString(), *GetScopeString(Memory), bHostHas ? TEXT(", host has it") : TEXT("")));

	const FMemoryEvent HostsPart = MemoryConditions::FilterParty(Event, FriendsCharacters, /*bKeep=*/false);
	TArray<UCharacterData *> HostCharacters;
	for (const FMemoryEventMember &Member : HostsPart.Party)
	{
		HostCharacters.AddUnique(const_cast<UCharacterData *>(Member.Character));
	}
	Lines.Add(FString::Printf(TEXT("  host (%s): %s"), *CharactersString(HostCharacters), *GetRoutesString(Memory, HostsPart, Journal, Team)));
	for (const AWoRPlayerState *Friend : Friends)
	{
		const TArray<UCharacterData *> Own = Friend->GetOwnCharacters();
		const FMemoryEvent FriendsPart = MemoryConditions::FilterParty(Event, Own, /*bKeep=*/true);
		Lines.Add(FString::Printf(TEXT("  %s (%s)%s: %s"), *Friend->GetPlayerName(), *CharactersString(Own),
								  Friend->HasMemoryId(Memory.GetMemoryId()) ? TEXT(", has it") : TEXT(""),
								  *GetRoutesString(Memory, FriendsPart, Journal, Team)));
	}
	const FMemoryReceivers Receivers = MemoryConditions::PickReceivers(GameInstance, Memory, Event, Journal);
	TArray<FString> Names;
	if (Receivers.bHost)
	{
		Names.Add(TEXT("host"));
	}
	for (const AWoRPlayerState *Friend : Receivers.Friends)
	{
		Names.Add(Friend ? Friend->GetPlayerName() : TEXT("?"));
	}
	Lines.Add(FString::Printf(TEXT("  would go to: %s"), Names.IsEmpty() ? TEXT("nobody") : *FString::Join(Names, TEXT(", "))));
	return FString::Join(Lines, TEXT("\n"));
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GMemoryWhyCommand(
	TEXT("wor.Memory.Why"),
	TEXT("B-375: wor.Memory.Why <MemoryId> - for that memory, who it is for, each condition met or not for each player present, and who would get it now (out of a fight)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogWoR, Display, TEXT("[MemoryDebug] usage: wor.Memory.Why <MemoryId>"));
			return;
		}
		const UMemoryData *Memory = MemoryDebug::FindMemory(FName(*Args[0]));
		if (!Memory)
		{
			UE_LOG(LogWoR, Display, TEXT("[MemoryDebug] no memory with id '%s'"), *Args[0]);
			return;
		}
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const FMemoryEvent Now = MemoryConditions::MakeEvent(GameInstance, EMemorySource::Fragment);
		UE_LOG(LogWoR, Display, TEXT("[MemoryDebug]\n%s"), *MemoryDebug::GetWhyString(GameInstance, *Memory, Now));
	}));
#endif
