// EffectDefinition.cpp

#include "Skills/Effects/EffectDefinition.h"
#include "Skills/Effects/SkillEffectDebug.h"
#include "Skills/Effects/EffectIdentity.h"
#include "Loadout/LoadoutConstants.h"

void UEffectDefinition::LogEffect() const
{
    const FString Label = DisplayName.IsEmpty() ? GetName() : DisplayName.ToString();
    UE_LOG(LogTemp, Display, TEXT("=== UEffectDefinition: %s (Price=%d, %d effect(s)) ==="),
           *Label, Price, Effects.Num());
    for (int32 i = 0; i < Effects.Num(); ++i)
    {
        UE_LOG(LogTemp, Display, TEXT("  [%d] %s"), i, *SkillEffectDebug::DescribeEffect(Effects[i]));
    }
}

#if WITH_EDITOR
void UEffectDefinition::PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);

    // Each bundled effect owns its threshold-visibility flags; sync them so the
    // ConditionThreshold / SecondaryThreshold / TargetThreshold EditConditions stay live.
    for (FSkillEffect &Effect : Effects)
    {
        Effect.SyncThresholdFlags();
    }
}

EDataValidationResult UEffectDefinition::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Sole stable-ID guard: this bundle's effects index 0..N-1 in its DefID*100 window, so
    // the effect count must stay <= EFFECT_ID_SUBBAND_MAX + 1 (=10) and each effect's
    // payloads <= MAX_PAYLOADS (=9), or packed cast/equipment EffectIDs collide.
    if (Effects.Num() > EffectIdentity::EFFECT_ID_SUBBAND_MAX + 1)
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("Bundle has %d effects; max %d per definition for stable-ID packing."),
            Effects.Num(), EffectIdentity::EFFECT_ID_SUBBAND_MAX + 1)));
        Result = EDataValidationResult::Invalid;
    }

    for (int32 i = 0; i < Effects.Num(); ++i)
    {
        if (Effects[i].Payloads.Num() > LoadoutConstants::MAX_PAYLOADS)
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Effect %d has too many payloads (%d). Maximum is %d."),
                i, Effects[i].Payloads.Num(), LoadoutConstants::MAX_PAYLOADS)));
            Result = EDataValidationResult::Invalid;
        }
    }

    return Result;
}
#endif
