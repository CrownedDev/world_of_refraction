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
        Effect.MigrateConditionGroup(/*bRebuild=*/true); // B-337e: the group follows the edited old rows
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
                // D-056: the generic Speed Buff / Debuff do nothing.
                Context.AddError(FText::FromString(FString::Printf(
                    TEXT("Effect %d uses %s, which is retired (D-056) and does nothing. Pick a current type."),
                    i, *TypeName)));
                Result = EDataValidationResult::Invalid;
            }
            else if (SkillEffectClassification::IsTurnSpeedFamily(Payload.EffectType))
            {
                // B-170g (Q-170h, Q-170g2): retired with action pips. A warning, not an error, while the speed timeline
                // still reads it (wor.Turns.ActionPips 0); B-170h deletes the reads.
                Context.AddWarning(FText::FromString(FString::Printf(
                    TEXT("Effect %d uses %s: Turn Speed is retired with action pips (B-170) and does nothing once they're on. "
                         "Use Gain / Lose Action or Max Actions Up / Down instead."),
                    i, *TypeName)));
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
            else if (SkillEffectClassification::CountsWholeActions(Payload.EffectType))
            {
                // B-170d (Q-170d2): the action effects count whole actions in Value; Duration is the timing (Gain / Lose:
                // 0 = now, N = the Nth turn start) or the span (Max Actions); a permanent one is refused at runtime.
                if (Payload.Value <= 0)
                {
                    Context.AddWarning(FText::FromString(FString::Printf(
                        TEXT("Effect %d (%s) needs a positive Value (whole actions)%s; it does nothing as authored."), i, *TypeName,
                        Payload.Magnitude > 0.0f ? TEXT(": Magnitude is not read for action effects") : TEXT(""))));
                }
                if (Payload.bPermanent)
                {
                    Context.AddWarning(FText::FromString(FString::Printf(
                        TEXT("Effect %d (%s) is Permanent; action effects can't be permanent and are refused when applied."), i, *TypeName)));
                }
                const bool bMaxActions = !SkillEffectClassification::IsActionChange(Payload.EffectType);
                if (bMaxActions && !Payload.bPermanent && Payload.Duration <= 0)
                {
                    Context.AddWarning(FText::FromString(FString::Printf(
                        TEXT("Effect %d (%s) has Duration 0; a Max Actions change needs the turns it lasts."), i, *TypeName)));
                }
            }
        }
    }

    return Result;
}
#endif
