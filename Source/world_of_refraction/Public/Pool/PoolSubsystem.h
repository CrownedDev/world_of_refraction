// PoolSubsystem.h
// The persistent owned-item POOL — the account/meta layer that survives level
// transitions (hub <-> combat) WITHIN a session. A GameInstanceSubsystem (the same
// pattern as UEconomyService / UDamageCalculator): auto-registered, GI-lifetime, no
// manual wiring.
//
// PHASE 1 — WEAPONS ONLY + INERT. The subsystem is registered, but NOTHING draws from
// or writes to it yet: the run UInventoryComponent still seeds from authored
// CharacterData (the draw repoint is a LATER phase). This is pure additive foundation.
//
// Disk-save is a LATER phase. The SaveGame tag on the pool is DORMANT (honoured when
// the UPoolSaveGame phase lands). In-session GI persistence works as-is because the
// entry's UObject refs are DATA ASSETS (always-loaded); cross-session disk save will
// need soft-reference handling for those raw pointers (flagged for that phase).
//
// See Resources_Design.md (Pool-arc plan) + Architecture/TierOnInstance.md.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Loadout/Entries/FWeaponInventoryEntry.h"
#include "PoolSubsystem.generated.h"

/**
 * UPoolSubsystem
 * The persistent owned-item pool. Phase 1 holds the owned WEAPON pool only, using the
 * SAME entry struct the run inventory uses (FWeaponInventoryEntry) so pool <-> run is a
 * structural copy, not a translation. API is present but UNCALLED this phase — it
 * defines the shape the draw/return repoint (later phase) will wire into.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UPoolSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ==================== WEAPON POOL (Phase 1) ====================

    /** Bank a weapon instance into the persistent pool. INERT this phase — no caller yet. */
    UFUNCTION(BlueprintCallable, Category = "Pool|Weapons")
    void AddWeaponToPool(const FWeaponInventoryEntry &Entry);

    /** Read the owned weapon pool (the persistent layer). */
    const TArray<FWeaponInventoryEntry> &GetOwnedWeapons() const { return OwnedWeapons; }

    /** Locate an owned weapon by its PersistentID — nullptr if absent. Mutable + const. */
    FWeaponInventoryEntry *FindWeaponInPool(const FGuid &PersistentID);
    const FWeaponInventoryEntry *FindWeaponInPool(const FGuid &PersistentID) const;

    // ==================== DEBUG ====================

    /** Dump the pool to log + screen: count, then per-weapon asset/tier/quality/PersistentID.
     *  Lets the pool be inspected without PIE-triggering a draw (per the debug-tools rule). */
    UFUNCTION(BlueprintCallable, Category = "Pool|Debug")
    void PrintPoolState() const;

    /** Formatted one-line-per-weapon snapshot of the pool for inspection. */
    UFUNCTION(BlueprintCallable, Category = "Pool|Debug")
    FString GetPoolString() const;

private:
    /** The persistent owned WEAPON pool. SaveGame-tagged (DORMANT — honoured at the
     *  disk-save phase). UPROPERTY so the nested data-asset UObject refs stay GC-reachable
     *  at subsystem scope. Mirrors the run UInventoryComponent's entry type exactly. */
    UPROPERTY(SaveGame)
    TArray<FWeaponInventoryEntry> OwnedWeapons;
};
