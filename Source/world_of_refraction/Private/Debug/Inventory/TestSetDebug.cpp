// TestSetDebug.cpp

#include "Debug/Inventory/TestSetDebug.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Debug/Currency/MaxResourcesDebug.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/FEvolutionInventoryEntry.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/WornGear.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "UI/Menus/LoadoutV2Model.h"
#include "WoRLog.h"

namespace
{
	constexpr float TEST_SET_SCREEN_SECONDS = 12.0f;

	FString CrystalName(const FCrystalId &Id)
	{
		return FString::Printf(TEXT("%s %s"), *UEnum::GetValueAsString(Id.Type), *UEnum::GetValueAsString(Id.Tier));
	}

	template <class TAsset>
	TArray<TAsset *> Resolve(const TArray<FName> &Names)
	{
		TArray<TAsset *> Out;
		for (const FName Name : Names)
		{
			if (TAsset *Asset = Cast<TAsset>(WoRTestSet::FindAsset(TAsset::StaticClass(), Name)))
			{
				Out.Add(Asset);
			}
		}
		return Out;
	}

	template <class TAsset>
	TAsset *ResolveOne(FName Name)
	{
		return Name.IsNone() ? nullptr : Cast<TAsset>(WoRTestSet::FindAsset(TAsset::StaticClass(), Name));
	}

	void AddStockLines(TArray<FMerchantStockEntry> &Stock, const TArray<UPrimaryDataAsset *> &Assets)
	{
		for (UPrimaryDataAsset *Asset : Assets)
		{
			Stock.AddDefaulted_GetRef().Asset = Asset;
		}
	}

	template <class TAsset>
	TArray<UPrimaryDataAsset *> AsStock(const TArray<TAsset *> &Assets)
	{
		return TArray<UPrimaryDataAsset *>(Assets);
	}

	/** Runs Change on the active loadout through the screen's door; the refusal (the door's or the change's) in Lines. */
	bool ChangeGear(ULoadoutComponent &Loadout, const UInventoryComponent &Inventory, TFunctionRef<bool(FCombatLoadout &, FText &)> Change, const TCHAR *What,
					TArray<FString> &Lines)
	{
		bool bApplied = false;
		FText ChangeRefusal;
		FText DoorRefusal;
		const bool bDone = Loadout.TryChangeGear(Inventory.ActiveLoadoutIndex, [&](FCombatLoadout &Changed) { bApplied = Change(Changed, ChangeRefusal); }, DoorRefusal);
		Lines.Add(FString::Printf(TEXT("%s: %s"), What, bDone && bApplied ? TEXT("in") : *FString::Printf(TEXT("REFUSED (%s)"), *(bDone ? ChangeRefusal : DoorRefusal).ToString())));
		return bDone && bApplied;
	}

	/** The owned gear the character had, by name, and its ids (weapons and adapters move to the chest; evolutions can't
	 *  yet, Q-323). */
	struct FOldGear
	{
		TArray<FGuid> Movable;
		TArray<FString> Names;
	};

	FOldGear ReadOldGear(const UPoolSubsystem &Pool, FName Key)
	{
		FOldGear Old;
		const FPoolGearView View = Pool.GetOwnedGearOf(Key);
		for (const FWeaponInventoryEntry *Weapon : View.Weapons)
		{
			Old.Movable.Add(Weapon->PersistentID);
			Old.Names.Add(Weapon->Weapon ? Weapon->Weapon->Name : TEXT("?"));
		}
		for (const FRingInventoryEntry *Adapter : View.Adapters)
		{
			Old.Movable.Add(Adapter->PersistentID);
			Old.Names.Add(Adapter->Ring ? Adapter->Ring->Name : TEXT("?"));
		}
		for (const FEvolutionInventoryEntry *Evolution : View.Evolutions)
		{
			Old.Names.Add(Evolution->Item ? Evolution->Item->ItemName : TEXT("?"));
		}
		return Old;
	}

	FString SkillNames(const FPoolSkills *Skills)
	{
		TArray<FString> Names;
		for (const FSpellInstance &Spell : Skills ? Skills->Spells : TArray<FSpellInstance>())
		{
			Names.Add(Spell.Spell ? Spell.Spell->Name : TEXT("?"));
		}
		for (const FAbilityInstance &Ability : Skills ? Skills->Abilities : TArray<FAbilityInstance>())
		{
			Names.Add(Ability.Ability ? Ability.Ability->Name : TEXT("?"));
		}
		return Names.IsEmpty() ? FString(TEXT("none")) : FString::Join(Names, TEXT(", "));
	}

	/** An owned copy of Asset used again (not a class kit piece, not taken yet, and with Want set: its crystal socket empty
	 *  or holding Want), so a second run doesn't pile copies in the chest; else Give() gives a fresh one through the
	 *  inventory (it reaches the pool as the character's). Its id, invalid when refused. */
	template <class TEntry, class TAsset>
	FGuid ReuseOrGive(const TArray<TEntry> &Owned, TAsset *TEntry::*Field, const TAsset *Asset, const TSet<FGuid> &Taken, const UPoolSubsystem &Pool,
					  const FCrystalId *Want, TFunctionRef<bool()> Give)
	{
		const TEntry *Found = Owned.FindByPredicate([&](const TEntry &Entry)
		{
			return Entry.*Field == Asset && !Taken.Contains(Entry.PersistentID) && !Pool.IsClassDefault(Entry.PersistentID) &&
				   (!Want || Entry.AttachedItem.IsEmpty() || (Entry.AttachedItem.IsCrystal() && Entry.AttachedItem.Crystal.Id == *Want));
		});
		if (Found)
		{
			return Found->PersistentID;
		}
		return Give() ? Owned.Last().PersistentID : FGuid();
	}

#if !UE_BUILD_SHIPPING
	FAutoConsoleCommandWithWorld GTestSetCmd(
		TEXT("wor.Debug.TestSet"),
		TEXT("DEBUG (B-634): the played character's bag, worn gear and learned skills become the current test set "
			 "(Config/DefaultGame.ini [/Script/world_of_refraction.WoRTestSet]); every currency 9999; saves. Replaced gear goes to "
			 "the home chest, replaced items to storage; learned skills outside the set are overwritten (a set skill already known "
			 "keeps its tier). The log lists what was replaced. Hub only."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				const FString Report = WoRTestSet::Apply(World);
				UE_LOG(LogWoR, Display, TEXT("[TestSet]\n%s"), *Report);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, TEST_SET_SCREEN_SECONDS, FColor::Cyan, Report);
				}
			}));

	FAutoConsoleCommand GPrintTestSetCmd(
		TEXT("wor.Debug.PrintTestSet"),
		TEXT("DEBUG (B-634): lists the current test set and any name that resolves to no asset."),
		FConsoleCommandDelegate::CreateLambda(
			[]()
			{
				UE_LOG(LogWoR, Display, TEXT("%s"), *WoRTestSet::GetTestSetString(*GetDefault<UWoRTestSet>()));
			}));
#endif // !UE_BUILD_SHIPPING
}

UObject *WoRTestSet::FindAsset(const UClass *Class, FName AssetName)
{
	if (!Class || AssetName.IsNone())
	{
		return nullptr;
	}
	TArray<FAssetData> Found;
	IAssetRegistry::GetChecked().GetAssetsByClass(Class->GetClassPathName(), Found, /*bSearchSubClasses=*/true);
	const TArray<FAssetData> Named = Found.FilterByPredicate([AssetName](const FAssetData &Asset) { return Asset.AssetName == AssetName; });
	if (Named.Num() != 1)
	{
		UE_LOG(LogWoR, Warning, TEXT("[TestSet] %s %s: %d asset(s) found with that name (need exactly one)"), *Class->GetName(), *AssetName.ToString(), Named.Num());
		return nullptr;
	}
	return Named[0].GetAsset();
}

TArray<FString> WoRTestSet::FindUnresolvedNames(const UWoRTestSet &Set)
{
	TArray<FString> Missing;
	const auto Check = [&Missing](const TCHAR *List, const UClass *Class, const TArray<FName> &Names)
	{
		for (const FName Name : Names)
		{
			if (!FindAsset(Class, Name))
			{
				Missing.Add(FString::Printf(TEXT("%s: %s"), List, *Name.ToString()));
			}
		}
	};
	Check(TEXT("Weapons"), UWeaponData::StaticClass(), Set.Weapons);
	Check(TEXT("Adapters"), URingData::StaticClass(), Set.Adapters);
	Check(TEXT("Evolutions"), UEvolutionItemData::StaticClass(), Set.Evolutions);
	Check(TEXT("Abilities"), UAbilityData::StaticClass(), Set.Abilities);
	Check(TEXT("Spells"), USpellData::StaticClass(), Set.Spells);
	Check(TEXT("GenericEvolution"), UEvolutionItemData::StaticClass(), {Set.GenericEvolution});
	Check(TEXT("GenericWeapon"), UWeaponData::StaticClass(), {Set.GenericWeapon});
	Check(TEXT("GenericOffHand"), UWeaponData::StaticClass(), {Set.GenericOffHand});
	Check(TEXT("GenericAdapter"), URingData::StaticClass(), {Set.GenericAdapter});
	return Missing;
}

TArray<FMerchantStockEntry> WoRTestSet::GetStock(const UWoRTestSet &Set, EMerchantType Vendor)
{
	TArray<FMerchantStockEntry> Stock;
	const auto AddItems = [&Set, &Stock](TFunctionRef<bool(ECrystalType)> Sells)
	{
		for (const FWoRTestSetItem &Item : Set.Items)
		{
			if (Sells(Item.Crystal.Type))
			{
				FMerchantStockEntry &Line = Stock.AddDefaulted_GetRef();
				Line.Crystal = Item.Crystal;
				Line.Count = Item.Count;
			}
		}
	};
	switch (Vendor)
	{
	case EMerchantType::Blacksmith:
		AddStockLines(Stock, AsStock(Resolve<UWeaponData>(Set.Weapons)));
		break;
	case EMerchantType::Jeweler:
		AddStockLines(Stock, AsStock(Resolve<URingData>(Set.Adapters)));
		break;
	case EMerchantType::CombatMaster:
		AddStockLines(Stock, AsStock(Resolve<UAbilityData>(Set.Abilities)));
		break;
	case EMerchantType::SpellShop:
		AddStockLines(Stock, AsStock(Resolve<USpellData>(Set.Spells)));
		break;
	case EMerchantType::Spiritualist:
		AddStockLines(Stock, AsStock(Resolve<UEvolutionItemData>(Set.Evolutions)));
		AddItems([](ECrystalType Type) { return CrystalTypeHelpers::IsGemType(Type); });
		break;
	case EMerchantType::StoneSmith:
		AddItems([](ECrystalType Type) { return CrystalTypeHelpers::IsAugmentStoneType(Type); });
		break;
	default:
		break; // the loadout vendor sells built loadouts, not pieces: its own stock stays
	}
	return Stock;
}

bool WoRTestSet::WearGenericKit(FCombatLoadout &Loadout, const FEvolutionInventoryEntry &Evolution, const FWeaponInventoryEntry &Weapon,
								const FWeaponInventoryEntry &OffHand, const FRingInventoryEntry &Adapter, FText &OutRefusal)
{
	using namespace WoRLoadoutV2;
	constexpr ECharacterClass Generic = ECharacterClass::Generic;
	int32 CellsAfter = 0;
	RemoveAllGear(Loadout, Generic);

	FLoadoutV2Draft Seat;
	Seat.Kind = ELoadoutV2DraftKind::Evolution;
	Seat.EvolutionInstance = Evolution.InstanceID;
	Seat.Socket1 = MakeEvolutionAttachment(Evolution);
	// The first entry never empties (D-116): it is replaced by the evolution, or the evolution goes in an empty loadout.
	const TArray<FLoadoutV2Entry> Entries = BuildWeaponsPage(Loadout, Generic).Entries;
	if (Entries.IsEmpty())
	{
		OutRefusal = CheckDraft(Loadout, Generic, Seat, CellsAfter);
		if (!OutRefusal.IsEmpty() || !ApplyDraft(Loadout, Generic, Seat, OutRefusal))
		{
			return false;
		}
	}
	else
	{
		OutRefusal = CheckEntryEdit(Loadout, Generic, Entries[0], Seat, CellsAfter);
		if (!OutRefusal.IsEmpty() || !ReplaceEntry(Loadout, Generic, Entries[0], Seat, OutRefusal))
		{
			return false;
		}
	}

	FLoadoutV2Draft Arms;
	Arms.Kind = ELoadoutV2DraftKind::Weapon;
	Arms.MainHand = Weapon;
	Arms.OffHand = OffHand;
	Arms.Socket1 = Weapon.AttachedItem; // the sockets stay on their piece (Q-476c)
	Arms.Socket2 = Weapon.StoneSocket;
	OutRefusal = CheckDraft(Loadout, Generic, Arms, CellsAfter);
	if (!OutRefusal.IsEmpty() || !ApplyDraft(Loadout, Generic, Arms, OutRefusal))
	{
		return false;
	}

	FLoadoutV2Draft Ring;
	Ring.Kind = ELoadoutV2DraftKind::Adapter;
	Ring.Adapter = Adapter;
	Ring.Socket1 = Adapter.AttachedItem;
	Ring.Socket2 = Adapter.StoneSocket;
	OutRefusal = CheckDraft(Loadout, Generic, Ring, CellsAfter);
	return OutRefusal.IsEmpty() && ApplyDraft(Loadout, Generic, Ring, OutRefusal);
}

FString WoRTestSet::Apply(UWorld *World)
{
	const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn *Pawn = PC ? PC->GetPawn() : nullptr;
	UInventoryComponent *Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	ULoadoutComponent *Loadout = Pawn ? Pawn->FindComponentByClass<ULoadoutComponent>() : nullptr;
	const UEvolutionInventoryComponent *Evolutions = Pawn ? Pawn->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
	const UCharacterDataComponent *DataComponent = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
	UPoolSubsystem *Pool = GameInstance ? GameInstance->GetSubsystem<UPoolSubsystem>() : nullptr;
	if (!Inventory || !Loadout || !Evolutions || !DataComponent || !DataComponent->CharacterData || !Pool ||
		!Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
	{
		return TEXT("no played character with an inventory, a loadout and a pool here");
	}
	if (World->GetNetMode() == NM_Client)
	{
		return TEXT("only the host can set the test set");
	}
	if (Pool->IsRunLayerActive())
	{
		return TEXT("a run is open: gear given now would be run-only; use it in the hub");
	}

	const UWoRTestSet &Set = *GetDefault<UWoRTestSet>();
	const FName Key = UPoolSubsystem::GetCharacterKey(DataComponent);
	const FName PouchKey = WoRGive::GetPouchKey(World, Key);
	const ECharacterClass Class = DataComponent->CharacterData->CharacterClass;
	TArray<FString> Lines;
	for (const FString &Missing : FindUnresolvedNames(Set))
	{
		Lines.Add(FString::Printf(TEXT("not found, skipped: %s"), *Missing));
	}

	// What it replaces, first (B-634: logged so it can be put back by hand).
	const FOldGear Old = ReadOldGear(*Pool, Key);
	Lines.Add(FString::Printf(TEXT("replaced gear: %s"), Old.Names.IsEmpty() ? TEXT("none") : *FString::Join(Old.Names, TEXT(", "))));
	if (!Pool->HasBankedSkills(Key))
	{
		Pool->BankSkills(Key, Inventory->Spells.LearnedSpells, Inventory->Abilities.LearnedAbilities);
	}
	Lines.Add(FString::Printf(TEXT("replaced skills: %s"), *SkillNames(Pool->FindSkills(Key))));
	const TMap<FCrystalId, int32> OldItems = Pool->GetCarriedItems(PouchKey);
	TArray<FString> ItemNames;
	for (const TPair<FCrystalId, int32> &Stack : OldItems)
	{
		ItemNames.Add(FString::Printf(TEXT("%s x%d"), *CrystalName(Stack.Key), Stack.Value));
		Pool->MoveToStorage(PouchKey, Stack.Key, Stack.Value);
	}
	Lines.Add(FString::Printf(TEXT("replaced pouch (now in storage): %s"), ItemNames.IsEmpty() ? TEXT("none") : *FString::Join(ItemNames, TEXT(", "))));

	// The pouch: the set's items and the two kit crystals (a socket takes its crystal from the pouch).
	for (const FWoRTestSetItem &Item : Set.Items)
	{
		Pool->CarryOrStore(PouchKey, Item.Crystal, Item.Count);
	}

	// The bag: the set's gear, each once (an owned copy is used again). The Generic's sword and adapter need their socket
	// empty or already holding the set's crystal.
	const bool bGeneric = Class == ECharacterClass::Generic;
	TMap<FName, FGuid> Given;
	TSet<FGuid> Taken;
	for (UWeaponData *Weapon : Resolve<UWeaponData>(Set.Weapons))
	{
		const FCrystalId *Want = bGeneric && Weapon->GetFName() == Set.GenericWeapon ? &Set.GenericWeaponCrystal : nullptr;
		const FGuid ID = ReuseOrGive(Inventory->Weapons, &FWeaponInventoryEntry::Weapon, Weapon, Taken, *Pool, Want, [&]() { return Inventory->AddWeapon(Weapon); });
		Given.Add(Weapon->GetFName(), ID);
		Taken.Add(ID);
	}
	for (URingData *Ring : Resolve<URingData>(Set.Adapters))
	{
		const FCrystalId *Want = bGeneric && Ring->GetFName() == Set.GenericAdapter ? &Set.GenericAdapterCrystal : nullptr;
		const FGuid ID = ReuseOrGive(Inventory->Rings, &FRingInventoryEntry::Ring, Ring, Taken, *Pool, Want, [&]() { return Inventory->AddRing(Ring); });
		Given.Add(Ring->GetFName(), ID);
		Taken.Add(ID);
	}
	FGuid SeatID;
	for (UEvolutionItemData *Evolution : Resolve<UEvolutionItemData>(Set.Evolutions))
	{
		// CL-15: at most 5 owned; one already owned of this asset is used again (evolutions can't go to the chest yet).
		const FEvolutionInventoryEntry *Owned = Evolutions->Entries.FindByPredicate([Evolution](const FEvolutionInventoryEntry &E) { return E.Item == Evolution; });
		const bool bGiven = !Owned && Inventory->AddEvolutionInstance(Evolution);
		const FGuid ID = Owned ? Owned->InstanceID : bGiven ? Evolutions->Entries.Last().InstanceID : FGuid();
		Lines.Add(FString::Printf(TEXT("evolution %s: %s"), *Evolution->ItemName, Owned ? TEXT("already owned") : bGiven ? TEXT("given") : TEXT("REFUSED (5 owned, CL-15)")));
		if (Evolution->GetFName() == Set.GenericEvolution)
		{
			SeatID = ID;
		}
	}

	// A Generic's three sources (Crown, 2026-11-25): the evolution primary, the sword and shield with a crystal, the adapter with one.
	bool bKitWorn = true;
	if (bGeneric)
	{
		const FGuid WeaponID = Given.FindRef(Set.GenericWeapon);
		const FGuid OffHandID = Given.FindRef(Set.GenericOffHand);
		const FGuid AdapterID = Given.FindRef(Set.GenericAdapter);
		// A socket takes its crystal from the pouch; a reused piece may hold it already.
		const auto Socket = [&](bool bEmpty, const FCrystalId &Crystal, TFunctionRef<bool()> Attach)
		{
			if (!bEmpty)
			{
				return TEXT("already in");
			}
			Pool->CarryOrStore(PouchKey, Crystal, 1);
			return Attach() ? TEXT("in") : TEXT("REFUSED");
		};
		const FWeaponInventoryEntry *OwnedWeapon = Inventory->FindOwnedWeapon(WeaponID);
		const FRingInventoryEntry *OwnedAdapter = Inventory->FindOwnedRing(AdapterID);
		const TCHAR *WeaponSocket = Socket(OwnedWeapon && OwnedWeapon->AttachedItem.IsEmpty(), Set.GenericWeaponCrystal,
										   [&]() { return Inventory->AttachCrystalToWeapon(WeaponID, Set.GenericWeaponCrystal); });
		const TCHAR *AdapterSocket = Socket(OwnedAdapter && OwnedAdapter->AttachedItem.IsEmpty(), Set.GenericAdapterCrystal,
											[&]() { return Inventory->AttachCrystalToRing(AdapterID, Set.GenericAdapterCrystal); });
		Lines.Add(FString::Printf(TEXT("sockets: weapon %s %s, adapter %s %s"), *CrystalName(Set.GenericWeaponCrystal), WeaponSocket,
								  *CrystalName(Set.GenericAdapterCrystal), AdapterSocket));
		const FWeaponInventoryEntry *Weapon = Inventory->FindOwnedWeapon(WeaponID);
		const FWeaponInventoryEntry *OffHand = Inventory->FindOwnedWeapon(OffHandID);
		const FRingInventoryEntry *Adapter = Inventory->FindOwnedRing(AdapterID);
		const FEvolutionInventoryEntry *Seat = Evolutions->Entries.FindByPredicate([&SeatID](const FEvolutionInventoryEntry &E) { return E.InstanceID == SeatID; });
		if (Weapon && OffHand && Adapter && Seat)
		{
			// Copies: the inventory's arrays may move while the loadout changes.
			const FWeaponInventoryEntry WeaponCopy = *Weapon;
			const FWeaponInventoryEntry OffHandCopy = *OffHand;
			const FRingInventoryEntry AdapterCopy = *Adapter;
			const FEvolutionInventoryEntry SeatCopy = *Seat;
			bKitWorn = ChangeGear(*Loadout, *Inventory,
					   [&](FCombatLoadout &Changed, FText &Refusal)
					   {
						   FCombatLoadout Kit = Changed; // all or nothing: a refusal leaves the worn gear as it was
						   if (!WearGenericKit(Kit, SeatCopy, WeaponCopy, OffHandCopy, AdapterCopy, Refusal))
						   {
							   return false;
						   }
						   Changed = Kit;
						   return true;
					   },
					   TEXT("three sources (evolution primary, sword and shield, adapter)"), Lines);
		}
		else
		{
			Lines.Add(TEXT("three sources: REFUSED (a Generic piece of the set was not given; see above)"));
			bKitWorn = false;
		}
	}
	else
	{
		Lines.Add(FString::Printf(TEXT("worn gear: kept (the three-source kit is the Generic's; this is %s)"), *UEnum::GetValueAsString(Class)));
	}
	if (!bKitWorn)
	{
		// The gear and items are given; nothing more is changed or saved, so the refusal above can be read and fixed.
		Lines.Add(TEXT("STOPPED before the chest, the skills, the currencies and the save: the three sources were refused (see above)"));
		return FString::Join(Lines, TEXT("\n"));
	}

	// The old gear that isn't worn now goes to the home chest (nothing leaves the pool; GiveGear also takes it out of the
	// character's other loadouts). A class kit piece stays its own; a reused set piece stays in the bag.
	const FWornGear *Worn = Pool->GetWornGear(Key);
	const TArray<FGuid> WornIDs = Worn ? Worn->GetPieceIDs() : TArray<FGuid>();
	int32 Chested = 0;
	for (const FGuid &ID : Old.Movable)
	{
		if (!WornIDs.Contains(ID) && !Taken.Contains(ID) && !Pool->IsClassDefault(ID) && Pool->GiveGear(ID, UPoolSubsystem::GetChestKey()))
		{
			++Chested;
		}
	}
	Lines.Add(FString::Printf(TEXT("old gear to the home chest: %d of %d"), Chested, Old.Movable.Num()));

	// The learned skills: exactly the set's (the member reloads them from the store). One already known keeps its instance
	// (its tier); the others are overwritten, their names in "replaced skills" above.
	const TArray<UAbilityData *> SetAbilities = Resolve<UAbilityData>(Set.Abilities);
	const TArray<USpellData *> SetSpells = Resolve<USpellData>(Set.Spells);
	const FPoolSkills *Known = Pool->FindSkills(Key);
	const TArray<FSpellInstance> KeptSpells = Known ? Known->Spells.FilterByPredicate([&SetSpells](const FSpellInstance &S) { return SetSpells.Contains(S.Spell); })
													: TArray<FSpellInstance>();
	const TArray<FAbilityInstance> KeptAbilities = Known ? Known->Abilities.FilterByPredicate([&SetAbilities](const FAbilityInstance &A) { return SetAbilities.Contains(A.Ability); })
														 : TArray<FAbilityInstance>();
	Pool->SetSkills(Key, KeptSpells, KeptAbilities);
	TArray<FString> Learned;
	TArray<FString> Refused;
	for (UAbilityData *Ability : SetAbilities)
	{
		(Pool->KnowsAbility(Key, Ability) || Pool->LearnSkillCopy(Key, Ability) ? Learned : Refused).Add(Ability->Name);
	}
	for (USpellData *Spell : SetSpells)
	{
		(Pool->KnowsSpell(Key, Spell) || Pool->LearnSkillCopy(Key, Spell) ? Learned : Refused).Add(Spell->Name);
	}
	Lines.Add(FString::Printf(TEXT("skills learned: %s%s"), Learned.IsEmpty() ? TEXT("none") : *FString::Join(Learned, TEXT(", ")),
							  Refused.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("; REFUSED: %s"), *FString::Join(Refused, TEXT(", ")))));

	if (UCurrencyComponent *Wallet = CurrencyWallet::FindFor(Pawn))
	{
		const TArray<FString> Raised = WoRMaxResources::TopUp(*Wallet);
		Lines.Add(FString::Printf(TEXT("currencies: %s"), Raised.IsEmpty() ? TEXT("all 9999 already") : *FString::Join(Raised, TEXT(", "))));
	}
	USaveSubsystem *Save = GameInstance->GetSubsystem<USaveSubsystem>();
	Lines.Add(Save && Save->SaveNow(World, /*bAsync=*/false) ? TEXT("saved") : TEXT("NOT saved"));
	return FString::Join(Lines, TEXT("\n"));
}

FString WoRTestSet::GetTestSetString(const UWoRTestSet &Set)
{
	const auto Join = [](const TArray<FName> &Names)
	{
		TArray<FString> Out;
		for (const FName Name : Names)
		{
			Out.Add(Name.ToString());
		}
		return Out.IsEmpty() ? FString(TEXT("none")) : FString::Join(Out, TEXT(", "));
	};
	TArray<FString> Items;
	for (const FWoRTestSetItem &Item : Set.Items)
	{
		Items.Add(FString::Printf(TEXT("%s x%d"), *CrystalName(Item.Crystal), Item.Count));
	}
	const TArray<FString> Missing = FindUnresolvedNames(Set);
	return FString::Printf(TEXT("[TestSet] weapons: %s\n  adapters: %s\n  evolutions: %s\n  abilities: %s\n  spells: %s\n  items: %s\n"
								"  Generic: %s primary; %s + %s with %s; %s with %s\n  not found: %s"),
						   *Join(Set.Weapons), *Join(Set.Adapters), *Join(Set.Evolutions), *Join(Set.Abilities), *Join(Set.Spells),
						   Items.IsEmpty() ? TEXT("none") : *FString::Join(Items, TEXT(", ")), *Set.GenericEvolution.ToString(), *Set.GenericWeapon.ToString(),
						   *Set.GenericOffHand.ToString(), *CrystalName(Set.GenericWeaponCrystal), *Set.GenericAdapter.ToString(),
						   *CrystalName(Set.GenericAdapterCrystal), Missing.IsEmpty() ? TEXT("none") : *FString::Join(Missing, TEXT(", ")));
}
