// SkillProjectile.h
// Unified spell delivery actor - handles the Projectile delivery type
// AOE and Instant don't spawn this actor - use ActionExecutor directly

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Skills/Definitions/ESpellDeliveryType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "SkillProjectile.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class USphereComponent;
class USpellData;
struct FSkillCastEntry;

// ==================== DELEGATES ====================

/** Broadcast when projectile hits target (or reaches target location). CastEntryIndex carries which
 *  CastArray entry spawned this projectile (Stage 6 cluster 4) so the arrival handler can resolve the
 *  per-entry defense difficulty; INDEX_NONE for non-entry (loose/test) projectiles. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
    FOnSkillImpact,
    AActor *, Target,
    FVector, ImpactLocation,
    float, ImpactRadius,
    int32, Damage,
    int32, CastEntryIndex);

/** Broadcast when target successfully dodged (moved out of impact zone) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnSkillDodged,
    AActor *, Target,
    FVector, ImpactLocation);

// ==================== MAIN CLASS ====================

/**
 * ASkillProjectile
 *
 * Unified spell delivery actor that handles:
 * - Projectile: Travels to fixed location, dodgeable by moving
 *
 * AOE and Instant spells don't use this actor.
 *
 * Usage:
 * 1. Spawn via SpawnActor
 * 2. Call InitializeProjectile() with spell data
 * 3. Call SetVFXAssets() to assign Niagara systems
 * 4. Bind to OnSkillImpact/OnSkillDodged events
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API ASkillProjectile : public AActor
{
    GENERATED_BODY()

public:
    ASkillProjectile();

    // ==================== COMPONENTS ====================

protected:
    /** Root scene component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent *Root;

    /** Collision detection sphere - sized by ImpactRadius */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent *HitBox;

    /** VFX: Spawn flash at caster (optional) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|VFX")
    UNiagaraComponent *MuzzleFX;

    /** VFX: Traveling projectile visual */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|VFX")
    UNiagaraComponent *ProjectileFX;

    /** VFX: Impact effect (spawned on hit/miss) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|VFX")
    UNiagaraComponent *HitFX;

public:
    // ==================== EVENTS ====================

    /** Called when spell reaches target / impact point */
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSkillImpact OnSkillImpact;

    /** Called when target successfully dodged */
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSkillDodged OnSkillDodged;

    // ==================== CONFIGURATION ====================

protected:
    /** Delivery behavior type */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    ESpellDeliveryType DeliveryType;

    /** Element for VFX coloring */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    ESpellElement Element;

    /** Reference to source spell data */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    USpellData *SourceSpell;

    /** Who cast this spell */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    AActor *Caster;

    /** Primary target actor */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    AActor *Target;

    /** Target location at cast time (for projectile) */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    FVector TargetLocation;

    /** Travel speed (units/second) */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    float Speed;

    /** Final calculated impact radius */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    float ImpactRadius;

    /** Final calculated damage */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    int32 Damage;

    /** Visual scale for VFX */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    float VisualScale;

public:
    // ==================== INITIALIZATION ====================

    /**
     * Initialize the projectile with spell data and combat context
     * Call this immediately after spawning
     *
     * @param Spell Source spell data asset
     * @param InCaster Actor who cast this spell
     * @param InTarget Target actor
     * @param FinalImpactRadius Calculated impact radius (BaseSize * HitboxRatio * multipliers)
     * @param FinalVisualScale Calculated visual scale for VFX
     * @param FinalDamage Calculated damage value
     */
    UFUNCTION(BlueprintCallable, Category = "SkillProjectile")
    void InitializeProjectile(
        USpellData *Spell,
        AActor *InCaster,
        AActor *InTarget,
        float FinalImpactRadius,
        float FinalVisualScale,
        int32 FinalDamage);

    /** Entry-based initialization (D6 Stage 12): delivery values (type,
     *  speed) come from the Cast ENTRY; Spell supplies element/color context
     *  only. C++-only overload — the runner's dispatch path. */
    void InitializeProjectile(
        const FSkillCastEntry &Entry,
        USpellData *Spell,
        AActor *InCaster,
        AActor *InTarget,
        float FinalImpactRadius,
        float FinalVisualScale,
        int32 FinalDamage,
        int32 InCastEntryIndex);

    /**
     * Set VFX assets (call after Initialize or set via defaults)
     *
     * @param InMuzzleFX Spawn flash effect (optional)
     * @param InProjectileFX Traveling projectile effect
     * @param InHitFX Impact explosion effect (optional)
     */
    UFUNCTION(BlueprintCallable, Category = "SkillProjectile|VFX")
    void SetVFXAssets(
        UNiagaraSystem *InMuzzleFX,
        UNiagaraSystem *InProjectileFX,
        UNiagaraSystem *InHitFX);

    /**
     * Start projectile movement and activate VFX
     * Call after SetVFXAssets and InitializeProjectile
     */

    UFUNCTION(BlueprintCallable, Category = "SkillProjectile")
    void Launch();
    // ==================== GETTERS ====================

    UFUNCTION(BlueprintPure, Category = "SkillProjectile")
    FORCEINLINE ESpellDeliveryType GetDeliveryType() const { return DeliveryType; }

    UFUNCTION(BlueprintPure, Category = "SkillProjectile")
    FORCEINLINE AActor *GetCaster() const { return Caster; }

    UFUNCTION(BlueprintPure, Category = "SkillProjectile")
    FORCEINLINE AActor *GetTarget() const { return Target; }

    UFUNCTION(BlueprintPure, Category = "SkillProjectile")
    FORCEINLINE float GetImpactRadius() const { return ImpactRadius; }

    // ==================== RUNTIME STATE ====================

    /** Cast-array index of the entry that spawned this projectile (Option A — barrage/beam share it).
     *  INDEX_NONE (-1) = not entry-spawned (loose-field / test path) -> cluster-4 reader falls back to
     *  Easy x1.0. Read at arrival in cluster 4. */
    int32 CastEntryIndex = INDEX_NONE;

    /** Has the projectile reached its destination? */
    bool bHasImpacted = false;

    /** Has Launch() been called? Prevents Tick until ready */
    bool bIsLaunched = false;

    UFUNCTION(BlueprintPure, Category = "SkillProjectile")
    FORCEINLINE bool HasImpacted() const { return bHasImpacted; }

protected:
    // ==================== LIFECYCLE ====================

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Shared tail of both InitializeProjectile overloads: target capture,
     *  collision sizing, positioning, colors/scale. Callers set the per-source
     *  fields (delivery/speed) first. */
    void InitializeCommon();

    // ==================== MOVEMENT ====================

    /** Projectile: Move toward fixed target location */
    void TickProjectile(float DeltaTime);

    // ==================== COLLISION ====================

    UFUNCTION()
    void OnHitBoxOverlap(
        UPrimitiveComponent *OverlappedComponent,
        AActor *OtherActor,
        UPrimitiveComponent *OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult &SweepResult);

    // ==================== RESOLUTION ====================

    /** Called when projectile reaches destination */
    void ResolveImpact();

    /** Check if target dodged by moving */
    bool CheckMoveDodge() const;

    /** Spawn hit VFX and cleanup */
    void PlayHitEffect(FVector Location);

    /** Cleanup and destroy */
    void DestroyProjectile();

    // ==================== VFX ====================

    /** Apply element colors to Niagara components */
    void ApplyElementColors();

    /** Scale VFX components to VisualScale */
    void ApplyVisualScale();

    // ==================== DEBUG ====================

#if WITH_EDITOR
public:
    /** Debug: Draw collision sphere */
    UFUNCTION(CallInEditor, Category = "Debug")
    void Debug_DrawCollision();

    /** Debug: Print projectile state */
    UFUNCTION(CallInEditor, Category = "Debug")
    void Debug_PrintState();
#endif
};
