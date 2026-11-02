// EffectDefinition.cpp

#include "Skills/Effects/EffectDefinition.h"
#include "Debug/Skills/SkillEffectDebug.h"
#include "Debug/Skills/EffectTextDebug.h"
#include "WoRLog.h"
#include "Skills/Effects/EffectIdentity.h"
#include "Loadout/LoadoutConstants.h"
#include "Skills/Effects/ESkillEffectType.h"

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

void UEffectDefinition::LogEffectText() const
{
    const FString Label = DisplayName.IsEmpty() ? GetName() : DisplayName.ToString();
    UE_LOG(LogWoREffects, Display, TEXT("=== %s, as the player reads it ===\n%s"), *Label,
           *UEffectTextDebug::GetEffectTextString(EffectText::BuildTree(Effects, EEffectTextSource::Gear),
                                                  EffectText::BuildShortForm(Effects, EEffectTextSource::Gear)));
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
        for (const FSkillEffectPayload &Payload : Effects[i].Payloads)
        {
            const FString TypeName = UEnum::GetValueAsString(Payload.EffectType);
            if (SkillEffectClassification::IsRetired(Payload.EffectType))
            {
                // D-056: the generic Speed Buff / Debuff do nothing; turn order reads the Turn Speed family.
                Context.AddError(FText::FromString(FString::Printf(
                    TEXT("Effect %d uses %s, which is retired (D-056) and does nothing. Use TurnSpeedBuff / TurnSpeedDebuff."),
                    i, *TypeName)));
                Result = EDataValidationResult::Invalid;
            }
            else if (SkillEffectClassification::HasNoReader(Payload.EffectType))
            {
                Context.AddWarning(FText::FromString(FString::Printf(
                    TEXT("Effect %d uses legacy %s, which nothing reads; it does nothing. Pick a current type."), i, *TypeName)));
            }
            else if (SkillEffectClassification::ResolveLegacyAlias(Payload.EffectType) != Payload.EffectType)
            {
                Context.AddWarning(FText::FromString(FString::Printf(
                    TEXT("Effect %d uses legacy %s; it works as %s. Pick that type instead."), i, *TypeName,
                    *UEnum::GetValueAsString(SkillEffectClassification::ResolveLegacyAlias(Payload.EffectType)))));
            }
        }
    }

    return Result;
}
#endif
