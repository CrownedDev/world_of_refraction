// RingData.cpp

#include "RingData.h"
#include "SpellData.h"
#include "ItemData.h"
#include "CrystalType.h"

ESpellElement URingData::GetRingElement() const
{
    if (SlottedCrystal)
    {
        return SlottedCrystal->GetAssociatedElement();
    }
    return Element;
}

bool URingData::IsEvolved() const
{
    return SlottedCrystal && SlottedCrystal->CrystalType == ECrystalType::EvolutionCrystal;
}

float URingData::CalculateBreakChance(USpellData *Spell, bool bIsInfused) const
{
    using namespace RingBreakConstants;

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

    if (IsCustomSpell(Spell))
    {
        BreakChance += CUSTOM_SPELL_BREAK_BONUS;
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
virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override
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

    // Spell count validation
    int32 TotalSpells = DefaultSpells.Num() + CustomSpells.Num();
    if (TotalSpells > MaxSpellSlots)
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("Too many spells (%d) for spell slots (%d)"), TotalSpells, MaxSpellSlots)));
        Result = EDataValidationResult::Invalid;
    }

    if (DefaultSpells.Num() == 0)
    {
        Context.AddWarning(FText::FromString(TEXT("Ring has no default spells")));
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

    return Result;
}
#endif