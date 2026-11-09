// SaveSubsystem.cpp

#include "Save/SaveSubsystem.h"
#include "Save/CoopCheckpoint.h"
#include "Equipment/Crystals/FFusionId.h" // FusionRules::RETIRED (B-352)
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
#include "Equipment/Crystals/StatStone.h"
#include "Skills/Definitions/SpellData.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "UObject/UObjectIterator.h"
#include "Memory/CurseSubsystem.h"
#include "Memory/HiddenCounterSubsystem.h"
#include "Memory/HiddenQuestSubsystem.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Memory/MemoryData.h"
#include "Equipment/Apparel/ApparelData.h"
#include "TimerManager.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"
#include "Inventory/ItemTier.h" // TierHelpers (B-234d: the run's upgrade memory)
#include "WoRLog.h"
#include <type_traits> // std::is_same_v (B-351), as InventoryComponent.cpp does

const TCHAR *USaveSubsystem::SLOT_NAME = TEXT("WoR_Main");

namespace
{
	/** The banked scalar currencies (B-333: six of the seven balances; B-361b: Mentor Tokens); Gold is run-only and
	 *  the OBSOLETE EssenceTyped is per key. */
	constexpr ECurrencyType BANKED_SCALARS[] = {ECurrencyType::Prisms, ECurrencyType::Diamond,
												ECurrencyType::GearEssence, ECurrencyType::SkillEssence,
												ECurrencyType::ElementalEssence, ECurrencyType::PillarEssence,
												ECurrencyType::MentorTokens};

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

	/** B-314b: a run mark with its pointers as refs (the evolution as SaveGearDetail keeps it). */
	FSavedRunMark SaveRunMark(const FGuid &GearID, const FPoolRunMark &Mark)
	{
		FSavedRunMark Out;
		Out.GearID = GearID;
		Out.bRunOnly = Mark.bRunOnly;
		Out.bOnRun = Mark.bOnRun;
		Out.HomeAttached = Mark.HomeAttached;
		Out.HomeEvolution = FSavedAssetRef::Make(Mark.HomeAttached.Evolution.Item);
		Out.HomeAttached.Evolution.Item = nullptr;
		for (const USpellData *Spell : Mark.HomeSpells)
		{
			Out.HomeSpells.Add(FSavedAssetRef::Make(Spell));
		}
		Out.HomeDurability = Mark.HomeDurability;
		return Out;
	}

	FPoolRunMark RestoreRunMark(const FSavedRunMark &In)
	{
		FPoolRunMark Mark;
		Mark.bRunOnly = In.bRunOnly;
		Mark.bOnRun = In.bOnRun;
		Mark.HomeAttached = In.HomeAttached;
		if (FusionRules::RETIRED && Mark.HomeAttached.Kind == EAttachedItemKind::Fusion)
		{
			Mark.HomeAttached = FRuntimeAttachedItem(); // B-352 (D-141): as RestoreGearDetail, a retired fusion is an empty socket
		}
		Mark.HomeAttached.Evolution.Item = In.HomeEvolution.IsSet() ? In.HomeEvolution.ResolveAs<UEvolutionItemData>() : nullptr;
		Mark.HomeAttached.Crystal.Id = StatStone::Normalize(Mark.HomeAttached.Crystal.Id); // B-353 (D-142)
		for (const FSavedAssetRef &Ref : In.HomeSpells)
		{
			Mark.HomeSpells.Add(Ref.IsSet() ? Ref.ResolveAs<USpellData>() : nullptr);
		}
		Mark.HomeDurability = In.HomeDurability;
		return Mark;
	}

	/** B-124: a weapon or ring entry's attachment, spells and rolls. */
	template <typename TGearEntry>
	FSavedGearDetail SaveGearDetail(const TGearEntry &Entry)
	{
		FSavedGearDetail Gear;
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
		for (const USpellData *Pick : Entry.SpellPicks) // B-351d (D-145), one ref per slot
		{
			Gear.SpellPicks.Add(FSavedAssetRef::Make(Pick));
		}
		Gear.bSpellSlotsActive = Entry.bSpellSlotsActive;
		Gear.StatBonus = Entry.StatBonus;
		Gear.ResistanceBonus = Entry.ResistanceBonus;
		Gear.StatPool = Entry.StatPool;
		Gear.StatMaxPool = Entry.StatMaxPool;
		Gear.ResistancePool = Entry.ResistancePool;
		Gear.ResistanceMaxPool = Entry.ResistanceMaxPool;
		return Gear;
	}

	/** B-124: puts the saved detail on a freshly made entry. */
	template <typename TGearEntry>
	void RestoreGearDetail(const FSavedGearDetail &Gear, TGearEntry &Entry)
	{
		Entry.AttachedItem = Gear.Attached;
		if (FusionRules::RETIRED && Entry.AttachedItem.Kind == EAttachedItemKind::Fusion)
		{
			Entry.AttachedItem = FRuntimeAttachedItem(); // B-352 (D-141): a fusion saved by an earlier dev build reads as an empty socket
		}
		Entry.AttachedItem.Evolution.Item = Gear.EvolutionItem.IsSet() ? Gear.EvolutionItem.ResolveAs<UEvolutionItemData>() : nullptr;
		Entry.StoneSocket = Gear.StoneSocket; // B-351
		Entry.StoneSocket.Crystal.Id = StatStone::Normalize(Entry.StoneSocket.Crystal.Id);   // B-353 (D-142)
		Entry.AttachedItem.Crystal.Id = StatStone::Normalize(Entry.AttachedItem.Crystal.Id); // B-353: a pre-B-351 stone
		if constexpr (std::is_same_v<TGearEntry, FWeaponInventoryEntry>)
		{
			Entry.SkillPicks.Reset();
			for (const FSavedAssetRef &Ref : Gear.SkillPicks) // B-351 (D-145); a gone ability leaves its slot to the built-in
			{
				Entry.SkillPicks.Add(Ref.IsSet() ? Ref.ResolveAs<UAbilityData>() : nullptr);
			}
			Entry.bSkillSlotsActive = Gear.bSkillSlotsActive;
		}
		Entry.SpellPicks.Reset();
		for (const FSavedAssetRef &Ref : Gear.SpellPicks) // B-351d (D-145); a gone spell leaves its slot to the built-in
		{
			Entry.SpellPicks.Add(Ref.IsSet() ? Ref.ResolveAs<USpellData>() : nullptr);
		}
		Entry.bSpellSlotsActive = Gear.bSpellSlotsActive;
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
	for (int32 Index = 0; Index < Loadout.Apparel.Num(); ++Index) // B-380c
	{
		if (Loadout.Apparel[Index])
		{
			Saved.Apparel.Add(FSavedAssetRef::Make(Loadout.Apparel[Index].Get()));
			Saved.ApparelInstances.Add(Loadout.ApparelInstances.IsValidIndex(Index) ? Loadout.ApparelInstances[Index] : FGuid());
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
	for (int32 Index = 0; Index < Saved.Apparel.Num(); ++Index) // B-380c
	{
		if (UApparelData *Piece = Saved.Apparel[Index].ResolveAs<UApparelData>())
		{
			Loadout.Apparel.Add(Piece);
			Loadout.ApparelInstances.Add(Saved.ApparelInstances.IsValidIndex(Index) ? Saved.ApparelInstances[Index] : FGuid());
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
	FWorldDelegates::OnWorldInitializedActors.Remove(OwnSaveReturnHandle); // B-234d
	Super::Deinitialize();
}

void USaveSubsystem::HandleLocalPlayerAdded(ULocalPlayer *LocalPlayer)
{
	if (!IsBoundToPlayer() && !bPlayingHostCopy) // B-234b review: a handed-over run keeps the host's copy
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
	GI->GetSubsystem<UMemoryJournalSubsystem>()->Restore(Data.Journal); // B-377b: before the quests that start from it
	GI->GetSubsystem<UHiddenQuestSubsystem>()->Restore(Data.HiddenQuests); // B-377
	GI->GetSubsystem<UCurseSubsystem>()->Restore(Data.Curses); // B-378
	GI->GetSubsystem<UHiddenCounterSubsystem>()->Restore(Data.Counters); // B-379
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

bool USaveSubsystem::AdoptCheckpoint()
{
	return AdoptCheckpointFrom(WoRCheckpoint::SLOT_NAME);
}

bool USaveSubsystem::AdoptCheckpointFrom(const FString &SlotName)
{
	UGameInstance *GI = GetGameInstance();
	const ULocalPlayer *Local = GI ? GI->GetFirstGamePlayer() : nullptr;
	const int32 UserIndex = Local ? Local->GetPlatformUserIndex() : 0;
	UWoRSaveGame *Copy = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex)
							 ? Cast<UWoRSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex))
							 : nullptr;
	if (Copy)
	{
		// The version check and migrations (D-060), as a loaded slot gets. InitializeSaveGame, not HandlePostLoad alone:
		// only it reads the saved version, and it binds the copy to this player, so a local player joining later
		// doesn't swap this player's own save in, and the shutdown save writes the copy (desktop review).
		Copy->InitializeSaveGame(Local, SlotName, /*bWasLoaded=*/true);
	}
	if (!GI || !Copy || !Copy->Data.Run.bHasRun)
	{
		UE_LOG(LogWoR, Warning, TEXT("[Save] Hand-over: %s"), Copy ? TEXT("the copy holds no run") : TEXT("no checkpoint copy to host from"));
		return false;
	}
	// This player's own wallet and look stay its own (Q-234b (2)).
	if (SaveGame)
	{
		Copy->Data.Wallet = SaveGame->Data.Wallet;
		Copy->Data.Forms = SaveGame->Data.Forms;
	}
	UPartySessionSubsystem *PartySession = GI->GetSubsystem<UPartySessionSubsystem>();
	UPoolSubsystem *Pool = GI->GetSubsystem<UPoolSubsystem>();
	if (!PartySession || !Pool)
	{
		UE_LOG(LogWoR, Warning, TEXT("[Save] Hand-over: no party or pool subsystem; the copy isn't adopted"));
		return false;
	}
	PartySession->ForgetLocalParty();
	const bool bParty = Copy->Data.Party.bHasParty && RestoreParty(Copy->Data.Party, *PartySession);
	// The old host's gear joins the pool until the run ends (party gear points into it); never written to this
	// player's own slot. B-234d: the run's end gives this player its own pool back (ReturnToOwnSave).
	const int32 PoolEntries = RestorePool(Copy->Data.Pool, *Pool);
	SaveGame = Copy;
	bPlayingHostCopy = true;
	AdoptedSlotName = SlotName;
	AdoptedUpgradeMemory = Copy->Data.Pool.UpgradeMemory; // B-234d review: what the run raises is told apart from this
	PendingRun = Copy->Data.Run;
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldActorsInitializedHandle);
	WorldActorsInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &USaveSubsystem::HandleWorldActorsInitialized);
	UE_LOG(LogWoR, Display, TEXT("[Save] Hand-over: hosting from the copy (party %s, %d pool entries); the run resumes in the next hub"),
		   bParty ? TEXT("restored") : TEXT("not restored"), PoolEntries);
	return true;
}

TMap<FName, EItemTier> USaveSubsystem::MergeRunUpgradeMemory(const TMap<FName, EItemTier> &Own, const TMap<FName, EItemTier> &AtHandOver,
															 const TMap<FName, EItemTier> &Played)
{
	TMap<FName, EItemTier> Merged = Own;
	for (const TPair<FName, EItemTier> &Entry : Played)
	{
		const EItemTier *Before = AtHandOver.Find(Entry.Key);
		if (Before && TierHelpers::GetTierValue(Entry.Value) <= TierHelpers::GetTierValue(*Before))
		{
			continue; // the old host's memory, not raised in this run
		}
		EItemTier &Kept = Merged.FindOrAdd(Entry.Key, Entry.Value);
		if (TierHelpers::GetTierValue(Entry.Value) > TierHelpers::GetTierValue(Kept))
		{
			Kept = Entry.Value;
		}
	}
	return Merged;
}

UWoRSaveGame *USaveSubsystem::LoadOwnSaveGame(const FString &SlotName) const
{
	const UGameInstance *GI = GetGameInstance();
	const ULocalPlayer *Local = GI ? GI->GetFirstGamePlayer() : nullptr;
	const int32 UserIndex = Local ? Local->GetPlatformUserIndex() : 0;
	const bool bOwnExists = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
	UWoRSaveGame *Own = bOwnExists ? Cast<UWoRSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex))
								   : Cast<UWoRSaveGame>(UGameplayStatics::CreateSaveGameObject(UWoRSaveGame::StaticClass()));
	if (!Own)
	{
		UE_LOG(LogWoR, Error, TEXT("[Save] Handed-over run's end: this player's own save '%s' could not be read"), *SlotName);
		return nullptr;
	}
	// The version check and migrations (D-060) and bound to this player, as AdoptCheckpointFrom does. No own save (or
	// one the version check wiped) gives this player a fresh party and pool, as a first start does.
	Own->InitializeSaveGame(Local, SlotName, /*bWasLoaded=*/bOwnExists);
	return Own;
}

bool USaveSubsystem::BuildOwnSaveFrom(const FString &SlotName, FWoRSaveData &OutData) const
{
	const UWoRSaveGame *Own = bPlayingHostCopy ? LoadOwnSaveGame(SlotName) : nullptr;
	if (!Own)
	{
		return false;
	}
	// Q-234d: only the party, the pool and the run came from the old host's copy. The rest (wallet with the run's
	// payout, forms, journal, quests, curses, counters) was this player's own all along, and the exit save has just
	// captured it, so it stays as played. The run is over. Review (D-097): the upgrade memory the run raised is kept.
	OutData = SaveGame->Data;
	OutData.Party = Own->Data.Party;
	OutData.Pool = Own->Data.Pool;
	OutData.Pool.UpgradeMemory = MergeRunUpgradeMemory(Own->Data.Pool.UpgradeMemory, AdoptedUpgradeMemory, SaveGame->Data.Pool.UpgradeMemory);
	OutData.Run = FSavedRun();
	return true;
}

void USaveSubsystem::EndHandedOverRun()
{
	if (!bPlayingHostCopy)
	{
		return;
	}
	// Review (save safety): the own slot is written now, so a quit or crash before the hub keeps the run's gains (the
	// exit save and the shutdown save still go to the copy's slot). Memory is not touched until the hub.
	UWoRSaveGame *Own = LoadOwnSaveGame(SLOT_NAME);
	bool bWritten = false;
	if (Own && BuildOwnSaveFrom(SLOT_NAME, Own->Data) && Own->GetLocalPlayer() && !bWritesStopped)
	{
		bWritten = Own->SaveGameToSlotForLocalPlayer();
	}
	// The in-memory return waits: the run's members still stand in this level and push their loadouts into the pool
	// as it goes.
	FWorldDelegates::OnWorldInitializedActors.Remove(OwnSaveReturnHandle);
	OwnSaveReturnHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &USaveSubsystem::HandleOwnSaveReturnWorld);
	UE_LOG(LogWoR, Display, TEXT("[Save] Handed-over run ended: own save written %s; this player's own party and pool come back in the hub"),
		   bWritten ? TEXT("yes") : TEXT("no"));
}

void USaveSubsystem::CancelHandedOverRunEnd()
{
	if (!OwnSaveReturnHandle.IsValid())
	{
		return;
	}
	FWorldDelegates::OnWorldInitializedActors.Remove(OwnSaveReturnHandle);
	OwnSaveReturnHandle.Reset();
	UE_LOG(LogWoR, Warning, TEXT("[Save] The travel to the hub was refused: still playing the copy this session (the own save is written; a restart loads it)"));
}

void USaveSubsystem::HandleOwnSaveReturnWorld(const FActorsInitializedParams &Params)
{
	if (!Params.World || !Params.World->IsGameWorld() || Params.World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	// Review: only the hub (ExitTrial's destination), never a transition map or a level reached some other way.
	const UTrialRunSubsystem *TrialRun = GetGameInstance()->GetSubsystem<UTrialRunSubsystem>();
	if (!TrialRun || !TrialRun->IsHubWorld(Params.World))
	{
		UE_LOG(LogWoR, Verbose, TEXT("[Save] %s is not the hub: the handed-over run's return keeps waiting"), *Params.World->GetMapName());
		return;
	}
	FWorldDelegates::OnWorldInitializedActors.Remove(OwnSaveReturnHandle);
	OwnSaveReturnHandle.Reset();
	ReturnToOwnSave(); // before the level's BeginPlay builds the party from it
}

bool USaveSubsystem::ReturnToOwnSave()
{
	return ReturnToOwnSaveFrom(SLOT_NAME);
}

bool USaveSubsystem::ReturnToOwnSaveFrom(const FString &SlotName)
{
	UGameInstance *GI = GetGameInstance();
	UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	UWoRSaveGame *Own = bPlayingHostCopy && PartySession && Pool ? LoadOwnSaveGame(SlotName) : nullptr;
	if (!Own || !BuildOwnSaveFrom(SlotName, Own->Data))
	{
		return false;
	}

	FWorldDelegates::OnWorldInitializedActors.Remove(WorldActorsInitializedHandle);
	WorldActorsInitializedHandle.Reset();
	PendingRun = FSavedRun();
	PartySession->ForgetLocalParty();
	const bool bParty = Own->Data.Party.bHasParty && RestoreParty(Own->Data.Party, *PartySession);
	Pool->ResetStore();
	const int32 PoolEntries = RestorePool(Own->Data.Pool, *Pool);
	SaveGame = Own;
	bPlayingHostCopy = false;
	AdoptedSlotName.Reset();
	AdoptedUpgradeMemory.Reset();
	const bool bWritten = WriteSlot(/*bAsync=*/false);
	UE_LOG(LogWoR, Display, TEXT("[Save] Handed-over run over: back to this player's own save (party %s, %d pool entries; written %s)"),
		   bParty ? TEXT("restored") : TEXT("fresh"), PoolEntries, bWritten ? TEXT("yes") : TEXT("no"));
	return true;
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
	Data.Journal = GI->GetSubsystem<UMemoryJournalSubsystem>()->Capture(); // B-377b
	Data.HiddenQuests = GI->GetSubsystem<UHiddenQuestSubsystem>()->Capture(); // B-377: steps done, kept across runs
	Data.Curses = GI->GetSubsystem<UCurseSubsystem>()->Capture(); // B-378: held across runs until removed
	Data.Counters = GI->GetSubsystem<UHiddenCounterSubsystem>()->Capture(); // B-379: counting across runs
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
	const bool bWritten = WriteSlot(bAsync);
	if (bWritten)
	{
		WoRCheckpoint::SendToClients(World, SaveGame); // B-234: every other player keeps the host's checkpoint (review: only a written one)
	}
	return bWritten;
}

bool USaveSubsystem::WriteSlot(bool bAsync)
{
	if (bPlayingHostCopy && !bWritesStopped)
	{
		// B-234b: a handed-over run saves into the copy's slot; this player's own save is never overwritten.
		const ULocalPlayer *Local = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
		const int32 UserIndex = Local ? Local->GetPlatformUserIndex() : 0;
		const bool bCopyWritten = UGameplayStatics::SaveGameToSlot(SaveGame, AdoptedSlotName, UserIndex);
		UE_LOG(LogWoR, Log, TEXT("[Save] Handed-over run saved to '%s': %s"), *AdoptedSlotName, bCopyWritten ? TEXT("yes") : TEXT("no"));
		return bCopyWritten;
	}
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
		Out.AISeatDifficulty = Member.AISeatDifficulty;
		Out.HeadStartMind = Member.HeadStartMind; // B-348b
		Out.HeadStartBody = Member.HeadStartBody;
		Out.HeadStartSpirit = Member.HeadStartSpirit;
		Out.bBrokenDarkness = Member.bBrokenDarkness; // B-348c: beyond the run
		Out.bRunOnly = Member.bRunOnly;
		Out.RunWeapon = Member.RunWeapon;
		Out.RunRing = Member.RunRing;
		Out.MemberId = Member.MemberId; // B-360b: the pool's keys for a copy name this id
		Out.bKeyedByMember = Member.bKeyedByMember;
		if (bKeepVitals)
		{
			Out.CurrentHP = Member.CurrentHP;
			Out.CurrentEP = Member.CurrentEP;
			Out.CarriedGear = FSavedCarriedGear::Make(Member.CarriedGear);
			Out.CarriedAbsorption = Member.CarriedAbsorption;
			Out.RunStrain = Member.RunStrain; // B-348c: the run's strain goes with the run
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
	// OBSOLETE (B-333): nothing earns typed essence now (D-125); delete in the sweep (D-124).
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
	// B-380b (D-164): owned apparel in the one gear list (the kind follows from the asset), with its socket; an
	// evolution there is kept as a ref, as SaveGearDetail does.
	for (const FApparelEntry &E : Pool.GetOwnedApparel())
	{
		FSavedOwnedItem &Item = Saved.Gear.Add_GetRef(SaveItem(E.Apparel.Get(), E, E.PersistentID));
		Item.Gear.Attached = E.AttachedItem;
		Item.Gear.EvolutionItem = FSavedAssetRef::Make(E.AttachedItem.Evolution.Item);
		Item.Gear.Attached.Evolution.Item = nullptr;
		Item.Gear.StatBonus = E.StatBonus;
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
			Out.StoneStat = static_cast<uint8>(Stack.Key.StoneStat); // B-353
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
	// B-322: a pouch per character.
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
			OutStack.StoneStat = static_cast<uint8>(Stack.Key.StoneStat); // B-353
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
	Saved.UpgradeMemory = Pool.GetUpgradeMemory(); // B-314b (D-097)
	for (const TPair<FName, FPoolSkills> &Skills : Pool.GetCharacterSkills()) // B-316b
	{
		FSavedCharacterSkills &Out = Saved.CharacterSkills.AddDefaulted_GetRef();
		Out.Character = Skills.Key;
		for (const FSpellInstance &E : Skills.Value.Spells)
		{
			Out.Spells.Add(SaveItem(E.Spell, E, E.InstanceID));
		}
		for (const FAbilityInstance &E : Skills.Value.Abilities)
		{
			Out.Abilities.Add(SaveItem(E.Ability, E, E.InstanceID));
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
	Saved.Wipes = TrialRun.GetRunWipes(); // B-362
	Saved.MainBossLosses = TrialRun.GetRunMainBossLosses();
	Saved.LastRestSpot = TrialRun.GetLastRestSpot();
	Saved.DroppedPrisms = TrialRun.GetDroppedPrisms(); // B-362b
	Saved.bDropHasSpot = TrialRun.GetDropSpot(Saved.DropSpot); // B-362c
	Saved.TallyHubGold = TrialRun.GetRunTally().HubGold; // B-073
	Saved.TallyGearEssence = TrialRun.GetRunTally().GearEssence;
	Saved.TallySkillEssence = TrialRun.GetRunTally().SkillEssence;
	Saved.bTallyBossBeaten = TrialRun.IsRunBossBeaten();
	Saved.BeatenMiniBosses = TrialRun.GetBeatenMiniBosses(); // B-187c
	Saved.EncounterRevivals = TrialRun.GetEncounterRevivalCounts();
	// B-314b (Q-314): the open run layer, so a resumed run keeps its found gear run-only and its home gear's home state.
	const UGameInstance *GI = TrialRun.GetGameInstance();
	const UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	if (Pool && Pool->IsRunLayerActive())
	{
		Saved.bHasRunLayer = true;
		for (const TPair<FGuid, FPoolRunMark> &Mark : Pool->GetRunMarks())
		{
			Saved.RunMarks.Add(SaveRunMark(Mark.Key, Mark.Value));
		}
	}
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
	TrialRun.RestoreRunLosses(Saved.Wipes, Saved.MainBossLosses, Saved.LastRestSpot); // B-362
	TrialRun.RestoreDrop(Saved.DroppedPrisms, Saved.bDropHasSpot, Saved.DropSpot); // B-362b, B-362c: with its spot
	FRunPayout Tally; // B-073: the run's reward tally, still to be paid at its end
	Tally.HubGold = Saved.TallyHubGold;
	Tally.GearEssence = Saved.TallyGearEssence;
	Tally.SkillEssence = Saved.TallySkillEssence;
	TrialRun.RestoreRunTally(Tally);
	if (Saved.bTallyBossBeaten)
	{
		TrialRun.MarkRunBossBeaten();
	}
	TrialRun.RestoreMiniBossRevivals(Saved.BeatenMiniBosses, Saved.EncounterRevivals); // B-187c: after RestoreRun cleared them
	// B-314b: the saved layer replaces the fresh one RestoreRun opened (the store was loaded from this same save).
	UGameInstance *GI = TrialRun.GetGameInstance();
	UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	if (Pool && !Saved.bHasRunLayer)
	{
		UE_LOG(LogWoR, Warning, TEXT("[Save] The saved run has no run layer: the resumed run takes the gear as saved as its home state"));
	}
	else if (Pool)
	{
		TMap<FGuid, FPoolRunMark> Marks;
		for (const FSavedRunMark &In : Saved.RunMarks)
		{
			Marks.Add(In.GearID, RestoreRunMark(In));
		}
		Pool->RestoreRunLayer(Marks);
	}
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
		Member.AISeatDifficulty = In.AISeatDifficulty;
		Member.HeadStartMind = In.HeadStartMind; // B-348b
		Member.HeadStartBody = In.HeadStartBody;
		Member.HeadStartSpirit = In.HeadStartSpirit;
		Member.bBrokenDarkness = In.bBrokenDarkness; // B-348c
		Member.RunStrain = In.RunStrain;
		Member.bRunOnly = In.bRunOnly;
		Member.RunWeapon = In.RunWeapon;
		Member.RunRing = In.RunRing;
		Member.MemberId = In.MemberId; // B-360b (RestoreLocalParty mints one when invalid)
		Member.bKeyedByMember = In.bKeyedByMember;
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
	// B-308: one gear list; the kind follows from the asset.
	for (const FSavedOwnedItem &In : Saved.Gear)
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
		else if (UApparelData *Apparel = Cast<UApparelData>(Asset)) // B-380b
		{
			FApparelEntry Entry = FApparelEntry::CreateFromApparel(Apparel);
			Entry.PersistentID = In.InstanceID;
			Entry.Tier = In.Tier;
			Entry.Quality = In.Quality;
			Entry.AttachedItem = In.Gear.Attached;
			Entry.AttachedItem.Evolution.Item = In.Gear.EvolutionItem.ResolveAs<UEvolutionItemData>();
			// Review: a socket apparel cannot hold (a corrupt or retired attachment) loads empty.
			if (!Entry.AttachedItem.IsEmpty() && (!ApparelRules::CanSocket(Entry.AttachedItem) ||
												  (Entry.AttachedItem.IsEvolution() && !Entry.AttachedItem.Evolution.Item)))
			{
				UE_LOG(LogWoR, Warning, TEXT("[Save] apparel %s: its socket could not be restored - left empty"), *Apparel->GetName());
				Entry.AttachedItem = FRuntimeAttachedItem();
			}
			Entry.StatBonus = In.Gear.StatBonus;
			Pool.AddApparelToPool(Entry);
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
		// B-353 (D-142): a StatStone keeps its sub-stat; an old single-stat stone loads as the StatStone it now is.
		const FCrystalId Id = StatStone::Normalize(FCrystalId(static_cast<ECrystalType>(In.Type), In.Tier, static_cast<ESubStat>(In.StoneStat)));
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
	// B-322: a pouch per character.
	auto ReadStacks = [&Restored](const TArray<FSavedCrystalStack> &In, TMap<FCrystalId, int32> &Out)
	{
		for (const FSavedCrystalStack &Stack : In)
		{
			if (Stack.Count > 0 && StaticEnum<ECrystalType>()->IsValidEnumValue(Stack.Type))
			{
				Out.FindOrAdd(StatStone::Normalize(FCrystalId(static_cast<ECrystalType>(Stack.Type), Stack.Tier,
															  static_cast<ESubStat>(Stack.StoneStat)))) += Stack.Count; // B-353
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
	for (const FSavedCharacterLoadouts &Custom : Saved.CustomLoadouts)
	{
		TArray<FSavedLoadout> Loadouts;
		for (const FSavedCustomLoadout &Loadout : Custom.Loadouts)
		{
			Loadouts.Add(RestoreCustomLoadout(Loadout));
		}
		Pool.SetCustomLoadouts(Custom.Character, Loadouts);
	}
	Pool.RestoreUpgradeMemory(Saved.UpgradeMemory); // B-314b (D-097)
	TMap<FName, FPoolSkills> CharacterSkills; // B-316b: a skill whose asset is gone is dropped
	for (const FSavedCharacterSkills &In : Saved.CharacterSkills)
	{
		if (In.Character.IsNone())
		{
			continue;
		}
		FPoolSkills &Skills = CharacterSkills.FindOrAdd(In.Character);
		for (const FSavedOwnedItem &Item : In.Spells)
		{
			if (USpellData *Spell = Item.Def.ResolveAs<USpellData>())
			{
				FSpellInstance &Instance = Skills.Spells.Add_GetRef(FSpellInstance(Spell));
				Instance.InstanceID = Item.InstanceID;
				Instance.Tier = Item.Tier;
				Instance.Quality = Item.Quality;
			}
		}
		for (const FSavedOwnedItem &Item : In.Abilities)
		{
			if (UAbilityData *Ability = Item.Def.ResolveAs<UAbilityData>())
			{
				FAbilityInstance &Instance = Skills.Abilities.Add_GetRef(FAbilityInstance(Ability));
				Instance.InstanceID = Item.InstanceID;
				Instance.Tier = Item.Tier;
				Instance.Quality = Item.Quality;
			}
		}
	}
	Pool.RestoreCharacterSkills(CharacterSkills);
	if (Restored > 0)
	{
		Pool.MarkPopulated();
	}
	return Restored;
}
