// RingData.cpp

#include "RingData.h"
#include "SpellData.h"
#include "ItemData.h"
#include "CrystalType.h"

bool URingData::IsEvolved() const
{
    return SlottedCrystal && SlottedCrystal->bIsEvolutionCrystal;
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
}

ESpellElement URingData::GetRingElement() const
{
    if (SlottedCrystal)
    {
        return SlottedCrystal->GetAssociatedElement();
    }
    return ESpellElement::Generic;
}

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

    // Crystal validation
    if (!SlottedCrystal)
    {
        Context.AddWarning(FText::FromString(TEXT("Ring has no crystal - cannot be used in combat")));
    }
    else
    {
        if (!SlottedCrystal->bIsRefined)
        {
            Context.AddWarning(FText::FromString(TEXT("Slotted crystal is not refined")));
        }

        // Evolved ring validation
        if (IsEvolved() && !SlottedCrystal->GrantsEvolution())
        {
            Context.AddError(FText::FromString(TEXT("Evolution crystal has no Evolution assigned")));
            Result = EDataValidationResult::Invalid;
        }
    }

    return Result;
}
#endif