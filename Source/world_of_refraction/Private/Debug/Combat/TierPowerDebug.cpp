// TierPowerDebug.cpp

#include "Debug/Combat/TierPowerDebug.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "GameFramework/Actor.h"

void UTierPowerDebug::PrintCurve()
{
    UE_LOG(LogTemp, Display,
           TEXT("=== Tier-Power Multipliers (own-tier; scales damage/status/cost, NOT effects) ==="));
    for (int32 i = 0; i <= 6; ++i)
    {
        const EItemTier Tier = TierHelpers::GetTierFromValue(i);
        UE_LOG(LogTemp, Display, TEXT("  %s -> x%.2f"),
               *TierHelpers::GetTierName(Tier),
               TierPowerScaling::GetTierPowerMultiplier(Tier));
    }
}

FString UTierPowerDebug::GetSkillPowerString(EItemTier Tier)
{
    const float Mult = TierPowerScaling::GetTierPowerMultiplier(Tier);
    return FString::Printf(TEXT("%s -> x%.2f on damage/status/cost (effects excluded)"),
                           *TierHelpers::GetTierDisplayString(Tier), Mult);
}

void UTierPowerDebug::PrintGearContribution(ULoadoutComponent *Loadout)
{
    if (!Loadout)
    {
        UE_LOG(LogTemp, Warning, TEXT("[TierPower] PrintGearContribution: null loadout"));
        return;
    }

    UE_LOG(LogTemp, Display,
           TEXT("=== Tier-Power Gear Contribution (per-item raw -> tier-scaled) ==="));
    UE_LOG(LogTemp, Display,
           TEXT("Per-item lines are illustrative (unrounded); actual contribution follows class "
                "slot rules — the Aggregated Combined below is authoritative."));

    // Print one item's tier + multiplier + each non-zero substat raw -> scaled. The 12 tier-scaled
    // substats only (pools + pillar percents are excluded from tier scaling — see GetActiveStatBonus).
    auto PrintItem = [](const TCHAR *Slot, const FString &Name, EItemTier Tier, const FEquipmentStatBonus &B)
    {
        const float F = TierPowerScaling::GetTierPowerMultiplier(Tier);
        UE_LOG(LogTemp, Display, TEXT("[%s] %s  tier %s  x%.2f"),
               Slot, *Name, *TierHelpers::GetTierName(Tier), F);

        FString Line;
        auto Add = [&](const TCHAR *FieldName, float Raw)
        {
            if (Raw != 0.0f)
            {
                Line += FString::Printf(TEXT("  %s %g->%g"), FieldName, Raw, Raw * F);
            }
        };
        Add(TEXT("RawDmg"),    static_cast<float>(B.BonusRawDamage));
        Add(TEXT("SpellDmg"),  static_cast<float>(B.BonusSpellDamage));
        Add(TEXT("Eff"),       static_cast<float>(B.BonusEfficiency));
        Add(TEXT("StatusMul"), static_cast<float>(B.BonusStatusMultiplier));
        Add(TEXT("CritDmg"),   B.BonusCritDamage);
        Add(TEXT("SpellSpd"),  static_cast<float>(B.BonusSpellSpeed));
        Add(TEXT("Def"),       static_cast<float>(B.BonusDefense));
        Add(TEXT("ActSpd"),    static_cast<float>(B.BonusActionSpeed));
        Add(TEXT("Reflex"),    static_cast<float>(B.BonusReflex));
        Add(TEXT("Resist"),    static_cast<float>(B.BonusResistance));
        Add(TEXT("TurnSpd"),   static_cast<float>(B.BonusTurnSpeed));
        Add(TEXT("Luck"),      static_cast<float>(B.BonusLuck));
        if (Line.IsEmpty())
        {
            Line = TEXT("  (no non-zero tier-scaled substats)");
        }
        UE_LOG(LogTemp, Display, TEXT("%s"), *Line);
    };

    // Active weapon (handles dual-weapon active selection).
    if (const FWeaponLoadoutEntry *W = Loadout->GetActiveWeaponLoadout())
    {
        if (W->WeaponEntry.Weapon)
        {
            PrintItem(TEXT("Weapon"), W->WeaponEntry.Weapon->GetName(),
                      W->WeaponEntry.Weapon->Tier, W->WeaponEntry.StatBonus);
        }
    }

    // Active ring (Resonator RingLoadout + ring-primary classes).
    if (const FRingLoadoutEntry *R = Loadout->GetActiveRingLoadout())
    {
        if (R->RingEntry.Ring)
        {
            PrintItem(TEXT("Ring"), R->RingEntry.Ring->GetName(),
                      R->RingEntry.Ring->Tier, R->RingEntry.StatBonus);
        }
    }

    // Primary-slot evolution — contributes Base + Generated at the item's tier
    // (mirrors GetActiveStatBonus; pillar percents are not summed there).
    const FCombatLoadout Active = Loadout->GetActiveLoadout();
    if (Active.IsEvolutionLoadout() && Active.PrimaryEvolution.Item)
    {
        FEquipmentStatBonus EvoBonus = Active.PrimaryEvolution.Item->BaseStatBonus;
        EvoBonus.Accumulate(Active.PrimaryEvolution.GeneratedStatBonus);
        PrintItem(TEXT("Evolution"), Active.PrimaryEvolution.Item->GetName(),
                  Active.PrimaryEvolution.Item->Tier, EvoBonus);
    }

    // Authoritative aggregate the game actually reads (tier-weighted, rounded once).
    if (AActor *Owner = Loadout->GetOwner())
    {
        const FEquipmentStatBonus C = Loadout->GetActiveStatBonus(Owner);
        UE_LOG(LogTemp, Display,
               TEXT("--- Aggregated Combined (tier-weighted totals used by combat) ---"));
        UE_LOG(LogTemp, Display,
               TEXT("  RawDmg %d  SpellDmg %d  Eff %d  StatusMul %d  CritDmg %.2f  SpellSpd %d  "
                    "Def %d  ActSpd %d  Reflex %d  Resist %d  TurnSpd %d  Luck %d  |  "
                    "(excluded) MaxHP %d  MaxEP %d"),
               C.BonusRawDamage, C.BonusSpellDamage, C.BonusEfficiency, C.BonusStatusMultiplier,
               C.BonusCritDamage, C.BonusSpellSpeed, C.BonusDefense, C.BonusActionSpeed,
               C.BonusReflex, C.BonusResistance, C.BonusTurnSpeed, C.BonusLuck,
               C.BonusMaxHP, C.BonusMaxEnergy);
    }
}
