// FGearInstance.h
// D-110 unified loadout, step 1 (B-301): one owned-gear instance for a weapon or an adapter (today's ring). Today's
// FWeaponInventoryEntry and FRingInventoryEntry are field-for-field the same; this is that shape once, with the kind
// stored. Converters go to and from both entries so the new model can be built beside the old one. Nothing reads it
// yet (B-302 mirrors loadout slots onto it).

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Inventory/ItemTier.h"
#include "Inventory/ItemQuality.h"
#include "FGearInstance.generated.h"

class UEquipmentDataBase;
class UWeaponData;
class URingData;
class USpellData;
struct FWeaponInventoryEntry;
struct FRingInventoryEntry;

/** What a piece of gear is. Append-only (saved, B-308). Adapter = today's ring (D-107). */
UENUM(BlueprintType)
enum class EGearKind : uint8
{
	Weapon = 0,
	Adapter = 1
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FGearInstance
{
	GENERATED_BODY()

	/** The template: a UWeaponData for a weapon, a URingData for an adapter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear")
	TObjectPtr<UEquipmentDataBase> Asset = nullptr;

	/** Set from the asset's class when built; stored so a save can say what it holds. */
	UPROPERTY(BlueprintReadOnly, Category = "Gear")
	EGearKind Kind = EGearKind::Weapon;

	UPROPERTY(BlueprintReadOnly, Category = "Identity")
	int32 InstanceID = 0;

	/** The owned instance (B-085: owned is the truth); invalid for a loadout-inflated copy. */
	UPROPERTY(BlueprintReadOnly, Category = "Identity")
	FGuid PersistentID;

	UPROPERTY(BlueprintReadOnly, Category = "Identity")
	EItemTier Tier = EItemTier::F_Tier;

	UPROPERTY(BlueprintReadOnly, Category = "Identity")
	EItemQuality Quality = EItemQuality::F_Quality;

	/** D-110 (A): the attachment lives on the item and travels with it; the slot only references the item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear")
	FRuntimeAttachedItem AttachedItem;

	/** An adapter's spells (a weapon's go when B-098 lands, D-027). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gear")
	TArray<TObjectPtr<USpellData>> AssignedSpells;

	UPROPERTY(BlueprintReadWrite, Category = "Rolls")
	FEquipmentStatBonus StatBonus;

	UPROPERTY(BlueprintReadWrite, Category = "Rolls")
	FResistanceBonus ResistanceBonus;

	UPROPERTY(BlueprintReadWrite, Category = "Rolls")
	int32 StatPool = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Rolls")
	int32 StatMaxPool = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Rolls")
	int32 ResistancePool = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Rolls")
	int32 ResistanceMaxPool = 0;

	bool IsValid() const { return Asset != nullptr; }
	bool IsWeapon() const { return Kind == EGearKind::Weapon; }
	bool IsAdapter() const { return Kind == EGearKind::Adapter; }
	bool HasAttachment() const { return !AttachedItem.IsEmpty(); }

	/** The asset as a weapon / an adapter, or null when it's the other kind. */
	UWeaponData *GetWeapon() const;
	URingData *GetAdapter() const;

	/** One rule for both kinds (CL-24, D-110): the attachment's element, None when there is none or it is broken.
	 *  (Today's weapon entry skips the broken check; this is the drift the unification ends.) */
	ESpellElement GetElement() const;

	static FGearInstance FromWeaponEntry(const FWeaponInventoryEntry &Entry);
	static FGearInstance FromRingEntry(const FRingInventoryEntry &Entry);

	/** Back to today's entries; the wrong kind gives an empty entry. */
	FWeaponInventoryEntry ToWeaponEntry() const;
	FRingInventoryEntry ToRingEntry() const;
};
