// InventoryData.cpp
// Per-character inventory + saved loadouts. Validation cross-checks each
// FSavedLoadout against the asset's ownership lists.

#include "Inventory/InventoryData.h"

#include "Skills/Definitions/AbilityData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h" // IsGemType — split CrystalStock gem/stone for the cap check
#include "Inventory/InventoryConstants.h"
#include "Inventory/ItemTier.h"
#include "Equipment/Rings/RingData.h"
#include "Skills/Definitions/SpellData.h"
#include "Equipment/Weapons/WeaponData.h"

// ==================== VALIDATION ====================

TArray<FString> UInventoryData::GetValidationErrors() const
{
    TArray<FString> Errors;

    if (SavedLoadouts.Num() > 0 &&
        !SavedLoadouts.IsValidIndex(DefaultActiveLoadoutIndex))
    {
        Errors.Add(FString::Printf(
            TEXT("DefaultActiveLoadoutIndex %d is out of range for SavedLoadouts (%d entries)"),
            DefaultActiveLoadoutIndex, SavedLoadouts.Num()));
    }

    // ---------- Authored crystal per-tier caps ----------
    // CrystalStock is MIXED gem+stone; at load it splits to the runtime Crystals / Stones pools, each
    // capped CRYSTAL_PER_TIER_CAP per tier INDEPENDENTLY. So validate the gem entries and the stone
    // entries separately per tier — matching the runtime CanAddCount semantics (a tier can hold up
    // to 20 gems AND 20 stones). Empty CrystalStock skips the loops entirely.
    {
        TMap<EItemTier, int32> GemByTier;
        TMap<EItemTier, int32> StoneByTier;
        for (const TPair<FCrystalId, int32> &Pair : CrystalStock)
        {
            if (Pair.Value < 0)
            {
                Errors.Add(FString::Printf(
                    TEXT("CrystalStock entry (Type=%d, Tier=%d) has negative count %d"),
                    static_cast<int32>(Pair.Key.Type),
                    static_cast<int32>(Pair.Key.Tier),
                    Pair.Value));
            }
            const int32 Clamped = FMath::Max(0, Pair.Value);
            (CrystalTypeHelpers::IsGemType(Pair.Key.Type) ? GemByTier : StoneByTier)
                .FindOrAdd(Pair.Key.Tier) += Clamped;
        }
        auto CheckCaps = [&Errors](const TMap<EItemTier, int32> &ByTier, const TCHAR *Label)
        {
            for (const TPair<EItemTier, int32> &TierPair : ByTier)
            {
                if (TierPair.Value > InventoryConstants::CRYSTAL_PER_TIER_CAP)
                {
                    Errors.Add(FString::Printf(
                        TEXT("CrystalStock %s sum for Tier=%d is %d, exceeds CRYSTAL_PER_TIER_CAP (%d)"),
                        Label,
                        static_cast<int32>(TierPair.Key),
                        TierPair.Value,
                        InventoryConstants::CRYSTAL_PER_TIER_CAP));
                }
            }
        };
        CheckCaps(GemByTier, TEXT("gem"));
        CheckCaps(StoneByTier, TEXT("stone"));
    }

    if (EvolutionEquipment.Num() > InventoryConstants::MAX_EVOLUTION_ITEMS)
    {
        Errors.Add(FString::Printf(
            TEXT("EvolutionEquipment has %d entries, exceeds MAX_EVOLUTION_ITEMS (%d)"),
            EvolutionEquipment.Num(), InventoryConstants::MAX_EVOLUTION_ITEMS));
    }

    for (int32 LoadoutIdx = 0; LoadoutIdx < SavedLoadouts.Num(); ++LoadoutIdx)
    {
        const FSavedLoadout &Loadout = SavedLoadouts[LoadoutIdx];
        const FString Prefix = FString::Printf(
            TEXT("Loadout[%d] '%s': "), LoadoutIdx, *Loadout.LoadoutName);

        // Struct-internal errors (slot consistency, count caps, BD rules)
        for (const FString &Err : Loadout.GetValidationErrors())
        {
            Errors.Add(Prefix + Err);
        }

        // Cross-check: every referenced asset must appear in its
        // ownership list.
        if (UWeaponData *Weapon = Loadout.GetPrimaryWeapon())
        {
            if (!Weapons.Contains(Weapon))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Primary weapon '%s' not in inventory Weapons list"),
                    *Weapon->GetName()));
            }
        }

        if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon)
        {
            if (!Weapons.Contains(Loadout.SecondaryWeapon))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Secondary weapon '%s' not in inventory Weapons list"),
                    *Loadout.SecondaryWeapon->GetName()));
            }
        }

        if (URingData *Ring = Loadout.GetPrimaryRing())
        {
            if (!Rings.Contains(Ring))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Primary ring '%s' not in inventory Rings list"),
                    *Ring->GetName()));
            }
        }

        if (UEvolutionItemData *Evolution = Loadout.GetPrimaryEvolution())
        {
            if (!EvolutionEquipment.Contains(Evolution))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Primary evolution crystal '%s' not in inventory EvolutionEquipment list"),
                    *Evolution->GetName()));
            }
        }

        for (const FResonatorRingSlot &Slot : Loadout.EquippedRings)
        {
            if (Slot.Ring && !Rings.Contains(Slot.Ring))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Equipped ring '%s' not in inventory Rings list"),
                    *Slot.Ring->GetName()));
            }
        }

        // [9.5b] Inventory/EquippedItems cross-check removed: under the transfer
        // model, equipped slots are debited from inventory at equip time, so
        // consistency is invariant by construction — no runtime cross-validation
        // needed.

        for (USpellData *Spell : Loadout.GetAllSpells())
        {
            if (Spell && !Spells.Contains(Spell))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Spell '%s' not in inventory Spells list"),
                    *Spell->GetName()));
            }
        }

        for (UAbilityData *Ability : Loadout.GetAllAbilities())
        {
            if (Ability && !Abilities.Contains(Ability))
            {
                Errors.Add(Prefix + FString::Printf(
                    TEXT("Ability '%s' not in inventory Abilities list"),
                    *Ability->GetName()));
            }
        }
    }

    return Errors;
}

bool UInventoryData::HasValidationErrors() const
{
    return GetValidationErrors().Num() > 0;
}

// ==================== EDITOR ====================

#if WITH_EDITOR
EDataValidationResult UInventoryData::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    for (const FString &Error : GetValidationErrors())
    {
        Context.AddError(FText::FromString(Error));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}

void UInventoryData::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Auto-name empty entries as "Loadout N" (1-indexed). Designer-set
    // names are preserved — only empties are filled.
    for (int32 i = 0; i < SavedLoadouts.Num(); ++i)
    {
        if (SavedLoadouts[i].LoadoutName.IsEmpty())
        {
            SavedLoadouts[i].LoadoutName = FString::Printf(TEXT("Loadout %d"), i + 1);
        }
    }

    // Mirrors ULoadoutData::PostEditChangeProperty per-loadout clearing
    // logic. USTRUCT-internal edits don't trigger PostEditChangeProperty on
    // the owning UObject — so we sweep every SavedLoadouts entry on any
    // edit. Editor-only, runs once per user interaction; perf is irrelevant.
    for (FSavedLoadout &Loadout : SavedLoadouts)
    {
        // Class-based clears
        if (Loadout.RequiredClass != ECharacterClass::Generic)
        {
            Loadout.SecondarySlotType = ESecondarySlotType::None;
            Loadout.SecondaryWeapon = nullptr;
            Loadout.SecondaryWeaponAbilities.Empty();
        }

        if (Loadout.RequiredClass != ECharacterClass::Caster)
        {
            Loadout.InnateSpells.Empty();
            Loadout.BDSpellPools.Empty();
        }

        if (Loadout.RequiredClass != ECharacterClass::Resonator)
        {
            Loadout.EquippedRings.Empty();
        }

        // Resonator cannot use Ring primary
        if (Loadout.RequiredClass == ECharacterClass::Resonator &&
            Loadout.PrimarySlotType == EPrimarySlotType::Ring)
        {
            Loadout.PrimarySlotType = EPrimarySlotType::None;
            Loadout.PrimaryRing = nullptr;
        }

        // Primary-slot-type based clears
        if (Loadout.PrimarySlotType != EPrimarySlotType::Weapon)
        {
            Loadout.PrimaryWeapon = nullptr;
            Loadout.PrimaryWeaponAbilities.Empty();
        }

        if (Loadout.PrimarySlotType != EPrimarySlotType::Ring)
        {
            Loadout.PrimaryRing = nullptr;
        }

        if (Loadout.PrimarySlotType != EPrimarySlotType::Evolution)
        {
            Loadout.PrimaryEvolution = nullptr;
        }

        // Secondary slot type based clears
        if (Loadout.SecondarySlotType != ESecondarySlotType::Weapon)
        {
            Loadout.SecondaryWeapon = nullptr;
            Loadout.SecondaryWeaponAbilities.Empty();
        }
    }
}
#endif

// ==================== DROPDOWN OPTIONS ====================

TArray<FString> UInventoryData::GetActiveLoadoutOptions() const
{
    TArray<FString> Options;
    Options.Reserve(SavedLoadouts.Num());
    for (const FSavedLoadout &Loadout : SavedLoadouts)
    {
        Options.Add(Loadout.LoadoutName);
    }
    return Options;
}

// ==================== DATA ASSET ====================

FPrimaryAssetId UInventoryData::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("InventoryData"), GetFName());
}

void UInventoryData::PostLoad()
{
    Super::PostLoad();

    // Gem-merge migration (§13.5): fold the deprecated ItemCrystals + RefinedCrystals into the
    // unified CrystalStock so legacy .uasset files load without re-authoring. Idempotent — guarded
    // on CrystalStock being empty, so a migrated / freshly-authored asset no-ops. Both old maps fold
    // by FCrystalId (the bulk-load splits gem/stone later); the same Id in both maps SUMS (the two
    // forms merge into the one dual-purpose stack). The old maps are cleared after folding so a
    // re-save drops the duplicate. TODO: remove the deprecated maps + this fold after one save cycle.
    if (CrystalStock.Num() == 0 && (ItemCrystals.Num() > 0 || RefinedCrystals.Num() > 0))
    {
        for (const TPair<FCrystalId, int32> &Pair : ItemCrystals)
        {
            CrystalStock.FindOrAdd(Pair.Key) += Pair.Value;
        }
        for (const TPair<FCrystalId, int32> &Pair : RefinedCrystals)
        {
            CrystalStock.FindOrAdd(Pair.Key) += Pair.Value;
        }
        ItemCrystals.Empty();
        RefinedCrystals.Empty();
    }
}
