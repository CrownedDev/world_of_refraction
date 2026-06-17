// LoadoutComponent.h
// Active combat loadout and runtime battle state
//
// ARCHITECTURE:
// InventoryComponent = What you OWN (warehouse)
// LoadoutComponent = What you're USING in battle (equipped gear)
//
// Players can save multiple loadout configurations.
// At battle start, active loadout is validated against inventory.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/ECharacterClass.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/Entries/FItemLoadoutSlot.h"
#include "Equipment/FEquippedItemSlot.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Character/CharacterDataComponent.h"
#include "Equipment/EAttachedItemKind.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Skills/Definitions/ESpellSource.h"
#include "LoadoutComponent.generated.h"

class UInventoryComponent;
class USpellData;
class UAbilityData;
class UEvolutionItemData;
class URingData;
class UEvolutionItemData;
struct FWeaponLoadoutEntry;
struct FRingLoadoutEntry;
struct FRuntimeAttachedItem;
class UStanceData;
class UWeaponAttackData;
class USkillDataBase;
class UAnimMontage;
class USpellData;
class UInfusionDisplayData;

/** Delegate for loadout changes (query GetActiveLoadout() for data) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadoutChanged, int32, NewLoadoutIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLoadoutItemUsed, int32, SlotIndex, FCrystalId, ItemId, int32, NewQuantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadoutValidationFailed, const FString &, Reason);

/** A single crystal-bearing slot — discriminated by Kind. Refined slots
 *  identify by FCrystalId (refined crystals have no asset pointer in the
 *  new model); Evolution slots carry the item asset pointer.
 *  Holder is UObject* because UWeaponData and URingData don't share
 *  a base beyond UPrimaryDataAsset; consumers cast as needed. The Evolution
 *  primary slot uses the item itself as its holder.
 *
 *  Populated by ULoadoutComponent::GetEquippedCrystals; consumers branch on
 *  Kind for the refined-vs-evolution discriminator. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEquippedCrystalSlot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Crystal")
    EAttachedItemKind Kind = EAttachedItemKind::None;

    /** Meaningful when Kind == Refined. */
    UPROPERTY(BlueprintReadOnly, Category = "Crystal")
    FCrystalId CrystalId;

    /** Meaningful when Kind == Evolution. */
    UPROPERTY(BlueprintReadOnly, Category = "Crystal")
    UEvolutionItemData *Item = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Crystal")
    UObject *Holder = nullptr;
};

/** Discriminates which loadout slot an FInvalidSlotFinding refers to.
 *  `None` marks a non-slot guard failure (no inventory, invalid index) —
 *  surfaced but not clearable. `BDPoolSpell` is a non-clearable BD-pool
 *  diagnostic. The three *Overflow / DuplicateItems values are "category"
 *  findings (no single slot) that ClearInvalidSlots re-derives the clearing
 *  action for. */
UENUM()
enum class ELoadoutSlotType : uint8
{
    None,
    PrimaryWeapon,
    PrimaryRing,
    PrimaryEvolution,
    SecondaryWeapon,
    ResonatorRing,
    InnateSpell,
    WeaponAbility,
    WeaponSpell,
    RingSpell,
    BDPoolSpell,
    // Category findings — bClearable, but cleared by re-derived rules rather
    // than nulling one addressable slot.
    EvolutionOverflow,
    RingOverflow,
    DuplicateItems
};

/** One actionable validation finding, produced by
 *  ULoadoutComponent::CollectInvalidSlotFindings — the structured counterpart to
 *  the human-readable strings from GetValidationErrors. SlotType + SlotIndex make
 *  a finding addressable so ClearInvalidSlots can null the exact slot.
 *  bClearable == false marks diagnostics (guards, structural caps, BD pools) that
 *  are reported but never auto-cleared. */
USTRUCT()
struct WORLD_OF_REFRACTION_API FInvalidSlotFinding
{
    GENERATED_BODY()

    UPROPERTY()
    ELoadoutSlotType SlotType = ELoadoutSlotType::None;

    /** Index within the slot category (ability index, spell index, ring index).
     *  -1 for non-indexed / single slots (primary weapon, primary ring, etc.).
     *  For RingSpell: the ring-loadout index (-1 = Generic/Caster primary ring). */
    UPROPERTY()
    int32 SlotIndex = 0;

    /** Secondary index for nested slots. For RingSpell: the spell index within
     *  the ring's AssignedSpells. -1 when unused. */
    UPROPERTY()
    int32 SubIndex = -1;

    UPROPERTY()
    FString Reason;

    UPROPERTY()
    bool bClearable = true;
};

/**
 * ULoadoutComponent
 * Manages active combat loadout and runtime battle state
 *
 * Responsibilities:
 * - Store saved loadout configurations
 * - Track active loadout for current battle
 * - Validate loadouts against inventory
 * - Track runtime state (item uses, etc.)
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API ULoadoutComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULoadoutComponent();

    // ==================== CONFIGURATION ====================

    /** Character class (determines loadout rules) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Config")
    ECharacterClass CharacterClass = ECharacterClass::Generic;

    // ==================== ACTIVE LOADOUT ====================

    /** Get the currently active loadout (C++ only - use specific getters for Blueprint) */
    FCombatLoadout GetActiveLoadout() const;

    /** Get active loadout by reference (C++ only) */
    bool GetActiveLoadoutRef(FCombatLoadout &OutLoadout) const;

    /** Set active loadout index */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    bool SetActiveLoadoutIndex(int32 Index);

    /** Get active loadout index */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    int32 GetActiveLoadoutIndex() const;

    /** Get active loadout name */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    FString GetActiveLoadoutName() const;

    /** Set active ring index for Resonator */
    void SetActiveRingIndex(int32 NewIndex);

    // ==================== LOADOUT MANAGEMENT ====================

    /** Create a new empty loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Management")
    int32 CreateNewLoadout(const FString &LoadoutName);

    /** Create and populate a new loadout with spells, abilities, and items.
     *  Crystal-refactor 9.5b: ItemsToAdd is now TArray<FEquippedItemSlot>
     *  (CrystalId + Quantity per entry). Blueprint callers must re-wire. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Management")
    int32 CreateAndConfigureLoadout(
        const FString &LoadoutName,
        UInventoryComponent *Inventory,
        const TArray<USpellData *> &SpellsToAdd,
        const TArray<UAbilityData *> &AbilitiesToAdd,
        const TArray<FEquippedItemSlot> &ItemsToAdd);

    /** Delete a saved loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Management")
    bool DeleteLoadout(int32 Index);

    /** Duplicate a loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Management")
    int32 DuplicateLoadout(int32 SourceIndex, const FString &NewName);

    /** Rename a loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Management")
    bool RenameLoadout(int32 Index, const FString &NewName);

    /** Get loadout count */
    UFUNCTION(BlueprintPure, Category = "Loadout|Management")
    int32 GetLoadoutCount() const;

    /** Get loadout names for UI */
    UFUNCTION(BlueprintPure, Category = "Loadout|Management")
    TArray<FString> GetLoadoutNames() const;

    // ==================== VALIDATION ====================

    /** Validate active loadout against inventory */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Validation")
    bool ValidateActiveLoadout(UInventoryComponent *Inventory);

    /** Validate specific loadout against inventory */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Validation")
    bool ValidateLoadout(int32 Index, UInventoryComponent *Inventory);

    /** Get validation errors for loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Validation")
    TArray<FString> GetValidationErrors(int32 Index, UInventoryComponent *Inventory) const;

    /** Slot-actionable counterpart to GetValidationErrors. Re-runs the same
     *  validation against the ACTIVE saved loadout but returns structured,
     *  addressable findings (SlotType + SlotIndex) instead of strings, so
     *  ClearInvalidSlots can null the exact offending slots. GetValidationErrors
     *  is intentionally left untouched for its existing callers.
     *
     *  Inventory is resolved via GetInventoryComponent(); operates on the active
     *  loadout index. (C++ only — the BP-facing surface is the Phase 5 debug
     *  helper / the eventual clear path.)
     *
     *  Parity with GetValidationErrors (ownership + structural +
     *  class-element-castability + BD pool), plus element-source and weapon-type
     *  coherence detection. */
    TArray<FInvalidSlotFinding> CollectInvalidSlotFindings() const;

    /** Clears every clearable finding from CollectInvalidSlotFindings on the
     *  active loadout. Mutates Inv->SavedLoadouts[ActiveLoadoutIndex] in place
     *  (the live runtime FCombatLoadout — see :536 note). Single-pass: validate
     *  once, clear once; a clear that exposes a new invalidity persists until
     *  the next validation. Logs one LogTemp Warning per cleared slot. */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    void ClearInvalidSlots();

    // ==================== BATTLE PREPARATION ====================

    /** Prepare loadout for battle (reset item uses, validate) */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Battle")
    bool PrepareForBattle(UInventoryComponent *Inventory);

    /** Check if loadout is ready for battle */
    UFUNCTION(BlueprintPure, Category = "Loadout|Battle")
    bool IsReadyForBattle() const { return bIsReadyForBattle; }

    // ==================== RUNTIME COMBAT STATE ====================

    /** Use an item from loadout (decrements uses) */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Combat")
    bool UseItem(int32 SlotIndex);

    /** Get remaining uses for item slot */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    int32 GetItemRemainingUses(int32 SlotIndex) const;

    /** Get attacks from ALL equipped weapons (for AI evaluation). Base type (attack/ability merge
     *  step 5a) — holds UWeaponAttackData today, UAbilityData after step-5b conversion. */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<USkillDataBase *> GetAllWeaponAttacks() const;

    /** Get all available abilities from active loadout */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<UAbilityData *> GetAvailableAbilities() const;

    /** Get all available spells from active loadout */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<USpellData *> GetAvailableSpells() const;

    /** Resolve which source a spell came from on the active loadout. Precedence
     *  order (first match wins): Innate (including BD per-element pools) →
     *  RingCrystal (primary ring + Resonator ring loadout) → WeaponCrystal
     *  (primary + secondary weapon spell lists) → Evolution. Defaults to Innate
     *  with a warning if the spell isn't in any loadout source — a data
     *  inconsistency the AI shouldn't normally hit. Mirrors the player path's
     *  UCombatCommandMenuSubsystem::MapCategoryToSpellSource semantically:
     *  Refractions → Innate, ResonateRing → RingCrystal, ResonateWeapon →
     *  WeaponCrystal, Breakthrough → Evolution. */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    ESpellSource ResolveSpellSource(USpellData *Spell) const;

    /** Resolve which catalyst HOLDER (URingData* or UWeaponData*) grants a spell on
     *  the active loadout. Walks the same precedence as ResolveSpellSource —
     *  Innate (returns nullptr) → PrimaryRing → RingLoadout → PrimaryWeapon →
     *  SecondaryWeapon — so the two always agree on which source wins. Returns
     *  nullptr for Innate/BD-pool, Evolution, Item, and unmapped spells: those
     *  have no ring/weapon catalyst. Pair with FindAttachedItemByHolder to reach
     *  the slotted crystal (e.g. channel-tier resolution). */
    UObject *FindSpellCatalystHolder(USpellData *Spell) const;

    /** Get spells from primary slot only (Ring or evolved Weapon) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<USpellData *> GetPrimarySlotSpells() const;

    /** Get spells from secondary slot only (Generic with secondary weapon's crystal) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<USpellData *> GetSecondarySlotSpells() const;

    /** Get spells based on bShowPrimary state */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<USpellData *> GetActiveSlotSpells() const;

    /** Get usable item slots (C++ only - use GetUsableItemCount for Blueprint) */
    TArray<FItemLoadoutSlot> GetUsableItems() const;

    UFUNCTION(BlueprintPure, Category = "Loadout|Cosmetics")
    UAnimMontage *GetItemUseAnimation(bool bIsSelfTarget) const;

    /** Get count of usable item slots (Blueprint friendly) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    int32 GetUsableItemCount() const;

    // ==================== COMBAT ACCESSORS ====================

    /** Get active weapon based on current loadout and bShowPrimary state */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UWeaponData *GetActiveWeapon() const;

    /** Get primary weapon (nullptr if using ring/evolution) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UWeaponData *GetPrimaryWeapon() const;

    /** Get secondary weapon (Generic only) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UWeaponData *GetSecondaryWeapon() const;

    /** Get primary ring (Generic/Caster with ring in primary slot) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    URingData *GetPrimaryRing() const;

    /** Get active ring for Resonator (from ring loadout) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    URingData *GetActiveRing() const;

    /** Get primary evolution crystal */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UEvolutionItemData *GetPrimaryEvolution() const;

    /** Enumerate every crystal-bearing slot currently equipped on this actor.
     *  Iterates: Primary weapon, Secondary weapon (Generic only), Primary ring
     *  (Generic/Caster), every ring in RingLoadout (Resonator), and the primary
     *  evolution crystal slot (if any).
     *
     *  Empty slots are skipped — only slots with a crystal attached
     *  (non-empty AttachedItem) appear in the result.
     *
     *  Used by UCrystalManager for combat-init enumeration and by any code
     *  that needs to walk all equipped crystals (e.g. between-combat repair). */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Crystals")
    TArray<FEquippedCrystalSlot> GetEquippedCrystals() const;

    /** True if any crystal equipped on Actor channels the given element.
     *  Walks every slot from GetEquippedCrystals() — weapon/ring crystals plus
     *  the primary evolution slot. Used as a casting unlock channel by
     *  UBrokenDarknessManager::IsElementCastable: a Caster can cast a spell
     *  whose element matches an equipped crystal, regardless of innate element.
     *  Returns false (no unlock) when Actor or its ULoadoutComponent is null. */
    static bool HasEquippedSourceForElement(AActor *Actor, ESpellElement Element);

    /** Get primary slot type */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    EPrimarySlotType GetPrimarySlotType() const;

    /** Get secondary slot type */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    ESecondarySlotType GetSecondarySlotType() const;

    /** Is currently using primary equipment? */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool IsShowingPrimary() const;

    /** Switch between primary/secondary (Generic) or armed/unarmed (Caster/Resonator) */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Combat")
    void ToggleEquipment();

    /** Check if character has weapon access in current state */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool HasWeaponAccess() const;

    /** Check if character is currently armed */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool IsArmed() const;

    /** Check if current loadout has evolution */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool IsEvolved() const;

    // ==================== EQUIPMENT STATE HELPERS ====================

    /** Check if character has secondary equipment (Generic only) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool HasSecondaryEquipment() const;

    /** Check if secondary slot has a ring (Generic only) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool HasRingInSecondary() const;

    // ==================== STANCE & ANIMATION ====================

    /** Get current stance based on armed state and active weapon */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UStanceData *GetCurrentStance() const;

    /** Get current idle montage from stance */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    UAnimMontage *GetCurrentIdleMontage() const;

    /** Get current attack data from active weapon. Base type (attack/ability merge step 5a). */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    USkillDataBase *GetCurrentAttack() const;

    // GetCurrentAttackMontage deleted (Stage 12 SC7) — superseded by
    // SkillMontage on the attack data; zero C++ callers, BP scan clean.

    // ==================== SPELL ACCESS ====================

    /** Get spells from evolved weapon crystal */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<USpellData *> GetWeaponResonateSpells() const;

    /** Get spells from ring (Generic secondary or Caster primary) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    TArray<USpellData *> GetRingResonateSpells() const;

    // ==================== DEFENSE  ====================

    /** Check if should use weapon's parry animation */
    UFUNCTION(BlueprintPure, Category = "Loadout|Defense")
    bool ShouldUseWeaponParry() const;

    // ==================== DEFENSE & COSMETICS (from CharacterData) ====================

    /** Get dodge left animation */
    UFUNCTION(BlueprintPure, Category = "Loadout|Defense")
    UAnimMontage *GetDodgeLeftMontage() const;

    /** Get dodge right animation */
    UFUNCTION(BlueprintPure, Category = "Loadout|Defense")
    UAnimMontage *GetDodgeRightMontage() const;

    /** Get block animation */
    UFUNCTION(BlueprintPure, Category = "Loadout|Defense")
    UAnimMontage *GetBlockMontage() const;

    /** Get parry animation (checks weapon override if loadout prefers it) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Defense")
    UAnimMontage *GetParryMontage() const;

    /** Get unarmed stance */
    UFUNCTION(BlueprintPure, Category = "Loadout|Cosmetics")
    UStanceData *GetUnarmedStance() const;

    /** Get infusion display */
    UFUNCTION(BlueprintPure, Category = "Loadout|Cosmetics")
    UInfusionDisplayData *GetInfusionDisplay() const;

    /** Get ring switch animation (Resonator only) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Cosmetics")
    UAnimMontage *GetRingSwitchMontage() const;

    // ==================== LOADOUT ENTRY ACCESSORS ====================

    /** Get active weapon loadout entry (full access to crystal, abilities, spells) */
    const FWeaponLoadoutEntry *GetActiveWeaponLoadout() const;

    /** The active weapon's attached item, or nullptr if there is no active weapon.
     *  Consolidates the GetActiveWeaponLoadout()->WeaponEntry.GetAttachedItem() idiom.
     *  The returned pointer is into the live loadout entry (valid while the loadout is
     *  unchanged). Null exactly when GetActiveWeaponLoadout() is null — callers null-check
     *  it the same way they guarded the active-weapon lookup. */
    const FRuntimeAttachedItem *GetActiveWeaponAttachment() const;

    /** Get primary weapon loadout entry */
    const FWeaponLoadoutEntry *GetPrimaryWeaponLoadout() const;

    /** Get secondary weapon loadout entry (Generic only) */
    const FWeaponLoadoutEntry *GetSecondaryWeaponLoadout() const;

    /** Get primary ring loadout entry (Generic/Caster with ring in primary slot) */
    const FRingLoadoutEntry *GetPrimaryRingLoadout() const;

    /** Get active ring loadout entry (Resonator) */
    const FRingLoadoutEntry *GetActiveRingLoadout() const;

    // ==================== EQUIPMENT STAT QUERIES ====================

    /** Combined FEquipmentStatBonus for the actor's currently active equipment.
     *  Per-class resolution:
     *   - Generic dual weapon: active weapon StatBonus (which is which is gated by bShowPrimary)
     *   - Generic ring + weapon (primary ring + secondary weapon): both StatBonuses summed
     *   - Generic weapon-only / ring-only: that one slot only
     *   - Caster: primary-slot StatBonus only (weapon or ring)
     *   - Resonator: active-ring StatBonus + primary-weapon StatBonus (if a weapon is equipped)
     *  Returns a zero-initialized FEquipmentStatBonus if no equipment matches.
     *  Actor parameter is for caller clarity (always == GetOwner()). */
    UFUNCTION(BlueprintPure, Category = "Loadout|Stats")
    FEquipmentStatBonus GetActiveStatBonus(AActor *Actor) const;

    /** Aggregated per-instance status-buildup resistance across the actor's active
     *  equipment — same per-class resolution as GetActiveStatBonus, summing the
     *  ResistanceBonus of each contributing weapon/ring entry. Read at buildup by
     *  UStatusBuildupManager::GetTotalStatusResistance (source #6). Returns an
     *  all-zero FResistanceBonus when no equipment carries resistance.
     *  NOTE: evolution-crystal resistance is added in C-3 (via GetActivePrimaryEvolutionCrystal). */
    UFUNCTION(BlueprintPure, Category = "Loadout|Stats")
    FResistanceBonus GetActiveResistanceBonus(AActor *Actor) const;

    /** STARTING effects (non-conditional) from the actor's currently active gear,
     *  applied once at combat start. Coverage mirrors GetActiveStatBonus:
     *   - Generic   → active weapon; ring-primary adds primary ring + secondary weapon
     *   - Caster    → primary-slot gear (weapon OR ring, not both)
     *   - Resonator → active ring + primary weapon
     *   - Innate (primary-slot) evolution included; ATTACHED evolutions contribute
     *     NO effects (deliberate exception).
     *  Actor parameter is for caller clarity (always == GetOwner()). */
    UFUNCTION(BlueprintPure, Category = "Loadout|Effects")
    TArray<FSkillEffect> GetActiveEffects(AActor *Actor) const;

    /** Evolution crystal slotted in the PRIMARY WEAPON slot only.
     *  Returns nullptr for: secondary weapon crystal, ring crystal, non-evolution
     *  crystal in primary weapon, no weapon equipped, or non-Weapon primary slot. */
    UFUNCTION(BlueprintPure, Category = "Loadout|Stats")
    UEvolutionItemData *GetActivePrimaryEvolutionCrystal(AActor *Actor) const;

    /** The full FEvolutionAttachment behind GetActivePrimaryEvolutionCrystal —
     *  same weapon-slot resolution, but returns the ATTACHMENT (by value), which
     *  carries the per-instance rolled state (GeneratedStatBonus/Resistance +
     *  pools) that the bare asset pointer cannot. Item == nullptr means none.
     *  U3c read sites combine asset Base + this attachment's Generated. */
    UFUNCTION(BlueprintPure, Category = "Loadout|Stats")
    FEvolutionAttachment GetActivePrimaryEvolutionAttachment(AActor *Actor) const;

    /** Returns the FRuntimeAttachedItem for the given holder by value.
     *  Searches primary/secondary weapon entries, primary ring entry, and
     *  the Resonator RingLoadout array. Returns a default-constructed
     *  (empty) attachment if no match is found. */
    FRuntimeAttachedItem GetCrystalEntryByHolder(UObject *Holder) const;

    /** Returns a mutable pointer into the active loadout's attached item for
     *  the given holder. LIFETIME: valid only within the calling scope —
     *  array mutations on SavedLoadouts or any inner RingLoadout invalidate
     *  the pointer. Used by ProcessPostCastWear callers that need to mutate
     *  the live attachment, and by the public Reset/Repair/ApplyWear wrappers
     *  which perform fetch-and-use within a single scope. */
    FRuntimeAttachedItem *FindAttachedItemByHolder(UObject *Holder);

    /** Resets the attachment to its default (empty) state. Used by end-of-combat
     *  crystal destruction. No-op if no matching entry exists. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Crystal")
    void ResetCrystalEntryByHolder(UObject *Holder);

    /** Repairs the attachment by Amount, clamped to MaxDurability.
     *  Returns the new CurrentDurability after the repair. Returns -1 if
     *  no matching entry exists. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Crystal")
    int32 RepairCrystalEntryByHolder(UObject *Holder, int32 Amount);

    /** Applies wear to the attachment by Amount. Returns true if the
     *  wear caused the attachment to break. Returns false if no matching entry
     *  exists or if it didn't break. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Crystal")
    bool ApplyWearToCrystalEntryByHolder(UObject *Holder, int32 Amount);

    /** Applies wear to the active loadout's standalone primary-slot evolution
     *  (PrimarySlotType==Evolution, e.g. Broken Darkness). Writes the LIVE
     *  storage in Inv->SavedLoadouts[ActiveLoadoutIndex] — not a copy.
     *  Returns true if the wear took the crystal to 0 (broke). Returns false
     *  if there is no active evolution slot, no item, or the wear did not
     *  break it.
     *
     *  bForceWear (default false): forwarded to FEvolutionAttachment::ApplyWear.
     *  When false, per-item bCanBreak still gates the wear (no-op if false).
     *  When true, the bCanBreak gate is bypassed — intended for intrinsic
     *  mechanics like Broken Darkness. Default preserves existing behavior. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Crystal")
    bool ApplyWearToActivePrimaryEvolution(int32 Amount, bool bForceWear = false);

    /** Clears the active loadout's primary evolution slot if its attachment
     *  is broken (durability <= 0 and bCanBreak). PrimarySlotType is left
     *  unchanged (stays Evolution — the slot just empties). Returns true if
     *  a clear occurred. Intended for between-combat destruction sweeps. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Crystal")
    bool ClearBrokenPrimaryEvolution();

    // ==================== POST-BATTLE ====================
    // Crystal-refactor 9.5b: ConsumeUsedItems and GetItemsToConsume removed.
    // Item slot Quantity is now the persistent post-combat state; inventory
    // is debited at equip/auto-equip time, not at battle end.

    /** Reset battle state (call after battle ends) */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Battle")
    void ResetBattleState();

    // ==================== QUICK SETUP ====================

    /** Initialize loadout from CharacterData and Inventory */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Setup")
    void InitializeFromCharacterData(UCharacterData *CharacterData, UInventoryComponent *Inventory);

    /** Auto-populate loadout from inventory (best available) */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Setup")
    bool AutoPopulateLoadout(int32 LoadoutIndex, UInventoryComponent *Inventory);

    /** Clear all equipment from loadout */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Setup")
    void ClearLoadout(int32 LoadoutIndex);

    // ==================== EVENTS ====================

    /** Fired when active loadout changes (use GetActiveLoadout() to get data) */
    UPROPERTY(BlueprintAssignable, Category = "Loadout|Events")
    FOnLoadoutChanged OnLoadoutChanged;

    /** Fired when item is used in combat */
    UPROPERTY(BlueprintAssignable, Category = "Loadout|Events")
    FOnLoadoutItemUsed OnItemUsed;

    /** Fired when loadout validation fails */
    UPROPERTY(BlueprintAssignable, Category = "Loadout|Events")
    FOnLoadoutValidationFailed OnValidationFailed;

    /** Debug: Log current loadout state */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugLogLoadout();

    /** Debug: print the active loadout's validation findings, run
     *  ClearInvalidSlots, then re-print to show the cleared result.
     *  CallInEditor button in the Details panel. Counts only clearable
     *  findings post-clear (expect 0; non-clearable BD/guard findings may
     *  legitimately remain). */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Debug")
    void DebugClearAndReportValidation();

private:
    /** Helper to get CharacterData from sibling component */
    UCharacterData *GetOwnerCharacterData() const;

    /** Helper to get the sibling UInventoryComponent. Re-fetched per call (no
     *  cache) — loading screens hide the FindComponentByClass cost. Returns
     *  nullptr if the owner has no inventory component; callers must guard. */
    UInventoryComponent *GetInventoryComponent() const;

    // FindAttachedItemByHolder is the unified mutable-lookup helper, now public above.

    /** Build the Broken Darkness element-spell pools on a loadout — one empty
     *  pool per absorbable non-Darkness element (7 total). Idempotent: existing
     *  pools are kept, only missing ones are added. */
    static void InitializeBDPools(FCombatLoadout &Loadout);

    /** If the owning character is Broken Darkness, ensure its active loadout
     *  has the BD element-spell pools initialised. */
    void ApplyBDPoolsIfBroken();

protected:
    virtual void BeginPlay() override;

    /** Is loadout ready for battle? */
    UPROPERTY(BlueprintReadOnly, Category = "Loadout|Runtime")
    bool bIsReadyForBattle = false;

    /** Ensure at least one loadout exists */
    void EnsureDefaultLoadout();
};