// MemoryUnlock.cpp

#include "Memory/MemoryUnlock.h"

#include "Character/CharacterDataComponent.h"
#include "Combat/Boss/BossData.h"
#include "Engine/GameInstance.h"
#include "Equipment/EquipmentDataBase.h"
#include "Memory/MemoryData.h"
#include "Reaction/ReactionTrigger.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

bool MemoryUnlock::Unlock(UGameInstance *GameInstance, UMemoryData *Memory, EMemorySource Source)
{
	UTrialRunSubsystem *TrialRun = GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
	if (!Memory || !TrialRun || !Journal)
	{
		return false;
	}
	const bool bNewInRun = TrialRun->CollectMemory(Memory);
	const bool bNewInJournal = Journal->RecordMemory(Memory, Source);
	return bNewInRun || bNewInJournal;
}

int32 MemoryUnlock::UnlockBossDrops(UGameInstance *GameInstance, const TArray<UCharacterData *> &Beaten)
{
	int32 Unlocked = 0;
	for (const UCharacterData *Character : Beaten)
	{
		const UBossData *BossData = UBossData::FindFor(Character);
		if (BossData && Unlock(GameInstance, BossData->MemoryOnDefeat, EMemorySource::Boss))
		{
			++Unlocked;
		}
	}
	return Unlocked;
}

int32 MemoryUnlock::UnlockAtFightStart(UGameInstance *GameInstance, const TArray<FFiredReaction> &Fired,
									   const TArray<AActor *> &Party)
{
	int32 Unlocked = 0;
	for (const FFiredReaction &Reaction : Fired)
	{
		if (Reaction.Trigger && Unlock(GameInstance, Reaction.Trigger->UnlockMemory, EMemorySource::Reaction))
		{
			++Unlocked;
		}
	}
	for (const AActor *Member : Party)
	{
		const UCharacterDataComponent *DataComponent = Member ? Member->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		const UCharacterData *Wearer = DataComponent ? DataComponent->CharacterData : nullptr;
		for (const UEquipmentDataBase *Item : ReactionTriggers::GetWornItems(Member))
		{
			if (!Item || !Item->IsBoundBonusActive(Wearer))
			{
				continue;
			}
			for (UMemoryData *Memory : Item->BoundMemories)
			{
				Unlocked += Unlock(GameInstance, Memory, EMemorySource::BoundGear) ? 1 : 0;
			}
		}
	}
	if (Unlocked > 0)
	{
		UE_LOG(LogWoR, Log, TEXT("[MemoryUnlock] %d memory(ies) unlocked at fight start"), Unlocked);
	}
	return Unlocked;
}
