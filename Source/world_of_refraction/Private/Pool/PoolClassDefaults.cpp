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

	/** A kit piece leaves the store: a crystal in its socket goes to its owner's pouch or storage first (never lost). The
	 *  worn record keeps naming it until the next trial start writes the kit (B-512). */
	void RemoveKitPiece(UPoolSubsystem &Pool, const FGuid &ID)
	{
		const FRuntimeAttachedItem *Socket = nullptr;
		if (const FWeaponInventoryEntry *Weapon = Pool.FindWeaponInPool(ID))
		{
			Socket = &Weapon->AttachedItem;
		}
		else if (const FRingInventoryEntry *Ring = Pool.FindRingInPool(ID))
		{
			Socket = &Ring->AttachedItem;
		}
		else if (const FApparelEntry *Apparel = Pool.FindApparelInPool(ID))
		{
			Socket = &Apparel->AttachedItem;
		}
		if (Socket && Socket->IsCrystal())
		{
			Pool.CarryOrStore(Pool.GetGearOwner(ID), Socket->Crystal.Id, 1);
		}
		Pool.RemoveGear(ID);
	}

	/** A new owned weapon or adapter at the asset's tier, sockets empty, with the kit's skill picks. Invalid when Asset
	 *  is neither. */
	FGuid MintKitPiece(UPoolSubsystem &Pool, UEquipmentDataBase *Asset, const TArray<TObjectPtr<UAbilityData>> &SkillPicks, FName CharacterKey)
	{
		if (UWeaponData *Weapon = Cast<UWeaponData>(Asset))
		{
			FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Weapon);
			Entry.PersistentID = FGuid::NewGuid();
			Entry.AttachedItem = FRuntimeAttachedItem(); // the kit's sockets start empty (no crystal from nowhere)
			Entry.SkillPicks.Reset();
			for (UAbilityData *Pick : SkillPicks)
			{
				Entry.SkillPicks.Add(Pick);
			}
			Entry.bSkillSlotsActive = !SkillPicks.IsEmpty();
			Pool.SetGearOwner(Entry.PersistentID, CharacterKey); // whose piece it is before anyone hears of it
			Pool.UpdateWeapon(Entry);
			return Entry.PersistentID;
		}
		if (URingData *Ring = Cast<URingData>(Asset))
		{
			FRingInventoryEntry Entry = FRingInventoryEntry::CreateFromRing(Ring);
			Entry.PersistentID = FGuid::NewGuid();
			Entry.AttachedItem = FRuntimeAttachedItem();
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
									  [this, &Kit, CharacterKey](int32 I) { return MintKitPiece(*this, Kit.Pieces[I].Piece, Kit.Pieces[I].SkillPicks, CharacterKey); });
	Minted += ReconcileKitPlaces(*this, IDs.OffHands, Places, [&Kit](int32 I) -> const UObject * { return Kit.Pieces[I].OffHand; },
								 [this, &Kit, CharacterKey](int32 I) { return MintKitPiece(*this, Kit.Pieces[I].OffHand, {}, CharacterKey); });
	Minted += ReconcileKitPlaces(*this, IDs.Apparel, Kit.Apparel.Num(), [&Kit](int32 I) -> const UObject * { return Kit.Apparel[I]; },
								 [this, &Kit, CharacterKey](int32 I) { return MintKitApparel(*this, Kit.Apparel[I], CharacterKey); });
	if (Minted > 0)
	{
		UE_LOG(LogWoRLoadout, Log, TEXT("[ClassDefaults] %s: minted %d kit piece(s) (D-221)"), *CharacterKey.ToString(), Minted);
	}
	return ClassDefaultKits.Add(CharacterKey, MoveTemp(IDs));
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
	const auto Line = [this](const TCHAR *Label, const TArray<FGuid> &IDs)
	{
		FString Lines;
		for (int32 Index = 0; Index < IDs.Num(); ++Index)
		{
			if (IDs[Index].IsValid())
			{
				const UObject *Asset = StoredKitAsset(*this, IDs[Index]);
				Lines += FString::Printf(TEXT("\n    %s [%d] %s %s%s"), Label, Index, *GetNameSafe(Asset), *IDs[Index].ToString(),
										 Asset ? TEXT("") : TEXT(" (MISSING from the store)"));
			}
		}
		return Lines;
	};
	for (const TPair<FName, FClassDefaultKitIDs> &Pair : ClassDefaultKits)
	{
		Out += FString::Printf(TEXT("\n  %s"), *Pair.Key.ToString());
		Out += Line(TEXT("piece"), Pair.Value.Pieces) + Line(TEXT("off hand"), Pair.Value.OffHands) + Line(TEXT("apparel"), Pair.Value.Apparel);
	}
	return Out;
}
