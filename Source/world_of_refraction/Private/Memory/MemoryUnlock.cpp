// MemoryUnlock.cpp

#include "Memory/MemoryUnlock.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Player/WoRPlayerState.h"
#include "Character/CharacterDataComponent.h"
#include "Conditions/WoRConditions.h"
#include "Memory/MemoryConditions.h"
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

namespace
{
	/** B-235e: on a client, its own new find goes to the host (noted there; one for every character is shared). */
	void ReportFindToHost(const UGameInstance *GameInstance, UMemoryData *Memory, EMemorySource Source, bool bRecordedHere)
	{
		const UWorld *World = GameInstance ? GameInstance->GetWorld() : nullptr;
		if (!bRecordedHere || !World || World->GetNetMode() != NM_Client)
		{
			return;
		}
		const APlayerController *PC = World->GetFirstPlayerController();
		if (AWoRPlayerState *Self = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr)
		{
			Self->ServerReportFoundMemory(Memory, Source);
		}
	}
}

bool MemoryUnlock::Unlock(UGameInstance *GameInstance, UMemoryData *Memory, EMemorySource Source)
{
	// This machine's player found it: friends get it only when it is for every character (they met nothing themselves).
	TArray<AWoRPlayerState *> Friends;
	for (AWoRPlayerState *Friend : GetRemotePlayers(GameInstance))
	{
		if (ShouldFriendReceive(Memory, Friend->GetOwnCharacters(), /*bMetThemselves=*/false))
		{
			Friends.Add(Friend);
		}
	}
	return UnlockFor(GameInstance, Memory, Source, /*bHostReceives=*/true, Friends);
}

bool MemoryUnlock::UnlockFor(UGameInstance *GameInstance, UMemoryData *Memory, EMemorySource Source, bool bHostReceives,
							 const TArray<AWoRPlayerState *> &Friends)
{
	UTrialRunSubsystem *TrialRun = GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
	if (!Memory || !TrialRun || !Journal)
	{
		return false;
	}
	// Someone new must receive it: the host (not in its journal yet) or a friend who does not have it. A memory the
	// host had from an earlier run, with no friend new to it, stays out of this run as before; a friend new to it gets
	// it (their own spots and fragments run on their side, B-235e).
	const bool bHostNew = bHostReceives && !Journal->HasMemory(Memory);
	TArray<AWoRPlayerState *> NewFriends;
	for (AWoRPlayerState *Friend : Friends)
	{
		if (Friend && !Friend->HasMemoryId(Memory->GetMemoryId()))
		{
			NewFriends.AddUnique(Friend);
		}
	}
	if (!bHostNew && NewFriends.IsEmpty())
	{
		return false;
	}
	TrialRun->CollectMemory(Memory); // no-op when the run already has it (a second finder)
	if (bHostNew && !Journal->RecordMemory(Memory, Source))
	{
		return false;
	}
	for (AWoRPlayerState *Friend : NewFriends)
	{
		Friend->AddMemoryId(Memory->GetMemoryId());
		Friend->Client_UnlockMemory(Memory, Source);
	}
	ReportFindToHost(GameInstance, Memory, Source, bHostNew);
	UE_LOG(LogWoR, Log, TEXT("[MemoryUnlock] %s: host %s, %d friend(s) (Q-235d)"), *Memory->GetMemoryId().ToString(),
		   bHostNew ? TEXT("records it") : TEXT("does not (a friend found it, or already had it)"), NewFriends.Num());
	// Q-209c: rewards come once, with the unlock. TODO: grant each kind once it can be: the item
	// into the party's inventory (out-of-fight gear, B-085), the skill once rewards teach skills,
	// titles and curses once they are systems; in co-op, to whoever received the memory.
	for (const FMemoryReward &Reward : Memory->Rewards)
	{
		UE_LOG(LogWoR, Log, TEXT("[MemoryUnlock] %s reward: %s (logged, not granted yet)"), *Memory->GetMemoryId().ToString(),
			   *UMemoryData::GetRewardString(Reward));
	}
	return true;
}

AWoRPlayerState *MemoryUnlock::FindRemotePlayerOf(const UGameInstance *GameInstance, const UCharacterData *Character)
{
	if (!Character)
	{
		return nullptr;
	}
	for (AWoRPlayerState *Friend : GetRemotePlayers(GameInstance))
	{
		if (Friend->GetOwnCharacters().Contains(Character))
		{
			return Friend;
		}
	}
	return nullptr;
}

bool MemoryUnlock::UnlockFoundBy(UGameInstance *GameInstance, UMemoryData *Memory, EMemorySource Source,
								 const UCharacterData *FinderCharacter)
{
	AWoRPlayerState *FinderFriend = FindRemotePlayerOf(GameInstance, FinderCharacter);
	if (!Memory || !FinderFriend)
	{
		return Unlock(GameInstance, Memory, Source); // the host's character (or no one known): the host's find
	}
	const bool bForEveryone = Memory->CharacterDepth.IsEmpty();
	TArray<AWoRPlayerState *> Friends = {FinderFriend};
	if (bForEveryone)
	{
		for (AWoRPlayerState *Friend : GetRemotePlayers(GameInstance))
		{
			Friends.AddUnique(Friend);
		}
	}
	return UnlockFor(GameInstance, Memory, Source, /*bHostReceives=*/bForEveryone, Friends);
}

bool MemoryUnlock::QualifiesFor(const UMemoryData *Memory, const TArray<UCharacterData *> &PlayerCharacters)
{
	if (!Memory)
	{
		return false;
	}
	if (Memory->CharacterDepth.IsEmpty())
	{
		return true; // a memory about no one in particular: the whole party's
	}
	return PlayerCharacters.ContainsByPredicate([Memory](const UCharacterData *Character)
	{
		return Character && Memory->CharacterDepth.FindRef(Character) != EMemoryDepth::None;
	});
}

bool MemoryUnlock::ShouldFriendReceive(const UMemoryData *Memory, const TArray<UCharacterData *> &FriendCharacters, bool bMetThemselves)
{
	if (!Memory)
	{
		return false;
	}
	return Memory->CharacterDepth.IsEmpty() || (bMetThemselves && QualifiesFor(Memory, FriendCharacters));
}

bool MemoryUnlock::ShouldHostReceive(const UMemoryData *Memory, const TArray<UCharacterData *> &HostCharacters, bool bHostMet,
									 bool bAnyFriendReceives)
{
	if (!Memory)
	{
		return false;
	}
	return Memory->CharacterDepth.IsEmpty() || (bHostMet && QualifiesFor(Memory, HostCharacters)) || !bAnyFriendReceives;
}

TArray<AWoRPlayerState *> MemoryUnlock::GetRemotePlayers(const UGameInstance *GameInstance)
{
	TArray<AWoRPlayerState *> Remote;
	const UWorld *World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const AGameStateBase *GameState = World ? World->GetGameState() : nullptr;
	if (!GameState || World->GetNetMode() != NM_ListenServer)
	{
		return Remote; // solo: the host's journal is the only one
	}
	for (APlayerState *Player : GameState->PlayerArray)
	{
		AWoRPlayerState *WoRPlayer = Cast<AWoRPlayerState>(Player);
		const APlayerController *PC = WoRPlayer ? Cast<APlayerController>(WoRPlayer->GetOwner()) : nullptr;
		if (PC && !PC->IsLocalController())
		{
			Remote.Add(WoRPlayer);
		}
	}
	return Remote;
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
	auto OwnedTier = [](const auto *Owned, const auto &Copy) { return Owned && Copy.PersistentID.IsValid() ? Owned->Tier : Copy.Tier; };
	TArray<TPair<const UEquipmentDataBase *, EItemTier>> Entries;
	for (const FWeaponInventoryEntry *Weapon : {&Loadout.PrimaryWeapon.WeaponEntry, &Loadout.SecondaryWeapon.WeaponEntry})
	{
		Entries.Emplace(Weapon->Weapon, OwnedTier(Inventory->FindOwnedWeapon(Weapon->PersistentID), *Weapon));
	}
	TArray<const FRingInventoryEntry *> Rings = {&Loadout.PrimaryRing.RingEntry};
	if (Loadout.RingLoadout.IsValidIndex(Loadout.ActiveRingIndex))
	{
		Rings.Add(&Loadout.RingLoadout[Loadout.ActiveRingIndex].RingEntry);
	}
	for (const FRingInventoryEntry *Ring : Rings)
	{
		Entries.Emplace(Ring->Ring, OwnedTier(Inventory->FindOwnedRing(Ring->PersistentID), *Ring));
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

FWoRConditionSet MemoryUnlock::GetBoundConditions(const FBoundMemory &Bound, const UEquipmentDataBase &Item)
{
	if (!Bound.Conditions.Conditions.IsEmpty())
	{
		return Bound.Conditions;
	}
	// The old fields: an old trigger can only hold in a fight, so its conditions check at FightStart.
	const EWoRConditionTrigger When = Bound.HasTrigger() ? EWoRConditionTrigger::FightStart : EWoRConditionTrigger::Any;
	FWoRConditionSet Set;
	FWoRCondition &Worn = Set.Conditions.AddDefaulted_GetRef();
	Worn.Trigger = When;
	Worn.Subject = EWoRConditionSubject::GearEquipped;
	Worn.Item = const_cast<UEquipmentDataBase *>(&Item);
	Worn.Operator = EWoRConditionOperator::AtLeast;
	Worn.Value = Bound.UpgradeLevel;
	if (Bound.bNeedsOwnerWearing)
	{
		Worn.Character = Item.BoundOwner;
		if (!Item.BoundOwner)
		{
			// Owner wearing with no owner never held: keep it that way.
			FWoRCondition &Never = Set.Conditions.AddDefaulted_GetRef();
			Never.Subject = EWoRConditionSubject::Hidden;
		}
	}
	if (Bound.FacingCharacter)
	{
		FWoRCondition &Facing = Set.Conditions.AddDefaulted_GetRef();
		Facing.Trigger = When;
		Facing.Subject = EWoRConditionSubject::FacingEnemy;
		Facing.Enemy = Bound.FacingCharacter;
	}
	return Set;
}

bool MemoryUnlock::IsBoundMet(const FBoundMemory &Bound, const UEquipmentDataBase &Item, const FWoRConditionContext &Context)
{
	return Bound.Memory && WoRConditions::IsMet(GetBoundConditions(Bound, Item), Context);
}

int32 MemoryUnlock::UnlockBoundOnLevelUp(UGameInstance *GameInstance, const UEquipmentDataBase *Item, EItemTier InstanceTier)
{
	if (!Item)
	{
		return 0;
	}
	int32 Unlocked = 0;
	// B-337c: the level-up as an event of its own (no party: a trigger needing one cannot hold).
	FMemoryEvent LevelUp;
	LevelUp.Source = EMemorySource::BoundGear;
	LevelUp.UpgradedItem = Item;
	LevelUp.UpgradedTier = InstanceTier;
	const FWoRConditionContext Context =
		WoRConditions::MakeContext(LevelUp, GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr);
	for (const FBoundMemory &Bound : Item->BoundMemories)
	{
		if (IsBoundMet(Bound, *Item, Context) && Unlock(GameInstance, Bound.Memory, EMemorySource::BoundGear))
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
		if (Reaction.Trigger && UnlockFoundBy(GameInstance, Reaction.Trigger->UnlockMemory, EMemorySource::Reaction, CharacterOf(Reaction.Wearer)))
		{
			++Unlocked;
		}
	}
	// B-337c: the fight start as one event: the party's worn gear and the opposing team.
	FMemoryEvent FightStart;
	FightStart.Source = EMemorySource::BoundGear;
	for (const AActor *Member : Party)
	{
		FightStart.Party.Add(MemoryConditions::MakeMember(Member));
	}
	FWoRConditionContext Context =
		WoRConditions::MakeContext(FightStart, GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr);
	Context.Trigger = EWoRConditionTrigger::FightStart;
	for (const AActor *Foe : Opposing)
	{
		Context.Opposing.Add(CharacterOf(Foe));
	}
	for (const FMemoryEventMember &Member : FightStart.Party)
	{
		for (const TPair<const UEquipmentDataBase *, EItemTier> &Worn : Member.Worn)
		{
			for (const FBoundMemory &Bound : Worn.Key->BoundMemories)
			{
				if (IsBoundMet(Bound, *Worn.Key, Context) &&
					UnlockFoundBy(GameInstance, Bound.Memory, EMemorySource::BoundGear, Member.Character)) // B-235e: the wearer's player
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
