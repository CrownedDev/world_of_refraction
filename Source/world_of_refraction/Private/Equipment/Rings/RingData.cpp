// RingData.cpp

#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Loadout/LoadoutConstants.h"

int32 URingData::GetMaxSpells() const
{
    return LoadoutConstants::MAX_RING_SPELLS;
}

#if WITH_EDITOR
EDataValidationResult URingData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    // Crystal validation — reads the design-time AttachedItem struct (the slot
    // discriminated by Kind).
    if (AttachedItem.Kind == EAttachedItemKind::None)
    {
        Context.AddWarning(FText::FromString(TEXT("Ring has no crystal - cannot be used in combat")));
    }
    else
    {
        // Augment stones are weapon-only attachments — a ring must never carry one.
        // The attachment struct is shared with weapons, so this is reachable by
        // mis-authoring; flag it as a hard error.
        if (AttachedItem.Kind == EAttachedItemKind::AugmentStone)
        {
            Context.AddError(FText::FromString(TEXT(
                "Augment stones cannot be attached to a ring — augment stones are weapon-only attachments")));
            Result = EDataValidationResult::Invalid;
        }

        // Fusion placement guard: augmented fusions (two stones) are weapon-only; a ring
        // accepts only ELEMENTAL fusions (one crystal half). The base IsDataValid already
        // checked well-formedness — this is the ring-specific loosening of the blanket
        // stone reject above for the elemental case.
        if (AttachedItem.Kind == EAttachedItemKind::Fusion
            && !CrystalTypeHelpers::IsElementalFusion(AttachedItem.FusionHalfAType, AttachedItem.FusionHalfBType))
        {
            Context.AddError(FText::FromString(TEXT(
                "Augmented fusions (two stones) are weapon-only — a ring accepts only elemental fusions (with a crystal half)")));
            Result = EDataValidationResult::Invalid;
        }

        // Evolved ring validation: an Evolution-kind slot must carry an assigned
        // evolution item that actually grants evolution.
        if (IsEvolved() && !AttachedItem.Evolution)
        {
            Context.AddError(FText::FromString(TEXT("Evolution crystal has no Evolution assigned")));
            Result = EDataValidationResult::Invalid;
        }

        // Ring tier is informational only; warn on crystal tier mismatch. Refined
        // carries (Type, Tier) directly; Evolution's tier lives on the asset.
        EItemTier CrystalTier = Tier;
        bool bHasCrystalTier = false;
        if (AttachedItem.Kind == EAttachedItemKind::Crystal)
        {
            CrystalTier = AttachedItem.CrystalTier;
            bHasCrystalTier = true;
        }
        else if (AttachedItem.Kind == EAttachedItemKind::Evolution && AttachedItem.Evolution)
        {
            CrystalTier = AttachedItem.Evolution->Tier;
            bHasCrystalTier = true;
        }

        if (bHasCrystalTier && CrystalTier != Tier)
        {
            Context.AddWarning(FText::FromString(FString::Printf(
                TEXT("Ring tier (%s) does not match crystal tier (%s) — ring tier is informational only"),
                *UEnum::GetValueAsString(Tier),
                *UEnum::GetValueAsString(CrystalTier))));
        }
    }

    return Result;
}
#endif
