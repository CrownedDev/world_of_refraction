// InventoryData.cpp
// Per-character inventory + saved loadouts. Validation cross-checks each
// FSavedLoadout against the asset's ownership lists.

#include "InventoryData.h"

#include "AbilityData.h"
#include "EvolutionItemData.h"
#include "RingData.h"
#include "SpellData.h"
#include "WeaponData.h"

// ==================== VALIDATION ====================

TArray<FString> UInventoryData::GetValidationErrors() const
{
    TArray<FString> Errors;

    if (SavedLoadouts.Num() > 0 &&
        !SavedLoadouts.IsValidIndex(DefaultActiveLoadoutIndex))
    {
        Errors.Add(FString::Printf(
            TEXT("DefaultActiveLoadoutIndex %d is out of range for SavedLoadouts (%d entries)"),
            DefaultActiveLoadoutIndex, SavedLoadouts.Num()));
    }

    for (int32 LoadoutIdx = 0; LoadoutIdx < SavedLoadouts.Num(); ++LoadoutIdx)
    {
        const FSavedLoadout &Loadout = SavedLoadouts[LoadoutIdx];
        const FString Prefix = FString::Printf(
            TEXT("Loadout[%d] '%s': "), LoadoutIdx, *Loadout.LoadoutName);

        // Struct-internal errors (slot consistency, count caps, BD rules)
        for (const FString &Err : Loadout.GetValidationErrors())
        {
            Errors.Add(Prefix + Err);
        }

        // Cross-check: every referenced asset must appear in its
        // ownership list.
        if (UWeaponData *Weapon = Loadout.GetPrimaryWeapon())
        {
            if (!Weapons.Contains(Weapon))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Primary weapon '%s' not in inventory Weapons list"),
                    *Weapon->GetName()));
            }
        }

        if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon)
        {
            if (!Weapons.Contains(Loadout.SecondaryWeapon))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Secondary weapon '%s' not in inventory Weapons list"),
                    *Loadout.SecondaryWeapon->GetName()));
            }
        }

        if (URingData *Ring = Loadout.GetPrimaryRing())
        {
            if (!Rings.Contains(Ring))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Primary ring '%s' not in inventory Rings list"),
                    *Ring->GetName()));
            }
        }

        if (UEvolutionItemData *Evolution = Loadout.GetPrimaryEvolution())
        {
            if (!Crystals.Contains(Evolution))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Primary evolution crystal '%s' not in inventory Crystals list"),
                    *Evolution->GetName()));
            }
        }

        for (const FResonatorRingSlot &Slot : Loadout.EquippedRings)
        {
            if (Slot.Ring && !Rings.Contains(Slot.Ring))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Equipped ring '%s' not in inventory Rings list"),
                    *Slot.Ring->GetName()));
            }
        }

        for (UEvolutionItemData *Item : Loadout.EquippedItems)
        {
            if (Item && !Items.Contains(Item))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Equipped item '%s' not in inventory Items list"),
                    *Item->GetName()));
            }
        }

        for (USpellData *Spell : Loadout.GetAllSpells())
        {
            if (Spell && !Spells.Contains(Spell))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Spell '%s' not in inventory Spells list"),
                    *Spell->GetName()));
            }
        }

        for (UAbilityData *Ability : Loadout.GetAllAbilities())
        {
            if (Ability && !Abilities.Contains(Ability))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Ability '%s' not in inventory Abilities list"),
                    *Ability->GetName()));
            }
        }
    }

    return Errors;
}

bool UInventoryData::HasValidationErrors() const
{
    return GetValidationErrors().Num() > 0;
}

// ==================== EDITOR ====================

#if WITH_EDITOR
EDataValidationResult UInventoryData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    for (const FString &Error : GetValidationErrors())
    {
        Context.AddError(FText::FromString(Error));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}

void UInventoryData::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Auto-name empty entries as "Loadout N" (1-indexed). Designer-set
    // names are preserved — only empties are filled.
    for (int32 i = 0; i < SavedLoadouts.Num(); ++i)
    {
        if (SavedLoadouts[i].LoadoutName.IsEmpty())
        {
            SavedLoadouts[i].LoadoutName = FString::Printf(TEXT("Loadout %d"), i + 1);
        }
    }

    // Mirrors ULoadoutData::PostEditChangeProperty per-loadout clearing
    // logic. USTRUCT-internal edits don't trigger PostEditChangeProperty on
    // the owning UObject — so we sweep every SavedLoadouts entry on any
    // edit. Editor-only, runs once per user interaction; perf is irrelevant.
    for (FSavedLoadout &Loadout : SavedLoadouts)
    {
        // Class-based clears
        if (Loadout.RequiredClass != ECharacterClass::Generic)
        {
            Loadout.SecondarySlotType = ESecondarySlotType::None;
            Loadout.SecondaryWeapon = nullptr;
            Loadout.SecondaryWeaponAbilities.Empty();
            Loadout.SecondaryWeaponStanceOverride = nullptr;
        }

        if (Loadout.RequiredClass != ECharacterClass::Caster)
        {
            Loadout.InnateSpells.Empty();
            Loadout.BDSpellPools.Empty();
        }

        if (Loadout.RequiredClass != ECharacterClass::Resonator)
        {
            Loadout.EquippedRings.Empty();
        }

        // Resonator cannot use Ring primary
        if (Loadout.RequiredClass == ECharacterClass::Resonator &&
            Loadout.PrimarySlotType == EPrimarySlotType::Ring)
        {
            Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
            Loadout.PrimaryRing = nullptr;
        }

        // Primary-slot-type based clears
        if (Loadout.PrimarySlotType != EPrimarySlotType::Weapon)
        {
            Loadout.PrimaryWeapon = nullptr;
            Loadout.PrimaryWeaponAbilities.Empty();
            Loadout.PrimaryWeaponStanceOverride = nullptr;
        }

        if (Loadout.PrimarySlotType != EPrimarySlotType::Ring)
        {
            Loadout.PrimaryRing = nullptr;
        }

        if (Loadout.PrimarySlotType != EPrimarySlotType::Evolution)
        {
            Loadout.PrimaryEvolution = nullptr;
        }

        // Secondary slot type based clears
        if (Loadout.SecondarySlotType != ESecondarySlotType::Weapon)
        {
            Loadout.SecondaryWeapon = nullptr;
            Loadout.SecondaryWeaponAbilities.Empty();
            Loadout.SecondaryWeaponStanceOverride = nullptr;
        }
    }
}
#endif

// ==================== DROPDOWN OPTIONS ====================

TArray<FString> UInventoryData::GetActiveLoadoutOptions() const
{
    TArray<FString> Options;
    Options.Reserve(SavedLoadouts.Num());
    for (const FSavedLoadout &Loadout : SavedLoadouts)
    {
        Options.Add(Loadout.LoadoutName);
    }
    return Options;
}

// ==================== DATA ASSET ====================

FPrimaryAssetId UInventoryData::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("InventoryData"), GetFName());
}
