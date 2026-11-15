// PoolClassDefaults.cpp
// D-221 stage 1c (B-511): the class default kit's pieces in the store, minted once per character. UPoolSubsystem's
// kit half, kept out of PoolSubsystem.cpp.

#include "Pool/PoolSubsystem.h"

#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/Entries/FApparelEntry.h"
#include "Skills/Definitions/AbilityData.h"
#include "WoRLog.h"

namespace
{
	/** The asset a stored weapon, adapter or apparel piece is an instance of; null when the ID isn't in the store. */
	const UObject *StoredKitAsset(const UPoolSubsystem &Pool, const FGuid &ID)
	{
		if (const FWeaponInventoryEntry *Weapon = Pool.FindWeaponInPool(ID))
		{
			return Weapon->Weapon;
		}
		if (const FRingInventoryEntry *Ring = Pool.FindRingInPool(ID))
		{
			return Ring->Ring;
		}
		const FApparelEntry *Apparel = Pool.FindApparelInPool(ID);
		return Apparel ? Apparel->Apparel.Get() : nullptr;
	}

	/** A crystal or stone in a kit piece's socket goes to its owner's pouch or storage (never lost). Returns 1 when one
	 *  went. shortcut: a fusion in a kit socket is cleared, not returned; return it if fusions reach kit pieces. */
	int32 FreeKitSocket(UPoolSubsystem &Pool, const FGuid &ID, const FRuntimeAttachedItem &Socket)
	{
		if (!Socket.IsCrystal() && !Socket.IsAugmentStone())
		{
			return 0;
		}
		Pool.CarryOrStore(Pool.GetGearOwner(ID), Socket.Crystal.Id, 1);
		return 1;
	}

	/** A kit piece leaves the store: a crystal in its socket goes to its owner's pouch or storage first (never lost). The
	 *  worn record keeps naming it until the next trial start writes the kit (B-512). */
	void RemoveKitPiece(UPoolSubsystem &Pool, const FGuid &ID)
	{
		if (const FWeaponInventoryEntry *Weapon = Pool.FindWeaponInPool(ID))
		{
			const FRuntimeAttachedItem Stone = Weapon->StoneSocket;
			FreeKitSocket(Pool, ID, Weapon->AttachedItem);
			FreeKitSocket(Pool, ID, Stone);
		}
		else if (const FRingInventoryEntry *Ring = Pool.FindRingInPool(ID))
		{
			const FRuntimeAttachedItem Stone = Ring->StoneSocket;
			FreeKitSocket(Pool, ID, Ring->AttachedItem);
			FreeKitSocket(Pool, ID, Stone);
		}
		else if (const FApparelEntry *Apparel = Pool.FindApparelInPool(ID))
		{
			FreeKitSocket(Pool, ID, Apparel->AttachedItem);
		}
		Pool.RemoveGear(ID);
	}

	/** A kit weapon as authored: the asset's tier, sockets empty (no crystal from nowhere). The kit's skill picks go on
	 *  the worn entry (WearClassDefaultKit, D-224), not the piece. */
	FWeaponInventoryEntry MakeKitWeapon(UWeaponData *Weapon, const FGuid &ID)
	{
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
		Entry.PersistentID = ID;
		Entry.AttachedItem = FRuntimeAttachedItem();
		Entry.StoneSocket = FRuntimeAttachedItem();
		return Entry;
	}

	FRingInventoryEntry MakeKitRing(URingData *Ring, const FGuid &ID)
	{
		FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(Ring);
		Entry.PersistentID = ID;
		Entry.AttachedItem = FRuntimeAttachedItem();
		Entry.StoneSocket = FRuntimeAttachedItem();
		return Entry;
	}

	/** B-512: a minted kit piece back to its authored state, same ID; its socketed crystals go to its owner first.
	 *  Returns how many crystals went. */
	int32 ResetKitPiece(UPoolSubsystem &Pool, const FGuid &ID)
	{
		int32 Freed = 0;
		if (const FWeaponInventoryEntry *Weapon = Pool.FindWeaponInPool(ID))
		{
			FWeaponInventoryEntry Reset = MakeKitWeapon(Weapon->Weapon, ID);
			Reset.InstanceID = Weapon->InstanceID;
			const FRuntimeAttachedItem Attached = Weapon->AttachedItem, Stone = Weapon->StoneSocket; // CarryOrStore broadcasts
			Freed = FreeKitSocket(Pool, ID, Attached) + FreeKitSocket(Pool, ID, Stone);
			Pool.UpdateWeapon(Reset);
		}
		else if (const FRingInventoryEntry *Ring = Pool.FindRingInPool(ID))
		{
			FRingInventoryEntry Reset = MakeKitRing(Ring->Ring, ID);
			Reset.InstanceID = Ring->InstanceID;
			const FRuntimeAttachedItem Attached = Ring->AttachedItem, Stone = Ring->StoneSocket;
			Freed = FreeKitSocket(Pool, ID, Attached) + FreeKitSocket(Pool, ID, Stone);
			Pool.UpdateRing(Reset);
		}
		return Freed;
	}

	/** A new owned weapon or adapter at the asset's tier, sockets empty. Invalid when Asset is neither. */
	FGuid MintKitPiece(UPoolSubsystem &Pool, UEquipmentDataBase *Asset, FName CharacterKey)
	{
		if (UWeaponData *Weapon = Cast<UWeaponData>(Asset))
		{
			const FWeaponInventoryEntry Entry = MakeKitWeapon(Weapon, FGuid::NewGuid());
			Pool.SetGearOwner(Entry.PersistentID, CharacterKey); // whose piece it is before anyone hears of it
			Pool.UpdateWeapon(Entry);
			return Entry.PersistentID;
		}
		if (URingData *Ring = Cast<URingData>(Asset))
		{
			const FRingInventoryEntry Entry = MakeKitRing(Ring, FGuid::NewGuid());
			Pool.SetGearOwner(Entry.PersistentID, CharacterKey);
			Pool.UpdateRing(Entry);
			return Entry.PersistentID;
		}
		if (Asset)
		{
			UE_LOG(LogWoRLoadout, Warning, TEXT("[ClassDefaults] %s is not a weapon or an adapter: its place stays empty"), *Asset->GetName());
		}
		return FGuid();
	}

	FGuid MintKitApparel(UPoolSubsystem &Pool, UApparelData *Apparel, FName CharacterKey)
	{
		if (!Apparel)
		{
			return FGuid();
		}
		FApparelEntry Entry = FApparelEntry::CreateFromApparel(Apparel);
		Entry.AttachedItem = FRuntimeAttachedItem();
		return Pool.AddApparelToPool(Entry, CharacterKey);
	}

	/** Keeps IDs place for place with the authored list: a place whose stored piece is still the authored asset keeps
	 *  it; otherwise the old piece leaves the store and MintAt makes the new one. Returns how many were minted. */
	int32 ReconcileKitPlaces(UPoolSubsystem &Pool, TArray<FGuid> &IDs, int32 NumPlaces, TFunctionRef<const UObject *(int32)> AuthoredAt,
							 TFunctionRef<FGuid(int32)> MintAt)
	{
		for (int32 Index = NumPlaces; Index < IDs.Num(); ++Index)
		{
			if (IDs[Index].IsValid())
			{
				RemoveKitPiece(Pool, IDs[Index]); // the kit lost this place
			}
		}
		IDs.SetNum(NumPlaces);
		int32 Minted = 0;
		for (int32 Index = 0; Index < NumPlaces; ++Index)
		{
			const UObject *Authored = AuthoredAt(Index);
			if (IDs[Index].IsValid() && Authored && StoredKitAsset(Pool, IDs[Index]) == Authored)
			{
				continue;
			}
			if (IDs[Index].IsValid())
			{
				RemoveKitPiece(Pool, IDs[Index]);
			}
			IDs[Index] = Authored ? MintAt(Index) : FGuid();
			Minted += IDs[Index].IsValid() ? 1 : 0;
		}
		return Minted;
	}
}

const FClassDefaultKitIDs &UPoolSubsystem::MintClassDefaultKit(FName CharacterKey, const FClassDefaultLoadout &Kit)
{
	// A copy, written back at the end: the store's broadcasts below may reach code that changes the map.
	FClassDefaultKitIDs IDs = ClassDefaultKits.FindRef(CharacterKey);
	const int32 Places = Kit.Pieces.Num();
	int32 Minted = ReconcileKitPlaces(*this, IDs.Pieces, Places, [&Kit](int32 I) -> const UObject * { return Kit.Pieces[I].Piece; },
									  [this, &Kit, CharacterKey](int32 I) { return MintKitPiece(*this, Kit.Pieces[I].Piece, CharacterKey); });
	Minted += ReconcileKitPlaces(*this, IDs.OffHands, Places, [&Kit](int32 I) -> const UObject * { return Kit.Pieces[I].OffHand; },
								 [this, &Kit, CharacterKey](int32 I) { return MintKitPiece(*this, Kit.Pieces[I].OffHand, CharacterKey); });
	Minted += ReconcileKitPlaces(*this, IDs.Apparel, Kit.Apparel.Num(), [&Kit](int32 I) -> const UObject * { return Kit.Apparel[I]; },
								 [this, &Kit, CharacterKey](int32 I) { return MintKitApparel(*this, Kit.Apparel[I], CharacterKey); });
	if (Minted > 0)
	{
		UE_LOG(LogWoRLoadout, Log, TEXT("[ClassDefaults] %s: minted %d kit piece(s) (D-221)"), *CharacterKey.ToString(), Minted);
	}
	return ClassDefaultKits.Add(CharacterKey, MoveTemp(IDs));
}

void UPoolSubsystem::ResetClassDefaultKit(FName CharacterKey, const FClassDefaultLoadout &Kit)
{
	const FClassDefaultKitIDs IDs = MintClassDefaultKit(CharacterKey, Kit); // a copy: the resets below broadcast
	int32 Freed = 0;
	for (int32 Index = 0; Index < IDs.Pieces.Num() && Kit.Pieces.IsValidIndex(Index); ++Index)
	{
		Freed += ResetKitPiece(*this, IDs.Pieces[Index]);
		Freed += IDs.OffHands.IsValidIndex(Index) ? ResetKitPiece(*this, IDs.OffHands[Index]) : 0;
	}
	// shortcut: apparel only has its socket cleared (its crystal goes to storage, AttachToApparel), not its tier reset;
	// kit apparel is F tier and a wipe can't drop it lower. Reset the tier if apparel ever gains tiers in a run.
	for (const FGuid &ID : IDs.Apparel)
	{
		const FApparelEntry *Apparel = FindApparelInPool(ID);
		if (Apparel && !Apparel->AttachedItem.IsEmpty())
		{
			Freed += AttachToApparel(ID, FRuntimeAttachedItem()) ? 1 : 0;
		}
	}
	UE_LOG(LogWoRLoadout, Log, TEXT("[ClassDefaults] %s: kit reset to its authored state, %d socketed item(s) back to the inventory (D-221)"),
		   *CharacterKey.ToString(), Freed);
}

TArray<FGuid> UPoolSubsystem::WearClassDefaultKit(FName CharacterKey, const FClassDefaultLoadout &Kit)
{
	const FClassDefaultKitIDs *IDs = ClassDefaultKits.Find(CharacterKey);
	if (!IDs)
	{
		UE_LOG(LogWoRLoadout, Warning, TEXT("[ClassDefaults] %s has no minted kit: nothing worn"), *CharacterKey.ToString());
		return {};
	}
	// The character's own evolution, innate spells and Broken Darkness pools stay: they aren't the kit's.
	FWornGear Worn = WornGear.FindRef(CharacterKey);
	Worn.Equipment.Reset();
	Worn.Equipment.SetNum(FMath::Max(IDs->Pieces.Num(), WornGearLayout::FIRST_ADAPTER_INDEX));
	for (int32 Index = 0; Index < IDs->Pieces.Num(); ++Index)
	{
		FLoadoutSlot &Slot = Worn.Equipment[Index];
		Slot.GearID = IDs->Pieces[Index];
		Slot.OffHandID = IDs->OffHands.IsValidIndex(Index) ? IDs->OffHands[Index] : FGuid();
		if (Kit.Pieces.IsValidIndex(Index))
		{
			Slot.AssignedAbilities.Append(Kit.Pieces[Index].SkillPicks); // D-224 (B-517): the picks are the worn entry's
		}
		if (const FWeaponInventoryEntry *Weapon = FindWeaponInPool(Slot.GearID))
		{
			Slot.Gear = FGearInstance::FromWeaponEntry(*Weapon);
		}
		else if (const FRingInventoryEntry *Ring = FindRingInPool(Slot.GearID))
		{
			Slot.Gear = FGearInstance::FromRingEntry(*Ring);
		}
	}
	Worn.ActiveGearID = IDs->Pieces.IsValidIndex(Kit.ActiveIndex) ? IDs->Pieces[Kit.ActiveIndex] : FGuid();
	Worn.Apparel = IDs->Apparel.FilterByPredicate([](const FGuid &ID) { return ID.IsValid(); });
	SetWornGear(CharacterKey, Worn);
	SetWheelPick(CharacterKey, FWheelPick());
	UE_LOG(LogWoRLoadout, Log, TEXT("[ClassDefaults] %s wears its class kit (D-221): %d piece(s), the wheel pick cleared"),
		   *CharacterKey.ToString(), Worn.GetPieceIDs().Num());
	return Worn.GetPieceIDs();
}

bool UPoolSubsystem::IsClassDefault(const FGuid &GearID) const
{
	for (const TPair<FName, FClassDefaultKitIDs> &Pair : ClassDefaultKits)
	{
		if (Pair.Value.Contains(GearID))
		{
			return true;
		}
	}
	return false;
}

FString UPoolSubsystem::GetClassDefaultKitsString() const
{
	FString Out = FString::Printf(TEXT("[ClassDefaults] %d character(s) with a minted kit"), ClassDefaultKits.Num());
	TArray<FGuid> Worn;
	const auto Line = [this, &Worn](const TCHAR *Label, const TArray<FGuid> &IDs)
	{
		FString Lines;
		for (int32 Index = 0; Index < IDs.Num(); ++Index)
		{
			if (IDs[Index].IsValid())
			{
				const UObject *Asset = StoredKitAsset(*this, IDs[Index]);
				Lines += FString::Printf(TEXT("\n    %s [%d] %s %s%s%s"), Label, Index, *GetNameSafe(Asset), *IDs[Index].ToString(),
										 Asset ? TEXT("") : TEXT(" (MISSING from the store)"), Worn.Contains(IDs[Index]) ? TEXT(" (worn now)") : TEXT(""));
			}
		}
		return Lines;
	};
	for (const TPair<FName, FClassDefaultKitIDs> &Pair : ClassDefaultKits)
	{
		const FWornGear *Record = WornGear.Find(Pair.Key);
		Worn = Record ? Record->GetPieceIDs() : TArray<FGuid>();
		Out += FString::Printf(TEXT("\n  %s"), *Pair.Key.ToString());
		Out += Line(TEXT("piece"), Pair.Value.Pieces) + Line(TEXT("off hand"), Pair.Value.OffHands) + Line(TEXT("apparel"), Pair.Value.Apparel);
	}
	return Out;
}
