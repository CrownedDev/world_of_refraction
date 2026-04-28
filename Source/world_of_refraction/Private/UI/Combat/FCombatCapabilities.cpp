// FCombatCapabilities.cpp
// World of Refraction - Combat UI

#include "UI/Combat/FCombatCapabilities.h"
#include "LoadoutComponent.h"
#include "FCombatLoadout.h"
#include "FWeaponLoadoutEntry.h"
#include "FRingLoadoutEntry.h"
#include "SpellData.h"
#include "AbilityData.h"
#include "RingData.h"
#include "UI/Combat/PieMenuButtonData.h"

const TArray<USpellData *> FCombatCapabilities::EmptySpells = TArray<USpellData *>();

// ==================== FACTORY ====================

FCombatCapabilities FCombatCapabilities::BuildFrom(
    ULoadoutComponent *LC,
    ECharacterClass CharClass,
    TFunction<FLinearColor(int32)> GetElementColorFn)
{
    FCombatCapabilities Out;
    Out.CharacterClass = CharClass;

    if (!LC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[FCombatCapabilities] BuildFrom: null LoadoutComponent"));
        return Out;
    }

    const FCombatLoadout &Loadout = LC->GetActiveLoadout();

    UE_LOG(LogTemp, Warning, TEXT("[FCombatCapabilities] DEBUG PrimarySlotType=%d PrimaryRingPtr=%s"),
           static_cast<int32>(Loadout.PrimarySlotType),
           LC->GetPrimaryRingLoadout() ? TEXT("valid") : TEXT("null"));

    // ==================== ACTIVE WEAPON ====================

    const FWeaponLoadoutEntry *ActiveWeapon = LC->GetActiveWeaponLoadout();

    if (ActiveWeapon && ActiveWeapon->WeaponEntry.Weapon)
    {
        Out.bCanAttack = true;
        Out.ActiveWeaponName = ActiveWeapon->WeaponEntry.Weapon->WeaponName;
        Out.WeaponAbilities = ActiveWeapon->GetAllAbilities();
        Out.bCanUseAbilities = Out.WeaponAbilities.Num() > 0;

        // Crystal spells on active weapon
        if (ActiveWeapon->CanHaveSpells())
        {
            Out.WeaponCrystalSpells = ActiveWeapon->GetAllSpells();
            Out.bHasWeaponCrystal = Out.WeaponCrystalSpells.Num() > 0;
            Out.WeaponCrystalColor = GetElementColorFn(
                static_cast<int32>(ActiveWeapon->WeaponEntry.GetElement()));
        }
    }

    // ==================== REFRACTIONS (Caster innate) ====================

    if (CharClass == ECharacterClass::Caster)
    {
        Out.RefractionSpells = Loadout.InnateSpells;
        Out.bHasRefractions = Out.RefractionSpells.Num() > 0;
    }

    // ==================== BREAKTHROUGH (Evolution) ====================

    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        Out.BreakthroughSpells = Loadout.EvolutionSpells;
        Out.bHasBreakthrough = Out.BreakthroughSpells.Num() > 0;
        Out.BreakthroughColor = GetElementColorFn(0); // TODO: evolution element
    }

    // ==================== PRIMARY RING (Generic/Caster ring slot) ====================

    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
    {
        const FRingLoadoutEntry *PrimaryRing = LC->GetPrimaryRingLoadout();

        if (PrimaryRing && PrimaryRing->RingEntry.Ring != nullptr)
        {
            Out.bHasPrimaryRing = true;

            UE_LOG(LogTemp, Log, TEXT("[FCombatCapabilities] PrimarySlot=%d PrimaryRing=%s RingValid=%s RingSpells=%d"),
                   static_cast<int32>(Loadout.PrimarySlotType),
                   LC->GetPrimaryRingLoadout() ? TEXT("exists") : TEXT("null"),
                   LC->GetPrimaryRingLoadout() && LC->GetPrimaryRingLoadout()->RingEntry.Ring != nullptr ? TEXT("true") : TEXT("false"),
                   Out.RingSpells.Num());
            Out.RingSpells = PrimaryRing->GetAllSpells();
            if (Out.RingSpells.IsEmpty())
                Out.RingSpells = PrimaryRing->AssignedSpells;
            Out.bRingHasBreakChance = !PrimaryRing->RingEntry.IsEvolved();
            Out.RingColor = GetElementColorFn(
                static_cast<int32>(PrimaryRing->RingEntry.GetElement()));

            if (PrimaryRing->RingEntry.Ring)
            {
                Out.ActiveRingName = PrimaryRing->RingEntry.Ring->RingName;
            }
        }
    }

    // ==================== RING LOADOUT (Resonator) ====================

    if (CharClass == ECharacterClass::Resonator)
    {
        int32 RingCount = 0;
        for (const FRingLoadoutEntry &Entry : Loadout.RingLoadout)
        {
            if (Entry.IsValid())
                RingCount++;
        }

        Out.bHasRingLoadout = RingCount > 0;
        Out.bCanSwitchRing = RingCount > 1;

        const FRingLoadoutEntry *ActiveRing = LC->GetActiveRingLoadout();
        if (ActiveRing && ActiveRing->IsValid())
        {
            Out.RingSpells = ActiveRing->GetAllSpells();
            Out.bRingHasBreakChance = !ActiveRing->RingEntry.IsEvolved();
            Out.RingColor = GetElementColorFn(
                static_cast<int32>(ActiveRing->RingEntry.GetElement()));

            if (ActiveRing->RingEntry.Ring)
            {
                Out.ActiveRingName = ActiveRing->RingEntry.Ring->RingName;
            }
        }
    }

    // ==================== SWITCH WEAPON ====================

    Out.bCanSwitchWeapon = LC->HasSecondaryEquipment();

    UE_LOG(LogTemp, Log, TEXT("[FCombatCapabilities] Built for %s: Attack=%d Abilities=%d WeaponCrystal=%d Refractions=%d Breakthrough=%d PrimaryRing=%d RingLoadout=%d SwitchWeapon=%d SwitchRing=%d"),
           *UEnum::GetValueAsString(CharClass),
           Out.bCanAttack, Out.bCanUseAbilities, Out.bHasWeaponCrystal,
           Out.bHasRefractions, Out.bHasBreakthrough,
           Out.bHasPrimaryRing, Out.bHasRingLoadout,
           Out.bCanSwitchWeapon, Out.bCanSwitchRing);

    return Out;
}

// ==================== HELPERS ====================

const TArray<USpellData *> &FCombatCapabilities::GetSpellsForCategory(EPieMenuCategory Category) const
{
    switch (Category)
    {
    case EPieMenuCategory::ResonateWeapon:
        return WeaponCrystalSpells;
    case EPieMenuCategory::ResonateRing:
        return RingSpells;
    case EPieMenuCategory::Refractions:
        return RefractionSpells;
    case EPieMenuCategory::Breakthrough:
        return BreakthroughSpells;
    default:
        UE_LOG(LogTemp, Warning, TEXT("[FCombatCapabilities] GetSpellsForCategory: no spell pool for category %d"),
               static_cast<int32>(Category));
        return EmptySpells;
    }
}