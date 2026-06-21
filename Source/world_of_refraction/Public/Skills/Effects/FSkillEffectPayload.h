// FSkillEffectPayload.h
// The "what happens" half of a skill effect, lifted verbatim (types/defaults/meta)
// from FSkillEffect. One skill effect may carry several payloads. Condition/threshold
// data lives on the effect's condition group (FSkillCondition), not here.
//
// Cluster A: additive new type for the FSkillEffect reshape. Nothing reads it yet.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Effects/ESkillEffectTiming.h"
#include "Combat/TargetType.h"
#include "FSkillEffectPayload.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSkillEffectPayload
{
    GENERATED_BODY()

    /** What effect to apply (buff, debuff, restore, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    ESkillEffectType EffectType = ESkillEffectType::None;

    /** Effect strength. Buffs/debuffs: decimal percent (0.2 = 20%). Restore/drain: flat or DrainPercent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect",
              meta = (ClampMin = "0.0"))
    float Magnitude = 0.0f;

    /** Flat value for absolute amounts (e.g. 30 = 30 HP/turn DOT). Don't set with Magnitude. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect", meta = (ClampMin = "0"))
    int32 Value = 0;

    /** Duration in turns (0 = instant effect like heal/damage). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect",
              meta = (ClampMin = "0"))
    int32 Duration = 0;

    /** When this effect ticks during its life (authored). Default Persistent matches the
     *  prior hardcoded Build timing, so unauthored payloads behave as before. An OnTrigger
     *  promotion still overrides this when the effect carries owner-side conditions. A
     *  timing that doesn't fit a passively-read stat-buff is harmless (its logic is a no-op). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    ESkillEffectTiming ProcessTiming = ESkillEffectTiming::Persistent;

    /** Explicit "never expires" toggle — author permanent gear/aura effects by ticking
     *  THIS, not by setting Duration to 0. Permanent is now decoupled from duration-0
     *  (which means INSTANT). When true, Duration is irrelevant (the effect is Persistent
     *  and never ticks down). Default false. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    bool bPermanent = false;

    /** Opt out of fire-on-application. Default false = the effect fires on apply (turn 0) then
     *  ticks on its windows. True = no apply tick, windows only (e.g. a DOT that should not deal
     *  its first tick on the cast turn). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    bool bDelayFirstExecution = false;

    /** Who receives this effect. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    ETargetType Target = ETargetType::Enemy;

    /** How many recipients of the Target role this payload hits. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    ETargetCount TargetCount = ETargetCount::Single;

    /** For drain payloads: fraction of damage dealt converted to restore (0.3 = 30%).
     *  The legacy OnHit EditCondition is intentionally dropped here — the condition
     *  now lives on the effect's condition group, revisited in a later cluster. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drain",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DrainPercent = 0.0f;
};
