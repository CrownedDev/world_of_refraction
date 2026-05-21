// Source/world_of_refraction/Public/CrystalManager.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ItemTier.h"
#include "CrystalManager.generated.h"

class UEvolutionItemData;
class ULoadoutComponent;

/**
 * UCrystalManager
 *
 * Unified crystal lifecycle and event broker. Owns:
 *  - Post-cast durability wear (replaces UWeaponManager::ProcessPostCastWear
 *    and URingManager::ProcessPostCastWear)
 *  - Unified broadcasts: OnCrystalBroken, OnCrystalDurabilityChanged
 *
 * Per-instance durability state lives on FCrystalInventoryEntry; this
 * manager resolves the entry via LoadoutComponent at wear time and
 * broadcasts breaks directly on the entry's transition to 0.
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

    /** Apply post-cast wear to a crystal. Generic on Crystal so it works for
     *  weapons, rings, or any future holder type.
     *  @param Actor       The wielder/wearer
     *  @param Crystal     The crystal taking wear
     *  @param Holder      Provenance — the UWeaponData or URingData that
     *                     holds this crystal. Forwarded to broadcasts for
     *                     consumer disambiguation.
     *  @param ActionTier  Tier of the action being wear-applied
     *  @param InfusionLevel  L0/L1/L2 charge level
     *  @param bIsSpell    True if the action is a spell (affects wear math)
     *  @return Wear amount actually applied (0 if Luck skip or immune) */
    UFUNCTION(BlueprintCallable, Category = "Crystal Manager")
    int32 ProcessPostCastWear(
        AActor *Actor,
        UEvolutionItemData *Crystal,
        UObject *Holder,
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
        UEvolutionItemData *, Crystal);

    /** Fires when any tracked crystal's durability hits 0.
     *  Holder is provenance — cast to UWeaponData or URingData to filter
     *  by holder type. */
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

private:
    // ========================================
    // HELPERS
    // ========================================

    /** Get LoadoutComponent for an actor (null-safe). */
    ULoadoutComponent *GetLoadoutComponent(AActor *Actor) const;
};
