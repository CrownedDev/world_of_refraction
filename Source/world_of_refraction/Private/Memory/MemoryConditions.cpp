// MemoryConditions.cpp

#include "Memory/MemoryConditions.h"

#include "Algo/AllOf.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Character/CharacterDataComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/EquipmentDataBase.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryUnlock.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

namespace
{
	bool IsInParty(const UCharacterData *Character, const FMemoryEvent &Event)
	{
		return Event.Party.ContainsByPredicate([Character](const FMemoryEventMember &Member) { return Member.Character == Character; });
	}

	bool IsGearMet(const FMemoryCondition &Condition, const FMemoryEvent &Event)
	{
		if (!Condition.Item)
		{
			return false;
		}
		const auto IsLevelled = [&Condition](EItemTier Tier)
		{
			return MemoryUnlock::GetUpgradeSteps(*Condition.Item, Tier) >= Condition.UpgradeLevel;
		};
		for (const FMemoryEventMember &Member : Event.Party)
		{
			if (Condition.Character && Member.Character != Condition.Character)
			{
				continue;
			}
			for (const TPair<const UEquipmentDataBase *, EItemTier> &Worn : Member.Worn)
			{
				if (Worn.Key == Condition.Item && IsLevelled(Worn.Value))
				{
					return true;
				}
			}
		}
		// A gear upgrade: the levelled item counts as worn by its wearer, if that one is in the party.
		return Event.UpgradedItem == Condition.Item && IsLevelled(Event.UpgradedTier) &&
			   (!Condition.Character || IsInParty(Condition.Character, Event));
	}
}

FMemoryEvent MemoryConditions::MakeEvent(const UGameInstance *GameInstance, EMemorySource Source)
{
	FMemoryEvent Event;
	Event.Source = Source;
	const UPartySessionSubsystem *PartySession = GameInstance ? GameInstance->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
	{
		for (const FPartyMember &Member : Party->Members)
		{
			if (Member.CharacterData)
			{
				Event.Party.Add({Member.CharacterData, {}});
			}
		}
	}
	const UTrialRunSubsystem *TrialRun = GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (const UTrialData *Trial = TrialRun ? TrialRun->GetActiveTrial() : nullptr)
	{
		Event.Locations.Add(Trial->GetFName());
	}
	return Event;
}

FMemoryEventMember MemoryConditions::MakeMember(const AActor *Combatant)
{
	const UCharacterDataComponent *DataComponent = Combatant ? Combatant->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	return {DataComponent ? DataComponent->CharacterData : nullptr, MemoryUnlock::GetWornWithTiers(Combatant)};
}

bool MemoryConditions::IsMet(const FMemoryCondition &Condition, const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal)
{
	switch (Condition.Kind)
	{
	case EMemoryConditionKind::Character:
		return Condition.Character && IsInParty(Condition.Character, Event);
	case EMemoryConditionKind::GearEquipped:
		return IsGearMet(Condition, Event);
	case EMemoryConditionKind::Location:
		return !Condition.LocationId.IsNone() && Event.Locations.Contains(Condition.LocationId);
	case EMemoryConditionKind::EnemyBeaten:
		return Condition.Enemy && Event.Beaten.Contains(Condition.Enemy.Get());
	case EMemoryConditionKind::NoDamageTaken:
		return Event.bNoDamageTaken;
	case EMemoryConditionKind::PriorMemory:
		return Condition.PriorMemory && Journal && Journal->HasMemory(Condition.PriorMemory);
	case EMemoryConditionKind::Hidden:
		return false;
	}
	return false;
}

bool MemoryConditions::AreMet(const UMemoryData &Memory, const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal)
{
	return Algo::AllOf(Memory.UnlockConditions, [&](const FMemoryCondition &Condition) { return IsMet(Condition, Event, Journal); });
}

TArray<UMemoryData *> MemoryConditions::GatherConditional()
{
	const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UMemoryData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
	// The registry's order isn't fixed; a chain should unlock the same way every time.
	Assets.Sort([](const FAssetData &A, const FAssetData &B) { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });

	TArray<UMemoryData *> Conditional;
	for (const FAssetData &Asset : Assets)
	{
		UMemoryData *Memory = Cast<UMemoryData>(Asset.GetAsset());
		if (Memory && !Memory->UnlockConditions.IsEmpty())
		{
			Conditional.Add(Memory);
		}
	}
	return Conditional;
}

int32 MemoryConditions::CheckEvent(UGameInstance *GameInstance, const FMemoryEvent &Event, const TArray<UMemoryData *> &Conditional)
{
	const UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
	if (!Journal)
	{
		return 0;
	}
	TArray<UMemoryData *> Candidates = Event.Named;
	for (UMemoryData *Memory : Conditional)
	{
		Candidates.AddUnique(Memory);
	}
	int32 Unlocked = 0;
	// Each pass can earn a memory another one needs (PriorMemory); stop once a pass earns nothing.
	for (bool bEarned = true; bEarned;)
	{
		bEarned = false;
		for (UMemoryData *Memory : Candidates)
		{
			if (Memory && !Journal->HasMemory(Memory) && AreMet(*Memory, Event, Journal) &&
				MemoryUnlock::Unlock(GameInstance, Memory, Event.Source))
			{
				++Unlocked;
				bEarned = true;
			}
		}
	}
	if (Unlocked > 0)
	{
		UE_LOG(LogWoR, Log, TEXT("[MemoryConditions] %s: %d memory(ies) unlocked"),
			   *UEnum::GetDisplayValueAsText(Event.Source).ToString(), Unlocked);
	}
	return Unlocked;
}

int32 MemoryConditions::CheckEvent(UGameInstance *GameInstance, const FMemoryEvent &Event)
{
	return CheckEvent(GameInstance, Event, GatherConditional());
}

FString MemoryConditions::GetConditionsString(const UMemoryData &Memory, const FMemoryEvent &Event, const UMemoryJournalSubsystem *Journal)
{
	TArray<FString> Parts;
	for (const FMemoryCondition &Condition : Memory.UnlockConditions)
	{
		Parts.Add(FString::Printf(TEXT("%s [%s]"), *UMemoryData::GetConditionString(Condition),
								  IsMet(Condition, Event, Journal) ? TEXT("met") : TEXT("not yet")));
	}
	const bool bCollected = Journal && Journal->HasMemory(&Memory);
	return FString::Printf(TEXT("%s%s: %s"), *Memory.GetMemoryId().ToString(), bCollected ? TEXT(" (collected)") : TEXT(""),
						   Parts.IsEmpty() ? TEXT("by its source only") : *FString::Join(Parts, TEXT(" + ")));
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GMemoryConditionsCommand(
	TEXT("wor.Memory.Conditions"),
	TEXT("Print every memory with unlock conditions, each marked met or not for the party now, out of a fight (B-209)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
		const FMemoryEvent Now = MemoryConditions::MakeEvent(GameInstance, EMemorySource::Fragment);
		const TArray<UMemoryData *> Conditional = MemoryConditions::GatherConditional();
		UE_LOG(LogWoR, Display, TEXT("%d memory(ies) with unlock conditions (party of %d)"), Conditional.Num(), Now.Party.Num());
		for (const UMemoryData *Memory : Conditional)
		{
			UE_LOG(LogWoR, Display, TEXT("  %s"), *MemoryConditions::GetConditionsString(*Memory, Now, Journal));
		}
	}));
#endif
