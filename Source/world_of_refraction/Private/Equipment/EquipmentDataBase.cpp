// EquipmentDataBase.cpp

#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/EquipmentBonusGenerator.h"
#include "Character/FPillarWeights.h"
#include "Skills/Effects/SkillTriggerUtils.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogEquipmentBonusEditor, Log, All);

bool UEquipmentDataBase::IsEvolved() const
{
    return AttachedItem.Kind == EAttachedItemKind::Evolution;
}

ESpellElement UEquipmentDataBase::GetCrystalElement() const
{
    switch (AttachedItem.Kind)
    {
    case EAttachedItemKind::Crystal:
        return CrystalTypeHelpers::GetElement(AttachedItem.CrystalType);
    case EAttachedItemKind::Evolution:
        return AttachedItem.Evolution ? AttachedItem.Evolution->GetAssociatedElement()
                                      : ESpellElement::Generic;
    case EAttachedItemKind::Fusion:
        // Mirror the runtime GetElement gem-half logic on the flat authored fields,
        // for design-time/preview parity. The gem half drives the element; an
        // augmented fusion (two stones, no gem) is Generic. Check both halves.
        if (CrystalTypeHelpers::IsGemType(AttachedItem.FusionHalfAType))
        {
            return CrystalTypeHelpers::GetElement(AttachedItem.FusionHalfAType);
        }
        if (CrystalTypeHelpers::IsGemType(AttachedItem.FusionHalfBType))
        {
            return CrystalTypeHelpers::GetElement(AttachedItem.FusionHalfBType);
        }
        return ESpellElement::Generic;
    case EAttachedItemKind::None:
    default:
        return ESpellElement::Generic;
    }
}

TArray<FString> UEquipmentDataBase::GetRestrictedCrystalTypes() const
{
    TArray<FString> Restricted;

    const bool bRestrictStones = (AttachedItem.Kind == EAttachedItemKind::Crystal);
    const bool bRestrictGems   = (AttachedItem.Kind == EAttachedItemKind::AugmentStone);
    if (!bRestrictStones && !bRestrictGems)
    {
        // Kind None / Evolution — no crystal-sub-type dropdown to constrain.
        return Restricted;
    }

    const UEnum *Enum = StaticEnum<ECrystalType>();
    if (!Enum)
    {
        return Restricted;
    }

    // Build the grey-out set from the CrystalTypeHelpers predicates so it can
    // never drift from the gem/stone split. NumEnums() - 1 skips the implicit
    // _MAX sentinel. GetNameStringByIndex returns the short value name
    // ("Garnet"), which GetRestrictedEnumValues re-expands to the full name.
    for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
    {
        const ECrystalType Type = static_cast<ECrystalType>(Enum->GetValueByIndex(Index));
        const bool bRestrictThis =
            (bRestrictStones && CrystalTypeHelpers::IsAugmentStoneType(Type)) ||
            (bRestrictGems && CrystalTypeHelpers::IsGemType(Type));
        if (bRestrictThis)
        {
            Restricted.Add(Enum->GetNameStringByIndex(Index));
        }
    }
    return Restricted;
}

TArray<FString> UEquipmentDataBase::GetRestrictedFusionHalfATypes() const
{
    TArray<FString> Restricted;

    const UEnum *Enum = StaticEnum<ECrystalType>();
    if (!Enum)
    {
        return Restricted;
    }

    // Half A is augment-stone only — grey every gem. Same loop shape as
    // GetRestrictedCrystalTypes; NumEnums() - 1 skips the implicit _MAX sentinel.
    for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
    {
        const ECrystalType Type = static_cast<ECrystalType>(Enum->GetValueByIndex(Index));
        if (CrystalTypeHelpers::IsGemType(Type))
        {
            Restricted.Add(Enum->GetNameStringByIndex(Index));
        }
    }
    return Restricted;
}

TArray<FString> UEquipmentDataBase::GetRestrictedFusionHalfBTypes() const
{
    TArray<FString> Restricted;

    const UEnum *Enum = StaticEnum<ECrystalType>();
    if (!Enum)
    {
        return Restricted;
    }

    // Keyed off the sibling kind selector (mirrors GetRestrictedCrystalTypes reading
    // AttachedItem.Kind): crystal half -> grey stones, stone half -> grey gems.
    const bool bRestrictStones = AttachedItem.bFusionHalfBIsCrystal;
    const bool bRestrictGems   = !AttachedItem.bFusionHalfBIsCrystal;
    for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
    {
        const ECrystalType Type = static_cast<ECrystalType>(Enum->GetValueByIndex(Index));
        const bool bRestrictThis =
            (bRestrictStones && CrystalTypeHelpers::IsAugmentStoneType(Type)) ||
            (bRestrictGems && CrystalTypeHelpers::IsGemType(Type));
        if (bRestrictThis)
        {
            Restricted.Add(Enum->GetNameStringByIndex(Index));
        }
    }
    return Restricted;
}

TArray<FString> UEquipmentDataBase::GetRestrictedFusionBonusStats() const
{
    TArray<FString> Restricted;

    const UEnum *Enum = StaticEnum<ESubStat>();
    if (!Enum)
    {
        return Restricted;
    }

    // Only the wired six are queried by a read-site (they are exactly the non-None
    // outputs of CrystalEffectTable::StoneTargetStat). Grey everything else (and None)
    // so a fusion bonus can never target an inert sub-stat.
    auto IsWired = [](ESubStat S)
    {
        return S == ESubStat::RawDamage || S == ESubStat::Defense ||
               S == ESubStat::CritChance || S == ESubStat::TurnSpeed ||
               S == ESubStat::StatusMultiplier || S == ESubStat::Efficiency;
    };
    for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
    {
        const ESubStat Stat = static_cast<ESubStat>(Enum->GetValueByIndex(Index));
        if (!IsWired(Stat))
        {
            Restricted.Add(Enum->GetNameStringByIndex(Index));
        }
    }
    return Restricted;
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
    // Clears the generator layer only — BaseStatBonus / BaseResistance (designer
    // baselines) are intentionally preserved.
    Modify();
    GeneratedStatBonus = FEquipmentStatBonus();
    GeneratedResistance = FResistanceBonus();
    SubstatPoints = 0;
    PillarPoints = 0.0f;
}

void UEquipmentDataBase::RollResistance()
{
    // No point-gate (resistance has no pending-pool surface) — roll a full tier
    // budget straight into the generator layer. BaseResistance is preserved.
    Modify();
    GeneratedResistance.RerollResistance(GetGeneratorTier());
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

FResistanceBonus UEquipmentDataBase::GetCombinedResistance() const
{
    FResistanceBonus Combined;
    Combined.Fire      = BaseResistance.Fire      + GeneratedResistance.Fire;
    Combined.Water     = BaseResistance.Water     + GeneratedResistance.Water;
    Combined.Earth     = BaseResistance.Earth     + GeneratedResistance.Earth;
    Combined.Wind      = BaseResistance.Wind      + GeneratedResistance.Wind;
    Combined.Light     = BaseResistance.Light     + GeneratedResistance.Light;
    Combined.Darkness  = BaseResistance.Darkness  + GeneratedResistance.Darkness;
    Combined.Lightning = BaseResistance.Lightning + GeneratedResistance.Lightning;
    Combined.Void      = BaseResistance.Void      + GeneratedResistance.Void;
    Combined.Reality   = BaseResistance.Reality   + GeneratedResistance.Reality;
    Combined.Slash     = BaseResistance.Slash     + GeneratedResistance.Slash;
    Combined.Pierce    = BaseResistance.Pierce    + GeneratedResistance.Pierce;
    Combined.Impact    = BaseResistance.Impact    + GeneratedResistance.Impact;
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

    // Crystal-sub-type / Kind consistency backstop. The editor dropdown only
    // greys the wrong-kind values (GetRestrictedCrystalTypes), so this is the
    // hard guarantee against a mis-authored CrystalType. Applies to both
    // weapons and rings — both subclasses call Super::IsDataValid first.
    if (AttachedItem.Kind == EAttachedItemKind::Crystal
        && !CrystalTypeHelpers::IsGemType(AttachedItem.CrystalType))
    {
        Context.AddError(FText::FromString(TEXT(
            "Crystal attachment requires a gem CrystalType (Garnet..Quartz) — an augment stone is not valid here")));
        Result = EDataValidationResult::Invalid;
    }

    if (AttachedItem.Kind == EAttachedItemKind::AugmentStone
        && !CrystalTypeHelpers::IsAugmentStoneType(AttachedItem.CrystalType))
    {
        Context.AddError(FText::FromString(TEXT(
            "Augment stone attachment requires a stone CrystalType (DamageStone / AbilityStone) — a gem is not valid here")));
        Result = EDataValidationResult::Invalid;
    }

    // Fusion well-formedness (shared by weapon + ring). The half/pair rules aren't
    // editor-enforced (plain FCrystalId halves), so this is the hard guarantee. The
    // placement rule (augmented = weapon-only) is enforced in URingData::IsDataValid.
    if (AttachedItem.Kind == EAttachedItemKind::Fusion)
    {
        if (!CrystalTypeHelpers::IsValidFusionPair(AttachedItem.FusionHalfAType, AttachedItem.FusionHalfBType))
        {
            Context.AddError(FText::FromString(TEXT(
                "Invalid fusion halves — a fusion needs at least one stat-stone and at most one crystal "
                "(no crystal+crystal, no AbilityStone+AbilityStone, no AbilityStone+crystal)")));
            Result = EDataValidationResult::Invalid;
        }
        // Half B kind/type consistency backstop (mirrors the CrystalType/Kind backstop
        // above). The dropdown greys the wrong category, but flipping bFusionHalfBIsCrystal
        // after picking a Type can desync the two — this is the hard guarantee.
        if ((AttachedItem.bFusionHalfBIsCrystal && CrystalTypeHelpers::IsAugmentStoneType(AttachedItem.FusionHalfBType))
            || (!AttachedItem.bFusionHalfBIsCrystal && CrystalTypeHelpers::IsGemType(AttachedItem.FusionHalfBType)))
        {
            Context.AddError(FText::FromString(TEXT(
                "Half B kind/type mismatch — bFusionHalfBIsCrystal says crystal but the type is an augment "
                "stone (or vice versa). Set the type to match the selected half kind")));
            Result = EDataValidationResult::Invalid;
        }
        if (AttachedItem.FusionBonusStat == ESubStat::None)
        {
            Context.AddError(FText::FromString(TEXT(
                "Fusion bonus stat must be set to a wired sub-stat (not None)")));
            Result = EDataValidationResult::Invalid;
        }
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
