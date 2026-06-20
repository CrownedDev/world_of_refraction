// EffectDefinition.cpp

#include "Skills/Effects/EffectDefinition.h"
#include "Skills/Effects/SkillEffectDebug.h"

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
#endif
