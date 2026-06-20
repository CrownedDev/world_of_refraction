// SkillEffectDebug.cpp

#include "Skills/Effects/SkillEffectDebug.h"
#include "Skills/Effects/FSkillEffect.h"

namespace
{
    // Mirrors FSkillEffect::GetDescription's StaticEnum usage so it compiles wherever
    // FSkillEffect.h does. Returns the raw enumerator name for the value.
    template <typename TEnum>
    FString EnumName(TEnum Value)
    {
        const UEnum *E = StaticEnum<TEnum>();
        return E ? E->GetNameStringByValue(static_cast<int64>(Value)) : TEXT("?");
    }
}

FString SkillEffectDebug::DescribeEffect(const FSkillEffect &Effect)
{
    FString Out = TEXT("FSkillEffect:\n");

    // ---- Legacy flat fields ----
    Out += FString::Printf(
        TEXT("  [legacy] EffectType=%s Magnitude=%.3f Value=%d Duration=%d Target=%s TargetCount=%s\n"),
        *EnumName(Effect.EffectType), Effect.Magnitude, Effect.Value, Effect.Duration,
        *EnumName(Effect.Target), *EnumName(Effect.TargetCount));

    if (!Effect.EffectName.IsEmpty())
    {
        Out += FString::Printf(TEXT("           Name=\"%s\"\n"), *Effect.EffectName);
    }

    Out += FString::Printf(
        TEXT("           Condition=%s(%.0f) Secondary=%s(%.0f) RequireBoth=%s TargetCond=%s(%.0f) DrainPercent=%.2f\n"),
        *EnumName(Effect.Condition), Effect.ConditionThreshold,
        *EnumName(Effect.SecondaryCondition), Effect.SecondaryThreshold,
        Effect.bRequireBothConditions ? TEXT("true") : TEXT("false"),
        *EnumName(Effect.TargetCondition), Effect.TargetThreshold,
        Effect.DrainPercent);

    // ---- New Conditions[] ----
    Out += FString::Printf(TEXT("  [new] Conditions[%d]:\n"), Effect.Conditions.Num());
    for (int32 i = 0; i < Effect.Conditions.Num(); ++i)
    {
        const FSkillCondition &C = Effect.Conditions[i];
        Out += FString::Printf(
            TEXT("    [%d] Trigger=%s Threshold=%.0f Combine=%s TargetSide=%s\n"),
            i, *EnumName(C.Trigger), C.Threshold, *EnumName(C.Combine),
            C.bTargetSide ? TEXT("true") : TEXT("false"));
    }

    // ---- New Payloads[] ----
    Out += FString::Printf(TEXT("  [new] Payloads[%d]:\n"), Effect.Payloads.Num());
    for (int32 i = 0; i < Effect.Payloads.Num(); ++i)
    {
        const FSkillEffectPayload &P = Effect.Payloads[i];
        Out += FString::Printf(
            TEXT("    [%d] EffectType=%s Magnitude=%.3f Value=%d Duration=%d Target=%s TargetCount=%s DrainPercent=%.2f\n"),
            i, *EnumName(P.EffectType), P.Magnitude, P.Value, P.Duration,
            *EnumName(P.Target), *EnumName(P.TargetCount), P.DrainPercent);
    }

    return Out;
}

void SkillEffectDebug::LogEffect(const FSkillEffect &Effect)
{
    UE_LOG(LogTemp, Display, TEXT("%s"), *DescribeEffect(Effect));
}

bool SkillEffectDebug::CompareEffect(const FSkillEffect &Effect, FString &OutReport)
{
    // Legacy view: new arrays empty -> folded classifiers fall back to the legacy fields.
    FSkillEffect LegacyOnly = Effect;
    LegacyOnly.Conditions.Empty();
    LegacyOnly.Payloads.Empty();

    // New view: migrate from legacy via the SAME code PostSerialize runs, then clear the
    // legacy scalars so the folded classifiers read ONLY the migrated arrays.
    FSkillEffect NewOnly = Effect;
    NewOnly.Conditions.Empty();
    NewOnly.Payloads.Empty();
    NewOnly.MigrateLegacyToNew();
    NewOnly.EffectType = ESkillEffectType::None;
    NewOnly.Magnitude = 0.0f;
    NewOnly.DrainPercent = 0.0f;
    NewOnly.Condition = ESkillTrigger::Always;
    NewOnly.SecondaryCondition = ESkillTrigger::None;
    NewOnly.TargetCondition = ESkillTrigger::None;

    bool bOk = true;
    FString Diff;

    auto Check = [&](const TCHAR *Name, bool A, bool B)
    {
        if (A != B)
        {
            bOk = false;
            Diff += FString::Printf(TEXT("  MISMATCH %s: legacy=%s new=%s\n"),
                                    Name, A ? TEXT("T") : TEXT("F"), B ? TEXT("T") : TEXT("F"));
        }
    };

    Check(TEXT("IsValid"), LegacyOnly.IsValid(), NewOnly.IsValid());
    Check(TEXT("IsBuff"), LegacyOnly.IsBuff(), NewOnly.IsBuff());
    Check(TEXT("IsDebuff"), LegacyOnly.IsDebuff(), NewOnly.IsDebuff());
    Check(TEXT("IsRestore"), LegacyOnly.IsRestore(), NewOnly.IsRestore());
    Check(TEXT("IsDrain"), LegacyOnly.IsDrain(), NewOnly.IsDrain());
    Check(TEXT("IsConditional"), LegacyOnly.IsConditional(), NewOnly.IsConditional());
    Check(TEXT("IsConditionalEffect"), LegacyOnly.IsConditionalEffect(), NewOnly.IsConditionalEffect());

    // Expected counts from the legacy fields — mirrors MigrateLegacyToNew's rules.
    const bool bSkipPrimary = (Effect.Condition == ESkillTrigger::Always &&
                               Effect.SecondaryCondition == ESkillTrigger::None &&
                               Effect.TargetCondition == ESkillTrigger::None);
    int32 ExpectedConds = 0;
    if (!bSkipPrimary) ++ExpectedConds;
    if (Effect.SecondaryCondition != ESkillTrigger::None) ++ExpectedConds;
    if (Effect.TargetCondition != ESkillTrigger::None) ++ExpectedConds;
    const int32 ExpectedPayloads = (Effect.EffectType != ESkillEffectType::None) ? 1 : 0;

    FSkillEffect Migrated = Effect;
    Migrated.Conditions.Empty();
    Migrated.Payloads.Empty();
    Migrated.MigrateLegacyToNew();

    if (Migrated.Conditions.Num() != ExpectedConds)
    {
        bOk = false;
        Diff += FString::Printf(TEXT("  MISMATCH Conditions count: expected=%d actual=%d\n"),
                                ExpectedConds, Migrated.Conditions.Num());
    }
    if (Migrated.Payloads.Num() != ExpectedPayloads)
    {
        bOk = false;
        Diff += FString::Printf(TEXT("  MISMATCH Payloads count: expected=%d actual=%d\n"),
                                ExpectedPayloads, Migrated.Payloads.Num());
    }

    // Eval parity (AND/OR combining). Only meaningful when a secondary source condition
    // exists (two source-side conditions) — that's where the legacy 2-field AND/OR and the
    // N-condition partition rule could diverge (the OR-as-AND bug). Drives the migrated
    // condition group through the partition rule for all 4 (P,S) trigger states and
    // compares to the legacy formula (bRequireBothConditions ? P&&S : P||S). No Actor needed:
    // the per-condition 'met' values are fed directly.
    if (Effect.SecondaryCondition != ESkillTrigger::None)
    {
        TArray<FSkillCondition> SourceConds;
        for (const FSkillCondition &C : Migrated.Conditions)
        {
            if (!C.bTargetSide) SourceConds.Add(C);
        }

        // Mirrors SkillEffectManager::IsTriggerConditionMet's partition rule. Keep in sync.
        // [0] = primary -> MetP, [1] = secondary -> MetS.
        auto EvalNew = [&SourceConds](bool MetP, bool MetS) -> bool
        {
            bool bAllAndMet = true;
            bool bAnyOr = false;
            bool bAnyOrMet = false;
            for (int32 i = 0; i < SourceConds.Num(); ++i)
            {
                const bool bMet = (i == 0) ? MetP : MetS;
                if (SourceConds[i].Combine == ECondCombine::And)
                {
                    bAllAndMet = bAllAndMet && bMet;
                }
                else
                {
                    bAnyOr = true;
                    bAnyOrMet = bAnyOrMet || bMet;
                }
            }
            return bAllAndMet && (!bAnyOr || bAnyOrMet);
        };

        const bool bReqBoth = Effect.bRequireBothConditions;
        const bool States[4][2] = {{false, false}, {true, false}, {false, true}, {true, true}};
        for (int32 s = 0; s < 4; ++s)
        {
            const bool P = States[s][0];
            const bool S = States[s][1];
            const bool bLegacy = bReqBoth ? (P && S) : (P || S);
            const bool bNew = EvalNew(P, S);
            if (bLegacy != bNew)
            {
                bOk = false;
                Diff += FString::Printf(TEXT("  MISMATCH eval(P=%s,S=%s): legacy=%s new=%s\n"),
                                        P ? TEXT("T") : TEXT("F"), S ? TEXT("T") : TEXT("F"),
                                        bLegacy ? TEXT("T") : TEXT("F"), bNew ? TEXT("T") : TEXT("F"));
            }
        }
    }

    if (bOk)
    {
        OutReport = TEXT("OK - legacy/new classifiers, counts, and eval match.\n");
    }
    else
    {
        OutReport = FString::Printf(TEXT("PARITY FAIL:\n%s%s"), *Diff, *DescribeEffect(Effect));
    }
    return bOk;
}
