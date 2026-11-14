// WeaponSetRules.h
// B-477 (D-209, D-210, D-211; Crown 2026-11-14): a Generic's weapon set: one weapon, or two one-handed weapons of the
// same type (dual wield) in one loadout entry. The entry's sockets are the main hand's and serve the pair; the pair
// gives both weapons' stats and one merged skill list sized by the average of the two tiers (D-211, rounded down)
// plus the entry's crystal's extra slots. The pair stands in the main hand's dual-wield stance (the first chosen).
// FWeaponLoadoutEntry::OffHandEntry holds the second weapon; these rules say whether it may be there.
// Debug: wor.Loadout.PrintWeaponSets (WeaponSetRulesDebug).

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Weapons/EWeaponWieldMode.h"

struct FWeaponInventoryEntry;
struct FWeaponLoadoutEntry;
class UStanceData;
class UAbilityData;
enum class EItemTier : uint8;

namespace WeaponSetRules
{
	/** Why two weapons can't be a set (D-209). */
	enum class EPairRefusal : uint8
	{
		None,
		MissingWeapon,   // a hand is empty
		SamePiece,       // the same owned weapon in both hands
		NotPairable,     // a SingleOnly weapon (EWeaponHandling)
		DifferentTypes   // only two of the same type pair (two swords, two daggers)
	};

	/** D-209: None when Main and OffHand may be held together, else the first reason they can't. */
	WORLD_OF_REFRACTION_API EPairRefusal CheckPair(const FWeaponInventoryEntry &Main, const FWeaponInventoryEntry &OffHand);

	/** Player-facing reason for a refusal (empty for None). */
	WORLD_OF_REFRACTION_API FText GetPairRefusalText(EPairRefusal Refusal);

	/** D-209: the set holds an off hand that CheckPair allows. The pair readers below use it; the class (a Generic's
	 *  sets only) is gated by the loadout check (FLoadoutRules, OffHandWeapon) and ULoadoutComponent::IsWeaponSetPaired. */
	WORLD_OF_REFRACTION_API bool IsValidPair(const FWeaponLoadoutEntry &Set);

	/** D-209: false for a DualOnly weapon held alone (an empty entry is not refused here). */
	WORLD_OF_REFRACTION_API bool CanStandAlone(const FWeaponLoadoutEntry &Set);

	/** D-211: the tier slots of a pair: the average of the two tiers' slots (CL-20), a half rounded down. */
	WORLD_OF_REFRACTION_API int32 GetPairTierSlots(EItemTier MainTier, EItemTier OffHandTier);

	/** The set's tier slots: the weapon's own (CL-20), or the pair's average (D-211). */
	WORLD_OF_REFRACTION_API int32 GetSetTierSlots(const FWeaponLoadoutEntry &Set);

	/** D-210: the set's built-in abilities, the main hand's then the off hand's not already listed. */
	WORLD_OF_REFRACTION_API TArray<UAbilityData *> GetSetBuiltIns(const FWeaponLoadoutEntry &Set);

	/** D-210: a pair holds single-blade and dual-blade abilities of its type: it reads as Dual; a weapon alone as its own
	 *  wield mode. */
	WORLD_OF_REFRACTION_API EWeaponWieldMode GetSetWieldMode(const FWeaponLoadoutEntry &Set);

	/** D-209: the stance the set stands in: a pair the main hand's DualWieldStance (its WeaponStance when unset), a
	 *  weapon alone its WeaponStance. Loads the soft stance on demand. */
	WORLD_OF_REFRACTION_API UStanceData *GetSetStance(const FWeaponLoadoutEntry &Set);
}
