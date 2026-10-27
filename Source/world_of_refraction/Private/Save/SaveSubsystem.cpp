// SaveSubsystem.cpp

#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRPlayerState.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "WoRLog.h"

const TCHAR *USaveSubsystem::SLOT_NAME = TEXT("WoR_Main");

namespace
{
	/** The banked scalar currencies; Gold is run-only and EssenceTyped is per key. */
	constexpr ECurrencyType BANKED_SCALARS[] = {ECurrencyType::Prisms, ECurrencyType::Diamond,
												ECurrencyType::GearEssence, ECurrencyType::SkillEssence};

	UCurrencyComponent *FindLocalWallet(UWorld *World)
	{
		APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		const AWoRPlayerState *PS = PC && PC->IsLocalController() ? PC->GetPlayerState<AWoRPlayerState>() : nullptr;
		return PS ? PS->GetWallet() : nullptr;
	}

	template <typename TEntry>
	FSavedOwnedItem SaveItem(const UObject *Def, const TEntry &Entry, const FGuid &InstanceID)
	{
		FSavedOwnedItem Item;
		Item.Def = FSavedAssetRef::Make(Def);
		Item.InstanceID = InstanceID;
		Item.Tier = Entry.Tier;
		Item.Quality = Entry.Quality;
		return Item;
	}
}

void USaveSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UPartySessionSubsystem>();
	Collection.InitializeDependency<UPoolSubsystem>();

	SaveGame = NewObject<UWoRSaveGame>(this);
	UGameInstance *GI = GetGameInstance();
	LocalPlayerAddedHandle = GI->OnLocalPlayerAddedEvent.AddUObject(this, &USaveSubsystem::HandleLocalPlayerAdded);
	LocalPlayerRemovedHandle = GI->OnLocalPlayerRemovedEvent.AddUObject(this, &USaveSubsystem::HandleLocalPlayerRemoved);
	if (ULocalPlayer *First = GI->GetFirstGamePlayer())
	{
		LoadForPlayer(First);
	}
}

void USaveSubsystem::Deinitialize()
{
	// No save here: the subsystems deinitialise in creation order, so the party may already be
	// gone. The shutdown save runs when the local player is removed, before any of them.
	UGameInstance *GI = GetGameInstance();
	GI->OnLocalPlayerAddedEvent.Remove(LocalPlayerAddedHandle);
	GI->OnLocalPlayerRemovedEvent.Remove(LocalPlayerRemovedHandle);
	Super::Deinitialize();
}

void USaveSubsystem::HandleLocalPlayerAdded(ULocalPlayer *LocalPlayer)
{
	if (!IsBoundToPlayer())
	{
		LoadForPlayer(LocalPlayer);
	}
}

void USaveSubsystem::HandleLocalPlayerRemoved(ULocalPlayer *LocalPlayer)
{
	if (IsBoundToPlayer() && SaveGame->GetLocalPlayer() == LocalPlayer)
	{
		// The PlayerState banked the wallet at EndPlay; the world is gone by now.
		CaptureInto(nullptr);
		WriteSlot(false);
	}
}

void USaveSubsystem::LoadForPlayer(ULocalPlayer *LocalPlayer)
{
	UWoRSaveGame *Loaded = Cast<UWoRSaveGame>(
		ULocalPlayerSaveGame::LoadOrCreateSaveGameForLocalPlayer(UWoRSaveGame::StaticClass(), LocalPlayer, SLOT_NAME));
	if (!Loaded)
	{
		UE_LOG(LogWoR, Error, TEXT("[Save] Could not load or create slot '%s'"), SLOT_NAME);
		return;
	}
	SaveGame = Loaded;
	if (!Loaded->WasLoaded())
	{
		UE_LOG(LogWoR, Log, TEXT("[Save] No save in slot '%s': starting fresh"), SLOT_NAME);
		return;
	}

	const FWoRSaveData &Data = Loaded->Data;
	UGameInstance *GI = GetGameInstance();
	const bool bParty = Data.Party.bHasParty && RestoreParty(Data.Party, *GI->GetSubsystem<UPartySessionSubsystem>());
	const int32 PoolEntries = RestorePool(Data.Pool, *GI->GetSubsystem<UPoolSubsystem>());
	UE_LOG(LogWoR, Log, TEXT("[Save] Loaded slot '%s' (version %d): party %s, %d pool entries, wallet %s"),
		   SLOT_NAME, Loaded->GetSavedDataVersion(), bParty ? TEXT("restored") : TEXT("not restored"), PoolEntries,
		   Data.Wallet.bHasWallet ? TEXT("banked") : TEXT("empty"));
}

void USaveSubsystem::CaptureInto(UWorld *World)
{
	UGameInstance *GI = GetGameInstance();
	FWoRSaveData &Data = SaveGame->Data;
	// A level that never builds a party keeps the saved one.
	if (const UParty *Party = GI->GetSubsystem<UPartySessionSubsystem>()->GetLocalParty())
	{
		Data.Party = CaptureParty(Party, false);
	}
	Data.Pool = CapturePool(*GI->GetSubsystem<UPoolSubsystem>());
	if (const UCurrencyComponent *Wallet = FindLocalWallet(World))
	{
		BankWallet(Wallet);
	}
}

bool USaveSubsystem::SaveNow(UWorld *World, bool bAsync)
{
	CaptureInto(World);
	return WriteSlot(bAsync);
}

bool USaveSubsystem::WriteSlot(bool bAsync)
{
	if (!IsBoundToPlayer() || bWritesStopped)
	{
		return false;
	}
	const bool bStarted = bAsync ? SaveGame->AsyncSaveGameToSlotForLocalPlayer() : SaveGame->SaveGameToSlotForLocalPlayer();
	UE_LOG(LogWoR, Log, TEXT("[Save] %s save to '%s': %d member(s), %d pool entries, %d balance(s)"),
		   bAsync ? TEXT("Async") : TEXT("Sync"), SLOT_NAME, SaveGame->Data.Party.Members.Num(),
		   SaveGame->Data.Pool.Num(), SaveGame->Data.Wallet.Balances.Num());
	return bStarted;
}

void USaveSubsystem::DeleteSave()
{
	const ULocalPlayer *Player = SaveGame->GetLocalPlayer();
	const int32 UserIndex = Player ? Player->GetPlatformUserIndex() : 0;
	UGameplayStatics::DeleteGameInSlot(SLOT_NAME, UserIndex);
	SaveGame->ResetToDefault();
	bWritesStopped = true;
	UE_LOG(LogWoR, Warning, TEXT("[Save] Slot '%s' deleted; no automatic saves until the next start (or wor.Save.Now)"), SLOT_NAME);
}

void USaveSubsystem::BankWallet(const UCurrencyComponent *Wallet)
{
	if (Wallet)
	{
		SaveGame->Data.Wallet = CaptureWallet(Wallet);
	}
}

void USaveSubsystem::ApplyWalletTo(UCurrencyComponent *Wallet) const
{
	ApplyWallet(SaveGame->Data.Wallet, Wallet);
}

const FWoRSaveData &USaveSubsystem::GetData() const
{
	return SaveGame->Data;
}

bool USaveSubsystem::IsBoundToPlayer() const
{
	return SaveGame && SaveGame->GetLocalPlayer() != nullptr;
}

// ==================== CAPTURE ====================

FSavedParty USaveSubsystem::CaptureParty(const UParty *Party, bool bKeepVitals)
{
	FSavedParty Saved;
	if (!Party)
	{
		return Saved;
	}
	Saved.bHasParty = true;
	Saved.DisplayName = Party->DisplayName;
	Saved.bLetAIFinish = Party->bLetAIFinish;
	for (const FPartyMember &Member : Party->Members)
	{
		FSavedPartyMember &Out = Saved.Members.AddDefaulted_GetRef();
		Out.CharacterData = FSavedAssetRef::Make(Member.CharacterData);
		Out.PawnClass = FSoftClassPath(Member.PawnClass.ToSoftObjectPath().ToString());
		Out.PreferredRow = Member.PreferredRow;
		Out.PreferredColumn = Member.PreferredColumn;
		Out.bPlayerControlled = Member.bPlayerControlled;
		if (bKeepVitals)
		{
			Out.CurrentHP = Member.CurrentHP;
			Out.CurrentEP = Member.CurrentEP;
		}
	}
	return Saved;
}

FSavedWallet USaveSubsystem::CaptureWallet(const UCurrencyComponent *Wallet)
{
	FSavedWallet Saved;
	if (!Wallet)
	{
		return Saved;
	}
	Saved.bHasWallet = true;
	auto Bank = [&Saved, Wallet](ECurrencyType Currency, uint8 SubKey)
	{
		const int32 Amount = Wallet->GetBalance(Currency, SubKey);
		if (Amount > 0)
		{
			FSavedBalance &Balance = Saved.Balances.AddDefaulted_GetRef();
			Balance.Currency = static_cast<uint8>(Currency);
			Balance.SubKey = SubKey;
			Balance.Amount = Amount;
		}
	};
	for (const ECurrencyType Currency : BANKED_SCALARS)
	{
		Bank(Currency, 0);
	}
	for (uint8 Key = 0; Key <= static_cast<uint8>(EEssenceType::Ability); ++Key)
	{
		Bank(ECurrencyType::EssenceTyped, Key);
	}
	return Saved;
}

FSavedPool USaveSubsystem::CapturePool(const UPoolSubsystem &Pool)
{
	FSavedPool Saved;
	for (const FWeaponInventoryEntry &E : Pool.GetOwnedWeapons())
	{
		Saved.Weapons.Add(SaveItem(E.Weapon, E, E.PersistentID));
	}
	for (const FRingInventoryEntry &E : Pool.GetOwnedRings())
	{
		Saved.Rings.Add(SaveItem(E.Ring, E, E.PersistentID));
	}
	for (const FEvolutionInventoryEntry &E : Pool.GetOwnedEvolutions())
	{
		FSavedOwnedItem &Item = Saved.Evolutions.Add_GetRef(SaveItem(E.Item, E, E.InstanceID));
		Item.Durability = E.CurrentDurability;
	}
	for (const FSpellInstance &E : Pool.GetOwnedSpells())
	{
		Saved.Spells.Add(SaveItem(E.Spell, E, E.InstanceID));
	}
	for (const FAbilityInstance &E : Pool.GetOwnedAbilities())
	{
		Saved.Abilities.Add(SaveItem(E.Ability, E, E.InstanceID));
	}
	for (const TMap<FCrystalId, int32> *Stacks : {&Pool.GetCrystals(), &Pool.GetStones()})
	{
		for (const TPair<FCrystalId, int32> &Stack : *Stacks)
		{
			FSavedCrystalStack &Out = Saved.Crystals.AddDefaulted_GetRef();
			Out.Type = static_cast<uint8>(Stack.Key.Type);
			Out.Tier = Stack.Key.Tier;
			Out.Count = Stack.Value;
		}
	}
	return Saved;
}

// ==================== RESTORE ====================

bool USaveSubsystem::RestoreParty(const FSavedParty &Saved, UPartySessionSubsystem &PartySession)
{
	TArray<FPartyMember> Members;
	for (const FSavedPartyMember &In : Saved.Members)
	{
		FPartyMember Member;
		Member.CharacterData = In.CharacterData.ResolveAs<UCharacterData>();
		Member.PawnClass = TSoftClassPtr<ACombatCharacter>(FSoftObjectPath(In.PawnClass.ToString()));
		if (!Member.IsValidMember())
		{
			UE_LOG(LogWoR, Warning, TEXT("[Save] Party member %s dropped: character or pawn class not found"),
				   *In.CharacterData.Path.ToString());
			continue;
		}
		Member.PreferredRow = In.PreferredRow;
		Member.PreferredColumn = In.PreferredColumn;
		Member.bPlayerControlled = In.bPlayerControlled;
		Member.CurrentHP = In.CurrentHP;
		Member.CurrentEP = In.CurrentEP;
		Members.Add(Member);
	}
	return PartySession.RestoreLocalParty(Saved.DisplayName, Saved.bLetAIFinish, Members) != nullptr;
}

void USaveSubsystem::ApplyWallet(const FSavedWallet &Saved, UCurrencyComponent *Wallet)
{
	if (!Wallet || !Saved.bHasWallet)
	{
		return;
	}
	for (const FSavedBalance &Balance : Saved.Balances)
	{
		if (!StaticEnum<ECurrencyType>()->IsValidEnumValue(Balance.Currency))
		{
			continue;
		}
		const ECurrencyType Currency = static_cast<ECurrencyType>(Balance.Currency);
		const int32 Missing = Balance.Amount - Wallet->GetBalance(Currency, Balance.SubKey);
		if (Currency != ECurrencyType::Gold && Missing > 0)
		{
			Wallet->Add(Currency, Missing, Balance.SubKey);
		}
	}
}

int32 USaveSubsystem::RestorePool(const FSavedPool &Saved, UPoolSubsystem &Pool)
{
	int32 Restored = 0;
	for (const FSavedOwnedItem &In : Saved.Weapons)
	{
		if (UWeaponData *Weapon = In.Def.ResolveAs<UWeaponData>())
		{
			FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
			Entry.PersistentID = In.InstanceID;
			Entry.Tier = In.Tier;
			Entry.Quality = In.Quality;
			Pool.AddWeaponToPool(Entry);
			++Restored;
		}
	}
	for (const FSavedOwnedItem &In : Saved.Rings)
	{
		if (URingData *Ring = In.Def.ResolveAs<URingData>())
		{
			FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(Ring);
			Entry.PersistentID = In.InstanceID;
			Entry.Tier = In.Tier;
			Entry.Quality = In.Quality;
			Pool.AddRingToPool(Entry);
			++Restored;
		}
	}
	for (const FSavedOwnedItem &In : Saved.Evolutions)
	{
		if (UEvolutionItemData *Item = In.Def.ResolveAs<UEvolutionItemData>())
		{
			FEvolutionInventoryEntry Entry;
			Entry.Item = Item;
			Entry.InstanceID = In.InstanceID;
			Entry.Tier = In.Tier;
			Entry.Quality = In.Quality;
			Entry.CurrentDurability = In.Durability;
			Pool.AddEvolutionToPool(Entry);
			++Restored;
		}
	}
	for (const FSavedOwnedItem &In : Saved.Spells)
	{
		if (USpellData *Spell = In.Def.ResolveAs<USpellData>())
		{
			FSpellInstance Instance(Spell);
			Instance.InstanceID = In.InstanceID;
			Instance.Tier = In.Tier;
			Instance.Quality = In.Quality;
			Pool.AddSpellToPool(Instance);
			++Restored;
		}
	}
	for (const FSavedOwnedItem &In : Saved.Abilities)
	{
		if (UAbilityData *Ability = In.Def.ResolveAs<UAbilityData>())
		{
			FAbilityInstance Instance(Ability);
			Instance.InstanceID = In.InstanceID;
			Instance.Tier = In.Tier;
			Instance.Quality = In.Quality;
			Pool.AddAbilityToPool(Instance);
			++Restored;
		}
	}
	for (const FSavedCrystalStack &In : Saved.Crystals)
	{
		if (!StaticEnum<ECrystalType>()->IsValidEnumValue(In.Type))
		{
			continue;
		}
		FCrystalId Id;
		Id.Type = static_cast<ECrystalType>(In.Type);
		Id.Tier = In.Tier;
		Pool.AddCrystalToPool(Id, In.Count);
		++Restored;
	}
	if (Restored > 0)
	{
		Pool.MarkPopulated();
	}
	return Restored;
}
