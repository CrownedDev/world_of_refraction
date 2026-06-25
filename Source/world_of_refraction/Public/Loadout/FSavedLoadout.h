// FSavedLoadout.h
// Designer-authored loadout configuration stored inline on a UInventoryData
// asset. Mirrors the fields on ULoadoutData but lives inside an asset's
// SavedLoadouts[] array — no separate UPrimaryDataAsset per loadout.
//
// FCombatLoadout::CreateFromSavedLoadout inflates this into the runtime
// FCombatLoadout (per-instance crystal state, item-use counters, etc).

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "Equipment/Weapons/EWeaponSlotType.h"
#include "Loadout/FCombatLoadout.h"
#include "Equipment/FEquippedItemSlot.h"
#include "FSavedLoadout.generated.h"

class UWeaponData;
class URingData;
class UEvolutionItemData;
class USpellData;
class UAbilityData;

/**
 * FSpellRef
 * Saved-side spell + per-instance identity pairing (cluster ii-a). The asset says WHICH spell;
 * InstanceID (when valid) says WHICH owned FSpellInstance — matched at inflation (ii-b). Invalid
 * (default) = asset-fallback, the shape-B convention (mirrors the weapon/ring/evolution instance
 * refs + the owned-side FSpellInstance pairing). Combat stays bare USpellData*: inflation copies
 * .Spell OUT into the runtime arrays, so the InstanceID is carried but UNRESOLVED until ii-b.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSpellRef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Spell")
    USpellData *Spell = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Spell")
    FGuid InstanceID;

    FSpellRef() = default;
    explicit FSpellRef(USpellData *InSpell) : Spell(InSpell) {} // InstanceID invalid by default (asset-fallback)

    /** Extract the bare assets (drops InstanceID) for the inert runtime/validation paths that still
     *  operate on TArray<USpellData*>. Used at inflation (copies .Spell into the runtime arrays) and
     *  by saved-side validation/accessors until ii-b resolves the InstanceID. */
    static TArray<USpellData *> ExtractSpells(const TArray<FSpellRef> &Refs)
    {
        TArray<USpellData *> Out;
        Out.Reserve(Refs.Num());
        for (const FSpellRef &Ref : Refs)
        {
            Out.Add(Ref.Spell);
        }
        return Out;
    }
};

/**
 * FSavedBDElementSpellPool
 * Saved-side Broken Darkness element pool (cluster ii-a2). Mirrors the runtime FBDElementSpellPool
 * but pairs each spell with its owned-instance identity (TArray<FSpellRef>). Split from the runtime
 * struct so the saved side can carry InstanceIDs WITHOUT touching the ~6 combat BD readers — the
 * runtime FCombatLoadout::BDSpellPools stays bare FBDElementSpellPool. Inflation converts via
 * ToRuntimePool() (copies .Spell out; InstanceID rides the saved side, unresolved until ii-b).
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSavedBDElementSpellPool
{
    GENERATED_BODY()

    /** Element this pool channels (matches the runtime pool's Element). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells")
    ESpellElement Element = ESpellElement::Generic;

    /** Spells in this pool (max 6, each matching Element) + their owned-instance identities. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spells")
    TArray<FSpellRef> Spells;

    /** Convert to the bare runtime pool (extracts .Spell — InstanceID is carried on the saved side,
     *  unresolved until ii-b). Used at inflation and by the saved-side validation path. */
    FBDElementSpellPool ToRuntimePool() const
    {
        FBDElementSpellPool Pool;
        Pool.Element = Element;
        Pool.Spells = FSpellRef::ExtractSpells(Spells);
        return Pool;
    }
};

/**
 * FResonatorRingSlot
 * One Resonator ring slot — pairs a ring asset with the spell-override list
 * that applies to that ring in this loadout. Empty AssignedSpells means the
 * ring exposes its DefaultSpells unchanged.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FResonatorRingSlot
{
    GENERATED_BODY()

    /** The ring asset assigned to this slot */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ring")
    URingData* Ring = nullptr;

    /** Optional owned-instance reference (shape B): pairs with Ring — the asset
     *  says WHICH ring type, this (when valid) says WHICH owned copy, matched
     *  against FRingInventoryEntry::PersistentID at inflation (U1c). Invalid
     *  (default) = no instance reference, inflate from the asset as today. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Ring")
    FGuid RingInstance;

    /** Spells assigned to this ring's slots in this loadout (overrides the ring's defaults).
     *  Empty array means use the ring's DefaultSpells. FSpellRef pairs each with its owned-instance
     *  identity (ii-a); inflation copies .Spell out into the bare runtime array. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ring")
    TArray<FSpellRef> AssignedSpells;
};

/**
 * FSavedLoadout
 *
 * Inline asset-side loadout configuration. Held by UInventoryData in a
 * SavedLoadouts[] array; runtime code inflates it via
 * FCombatLoadout::CreateFromSavedLoadout at character spawn.
 *
 * Field set mirrors ULoadoutData (minus Description). Class-specific rules
 * match: Generic = primary + optional secondary weapon; Caster = primary +
 * innate spells; Resonator = primary (Weapon/Evolution) + equipped rings.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSavedLoadout
{
    GENERATED_BODY()

    // ==================== IDENTITY ====================

    /** Display name for UI. Empty on construction so UInventoryData's
     *  PostEditChangeProperty auto-fills "Loadout N" for new entries;
     *  designers can override and renames are preserved. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
    FString LoadoutName;

    /** Which character class this loadout is designed for */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity")
    ECharacterClass RequiredClass = ECharacterClass::Generic;

    /** If true, character starts combat showing primary equipment stance. If false, starts showing secondary (Generic) or unarmed (Caster/Resonator). Visual/stance flag — does not gate combat capability. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "1. Identity", meta = (DisplayName = "Starts Showing Primary"))
    bool bShowPrimary = true;

    // ==================== CASTER INNATE SPELLS ====================

    /** Innate spells (Caster only - up to 24, must match InnateElement).
     *  For a Broken Darkness template this is the Darkness pool (max 6). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Class|Caster Spells",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Caster", EditConditionHides))
    TArray<FSpellRef> InnateSpells;

    /** Broken Darkness per-element spell pools (Fire/Water/Earth/Wind/Light/
     *  Lightning/Void). Authored for BD enemy templates; leave empty for a
     *  normal Caster. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Class|Caster Spells",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Caster", EditConditionHides))
    TArray<FSavedBDElementSpellPool> BDSpellPools;

    // ==================== RESONATOR RINGS ====================

    /** Equipped rings (Resonator only - 5 normal, 3 if evolved).
     *  Each slot pairs a ring with its per-loadout spell overrides. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "2. Class|Resonator Rings",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Resonator", EditConditionHides))
    TArray<FResonatorRingSlot> EquippedRings;

    // ==================== PRIMARY EQUIPMENT ====================

    /** Primary slot type (Generic/Caster: Weapon/Ring/Evolution, Resonator: Weapon/Evolution only) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary",
              meta = (EditCondition = "RequiredClass != ECharacterClass::Resonator", EditConditionHides))
    EPrimarySlotType PrimarySlotType = EPrimarySlotType::None;

    /** Primary weapon (when PrimarySlotType == Weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary",
              meta = (EditCondition = "PrimarySlotType == EPrimarySlotType::Weapon", EditConditionHides))
    UWeaponData *PrimaryWeapon = nullptr;

    /** Optional owned-instance reference (shape B): pairs with PrimaryWeapon —
     *  the asset says WHICH weapon type, this (when valid) says WHICH owned copy,
     *  matched against FWeaponInventoryEntry::PersistentID at inflation (U1c).
     *  Invalid (default) = inflate from the asset as today. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "3. Primary",
              meta = (EditCondition = "PrimarySlotType == EPrimarySlotType::Weapon", EditConditionHides))
    FGuid PrimaryWeaponInstance;

    /** Primary ring (Generic/Caster only, when PrimarySlotType == Ring) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary",
              meta = (EditCondition = "RequiredClass != ECharacterClass::Resonator && PrimarySlotType == EPrimarySlotType::Ring", EditConditionHides))
    URingData *PrimaryRing = nullptr;

    /** Optional owned-instance reference pairing with PrimaryRing (see
     *  PrimaryWeaponInstance). Invalid = asset inflation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "3. Primary",
              meta = (EditCondition = "RequiredClass != ECharacterClass::Resonator && PrimarySlotType == EPrimarySlotType::Ring", EditConditionHides))
    FGuid PrimaryRingInstance;

    /** Primary evolution crystal (when PrimarySlotType == Evolution) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary",
              meta = (EditCondition = "PrimarySlotType == EPrimarySlotType::Evolution", EditConditionHides))
    UEvolutionItemData *PrimaryEvolution = nullptr;

    /** Optional owned-instance reference pairing with PrimaryEvolution — matched
     *  against FEvolutionInventoryEntry::InstanceID (evolution's existing FGuid)
     *  at inflation (U1c). Invalid = asset inflation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "3. Primary",
              meta = (EditCondition = "PrimarySlotType == EPrimarySlotType::Evolution", EditConditionHides))
    FGuid PrimaryEvolutionInstance;

    // ==================== PRIMARY EQUIPMENT CONFIGURATION ====================

    /** Abilities assigned to primary weapon (max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary|Config",
              meta = (EditCondition = "PrimarySlotType == EPrimarySlotType::Weapon", EditConditionHides))
    TArray<UAbilityData *> PrimaryWeaponAbilities;

    /** Per-loadout override for the augment stone's extra abilities (base = weapon DefaultAbilities).
     *  Only meaningful when the primary weapon has a augment stone attached. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary|Config",
              meta = (EditCondition = "PrimarySlotType == EPrimarySlotType::Weapon", EditConditionHides))
    TArray<UAbilityData *> PrimaryAugmentStoneAbilities;

    /** Evolution spells (player-found world drops with RequiredEvolutionCrystal validation). FSpellRef
     *  pairs each with its owned-instance identity (ii-a); inflation copies .Spell out to the runtime. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3. Primary|Config",
              meta = (EditCondition = "PrimarySlotType == EPrimarySlotType::Evolution", EditConditionHides))
    TArray<FSpellRef> EvolutionSpells;

    // ==================== SECONDARY EQUIPMENT (Generic only) ====================

    /** Secondary slot type (Generic only - None or Weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Secondary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic", EditConditionHides))
    ESecondarySlotType SecondarySlotType = ESecondarySlotType::None;

    /** Secondary weapon (Generic only, when SecondarySlotType == Weapon) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Secondary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    UWeaponData *SecondaryWeapon = nullptr;

    /** Optional owned-instance reference pairing with SecondaryWeapon (see
     *  PrimaryWeaponInstance). Invalid = asset inflation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "4. Secondary",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    FGuid SecondaryWeaponInstance;

    // ==================== SECONDARY WEAPON CONFIGURATION (Generic only) ====================

    /** Abilities assigned to secondary weapon (Generic only, max 6) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Secondary|Config",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    TArray<UAbilityData *> SecondaryWeaponAbilities;

    /** Per-loadout override for the secondary weapon's augment-stone extra abilities
     *  (base = weapon DefaultAbilities). Generic only; meaningful only when the
     *  secondary weapon has a augment stone attached. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "4. Secondary|Config",
              meta = (EditCondition = "RequiredClass == ECharacterClass::Generic && SecondarySlotType == ESecondarySlotType::Weapon", EditConditionHides))
    TArray<UAbilityData *> SecondaryAugmentStoneAbilities;

    // ==================== ITEMS ====================

    /** Equipped item slots (max 6, max 3 quantity per slot, unique by ECrystalType). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "5. Items")
    TArray<FEquippedItemSlot> EquippedItems;

    /** When true, runtime auto-equips items from inventory at combat start
     *  (and refills empty/partial slots between combats), ignoring each
     *  slot's designer-authored Quantity. When false, the slot's authored
     *  Quantity is taken as the starting amount and the crystal inventory is
     *  debited at character init; no between-combat refill.
     *  Auto-equip runtime logic lands with the equip API in a later commit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "5. Items",
              meta = (DisplayName = "Auto-Equip Items at Combat Start"))
    bool bAutoEquipItemsOnCombatStart = false;

    // ==================== VALIDATION ====================

    /** Check if loadout is valid for given class */
    bool IsValidForClass(ECharacterClass CharacterClass) const;

    /** Struct-internal validation errors (does not cross-check inventory
     *  ownership — that happens at UInventoryData level). */
    TArray<FString> GetValidationErrors() const;

    /** Quick check if loadout has any internal errors */
    bool HasValidationErrors() const;

    // ==================== ACCESSORS ====================

    /** Get all spells available from this loadout */
    TArray<USpellData *> GetAllSpells() const;

    /** Get all abilities available from this loadout */
    TArray<UAbilityData *> GetAllAbilities() const;

    /** Get primary weapon (nullptr if using ring or evolution) */
    UWeaponData *GetPrimaryWeapon() const;

    /** Get primary ring (nullptr if using weapon or evolution) */
    URingData *GetPrimaryRing() const;

    /** Get primary evolution (nullptr if using weapon or ring) */
    UEvolutionItemData *GetPrimaryEvolution() const;

    /** Check if this loadout uses evolution */
    bool IsEvolutionLoadout() const { return PrimarySlotType == EPrimarySlotType::Evolution; }
};
