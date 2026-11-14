// WeaponSetRules.cpp

#include "Loadout/WeaponSetRules.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Character/StanceData.h"

#define LOCTEXT_NAMESPACE "WoRWeaponSets"

namespace
{
	constexpr int32 WEAPON_SET_PAIR_SIZE = 2;
}

WeaponSetRules::EPairRefusal WeaponSetRules::CheckPair(const FWeaponInventoryEntry &Main, const FWeaponInventoryEntry &OffHand)
{
	if (!Main.Weapon || !OffHand.Weapon)
	{
		return EPairRefusal::MissingWeapon;
	}
	if (Main.PersistentID.IsValid() && Main.PersistentID == OffHand.PersistentID)
	{
		return EPairRefusal::SamePiece;
	}
	if (!Main.Weapon->CanBePaired() || !OffHand.Weapon->CanBePaired())
	{
		return EPairRefusal::NotPairable;
	}
	if (Main.Weapon->WeaponType != OffHand.Weapon->WeaponType)
	{
		return EPairRefusal::DifferentTypes;
	}
	return EPairRefusal::None;
}

FText WeaponSetRules::GetPairRefusalText(EPairRefusal Refusal)
{
	switch (Refusal)
	{
	case EPairRefusal::MissingWeapon:
		return LOCTEXT("RefuseMissingWeapon", "A pair needs a weapon in each hand.");
	case EPairRefusal::SamePiece:
		return LOCTEXT("RefuseSamePiece", "That weapon is already in the other hand.");
	case EPairRefusal::NotPairable:
		return LOCTEXT("RefuseNotPairable", "This weapon can't be dual wielded.");
	case EPairRefusal::DifferentTypes:
		return LOCTEXT("RefuseDifferentTypes", "Only two weapons of the same type can be dual wielded.");
	case EPairRefusal::None:
	default:
		return FText::GetEmpty();
	}
}

bool WeaponSetRules::CanStandAlone(const FWeaponLoadoutEntry &Set)
{
	const UWeaponData *Weapon = Set.WeaponEntry.Weapon;
	return !Weapon || Set.HasOffHand() || Weapon->CanBeHeldAlone();
}

int32 WeaponSetRules::GetPairTierSlots(EItemTier MainTier, EItemTier OffHandTier)
{
	// D-211 default (reference-backed): a half rounds down; integer division does it for these positive counts.
	return (CrystalEffectTable::SlotsForContainerTier(MainTier) + CrystalEffectTable::SlotsForContainerTier(OffHandTier)) / WEAPON_SET_PAIR_SIZE;
}

int32 WeaponSetRules::GetSetTierSlots(const FWeaponLoadoutEntry &Set)
{
	return Set.HasOffHand() ? GetPairTierSlots(Set.WeaponEntry.Tier, Set.OffHandEntry.Tier) : CrystalEffectTable::SlotsForContainerTier(Set.WeaponEntry.Tier);
}

TArray<UAbilityData *> WeaponSetRules::GetSetBuiltIns(const FWeaponLoadoutEntry &Set)
{
	TArray<UAbilityData *> BuiltIns = Set.WeaponEntry.Weapon ? Set.WeaponEntry.Weapon->PresetAbilities : TArray<UAbilityData *>();
	if (Set.HasOffHand())
	{
		for (UAbilityData *Ability : Set.OffHandEntry.Weapon->PresetAbilities)
		{
			if (Ability)
			{
				BuiltIns.AddUnique(Ability);
			}
		}
	}
	return BuiltIns;
}

EWeaponWieldMode WeaponSetRules::GetSetWieldMode(const FWeaponLoadoutEntry &Set)
{
	if (Set.HasOffHand())
	{
		return EWeaponWieldMode::Dual;
	}
	return Set.WeaponEntry.Weapon ? Set.WeaponEntry.Weapon->WieldMode : EWeaponWieldMode::Single;
}

UStanceData *WeaponSetRules::GetSetStance(const FWeaponLoadoutEntry &Set)
{
	const UWeaponData *Main = Set.WeaponEntry.Weapon;
	if (!Main)
	{
		return nullptr;
	}
	if (Set.HasOffHand() && !Main->DualWieldStance.IsNull())
	{
		if (UStanceData *DualStance = Main->DualWieldStance.LoadSynchronous())
		{
			return DualStance;
		}
	}
	return Main->WeaponStance;
}

#undef LOCTEXT_NAMESPACE
