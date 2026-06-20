// EffectDefinition.cpp

#include "Skills/Effects/EffectDefinition.h"
#include "Skills/Effects/SkillEffectDebug.h"

void UEffectDefinition::LogEffect() const
{
    const FString Label = DisplayName.IsEmpty() ? GetName() : DisplayName.ToString();
    UE_LOG(LogTemp, Display, TEXT("=== UEffectDefinition: %s (Price=%d) ==="), *Label, Price);
    UE_LOG(LogTemp, Display, TEXT("%s"), *SkillEffectDebug::DescribeEffect(Effect));
}
