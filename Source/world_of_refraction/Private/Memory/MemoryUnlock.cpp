// MemoryUnlock.cpp

#include "Memory/MemoryUnlock.h"

#include "Character/CharacterDataComponent.h"
#include "Combat/Boss/BossData.h"
#include "Engine/GameInstance.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryComponent.h"
#include "Memory/MemoryData.h"
#include "Reaction/ReactionTrigger.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

namespace
{
	const UCharacterData *CharacterOf(const AActor *Actor)
	{
		const UCharacterDataComponent *DataComponent = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		return DataComponent ? DataComponent->CharacterData : nullptr;
	}

	/** The instance tier of each item Actor wears now (ReactionTriggers::GetWornItems decides what
	 *  is worn; the active loadout's entries hold the levelled tiers). */
	TArray<TPair<const UEquipmentDataBase *, EItemTier>> GetWornWithTiers(const AActor *Actor)
	{
		TArray<TPair<const UEquipmentDataBase *, EItemTier>> Worn;
		const UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			return Worn;
		}
		const FCombatLoadout &Loadout = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		TArray<TPair<const UEquipmentDataBase *, EItemTier>> Entries;
		Entries.Emplace(Loadout.PrimaryWeapon.WeaponEntry.Weapon, Loadout.PrimaryWeapon.WeaponEntry.Tier);
		Entries.Emplace(Loadout.SecondaryWeapon.WeaponEntry.Weapon, Loadout.SecondaryWeapon.WeaponEntry.Tier);
		Entries.Emplace(Loadout.PrimaryRing.RingEntry.Ring, Loadout.PrimaryRing.RingEntry.Tier);
		if (Loadout.RingLoadout.IsValidIndex(Loadout.ActiveRingIndex))
		{
			const FRingInventoryEntry &Active = Loadout.RingLoadout[Loadout.ActiveRingIndex].RingEntry;
			Entries.Emplace(Active.Ring, Active.Tier);
		}
		const TArray<const UEquipmentDataBase *> InUse = ReactionTriggers::GetWornItems(Actor);
		for (const TPair<const UEquipmentDataBase *, EItemTier> &Entry : Entries)
		{
			if (Entry.Key && InUse.Contains(Entry.Key) &&
				!Worn.ContainsByPredicate([&Entry](const TPair<const UEquipmentDataBase *, EItemTier> &Kept) { return Kept.Key == Entry.Key; }))
			{
				Worn.Add(Entry);
			}
		}
		return Worn;
	}

	bool IsFacing(const UCharacterData *Character, const TArray<AActor *> &Opposing)
	{
		return Opposing.ContainsByPredicate([Character](const AActor *Foe) { return CharacterOf(Foe) == Character; });
	}
}

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

bool MemoryUnlock::UnlockBossDrop(UGameInstance *GameInstance, const UBossData *BossData)
{
	return BossData && Unlock(GameInstance, BossData->MemoryOnDefeat, EMemorySource::Boss);
}

int32 MemoryUnlock::UnlockBossDrops(UGameInstance *GameInstance, const TArray<UCharacterData *> &Beaten)
{
	int32 Unlocked = 0;
	for (const UCharacterData *Character : Beaten)
	{
		Unlocked += UnlockBossDrop(GameInstance, UBossData::FindFor(Character)) ? 1 : 0;
	}
	return Unlocked;
}

int32 MemoryUnlock::GetUpgradeSteps(const UEquipmentDataBase &Item, EItemTier InstanceTier)
{
	return FMath::Max(0, static_cast<int32>(InstanceTier) - static_cast<int32>(Item.Tier));
}

bool MemoryUnlock::AreConditionsMet(const FBoundMemory &Bound, const UEquipmentDataBase &Item, int32 UpgradeSteps,
									const UCharacterData *Wearer, const TArray<AActor *> &Opposing)
{
	if (!Bound.Memory || UpgradeSteps < Bound.UpgradeLevel)
	{
		return false;
	}
	if (Bound.HasTrigger() && !Wearer)
	{
		return false;
	}
	return (!Bound.bNeedsOwnerWearing || Item.IsBoundBonusActive(Wearer)) &&
		   (!Bound.FacingCharacter || IsFacing(Bound.FacingCharacter, Opposing));
}

int32 MemoryUnlock::UnlockBoundOnLevelUp(UGameInstance *GameInstance, const UEquipmentDataBase *Item, EItemTier InstanceTier)
{
	if (!Item)
	{
		return 0;
	}
	int32 Unlocked = 0;
	const int32 Steps = GetUpgradeSteps(*Item, InstanceTier);
	for (const FBoundMemory &Bound : Item->BoundMemories)
	{
		if (AreConditionsMet(Bound, *Item, Steps, nullptr, {}) && Unlock(GameInstance, Bound.Memory, EMemorySource::BoundGear))
		{
			++Unlocked;
		}
	}
	return Unlocked;
}

int32 MemoryUnlock::UnlockAtFightStart(UGameInstance *GameInstance, const TArray<FFiredReaction> &Fired,
									   const TArray<AActor *> &Party, const TArray<AActor *> &Opposing)
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
		const UCharacterData *Wearer = CharacterOf(Member);
		for (const TPair<const UEquipmentDataBase *, EItemTier> &Worn : GetWornWithTiers(Member))
		{
			const int32 Steps = GetUpgradeSteps(*Worn.Key, Worn.Value);
			for (const FBoundMemory &Bound : Worn.Key->BoundMemories)
			{
				if (AreConditionsMet(Bound, *Worn.Key, Steps, Wearer, Opposing) &&
					Unlock(GameInstance, Bound.Memory, EMemorySource::BoundGear))
				{
					++Unlocked;
				}
			}
		}
	}
	if (Unlocked > 0)
	{
		UE_LOG(LogWoR, Log, TEXT("[MemoryUnlock] %d memory(ies) unlocked at fight start"), Unlocked);
	}
	return Unlocked;
}
