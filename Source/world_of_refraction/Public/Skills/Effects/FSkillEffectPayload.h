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
#include "Skills/Definitions/ESpellElement.h"
#include "Combat/TargetType.h"
#include "FSkillEffectPayload.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSkillEffectPayload
{
    GENERATED_BODY()

    /** What effect to apply (buff, debuff, restore, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    ESkillEffectType EffectType = ESkillEffectType::None;

    /** Element this effect is keyed to. None = generic / element-agnostic (e.g. a StatusMultiplierBuff
     *  that amplifies ALL elemental buildup); a real element = element-keyed (amplifies only that
     *  element). Threaded into the runtime FActiveSkillEffect::Element by BuildRuntimeFromPayload, so a
     *  conditional/gear payload can grant an element-keyed stack ("on parry → gain a Lightning stack").
     *  The spell-cast path overrides this with the resolved cast element. Default None = backward-
     *  compatible generic (USTRUCT serialized by name, so existing payloads load as None). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    ESpellElement Element = ESpellElement::None;

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

    /** Charge count — SEPARATE governor from Duration. 0 = unlimited (no charge system,
     *  existing effects untouched). >0 = the effect fires N times then is removed via its own
     *  charge-expiry path (ConsumeCharge), independent of turn-expiry. ⚠️ A charged effect MUST
     *  be bPermanent OR Duration > 0 — a duration-0 non-permanent effect takes the instant lane
     *  (never stored) so its charges would never fire. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect", meta = (ClampMin = "0"))
    int32 Charges = 0;

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

    /** Attack-scaled value source for reactive defense effects. >0 = this payload's granted value is
     *  a FRACTION of the triggering attack's BASE energy cost (pre-efficiency inherent worth, threaded
     *  through OnDefenseResolved) — e.g. 0.05 = "gain 5% of the parried attack's cost". 0 = OFF: use
     *  the authored Value/Magnitude (default, byte-identical). Consumed by
     *  USkillEffectManager::OnDefenseResolvedHandler; the payload's EffectType still picks the
     *  primitive (EnergyRestore → EP via ServerGainEnergy, HealthRestore → HP via ServerHeal). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect", meta = (ClampMin = "0"))
    float AttackCostScale = 0.0f;
};
