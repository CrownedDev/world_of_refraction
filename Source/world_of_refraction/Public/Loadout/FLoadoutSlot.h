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

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FLoadoutSlot
{
	GENERATED_BODY()

	/** The owned piece this slot references (B-085: owned is the truth); invalid for an asset-built slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FGuid GearID;

	/** The gear in the slot (a copy of the owned instance, as today's entries are). */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FGearInstance Gear;

	/** Weapon only: the slot's chosen abilities and augment-stone abilities. */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	TArray<TObjectPtr<UAbilityData>> AssignedAbilities;

	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	TArray<TObjectPtr<UAbilityData>> AssignedAugmentStoneAbilities;

	/** Weapon only: today's loadout-level spell overrides (weapon spells go with B-098). */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	TArray<TObjectPtr<USpellData>> SpellOverrides;

	/** Weapon only: the per-loadout attack override. */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	TObjectPtr<USkillDataBase> OverrideAttack = nullptr;

	EGearKind Kind() const { return Gear.Kind; }
	bool IsEmpty() const { return !Gear.IsValid(); }

	static FLoadoutSlot FromWeaponLoadoutEntry(const FWeaponLoadoutEntry &Entry)
	{
		FLoadoutSlot Slot;
		Slot.Gear = FGearInstance::FromWeaponEntry(Entry.WeaponEntry);
		Slot.GearID = Slot.Gear.PersistentID;
		Slot.AssignedAbilities.Append(Entry.AssignedAbilities);
		Slot.AssignedAugmentStoneAbilities.Append(Entry.AssignedAugmentStoneAbilities);
		Slot.SpellOverrides.Append(Entry.AssignedSpells);
		Slot.OverrideAttack = Entry.OverrideAttack;
		return Slot;
	}

	static FLoadoutSlot FromRingLoadoutEntry(const FRingLoadoutEntry &Entry)
	{
		FLoadoutSlot Slot;
		Slot.Gear = FGearInstance::FromRingEntry(Entry.RingEntry);
		Slot.GearID = Slot.Gear.PersistentID;
		return Slot;
	}
};
