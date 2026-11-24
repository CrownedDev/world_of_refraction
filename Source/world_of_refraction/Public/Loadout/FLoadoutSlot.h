// FLoadoutSlot.h
// D-110 unified loadout, step 2 (B-302): one loadout slot holding a weapon or an adapter, replacing
// FWeaponLoadoutEntry + FRingLoadoutEntry. For now FCombatLoadout::Slots is a mirror built from the old fields
// (FCombatLoadout::RebuildSlotsFromLegacy); nothing reads it yet. B-303 onward move the rules onto it.

#pragma once

#include "CoreMinimal.h"
#include "Loadout/FGearInstance.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "FLoadoutSlot.generated.h"

class UAbilityData;
class USkillDataBase;
class USpellData;

/** B-304: which old FCombatLoadout field a mirrored slot came from, so the getters that still return the old entry
 *  types can hand back the real one. Goes with the old fields (B-309). */
enum class ELegacySlotOrigin : uint8
{
	None,
	PrimaryWeapon,
	SecondaryWeapon,
	PrimaryRing,
	RingLoadout
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FLoadoutSlot
{
	GENERATED_BODY()

	/** THE TRUTH (D-111, B-085): the owned item this slot holds, by its GUID; invalid only for an asset-built slot.
	 *  A slot never owns gear: an item that is gone or worn elsewhere leaves the slot empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FGuid GearID;

	/** D-221 (B-505): the second piece of a same-type pair (D-209 weapon sets); invalid for a piece alone. Replaces the
	 *  old fields' off hands once the readers move (stage 2). */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FGuid OffHandID;

	/** A DERIVED cache of the owned item GearID names (today copied from the old loadout entry). Never the source:
	 *  readers that need the item's live state (wear, tier, attachment) look the GUID up in the owned store. */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FGearInstance Gear;

	/** D-224 (B-517): the weapon entry's skill picks, slot by slot (null = an empty slot). The picks live here, on what
	 *  is worn, so a preset carries them and the piece doesn't: giving the piece away leaves them behind. */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	TArray<TObjectPtr<UAbilityData>> AssignedAbilities;

	/** D-224 (B-517): the entry's spell picks, slot by slot, for a weapon's or an adapter's crystal. */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	TArray<TObjectPtr<USpellData>> SpellOverrides;

	/** Weapon only: the per-loadout attack override. */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	TObjectPtr<USkillDataBase> OverrideAttack = nullptr;

	/** B-309d: the WornGearLayout place (WornGear.h: the primary, the secondary, then the adapter loadout by index), so
	 *  the old fields can be rebuilt from the slots (FCombatLoadout::RebuildLegacyFromSlots). INDEX_NONE: not placed. */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	int32 Place = INDEX_NONE;

	/** B-309d: a DERIVED cache of the off hand OffHandID names, as Gear is of GearID. */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FGearInstance OffHand;

	/** B-309d: the old weapon entry's own lists, carried so it rebuilds with no loss until the old fields go (B-309f):
	 *  OBSOLETE (B-351) the position-merge abilities, the entry's spell list and (OBSOLETE, B-392) the parry flag. */
	UPROPERTY()
	TArray<TObjectPtr<UAbilityData>> MergeAbilities;

	UPROPERTY()
	TArray<TObjectPtr<USpellData>> WeaponSpells;

	UPROPERTY()
	bool bUseWeaponParryAnimation = true;

	/** B-304: the old field this slot mirrors (RingLoadout: LegacyIndex is the ring's index). Not saved. */
	ELegacySlotOrigin LegacyOrigin = ELegacySlotOrigin::None;
	int32 LegacyIndex = INDEX_NONE;

	EGearKind Kind() const { return Gear.Kind; }
	bool IsEmpty() const { return !Gear.IsValid(); }

	static FLoadoutSlot FromWeaponLoadoutEntry(const FWeaponLoadoutEntry &Entry)
	{
		FLoadoutSlot Slot;
		Slot.Gear = FGearInstance::FromWeaponEntry(Entry.WeaponEntry);
		Slot.GearID = Slot.Gear.PersistentID;
		Slot.OffHandID = Entry.OffHandEntry.PersistentID;
		Slot.OffHand = FGearInstance::FromWeaponEntry(Entry.OffHandEntry);
		Slot.AssignedAbilities.Append(Entry.WeaponEntry.SkillPicks);
		Slot.SpellOverrides.Append(Entry.WeaponEntry.SpellPicks);
		Slot.OverrideAttack = Entry.OverrideAttack;
		Slot.MergeAbilities.Append(Entry.AssignedAbilities);
		Slot.WeaponSpells.Append(Entry.AssignedSpells);
		Slot.bUseWeaponParryAnimation = Entry.bUseWeaponParryAnimation;
		return Slot;
	}

	static FLoadoutSlot FromRingLoadoutEntry(const FRingLoadoutEntry &Entry)
	{
		FLoadoutSlot Slot;
		Slot.Gear = FGearInstance::FromRingEntry(Entry.RingEntry);
		Slot.GearID = Slot.Gear.PersistentID;
		Slot.SpellOverrides.Append(Entry.RingEntry.SpellPicks);
		return Slot;
	}

	/** B-309d: FromWeaponLoadoutEntry's way back; the picks come from this entry (D-224), not the cached piece. */
	FWeaponLoadoutEntry ToWeaponLoadoutEntry() const
	{
		FWeaponLoadoutEntry Entry;
		Entry.WeaponEntry = Gear.ToWeaponEntry();
		Entry.WeaponEntry.SkillPicks.Reset(AssignedAbilities.Num());
		Entry.WeaponEntry.SkillPicks.Append(AssignedAbilities);
		Entry.WeaponEntry.SpellPicks.Reset(SpellOverrides.Num());
		Entry.WeaponEntry.SpellPicks.Append(SpellOverrides);
		Entry.OffHandEntry = OffHand.ToWeaponEntry();
		Entry.OverrideAttack = OverrideAttack;
		Entry.AssignedAbilities.Append(MergeAbilities);
		Entry.AssignedSpells.Append(WeaponSpells);
		Entry.bUseWeaponParryAnimation = bUseWeaponParryAnimation;
		return Entry;
	}

	/** B-309d: FromRingLoadoutEntry's way back. */
	FRingLoadoutEntry ToRingLoadoutEntry() const
	{
		FRingLoadoutEntry Entry;
		Entry.RingEntry = Gear.ToRingEntry();
		Entry.RingEntry.SpellPicks.Reset(SpellOverrides.Num());
		Entry.RingEntry.SpellPicks.Append(SpellOverrides);
		return Entry;
	}
};
