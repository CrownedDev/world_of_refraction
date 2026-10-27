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

	bool IsFacing(const UCharacterData *Character, const TArray<AActor *> &Opposing)
	{
		return Opposing.ContainsByPredicate([Character](const AActor *Foe) { return CharacterOf(Foe) == Character; });
	}
}

bool MemoryUnlock::Unlock(UGameInstance *GameInstance, UMemoryData *Memory, EMemorySource Source)
{
	UTrialRunSubsystem *TrialRun = GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
	if (!Memory || !TrialRun || !Journal || Journal->HasMemory(Memory))
	{
		return false;
	}
	TrialRun->CollectMemory(Memory);
	return Journal->RecordMemory(Memory, Source);
}

bool MemoryUnlock::IsCollected(const UGameInstance *GameInstance, const UMemoryData *Memory)
{
	const UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
	return Journal && Memory && Journal->HasMemory(Memory);
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

/** The instance tier of each item Actor wears now (ReactionTriggers::GetWornItems decides what
 *  is worn; the active loadout's entries hold the levelled tiers). */
TArray<TPair<const UEquipmentDataBase *, EItemTier>> MemoryUnlock::GetWornWithTiers(const AActor *Actor)
{
	TArray<TPair<const UEquipmentDataBase *, EItemTier>> Worn;
	const UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory || !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
	{
		return Worn;
	}
	const FCombatLoadout &Loadout = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	// A level-up writes the owned entry, not the loadout's copy: read the owned one by its id.
	// TODO(B-085): loadouts will point at owned gear, and this lookup goes.
	auto OwnedTier = [](const auto &Owned, const auto &Copy)
	{
		const auto *Found = Owned.FindByPredicate([&Copy](const auto &Entry) { return Entry.PersistentID == Copy.PersistentID; });
		return Found && Copy.PersistentID.IsValid() ? Found->Tier : Copy.Tier;
	};
	TArray<TPair<const UEquipmentDataBase *, EItemTier>> Entries;
	for (const FWeaponInventoryEntry *Weapon : {&Loadout.PrimaryWeapon.WeaponEntry, &Loadout.SecondaryWeapon.WeaponEntry})
	{
		Entries.Emplace(Weapon->Weapon, OwnedTier(Inventory->Weapons, *Weapon));
	}
	TArray<const FRingInventoryEntry *> Rings = {&Loadout.PrimaryRing.RingEntry};
	if (Loadout.RingLoadout.IsValidIndex(Loadout.ActiveRingIndex))
	{
		Rings.Add(&Loadout.RingLoadout[Loadout.ActiveRingIndex].RingEntry);
	}
	for (const FRingInventoryEntry *Ring : Rings)
	{
		Entries.Emplace(Ring->Ring, OwnedTier(Inventory->Rings, *Ring));
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
