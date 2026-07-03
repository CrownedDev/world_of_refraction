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
//   - ITEMS:     Crystals + Stones (counted stacks; mirrors the run 2-pool split)
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
#include "Pool/PoolQueryTypes.h"                // FPoolFilter + the per-category result views
#include "PoolSubsystem.generated.h"

class UInventoryData;
class AActor;

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
    // Two pools mirroring the run component (gem-merge): Crystals (dual-purpose) + Stones (slot-only).
    // gem/stone is auto-dispatched by IsGemType; there is no item/refined axis anymore.

    /** Add Count crystals at Id — routes to Crystals or Stones by IsGemType. INERT — no cap enforcement
     *  this phase (the run component owns caps; the pool is raw storage). */
    UFUNCTION(BlueprintCallable, Category = "Pool|Items")
    void AddCrystalToPool(const FCrystalId &Id, int32 Count = 1);

    const TMap<FCrystalId, int32> &GetCrystals() const { return Crystals; }
    const TMap<FCrystalId, int32> &GetStones() const { return Stones; }

    // ==================== CATEGORY GETTERS (filtered browse views) ====================
    // Apply only the axes valid for each category's member types (the rest exclude — see
    // PoolAccessors). Return by value (filtered COPIES) — pools are hundreds-sized; this is
    // the UI/draw read path. INERT: uncalled until the draw/UI phases.

    /** KNOWLEDGE — spells (element/school/tier/quality) + abilities (weapon-type/tier/quality). */
    UFUNCTION(BlueprintCallable, Category = "Pool|Query")
    FPoolKnowledgeView GetKnowledge(const FPoolFilter &Filter) const;

    /** ARMOURY — weapons (element-via-crystal/weapon-type/tier/quality) + rings + evolutions. */
    UFUNCTION(BlueprintCallable, Category = "Pool|Query")
    FPoolArmouryView GetArmoury(const FPoolFilter &Filter) const;

    /** ITEMS — crystal stacks (element-via-key/tier; NO quality), both pools, tagged bRefined. */
    UFUNCTION(BlueprintCallable, Category = "Pool|Query")
    TArray<FPoolItemStack> GetItems(const FPoolFilter &Filter) const;

    /** Cross-category — all three at once (element is the natural cross-axis). */
    UFUNCTION(BlueprintCallable, Category = "Pool|Query")
    FPoolQueryResult QueryAll(const FPoolFilter &Filter) const;

    // ==================== POPULATE (authored → pool) ====================

    /** Seed the pool from an authored UInventoryData asset, mirroring the POPULATE half of
     *  UInventoryComponent::InitializeFromInventoryAsset but targeting the POOL (account-wide, flat
     *  — no per-character keying). Weapons/Rings reuse the SAME factory primitive the run uses
     *  (FWeaponInventoryEntry::CreateFromWeapon + a freshly-minted PersistentID, default crystal +
     *  DefaultSpells always copied in); Spells/Abilities mint FSpellInstance/FAbilityInstance at learn-time semantics;
     *  Crystals route through AddCrystalToPool (gem/stone dispatch); Evolutions mint
     *  an FEvolutionInventoryEntry per authored item.
     *
     *  ONE-TIME per session: the pool is a GI subsystem, so this guards on bPopulated and is a no-op
     *  on every call after the first (calling per-pawn-BeginPlay would otherwise duplicate). The run
     *  seed path (InitializeFromCharacterData) is UNTOUCHED — this is purely additive content for the
     *  pool; nothing draws FROM the pool yet (Step 2). OwnerContext is the sourcing character actor
     *  (reserved for a future Luck-biased acquisition roll; authored banking is deterministic today). */
    void PopulateFromInventoryAsset(UInventoryData *Asset, AActor *OwnerContext);

    // ==================== DEBUG ====================

    /** Dump the whole pool to log + screen: per-category counts, crystal stacks, and the
     *  wallet. Inspect without PIE-triggering a draw (per the debug-tools rule). */
    UFUNCTION(BlueprintCallable, Category = "Pool|Debug")
    void PrintPoolState() const;

    /** Formatted multi-section snapshot of the pool for inspection. */
    UFUNCTION(BlueprintCallable, Category = "Pool|Debug")
    FString GetPoolString() const;

private:
    // ==================== SESSION STATE ====================
    // One-time populate guard. The pool is GI-scoped (one instance per session), so
    // PopulateFromInventoryAsset must run ONCE — a bPopulated flag (not an is-empty check) so a
    // legitimately-empty authored asset can't re-trigger. NOT SaveGame: session-derived, re-seeded
    // per session (disk-save is a later dormant phase).
    UPROPERTY(Transient)
    bool bPopulated = false;

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
    // Mirrors the run UCrystalInventoryComponent's 2-pool gem/stone split EXACTLY (so draw/
    // return is a direct structural copy, not a translation). Crystals are dual-purpose (one bucket,
    // no item/refined split); Stones are slot-only.
    UPROPERTY(SaveGame)
    TMap<FCrystalId, int32> Crystals;

    UPROPERTY(SaveGame)
    TMap<FCrystalId, int32> Stones;

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
