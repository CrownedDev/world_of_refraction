// FCombatCapabilities.cpp
// World of Refraction - Combat UI

#include "UI/Combat/FCombatCapabilities.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Loadout/Entries/FItemLoadoutSlot.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "UI/Combat/PieMenuButtonData.h"

const TArray<USpellData *> FCombatCapabilities::EmptySpells = TArray<USpellData *>();

// ==================== FACTORY ====================

FCombatCapabilities FCombatCapabilities::BuildFrom(
    ULoadoutComponent *LC,
    ECharacterClass CharClass,
    ESpellElement InnateElement,
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
    // GetActiveWeaponLoadout returns the combat-active weapon for all classes:
    // - Generic dual-weapon: respects bShowPrimary toggle
    // - Generic with ring-primary + weapon-secondary: returns secondary (always available)
    // - Caster/Resonator: returns primary weapon iff slotted (bShowPrimary is stance-only)

    const FWeaponLoadoutEntry *ActiveWeapon = LC->GetActiveWeaponLoadout();

    if (ActiveWeapon && ActiveWeapon->WeaponEntry.Weapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("[FCombatCapabilities] ActiveWeapon='%s' HasCrystal=%d"),
               *ActiveWeapon->WeaponEntry.Weapon->Name,
               ActiveWeapon->WeaponEntry.HasCrystal());
        Out.bCanAttack = true;
        Out.ActiveWeaponName = ActiveWeapon->WeaponEntry.Weapon->Name;
        Out.WeaponAbilities = ActiveWeapon->GetAllAbilities();
        Out.bCanUseAbilities = Out.WeaponAbilities.Num() > 0;

        // Crystal spells on active weapon (gated on CanProvideSpells —
        // broken attachments stay slotted for visual feedback but provide no spells)
        if (ActiveWeapon->WeaponEntry.AttachedItem.CanProvideSpells())
        {
            Out.WeaponCrystalSpells = ActiveWeapon->WeaponEntry.AssignedSpells;
            Out.bHasWeaponCrystal = true;
            Out.WeaponCrystalColor = GetElementColorFn(
                static_cast<int32>(ActiveWeapon->WeaponEntry.GetElement()));
        }
    }

    // ==================== REFRACTIONS (Caster innate) ====================

    if (CharClass == ECharacterClass::Caster)
    {
        // Darkness pool (InnateSpells) is always available. For Broken Darkness,
        // also surface each element pool whose element has been absorbed.
        Out.RefractionSpells = Loadout.InnateSpells;

        if (AActor *OwnerActor = LC->GetOwner())
        {
            UCharacterDataComponent *CharComp = OwnerActor->FindComponentByClass<UCharacterDataComponent>();
            UBrokenDarknessManager *BDManager = OwnerActor->FindComponentByClass<UBrokenDarknessManager>();
            if (CharComp && CharComp->IsBrokenDarkness() && BDManager)
            {
                for (const FBDElementSpellPool &Pool : Loadout.BDSpellPools)
                {
                    if (BDManager->HasAbsorbedElement(Pool.Element))
                    {
                        Out.RefractionSpells.Append(Pool.Spells);
                    }
                }
            }
        }

        Out.bHasRefractions = Out.RefractionSpells.Num() > 0;
        Out.RefractionColor = GetElementColorFn(static_cast<int32>(InnateElement));
    }

    // ==================== BREAKTHROUGH (Evolution) ====================

    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        Out.BreakthroughSpells = Loadout.EvolutionSpells;
        Out.bHasBreakthrough = Out.BreakthroughSpells.Num() > 0;

        // Get element from evolution crystal
        if (Loadout.PrimaryEvolution.Item)
        {
            Out.BreakthroughColor = GetElementColorFn(
                static_cast<int32>(Loadout.PrimaryEvolution.Item->GetAssociatedElement()));
        }
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
            // Spells gated on CanProvideSpells — broken attachments don't provide spells
            if (PrimaryRing->RingEntry.AttachedItem.CanProvideSpells())
            {
                Out.RingSpells = PrimaryRing->RingEntry.AssignedSpells;
            }
            Out.bRingHasBreakChance = !PrimaryRing->RingEntry.IsEvolved();
            Out.RingColor = GetElementColorFn(
                static_cast<int32>(PrimaryRing->RingEntry.GetElement()));

            if (PrimaryRing->RingEntry.Ring)
            {
                Out.ActiveRingName = PrimaryRing->RingEntry.Ring->Name;
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
            // Spells gated on CanProvideSpells — broken attachments don't provide spells
            if (ActiveRing->RingEntry.AttachedItem.CanProvideSpells())
            {
                Out.RingSpells = ActiveRing->RingEntry.AssignedSpells;
            }
            Out.bRingHasBreakChance = !ActiveRing->RingEntry.IsEvolved();
            Out.RingColor = GetElementColorFn(
                static_cast<int32>(ActiveRing->RingEntry.GetElement()));

            if (ActiveRing->RingEntry.Ring)
            {
                Out.ActiveRingName = ActiveRing->RingEntry.Ring->Name;
            }
        }
    }

    // ==================== ITEMS ====================

    for (const FItemLoadoutSlot &Slot : Loadout.ItemSlots)
    {
        if (!Slot.IsEmpty())
        {
            Out.AvailableItems.Add(Slot.CrystalId);
        }
    }
    Out.bHasItems = Out.AvailableItems.Num() > 0;

    // ==================== SWITCH WEAPON ====================
    // Switch Weapon requires TWO weapons in the loadout (Generic dual-wield).
    // Evolution + Secondary Weapon has only one weapon, so no switch.
    // Reading A confirmed in 30/04/2026 session: Generic Secondary is Weapon only,
    // so we don't need to handle Secondary Ring here.

    Out.bCanSwitchWeapon =
        Loadout.PrimarySlotType == EPrimarySlotType::Weapon &&
        Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
        Loadout.PrimaryWeapon.IsValid() &&
        Loadout.SecondaryWeapon.IsValid();

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