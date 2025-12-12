// SpellProjectile.h
// Unified spell delivery actor - handles Projectile, Homing, and Beam types
// AOE and Instant don't spawn this actor - use ActionExecutor directly

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ESpellDeliveryType.h"
#include "SpellElement.h"
#include "SpellProjectile.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class USphereComponent;
class USpellData;

// ==================== DELEGATES ====================

/** Broadcast when projectile hits target (or reaches target location) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FOnSpellImpact,
    AActor *, Target,
    FVector, ImpactLocation,
    float, ImpactRadius,
    int32, Damage);

/** Broadcast when target successfully dodged (moved out of impact zone) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnSpellDodged,
    AActor *, Target,
    FVector, ImpactLocation);

/** Broadcast when beam connects each tick */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnBeamTick,
    AActor *, Target,
    float, DeltaTime,
    bool, bTargetInBeam);

// ==================== MAIN CLASS ====================

/**
 * ASpellProjectile
 *
 * Unified spell delivery actor that handles:
 * - Projectile: Travels to fixed location, dodgeable by moving
 * - Homing: Tracks target actor, harder to dodge
 * - Beam: Continuous line trace, brief dodge window
 *
 * AOE and Instant spells don't use this actor.
 *
 * Usage:
 * 1. Spawn via SpawnActor
 * 2. Call InitializeProjectile() with spell data
 * 3. Call SetVFXAssets() to assign Niagara systems
 * 4. Bind to OnSpellImpact/OnSpellDodged events
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API ASpellProjectile : public AActor
{
    GENERATED_BODY()

public:
    ASpellProjectile();

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
    FOnSpellImpact OnSpellImpact;

    /** Called when target successfully dodged */
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSpellDodged OnSpellDodged;

    /** Called each tick while beam is active */
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBeamTick OnBeamTick;

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

    /** Primary target actor (for homing/beam) */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    AActor *Target;

    /** Target location at cast time (for projectile) */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    FVector TargetLocation;

    /** Travel speed (units/second) */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    float Speed;

    /** Homing tracking strength (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    float HomingStrength;

    /** Final calculated impact radius */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    float ImpactRadius;

    /** Final calculated damage */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    int32 Damage;

    /** Beam duration (for Beam type) */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    float BeamDuration;

    /** Visual scale for VFX */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    float VisualScale;

    /** Is this an elemental attack? (affects defense/absorption) */
    UPROPERTY(BlueprintReadOnly, Category = "Config")
    bool bIsElemental;

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
    UFUNCTION(BlueprintCallable, Category = "SpellProjectile")
    void InitializeProjectile(
        USpellData *Spell,
        AActor *InCaster,
        AActor *InTarget,
        float FinalImpactRadius,
        float FinalVisualScale,
        int32 FinalDamage);

    /**
     * Set VFX assets (call after Initialize or set via defaults)
     *
     * @param InMuzzleFX Spawn flash effect (optional)
     * @param InProjectileFX Traveling projectile effect
     * @param InHitFX Impact explosion effect (optional)
     */
    UFUNCTION(BlueprintCallable, Category = "SpellProjectile|VFX")
    void SetVFXAssets(
        UNiagaraSystem *InMuzzleFX,
        UNiagaraSystem *InProjectileFX,
        UNiagaraSystem *InHitFX);

    /**
     * Start projectile movement and activate VFX
     * Call after SetVFXAssets and InitializeProjectile
     */

    UFUNCTION(BlueprintCallable, Category = "SpellProjectile")
    void Launch();
    // ==================== GETTERS ====================

    UFUNCTION(BlueprintPure, Category = "SpellProjectile")
    FORCEINLINE ESpellDeliveryType GetDeliveryType() const { return DeliveryType; }

    UFUNCTION(BlueprintPure, Category = "SpellProjectile")
    FORCEINLINE AActor *GetCaster() const { return Caster; }

    UFUNCTION(BlueprintPure, Category = "SpellProjectile")
    FORCEINLINE AActor *GetTarget() const { return Target; }

    UFUNCTION(BlueprintPure, Category = "SpellProjectile")
    FORCEINLINE float GetImpactRadius() const { return ImpactRadius; }

    // ==================== RUNTIME STATE ====================

    /** Has the projectile reached its destination? */
    bool bHasImpacted = false;

    /** Has Launch() been called? Prevents Tick until ready */
    bool bIsLaunched = false;

    /** Beam: Time remaining */
    float BeamTimeRemaining;

    /** Beam: Is target currently in beam path? */
    bool bTargetInBeam;

    UFUNCTION(BlueprintPure, Category = "SpellProjectile")
    FORCEINLINE bool HasImpacted() const { return bHasImpacted; }

    UFUNCTION(BlueprintPure, Category = "SpellProjectile")
    FORCEINLINE bool IsElemental() const { return bIsElemental; }

protected:
    // ==================== LIFECYCLE ====================

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ==================== MOVEMENT ====================

    /** Projectile: Move toward fixed target location */
    void TickProjectile(float DeltaTime);

    /** Homing: Move toward target actor with tracking */
    void TickHoming(float DeltaTime);

    /** Beam: Update line trace and damage */
    void TickBeam(float DeltaTime);

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
