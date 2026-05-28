// Source/world_of_refraction/Public/CrystalManager.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ItemTier.h"
#include "FBrokenCrystalPayload.h"
#include "CrystalManager.generated.h"

class UEvolutionItemData;
class ULoadoutComponent;
struct FRuntimeAttachedItem;

/**
 * UCrystalManager
 *
 * Unified crystal lifecycle and event broker. Owns:
 *  - Post-cast durability wear (replaces UWeaponManager::ProcessPostCastWear
 *    and URingManager::ProcessPostCastWear)
 *  - Unified broadcasts: OnCrystalBroken, OnCrystalDurabilityChanged
 *
 * Per-instance durability state lives on FRuntimeAttachedItem; this
 * manager operates on the attachment provided by the caller directly
 * and broadcasts breaks via the discriminated FBrokenCrystalPayload.
 */
UCLASS()
class WORLD_OF_REFRACTION_API UCrystalManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;
    virtual void Deinitialize() override;

    // ========================================
    // WEAR
    // ========================================

    /** Apply post-cast wear to an attachment. Caller resolves the live
     *  FRuntimeAttachedItem reference (e.g. via
     *  ULoadoutComponent::FindAttachedItemByHolder) and passes it in.
     *  Branches internally on Attachment.Kind.
     *  @param Actor         The wielder/wearer
     *  @param Holder        Provenance — the UWeaponData or URingData that
     *                       owns the attachment. Forwarded to broadcasts so
     *                       consumers can disambiguate weapon vs ring breaks.
     *  @param Attachment    Live attachment reference; wear and break state
     *                       transitions are written here.
     *  @param ActionTier    Tier of the action being wear-applied
     *  @param InfusionLevel L0/L1/L2 charge level
     *  @param bIsSpell      True if the action is a spell (affects wear math) */
    void ProcessPostCastWear(
        AActor *Actor,
        UObject *Holder,
        FRuntimeAttachedItem &Attachment,
        EItemTier ActionTier,
        int32 InfusionLevel,
        bool bIsSpell);

    /** Apply post-cast wear to the active loadout's standalone primary-slot
     *  evolution (PrimarySlotType==Evolution, e.g. Broken Darkness). Reads
     *  the crystal-modified substat fractions off the caster's
     *  UCharacterDataComponent (case-B aware via ApplyEvolutionPillarModifier),
     *  computes wear via UBreakCalculator::CalculateDurabilityWearWithSubstats,
     *  and writes live storage via ULoadoutComponent::ApplyWearToActivePrimaryEvolution.
     *  Leaner than the refined path: NO luck-skip roll, NO OnCrystalBroken
     *  broadcast (between-combat destruction sweep handles cleanup). */
    void ProcessPostCastEvolutionWear(
        AActor *Actor,
        ULoadoutComponent *LC,
        EItemTier ActionTier,
        int32 InfusionLevel,
        bool bIsSpell);

    // ========================================
    // EVENTS
    // ========================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
        FOnCrystalBroken,
        AActor *, Actor,
        UObject *, Holder,
        FBrokenCrystalPayload, Payload);

    /** Fires when any tracked attachment's durability hits 0.
     *  Holder is provenance — cast to UWeaponData or URingData to filter
     *  by holder type. Payload discriminates refined (FCrystalId) vs
     *  evolution (asset pointer). */
    UPROPERTY(BlueprintAssignable, Category = "Crystal Manager|Events")
    FOnCrystalBroken OnCrystalBroken;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
        FOnCrystalDurabilityChanged,
        AActor *, Actor,
        UObject *, Holder,
        int32, NewDurability,
        int32, MaxDurability);

    /** Fires whenever a tracked crystal's durability changes (per-cast wear).
     *  Use for real-time UI durability bar updates. */
    UPROPERTY(BlueprintAssignable, Category = "Crystal Manager|Events")
    FOnCrystalDurabilityChanged OnCrystalDurabilityChanged;

    // ========================================
    // DEBUG
    // ========================================

    /** Force-break the active character's primary weapon crystal. Drains the
     *  per-instance durability to 1 directly, then routes through the real
     *  ProcessPostCastWear pipeline (in a bounded retry loop, since Luck-skip
     *  is probabilistic) so OnCrystalBroken broadcasts via the production path.
     *  Console: type "DebugBreakActiveCrystal" in PIE. No-op outside combat,
     *  on already-broken crystals, or on unrefined / immune crystals. */
    UFUNCTION(Exec)
    void DebugBreakActiveCrystal();

    /** Apply raw wear to the active character's primary weapon crystal. Calls
     *  FRuntimeAttachedItem::ApplyWear directly on the live attachment —
     *  bypasses the BreakCalculator tier math AND the luck-skip roll — so the
     *  underlying bCanBreak gate (evolution branch) and durability transition
     *  are exercised deterministically. Handles both refined and evolution
     *  attachments and logs WHICH branch was hit, since the bCanBreak flip
     *  only affects evolution.
     *  Console: type "DebugForceWearActiveCrystal 10" in PIE (default Amount=10).
     *  No broadcasts — this is a raw test path, not a production wear event. */
    UFUNCTION(Exec)
    void DebugForceWearActiveCrystal(int32 Amount = 10);

    // ========================================
    // WOR_ DEBUG SUITE
    // ========================================
    // Invoked from ACombatOrchestrator's Debug|CrystalWear CallInEditor buttons
    // (Details panel during PIE). Plain non-Exec methods — UGameInstanceSubsystem
    // isn't in the console FExec chain, so the orchestrator hosts the entry
    // points and these stay as thin live-state callers.

    /** Print a substat-modified wear prediction table (rows F->S) for the
     *  active combatant. Reads live caster fractions via the crystal-modified
     *  pillar accessors on UCharacterDataComponent — no actual wear is applied.
     *  Action config defaults to the worst-case envelope (S-Tier, L2, Spell). */
    void WOR_WearTable();

    /** Run the real ProcessPostCastWear path on the active combatant's primary
     *  weapon crystal once. Logs predicted breakdown (base, power_factor,
     *  control_factor, tier_gap, final wear) and before/after durability —
     *  decisive end-to-end live test versus the prediction table. Spell-cast
     *  is implied. Args clamp into valid ranges (ActionTier 0..6, InfusionLevel 0..2). */
    void WOR_SimCast(int32 ActionTier, int32 InfusionLevel);

    /** Print the active combatant's equipped primary weapon crystal: type
     *  (Refined / Evolution), tier, bCanBreak, and current/max durability. */
    void WOR_CrystalState();

private:
    // ========================================
    // HELPERS
    // ========================================

    /** Get LoadoutComponent for an actor (null-safe). */
    ULoadoutComponent *GetLoadoutComponent(AActor *Actor) const;
};
