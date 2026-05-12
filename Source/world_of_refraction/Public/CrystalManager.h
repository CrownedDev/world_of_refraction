// Source/world_of_refraction/Public/CrystalManager.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ItemTier.h"
#include "CrystalManager.generated.h"

class UItemData;
class ULoadoutComponent;

/**
 * UCrystalManager
 *
 * Unified crystal lifecycle and event broker. Owns:
 *  - Per-combatant crystal subscription pipeline (replaces the per-manager
 *    pipelines previously split between UWeaponManager and URingManager)
 *  - Post-cast durability wear (replaces UWeaponManager::ProcessPostCastWear
 *    and URingManager::ProcessPostCastWear)
 *  - Unified broadcasts: OnCrystalBroken, OnCrystalDurabilityChanged
 *
 * Phase A note: durability lives on shared UItemData::CurrentDurability.
 * Two actors slotting the same UItemData asset share wear — this matches
 * the pre-existing legacy behaviour. Phase B will migrate durability to
 * FCrystalInventoryEntry::CurrentDurability for per-instance state.
 *
 * Lifecycle:
 *   Combat start: CombatOrchestrator → RegisterCombatant(Actor)
 *   During combat: ProcessPostCastWear called from ActionExecutor
 *   Crystal break: UItemData::OnCrystalBroken → UCrystalManager
 *                  rebroadcasts as unified OnCrystalBroken
 *   Combat end: CombatOrchestrator → UnregisterCombatant(Actor)
 */
UCLASS()
class WORLD_OF_REFRACTION_API UCrystalManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;
    virtual void Deinitialize() override;

    // ========================================
    // LIFECYCLE
    // ========================================

    /** Register an actor with the crystal subscription pipeline. Called at
     *  combat start. Walks the actor's LoadoutComponent for equipped weapons
     *  and rings, subscribes to each slotted crystal's OnCrystalBroken. */
    UFUNCTION(BlueprintCallable, Category = "Crystal Manager")
    void RegisterCombatant(AActor *Actor);

    /** Unregister an actor from the crystal subscription pipeline. Called at
     *  combat end. Unsubscribes from all tracked crystals for this actor. */
    UFUNCTION(BlueprintCallable, Category = "Crystal Manager")
    void UnregisterCombatant(AActor *Actor);

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
        UItemData *Crystal,
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
        UItemData *, Crystal);

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

private:
    // ========================================
    // SUBSCRIPTION TRACKING
    // ========================================

    /** Crystals currently subscribed to, keyed by actor. Used at
     *  UnregisterCombatant time to know which crystals to detach from. */
    TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<UItemData>>> TrackedCrystals;

    /** Bound to UItemData::OnCrystalBroken for every subscribed crystal.
     *  Resolves owner via TObjectIterator<ULoadoutComponent>, then
     *  rebroadcasts as unified OnCrystalBroken. */
    UFUNCTION()
    void HandleCrystalBroken(UItemData *BrokenCrystal);

    // ========================================
    // HELPERS
    // ========================================

    /** Get LoadoutComponent for an actor (null-safe). */
    ULoadoutComponent *GetLoadoutComponent(AActor *Actor) const;
};
