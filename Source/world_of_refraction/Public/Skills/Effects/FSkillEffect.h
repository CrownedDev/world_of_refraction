// FSkillEffect.h
// Struct defining a single effect applied by abilities, spells, and weapon attacks
// Reuses existing enums: ESkillEffectType, ETargetType, ESkillTrigger

#pragma once

#include "CoreMinimal.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Combat/TargetType.h"
#include "Skills/Effects/ESkillTrigger.h"
#include "Skills/Effects/SkillTriggerUtils.h"
#include "Skills/Effects/FSkillCondition.h"
#include "Skills/Effects/FSkillEffectPayload.h"
#include "FSkillEffect.generated.h"

/**
 * FSkillEffect
 * Defines a single effect that an ability, spell, or weapon attack can apply.
 * Skills can have multiple effects (max 5).
 *
 * Examples:
 * - Power Strike: RawDamageBuff to Self OnHit
 * - Drain Strike: EnergyRestore to Self OnHit with DrainPercent
 * - War Cry: RawDamageBuff to Ally (All) Always
 * - Intimidate: RawDamageDebuff to Enemy (Single) Always
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSkillEffect
{
    GENERATED_BODY()

    // ==================== EFFECT ====================

    /** Optional display name for passive-style effects shown in UI. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FString EffectName = TEXT("");

    // ==================== DYNAMIC (Conditions[] + Payloads[]) ====================

    /** Condition group: per-entry AND/OR + source/target side (FSkillCondition).
     *  Empty == Always (unconditional). The single source of truth for gating. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
    TArray<FSkillCondition> Conditions;

    /** Payload list: one effect may carry several payloads (FSkillEffectPayload).
     *  Each payload is one applied effect (type/value/duration/target). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    TArray<FSkillEffectPayload> Payloads;

    // ==================== STACKING ====================

    /** Multiple applications stack (up to MaxStacks) instead of refreshing duration.
     *  Per-effect (shared across payloads). Default false = today's behaviour. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stacking")
    bool bStackable = false;

    /** Max stacks when bStackable. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stacking",
              meta = (EditCondition = "bStackable", ClampMin = "1", ClampMax = "99"))
    int32 MaxStacks = 3;

    /** Applies at most once per combat (keyed on the stable EffectID). Re-application
     *  is rejected before stacking/refresh. Default false = today's behaviour. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stacking")
    bool bFiresOncePerMatch = false;

    // ==================== CONSTRUCTORS ====================

    FSkillEffect()
    {
    }

    // ==================== HELPERS ====================

    // Every classifier reads the new Payloads[]/Conditions[] arrays directly.
    // Empty Conditions[] == Always (unconditional); empty Payloads[] == no effect.

    /** Is this effect valid? Any payload typed. */
    bool IsValid() const
    {
        for (const FSkillEffectPayload &P : Payloads)
        {
            if (P.EffectType != ESkillEffectType::None) return true;
        }
        return false;
    }

    /** Buff if ANY payload classifies as a buff. */
    bool IsBuff() const
    {
        for (const FSkillEffectPayload &P : Payloads)
        {
            if (SkillEffectClassification::IsBuff(P.EffectType, P.Magnitude)) return true;
        }
        return false;
    }

    /** Debuff if ANY payload classifies as a debuff. */
    bool IsDebuff() const
    {
        for (const FSkillEffectPayload &P : Payloads)
        {
            if (SkillEffectClassification::IsDebuff(P.EffectType, P.Magnitude)) return true;
        }
        return false;
    }

    /** Restore if ANY payload is Health/EnergyRestore. */
    bool IsRestore() const
    {
        for (const FSkillEffectPayload &P : Payloads)
        {
            if (P.EffectType == ESkillEffectType::HealthRestore ||
                P.EffectType == ESkillEffectType::EnergyRestore) return true;
        }
        return false;
    }

    /** Drain: a restore payload with DrainPercent>0 AND some source-side OnHit condition. */
    bool IsDrain() const
    {
        bool bDrainPayload = false;
        for (const FSkillEffectPayload &P : Payloads)
        {
            const bool bRestore = (P.EffectType == ESkillEffectType::HealthRestore ||
                                   P.EffectType == ESkillEffectType::EnergyRestore);
            if (bRestore && P.DrainPercent > 0.0f) { bDrainPayload = true; break; }
        }
        bool bOnHitSource = false;
        for (const FSkillCondition &C : Conditions)
        {
            if (!C.bTargetSide && C.Trigger == ESkillTrigger::OnHit) { bOnHitSource = true; break; }
        }
        return bDrainPayload && bOnHitSource;
    }

    /** Instant if every payload has Duration == 0. */
    bool IsInstant() const
    {
        for (const FSkillEffectPayload &P : Payloads)
        {
            if (P.Duration != 0) return false;
        }
        return true;
    }

    /** Conditional if the condition group is non-empty (empty == Always / unconditional). */
    bool IsConditional() const
    {
        return Conditions.Num() > 0;
    }

    /** Carries a target-side condition: any bTargetSide entry. */
    bool HasTargetCondition() const
    {
        for (const FSkillCondition &C : Conditions)
        {
            if (C.bTargetSide) return true;
        }
        return false;
    }

    /** Carries a secondary source-side condition: 2+ source-side entries. */
    bool HasSecondaryCondition() const
    {
        int32 SourceConds = 0;
        for (const FSkillCondition &C : Conditions)
        {
            if (!C.bTargetSide) ++SourceConds;
        }
        return SourceConds >= 2;
    }

    /** Has a trigger condition (source-side non-Always or target-side). The complement
     *  is a STARTING effect: gear effects with no condition apply once at combat start
     *  (GetStartingEffects), then live as normal clearable effects. Distinct from
     *  IsConditional(), which counts any non-empty group. */
    bool IsConditionalEffect() const
    {
        for (const FSkillCondition &C : Conditions)
        {
            if (C.bTargetSide) return true;
            if (C.Trigger != ESkillTrigger::Always) return true;
        }
        return false;
    }

    // ==================== DEBUG ====================

    /** Get a description string for debug/UI. Renders the Conditions[]/Payloads[] shape. */
    FString GetDescription() const
    {
        FString Desc;
        if (!EffectName.IsEmpty())
        {
            Desc = EffectName + TEXT(": ");
        }

        // Payloads — comma-joined.
        if (Payloads.Num() == 0)
        {
            Desc += TEXT("(no payload)");
        }
        else
        {
            for (int32 i = 0; i < Payloads.Num(); ++i)
            {
                if (i > 0) Desc += TEXT(", ");
                Desc += DescribePayload(Payloads[i]);
            }
        }

        // Conditions — source-side joined by each entry's AND/OR (a leading Always
        // contributes nothing, matching the legacy renderer); target-side as [target: …].
        const UEnum *TriggerEnum = StaticEnum<ESkillTrigger>();
        auto RenderCond = [TriggerEnum](const FSkillCondition &C) -> FString
        {
            FString Name = TriggerEnum ? TriggerEnum->GetDisplayNameTextByValue(static_cast<int64>(C.Trigger)).ToString() : TEXT("Unknown");
            return SkillTriggerUtils::IsThresholdTrigger(C.Trigger)
                       ? FString::Printf(TEXT("%s %.0f%%"), *Name, C.Threshold)
                       : Name;
        };

        FString SourceStr;
        for (const FSkillCondition &C : Conditions)
        {
            if (C.bTargetSide || C.Trigger == ESkillTrigger::Always) continue;
            if (!SourceStr.IsEmpty())
            {
                SourceStr += (C.Combine == ECondCombine::And) ? TEXT(" AND ") : TEXT(" OR ");
            }
            SourceStr += RenderCond(C);
        }
        if (!SourceStr.IsEmpty())
        {
            Desc += FString::Printf(TEXT(" (%s)"), *SourceStr);
        }

        for (const FSkillCondition &C : Conditions)
        {
            if (!C.bTargetSide) continue;
            Desc += FString::Printf(TEXT(" [target: %s]"), *RenderCond(C));
        }

        return Desc;
    }

    /** Render a single payload (type + value/magnitude/drain, duration, target + count). */
    FString DescribePayload(const FSkillEffectPayload &P) const
    {
        const UEnum *StatusEnum = StaticEnum<ESkillEffectType>();
        FString TypeName = StatusEnum ? StatusEnum->GetDisplayNameTextByValue(static_cast<int64>(P.EffectType)).ToString() : TEXT("Unknown");

        FString S;
        const bool bRestore = (P.EffectType == ESkillEffectType::HealthRestore ||
                               P.EffectType == ESkillEffectType::EnergyRestore);
        if (bRestore && P.DrainPercent > 0.0f)
        {
            S = FString::Printf(TEXT("%.0f%% of damage as %s"), P.DrainPercent * 100.0f, *TypeName);
        }
        else if (P.Value != 0)
        {
            S = FString::Printf(TEXT("%s %d"), *TypeName, P.Value);
        }
        else if (P.Magnitude != 0.0f)
        {
            if (SkillEffectClassification::IsBuff(P.EffectType, P.Magnitude) ||
                SkillEffectClassification::IsDebuff(P.EffectType, P.Magnitude))
            {
                S = FString::Printf(TEXT("%s %.0f%%"), *TypeName, P.Magnitude * 100.0f);
            }
            else
            {
                S = FString::Printf(TEXT("%s %.0f"), *TypeName, P.Magnitude);
            }
        }
        else
        {
            S = TypeName;
        }

        if (P.Duration > 0)
        {
            S += FString::Printf(TEXT(" for %d turn%s"), P.Duration, P.Duration > 1 ? TEXT("s") : TEXT(""));
        }

        const UEnum *TargetEnum = StaticEnum<ETargetType>();
        FString TargetName = TargetEnum ? TargetEnum->GetDisplayNameTextByValue(static_cast<int64>(P.Target)).ToString() : TEXT("Unknown");
        const UEnum *CountEnum = StaticEnum<ETargetCount>();
        FString CountName = CountEnum ? CountEnum->GetDisplayNameTextByValue(static_cast<int64>(P.TargetCount)).ToString() : TEXT("?");
        S += FString::Printf(TEXT(" → %s (%s)"), *TargetName, *CountName);

        return S;
    }

    // ==================== SERIALIZATION ====================

    /** Single source of truth for threshold-visibility gating. Sets each
     *  FSkillCondition::bUsesThreshold in Conditions[] so the editor's Threshold
     *  EditCondition stays live. Called by PostSerialize (load/save) and the three
     *  owners' PostEditChangeChainProperty (in-editor edits). */
    void SyncThresholdFlags()
    {
        for (FSkillCondition &C : Conditions)
        {
            C.bUsesThreshold = SkillTriggerUtils::IsThresholdTrigger(C.Trigger);
        }
    }

    /** Syncs threshold-visibility flags on load/save. */
    void PostSerialize(const FArchive &Ar)
    {
        SyncThresholdFlags();
    }
};

template <>
struct TStructOpsTypeTraits<FSkillEffect> : public TStructOpsTypeTraitsBase2<FSkillEffect>
{
    enum
    {
        WithPostSerialize = true,
    };
};
