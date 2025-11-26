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

float URingData::CalculateBreakChance(USpellData* Spell, bool bIsInfused) const
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
}

#if WITH_EDITOR
EDataValidationResult URingData::IsDataValid(TArray<FText>& ValidationErrors)
{
    EDataValidationResult Result = EDataValidationResult::Valid;

    // Name validation
    if (RingName.IsEmpty() || RingName == TEXT("Unnamed Ring"))
    {
        ValidationErrors.Add(FText::FromString(TEXT("Ring must have a unique name")));
        Result = EDataValidationResult::Invalid;
    }

    // Element validation
    if (Element == ESpellElement::Generic || Element == ESpellElement::BrokenDarkness)
    {
        ValidationErrors.Add(FText::FromString(TEXT("Ring cannot have Generic or BrokenDarkness element")));
        Result = EDataValidationResult::Invalid;
    }

    // Spell slot validation
    int32 TotalSpells = DefaultSpells.Num() + CustomSpells.Num();
    if (TotalSpells > MaxSpellSlots)
    {
        ValidationErrors.Add(FText::FromString(FString::Printf(
            TEXT("Total spells (%d) exceeds MaxSpellSlots (%d)"),
            TotalSpells, MaxSpellSlots)));
        Result = EDataValidationResult::Invalid;
    }

    // Crystal validation
    if (SlottedCrystal)
    {
        if (!SlottedCrystal->bIsRefined)
        {
            ValidationErrors.Add(FText::FromString(TEXT("Warning: Slotted crystal is not refined")));
        }

        ESpellElement CrystalElement = SlottedCrystal->GetAssociatedElement();
        if (CrystalElement != Element && CrystalElement != ESpellElement::Generic)
        {
            ValidationErrors.Add(FText::FromString(
                TEXT("Warning: Crystal element differs from ring element - GetRingElement() will use crystal")));
        }
    }

    return Result;
}
#endif