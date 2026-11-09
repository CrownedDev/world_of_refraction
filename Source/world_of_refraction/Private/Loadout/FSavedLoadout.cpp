// FSavedLoadout.cpp
// Designer-authored loadout configuration — validation and accessors.
// Logic mirrors ULoadoutData (LoadoutData.cpp) to keep behaviour identical
// once UInventoryData replaces ULoadoutData as the asset source.

#include "Loadout/FSavedLoadout.h"

#include "Skills/Definitions/AbilityData.h"
#include "Loadout/FCombatLoadout.h"
#include "Inventory/InventoryConstants.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Loadout/LoadoutConstants.h"
#include "Loadout/SpellPoolConstants.h"
#include "Equipment/Rings/RingData.h"
#include "Skills/Definitions/SpellData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Loadout/LoadoutRules.h"

// ==================== VALIDATION ====================

bool FSavedLoadout::IsValidForClass(ECharacterClass CharacterClass) const
{
    return RequiredClass == CharacterClass;
}

TArray<FString> FSavedLoadout::GetValidationErrors(TOptional<bool> bCharacterIsBrokenDarkness, int32 SpellDiscount,
                                                  bool bCheckSpellBudget) const
{
    TArray<FString> Errors = FLoadoutRules::CollectAuthoredShapeErrors(*this);

    FLoadoutRuleContext Context;
    Context.CharacterClass = RequiredClass;
    Context.bBrokenDarkness = bCharacterIsBrokenDarkness.Get(BDSpellPools.Num() > 0);
    // D-080 = D: the owning character's authored discount (0 with no character). Element
    // castability is left to the fight. Ownership is UInventoryData's cross-check.
    Context.SpellDiscount = SpellDiscount;
    Context.bCheckSpellBudget = bCheckSpellBudget;
    Errors.Append(FLoadoutRules::ToReasons(
        FLoadoutRules::CollectFindings(FCombatLoadout::CreateFromSavedLoadout(*this), Context)));
    return Errors;
}

bool FSavedLoadout::HasValidationErrors() const
{
    return GetValidationErrors().Num() > 0;
}

// ==================== ACCESSORS ====================

TArray<USpellData *> FSavedLoadout::GetAllSpells() const
{
    TArray<USpellData *> Result;

    if (PrimarySlotType == EPrimarySlotType::Evolution)
    {
        Result.Append(FSpellRef::ExtractSpells(EvolutionSpells));
    }

    if (RequiredClass == ECharacterClass::Caster)
    {
        Result.Append(FSpellRef::ExtractSpells(InnateSpells));
    }

    if (RequiredClass == ECharacterClass::Resonator)
    {
        for (const FResonatorRingSlot &Slot : EquippedRings)
        {
            if (Slot.Ring)
            {
                Result.Append(Slot.Ring->DefaultSpells);
                Result.Append(FSpellRef::ExtractSpells(Slot.AssignedSpells));
            }
        }
    }

    return Result;
}

TArray<UAbilityData *> FSavedLoadout::GetAllAbilities() const
{
    TArray<UAbilityData *> Result;

    if (PrimarySlotType == EPrimarySlotType::Weapon)
    {
        Result.Append(PrimaryWeaponAbilities);
        Result.Append(PrimaryAugmentStoneAbilities);
    }

    if (RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon)
    {
        Result.Append(SecondaryWeaponAbilities);
        Result.Append(SecondaryAugmentStoneAbilities);
    }

    return Result;
}

UWeaponData *FSavedLoadout::GetPrimaryWeapon() const
{
    return (PrimarySlotType == EPrimarySlotType::Weapon) ? PrimaryWeapon : nullptr;
}

URingData *FSavedLoadout::GetPrimaryRing() const
{
    return (PrimarySlotType == EPrimarySlotType::Ring) ? PrimaryRing : nullptr;
}

UEvolutionItemData *FSavedLoadout::GetPrimaryEvolution() const
{
    return (PrimarySlotType == EPrimarySlotType::Evolution) ? PrimaryEvolution : nullptr;
}
