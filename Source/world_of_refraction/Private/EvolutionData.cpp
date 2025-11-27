// EvolutionData.cpp
// Implementation of EvolutionData functions

#include "EvolutionData.h"
#include "CharacterData.h"
#include "UltimateData.h"
#include "SpellData.h"

// ==================== UTILITY FUNCTIONS ====================

FString UEvolutionData::GetElementName() const
{
    const UEnum *EnumPtr = StaticEnum<ESpellElement>();
    if (EnumPtr)
    {
        FString Name = EnumPtr->GetNameStringByValue(static_cast<int64>(Element));
        Name.RemoveFromStart(TEXT("ESpellElement::"));
        return Name;
    }
    return TEXT("Unknown");
}

FString UEvolutionData::GetEvolutionTypeName() const
{
    switch (EvolutionType)
    {
    case EEvolutionType::Positive:
        return TEXT("Positive");
    case EEvolutionType::Cursed:
        return TEXT("Cursed");
    case EEvolutionType::Balanced:
        return TEXT("Balanced");
    case EEvolutionType::Specialist:
        return TEXT("Specialist");
    default:
        return TEXT("Unknown");
    }
}

FString UEvolutionData::GetStatModifierSummary() const
{
    TArray<FString> Modifiers;

    if (StatModifierMode == EStatModifierMode::Pillar)
    {
        if (MindModifierPercent != 0.0f)
        {
            FString Sign = MindModifierPercent > 0 ? TEXT("+") : TEXT("");
            Modifiers.Add(FString::Printf(TEXT("Mind %s%.0f%%"), *Sign, MindModifierPercent));
        }
        if (BodyModifierPercent != 0.0f)
        {
            FString Sign = BodyModifierPercent > 0 ? TEXT("+") : TEXT("");
            Modifiers.Add(FString::Printf(TEXT("Body %s%.0f%%"), *Sign, BodyModifierPercent));
        }
        if (SpiritModifierPercent != 0.0f)
        {
            FString Sign = SpiritModifierPercent > 0 ? TEXT("+") : TEXT("");
            Modifiers.Add(FString::Printf(TEXT("Spirit %s%.0f%%"), *Sign, SpiritModifierPercent));
        }
    }
    else
    {
        // Sub-stat mode - show all non-zero
        auto AddMod = [&](const TCHAR *Name, float Value)
        {
            if (Value != 0.0f)
            {
                FString Sign = Value > 0 ? TEXT("+") : TEXT("");
                Modifiers.Add(FString::Printf(TEXT("%s %s%.0f%%"), Name, *Sign, Value));
            }
        };

        AddMod(TEXT("Cost Reduction"), CostReductionModifierPercent);
        AddMod(TEXT("Turn Speed"), TurnSpeedModifierPercent);
        AddMod(TEXT("Crit Chance"), CritChanceModifierPercent);
        AddMod(TEXT("Defense"), DefenseModifierPercent);
        AddMod(TEXT("Attack Speed"), AttackSpeedModifierPercent);
        AddMod(TEXT("Raw Damage"), RawDamageModifierPercent);
        AddMod(TEXT("Effect Damage"), EffectDamageModifierPercent);
        AddMod(TEXT("Resistance"), ResistanceModifierPercent);
        AddMod(TEXT("Ability Size"), AbilitySizeModifierPercent);
    }

    if (Modifiers.Num() == 0)
    {
        return TEXT("No stat changes");
    }

    return FString::Join(Modifiers, TEXT(", "));
}

// ==================== REQUIREMENT FUNCTIONS ====================

bool UEvolutionData::MeetsElementRequirement(const UCharacterData *Character) const
{
    if (!Character)
    {
        return false;
    }

    // Generic characters CANNOT evolve
    if (Character->InnateElement == ESpellElement::Generic)
    {
        return false;
    }

    // BrokenDarkness can use BrokenDarkness or Darkness evolutions
    if (Character->InnateElement == ESpellElement::BrokenDarkness)
    {
        return (Element == ESpellElement::BrokenDarkness ||
                Element == ESpellElement::Darkness);
    }

    // All other characters require exact element match
    return Character->InnateElement == Element;
}

bool UEvolutionData::CanCharacterUse(const UCharacterData *Character) const
{
    if (!Character)
    {
        return false;
    }

    return MeetsElementRequirement(Character);
}

FString UEvolutionData::GetRequirementsString() const
{
    return FString::Printf(TEXT("Element: %s"), *GetElementName());
}

// ==================== STAT CALCULATION ====================

float UEvolutionData::CalculateModifiedMind(float BaseMind) const
{
    return BaseMind * (1.0f + MindModifierPercent / 100.0f);
}

float UEvolutionData::CalculateModifiedBody(float BaseBody) const
{
    return BaseBody * (1.0f + BodyModifierPercent / 100.0f);
}

float UEvolutionData::CalculateModifiedSpirit(float BaseSpirit) const
{
    return BaseSpirit * (1.0f + SpiritModifierPercent / 100.0f);
}

// ==================== SUB-STAT GETTERS ====================
// Returns the appropriate modifier based on mode

float UEvolutionData::GetCostReductionModifier() const
{
    return (StatModifierMode == EStatModifierMode::Pillar) ? MindModifierPercent : CostReductionModifierPercent;
}

float UEvolutionData::GetTurnSpeedModifier() const
{
    return (StatModifierMode == EStatModifierMode::Pillar) ? MindModifierPercent : TurnSpeedModifierPercent;
}

float UEvolutionData::GetCritChanceModifier() const
{
    return (StatModifierMode == EStatModifierMode::Pillar) ? MindModifierPercent : CritChanceModifierPercent;
}

float UEvolutionData::GetDefenseModifier() const
{
    return (StatModifierMode == EStatModifierMode::Pillar) ? BodyModifierPercent : DefenseModifierPercent;
}

float UEvolutionData::GetAttackSpeedModifier() const
{
    return (StatModifierMode == EStatModifierMode::Pillar) ? BodyModifierPercent : AttackSpeedModifierPercent;
}

float UEvolutionData::GetRawDamageModifier() const
{
    return (StatModifierMode == EStatModifierMode::Pillar) ? BodyModifierPercent : RawDamageModifierPercent;
}

float UEvolutionData::GetEffectDamageModifier() const
{
    return (StatModifierMode == EStatModifierMode::Pillar) ? SpiritModifierPercent : EffectDamageModifierPercent;
}

float UEvolutionData::GetResistanceModifier() const
{
    return (StatModifierMode == EStatModifierMode::Pillar) ? SpiritModifierPercent : ResistanceModifierPercent;
}

float UEvolutionData::GetAbilitySizeModifier() const
{
    return (StatModifierMode == EStatModifierMode::Pillar) ? SpiritModifierPercent : AbilitySizeModifierPercent;
}

// ==================== PASSIVE HELPERS ====================

TArray<FPassiveEffect> UEvolutionData::GetAlwaysActivePassives() const
{
    TArray<FPassiveEffect> Result;

    for (const FPassiveEffect &Passive : PassiveEffects)
    {
        if (Passive.IsAlwaysActive())
        {
            Result.Add(Passive);
        }
    }

    return Result;
}

TArray<FPassiveEffect> UEvolutionData::GetTriggeredPassives() const
{
    TArray<FPassiveEffect> Result;

    for (const FPassiveEffect &Passive : PassiveEffects)
    {
        if (!Passive.IsAlwaysActive())
        {
            Result.Add(Passive);
        }
    }

    return Result;
}

// ==================== VALIDATION ====================

#if WITH_EDITOR
EDataValidationResult UEvolutionData::IsDataValid(TArray<FText> &ValidationErrors)
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Name validation
    if (EvolutionName.IsEmpty() || EvolutionName == TEXT("Unnamed Evolution"))
    {
        ValidationErrors.Add(FText::FromString(TEXT("Evolution must have a unique name")));
        Result = EDataValidationResult::Invalid;
    }

    // Generic element check
    if (Element == ESpellElement::Generic)
    {
        ValidationErrors.Add(FText::FromString(TEXT("Evolutions cannot be Generic element")));
        Result = EDataValidationResult::Invalid;
    }

    // Exclusive spells limit
    if (ExclusiveSpells.Num() > 6)
    {
        ValidationErrors.Add(FText::FromString(TEXT("Evolution cannot have more than 6 exclusive spells")));
        Result = EDataValidationResult::Invalid;
    }

    // Check exclusive spells element matching
    for (int32 i = 0; i < ExclusiveSpells.Num(); ++i)
    {
        if (ExclusiveSpells[i])
        {
            ESpellElement SpellElement = ExclusiveSpells[i]->Element;
            if (SpellElement != Element && SpellElement != ESpellElement::Generic)
            {
                ValidationErrors.Add(FText::FromString(FString::Printf(
                    TEXT("Exclusive spell '%s' has wrong element (%s)"),
                    *ExclusiveSpells[i]->SpellName,
                    *ExclusiveSpells[i]->GetElementName())));
                Result = EDataValidationResult::Invalid;
            }
        }
    }

    // Ultimate override validation
    if (bOverridesUltimate && EvolutionUltimate == nullptr)
    {
        ValidationErrors.Add(FText::FromString(TEXT("Ultimate override enabled but no ultimate assigned")));
        Result = EDataValidationResult::Invalid;
    }

    // Check ultimate element matching
    if (bOverridesUltimate && EvolutionUltimate)
    {
        ESpellElement UltElement = EvolutionUltimate->Element;
        if (UltElement != Element && UltElement != ESpellElement::Generic)
        {
            ValidationErrors.Add(FText::FromString(FString::Printf(
                TEXT("Evolution ultimate '%s' has wrong element (%s)"),
                *EvolutionUltimate->UltimateName,
                *EvolutionUltimate->GetElementName())));
            Result = EDataValidationResult::Invalid;
        }
    }

    // Passive validation
    for (int32 i = 0; i < PassiveEffects.Num(); ++i)
    {
        const FPassiveEffect &Passive = PassiveEffects[i];

        if (Passive.PassiveName.IsEmpty() || Passive.PassiveName == TEXT("Unnamed Passive"))
        {
            ValidationErrors.Add(FText::FromString(FString::Printf(TEXT("Passive %d must have a name"), i + 1)));
            Result = EDataValidationResult::Invalid;
        }

        if (Passive.PrimaryTrigger == EPassiveTrigger::None)
        {
            ValidationErrors.Add(FText::FromString(FString::Printf(TEXT("Passive %d has no trigger set"), i + 1)));
            Result = EDataValidationResult::Invalid;
        }

        if (Passive.EffectType == EPassiveEffectType::None)
        {
            ValidationErrors.Add(FText::FromString(FString::Printf(TEXT("Passive %d has no effect type set"), i + 1)));
            Result = EDataValidationResult::Invalid;
        }
    }

    // Evolution type stat pattern validation (warnings, not errors)
    switch (EvolutionType)
    {
    case EEvolutionType::Positive:
        if (MindModifierPercent < 0 || BodyModifierPercent < 0 || SpiritModifierPercent < 0)
        {
            ValidationErrors.Add(FText::FromString(TEXT("Warning: Positive evolution has negative stat modifiers")));
        }
        break;
    case EEvolutionType::Cursed:
        if (MindModifierPercent >= 0 && BodyModifierPercent >= 0 && SpiritModifierPercent >= 0)
        {
            ValidationErrors.Add(FText::FromString(TEXT("Warning: Cursed evolution has no negative stat modifiers")));
        }
        break;
    default:
        break;
    }

    return Result;
}

void UEvolutionData::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    for (FPassiveEffect &Passive : PassiveEffects)
    {
        Passive.RefreshDescription();
    }
}
#endif