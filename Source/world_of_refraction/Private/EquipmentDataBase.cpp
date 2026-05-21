// EquipmentDataBase.cpp

#include "EquipmentDataBase.h"
#include "EvolutionItemData.h"
#include "EquipmentBonusGenerator.h"
#include "FPillarWeights.h"
#include "SkillTriggerUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogEquipmentBonusEditor, Log, All);

bool UEquipmentDataBase::IsEvolved() const
{
    return SlottedCrystal && SlottedCrystal->bIsEvolutionCrystal;
}

ESpellElement UEquipmentDataBase::GetCrystalElement() const
{
    if (!SlottedCrystal)
    {
        return ESpellElement::Generic;
    }
    return SlottedCrystal->GetAssociatedElement();
}

void UEquipmentDataBase::RollSubstatPoints()
{
    const int32 Budget = EquipmentBonusGen::GetSubstatBudget(Tier);
    if (SubstatPoints < Budget)
    {
        UE_LOG(LogEquipmentBonusEditor, Warning,
               TEXT("[%s] RollSubstatPoints blocked — insufficient points: SubstatPoints (%d) < tier budget (%d)."),
               *Name, SubstatPoints, Budget);
        return;
    }
    Modify();
    const FPillarWeights EqualWeights;
    GeneratedStatBonus.RerollSubstats(Tier, EqualWeights);
    SubstatPoints = 0;
}

void UEquipmentDataBase::RollPillarPoints()
{
    const float Budget = EquipmentBonusGen::GetPillarBudget(Tier);
    if (PillarPoints < Budget)
    {
        UE_LOG(LogEquipmentBonusEditor, Warning,
               TEXT("[%s] RollPillarPoints blocked — insufficient points: PillarPoints (%.2f) < tier budget (%.2f)."),
               *Name, PillarPoints, Budget);
        return;
    }
    Modify();
    GeneratedStatBonus.RerollPillars(Tier);
    PillarPoints = 0.0f;
}

void UEquipmentDataBase::ClearAllBonuses()
{
    // Clears the generator layer only — BaseStatBonus (designer baseline)
    // is intentionally preserved.
    Modify();
    GeneratedStatBonus = FEquipmentStatBonus();
    SubstatPoints = 0;
    PillarPoints = 0.0f;
}

TArray<FSkillEffect> UEquipmentDataBase::GetAlwaysActiveEffects() const
{
    TArray<FSkillEffect> Result;
    for (const FSkillEffect &Effect : Effects)
    {
        if (Effect.IsAlwaysActive())
        {
            Result.Add(Effect);
        }
    }
    return Result;
}

TArray<FSkillEffect> UEquipmentDataBase::GetTriggeredEffects() const
{
    TArray<FSkillEffect> Result;
    for (const FSkillEffect &Effect : Effects)
    {
        if (!Effect.IsAlwaysActive())
        {
            Result.Add(Effect);
        }
    }
    return Result;
}

FEquipmentStatBonus UEquipmentDataBase::GetCombinedStatBonus() const
{
    FEquipmentStatBonus Combined;
    Combined.BonusRawDamage             = BaseStatBonus.BonusRawDamage             + GeneratedStatBonus.BonusRawDamage;
    Combined.BonusSpellDamage           = BaseStatBonus.BonusSpellDamage           + GeneratedStatBonus.BonusSpellDamage;
    Combined.BonusEfficiency            = BaseStatBonus.BonusEfficiency            + GeneratedStatBonus.BonusEfficiency;
    Combined.BonusStatusMultiplier      = BaseStatBonus.BonusStatusMultiplier      + GeneratedStatBonus.BonusStatusMultiplier;
    Combined.BonusCritChance            = BaseStatBonus.BonusCritChance            + GeneratedStatBonus.BonusCritChance;
    Combined.BonusSpellSpeed            = BaseStatBonus.BonusSpellSpeed            + GeneratedStatBonus.BonusSpellSpeed;
    Combined.BonusDefense               = BaseStatBonus.BonusDefense               + GeneratedStatBonus.BonusDefense;
    Combined.BonusActionSpeed           = BaseStatBonus.BonusActionSpeed           + GeneratedStatBonus.BonusActionSpeed;
    Combined.BonusMaxHP                 = BaseStatBonus.BonusMaxHP                 + GeneratedStatBonus.BonusMaxHP;
    Combined.BonusMaxEnergy             = BaseStatBonus.BonusMaxEnergy             + GeneratedStatBonus.BonusMaxEnergy;
    Combined.BonusResistance            = BaseStatBonus.BonusResistance            + GeneratedStatBonus.BonusResistance;
    Combined.BonusTurnSpeed             = BaseStatBonus.BonusTurnSpeed             + GeneratedStatBonus.BonusTurnSpeed;
    Combined.BonusLuck                  = BaseStatBonus.BonusLuck                  + GeneratedStatBonus.BonusLuck;
    Combined.BonusMindModifierPercent   = BaseStatBonus.BonusMindModifierPercent   + GeneratedStatBonus.BonusMindModifierPercent;
    Combined.BonusBodyModifierPercent   = BaseStatBonus.BonusBodyModifierPercent   + GeneratedStatBonus.BonusBodyModifierPercent;
    Combined.BonusSpiritModifierPercent = BaseStatBonus.BonusSpiritModifierPercent + GeneratedStatBonus.BonusSpiritModifierPercent;
    return Combined;
}

#if WITH_EDITOR
EDataValidationResult UEquipmentDataBase::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    if (Name.IsEmpty())
    {
        Context.AddWarning(FText::FromString(TEXT("Equipment must have a unique name")));
    }

    const int32 MaxSpells = GetMaxSpells();
    if (DefaultSpells.Num() > MaxSpells)
    {
        Context.AddWarning(FText::FromString(FString::Printf(
            TEXT("DefaultSpells (%d) exceeds max (%d)"),
            DefaultSpells.Num(), MaxSpells)));
    }

    return Result;
}

void UEquipmentDataBase::PostEditChangeChainProperty(FPropertyChangedChainEvent &PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);

    // Refresh threshold-visibility flags on every effect so EditCondition gating
    // for ConditionThreshold / SecondaryThreshold / TargetThreshold reacts live
    // to in-editor edits of the matching ESkillTrigger field.
    for (FSkillEffect &Effect : Effects)
    {
        Effect.bConditionUsesThreshold          = SkillTriggerUtils::IsThresholdTrigger(Effect.Condition);
        Effect.bSecondaryConditionUsesThreshold = SkillTriggerUtils::IsThresholdTrigger(Effect.SecondaryCondition);
        Effect.bTargetConditionUsesThreshold    = SkillTriggerUtils::IsThresholdTrigger(Effect.TargetCondition);
    }
}
#endif
