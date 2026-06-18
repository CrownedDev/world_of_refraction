// SkillCastEntry.h
// One self-contained delivery in a skill's Cast array (D6). Index-ordered:
// a UCombatNotify (Family=Cast, Index=N) fires entry N — array position IS
// identity. Size is the MECHANICAL hitbox, shaped by DeliveryType; VisualScale
// is the delivery's overall visual size (Scale on VFX entries for those).

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Skills/Definitions/ESpellDeliveryType.h"
#include "Combat/Defense/DefenseDifficulty.h"
#include "SkillCastEntry.generated.h"

class UNiagaraSystem;
class ASkillProjectile;

/**
 * One entry in a skill's Cast array. Each entry describes ONE delivery —
 * its type, travel, hitbox, and trail. A skill mixes freely (fireball-then-
 * pillar = a Projectile entry + an AOE entry). Consumed by the fused-montage
 * runner (Stage 12); until then nothing reads it.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSkillCastEntry
{
    GENERATED_BODY()

    /** Designer note ("fireball volley") — never read by code. */
    UPROPERTY(EditAnywhere, Category = "Cast")
    FString Label;

    UPROPERTY(EditAnywhere, Category = "Cast")
    ESpellDeliveryType DeliveryType = ESpellDeliveryType::Projectile;

    /** Travel actor for Projectile. Null = the executor's
     *  DefaultProjectileClass (today's fallback). */
    UPROPERTY(EditAnywhere, Category = "Cast",
              meta = (EditCondition = "DeliveryType == ESpellDeliveryType::Projectile",
                      EditConditionHides))
    TSubclassOf<ASkillProjectile> ProjectileClass;

    UPROPERTY(EditAnywhere, Category = "Cast",
              meta = (EditCondition = "DeliveryType == ESpellDeliveryType::Projectile",
                      EditConditionHides, ClampMin = "100.0"))
    float ProjectileSpeed = 1500.0f;

    /** MECHANICAL hitbox — meaning follows DeliveryType: Projectile =
     *  moving collision radius, AOE = static ground radius. Never the
     *  visual (that's VisualScale / the VFX entries' Scale). */
    UPROPERTY(EditAnywhere, Category = "Cast", meta = (ClampMin = "0.1"))
    float Size = 1.0f;

    /** Trail VFX — follows the moving projectile (which a location-spawned
     *  VFXArray entry can't); doubles as the AOE/Instant ground visual, as
     *  the loose SpellVFX does today. Soft ref — the runner resolves at
     *  spawn time (Stage 12). */
    UPROPERTY(EditAnywhere, Category = "Cast")
    TSoftObjectPtr<UNiagaraSystem> Trail;

    /** Overall VISUAL size of this delivery (feeds the defense-window size,
     *  muzzle scale, dispatch radius multiplier, and trail) — NOT trail-
     *  specific (Trail above is the VFX itself). Cosmetic axis; Size is the
     *  mechanical hitbox (D6). */
    UPROPERTY(EditAnywhere, Category = "Cast", meta = (ClampMin = "0.1"))
    float VisualScale = 1.0f;

    /** Barrage: >1 spawns Count deliveries staggered by BurstInterval. */
    UPROPERTY(EditAnywhere, Category = "Cast", meta = (ClampMin = "1"))
    int32 Count = 1;

    UPROPERTY(EditAnywhere, Category = "Cast",
              meta = (EditCondition = "Count > 1", ClampMin = "0.01"))
    float BurstInterval = 0.15f;

    /** This cast deals no damage (a pure interaction moment). The defense window still opens
     *  (defendable/interruptable); only damage is suppressed. Default false. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
    bool bIgnoreDamage = false;

    /** This cast applies no status buildup. Independent of bIgnoreDamage (all four combinations
     *  authorable). Default false. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
    bool bIgnoreStatus = false;

    /** If this cast is successfully parried OR dodged (by ALL targets), the rest of the action aborts
     *  (montage stops, no further hits/casts; plays the return montage if authored). Default false. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
    bool bInterruptable = false;

    /** This delivery's per-defense-type difficulty (parry/dodge/block), keyed PER CAST-ENTRY (per
     *  delivery), not per hit. All-Inherit -> resolves to the skill-level DefaultDifficulty, then
     *  Easy (x1.0). Resolve + per-impact routing wired in later Stage-6 clusters. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast")
    FDefenseDifficultyTriple Difficulty;
};
