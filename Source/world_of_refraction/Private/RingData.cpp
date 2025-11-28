// RingData.cpp

#include "RingData.h"
#include "SpellData.h"
#include "ItemData.h"
#include "CrystalType.h"
#include "EvolutionData.h"

TArray<USpellData *> URingData::GetAvailableSpells() const
{
    // If evolved, use evolution's equipped spells
    if (IsEvolved() && SlottedCrystal && SlottedCrystal->Evolution)
    {
        return SlottedCrystal->Evolution->GetEquippedSpells();
    }

    // Otherwise use ring's spell array
    return Spells;
}

bool URingData::IsEvolved() const
{
    return SlottedCrystal && SlottedCrystal->CrystalType == ECrystalType::EvolutionCrystal;
}

float URingData::CalculateBreakChance(USpellData *Spell, bool bIsInfused) const
{
    using namespace RingBreakConstants;

    // Evolved rings have no break chance
    if (IsEvolved())
    {
        return 0.0f;
    }

    if (!Spell || bIsBroken)
    {
        return 0.0f;
    }

    float BreakChance = 0.0f;

    EItemTier SpellTier = Spell->Tier;
    int32 TierGap = TierHelpers::GetTierGap(Tier, SpellTier);

    if (TierGap > 0)
    {
        BreakChance = TierGap * BASE_BREAK_CHANCE_PER_TIER;
    }

    if (bIsInfused)
    {
        if (Tier == EItemTier::S_Tier)
        {
            BreakChance += S_TIER_INFUSION_BREAK;
        }
        else
        {
            BreakChance += INFUSION_BREAK_BONUS;
        }
    }

    float DurabilityPercent = GetDurabilityPercent();
    if (DurabilityPercent < LOW_DURABILITY_THRESHOLD)
    {
        BreakChance += LOW_DURABILITY_BREAK_BONUS;
    }
    else if (DurabilityPercent < MED_DURABILITY_THRESHOLD)
    {
        BreakChance += MED_DURABILITY_BREAK_BONUS;
    }

    return FMath::Clamp(BreakChance, 0.0f, 1.0f);
};
#if WITH_EDITOR
EDataValidationResult URingData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Name validation
    if (RingName.IsEmpty() || RingName == TEXT("Unnamed Ring"))
    {
        Context.AddError(FText::FromString(TEXT("Ring must have a unique name")));
        Result = EDataValidationResult::Invalid;
    }

    // Element validation
    if (Element == ESpellElement::Generic)
    {
        Context.AddError(FText::FromString(TEXT("Ring cannot have Generic element")));
        Result = EDataValidationResult::Invalid;
    }
    if (Element == ESpellElement::BrokenDarkness)
    {
        Context.AddError(FText::FromString(TEXT("Ring cannot have BrokenDarkness element")));
        Result = EDataValidationResult::Invalid;
    }

    // Crystal validation
    if (SlottedCrystal)
    {
        if (!SlottedCrystal->bIsRefined)
        {
            Context.AddWarning(FText::FromString(TEXT("Slotted crystal is not refined")));
        }

        ESpellElement CrystalElement = SlottedCrystal->GetAssociatedElement();
        if (CrystalElement != Element && CrystalElement != ESpellElement::Generic)
        {
            Context.AddWarning(FText::FromString(
                TEXT("Crystal element differs from ring element - GetRingElement() will use crystal")));
        }
    }

    // Evolved ring validation
    if (IsEvolved())
    {
        if (!SlottedCrystal->Evolution)
        {
            Context.AddError(FText::FromString(TEXT("Evolution crystal has no Evolution assigned")));
            Result = EDataValidationResult::Invalid;
        }

        // Spells array should be empty for evolved rings (uses evolution spells)
        if (Spells.Num() > 0)
        {
            Context.AddWarning(FText::FromString(
                TEXT("Ring has spells but is evolved - ring spells will be ignored, using evolution spells")));
        }
    }
    else
    {
        // Non-evolved ring spell validation
        if (Spells.Num() > MaxSpellSlots)
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Too many spells (%d) for spell slots (%d)"), Spells.Num(), MaxSpellSlots)));
            Result = EDataValidationResult::Invalid;
        }

        if (Spells.Num() == 0)
        {
            Context.AddWarning(FText::FromString(TEXT("Ring has no spells")));
        }

        // Spell element validation - must match ring element
        ESpellElement RingElement = GetRingElement();
        for (int32 i = 0; i < Spells.Num(); ++i)
        {
            if (Spells[i])
            {
                ESpellElement SpellElement = Spells[i]->Element;
                if (SpellElement != RingElement && SpellElement != ESpellElement::Generic)
                {
                    Context.AddError(FText::FromString(FString::Printf(
                        TEXT("Spell '%s' element (%s) doesn't match ring element (%s)"),
                        *Spells[i]->SpellName,
                        *Spells[i]->GetElementName(),
                        *StaticEnum<ESpellElement>()->GetNameStringByValue(static_cast<int64>(RingElement)))));
                    Result = EDataValidationResult::Invalid;
                }
            }
        }
    }

    return Result;
}
#endif