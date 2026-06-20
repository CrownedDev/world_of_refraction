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

    if (!Effect.EffectName.IsEmpty())
    {
        Out += FString::Printf(TEXT("  Name=\"%s\"\n"), *Effect.EffectName);
    }

    // ---- Conditions[] ----
    Out += FString::Printf(TEXT("  Conditions[%d]:\n"), Effect.Conditions.Num());
    for (int32 i = 0; i < Effect.Conditions.Num(); ++i)
    {
        const FSkillCondition &C = Effect.Conditions[i];
        Out += FString::Printf(
            TEXT("    [%d] Trigger=%s Threshold=%.0f Combine=%s Subject=%s\n"),
            i, *EnumName(C.Trigger), C.Threshold, *EnumName(C.Combine),
            *EnumName(C.Subject));
    }

    // ---- Payloads[] ----
    Out += FString::Printf(TEXT("  Payloads[%d]:\n"), Effect.Payloads.Num());
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
