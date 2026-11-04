// SaveSubsystem.cpp

#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Character/CharacterData.h"
#include "Character/Forms/CharacterFormSubsystem.h"
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
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "UObject/UObjectIterator.h"
#include "Memory/MemoryData.h"
#include "TimerManager.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"
#include <type_traits> // std::is_same_v (B-351), as InventoryComponent.cpp does

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

	/** B-124: a weapon or ring entry's attachment, spells and rolls. */
	template <typename TGearEntry>
	FSavedGearDetail SaveGearDetail(const TGearEntry &Entry)
	{
		FSavedGearDetail Gear;
		Gear.bSaved = true;
		Gear.Attached = Entry.AttachedItem;
		Gear.EvolutionItem = FSavedAssetRef::Make(Entry.AttachedItem.Evolution.Item);
		Gear.Attached.Evolution.Item = nullptr;
		Gear.StoneSocket = Entry.StoneSocket; // B-351
		if constexpr (std::is_same_v<TGearEntry, FWeaponInventoryEntry>)
		{
			for (const UAbilityData *Pick : Entry.SkillPicks) // B-351 (D-145), one ref per slot
			{
				Gear.SkillPicks.Add(FSavedAssetRef::Make(Pick));
			}
			Gear.bSkillSlotsActive = Entry.bSkillSlotsActive;
		}
		// One ref per spell slot, an empty slot as an unset ref, so slot positions survive.
		for (const USpellData *Spell : Entry.AssignedSpells)
		{
			Gear.AssignedSpells.Add(FSavedAssetRef::Make(Spell));
		}
		Gear.StatBonus = Entry.StatBonus;
		Gear.ResistanceBonus = Entry.ResistanceBonus;
		Gear.StatPool = Entry.StatPool;
		Gear.StatMaxPool = Entry.StatMaxPool;
		Gear.ResistancePool = Entry.ResistancePool;
		Gear.ResistanceMaxPool = Entry.ResistanceMaxPool;
		return Gear;
	}

	/** B-124: puts the saved detail on a freshly made entry; an older save keeps the asset's defaults. */
	template <typename TGearEntry>
	void RestoreGearDetail(const FSavedGearDetail &Gear, TGearEntry &Entry)
	{
		if (!Gear.bSaved)
		{
			return;
		}
		Entry.AttachedItem = Gear.Attached;
		Entry.AttachedItem.Evolution.Item = Gear.EvolutionItem.IsSet() ? Gear.EvolutionItem.ResolveAs<UEvolutionItemData>() : nullptr;
		Entry.StoneSocket = Gear.StoneSocket; // B-351
		if constexpr (std::is_same_v<TGearEntry, FWeaponInventoryEntry>)
		{
			Entry.SkillPicks.Reset();
			for (const FSavedAssetRef &Ref : Gear.SkillPicks) // B-351 (D-145); a gone ability leaves its slot to the built-in
			{
				Entry.SkillPicks.Add(Ref.IsSet() ? Ref.ResolveAs<UAbilityData>() : nullptr);
			}
			Entry.bSkillSlotsActive = Gear.bSkillSlotsActive;
		}
		Entry.AssignedSpells.Reset();
		for (const FSavedAssetRef &Ref : Gear.AssignedSpells)
		{
			USpellData *Spell = Ref.IsSet() ? Ref.ResolveAs<USpellData>() : nullptr;
			if (Ref.IsSet() && !Spell)
			{
				UE_LOG(LogWoR, Warning, TEXT("[Save] A saved gear spell %s is gone: its slot stays empty"), *Ref.Path.ToString());
			}
			Entry.AssignedSpells.Add(Spell);
		}
		Entry.StatBonus = Gear.StatBonus;
		Entry.ResistanceBonus = Gear.ResistanceBonus;
		Entry.StatPool = Gear.StatPool;
		Entry.StatMaxPool = Gear.StatMaxPool;
		Entry.ResistancePool = Gear.ResistancePool;
		Entry.ResistanceMaxPool = Gear.ResistanceMaxPool;
	}
}

namespace
{
/** B-311: asset lists to refs and back (a gone asset is dropped). */
template <typename TAsset>
TArray<FSavedAssetRef> SaveLoadoutRefs(const TArray<TAsset *> &Assets)
{
	TArray<FSavedAssetRef> Refs;
	for (const TAsset *Asset : Assets)
	{
		if (Asset)
		{
			Refs.Add(FSavedAssetRef::Make(Asset));
		}
	}
	return Refs;
}

template <typename TAsset>
TArray<TAsset *> RestoreLoadoutRefs(const TArray<FSavedAssetRef> &Refs)
{
	TArray<TAsset *> Assets;
	for (const FSavedAssetRef &Ref : Refs)
	{
		if (TAsset *Asset = Ref.ResolveAs<TAsset>())
		{
			Assets.Add(Asset);
		}
	}
	return Assets;
}

TArray<FSavedAssetRef> SaveLoadoutSpellRefs(const TArray<FSpellRef> &Spells)
{
	return SaveLoadoutRefs(FSpellRef::ExtractSpells(Spells));
}

TArray<FSpellRef> RestoreLoadoutSpellRefs(const TArray<FSavedAssetRef> &Refs)
{
	TArray<FSpellRef> Spells;
	for (USpellData *Spell : RestoreLoadoutRefs<USpellData>(Refs))
	{
		Spells.Emplace(Spell);
	}
	return Spells;
}

FSavedCustomLoadout SaveCustomLoadout(const FSavedLoadout &Loadout)
{
	FSavedCustomLoadout Saved;
	Saved.Name = Loadout.LoadoutName;
	Saved.RequiredClass = static_cast<uint8>(Loadout.RequiredClass);
	Saved.bShowPrimary = Loadout.bShowPrimary;
	Saved.PrimarySlotType = static_cast<uint8>(Loadout.PrimarySlotType);
	switch (Loadout.PrimarySlotType)
	{
	case EPrimarySlotType::Weapon:
		Saved.PrimaryAsset = FSavedAssetRef::Make(Loadout.PrimaryWeapon);
		Saved.PrimaryInstance = Loadout.PrimaryWeaponInstance;
		break;
	case EPrimarySlotType::Ring:
		Saved.PrimaryAsset = FSavedAssetRef::Make(Loadout.PrimaryRing);
		Saved.PrimaryInstance = Loadout.PrimaryRingInstance;
		break;
	case EPrimarySlotType::Evolution:
		Saved.PrimaryAsset = FSavedAssetRef::Make(Loadout.PrimaryEvolution);
		Saved.PrimaryInstance = Loadout.PrimaryEvolutionInstance;
		break;
	default:
		break;
	}
	Saved.PrimaryAbilities = SaveLoadoutRefs(Loadout.PrimaryWeaponAbilities);
	Saved.PrimaryStoneAbilities = SaveLoadoutRefs(Loadout.PrimaryAugmentStoneAbilities);
	Saved.EvolutionSpells = SaveLoadoutSpellRefs(Loadout.EvolutionSpells);
	Saved.SecondarySlotType = static_cast<uint8>(Loadout.SecondarySlotType);
	Saved.SecondaryWeapon = FSavedAssetRef::Make(Loadout.SecondaryWeapon);
	Saved.SecondaryInstance = Loadout.SecondaryWeaponInstance;
	Saved.SecondaryAbilities = SaveLoadoutRefs(Loadout.SecondaryWeaponAbilities);
	Saved.SecondaryStoneAbilities = SaveLoadoutRefs(Loadout.SecondaryAugmentStoneAbilities);
	for (const FResonatorRingSlot &Slot : Loadout.EquippedRings)
	{
		if (Slot.Ring)
		{
			Saved.Adapters.Add(FSavedAssetRef::Make(Slot.Ring));
			Saved.AdapterInstances.Add(Slot.RingInstance);
		}
	}
	Saved.InnateSpells = SaveLoadoutSpellRefs(Loadout.InnateSpells);
	for (const FSavedBDElementSpellPool &Pool : Loadout.BDSpellPools)
	{
		FSavedCustomBDPool &Out = Saved.BDSpellPools.AddDefaulted_GetRef();
		Out.Element = static_cast<uint8>(Pool.Element);
		Out.Spells = SaveLoadoutSpellRefs(Pool.Spells);
	}
	return Saved;
}

FSavedLoadout RestoreCustomLoadout(const FSavedCustomLoadout &Saved)
{
	FSavedLoadout Loadout;
	Loadout.LoadoutName = Saved.Name;
	Loadout.RequiredClass = static_cast<ECharacterClass>(Saved.RequiredClass);
	Loadout.bShowPrimary = Saved.bShowPrimary;
	Loadout.PrimarySlotType = static_cast<EPrimarySlotType>(Saved.PrimarySlotType);
	Loadout.PrimaryWeapon = Saved.PrimaryAsset.ResolveAs<UWeaponData>();
	Loadout.PrimaryRing = Saved.PrimaryAsset.ResolveAs<URingData>();
	Loadout.PrimaryEvolution = Saved.PrimaryAsset.ResolveAs<UEvolutionItemData>();
	Loadout.PrimaryWeaponInstance = Loadout.PrimaryWeapon ? Saved.PrimaryInstance : FGuid();
	Loadout.PrimaryRingInstance = Loadout.PrimaryRing ? Saved.PrimaryInstance : FGuid();
	Loadout.PrimaryEvolutionInstance = Loadout.PrimaryEvolution ? Saved.PrimaryInstance : FGuid();
	Loadout.PrimaryWeaponAbilities = RestoreLoadoutRefs<UAbilityData>(Saved.PrimaryAbilities);
	Loadout.PrimaryAugmentStoneAbilities = RestoreLoadoutRefs<UAbilityData>(Saved.PrimaryStoneAbilities);
	Loadout.EvolutionSpells = RestoreLoadoutSpellRefs(Saved.EvolutionSpells);
	Loadout.SecondarySlotType = static_cast<ESecondarySlotType>(Saved.SecondarySlotType);
	Loadout.SecondaryWeapon = Saved.SecondaryWeapon.ResolveAs<UWeaponData>();
	Loadout.SecondaryWeaponInstance = Saved.SecondaryInstance;
	Loadout.SecondaryWeaponAbilities = RestoreLoadoutRefs<UAbilityData>(Saved.SecondaryAbilities);
	Loadout.SecondaryAugmentStoneAbilities = RestoreLoadoutRefs<UAbilityData>(Saved.SecondaryStoneAbilities);
	for (int32 Index = 0; Index < Saved.Adapters.Num(); ++Index)
	{
		if (URingData *Ring = Saved.Adapters[Index].ResolveAs<URingData>())
		{
			FResonatorRingSlot &Slot = Loadout.EquippedRings.AddDefaulted_GetRef();
			Slot.Ring = Ring;
			Slot.RingInstance = Saved.AdapterInstances.IsValidIndex(Index) ? Saved.AdapterInstances[Index] : FGuid();
		}
	}
	Loadout.InnateSpells = RestoreLoadoutSpellRefs(Saved.InnateSpells);
	for (const FSavedCustomBDPool &In : Saved.BDSpellPools)
	{
		FSavedBDElementSpellPool &Pool = Loadout.BDSpellPools.AddDefaulted_GetRef();
		Pool.Element = static_cast<ESpellElement>(In.Element);
		Pool.Spells = RestoreLoadoutSpellRefs(In.Spells);
	}
	return Loadout;
}
}

namespace SaveRunResume
{
	TAutoConsoleVariable<bool> CVarResumeRun(
		TEXT("wor.Save.ResumeRun"), true,
		TEXT("B-208: resume a saved trial run when the game starts in the hub (0: start in the hub; the run is dropped at the next save)."));
}

void USaveSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UPartySessionSubsystem>();
	Collection.InitializeDependency<UPoolSubsystem>();
	Collection.InitializeDependency<UTrialRunSubsystem>();

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
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldActorsInitializedHandle);
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
	UCharacterFormSubsystem *Forms = GI->GetSubsystem<UCharacterFormSubsystem>();
	Forms->RestoreProgress(Data.Forms.Progress);
	Forms->RestoreWorn(Data.Forms.WornForm, Data.Forms.BaseForm == static_cast<uint8>(EPlayerBaseForm::Quinn) ? EPlayerBaseForm::Quinn : EPlayerBaseForm::Manny);
	Forms->RestoreOutfit(FOutfitSet::FromSaved(Data.Forms.CurrentOutfit)); // B-279c: after the skin, so its authored outfit does not overwrite it
	UE_LOG(LogWoR, Log, TEXT("[Save] Loaded slot '%s' (version %d): party %s, %d pool entries, wallet %s"),
		   SLOT_NAME, Loaded->GetSavedDataVersion(), bParty ? TEXT("restored") : TEXT("not restored"), PoolEntries,
		   Data.Wallet.bHasWallet ? TEXT("banked") : TEXT("empty"));

	// B-208: a saved run waits for the first game world, then resumes from the hub (Q-208).
	if (Data.Run.bHasRun)
	{
		PendingRun = Data.Run;
		UWorld *World = GI->GetWorld();
		if (World && World->IsGameWorld() && World->AreActorsInitialized())
		{
			TryResumeRun(World);
		}
		else
		{
			WorldActorsInitializedHandle =
				FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &USaveSubsystem::HandleWorldActorsInitialized);
		}
	}
}

void USaveSubsystem::HandleWorldActorsInitialized(const FActorsInitializedParams &Params)
{
	if (Params.World && Params.World->IsGameWorld() && Params.World->GetGameInstance() == GetGameInstance())
	{
		TryResumeRun(Params.World);
	}
}

void USaveSubsystem::TryResumeRun(UWorld *World)
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldActorsInitializedHandle);
	WorldActorsInitializedHandle.Reset();
	const FSavedRun Run = PendingRun;
	PendingRun = FSavedRun();

	UTrialRunSubsystem *TrialRun = GetGameInstance()->GetSubsystem<UTrialRunSubsystem>();
	if (!TrialRun || !SaveRunResume::CVarResumeRun.GetValueOnGameThread() || !TrialRun->IsHubWorld(World))
	{
		UE_LOG(LogWoR, Warning, TEXT("[Save] Saved run of %s not resumed (started in %s, not the hub, or wor.Save.ResumeRun 0); it is dropped at the next save"),
			   *Run.Trial.Path.ToString(), World ? *World->GetMapName() : TEXT("<no world>"));
		DropRunVitals();
		return;
	}
	if (!RestoreRun(Run, *TrialRun))
	{
		DropRunVitals();
		return;
	}
	CarriedGold = Run.RunGold;
	// Loaded after the PlayerState's BeginPlay: its wallet takes the run's Gold now.
	if (UCurrencyComponent *Wallet = FindLocalWallet(World))
	{
		ApplyWalletTo(Wallet);
	}
	// Not inside world initialisation: open the trial on the next tick.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (UTrialRunSubsystem *Resumed = GetGameInstance()->GetSubsystem<UTrialRunSubsystem>())
		{
			Resumed->ResumeRun();
		}
	}));
}

void USaveSubsystem::DropRunVitals()
{
	// The restored party carried the run's HP, wear and absorption; outside the run they start fresh.
	if (UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>())
	{
		// B-076: and the characters found in that run leave with it.
		PartySession->DismissRunMembers();
		PartySession->ClearAllMemberVitals();
	}
}

void USaveSubsystem::CaptureInto(UWorld *World)
{
	UGameInstance *GI = GetGameInstance();
	FWoRSaveData &Data = SaveGame->Data;
	// A level that never builds a party keeps the saved one.
	const UTrialRunSubsystem *TrialRun = GI->GetSubsystem<UTrialRunSubsystem>();
	// A loaded run not yet resumed is still a run: its vitals and recruits stay with it.
	const bool bInRun = (TrialRun && TrialRun->IsRunActive()) || PendingRun.bHasRun;
	if (const UParty *Party = GI->GetSubsystem<UPartySessionSubsystem>()->GetLocalParty())
	{
		Data.Party = CaptureParty(Party, bInRun);
	}
	// B-311: live edits to custom loadouts reach the pool before it is captured.
	for (TObjectIterator<UInventoryComponent> It; It; ++It)
	{
		if (!It->IsTemplate() && It->GetWorld() == World)
		{
			It->PushCustomLoadoutsToPool();
		}
	}
	Data.Pool = CapturePool(*GI->GetSubsystem<UPoolSubsystem>());
	const UCharacterFormSubsystem *Forms = GI->GetSubsystem<UCharacterFormSubsystem>();
	Data.Forms.Progress = Forms->GetProgressByName();
	Data.Forms.WornForm = Forms->GetWornFormName();
	Data.Forms.BaseForm = static_cast<uint8>(Forms->GetBaseForm());
	Data.Forms.CurrentOutfit = Forms->GetCurrentOutfit().ToSaved();
	if (const UCurrencyComponent *Wallet = FindLocalWallet(World))
	{
		BankWallet(Wallet);
	}
	// A run still waiting to resume stays as loaded.
	if (TrialRun && !PendingRun.bHasRun)
	{
		Data.Run = CaptureRun(*TrialRun, CarriedGold);
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
		CarriedGold = Wallet->GetBalance(ECurrencyType::Gold);
	}
}

void USaveSubsystem::ApplyWalletTo(UCurrencyComponent *Wallet) const
{
	ApplyWallet(SaveGame->Data.Wallet, Wallet);
	const int32 MissingGold = Wallet ? CarriedGold - Wallet->GetBalance(ECurrencyType::Gold) : 0;
	if (MissingGold > 0)
	{
		Wallet->Add(ECurrencyType::Gold, MissingGold);
	}
}

void USaveSubsystem::ClearRunGold(UWorld *World)
{
	CarriedGold = 0;
	// The live wallet too: its PlayerState banks it again at EndPlay, during the level change.
	if (UCurrencyComponent *Wallet = FindLocalWallet(World))
	{
		const int32 Gold = Wallet->GetBalance(ECurrencyType::Gold);
		if (Gold > 0)
		{
			Wallet->Spend(ECurrencyType::Gold, Gold);
		}
	}
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
		// B-076: a run-only member belongs to the run; without one it isn't kept.
		if (Member.bRunOnly && !bKeepVitals)
		{
			continue;
		}
		FSavedPartyMember &Out = Saved.Members.AddDefaulted_GetRef();
		Out.CharacterData = FSavedAssetRef::Make(Member.CharacterData);
		Out.PawnClass = FSoftClassPath(Member.PawnClass.ToSoftObjectPath().ToString());
		Out.PreferredRow = Member.PreferredRow;
		Out.PreferredColumn = Member.PreferredColumn;
		Out.bPlayerControlled = Member.bPlayerControlled;
		Out.bRunOnly = Member.bRunOnly;
		Out.RunWeapon = Member.RunWeapon;
		Out.RunRing = Member.RunRing;
		if (bKeepVitals)
		{
			Out.CurrentHP = Member.CurrentHP;
			Out.CurrentEP = Member.CurrentEP;
			Out.CarriedGear = FSavedCarriedGear::Make(Member.CarriedGear);
			Out.CarriedAbsorption = Member.CarriedAbsorption;
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
		Saved.Gear.Add_GetRef(SaveItem(E.Weapon, E, E.PersistentID)).Gear = SaveGearDetail(E);
	}
	for (const FRingInventoryEntry &E : Pool.GetOwnedRings())
	{
		Saved.Gear.Add_GetRef(SaveItem(E.Ring, E, E.PersistentID)).Gear = SaveGearDetail(E);
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
	for (const TPair<FGuid, FName> &Worn : Pool.GetWearers())
	{
		Saved.Wearers.Add({Worn.Key, Worn.Value});
	}
	for (const TPair<FGuid, FName> &Owned : Pool.GetGearOwners())
	{
		Saved.Owners.Add({Owned.Key, Owned.Value});
	}
	Saved.BankedCharacters = Pool.GetBankedCharacters().Array();
	// B-322 (save v9): a pouch per character. An older save's party pouch no member has drawn yet is written as it was, so the
	// next load still gives it to the leader (a save between load and draw loses nothing).
	if (Pool.HasLegacyCarriedItems())
	{
		const FPoolPouch &Legacy = Pool.GetLegacyCarriedItems();
		Saved.ItemSlotCount = Legacy.SlotCount;
		for (const TPair<FCrystalId, int32> &Stack : Legacy.Stacks)
		{
			FSavedCrystalStack &Out = Saved.CarriedItems.AddDefaulted_GetRef();
			Out.Type = static_cast<uint8>(Stack.Key.Type);
			Out.Tier = Stack.Key.Tier;
			Out.Count = Stack.Value;
		}
	}
	for (const TPair<FName, FPoolPouch> &Pouch : Pool.GetPouches())
	{
		FSavedPouch &Out = Saved.Pouches.AddDefaulted_GetRef();
		Out.Character = Pouch.Key;
		Out.SlotCount = Pouch.Value.SlotCount;
		for (const TPair<FCrystalId, int32> &Stack : Pouch.Value.Stacks)
		{
			FSavedCrystalStack &OutStack = Out.Stacks.AddDefaulted_GetRef();
			OutStack.Type = static_cast<uint8>(Stack.Key.Type);
			OutStack.Tier = Stack.Key.Tier;
			OutStack.Count = Stack.Value;
		}
	}
	Saved.BankedItemCharacters = Pool.GetBankedItemCharacters().Array();
	for (const TPair<FName, FPoolCustomLoadouts> &Custom : Pool.GetAllCustomLoadouts())
	{
		FSavedCharacterLoadouts &Out = Saved.CustomLoadouts.AddDefaulted_GetRef();
		Out.Character = Custom.Key;
		for (const FSavedLoadout &Loadout : Custom.Value.Loadouts)
		{
			Out.Loadouts.Add(SaveCustomLoadout(Loadout));
		}
	}
	return Saved;
}

FSavedRun USaveSubsystem::CaptureRun(const UTrialRunSubsystem &TrialRun, int32 RunGold)
{
	FSavedRun Saved;
	if (!TrialRun.IsRunActive())
	{
		return Saved;
	}
	Saved.bHasRun = true;
	Saved.Trial = FSavedAssetRef::Make(TrialRun.GetActiveTrial());
	Saved.RunSeed = TrialRun.GetRunSeed();
	Saved.VariationIndex = TrialRun.GetVariationIndex();
	Saved.Difficulty = static_cast<uint8>(TrialRun.GetTrialDifficulty());
	Saved.BeatenEncounters = TrialRun.GetBeatenEncounters().Array();
	Saved.FoundFeatures = TrialRun.GetFoundFeatures();
	for (const UMemoryData *Memory : TrialRun.GetRunMemories())
	{
		Saved.RunMemories.Add(FSavedAssetRef::Make(Memory));
	}
	Saved.RunGold = RunGold;
	return Saved;
}

// ==================== RESTORE ====================

bool USaveSubsystem::RestoreRun(const FSavedRun &Saved, UTrialRunSubsystem &TrialRun)
{
	if (!Saved.bHasRun)
	{
		return false;
	}
	UTrialData *Trial = Saved.Trial.ResolveAs<UTrialData>();
	if (!Trial)
	{
		UE_LOG(LogWoR, Warning, TEXT("[Save] Saved run dropped: trial %s not found"), *Saved.Trial.Path.ToString());
		return false;
	}
	if (Saved.RunSeed == 0)
	{
		UE_LOG(LogWoR, Warning, TEXT("[Save] Saved run of %s dropped: no run seed"), *Saved.Trial.Path.ToString());
		return false;
	}
	TArray<UMemoryData *> Memories;
	for (const FSavedAssetRef &Ref : Saved.RunMemories)
	{
		if (UMemoryData *Memory = Ref.ResolveAs<UMemoryData>())
		{
			Memories.Add(Memory);
		}
		else
		{
			UE_LOG(LogWoR, Warning, TEXT("[Save] Run memory %s not found; dropped"), *Ref.Path.ToString());
		}
	}
	const ETrialDifficulty Difficulty = StaticEnum<ETrialDifficulty>()->IsValidEnumValue(Saved.Difficulty)
		? static_cast<ETrialDifficulty>(Saved.Difficulty)
		: TrialGate::DEFAULT_DIFFICULTY;
	TrialRun.RestoreRun(Trial, Saved.RunSeed, Saved.VariationIndex, Difficulty, Saved.BeatenEncounters,
						Saved.FoundFeatures, Memories);
	return true;
}

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
		Member.bRunOnly = In.bRunOnly;
		Member.RunWeapon = In.RunWeapon;
		Member.RunRing = In.RunRing;
		Member.CurrentHP = In.CurrentHP;
		Member.CurrentEP = In.CurrentEP;
		Member.CarriedGear = In.CarriedGear.Restore();
		Member.CarriedAbsorption = In.CarriedAbsorption;
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
	// B-308: v5 saves one gear list; v3 / v4 saves the weapon and adapter lists. The kind follows from the asset.
	for (const TArray<FSavedOwnedItem> *List : {&Saved.Gear, &Saved.Weapons, &Saved.Rings})
	{
		for (const FSavedOwnedItem &In : *List)
		{
			UObject *Asset = In.Def.Resolve();
			if (UWeaponData *Weapon = Cast<UWeaponData>(Asset))
			{
				FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
				Entry.PersistentID = In.InstanceID;
				Entry.Tier = In.Tier;
				Entry.Quality = In.Quality;
				RestoreGearDetail(In.Gear, Entry);
				Pool.AddWeaponToPool(Entry);
				++Restored;
			}
			else if (URingData *Ring = Cast<URingData>(Asset))
			{
				FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(Ring);
				Entry.PersistentID = In.InstanceID;
				Entry.Tier = In.Tier;
				Entry.Quality = In.Quality;
				RestoreGearDetail(In.Gear, Entry);
				Pool.AddRingToPool(Entry);
				++Restored;
			}
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
	TMap<FGuid, FName> Wearers;
	for (const FSavedWearer &Worn : Saved.Wearers)
	{
		Wearers.Add(Worn.GearID, Worn.Character);
	}
	Pool.RestoreSharedGear(Wearers, Saved.BankedCharacters);
	TMap<FGuid, FName> Owners;
	for (const FSavedWearer &Owned : Saved.Owners)
	{
		Owners.Add(Owned.GearID, Owned.Character);
	}
	Pool.RestoreGearOwners(Owners);
	// B-322: v9 pouches per character; an older save's one party pouch waits for the leader (AdoptLegacyCarriedItems).
	auto ReadStacks = [&Restored](const TArray<FSavedCrystalStack> &In, TMap<FCrystalId, int32> &Out)
	{
		for (const FSavedCrystalStack &Stack : In)
		{
			if (Stack.Count > 0 && StaticEnum<ECrystalType>()->IsValidEnumValue(Stack.Type))
			{
				Out.FindOrAdd(FCrystalId(static_cast<ECrystalType>(Stack.Type), Stack.Tier)) += Stack.Count;
				++Restored;
			}
		}
	};
	TMap<FName, FPoolPouch> Pouches;
	for (const FSavedPouch &In : Saved.Pouches)
	{
		if (In.Character.IsNone())
		{
			continue;
		}
		FPoolPouch &Pouch = Pouches.FindOrAdd(In.Character);
		Pouch.SlotCount = In.SlotCount;
		ReadStacks(In.Stacks, Pouch.Stacks);
	}
	Pool.RestorePouches(Pouches, Saved.BankedItemCharacters);
	TMap<FCrystalId, int32> LegacyCarried;
	ReadStacks(Saved.CarriedItems, LegacyCarried);
	// Bought slots come along even when the old pouch was empty.
	if (!LegacyCarried.IsEmpty() || Saved.ItemSlotCount > InventoryConstants::ITEM_SLOTS_START)
	{
		Pool.RestoreLegacyCarriedItems(LegacyCarried, Saved.ItemSlotCount, Saved.BankedItemCharacters);
	}
	for (const FSavedCharacterLoadouts &Custom : Saved.CustomLoadouts)
	{
		TArray<FSavedLoadout> Loadouts;
		for (const FSavedCustomLoadout &Loadout : Custom.Loadouts)
		{
			Loadouts.Add(RestoreCustomLoadout(Loadout));
		}
		Pool.SetCustomLoadouts(Custom.Character, Loadouts);
	}
	if (Restored > 0)
	{
		Pool.MarkPopulated();
	}
	return Restored;
}
