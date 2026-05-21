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

private:
    // ========================================
    // HELPERS
    // ========================================

    /** Get LoadoutComponent for an actor (null-safe). */
    ULoadoutComponent *GetLoadoutComponent(AActor *Actor) const;
};
