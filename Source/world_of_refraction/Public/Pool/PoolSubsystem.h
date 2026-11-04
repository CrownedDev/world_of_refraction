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
// Saved by explicit structs (FSavedPool, D-059; B-348 removed the SaveGame tags). In-session GI persistence
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
#include "Loadout/FSavedLoadout.h"
#include "Pool/PoolQueryTypes.h"                // FPoolFilter + the per-category result views
#include "Inventory/InventoryConstants.h"       // ITEM_SLOTS_START
#include "PoolSubsystem.generated.h"

class UInventoryData;
class UCharacterData;
class AActor;

/** B-091: a piece of shared gear changed in the pool (added, updated or removed). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPoolGearChanged, FGuid, GearID);

/** B-312 (D-117, Q-312): one owner's gear as views into the pool store: one instance per item, no copy. The pointers
 *  are valid until the pool's armoury next changes (an add or a removal): read them now, don't keep them. */
struct FPoolGearView
{
    TArray<const FWeaponInventoryEntry *> Weapons;
    TArray<const FRingInventoryEntry *> Adapters;
    TArray<const FEvolutionInventoryEntry *> Evolutions;

    int32 Num() const { return Weapons.Num() + Adapters.Num() + Evolutions.Num(); }
};

/** B-322 (D-117, Q-322): one character's pouch of carried item slots: one item type per slot, stacked to
 *  ITEM_STACK_MAX, ITEM_SLOTS_START slots to begin with. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FPoolPouch
{
    GENERATED_BODY()

    /** Crystal and stone stacks carried; one entry per slot. */
    UPROPERTY()
    TMap<FCrystalId, int32> Stacks;

    UPROPERTY()
    int32 SlotCount = InventoryConstants::ITEM_SLOTS_START;
};

/** B-314 (D-111, D-097, Q-314): where one piece of gear stands in the run layer, by its ID. Kept in the store beside
 *  the entry (as owners are until B-315), not on the entry struct, so a member's copy pushed back to the store can't
 *  wipe it. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FPoolRunMark
{
    GENERATED_BODY()

    /** Found or bought in this run: dropped at run end, its tier remembered for its definition. */
    UPROPERTY()
    bool bRunOnly = false;

    /** A home piece taken on the run: at run end it goes back to the home state below (D-093 wear, D-111 attachments). */
    UPROPERTY()
    bool bOnRun = false;

    /** A weapon's or adapter's attachment (with its wear) as it left home. */
    UPROPERTY()
    FRuntimeAttachedItem HomeAttached;

    /** The spells on that attachment as it left home. */
    UPROPERTY()
    TArray<USpellData *> HomeSpells;

    /** An evolution's durability as it left home. */
    UPROPERTY()
    int32 HomeDurability = 0;
};

/** B-314: what a run's end did to the store. */
struct FPoolRunEndResult
{
    /** Home pieces put back to their home state (only those the run changed). */
    int32 Restored = 0;
    /** Run-only pieces taken out of the store. */
    int32 Dropped = 0;
    /** Definitions whose remembered tier was written. */
    int32 Remembered = 0;
};

/** B-203: a pouch or the storage changed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPoolItemsChanged);

namespace PoolConstants
{
    /** B-311 (D-111): custom loadouts kept per character. */
    constexpr int32 MAX_CUSTOM_LOADOUTS = 5;
}

/** B-311 (D-111): one character's custom loadouts, pieces referenced by owned GUID. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FPoolCustomLoadouts
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FSavedLoadout> Loadouts;
};

/** B-091 (Q-091): a piece another member wears, taken out of a member's active loadout.
 *  OBSOLETE (B-321, D-117): members no longer share gear, so nothing clashes; delete after Crown's PIE pass. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPoolGearClash
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Pool|Armoury")
    FGuid GearID;

    UPROPERTY(BlueprintReadOnly, Category = "Pool|Armoury")
    FText ItemName;

    UPROPERTY(BlueprintReadOnly, Category = "Pool|Armoury")
    FText WearerName;

    UPROPERTY(BlueprintReadOnly, Category = "Pool|Armoury")
    FName LoserKey;

    UPROPERTY(BlueprintReadOnly, Category = "Pool|Armoury")
    FText LoserName;
};

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
    /** By ID through the GUID index (B-312); an unset ID finds nothing. Never change an entry's ID through the
     *  writable pointers below: the index would go stale (remove and add it instead). */
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

    // ==================== THE STORE: VIEWS (B-312, D-117, Q-312) ====================
    // The pool is the one store of owned gear; a member's gear is a view over it by owner (GetGearOwner). Lookups by
    // ID go through a GUID index (rebuilt lazily after an add or a removal) instead of scanning the arrays.

    /** The gear Owner owns (a character key, or GetChestKey()), in the pool's order. */
    FPoolGearView GetOwnedGearOf(FName Owner) const;

    /** Debug: each owner and the gear it owns, one line each (wor.Pool.PrintStore). */
    FString GetStoreString() const;

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

    /** B-090: a pool restored from a save counts as populated, so the authored seed can't add a second copy. */
    void MarkPopulated() { bPopulated = true; }

    // ==================== SHARED GEAR (B-091, D-020, Q-091) ====================
    // One armoury for the party. Each party member's inventory holds a copy of every piece
    // (UInventoryComponent::DrawSharedGear); a piece is worn by one member at a time, the one whose
    // active loadout holds it. A member that changes a piece pushes it here, and the others copy it.

    /** The key the pool knows a character by (its asset path). */
    static FName GetCharacterKey(const UCharacterData *CharacterData);

    /** Banks a character's starting gear the first time it draws; false when it was banked before. */
    bool BankStartingGear(FName CharacterKey, const TArray<FWeaponInventoryEntry> &Weapons,
                          const TArray<FRingInventoryEntry> &Rings, const TArray<FEvolutionInventoryEntry> &Evolutions);
    bool HasBankedGear(FName CharacterKey) const { return BankedCharacters.Contains(CharacterKey); }
    const TSet<FName> &GetBankedCharacters() const { return BankedCharacters; }

    /** Adds the piece, or replaces the one with its ID, and fires OnPoolGearChanged. */
    void UpdateWeapon(const FWeaponInventoryEntry &Entry);
    void UpdateRing(const FRingInventoryEntry &Entry);
    void UpdateEvolution(const FEvolutionInventoryEntry &Entry);

    /** Takes a sold or dismantled piece out of the pool (and off its wearer). */
    void RemoveGear(const FGuid &GearID);

    /** The character wearing the piece, or NAME_None. */
    FName GetWearer(const FGuid &GearID) const;
    void SetWearer(const FGuid &GearID, FName CharacterKey);
    void ReleaseWearer(const FGuid &GearID);
    const TMap<FGuid, FName> &GetWearers() const { return Wearers; }

    /** Replaces the clashes noted for LoserKey (the party panel lists them).
     *  OBSOLETE (B-321, D-117): wearers and clashes guarded the shared armoury; owners replace them. Delete after PIE. */
    void NoteClashes(FName LoserKey, const TArray<FPoolGearClash> &NewClashes);
    const TArray<FPoolGearClash> &GetClashes() const { return Clashes; }

    /** B-090 restore of who wears what and whose starting gear is banked. */
    void RestoreSharedGear(const TMap<FGuid, FName> &InWearers, const TArray<FName> &InBanked);

    // ==================== GEAR OWNERS (B-321, D-117) ====================
    // Each piece belongs to one character; a member draws only its own (the wearers above retire, flagged obsolete).

    /** The character who owns the piece, or NAME_None (a piece from before D-117 until AssignUnownedGear). */
    FName GetGearOwner(const FGuid &GearID) const;
    void SetGearOwner(const FGuid &GearID, FName CharacterKey);
    /** Gives every unowned piece an owner: whoever wears it, else Fallback (the party leader). Logs each; returns
     *  how many it gave. */
    int32 AssignUnownedGear(FName Fallback);
    const TMap<FGuid, FName> &GetGearOwners() const { return GearOwners; }
    void RestoreGearOwners(const TMap<FGuid, FName> &InOwners) { GearOwners = InOwners; }

    /** B-323 (D-117): the piece (a weapon or adapter in the pool; evolutions wait for Q-323) becomes ToKey's, and
     *  every member hears it (OnPoolGearChanged): the giver's copy drops out of its armoury and loadouts, the
     *  receiver's draws it. The rules (D-116 primary, D-115 duplicate) are checked by WoRGive (GearGive.h), not
     *  here. False when the piece isn't a weapon or adapter in the pool, ToKey is none, or it is already ToKey's. */
    bool GiveGear(const FGuid &GearID, FName ToKey);

    /** B-324 (D-117, Q-321): the shared home chest owns the gear put in it. It is an owner key no character can
     *  have (character keys are asset paths), distinct from an unowned piece (NAME_None): every member's draw
     *  leaves chest gear alone and AssignUnownedGear never hands it out. Put = GiveGear(ID, GetChestKey()); take =
     *  GiveGear(ID, member). */
    static FName GetChestKey();
    bool IsInChest(const FGuid &GearID) const { return GetGearOwner(GearID) == GetChestKey(); }

    // ==================== CUSTOM LOADOUTS (B-311, D-111) ====================
    // The player's own loadouts per character (lore loadouts come from the character asset and are not kept).

    /** Replaces the character's custom loadouts; keeps the first MAX_CUSTOM_LOADOUTS (warns past that). */
    void SetCustomLoadouts(FName CharacterKey, const TArray<FSavedLoadout> &Loadouts);
    /** The character's custom loadouts, or null when none are kept. */
    const TArray<FSavedLoadout> *GetCustomLoadouts(FName CharacterKey) const;
    const TMap<FName, FPoolCustomLoadouts> &GetAllCustomLoadouts() const { return CustomLoadouts; }

    UPROPERTY(BlueprintAssignable, Category = "Pool|Armoury")
    FOnPoolGearChanged OnPoolGearChanged;

    // ==================== POUCHES AND STORAGE (B-203, B-322, D-097, D-117, Q-322) ====================
    // Each character carries its own pouch of item slots (one item type, a crystal type and tier, per
    // slot, stacked to ITEM_STACK_MAX; ITEM_SLOTS_START slots each); everything else waits in the one
    // shared storage (the Crystals / Stones above, unlimited). A member's UCrystalInventoryComponent
    // mirrors its own pouch. Every call takes the character key (GetCharacterKey) of the pouch it means.

    /** The character's pouch (an empty default when it has none yet). */
    const FPoolPouch &GetPouch(FName CharacterKey) const;
    const TMap<FName, FPoolPouch> &GetPouches() const { return Pouches; }
    const TMap<FCrystalId, int32> &GetCarriedItems(FName CharacterKey) const { return GetPouch(CharacterKey).Stacks; }
    int32 GetCarriedCount(FName CharacterKey, const FCrystalId &Id) const;
    int32 GetItemSlotCount(FName CharacterKey) const { return GetPouch(CharacterKey).SlotCount; }
    int32 GetUsedItemSlots(FName CharacterKey) const { return GetPouch(CharacterKey).Stacks.Num(); }

    /** How many of Count fit the character's pouch (a free slot, or room in Id's stack). */
    int32 GetCarryRoom(FName CharacterKey, const FCrystalId &Id) const;

    /** D-097: what fits the character's pouch is carried, the rest goes to storage (never thrown away).
     *  Returns how many went to storage. */
    int32 CarryOrStore(FName CharacterKey, const FCrystalId &Id, int32 Count);

    /** Takes up to Count items from the character's pouch; returns how many. */
    int32 TakeCarried(FName CharacterKey, const FCrystalId &Id, int32 Count);

    /** Storage holds Id: the count waiting there. */
    int32 GetStoredCount(const FCrystalId &Id) const;

    /** Moves Count of the character's carried items to storage. False (nothing moved) when fewer are carried. */
    bool MoveToStorage(FName CharacterKey, const FCrystalId &Id, int32 Count);

    /** A character leaves for good (a run-only member at run end): its whole pouch goes to storage
     *  (D-097: items are never thrown away) and the pouch is removed. Returns how many items moved. */
    int32 ReleasePouch(FName CharacterKey);

    /** Moves Count stored items into the character's pouch. False (nothing moved) when fewer are
     *  stored or they don't fit (no free slot, or the stack is full). */
    bool MoveToCarried(FName CharacterKey, const FCrystalId &Id, int32 Count);

    /** Banks a character's starting items into its own pouch the first time it draws (the rest
     *  stored); false when banked before. */
    bool BankStartingItems(FName CharacterKey, const TMap<FCrystalId, int32> &Items);
    bool HasBankedItems(FName CharacterKey) const { return BankedItemCharacters.Contains(CharacterKey); }
    const TSet<FName> &GetBankedItemCharacters() const { return BankedItemCharacters; }

    /** B-090 restore of every pouch and whose items are banked. */
    void RestorePouches(const TMap<FName, FPoolPouch> &InPouches, const TArray<FName> &InBanked);

    /** B-323 (D-117): moves Count of FromKey's carried Id into ToKey's pouch. False (nothing moved) when FromKey
     *  carries fewer, or ToKey's pouch has no room for all of them (a full pouch refuses). */
    bool GiveCarried(FName FromKey, FName ToKey, const FCrystalId &Id, int32 Count);

    /** Debug: each character's pouch (slots in use, each stack) and storage. wor.Inventory.PrintSlots. */
    FString GetItemSlotsString() const;

    UPROPERTY(BlueprintAssignable, Category = "Pool|Items")
    FOnPoolItemsChanged OnPoolItemsChanged;

    // ==================== THE RUN LAYER (B-314, D-111, D-097, Q-314) ====================
    // A run is a layer over the store, one instance per item: a home piece taken on the run is the real piece, marked
    // "on the run" with its home state kept; gear found or bought in the run is marked run-only. At run end the home
    // pieces go back to their home state (wear and attachments added in the run), run-only gear leaves the store, and
    // each run-only definition's tier is remembered so the next copy found arrives at that tier. PoolRunLayer.cpp.

    /** A run starts (UTrialRunSubsystem::EnterTrial / RestoreRun). A layer still open is discarded, not applied (the
     *  store may have been reloaded under it); EnterTrial ends the last one itself first. */
    void BeginRunLayer();
    /** Closes the layer without putting anything back or dropping anything (stale marks after a reload). */
    void DiscardRunLayer();
    bool IsRunLayerActive() const { return bRunLayerActive; }

    /** The run purchase (D-111): the real home piece goes on the run as it is now. False when no run layer is open,
     *  the piece isn't in the store, or it is already on the run or run-only. */
    bool TakeOnRun(const FGuid &GearID);

    /** B-347 (D-137): the run loadout is free: the pieces the given party members wear (their active loadouts, already
     *  within the D-110 slot budget), and an evolution attached to one of them, go on the run as they are. Unworn
     *  home pieces stay home (one touched during the run is taken then, UInventoryComponent::PushGearToPool). Reads
     *  the wearers record until D-122 moves the worn loadout onto each member's component. Returns how many were
     *  taken. */
    int32 TakeHomeGearOnRun(const TSet<FName> &WearerKeys);

    /** Gear found or bought in the run. False when no run layer is open, the ID is unset, or it is a home piece. */
    bool MarkRunOnly(const FGuid &GearID);
    bool IsRunOnly(const FGuid &GearID) const;
    bool IsOnRun(const FGuid &GearID) const;
    const TMap<FGuid, FPoolRunMark> &GetRunMarks() const { return RunMarks; }

    /** The run ends (UTrialRunSubsystem::ExitTrial): home pieces back to their home state, run-only gear dropped
     *  with its tier remembered per definition. Every change fires OnPoolGearChanged. No-op when no layer is open. */
    FPoolRunEndResult EndRunLayer();

    /** The key upgrade memory knows a gear definition by (its asset path), as GetCharacterKey does for characters. */
    static FName GetDefinitionKey(const UObject *Definition);
    /** The tier Definition reached in an earlier run; false when none is remembered. */
    bool FindRememberedTier(const UObject *Definition, EItemTier &OutTier) const;
    /** Raises a newly found piece to its definition's remembered tier. True when it changed. */
    bool ApplyUpgradeMemory(FWeaponInventoryEntry &Entry) const;
    bool ApplyUpgradeMemory(FRingInventoryEntry &Entry) const;
    bool ApplyUpgradeMemory(FEvolutionInventoryEntry &Entry) const;
    const TMap<FName, EItemTier> &GetUpgradeMemory() const { return UpgradeMemory; }
    /** TODO: B-314b saves the memory and the open layer (save v10); until then both are in-session only. */
    void RestoreUpgradeMemory(const TMap<FName, EItemTier> &InMemory) { UpgradeMemory = InMemory; }

    /** Debug: the open layer (each marked piece) and the upgrade memory. wor.Run.PrintKept. */
    FString GetRunLayerString() const;

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
    // Saved by FSavedPool (D-059), not property tags. UPROPERTY so the nested
    // data-asset UObject refs stay GC-reachable at subsystem scope. Each mirrors the run
    // component's storage type exactly.

    // ARMOURY
    UPROPERTY()
    TArray<FWeaponInventoryEntry> OwnedWeapons;

    UPROPERTY()
    TArray<FRingInventoryEntry> OwnedRings;

    UPROPERTY()
    TArray<FEvolutionInventoryEntry> OwnedEvolutions;

    // B-312: GUID -> array index for each armoury array. Rebuilt on demand; a stale hit (an entry moved) rebuilds too.
    mutable TMap<FGuid, int32> WeaponIndex;
    mutable TMap<FGuid, int32> AdapterIndex;
    mutable TMap<FGuid, int32> EvolutionIndex;
    mutable bool bGearIndexDirty = true;

    void MarkGearIndexDirty() { bGearIndexDirty = true; }
    void RebuildGearIndex() const;
    /** Index of the entry with ID in Entries through Index (rebuilding once on a miss or a stale hit), or INDEX_NONE. */
    template <class TEntry, class FIdOf>
    int32 FindIndexed(const TArray<TEntry> &Entries, const TMap<FGuid, int32> &Index, const FGuid &ID, FIdOf IdOf) const;

    // KNOWLEDGE
    UPROPERTY()
    TArray<FSpellInstance> OwnedSpells;

    UPROPERTY()
    TArray<FAbilityInstance> OwnedAbilities;

    // ITEMS — counted stacks keyed by (Type, Tier); fungible, no per-instance identity.
    // Mirrors the run UCrystalInventoryComponent's 2-pool gem/stone split EXACTLY (so draw/
    // return is a direct structural copy, not a translation). Crystals are dual-purpose (one bucket,
    // no item/refined split); Stones are slot-only.
    UPROPERTY()
    TMap<FCrystalId, int32> Crystals;

    UPROPERTY()
    TMap<FCrystalId, int32> Stones;

    // ==================== WALLET (currency value-block) ====================
    // Mirrors UCurrencyComponent's VALUES (not the component — can't hold a UActorComponent
    // in a GI subsystem). All POD, so no asset-ref/soft-pointer caveat. Separate from the 3
    // browse categories.
    //
    // Gold is RUN-VOLATILE (never banked) — the run component tags it Replicated-only, NOT
    // SaveGame; mirrored faithfully here (NO SaveGame tag), so it will not persist into the
    // pool.
    // OBSOLETE (B-348): PoolGold .. PoolEssence are dead scaffolding: nothing writes them and nothing saves them
    // (the wallet is banked from the player's UCurrencyComponent by FSavedWallet, D-059). Delete with B-317.
    UPROPERTY()
    int32 PoolGold = 0;

    UPROPERTY()
    int32 PoolPrisms = 0;

    UPROPERTY()
    int32 PoolDiamond = 0;

    UPROPERTY()
    int32 PoolGearEssence = 0;

    UPROPERTY()
    int32 PoolSkillEssence = 0;

    // 14 typed-essence balances. FCurrencyArray reused for structural parity; at pool scope
    // its OwnerComponent back-ref stays null (its only job is client replication callbacks,
    // dormant here) and it functions as a pure serializable balance store.
    UPROPERTY()
    FCurrencyArray PoolEssence;

    // ==================== SHARED GEAR (B-091) ====================
    // Saved by explicit structs (FSavedPool, D-059), not these tags.

    /** Gear PersistentID / evolution InstanceID -> the character key wearing it. */
    UPROPERTY()
    TMap<FGuid, FName> Wearers;

    /** Characters whose starting gear is already in the pool. */
    UPROPERTY()
    TSet<FName> BankedCharacters;

    /** B-321: gear PersistentID / evolution InstanceID -> the owning character key. */
    UPROPERTY()
    TMap<FGuid, FName> GearOwners;

    /** B-311: character key -> its custom loadouts. */
    UPROPERTY()
    TMap<FName, FPoolCustomLoadouts> CustomLoadouts;

    /** The latest pieces taken out of a member's loadout because another member wears them. */
    UPROPERTY(Transient)
    TArray<FPoolGearClash> Clashes;

    // ==================== POUCHES (B-203, B-322) ====================
    // Saved by explicit structs (FSavedPool, D-059), not these tags.

    /** Each character's pouch, by character key. */
    UPROPERTY()
    TMap<FName, FPoolPouch> Pouches;

    /** Characters whose starting items are already carried or stored. */
    UPROPERTY()
    TSet<FName> BankedItemCharacters;

    FPoolPouch &FindOrAddPouch(FName CharacterKey);

    // ==================== THE RUN LAYER (B-314) ====================
    // In-session until B-314b's save (Q-314).

    /** A run is open (between BeginRunLayer and EndRunLayer). */
    bool bRunLayerActive = false;

    /** Gear ID -> its run mark, while a run is open. */
    UPROPERTY()
    TMap<FGuid, FPoolRunMark> RunMarks;

    /** Gear definition key (GetDefinitionKey) -> the highest tier a run-only copy of it reached (D-097). */
    UPROPERTY()
    TMap<FName, EItemTier> UpgradeMemory;

    /** Remembers Tier for Definition (the higher of it and what is remembered). True when it changed. */
    bool RememberTier(const UObject *Definition, EItemTier Tier);
    /** Remembers the tier of the run-only piece GearID (still in the store). True when it changed. */
    bool RememberRunOnlyTier(const FGuid &GearID);
    /** RemoveGear, before the piece goes: a run-only piece's tier is remembered (Q-314c) and its mark dropped. */
    void NoteRunGearRemoved(const FGuid &GearID);

    /** Takes up to Count stored items; returns how many. */
    int32 TakeStored(const FCrystalId &Id, int32 Count);
};
