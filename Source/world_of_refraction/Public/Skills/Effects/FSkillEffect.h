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

    // ==================== EFFECT TYPE ====================

    /** What effect to apply (buff, debuff, restore, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    ESkillEffectType EffectType = ESkillEffectType::None;

    // ==================== MAGNITUDE ====================

    /**
     * Effect strength:
     * - For buffs/debuffs: Percentage as decimal (0.2 = 20%)
     * - For restore/drain: Flat value OR use DrainPercent
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect",
              meta = (ClampMin = "0.0"))
    float Magnitude = 0.0f;

    /**
     * Flat value, used for absolute amounts (e.g. 30 = 30 HP per turn for DOT).
     * Distinct from Magnitude, which is decimal percent (0.2 = 20%).
     * Authors: use Value for flat amounts (DOT damage, heal HP), use Magnitude
     * for percentages (buff/debuff %). Don't set both on the same effect.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect", meta = (ClampMin = "0"))
    int32 Value = 0;

    /** Duration in turns (0 = instant effect like heal/damage) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect",
              meta = (ClampMin = "0"))
    int32 Duration = 0;

    // ==================== TARGETING ====================

    /** Who receives this effect */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    ETargetType Target = ETargetType::Enemy;

    /** How many recipients of the Target role this effect hits. Single = 1,
     *  Double = exactly 2 (auto-resolved for passive effects), All = every valid
     *  recipient. Authored per-effect; re-authored by hand on legacy assets. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
    ETargetCount TargetCount = ETargetCount::Single;

    // ==================== CONDITION ====================

    /** Optional display name for passive-style effects shown in UI. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FString EffectName = TEXT("");

    /** When does this effect trigger (source-side condition). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
    ESkillTrigger Condition = ESkillTrigger::Always;

    /** Auto-set by PostSerialize — true when Condition uses a threshold value.
     *  Drives EditCondition gating for ConditionThreshold. Do not edit manually. */
    UPROPERTY()
    bool bConditionUsesThreshold = false;

    /** HP or Energy % threshold for the source trigger (0..100). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger",
              meta = (EditCondition = "bConditionUsesThreshold",
                      EditConditionHides, ClampMin = "0.0", ClampMax = "100.0"))
    float ConditionThreshold = 30.0f;

    /** Secondary source-side condition, combined with Condition via AND/OR. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger",
              meta = (EditCondition = "Condition != ESkillTrigger::None && Condition != ESkillTrigger::Always",
                      EditConditionHides))
    ESkillTrigger SecondaryCondition = ESkillTrigger::None;

    /** Auto-set by PostSerialize — true when SecondaryCondition uses a threshold. */
    UPROPERTY()
    bool bSecondaryConditionUsesThreshold = false;

    /** HP or Energy % threshold for the secondary source trigger (0..100). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger",
              meta = (EditCondition = "bSecondaryConditionUsesThreshold",
                      EditConditionHides, ClampMin = "0.0", ClampMax = "100.0"))
    float SecondaryThreshold = 30.0f;

    /** AND = both source conditions must hold. OR = either is enough. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger",
              meta = (EditCondition = "SecondaryCondition != ESkillTrigger::None",
                      EditConditionHides))
    bool bRequireBothConditions = true;

    /** Condition that must hold on the TARGET for the effect to apply. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
    ESkillTrigger TargetCondition = ESkillTrigger::None;

    /** Auto-set by PostSerialize — true when TargetCondition uses a threshold. */
    UPROPERTY()
    bool bTargetConditionUsesThreshold = false;

    /** HP or Energy % threshold for the target condition (0..100). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger",
              meta = (EditCondition = "bTargetConditionUsesThreshold",
                      EditConditionHides, ClampMin = "0.0", ClampMax = "100.0"))
    float TargetThreshold = 100.0f;

    // ==================== DRAIN ====================

    /**
     * For drain effects (HealthRestore/EnergyRestore with OnHit):
     * Percentage of damage dealt that converts to restore (0.3 = 30%)
     * Only used when Condition is OnHit and EffectType is a restore type
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drain",
              meta = (ClampMin = "0.0", ClampMax = "1.0",
                      EditCondition = "Condition == ESkillTrigger::OnHit"))
    float DrainPercent = 0.0f;

    // ==================== DYNAMIC (Cluster A — additive, not yet read) ====================

    /** New condition group: per-entry AND/OR + source/target side (FSkillCondition).
     *  Cluster A only — no runtime path reads this yet; migration populates it later. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
    TArray<FSkillCondition> Conditions;

    /** New payload list: one effect may carry several payloads (FSkillEffectPayload).
     *  Cluster A only — no runtime path reads this yet; migration populates it later. */
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

    FSkillEffect(ESkillEffectType InType, float InMagnitude, int32 InValue, int32 InDuration,
                 ETargetType InTarget, ESkillTrigger InCondition)
        : EffectType(InType), Magnitude(InMagnitude), Value(InValue), Duration(InDuration), Target(InTarget), Condition(InCondition), DrainPercent(0.0f)
    {
    }

    // ==================== HELPERS ====================

    // Cluster B: every classifier folds over the new Payloads[]/Conditions[] arrays
    // first, then ORs the legacy flat-field result. While the arrays are empty
    // (not-yet-migrated in-memory effects) the legacy fields stay authoritative;
    // once migrated they mirror the legacy fields, so the OR is consistent. Legacy
    // remains the runtime source of truth until Cluster C/D repoints the readers.

    /** Is this effect valid? Any payload typed (new) or the legacy EffectType typed. */
    bool IsValid() const
    {
        for (const FSkillEffectPayload &P : Payloads)
        {
            if (P.EffectType != ESkillEffectType::None) return true;
        }
        return EffectType != ESkillEffectType::None;
    }

    /** Buff if ANY payload classifies as a buff (new) or the legacy fields do. */
    bool IsBuff() const
    {
        for (const FSkillEffectPayload &P : Payloads)
        {
            if (SkillEffectClassification::IsBuff(P.EffectType, P.Magnitude)) return true;
        }
        return SkillEffectClassification::IsBuff(EffectType, Magnitude);
    }

    /** Debuff if ANY payload classifies as a debuff (new) or the legacy fields do. */
    bool IsDebuff() const
    {
        for (const FSkillEffectPayload &P : Payloads)
        {
            if (SkillEffectClassification::IsDebuff(P.EffectType, P.Magnitude)) return true;
        }
        return SkillEffectClassification::IsDebuff(EffectType, Magnitude);
    }

    /** Restore if ANY payload is Health/EnergyRestore (new) or the legacy field is. */
    bool IsRestore() const
    {
        for (const FSkillEffectPayload &P : Payloads)
        {
            if (P.EffectType == ESkillEffectType::HealthRestore ||
                P.EffectType == ESkillEffectType::EnergyRestore) return true;
        }
        return EffectType == ESkillEffectType::HealthRestore ||
               EffectType == ESkillEffectType::EnergyRestore;
    }

    /** Drain: a restore payload with DrainPercent>0 AND some source-side OnHit
     *  condition (new), OR the legacy (restore && DrainPercent>0 && Condition==OnHit). */
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
        const bool bNew = bDrainPayload && bOnHitSource;

        const bool bLegacyRestore = (EffectType == ESkillEffectType::HealthRestore ||
                                     EffectType == ESkillEffectType::EnergyRestore);
        const bool bLegacy = bLegacyRestore && DrainPercent > 0.0f && Condition == ESkillTrigger::OnHit;

        return bNew || bLegacy;
    }

    /** Is this effect instant (Duration == 0)? */
    bool IsInstant() const
    {
        // TODO(Cluster C): Duration is per-payload once Payloads[] is authoritative —
        // fold over Payloads then. Legacy single-Duration meaning retained for now.
        return Duration == 0;
    }

    /** Conditional if the new Conditions[] is non-empty, else the legacy check.
     *  (Empty Conditions[] is the C-side contract for "Always / unconditional".) */
    bool IsConditional() const
    {
        if (Conditions.Num() > 0) return true;
        return Condition != ESkillTrigger::Always
            || SecondaryCondition != ESkillTrigger::None
            || TargetCondition != ESkillTrigger::None;
    }

    /** Carries a target-side condition: any bTargetSide entry (new) or legacy TargetCondition. */
    bool HasTargetCondition() const
    {
        for (const FSkillCondition &C : Conditions)
        {
            if (C.bTargetSide) return true;
        }
        return TargetCondition != ESkillTrigger::None;
    }

    /** Carries a secondary source-side condition: 2+ source-side entries (new) or legacy SecondaryCondition. */
    bool HasSecondaryCondition() const
    {
        int32 SourceConds = 0;
        for (const FSkillCondition &C : Conditions)
        {
            if (!C.bTargetSide) ++SourceConds;
        }
        if (SourceConds >= 2) return true;
        return SecondaryCondition != ESkillTrigger::None;
    }

    /** Has a trigger condition (source-side non-Always or target-side). The complement
     *  is a STARTING effect: gear effects with no condition apply once at combat start
     *  (GetStartingEffects), then live as normal clearable effects. NOTE: distinct from
     *  IsConditional() above, which additionally counts SecondaryCondition — a
     *  secondary-condition-only effect is conditional there but still a starting
     *  effect here.
     *  New equiv: any source-side Trigger!=Always, or any target-side entry; empty
     *  Conditions[] falls back to the legacy (Condition!=Always || TargetCondition!=None). */
    bool IsConditionalEffect() const
    {
        for (const FSkillCondition &C : Conditions)
        {
            if (C.bTargetSide) return true;
            if (C.Trigger != ESkillTrigger::Always) return true;
        }
        return Condition != ESkillTrigger::Always
            || TargetCondition != ESkillTrigger::None;
    }

    /** Does this effect target self? */
    bool TargetsSelf() const
    {
        return Target == ETargetType::Self;
    }

    /** Does this effect target allies? */
    bool TargetsAllies() const
    {
        return Target == ETargetType::Ally;
    }

    /** Does this effect target enemies? */
    bool TargetsEnemies() const
    {
        return Target == ETargetType::Enemy;
    }

    // ==================== DEBUG ====================

    /** Get a description string for debug/UI. Renders the new Conditions[]/Payloads[]
     *  shape when either is populated; otherwise falls back to the legacy renderer so
     *  not-yet-migrated effects still read out exactly as before. */
    FString GetDescription() const
    {
        if (Conditions.Num() == 0 && Payloads.Num() == 0)
        {
            return GetLegacyDescription();
        }

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

    /** The pre-Cluster-B renderer over the legacy flat fields. Used as the fallback
     *  by GetDescription when the new arrays are empty. Unchanged behaviour. */
    FString GetLegacyDescription() const
    {
        if (!IsValid())
        {
            return TEXT("No Effect");
        }

        FString Desc;

        // Optional named-passive prefix.
        if (!EffectName.IsEmpty())
        {
            Desc = EffectName + TEXT(": ");
        }

        // Effect type and magnitude
        const UEnum *StatusEnum = StaticEnum<ESkillEffectType>();
        FString TypeName = StatusEnum ? StatusEnum->GetDisplayNameTextByValue(static_cast<int64>(EffectType)).ToString() : TEXT("Unknown");

        if (IsDrain())
        {
            Desc += FString::Printf(TEXT("%.0f%% of damage as %s"),
                                    DrainPercent * 100.0f,
                                    *TypeName);
        }
        else if (Value != 0)
        {
            Desc += FString::Printf(TEXT("%s %d"), *TypeName, Value);
        }
        else if (Magnitude != 0.0f)
        {
            if (IsBuff() || IsDebuff())
            {
                Desc += FString::Printf(TEXT("%s %.0f%%"), *TypeName, Magnitude * 100.0f);
            }
            else
            {
                Desc += FString::Printf(TEXT("%s %.0f"), *TypeName, Magnitude);
            }
        }
        else
        {
            Desc += TypeName;
        }

        // Duration
        if (Duration > 0)
        {
            Desc += FString::Printf(TEXT(" for %d turn%s"),
                                    Duration,
                                    Duration > 1 ? TEXT("s") : TEXT(""));
        }

        // Target
        const UEnum *TargetEnum = StaticEnum<ETargetType>();
        FString TargetName = TargetEnum ? TargetEnum->GetDisplayNameTextByValue(static_cast<int64>(Target)).ToString() : TEXT("Unknown");
        Desc += FString::Printf(TEXT(" → %s"), *TargetName);

        // Condition (source-side)
        const UEnum *TriggerEnum = StaticEnum<ESkillTrigger>();
        if (Condition != ESkillTrigger::Always)
        {
            FString ConditionName = TriggerEnum ? TriggerEnum->GetDisplayNameTextByValue(static_cast<int64>(Condition)).ToString() : TEXT("Unknown");
            if (SkillTriggerUtils::IsThresholdTrigger(Condition))
            {
                Desc += FString::Printf(TEXT(" (%s %.0f%%)"), *ConditionName, ConditionThreshold);
            }
            else
            {
                Desc += FString::Printf(TEXT(" (%s)"), *ConditionName);
            }
        }

        // Secondary source-side condition, joined by AND / OR
        if (SecondaryCondition != ESkillTrigger::None)
        {
            FString SecondaryName = TriggerEnum ? TriggerEnum->GetDisplayNameTextByValue(static_cast<int64>(SecondaryCondition)).ToString() : TEXT("Unknown");
            const TCHAR *Joiner = bRequireBothConditions ? TEXT("AND") : TEXT("OR");
            if (SkillTriggerUtils::IsThresholdTrigger(SecondaryCondition))
            {
                Desc += FString::Printf(TEXT(" %s (%s %.0f%%)"), Joiner, *SecondaryName, SecondaryThreshold);
            }
            else
            {
                Desc += FString::Printf(TEXT(" %s (%s)"), Joiner, *SecondaryName);
            }
        }

        // Target-side condition
        if (TargetCondition != ESkillTrigger::None)
        {
            FString TargetCondName = TriggerEnum ? TriggerEnum->GetDisplayNameTextByValue(static_cast<int64>(TargetCondition)).ToString() : TEXT("Unknown");
            if (SkillTriggerUtils::IsThresholdTrigger(TargetCondition))
            {
                Desc += FString::Printf(TEXT(" [target: %s %.0f%%]"), *TargetCondName, TargetThreshold);
            }
            else
            {
                Desc += FString::Printf(TEXT(" [target: %s]"), *TargetCondName);
            }
        }

        return Desc;
    }

    // ==================== SERIALIZATION ====================

    /** Single source of truth for threshold-visibility gating. Sets the legacy 3
     *  b*UsesThreshold flags AND each FSkillCondition::bUsesThreshold in Conditions[],
     *  so the editor's Threshold EditCondition stays live on both shapes. Called by
     *  PostSerialize (load/save), MigrateLegacyToNew (after populating), and the three
     *  owners' PostEditChangeChainProperty (in-editor edits). */
    void SyncThresholdFlags()
    {
        // (a) legacy 3-flag sync.
        // TODO(cluster-f): drop the legacy flag sync when the legacy condition fields are removed.
        bConditionUsesThreshold = SkillTriggerUtils::IsThresholdTrigger(Condition);
        bSecondaryConditionUsesThreshold = SkillTriggerUtils::IsThresholdTrigger(SecondaryCondition);
        bTargetConditionUsesThreshold = SkillTriggerUtils::IsThresholdTrigger(TargetCondition);

        // (b) per-Conditions[] sync — the new shape (closes the in-editor stale-gating gap).
        for (FSkillCondition &C : Conditions)
        {
            C.bUsesThreshold = SkillTriggerUtils::IsThresholdTrigger(C.Trigger);
        }
    }

    /** Syncs threshold-visibility flags (legacy + Conditions[]) then mirrors the legacy
     *  flat fields into the new Conditions[]/Payloads[] arrays on load. The migration is
     *  a no-op once the asset is authored/re-saved in the new shape (guarded on both
     *  arrays being empty), so authored new data is never clobbered. */
    void PostSerialize(const FArchive &Ar)
    {
        SyncThresholdFlags();
        MigrateLegacyToNew();
    }

    /** Populates Conditions[]/Payloads[] from the legacy flat fields. Runs ONLY when
     *  both arrays are empty, so it never overwrites data authored in the new shape.
     *  Mapping (must stay identical to SkillEffectDebug::CompareEffect's expectations):
     *   - Primary source condition { Condition, ConditionThreshold, And, source } — UNLESS
     *     the effect is fully unconditional (Condition==Always && no secondary && no target),
     *     in which case Conditions stays empty (the C-side contract: empty == Always).
     *   - Secondary source condition (only if SecondaryCondition!=None), Combine from
     *     bRequireBothConditions (And/Or).
     *   - Target condition (only if TargetCondition!=None), And, target-side.
     *   - One payload { EffectType, Magnitude, Value, Duration, Target, TargetCount,
     *     DrainPercent } — only if EffectType!=None. */
    void MigrateLegacyToNew()
    {
        if (Conditions.Num() != 0 || Payloads.Num() != 0)
        {
            return;
        }

        const bool bSkipPrimary = (Condition == ESkillTrigger::Always
                                   && SecondaryCondition == ESkillTrigger::None
                                   && TargetCondition == ESkillTrigger::None);
        if (!bSkipPrimary)
        {
            FSkillCondition Primary;
            Primary.Trigger = Condition;
            Primary.Threshold = ConditionThreshold;
            // Primary inherits the group's AND/OR (same as the secondary) so the
            // partition-rule evaluator reproduces legacy: OR -> both Or (And-set empty,
            // result = P||S); AND -> both And (result = P&&S). For a lone primary the
            // choice is inert (single And or single Or both yield 'met').
            Primary.Combine = bRequireBothConditions ? ECondCombine::And : ECondCombine::Or;
            Primary.bTargetSide = false;
            Conditions.Add(Primary);
        }
        if (SecondaryCondition != ESkillTrigger::None)
        {
            FSkillCondition Secondary;
            Secondary.Trigger = SecondaryCondition;
            Secondary.Threshold = SecondaryThreshold;
            Secondary.Combine = bRequireBothConditions ? ECondCombine::And : ECondCombine::Or;
            Secondary.bTargetSide = false;
            Conditions.Add(Secondary);
        }
        if (TargetCondition != ESkillTrigger::None)
        {
            FSkillCondition TargetCond;
            TargetCond.Trigger = TargetCondition;
            TargetCond.Threshold = TargetThreshold;
            TargetCond.Combine = ECondCombine::And;
            TargetCond.bTargetSide = true;
            Conditions.Add(TargetCond);
        }

        if (EffectType != ESkillEffectType::None)
        {
            FSkillEffectPayload Payload;
            Payload.EffectType = EffectType;
            Payload.Magnitude = Magnitude;
            Payload.Value = Value;
            Payload.Duration = Duration;
            Payload.Target = Target;
            Payload.TargetCount = TargetCount;
            Payload.DrainPercent = DrainPercent;
            Payloads.Add(Payload);
        }

        // Single source of truth: set bUsesThreshold on the just-populated Conditions[]
        // (and re-affirm the legacy flags) instead of per-entry inline.
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
