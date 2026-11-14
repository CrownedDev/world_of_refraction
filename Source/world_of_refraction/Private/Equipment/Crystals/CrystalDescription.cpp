// CrystalDescription.cpp
// B-460 (D-061): the crystal and stone description table as FText (was FString in the header).

#include "Equipment/Crystals/CrystalDescription.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/StatStone.h" // B-353
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Combat/CombatConstants.h"

#define LOCTEXT_NAMESPACE "WoRCrystalDescription"

namespace
{
    /** Iolite's "removes every effect" count (the table's all-effects sentinel). */
    constexpr int32 CRYSTAL_TEXT_REMOVES_ALL = 99;

    /** %.0f: a percent shown as a whole number. */
    FText Whole(float Value)
    {
        return FText::AsNumber(FMath::RoundToInt(Value));
    }

    FText FormatTurns(int32 Count)
    {
        return FText::Format(LOCTEXT("Turns", "{0} {0}|plural(one=turn,other=turns)"), Count);
    }
}

FText CrystalDescription::GetTierDescriptor(EItemTier Tier)
{
    switch (Tier)
    {
    case EItemTier::F_Tier:
        return LOCTEXT("TierCrude", "crude");
    case EItemTier::E_Tier:
        return LOCTEXT("TierCommon", "common");
    case EItemTier::D_Tier:
        return LOCTEXT("TierRefined", "refined");
    case EItemTier::C_Tier:
        return LOCTEXT("TierQuality", "quality");
    case EItemTier::B_Tier:
        return LOCTEXT("TierExceptional", "exceptional");
    case EItemTier::A_Tier:
        return LOCTEXT("TierMasterwork", "masterwork");
    case EItemTier::S_Tier:
        return LOCTEXT("TierLegendary", "legendary");
    default:
        return LOCTEXT("TierUnknown", "unknown");
    }
}

FText CrystalDescription::GetCrystalText(const FCrystalId &InId)
{
    const FCrystalId Id = StatStone::AsRowId(InId); // B-353: a StatStone reads as its sub-stat's stone
    // TODO: ItemIdentity::GetTypeName is an FString name table; move it to FText (D-062) with the item names.
    if (CrystalTypeHelpers::IsAugmentStoneType(Id.Type))
    {
        return FText::Format(LOCTEXT("StoneSentence", "A {0} {1}."), GetTierDescriptor(Id.Tier), FText::FromString(ItemIdentity::GetTypeName(Id.Type)));
    }
    return FText::Format(LOCTEXT("CrystalSentence", "A {0} {1} crystal."), GetTierDescriptor(Id.Tier),
                         FText::FromString(ItemIdentity::GetTypeName(Id.Type).ToLower()));
}

FText CrystalDescription::GetItemEffectText(const FCrystalId &InId)
{
    const FCrystalId Id = StatStone::AsRowId(InId); // B-353: a StatStone reads as its sub-stat's stone
    const FText StoneTurns = FormatTurns(CombatConstants::AUGMENT_STONE_CONSUMABLE_DURATION);
    switch (Id.Type)
    {
    case ECrystalType::Garnet:
        return FText::Format(LOCTEXT("Garnet", "Applies a fire burn dealing {0}% of target's max HP per turn for {1}."),
                             Whole(CrystalEffectTable::GetDOTDamagePercent(Id)), FormatTurns(CrystalEffectTable::GetDOTDuration(Id)));
    case ECrystalType::Sapphire:
        return FText::Format(LOCTEXT("Sapphire", "Prevents death for {0} turns within that window gaining 50% max HP  On a DEAD target: revives them at 30% max HP."),
                             CrystalEffectTable::GetLastStandWindow(Id));
    case ECrystalType::Citrine:
        return FText::Format(LOCTEXT("Citrine", "Restores {0}% of the target's max energy; overloads the user with Lightning status buildup."),
                             Whole(CrystalEffectTable::GetEPRestorePercent(Id)));
    case ECrystalType::Emerald:
        // B-341 (D-133): the bonus lands right after this turn at every tier; S grants two.
        return CrystalEffectTable::GetEmeraldBonusTurnCount(Id) > 1
                   ? LOCTEXT("EmeraldTwo", "Sacrifice your turn to grant the target two bonus turns immediately.")
                   : LOCTEXT("EmeraldOne", "Sacrifice your turn to grant the target a bonus turn immediately.");
    case ECrystalType::Amber:
        return FText::Format(LOCTEXT("Amber", "Buffs an ally's defense (or debuffs an enemy's) by {0}% for {1}."),
                             Whole(CrystalEffectTable::GetAmberDefensePercent(Id)), FormatTurns(CrystalEffectTable::GetCrystalDuration(Id)));
    case ECrystalType::Opal:
        return FText::Format(LOCTEXT("Opal", "Buffs an ally's crit chance (or debuffs an enemy's) by {0}% for {1}."),
                             Whole(CrystalEffectTable::GetCritBuffPercent(Id)), FormatTurns(CrystalEffectTable::GetCrystalDuration(Id)));
    case ECrystalType::Onyx:
        if (Id.Tier == EItemTier::S_Tier)
        {
            return LOCTEXT("OnyxS", "Completely silences target for 1 turn.");
        }
        return FText::Format(LOCTEXT("Onyx", "Drains {0}% of the target's energy on use."), Whole(CrystalEffectTable::GetSilencePercentage(Id)));
    case ECrystalType::Amethyst:
        return FText::Format(LOCTEXT("Amethyst", "{0}% chance of a random buff (else a random debuff) at {1}% magnitude for {2}."),
                             Whole(CrystalEffectTable::GetBuffChancePercent(Id)), Whole(CrystalEffectTable::GetGambleMagnitudePercent(Id)),
                             FormatTurns(CrystalEffectTable::GetGambleDuration(Id)));
    case ECrystalType::Iolite:
    {
        const int32 Count = CrystalEffectTable::GetEffectsToRemoveCount(Id);
        if (Count >= CRYSTAL_TEXT_REMOVES_ALL)
        {
            return LOCTEXT("IoliteAll", "Removes all debuffs from an ally, or all buffs from an enemy.");
        }
        return FText::Format(LOCTEXT("Iolite", "Removes up to {0} debuff(s) from an ally, or {0} buff(s) from an enemy."), Count);
    }
    case ECrystalType::Quartz:
        return FText::Format(LOCTEXT("Quartz", "Clears {0}% of the target's status bar and grants matching elemental resistance for {1}."),
                             Whole(CrystalEffectTable::GetStatusClearPercent(Id)), FormatTurns(CrystalEffectTable::GetResistanceDuration(Id)));
    case ECrystalType::DamageStone:
        return FText::Format(LOCTEXT("DamageStone", "Sharpens the weapon to raise raw attack damage by {0}% for {1}; when attached"),
                             Whole(CrystalEffectTable::GetDamageStoneBasePercent(Id)), StoneTurns);
    case ECrystalType::DefenseStone:
        // Magnitude curve lands in C2, the directional consumable in C4 — placeholder text until then.
        return LOCTEXT("DefenseStone", "A stone that bolsters Defense — passively when attached, or on a target when used.");
    case ECrystalType::CritStone:
        // Placeholder until the attached crit-damage hook lands (CritStone maps to ESubStat::CritDamage).
        return LOCTEXT("CritStone", "A stone that raises Crit Damage when attached.");
    case ECrystalType::TurnSpeedStone:
        return LOCTEXT("TurnSpeedStone", "A stone that raises Turn Speed when attached.");
    case ECrystalType::StatusStone:
        return LOCTEXT("StatusStone", "A stone that raises Status Multiplier when attached.");
    case ECrystalType::EfficiencyStone:
        return LOCTEXT("EfficiencyStone", "A stone that improves Efficiency when attached.");
    case ECrystalType::MaxHPStone:
        return LOCTEXT("MaxHPStone", "A stone that raises Max HP when attached.");
    case ECrystalType::MaxEPStone:
        return LOCTEXT("MaxEPStone", "A stone that raises Max EP when attached.");
    case ECrystalType::SpellDamageStone:
        return FText::Format(LOCTEXT("SpellDamageStone", "Attunes the weapon to raise spell (magical) damage by {0}% for {1} — passively when attached, or on a target when used. The magical mirror of the Damage Stone."),
                             Whole(CrystalEffectTable::GetStoneBasePercent(Id.Type, Id.Tier)), StoneTurns);
    case ECrystalType::ResistanceStone:
        return FText::Format(LOCTEXT("ResistanceStone", "Wards against status buildup. When attached, grants {0}% blanket status resistance; when used, raises an ally's resistance or curses an enemy into amplified vulnerability for {1}."),
                             Whole(CrystalEffectTable::GetStoneBasePercent(Id.Type, Id.Tier)), StoneTurns);
    case ECrystalType::SpellSpeedStone:
        return FText::Format(LOCTEXT("SpellSpeedStone", "Quickens spellcasting — raises spell speed by {0}% for {1}. Speeds the cast animation now; tighter defense windows arrive with the real-time defense rework."),
                             Whole(CrystalEffectTable::GetStoneBasePercent(Id.Type, Id.Tier)), StoneTurns);
    case ECrystalType::ActionSpeedStone:
        return FText::Format(LOCTEXT("ActionSpeedStone", "Quickens weapon strikes — raises action speed by {0}% for {1}. Speeds the attack animation now; tighter defense windows arrive with the real-time defense rework."),
                             Whole(CrystalEffectTable::GetStoneBasePercent(Id.Type, Id.Tier)), StoneTurns);
    case ECrystalType::LuckStone:
        return LOCTEXT("LuckStone", "A stone that raises Luck when attached — lifting crit chance, crystal-wear break-skip, and other luck-driven odds together.");
    case ECrystalType::ReflexStone:
        return LOCTEXT("ReflexStone", "A stone that raises Reflex when attached — widening the defense input window so incoming attacks are easier to react to and block.");
    case ECrystalType::DurabilityStone:
        return FText::Format(LOCTEXT("DurabilityStone", "Reinforces an elemental fusion — adds {0} flat durability when fused with a crystal. Inert on its own or in a stone-only fusion."),
                             ItemIdentity::GetDurabilityStoneBonus(Id.Tier));
    case ECrystalType::HealingStone:
        return FText::Format(LOCTEXT("HealingStone", "Consume to instantly heal any target (ally or enemy) for {0}% of their max HP. Consume-only — attaching it to a weapon does nothing."),
                             Whole(CrystalEffectTable::GetHealPercent(Id)));
    default:
        return LOCTEXT("Unknown", "Unknown effect.");
    }
}

#undef LOCTEXT_NAMESPACE
