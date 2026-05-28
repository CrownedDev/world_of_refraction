// FCombatCapabilities.h
// Snapshot of what a character can do on their turn.
// Built once from LoadoutComponent, read by everything else.
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "UI/Combat/PieMenuButtonData.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "FCombatCapabilities.generated.h"

class ULoadoutComponent;
class USpellData;
class UAbilityData;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FCombatCapabilities
{
    GENERATED_BODY()

    // ==================== AVAILABLE ACTIONS ====================

    /** Active weapon exists - show Attack */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Actions")
    bool bCanAttack = false;

    /** Active weapon has abilities - show Abilities */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Actions")
    bool bCanUseAbilities = false;

    /** Active weapon has crystal with spells - show Resonate Weapon */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Actions")
    bool bHasWeaponCrystal = false;

    /** Caster with innate spells - show Refractions */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Actions")
    bool bHasRefractions = false;

    /** Evolution primary with spells - show Breakthrough */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Actions")
    bool bHasBreakthrough = false;

    /** Ring in primary slot (Generic/Caster) - show Resonate Ring */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Actions")
    bool bHasPrimaryRing = false;

    /** Resonator ring loadout has at least one ring - show Resonate Ring */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Actions")
    bool bHasRingLoadout = false;

    /** Generic with secondary weapon - show Switch Weapon */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Actions")
    bool bCanSwitchWeapon = false;

    /** Resonator with multiple rings - show Switch Ring */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Actions")
    bool bCanSwitchRing = false;

    /** Loadout has at least one usable item - show Items */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Actions")
    bool bHasItems = false;

    // ==================== SPELL POOLS ====================
    // Each submenu reads directly from the relevant array.
    // Never re-query LoadoutComponent after BuildFrom() runs.

    /** Spells for Resonate Weapon submenu */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Spells")
    TArray<USpellData *> WeaponCrystalSpells;

    /** Spells for Resonate Ring submenu (primary ring slot or Resonator active ring) */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Spells")
    TArray<USpellData *> RingSpells;

    /** Spells for Refractions submenu (Caster innate) */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Spells")
    TArray<USpellData *> RefractionSpells;

    /** Spells for Breakthrough submenu (Evolution) */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Spells")
    TArray<USpellData *> BreakthroughSpells;

    // ==================== ABILITY POOL ====================

    /** Abilities for Abilities submenu (from active weapon) */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Abilities")
    TArray<UAbilityData *> WeaponAbilities;

    // ==================== ITEM POOL ====================

    /** Usable items in the active loadout's item slots (CrystalId only —
     *  resolve display name via CrystalIdentity::GetDisplayName). */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Items")
    TArray<FCrystalId> AvailableItems;

    // ==================== DISPLAY DATA ====================
    // Used by Create*Button functions for descriptions and tints.

    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Display")
    FString ActiveWeaponName;

    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Display")
    FString ActiveRingName;

    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Display")
    FLinearColor WeaponCrystalColor = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Display")
    FLinearColor RingColor = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Display")
    FLinearColor RefractionColor = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Display")
    FLinearColor BreakthroughColor = FLinearColor::White;

    /** Non-evolved ring has break chance - shown in button description */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities|Display")
    bool bRingHasBreakChance = false;

    // ==================== CLASS ====================

    /** Stored so submenus know which ring source to route to */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities")
    ECharacterClass CharacterClass = ECharacterClass::Generic;

    // ==================== FACTORY ====================

    /**
     * Build capabilities from a LoadoutComponent.
     * This is the ONLY place LoadoutComponent is queried for menu purposes.
     * GetElementColorFn maps ESpellElement (as int32) to a display color.
     */
    static FCombatCapabilities BuildFrom(
        ULoadoutComponent *LC,
        ECharacterClass CharClass,
        ESpellElement InnateElement,
        TFunction<FLinearColor(int32)> GetElementColorFn);

    // ==================== HELPERS ====================

    /** True if at least one action is available */
    bool HasAnyAction() const
    {
        return bCanAttack || bHasWeaponCrystal || bHasRefractions ||
               bHasBreakthrough || bHasPrimaryRing || bHasRingLoadout;
    }

    /** True if Resonate Ring button should show */
    bool ShouldShowResonateRing() const
    {
        return bHasPrimaryRing || bHasRingLoadout;
    }

    /** Get the correct spell pool for a given menu category */
    const TArray<USpellData *> &GetSpellsForCategory(EPieMenuCategory Category) const;

    /** Empty array returned when category has no spells */
    static const TArray<USpellData *> EmptySpells;
};