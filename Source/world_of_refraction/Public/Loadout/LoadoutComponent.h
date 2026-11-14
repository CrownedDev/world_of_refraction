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
#include "Loadout/LoadoutRules.h"
#include "Loadout/Entries/FItemLoadoutSlot.h"
#include "Equipment/FEquippedItemSlot.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Equipment/EAttachedItemKind.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Skills/Definitions/ESpellSource.h"
#include "Loadout/CarriedGearState.h"
#include "Equipment/Crystals/EvolutionPlacement.h"
#include "Inventory/EInventoryChangeType.h"
#include "Combat/Defense/EGuardStyle.h"
#include "Combat/Defense/EParryElementPriority.h"
#include "LoadoutComponent.generated.h"

struct FBrokenCrystalPayload;

class UInventoryComponent;
class UCharacterDataComponent;
enum class ESubStat : uint8;
class USpellData;
class UAbilityData;
class UEvolutionItemData;
class URingData;
class UEvolutionItemData;
struct FWeaponLoadoutEntry;
struct FPartyMember;
struct FRingLoadoutEntry;
struct FRuntimeAttachedItem;
class UStanceData;
class USkillDataBase;
class UAnimMontage;
class USpellData;
class UInfusionDisplayData;
class UWeaponData;

/** Delegate for loadout changes (query GetActiveLoadout() for data) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadoutChanged, int32, NewLoadoutIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLoadoutItemUsed, int32, SlotIndex, FCrystalId, ItemId, int32, NewQuantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadoutValidationFailed, const FString &, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoadoutPartsDropped, int32, LoadoutIndex, const TArray<FText> &, Messages);
/** The weapon GetActiveWeapon() returns has changed (null = no weapon now). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveWeaponChanged, UWeaponData *, NewWeapon);

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

    /** B-310 (D-111): the owned item the attachment sits on, so two identical holders stay apart; invalid for
     *  asset-built gear and the evolution seat (then Holder alone). */
    UPROPERTY(BlueprintReadOnly, Category = "Crystal")
    FGuid GearID;
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

    /** The owner's CharacterData class (determines loadout rules); Generic when there is no CharacterData. */
    UFUNCTION(BlueprintPure, Category = "Loadout")
    ECharacterClass GetCharacterClass() const;

    // ==================== ACTIVE LOADOUT ====================

    /** The currently active saved loadout, or an empty one when there is none (C++ only -
     *  use specific getters for Blueprint). The reference is into the inventory's
     *  SavedLoadouts: don't hold it across anything that adds or removes saved loadouts. */
    const FCombatLoadout &GetActiveLoadout() const;

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

    /** Element of the active ring's attached crystal (Resonator); None without one. */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    ESpellElement GetActiveRingElement() const;

    /** Make the next ring (wrapping) whose attachment is present and unbroken the active
     *  one. False when no other ring is usable. Also runs when a ring's crystal breaks.
     *  Forwards to SwitchToNextUsableSlot(Adapter). */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Combat")
    bool SwitchToNextUsableRing();

    /** A crystal on the owner broke. On a ring, switches to the next usable ring (moved from
     *  URingManager, B-024). Called by UCrystalManager just before it broadcasts
     *  OnCrystalBroken, so listeners (the durability header) already see the new ring. */
    void HandleCrystalBroken(UObject *Holder, const FBrokenCrystalPayload &Payload);

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

    /** Same, for any saved loadout index, checking ownership against OwnershipSource (null =
     *  the owner's inventory). B-028: GetValidationErrors is its reasons. */
    TArray<FInvalidSlotFinding> CollectInvalidSlotFindings(int32 Index, const UInventoryComponent *OwnershipSource = nullptr) const;

    /** What the shared loadout rules can check for this character (class, BD, discount,
     *  ownership, castability). Inventory = the ownership source. The checks capture raw
     *  component pointers: use the context at once, don't store it. */
    FLoadoutRuleContext BuildRuleContext(const UInventoryComponent *Inventory) const;

    /** Clears every clearable finding from CollectInvalidSlotFindings on the
     *  active loadout (DropInvalidParts on the active index), announcing each
     *  drop through OnPartsDropped. */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    void ClearInvalidSlots();

    /** Drops every clearable finding from the saved loadout at Index (D-081: parts a change
     *  broke are dropped). Mutates Inv->SavedLoadouts[Index] in place (the live runtime
     *  FCombatLoadout). Single-pass: validate once, clear once; a clear that exposes a new
     *  invalidity persists until the next validation. Returns what was dropped. With
     *  OnlySlotTypes, only findings of those types are dropped (B-171: what a gear change breaks). */
    TArray<FInvalidSlotFinding> DropInvalidParts(int32 Index, const TArray<ELoadoutSlotType> &OnlySlotTypes = TArray<ELoadoutSlotType>());

    /** DropInvalidParts on every saved loadout, announcing each drop (B-159: run after a load). */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    void DropInvalidPartsFromAllLoadouts();

    // ==================== PICKS (D-081: an invalid pick is refused) ====================

    /** Each adds one part to the saved loadout at LoadoutIndex. Refused, with the reason in
     *  OutRefusal, if the loadout would break a rule it didn't break before. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Picks")
    bool TryAssignWeaponAbility(int32 LoadoutIndex, UAbilityData *Ability, FText &OutRefusal);

    UFUNCTION(BlueprintCallable, Category = "Loadout|Picks")
    bool TryAssignWeaponSpell(int32 LoadoutIndex, USpellData *Spell, FText &OutRefusal);

    /** The Generic/Caster primary ring. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Picks")
    bool TryAssignRingSpell(int32 LoadoutIndex, USpellData *Spell, FText &OutRefusal);

    UFUNCTION(BlueprintCallable, Category = "Loadout|Picks")
    bool TryAssignInnateSpell(int32 LoadoutIndex, USpellData *Spell, FText &OutRefusal);

    /** B-440b (the loadout screen): changes the gear of the saved loadout at LoadoutIndex (a weapon or adapter in or
     *  out, the weapon in hand), refused as a pick is; then what the active gear gives is re-read (ToggleEquipment).
     *  Refused during a fight. */
    bool TryChangeGear(int32 LoadoutIndex, TFunctionRef<void(FCombatLoadout &)> Change, FText &OutRefusal);

    // ==================== BATTLE PREPARATION ====================

    /** Prepare loadout for battle (reset item uses, validate) */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Battle")
    bool PrepareForBattle(UInventoryComponent *Inventory);

    /** Check if loadout is ready for battle */
    UFUNCTION(BlueprintPure, Category = "Loadout|Battle")
    bool IsReadyForBattle() const { return bIsReadyForBattle; }

    // ==================== RUNTIME COMBAT STATE ====================

    /** Use an item from loadout (decrements uses).
     *  OBSOLETE (B-313) for a member with a pouch: its fight items are the pouch (UseCarriedItem). Kept for actors
     *  with no pouch (enemies, unbound test actors); delete with FCombatLoadout::ItemSlots after Crown's PIE pass. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Combat")
    bool UseItem(int32 SlotIndex);

    // ==================== THE MEMBER'S POUCH IN A FIGHT (B-313, D-111, D-117, Q-313) ====================
    // A party member's fight items are its own pouch (UPoolSubsystem::GetPouch), nothing else: GetUsableItems
    // lists it and UseCarriedItem spends from it. No tier cap on what it uses (B-354, D-143 retired Q-313's per-fight
    // bands). Stones are slot-only, so they never show. Actors with no pouch keep the loadout's item slots.

    /** True when the owner carries a pouch (its crystal inventory is bound to the pool). */
    bool UsesPouchItems() const;

    /** Id is carried in the member's pouch and is a gem. */
    bool CanUseCarriedItem(const FCrystalId &Id) const;

    /** Spends one Id from the member's pouch (storage, other pouches and the loadout's item slots are
     *  untouched). False when not ready for battle or
     *  CanUseCarriedItem refuses. OnItemUsed fires with SlotIndex INDEX_NONE and the pouch's count left. */
    bool UseCarriedItem(const FCrystalId &Id);

    /** Get remaining uses for item slot */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    int32 GetItemRemainingUses(int32 SlotIndex) const;

    /** Get attacks from ALL equipped weapons (for AI evaluation). Base type (attack/ability merge) —
     *  each is a UAbilityData with bIsAttack=true. */
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

    /** Get usable item slots (C++ only - use GetUsableItemCount for Blueprint).
     *  B-313: for a member with a pouch, the pouch's gems (one entry per stack, Quantity = what this fight can
     *  still use: the whole stack, B-354), sorted by type then highest tier. Otherwise the
     *  active loadout's non-empty item slots (OBSOLETE (B-313) path, kept for enemies). */
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

    /** B-081 (D-008): after a mid-fight weapon toggle or ring switch, the gear effects follow
     *  (old gear's off, new gear's on, conditionals re-armed). No-op out of a fight. */
    void SwapGearEffects(const TArray<FGatheredEffect> &OldGearEffects) const;

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

    /** Check if character has secondary equipment (Generic only; kept with an evolution primary, D-027) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Combat")
    bool HasSecondaryEquipment() const;

    /** Always false: the secondary slot holds a weapon only (ESecondarySlotType) */
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

    /** OBSOLETE (B-202, Q-182 changed by Crown): every Guard plays GetBlockMontage; no caller.
     *  Remove after PIE. Get parry animation (checks weapon override if loadout prefers it) */
    UFUNCTION(BlueprintPure, Category = "Loadout|Defense")
    UAnimMontage *GetParryMontage() const;

    // ==================== GUARD (B-392, D-170) ====================

    /** B-392 (D-170, Q-392): the guard this character parries with now. No weapon in hand: Unarmed. A weapon in hand:
     *  the chosen guard, except that Unarmed holds only for the weapon it was chosen with (equipping another weapon
     *  brings the weapon guard back). Looks only: the timing window never reads it. */
    UFUNCTION(BlueprintPure, Category = "Loadout|Defense")
    EGuardStyle GetGuardStyle() const;

    /** B-392: choose the guard. Unarmed puts the weapon now in hand away for parrying; refused (false) with no weapon in
     *  hand, which is unarmed already. A rostered member (a pawn stamped with its PartyMemberId) keeps the choice on its
     *  party record, which the save writes; any other pawn keeps it here for its life. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Defense")
    bool SetGuardStyle(EGuardStyle Style);

    /** B-392: the one guard-montage resolver. The weapon guard is the active weapon's ParryMontage (its type's guard, not
     *  customisable); the unarmed guard is the character's Cosmetics BlockMontage (customisable per character). A weapon
     *  with no guard montage authored yet falls back to the unarmed one, so nothing goes still. */
    UFUNCTION(BlueprintPure, Category = "Loadout|Defense")
    UAnimMontage *GetGuardMontage() const;

    /** B-392: the rule GetGuardStyle applies, pure for the spec. ActiveWeaponId is the in-hand weapon's PersistentID. */
    static EGuardStyle ResolveGuardStyle(bool bWeaponInHand, const FGuid &ActiveWeaponId, EGuardStyle Chosen,
                                         const FGuid &ChosenWithWeaponId);

    /** B-393 (D-170, Q-393): which element colours this character's landed-parry effect when the weapon guard has two
     *  sources (the weapon's crystal, or its own magic). A rostered member's lives on its party record (saved); any
     *  other pawn keeps its own, default Weapon. Looks only. Resolved by ParryVisuals. */
    UFUNCTION(BlueprintPure, Category = "Loadout|Defense")
    EParryElementPriority GetParryElementPriority() const;

    /** B-393: choose the parry colour priority (the loadout screen's setting, D-024; wor.Guard.SetParryPriority until
     *  then). */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Defense")
    void SetParryElementPriority(EParryElementPriority Priority);

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

    /** B-082 (D-009): the ring whose fusion's stone half counts: the Resonator's active ring,
     *  else a primary ring (the rings gear effects read). Null when there is none. */
    const FRuntimeAttachedItem *GetStoneRingAttachment() const;

    /** B-351 (D-141): the stone socket of the weapon in use / of the adapter the stone getters read; null when none. */
    const FRuntimeAttachedItem *GetActiveWeaponStoneSocket() const;
    const FRuntimeAttachedItem *GetStoneRingStoneSocket() const;

    /** Every stat-stone read: the active weapon's attachment plus the counting ring's
     *  (CrystalEffectTable::GetAttachedStonePercent on each, added). 0 when neither has one. */
    float GetAttachedStonePercent(ESubStat Stat) const;

    /** The pool-stone mirror (MaxHPStone / MaxEPStone), same sources. */
    float GetAttachedStonePercentForType(ECrystalType StoneType) const;

    /** Get primary weapon loadout entry */
    const FWeaponLoadoutEntry *GetPrimaryWeaponLoadout() const;

    /** Get secondary weapon loadout entry (Generic only) */
    const FWeaponLoadoutEntry *GetSecondaryWeaponLoadout() const;

    /** Get primary ring loadout entry (Generic/Caster with ring in primary slot) */
    const FRingLoadoutEntry *GetPrimaryRingLoadout() const;

    /** Get active ring loadout entry (Resonator) */
    const FRingLoadoutEntry *GetActiveRingLoadout() const;

    /** B-304 (D-110): the slot in use for a kind: the active one, else the first usable slot of that kind (a Generic's
     *  two weapon slots are equal; no primary rule). A piece another member wears is skipped (B-221b). The old
     *  active-weapon / adapter getters forward here. The pointer is valid until the loadout next changes.
     *  Const but not read-only: it brings the active loadout's slot mirror in step first (a cache), so call it on
     *  the game thread only (the anim instance reads it from NativeUpdateAnimation, not the thread-safe update). */
    const FLoadoutSlot *GetActiveSlot(EGearKind Kind) const;

    /** B-306 (Q-244, D-110): the gear that counts, for every class and every reader (stats, resistances, effects):
     *  the adapter in use, then the weapon in hand. Visit gets the live entry behind each (one of the two is null).
     *  The primary evolution seat is not gear; callers add it. */
    void ForEachContributingGear(TFunctionRef<void(const FWeaponLoadoutEntry *Weapon, const FRingLoadoutEntry *Adapter)> Visit) const;

    /** B-477 (D-209): Weapon is a Generic's allowed pair (WeaponSetRules::CheckPair): its off hand's stats, resistances
     *  and the pair's dual-wield stance count. */
    bool IsWeaponSetPaired(const FWeaponLoadoutEntry &Weapon) const;

    /** D-083: why Spell can't be cast right now because the active weapon's or ring's crystal
     *  is broken (a broken ring still casts F-tier spells). Empty when it can. */
    FText GetBrokenSourceReason(const USpellData *Spell) const;

    /** Spell comes from the active (or primary) ring and that ring's crystal is broken, and it is
     *  not also an innate spell. Such a spell casts plain: no infusion, no element (Q-163b = B). */
    bool IsSpellFromBrokenRing(const USpellData *Spell) const;

    // ==================== TURN SWITCH (ET-19) ====================

    /** ET-19 (Crown, 2026-10-26): one free weapon or ring switch per turn, so swapping can't
     *  re-trigger "when equipped" effects over and over. Why a switch is refused now; empty
     *  when one is allowed. */
    FText GetTurnSwitchRefusedReason() const;

    /** B-305 (D-110): the one switch. The turn's free switch of the active Kind slot: a weapon goes to the other
     *  weapon, an adapter to the next adapter (wrapping). False, with the reason, once the turn's switch is used;
     *  false with no reason when nothing changes (that doesn't use it). */
    bool TrySwitchActiveSlot(EGearKind Kind, FText &OutReason);

    /** B-305: makes the next usable Kind slot active (wrapping). An adapter is usable with its attachment present
     *  and unbroken. Weapons don't switch on their own: false. */
    bool SwitchToNextUsableSlot(EGearKind Kind);

    /** The turn's free weapon switch. Forwards to TrySwitchActiveSlot(Weapon). */
    bool TrySwitchWeaponThisTurn(FText &OutReason);

    /** The turn's free ring switch to NewRingIndex (the same rules as TrySwitchActiveSlot). */
    bool TrySwitchRingThisTurn(int32 NewRingIndex, FText &OutReason);

    /** Gives back the free switch; the orchestrator calls it at the owner's turn start. */
    void ResetTurnSwitch();

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
     *  The character slot's evolution adds its resistance through GatherAttachedEvolutions (B-357c). */
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

    /** Def-identity carrier (R1): same per-class gear aggregation as GetActiveEffects(),
     *  but each effect carries its source def's id + bundle index for R2's per-definition
     *  ID packing. Additive — nothing reads this yet. */
    TArray<FGatheredEffect> GetActiveEffectsGathered(AActor *Actor) const;

    /** Same per-class gear aggregation, but the CONDITIONAL subset (each item's
     *  GetConditionalEffectsGathered). Armed at combat start; fired on impact by the
     *  defense handler (C3). Carries def id + bundle index per effect. */
    TArray<FGatheredEffect> GetActiveConditionalEffectsGathered(AActor *Actor) const;

    /** OBSOLETE (B-357e): no C++ reader left; read GatherAttachedEvolutions (Place Weapon) instead. Kept for any
     *  Blueprint caller until Crown's PIE.
     *  Evolution crystal slotted in the PRIMARY WEAPON slot only.
     *  Returns nullptr for: secondary weapon crystal, ring crystal, non-evolution
     *  crystal in primary weapon, no weapon equipped, or non-Weapon primary slot. */
    UFUNCTION(BlueprintPure, Category = "Loadout|Stats")
    UEvolutionItemData *GetActivePrimaryEvolutionCrystal(AActor *Actor) const;

    /** OBSOLETE (B-357e): as GetActivePrimaryEvolutionCrystal; read GatherAttachedEvolutions instead.
     *  The full FEvolutionAttachment behind GetActivePrimaryEvolutionCrystal —
     *  same weapon-slot resolution, but returns the ATTACHMENT (by value), which
     *  carries the per-instance rolled state (GeneratedStatBonus/Resistance +
     *  pools) that the bare asset pointer cannot. Item == nullptr means none.
     *  U3c read sites combine asset Base + this attachment's Generated. */
    UFUNCTION(BlueprintPure, Category = "Loadout|Stats")
    FEvolutionAttachment GetActivePrimaryEvolutionAttachment(AActor *Actor) const;

    /** B-357b (D-147): the one resolve path: every evolution attached to this character, with its place: the
     *  character's own slot (PrimarySlotType == Evolution), then the gear that counts (ForEachContributingGear: the
     *  adapter in use, the weapon in hand), each with an evolution in its socket, then each apparel piece's (B-357d).
     *  WoREvolutionPlacement says what each place gives. */
    TArray<FAttachedEvolution> GatherAttachedEvolutions() const;

    /** B-357e: the evolution on the character's own slot, read through GatherAttachedEvolutions (Place Character).
     *  Item == nullptr when the primary slot holds no evolution. The slot's writes (equip, wear, repair, save,
     *  inventory sync) still go to FCombatLoadout::PrimaryEvolution. */
    FEvolutionAttachment GetCharacterSlotEvolution() const;

    /** B-357d (D-147, Q-357b A): the lasting passive effects one action gets from the gear evolutions that power it
     *  (WoREvolutionPlacement::FilterForAction: charged to L2, the weapon / adapter / apparel holding it). An evolution
     *  also on the character's own slot is left out: its passives already act all fight. Empty below L2. */
    TArray<FGatheredEffect> GetGearEvolutionPassivesForAction(EActionType ActionType, EInfusionSourceOption Source, int32 ChargeLevel) const;

    /** Debug (B-357b): one line per attached evolution: its place and whether its passives act at charge 0 and 2. */
    FString GetAttachedEvolutionsString() const;

    /** InstanceID of the owned FEvolutionInventoryEntry the active PRIMARY-SLOT evolution
     *  (PrimarySlotType == Evolution) was inflated from (iii-b — retained on FCombatLoadout).
     *  Invalid FGuid when the primary slot is not an instance-resolved evolution. Removal/break
     *  paths read this to resolve the owned entry to dismantle. */
    UFUNCTION(BlueprintPure, Category = "Loadout|Stats")
    FGuid GetActivePrimaryEvolutionInstance() const;

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

    /** B-310: the attachment a GetEquippedCrystals slot names: matched by its item's GUID when it has one (so two
     *  identical adapters stay apart), else by holder. Crystal break / destroy / repair use these. */
    FRuntimeAttachedItem *FindAttachedItemForSlot(const FEquippedCrystalSlot &Slot);
    FRuntimeAttachedItem GetCrystalEntryForSlot(const FEquippedCrystalSlot &Slot) const;
    void ResetCrystalEntryForSlot(const FEquippedCrystalSlot &Slot);
    int32 RepairCrystalEntryForSlot(const FEquippedCrystalSlot &Slot, int32 Amount);

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
     *  When false, per-item Breakability still gates the wear (no-op if not breakable).
     *  When true, the Breakability gate is bypassed — intended for intrinsic
     *  mechanics like Broken Darkness. Default preserves existing behavior. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Crystal")
    bool ApplyWearToActivePrimaryEvolution(int32 Amount, bool bForceWear = false);

    /** Between-combat repair of the active loadout's standalone primary-slot evolution, broken
     *  included (Q-159c), clamped to its MaxDurability. Returns the durability restored; 0 when
     *  there is no evolution primary slot or it is already full. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Crystal")
    int32 RepairActivePrimaryEvolution(int32 Amount);

    /** Clears the active loadout's primary evolution slot if its attachment
     *  is broken (durability <= 0 and breakable). PrimarySlotType is left
     *  unchanged (stays Evolution — the slot just empties). Returns true if
     *  a clear occurred. Intended for between-combat destruction sweeps. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Crystal")
    bool ClearBrokenPrimaryEvolution();

    /** B-115: the active loadout's crystals as this fight leaves them: every weapon and ring
     *  attachment (empty ones too, so a destroyed gem stays gone) and the primary evolution. */
    FCarriedGearState SnapshotCarriedGear() const;

    /** B-115: puts carried crystal state back on the freshly built loadout. A holder the loadout
     *  no longer has is skipped. Returns how many attachments were restored. */
    int32 ApplyCarriedGear(const FCarriedGearState &Carried);

    /** Unconditionally empties the active loadout's primary evolution slot (player-initiated
     *  removal, not a break sweep). PrimarySlotType is left unchanged (stays Evolution — the slot
     *  just empties), mirroring ClearBrokenPrimaryEvolution but without the broken gate. Returns
     *  true if the active loadout has an evolution primary slot to clear. */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Crystal")
    bool ClearPrimaryEvolution();

    // ==================== POST-BATTLE ====================
    // Crystal-refactor 9.5b: ConsumeUsedItems and GetItemsToConsume removed.
    // Item slot Quantity is now the persistent post-combat state; inventory
    // is debited at equip/auto-equip time, not at battle end. A member with a
    // pouch fights from the pouch itself (B-313), so nothing is handed back.

    /** Reset battle state (call after battle ends). */
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

    /** Fired when parts of a loadout are dropped, one message per part ("Removed: ..."). */
    UPROPERTY(BlueprintAssignable, Category = "Loadout|Events")
    FOnLoadoutPartsDropped OnPartsDropped;

    /** Fired when the active weapon changes (B-138): loadout switch, weapon toggle, slot edits,
     *  init, carried gear, evolution cleared, and any inventory change (a load, gear removed). */
    UPROPERTY(BlueprintAssignable, Category = "Loadout|Events")
    FOnActiveWeaponChanged OnActiveWeaponChanged;

    /** Broadcasts OnActiveWeaponChanged if GetActiveWeapon() differs from the last broadcast.
     *  Mutators here call it on every return; code that edits SavedLoadouts directly calls it after. */
    void NotifyActiveWeaponChanged();

    /** B-380c: apparel worn in this loadout changed (worn, removed, or its socket set in play): the max pools and every
     *  stat reader (GAS sync, loadout UI: OnLoadoutChanged) re-read it. */
    void NotifyApparelChanged();

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

    /** Helper to get CharacterData from sibling component. Public so debug
     *  tooling (InventoryDebug pool-budget readout) can read the same input;
     *  const getter, no mutation — safe to expose. */
    UCharacterData *GetOwnerCharacterData() const;

private:
    /** ET-19: the free weapon / ring switch was used this turn (per-fight, reset each turn). */
    bool bUsedTurnSwitch = false;

    /** The weapon OnActiveWeaponChanged last announced. */
    UPROPERTY(Transient)
    TObjectPtr<UWeaponData> LastNotifiedWeapon;

    /** B-392: the guard choice of a pawn with no party record (an enemy, a hub or test pawn); a rostered member's lives
     *  on FPartyMember::GuardStyle / GuardWeaponId. */
    EGuardStyle LocalGuardStyle = EGuardStyle::Weapon;
    FGuid LocalGuardWeaponId;

    /** B-393: the parry colour priority of a pawn with no party record (FPartyMember::ParryElementPriority otherwise). */
    EParryElementPriority LocalParryElementPriority = EParryElementPriority::Weapon;

    /** B-392: the party record of a pawn stamped with its member id, else null (the head-start rule, B-348b). */
    const FPartyMember *FindGuardRecord() const;

    /** Inventory edits (loads, gear removed) write SavedLoadouts directly; re-check the weapon. */
    UFUNCTION()
    void HandleInventoryChanged(EInventoryChangeType ChangeType);

    /** B-171 (D-081): an owned weapon's or ring's socket changed; the loadouts holding it drop
     *  the parts the change broke (wrong-element spells, an over-cap list), with a message. */
    UFUNCTION()
    void HandleGearSocketChanged(FGuid GearID);

    /** Helper to get the sibling UInventoryComponent. Found once and cached (B-262: the anim instance and every
     *  stat read call it per frame / per read); found again if the cached one is gone. Returns nullptr if the
     *  owner has no inventory component; callers must guard. */
    UInventoryComponent *GetInventoryComponent() const;

    /** B-304: the active loadout with its slot mirror brought in step with the old fields (null when none). */
    FCombatLoadout *GetActiveLoadoutInStep() const;

    /** B-304 / B-221b: the piece is worn by another member (UPoolSubsystem's wearers). */
    bool IsWornByAnother(const FGuid &GearID) const;

    /** B-304: this character's wearer key (UPoolSubsystem::GetCharacterKey builds it from the asset path), kept per
     *  character data so the active-gear getters don't rebuild it on every read. */
    mutable TWeakObjectPtr<const UCharacterData> WearerKeyData;
    mutable FName WearerKey;

    /** B-304: the old ring entry behind the active adapter slot. */
    const FRingLoadoutEntry *GetActiveAdapterLegacyEntry() const;

    /** B-305: TrySwitchActiveSlot's body; AdapterIndex is the adapter to switch to (ignored for a weapon). */
    bool TrySwitchSlotTo(EGearKind Kind, int32 AdapterIndex, FText &OutReason);

    /** B-304: GetActiveSlot on a loadout already brought in step (one walk for the forwards). */
    const FLoadoutSlot *GetActiveSlotIn(const FCombatLoadout &Loadout, EGearKind Kind) const;
    /** B-262: the sibling components, found once (GetInventoryComponent / GetOwnerCharacterData). Weak, so a
     *  removed component is looked up again rather than read stale. */
    mutable TWeakObjectPtr<UInventoryComponent> CachedInventory;
    mutable TWeakObjectPtr<UCharacterDataComponent> CachedCharacterDataComponent;

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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Is loadout ready for battle? */
    UPROPERTY(BlueprintReadOnly, Category = "Loadout|Runtime")
    bool bIsReadyForBattle = false;

    /** OBSOLETE (B-354, D-143): B-313's pouch uses this fight per D-072 tier band. No longer counted; still cleared by
     *  PrepareForBattle and ResetBattleState. Delete after Crown's PIE. */
    TMap<int32, int32> BattleBandUses;

    /** OBSOLETE (B-354, D-143): how many of Id this fight still allows from the pouch. No cap for a gem since the bands
     *  retired (MAX_int32); 0 for a stone or None (slot-only). Delete with BattleBandUses after Crown's PIE. */
    int32 GetCarriedBandRoom(const FCrystalId &Id) const;

    /** Ensure at least one loadout exists */
    void EnsureDefaultLoadout();

    /** Shared body of the TryAssign* picks. CheckTarget refuses up front (the part isn't in use,
     *  the pick is already there; empty = fine); otherwise Change is applied to a copy, kept only
     *  if no new rule break appears (each reason counted, so a repeat of one is new too). */
    bool TryChangeLoadout(int32 LoadoutIndex, bool bHasPick,
                          TFunctionRef<FText(const FCombatLoadout &, ECharacterClass)> CheckTarget,
                          TFunctionRef<void(FCombatLoadout &)> Change, FText &OutRefusal);

    void AnnounceDroppedParts(int32 LoadoutIndex, const TArray<FInvalidSlotFinding> &Dropped);

    /** B-091: claims the active loadout's shared gear; announces what another member wears. */
    void ClaimSharedGear();
};