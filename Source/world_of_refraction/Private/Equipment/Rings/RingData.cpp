// RingData.cpp

#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
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
        // Weapon stones are weapon-only attachments — a ring must never carry one.
        // The attachment struct is shared with weapons, so this is reachable by
        // mis-authoring; flag it as a hard error.
        if (AttachedItem.Kind == EAttachedItemKind::WeaponStone)
        {
            Context.AddError(FText::FromString(TEXT(
                "Weapon stones cannot be attached to a ring — weapon stones are weapon-only attachments")));
            Result = EDataValidationResult::Invalid;
        }

        // Refined-kind attachments are always refined by construction (a
        // Type+Tier pair has no unrefined state, so the bad case is
        // unrepresentable). For Evolution kind the asset pointer still carries
        // bIsRefined, and the slot rule forbids a non-refined evolution item in
        // a ring — it belongs in the PrimaryEvolution slot.
        if (AttachedItem.Kind == EAttachedItemKind::Evolution
            && AttachedItem.Evolution
            && !AttachedItem.Evolution->bIsRefined)
        {
            Context.AddWarning(FText::FromString(TEXT(
                "Slotted evolution crystal is not refined — non-refined evolution items belong in the PrimaryEvolution slot, not a ring")));
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
