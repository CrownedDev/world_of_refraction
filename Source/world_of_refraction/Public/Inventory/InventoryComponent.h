// InventoryComponent.h
// Character inventory storage and management
//
// ARCHITECTURE:
// This component stores all character OWNERSHIP - what spells, abilities,
// weapons, rings, and items a character possesses.
//
// Loadout configuration (how items are equipped for combat) is separate.
// This is the "warehouse", LoadoutComponent is the "battle gear".

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Loadout/Entries/FSpellCollection.h"
#include "Loadout/Entries/FAbilityCollection.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Loadout/FCombatLoadout.h"
#include "InventoryComponent.generated.h"

class USpellData;
class UAbilityData;
class USkillDataBase;
class UWeaponData;
class URingData;
class UEvolutionItemData;
class UInventoryData;
class UCharacterData;
class UCrystalInventoryComponent;
class UEvolutionInventoryComponent;

/**
 * UInventoryComponent
 * Manages all character inventory (what they OWN)
 *
 * Attach to any actor that needs inventory (player characters, party members)
 * Provides validation, capacity management, and Blueprint access
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryComponent();

    // ==================== INVENTORY DATA ====================

    /** Initialize inventory from CharacterData template */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Setup")
    void InitializeFromCharacterData(UCharacterData *CharacterData);

    /** Learned spells (max 50) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Spells")
    FSpellCollection Spells;

    /** Learned abilities (max 50) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Abilities")
    FAbilityCollection Abilities;

    /** Owned weapons with crystal/evolution state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Weapons")
    TArray<FWeaponInventoryEntry> Weapons;

    /** Owned rings with crystal/evolution state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Rings")
    TArray<FRingInventoryEntry> Rings;

    // ============================================================
    // SAVED LOADOUTS
    // ULoadoutComponent's public facade methods read these via
    // GetInventoryComponent() — see LoadoutComponent for the API.
    // ============================================================

    /** Configured maximum number of saved loadouts. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout|Config")
    int32 MaxSavedLoadouts = 5;

    /** Array of saved loadout configurations for this character. Per-instance
     *  runtime state — populated from UInventoryData at character spawn. */
    UPROPERTY()
    TArray<FCombatLoadout> SavedLoadouts;

    /** Index of the currently active loadout within SavedLoadouts. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Saved")
    int32 ActiveLoadoutIndex = 0;

    // ==================== SPELL OPERATIONS ====================

    /** Learn a new spell */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Spells")
    bool LearnSpell(USpellData *Spell);

    /** Unlearn a spell */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Spells")
    bool UnlearnSpell(USpellData *Spell);

    /** Check if spell is known */
    UFUNCTION(BlueprintPure, Category = "Inventory|Spells")
    bool HasSpell(USpellData *Spell) const;

    /** Get spells by element */
    UFUNCTION(BlueprintPure, Category = "Inventory|Spells")
    TArray<USpellData *> GetSpellsByElement(ESpellElement Element) const;

    // ==================== ABILITY OPERATIONS ====================

    /** Learn a new ability */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Abilities")
    bool LearnAbility(UAbilityData *Ability);

    /** Unlearn an ability */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Abilities")
    bool UnlearnAbility(UAbilityData *Ability);

    /** Check if ability is known */
    UFUNCTION(BlueprintPure, Category = "Inventory|Abilities")
    bool HasAbility(UAbilityData *Ability) const;

    /** Get abilities for weapon type */
    UFUNCTION(BlueprintPure, Category = "Inventory|Abilities")
    TArray<UAbilityData *> GetAbilitiesForWeaponType(EWeaponType WeaponType) const;

    // ==================== INSTANCE-TIER RESOLUTION (spell-instance arc, ii-b/c) ====================

    /** Resolve the effective tier of Spell as cast BY Caster: the caster's owned FSpellInstance tier
     *  (leveled) when owned, else the asset tier (enemies / authored loadouts don't level). Asset-keyed
     *  (owners hold <=1 instance per asset — duplicates are rejected at learn time). Static so both the
     *  combat executor and the AI preview resolve through one path. Caster/Spell null → asset/F fallback. */
    static EItemTier ResolveSpellTier(const AActor *Caster, const USpellData *Spell);

    /** Ability/skill twin of ResolveSpellTier (resolves the caster's owned FAbilityInstance tier; basic
     *  attacks aren't learned, so they asset-fall-back). Takes the merged USkillDataBase pointer. */
    static EItemTier ResolveAbilityTier(const AActor *Caster, const USkillDataBase *Skill);

    // ==================== WEAPON OPERATIONS ====================

    /** Add a weapon to inventory */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    bool AddWeapon(UWeaponData *Weapon, bool bCopyDefaultCrystal = false);

    /** Remove a weapon from inventory */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    bool RemoveWeapon(int32 WeaponIndex);

    /** Remove the owned weapon instance whose PersistentID matches — resolves the GUID to its
     *  array index, then removes by index. Returns false if no entry carries that GUID. The
     *  by-instance counterpart to index-based RemoveWeapon (dismantle addresses a specific
     *  instance, not a slot). */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    bool RemoveWeaponByPersistentID(FGuid PersistentID);

    /** Get weapon count */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weapons")
    int32 GetWeaponCount() const { return Weapons.Num(); }

    /** Get weapon at index */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weapons")
    FWeaponInventoryEntry GetWeaponAt(int32 Index) const;

    /** Get total weapon slot cost */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weapons")
    int32 GetWeaponSlotCostTotal() const;

    /** Get remaining weapon capacity */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weapons")
    int32 GetRemainingWeaponCapacity() const;

    /** Can add weapon (checks capacity) */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weapons")
    bool CanAddWeapon(UWeaponData *Weapon) const;

    // ==================== WEAPON CRYSTAL OPERATIONS ====================

    /** Remove crystal from weapon. Returns true if an attachment was removed,
     *  false if the slot was already empty or the index was invalid. Refined
     *  and evolution attachments are both destroyed on removal. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    bool RemoveCrystalFromWeapon(int32 WeaponIndex);

    /** Attach an OWNED evolution (by EvoInstanceID) onto the owned weapon (by PersistentID),
     *  Spiritualist-gated. Reference model (§5.3b): the owned FEvolutionInventoryEntry PERSISTS; the
     *  weapon's AttachedItem.Evolution REFERENCES it (InstanceID link + copied leveled Tier + rolled
     *  state). Enforces one-evo-one-slot — rejects if the evo is already in any primary slot or gear
     *  attachment. False on: no authority; evo/weapon GUID not found; weapon slot already occupied;
     *  or evo already slotted. (Member of the owner's inventory — Owner is implicit via GetOwner.) */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    bool AttachEvolutionToWeapon(FGuid WeaponPersistentID, FGuid EvoInstanceID);

    /** Remove a PLAYER-ATTACHED evolution from the owned weapon (by PersistentID), Spiritualist-gated.
     *  Copies the worn runtime durability back onto the owned entry, applies 10% removal wear, then
     *  un-references (clears the gear slot — the entry PERSISTS, reference model). If the 10% wear
     *  drops durability to 0 the evo BREAKS → forced dismantle (DismantleEvolution: essence + remove
     *  entry + free cap); otherwise it returns to inventory at its worn durability. Rejects an
     *  AUTHORED-LOCKED attachment (invalid InstanceID — built-in, can't be removed). False on: no
     *  authority; weapon not found; slot not a player-attached evolution; or owned entry missing. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    bool RemoveEvolutionFromWeapon(FGuid WeaponPersistentID);

    // ==================== RING OPERATIONS ====================

    /** Add a ring to inventory */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    bool AddRing(URingData *Ring, bool bCopyDefaultCrystal = false);

    /** Remove a ring from inventory */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    bool RemoveRing(int32 RingIndex);

    /** Remove the owned ring instance whose PersistentID matches — resolves the GUID to its
     *  array index, then removes by index. Returns false if no entry carries that GUID. The
     *  by-instance counterpart to index-based RemoveRing. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    bool RemoveRingByPersistentID(FGuid PersistentID);

    /** Attach an OWNED evolution (by EvoInstanceID) onto the owned ring (by PersistentID). Ring
     *  counterpart of AttachEvolutionToWeapon — same reference model + one-evo-one-slot enforcement. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    bool AttachEvolutionToRing(FGuid RingPersistentID, FGuid EvoInstanceID);

    /** Remove a PLAYER-ATTACHED evolution from the owned ring (by PersistentID). Ring counterpart of
     *  RemoveEvolutionFromWeapon — same copy-back + 10% wear + break-on-zero + un-reference flow. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    bool RemoveEvolutionFromRing(FGuid RingPersistentID);

    /** Get ring count */
    UFUNCTION(BlueprintPure, Category = "Inventory|Rings")
    int32 GetRingCount() const { return Rings.Num(); }

    /** Get ring at index */
    UFUNCTION(BlueprintPure, Category = "Inventory|Rings")
    FRingInventoryEntry GetRingAt(int32 Index) const;

    /** Get total ring slot cost */
    UFUNCTION(BlueprintPure, Category = "Inventory|Rings")
    int32 GetRingSlotCostTotal() const;

    /** Get remaining ring capacity */
    UFUNCTION(BlueprintPure, Category = "Inventory|Rings")
    int32 GetRemainingRingCapacity() const;

    /** Can add ring (checks capacity) */
    UFUNCTION(BlueprintPure, Category = "Inventory|Rings")
    bool CanAddRing(URingData *Ring) const;

    // ==================== RING CRYSTAL OPERATIONS ====================

    /** Remove crystal from ring. Returns true if an attachment was removed,
     *  false if the slot was already empty or the index was invalid. Refined
     *  and evolution attachments are both destroyed on removal. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    bool RemoveCrystalFromRing(int32 RingIndex);

    // ==================== EVOLUTION HELPERS ====================

    /** Get all evolution crystals owned by this character.
     *  Reads UEvolutionInventoryComponent on the owner. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Evolution")
    TArray<UEvolutionItemData *> GetEvolutionCrystals() const;

    // ==================== UTILITY ====================

    /** Get inventory summary for debug */
    UFUNCTION(BlueprintPure, Category = "Inventory|Debug")
    FString GetInventorySummary() const;

private:
    /** Populates ownership lists + SavedLoadouts + ActiveLoadoutIndex from
     *  CharacterData->Inventory (a UInventoryData asset). Sole loadout-init
     *  path — InitializeFromCharacterData delegates here when Inventory is set. */
    void InitializeFromInventoryAsset(UCharacterData *CharacterData);

protected:
    virtual void BeginPlay() override;

#if WITH_EDITOR
    /** Context menu debug */
    UFUNCTION(CallInEditor, Category = "Debug")
    void DebugLogInventory();
#endif
};
