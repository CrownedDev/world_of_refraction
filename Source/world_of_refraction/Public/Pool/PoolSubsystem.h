// PoolSubsystem.h
// The persistent owned-item POOL — the account/meta layer that survives level
// transitions (hub <-> combat) WITHIN a session. A GameInstanceSubsystem (the same
// pattern as UEconomyService / UDamageCalculator): auto-registered, GI-lifetime, no
// manual wiring.
//
// PHASE 1c — ALL OWNED TYPES + INERT. The subsystem now holds every owned type, grouped
// into the three browse categories the structured-pool design defines:
//   - ARMOURY:   OwnedWeapons, OwnedRings, OwnedEvolutions
//   - KNOWLEDGE: OwnedSpells, OwnedAbilities
//   - ITEMS:     ItemCrystals + RefinedCrystals (counted stacks)
//   - WALLET:    the currency value-block (separate from the 3 browse categories)
// Each gear/skill store reuses the SAME struct the run components use, so pool <-> run
// is a structural copy, not a translation. The Crystal/Augment 2-bucket BROWSE split is
// a later (1c-iii) view-layer concern — storage here mirrors UCrystalInventoryComponent's
// two pools verbatim.
//
// STILL INERT. The subsystem is registered, but NOTHING draws from or writes to it: the
// run components (UInventoryComponent / UCrystalInventoryComponent / UCurrencyComponent /
// UEvolutionInventoryComponent) are UNCHANGED and still own all live state. Pure additive
// foundation.
//
// Disk-save is a LATER phase. SaveGame tags here are DORMANT. In-session GI persistence
// works as-is because every UObject ref is a DATA ASSET (always-loaded); cross-session
// disk save will need soft-reference handling for those raw pointers (flagged for that
// phase). Currency is POD — it has no such caveat.
//
// See Resources_Design.md (Pool-arc plan) + Architecture/TierOnInstance.md.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "Loadout/Entries/FRingInventoryEntry.h"
#include "Loadout/Entries/FSpellCollection.h"   // FSpellInstance
#include "Loadout/Entries/FAbilityCollection.h" // FAbilityInstance
#include "Equipment/Crystals/FEvolutionInventoryEntry.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Currency/CurrencyTypes.h"             // ECurrencyType / EEssenceType / FCurrencyArray
#include "PoolSubsystem.generated.h"

/**
 * UPoolSubsystem
 * The persistent owned-item pool. Holds all owned types in one flat subsystem (flatter than
 * the run, which spreads them across UInventoryComponent / UCrystalInventoryComponent /
 * UEvolutionInventoryComponent). API is present but UNCALLED this phase — it defines the
 * shape the draw/return repoint (later phase) will wire into.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UPoolSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ==================== ARMOURY — weapons / rings / evolutions ====================

    /** Bank a weapon instance. INERT this phase — no caller yet. */
    UFUNCTION(BlueprintCallable, Category = "Pool|Armoury")
    void AddWeaponToPool(const FWeaponInventoryEntry &Entry);
    const TArray<FWeaponInventoryEntry> &GetOwnedWeapons() const { return OwnedWeapons; }
    FWeaponInventoryEntry *FindWeaponInPool(const FGuid &PersistentID);
    const FWeaponInventoryEntry *FindWeaponInPool(const FGuid &PersistentID) const;

    /** Bank a ring instance. */
    UFUNCTION(BlueprintCallable, Category = "Pool|Armoury")
    void AddRingToPool(const FRingInventoryEntry &Entry);
    const TArray<FRingInventoryEntry> &GetOwnedRings() const { return OwnedRings; }
    FRingInventoryEntry *FindRingInPool(const FGuid &PersistentID);
    const FRingInventoryEntry *FindRingInPool(const FGuid &PersistentID) const;

    /** Bank an evolution instance (keyed by InstanceID, not PersistentID). */
    UFUNCTION(BlueprintCallable, Category = "Pool|Armoury")
    void AddEvolutionToPool(const FEvolutionInventoryEntry &Entry);
    const TArray<FEvolutionInventoryEntry> &GetOwnedEvolutions() const { return OwnedEvolutions; }
    FEvolutionInventoryEntry *FindEvolutionInPool(const FGuid &InstanceID);
    const FEvolutionInventoryEntry *FindEvolutionInPool(const FGuid &InstanceID) const;

    // ==================== KNOWLEDGE — spells / abilities ====================

    /** Bank a learned-spell instance (keyed by InstanceID). */
    UFUNCTION(BlueprintCallable, Category = "Pool|Knowledge")
    void AddSpellToPool(const FSpellInstance &Instance);
    const TArray<FSpellInstance> &GetOwnedSpells() const { return OwnedSpells; }
    FSpellInstance *FindSpellInPool(const FGuid &InstanceID);
    const FSpellInstance *FindSpellInPool(const FGuid &InstanceID) const;

    /** Bank a learned-ability instance (keyed by InstanceID). */
    UFUNCTION(BlueprintCallable, Category = "Pool|Knowledge")
    void AddAbilityToPool(const FAbilityInstance &Instance);
    const TArray<FAbilityInstance> &GetOwnedAbilities() const { return OwnedAbilities; }
    FAbilityInstance *FindAbilityInPool(const FGuid &InstanceID);
    const FAbilityInstance *FindAbilityInPool(const FGuid &InstanceID) const;

    // ==================== ITEMS — crystals (counted stacks) ====================

    /** Add Count consumable (item) crystals at Id. INERT — no cap enforcement this phase
     *  (the run UCrystalInventoryComponent owns caps; the pool is raw storage). */
    UFUNCTION(BlueprintCallable, Category = "Pool|Items")
    void AddItemCrystalToPool(const FCrystalId &Id, int32 Count = 1);

    /** Add Count refined (slottable) crystals at Id. Independent pool, same as the run model. */
    UFUNCTION(BlueprintCallable, Category = "Pool|Items")
    void AddRefinedCrystalToPool(const FCrystalId &Id, int32 Count = 1);

    const TMap<FCrystalId, int32> &GetItemCrystals() const { return ItemCrystals; }
    const TMap<FCrystalId, int32> &GetRefinedCrystals() const { return RefinedCrystals; }

    // ==================== DEBUG ====================

    /** Dump the whole pool to log + screen: per-category counts, crystal stacks, and the
     *  wallet. Inspect without PIE-triggering a draw (per the debug-tools rule). */
    UFUNCTION(BlueprintCallable, Category = "Pool|Debug")
    void PrintPoolState() const;

    /** Formatted multi-section snapshot of the pool for inspection. */
    UFUNCTION(BlueprintCallable, Category = "Pool|Debug")
    FString GetPoolString() const;

private:
    // ==================== STORAGE ====================
    // SaveGame-tagged (DORMANT — honoured at the disk-save phase). UPROPERTY so the nested
    // data-asset UObject refs stay GC-reachable at subsystem scope. Each mirrors the run
    // component's storage type exactly.

    // ARMOURY
    UPROPERTY(SaveGame)
    TArray<FWeaponInventoryEntry> OwnedWeapons;

    UPROPERTY(SaveGame)
    TArray<FRingInventoryEntry> OwnedRings;

    UPROPERTY(SaveGame)
    TArray<FEvolutionInventoryEntry> OwnedEvolutions;

    // KNOWLEDGE
    UPROPERTY(SaveGame)
    TArray<FSpellInstance> OwnedSpells;

    UPROPERTY(SaveGame)
    TArray<FAbilityInstance> OwnedAbilities;

    // ITEMS — counted stacks keyed by (Type, Tier); fungible, no per-instance identity.
    UPROPERTY(SaveGame)
    TMap<FCrystalId, int32> ItemCrystals;

    UPROPERTY(SaveGame)
    TMap<FCrystalId, int32> RefinedCrystals;

    // ==================== WALLET (currency value-block) ====================
    // Mirrors UCurrencyComponent's VALUES (not the component — can't hold a UActorComponent
    // in a GI subsystem). All POD, so no asset-ref/soft-pointer caveat. Separate from the 3
    // browse categories.
    //
    // Gold is RUN-VOLATILE (never banked) — the run component tags it Replicated-only, NOT
    // SaveGame; mirrored faithfully here (NO SaveGame tag), so it will not persist into the
    // pool. (Flagged: if the pool should bank Gold, add the tag — but that contradicts the
    // "Gold is never banked" rule.) The other four scalars + the typed Essence persist.
    UPROPERTY()
    int32 PoolGold = 0;

    UPROPERTY(SaveGame)
    int32 PoolPrisms = 0;

    UPROPERTY(SaveGame)
    int32 PoolDiamond = 0;

    UPROPERTY(SaveGame)
    int32 PoolGearEssence = 0;

    UPROPERTY(SaveGame)
    int32 PoolSkillEssence = 0;

    // 14 typed-essence balances. FCurrencyArray reused for structural parity; at pool scope
    // its OwnerComponent back-ref stays null (its only job is client replication callbacks,
    // dormant here) and it functions as a pure serializable balance store.
    UPROPERTY(SaveGame)
    FCurrencyArray PoolEssence;
};
