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
#include "Inventory/EInventoryChangeType.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/FFusionId.h"
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
class UPoolSubsystem;

/** Fired on EVERY inventory mutation (grant / removal / equip / bulk-load).
 *  The foundation change signal (InstanceBasedRuntimeLayer_Design.md #10) —
 *  loot / shop / UI react to it instead of polling. Mirrors ULoadoutComponent's
 *  OnLoadoutChanged shape: a re-read trigger, not a data carrier. Consumers
 *  query the inventory for the current state; ChangeType is an optimisation hint. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryChanged, EInventoryChangeType, ChangeType);

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

    /** DRAW the run inventory FROM the account pool instead of the authored asset
     *  (Pool Draw step 2). Mirrors InitializeFromInventoryAsset's CLEAR + POPULATE,
     *  but WHOLE-ENTRY copies the pool's owned instances — preserving instance
     *  identity (PersistentID / Tier / Quality / InstanceID). Does NOT re-run the
     *  asset factories (which would mint new GUIDs and reset tier to asset-base).
     *  SavedLoadouts still inflate from the owner's authored CharacterData->Inventory,
     *  resolved against the freshly-drawn owned inventory. NOT YET wired into
     *  InitializeFromCharacterData — reachable only via the wor.DrawFromPool debug
     *  command until step 3 (the pool-if-present-else-authored branch). */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Setup")
    void InitializeFromPool(UPoolSubsystem *Pool);

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

    /** Socket a crystal (refined gem) or augment stone onto the owned weapon (by PersistentID).
     *  DEBIT model (unlike evolution's reference model): one is consumed from the crystal stack —
     *  a refined gem from the refined pool, an augment stone from the item pool — and the socketed
     *  crystal becomes fungible slot state (destroyed on detach, no return). Debit happens BEFORE
     *  the write; a 0-debit (not owned) rejects with no phantom socket. Mirrors AttachEvolution's
     *  guard structure: no-authority / no-weapon / occupied-slot all return false (no replace).
     *  The debit goes through the SILENT atomic core, so a successful socket fires ONE signal: Equipped. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    bool AttachCrystalToWeapon(FGuid WeaponPersistentID, FCrystalId Id);

    /** Fuse two crystal halves directly onto the owned weapon (by PersistentID). Fusions have no
     *  loose-inventory home — this CONSUMES both halves from their stacks (fuse-and-socket) and
     *  writes the runtime fusion attachment. Rejects an invalid pair (IsValidFusionPair). ATOMIC:
     *  both halves are confirmed owned before either is debited (no half-consumed-then-fail) — via the
     *  SILENT atomic core. Weapon accepts both elemental and augmented fusions. Fires ONE signal: Equipped. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weapons")
    bool AttachFusionToWeapon(FGuid WeaponPersistentID, FFusionId FusionId);

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

    /** Socket a refined gem onto the owned ring (by PersistentID). Ring counterpart of
     *  AttachCrystalToWeapon — same DEBIT model, but with the inline ring-guard
     *  (URingData::IsDataValid:29-33): augment stones are weapon-only, so a non-gem Id is rejected.
     *  Refined gems are allowed (rings use crystals for their spell source). Fires ONE signal: Equipped. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    bool AttachCrystalToRing(FGuid RingPersistentID, FCrystalId Id);

    /** Fuse two crystal halves directly onto the owned ring (by PersistentID). Ring counterpart of
     *  AttachFusionToWeapon, with the inline ring-guard (URingData::IsDataValid:40-44): rings accept
     *  only ELEMENTAL fusions (one gem half); augmented fusions (two stones) are weapon-only and are
     *  rejected. Same invalid-pair reject + ATOMIC both-halves debit (silent core). Fires ONE signal: Equipped. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Rings")
    bool AttachFusionToRing(FGuid RingPersistentID, FFusionId FusionId);

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

    // ==================== CRYSTAL / EVOLUTION FACADE ====================
    // The crystal/evolution stores live on SIBLING components
    // (UCrystalInventoryComponent / UEvolutionInventoryComponent), reached
    // per-call off the owner. These thin wrappers route every mutation through
    // the facade that owns OnInventoryChanged, so crystal/evolution grants emit
    // the change signal like the native weapon/ring/spell paths do. Each returns
    // false / 0 (and broadcasts nothing) when the sibling is missing or the op
    // didn't change anything.

    /** Add Count crystals at Id via the sibling pool (gem/stone dispatched inside the component).
     *  Broadcasts Added on success. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    bool AddCrystal(FCrystalId Id, int32 Count = 1);

    /** Remove up to Count crystals at Id (gem/stone dispatched inside the component). Returns the
     *  number actually removed; broadcasts Removed when that is > 0. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    int32 RemoveCrystal(FCrystalId Id, int32 Count = 1);

    /** Atomic multi-remove primitive: remove the WHOLE set of crystals in one verify-then-commit,
     *  firing a SINGLE Removed (not one per item). Pool-AGNOSTIC — the component dispatches each Id to
     *  Crystals or Stones internally; the primitive just calls the unified count methods. VERIFY — every
     *  Id (duplicates summed) must have enough; any shortfall removes NOTHING and returns false.
     *  COMMIT — debit each. The general consume primitive (merge inputs, etc.); the attach-ops share
     *  its silent core but fire Equipped instead. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    bool RemoveCrystals(const TArray<FCrystalId> &Ids);

    /** Atomic multi-ADD primitive: add the WHOLE set in one verify-then-commit, firing a SINGLE
     *  Added. Pool-AGNOSTIC, mirroring RemoveCrystals. ATOMIC-GUARDED because adds CAN fail on the
     *  per-tier cap — VERIFY every Id (duplicates summed) fits via CanAddCount; any that wouldn't fit
     *  adds NOTHING and returns false. COMMIT — add each. (Merge output / refund consume this.) */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Crystals")
    bool AddCrystals(const TArray<FCrystalId> &Ids);

    /** Add an evolution instance via the sibling component. Broadcasts Added on success. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Evolution")
    bool AddEvolutionInstance(UEvolutionItemData *Item);

    /** Remove the owned evolution instance with this InstanceID. Broadcasts Removed on success. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Evolution")
    bool RemoveEvolutionInstance(FGuid InstanceID);

    // ==================== UTILITY ====================

    /** Get inventory summary for debug */
    UFUNCTION(BlueprintPure, Category = "Inventory|Debug")
    FString GetInventorySummary() const;

    /** Per-INSTANCE state dump (the diff tool for the pool draw — counts alone can't
     *  prove instance state survived). Lists, per owned item, the identity + leveled/
     *  rolled axes that the whole-entry draw must preserve:
     *   - weapons / rings : PersistentID (short), Tier, Quality, attached crystal (Type/Tier or none)
     *   - spells / abilities : InstanceID (short), Tier, Quality, asset name
     *   - evolutions : InstanceID (short), Tier, Quality, durability
     *   - crystals : per-pool stacks (Type Tier xN) — same shape as PrintPoolState for direct diff
     *  Read-only; resolves the sibling Crystal/Evolution components off the owner. After
     *  wor.PopulatePool + wor.DrawFromPool, this should MATCH wor.PrintPool's tiers/crystals. */
    UFUNCTION(BlueprintPure, Category = "Inventory|Debug")
    FString GetInventoryInstanceString() const;

    // ==================== EVENTS ====================

    /** Fired on every inventory mutation — grant, removal, equip, or bulk-load.
     *  The foundation signal (gap #10): bind here to react to inventory changes
     *  without polling. Query the inventory for current state; ChangeType hints
     *  what kind of change fired. */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnInventoryChanged OnInventoryChanged;

private:
    /** Single broadcast funnel for OnInventoryChanged — every mutation site
     *  routes through here on its success path. const: the delegate broadcast
     *  doesn't mutate logical inventory state. */
    void BroadcastInventoryChanged(EInventoryChangeType ChangeType) const;

    /** SILENT atomic verify-then-commit core behind RemoveCrystals and the crystal/fusion attach
     *  debits. Pool-AGNOSTIC: verifies every Id (duplicates summed) has enough via GetCount, then
     *  debits via RemoveCount — the component dispatches gem/stone internally. Does NOT broadcast —
     *  the caller fires the single signal (RemoveCrystals → Removed; attach-ops → Equipped). Returns
     *  false (removing nothing) on any shortfall or a missing crystal sibling. */
    bool CommitRemoveCrystals(const TArray<FCrystalId> &Ids);

    /** SILENT atomic add core behind AddCrystals. Pool-AGNOSTIC + ATOMIC-GUARDED: verifies every Id
     *  (duplicates summed) fits its per-tier cap via CanAddCount BEFORE adding any; on any rejection
     *  adds nothing and returns false. COMMIT adds via AddCount. Does NOT broadcast (caller fires one
     *  Added). NOTE: the per-Id cap check is exact for distinct (pool, tier) Ids and same-Id duplicates;
     *  a batch with DIFFERENT Ids sharing one (pool, tier) would need a grouped check — not reachable by
     *  current callers (merge output is single; refund is one Id per distinct tier). */
    bool CommitAddCrystals(const TArray<FCrystalId> &Ids);

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
