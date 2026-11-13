// WoRGameNames.cpp
// One case per enum value; see WoRGameNames.h.

#include "WoRGameNames.h"
#include "Inventory/InventoryVerbs.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Grid/ECombatRow.h"
#include "Currency/CurrencyTypes.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Equipment/Weapons/EWeaponWieldMode.h"
#include "Inventory/ItemTier.h"
#include "Memory/HiddenQuestData.h"
#include "Merchant/MerchantData.h"
#include "Skills/Definitions/SpellSchool.h"

#define LOCTEXT_NAMESPACE "WoRGameNames"

namespace WoRGameNames
{
FText GetElementName(ESpellElement Element)
{
    switch (Element)
    {
    case ESpellElement::Generic:
        return LOCTEXT("ESpellElement_Generic", "Generic");
    case ESpellElement::Fire:
        return LOCTEXT("ESpellElement_Fire", "Fire");
    case ESpellElement::Water:
        return LOCTEXT("ESpellElement_Water", "Water");
    case ESpellElement::Earth:
        return LOCTEXT("ESpellElement_Earth", "Earth");
    case ESpellElement::Wind:
        return LOCTEXT("ESpellElement_Wind", "Wind");
    case ESpellElement::Light:
        return LOCTEXT("ESpellElement_Light", "Light");
    case ESpellElement::Darkness:
        return LOCTEXT("ESpellElement_Darkness", "Darkness");
    case ESpellElement::Lightning:
        return LOCTEXT("ESpellElement_Lightning", "Lightning");
    case ESpellElement::Void:
        return LOCTEXT("ESpellElement_Void", "Void");
    case ESpellElement::Reality:
        return LOCTEXT("ESpellElement_Reality", "Unknown"); // D-095: players never see "Reality"
    case ESpellElement::None:
        return LOCTEXT("ESpellElement_None", "None");
    }
    return FText::GetEmpty();
}

FText GetSkillEffectTypeName(ESkillEffectType EffectType)
{
    switch (EffectType)
    {
    case ESkillEffectType::None:
        return LOCTEXT("ESkillEffectType_None", "None");
    case ESkillEffectType::DOT:
        return LOCTEXT("ESkillEffectType_DOT", "Damage Over Time");
    case ESkillEffectType::SpeedDebuff:
        return LOCTEXT("ESkillEffectType_SpeedDebuff", "Speed Debuff");
    case ESkillEffectType::DefenseDebuff:
        return LOCTEXT("ESkillEffectType_DefenseDebuff", "Defense Debuff");
    case ESkillEffectType::CritDebuff:
        return LOCTEXT("ESkillEffectType_CritDebuff", "Crit Chance Debuff");
    case ESkillEffectType::EnergyDebuff:
        return LOCTEXT("ESkillEffectType_EnergyDebuff", "Energy Debuff");
    case ESkillEffectType::SkipTurn:
        return LOCTEXT("ESkillEffectType_SkipTurn", "Skip Turn");
    case ESkillEffectType::RandomDebuff:
        return LOCTEXT("ESkillEffectType_RandomDebuff", "Random Stat Debuff");
    case ESkillEffectType::BurstDamage:
        return LOCTEXT("ESkillEffectType_BurstDamage", "Burst Damage");
    case ESkillEffectType::MindBuff:
        return LOCTEXT("ESkillEffectType_MindBuff", "Mind Buff (All Mind Sub-Stats)");
    case ESkillEffectType::MindDebuff:
        return LOCTEXT("ESkillEffectType_MindDebuff", "Mind Debuff (All Mind Sub-Stats)");
    case ESkillEffectType::BodyBuff:
        return LOCTEXT("ESkillEffectType_BodyBuff", "Body Buff (All Body Sub-Stats)");
    case ESkillEffectType::BodyDebuff:
        return LOCTEXT("ESkillEffectType_BodyDebuff", "Body Debuff (All Body Sub-Stats)");
    case ESkillEffectType::SpiritBuff:
        return LOCTEXT("ESkillEffectType_SpiritBuff", "Spirit Buff (All Spirit Sub-Stats)");
    case ESkillEffectType::SpiritDebuff:
        return LOCTEXT("ESkillEffectType_SpiritDebuff", "Spirit Debuff (All Spirit Sub-Stats)");
    case ESkillEffectType::SpellCostBuff:
        return LOCTEXT("ESkillEffectType_SpellCostBuff", "Spell Cost Buff");
    case ESkillEffectType::SpellCostDebuff:
        return LOCTEXT("ESkillEffectType_SpellCostDebuff", "Spell Cost Debuff");
    case ESkillEffectType::SpellDamageBuff:
        return LOCTEXT("ESkillEffectType_SpellDamageBuff", "Spell Damage Buff");
    case ESkillEffectType::SpellDamageDebuff:
        return LOCTEXT("ESkillEffectType_SpellDamageDebuff", "Spell Damage Debuff");
    case ESkillEffectType::StatusMultiplierBuff:
        return LOCTEXT("ESkillEffectType_StatusMultiplierBuff", "Status Multiplier Buff");
    case ESkillEffectType::StatusMultiplierDebuff:
        return LOCTEXT("ESkillEffectType_StatusMultiplierDebuff", "Status Multiplier Debuff");
    case ESkillEffectType::CritChanceBuff:
        return LOCTEXT("ESkillEffectType_CritChanceBuff", "Crit Chance Buff");
    case ESkillEffectType::CritChanceDebuff:
        return LOCTEXT("ESkillEffectType_CritChanceDebuff", "Crit Chance Debuff");
    case ESkillEffectType::SpellSpeedBuff:
        return LOCTEXT("ESkillEffectType_SpellSpeedBuff", "Spell Speed Buff");
    case ESkillEffectType::SpellSpeedDebuff:
        return LOCTEXT("ESkillEffectType_SpellSpeedDebuff", "Spell Speed Debuff");
    case ESkillEffectType::DefenseBuff:
        return LOCTEXT("ESkillEffectType_DefenseBuff", "Defense Buff");
    case ESkillEffectType::ActionSpeedBuff:
        return LOCTEXT("ESkillEffectType_ActionSpeedBuff", "Action Speed Buff");
    case ESkillEffectType::ActionSpeedDebuff:
        return LOCTEXT("ESkillEffectType_ActionSpeedDebuff", "Action Speed Debuff");
    case ESkillEffectType::RawDamageBuff:
        return LOCTEXT("ESkillEffectType_RawDamageBuff", "Raw Damage Buff");
    case ESkillEffectType::RawDamageDebuff:
        return LOCTEXT("ESkillEffectType_RawDamageDebuff", "Raw Damage Debuff");
    case ESkillEffectType::MaxEnergyBuff:
        return LOCTEXT("ESkillEffectType_MaxEnergyBuff", "Max Energy Buff");
    case ESkillEffectType::MaxEnergyDebuff:
        return LOCTEXT("ESkillEffectType_MaxEnergyDebuff", "Max Energy Debuff");
    case ESkillEffectType::ResistanceBuff:
        return LOCTEXT("ESkillEffectType_ResistanceBuff", "Resistance Buff");
    case ESkillEffectType::ResistanceDebuff:
        return LOCTEXT("ESkillEffectType_ResistanceDebuff", "Resistance Debuff");
    case ESkillEffectType::SpellSizeBuff:
        return LOCTEXT("ESkillEffectType_SpellSizeBuff", "Spell Size Buff");
    case ESkillEffectType::SpellSizeDebuff:
        return LOCTEXT("ESkillEffectType_SpellSizeDebuff", "Spell Size Debuff");
    case ESkillEffectType::TurnSpeedBuff:
        return LOCTEXT("ESkillEffectType_TurnSpeedBuff", "Turn Speed Buff");
    case ESkillEffectType::TurnSpeedDebuff:
        return LOCTEXT("ESkillEffectType_TurnSpeedDebuff", "Turn Speed Debuff");
    case ESkillEffectType::LuckBuff:
        return LOCTEXT("ESkillEffectType_LuckBuff", "Luck Buff");
    case ESkillEffectType::LuckDebuff:
        return LOCTEXT("ESkillEffectType_LuckDebuff", "Luck Debuff");
    case ESkillEffectType::DamageBuff:
        return LOCTEXT("ESkillEffectType_DamageBuff", "Damage Buff (Generic)");
    case ESkillEffectType::DamageDebuff:
        return LOCTEXT("ESkillEffectType_DamageDebuff", "Damage Debuff (Generic)");
    case ESkillEffectType::SpeedBuff:
        return LOCTEXT("ESkillEffectType_SpeedBuff", "Speed Buff (Generic)");
    case ESkillEffectType::EnergyRestore:
        return LOCTEXT("ESkillEffectType_EnergyRestore", "Energy Restore");
    case ESkillEffectType::EnergyDrain:
        return LOCTEXT("ESkillEffectType_EnergyDrain", "Energy Drain");
    case ESkillEffectType::HealthRestore:
        return LOCTEXT("ESkillEffectType_HealthRestore", "Health Restore");
    case ESkillEffectType::Heal:
        return LOCTEXT("ESkillEffectType_Heal", "Heal (Instant)");
    case ESkillEffectType::RetaliationDamage:
        return LOCTEXT("ESkillEffectType_RetaliationDamage", "Retaliation Damage");
    case ESkillEffectType::SelfDamage:
        return LOCTEXT("ESkillEffectType_SelfDamage", "Self Damage");
    case ESkillEffectType::Cleanse:
        return LOCTEXT("ESkillEffectType_Cleanse", "Cleanse (Remove All Debuffs)");
    case ESkillEffectType::RemoveSpeedDebuff:
        return LOCTEXT("ESkillEffectType_RemoveSpeedDebuff", "Remove Speed Debuff");
    case ESkillEffectType::RemoveDamageDebuff:
        return LOCTEXT("ESkillEffectType_RemoveDamageDebuff", "Remove Damage Debuff");
    case ESkillEffectType::RemoveDefenseDebuff:
        return LOCTEXT("ESkillEffectType_RemoveDefenseDebuff", "Remove Defense Debuff");
    case ESkillEffectType::Stun:
        return LOCTEXT("ESkillEffectType_Stun", "Stun (Attack/Defend Only)");
    case ESkillEffectType::HealBlock:
        return LOCTEXT("ESkillEffectType_HealBlock", "Heal Block");
    case ESkillEffectType::Silenced:
        return LOCTEXT("ESkillEffectType_Silenced", "Silenced");
    case ESkillEffectType::RandomSkill:
        return LOCTEXT("ESkillEffectType_RandomSkill", "Random Skill (Loss of Control)");
    case ESkillEffectType::ModifyDamageDealt:
        return LOCTEXT("ESkillEffectType_ModifyDamageDealt", "Damage Up");
    case ESkillEffectType::ReduceDamageTaken:
        return LOCTEXT("ESkillEffectType_ReduceDamageTaken", "Reduce Damage Taken");
    case ESkillEffectType::ModifyHealing:
        return LOCTEXT("ESkillEffectType_ModifyHealing", "Healing Up");
    case ESkillEffectType::ModifyCritChance:
        return LOCTEXT("ESkillEffectType_ModifyCritChance", "Crit Chance Up");
    case ESkillEffectType::ModifyCritDamage:
        return LOCTEXT("ESkillEffectType_ModifyCritDamage", "Crit Damage Up");
    case ESkillEffectType::ModifyEnergyCost:
        return LOCTEXT("ESkillEffectType_ModifyEnergyCost", "Energy Cost Down");
    case ESkillEffectType::ModifyTurnSpeed:
        return LOCTEXT("ESkillEffectType_ModifyTurnSpeed", "Turn Speed Up");
    case ESkillEffectType::ModifyStatusResist:
        return LOCTEXT("ESkillEffectType_ModifyStatusResist", "Status Resist Up");
    case ESkillEffectType::RestoreHPPercent:
        return LOCTEXT("ESkillEffectType_RestoreHPPercent", "Restore HP %");
    case ESkillEffectType::RestoreEnergyPercent:
        return LOCTEXT("ESkillEffectType_RestoreEnergyPercent", "Restore Energy %");
    case ESkillEffectType::DrainHP:
        return LOCTEXT("ESkillEffectType_DrainHP", "Drain HP");
    case ESkillEffectType::DrainEnergy:
        return LOCTEXT("ESkillEffectType_DrainEnergy", "Drain Energy");
    case ESkillEffectType::DamageReflect:
        return LOCTEXT("ESkillEffectType_DamageReflect", "Reflect");
    case ESkillEffectType::ReflectPhysicalDamage:
        return LOCTEXT("ESkillEffectType_ReflectPhysicalDamage", "Reflect Physical");
    case ESkillEffectType::ReflectSpellDamage:
        return LOCTEXT("ESkillEffectType_ReflectSpellDamage", "Reflect Spell");
    case ESkillEffectType::Lifesteal:
        return LOCTEXT("ESkillEffectType_Lifesteal", "Lifesteal");
    case ESkillEffectType::AbsorbDamage:
        return LOCTEXT("ESkillEffectType_AbsorbDamage", "Absorb");
    case ESkillEffectType::GrantDOTImmunity:
        return LOCTEXT("ESkillEffectType_GrantDOTImmunity", "DOT Immunity");
    case ESkillEffectType::GrantStunImmunity:
        return LOCTEXT("ESkillEffectType_GrantStunImmunity", "Stun Immunity");
    case ESkillEffectType::GrantSilenceImmunity:
        return LOCTEXT("ESkillEffectType_GrantSilenceImmunity", "Silence Immunity");
    case ESkillEffectType::GrantAllStatusImmunity:
        return LOCTEXT("ESkillEffectType_GrantAllStatusImmunity", "All-Status Immunity");
    case ESkillEffectType::GrantFireImmunity:
        return LOCTEXT("ESkillEffectType_GrantFireImmunity", "Fire Immunity");
    case ESkillEffectType::GrantWaterImmunity:
        return LOCTEXT("ESkillEffectType_GrantWaterImmunity", "Water Immunity");
    case ESkillEffectType::GrantEarthImmunity:
        return LOCTEXT("ESkillEffectType_GrantEarthImmunity", "Earth Immunity");
    case ESkillEffectType::GrantWindImmunity:
        return LOCTEXT("ESkillEffectType_GrantWindImmunity", "Wind Immunity");
    case ESkillEffectType::GrantLightImmunity:
        return LOCTEXT("ESkillEffectType_GrantLightImmunity", "Light Immunity");
    case ESkillEffectType::GrantDarknessImmunity:
        return LOCTEXT("ESkillEffectType_GrantDarknessImmunity", "Darkness Immunity");
    case ESkillEffectType::GrantLightningImmunity:
        return LOCTEXT("ESkillEffectType_GrantLightningImmunity", "Lightning Immunity");
    case ESkillEffectType::GrantVoidImmunity:
        return LOCTEXT("ESkillEffectType_GrantVoidImmunity", "Void Immunity");
    case ESkillEffectType::GrantRealityImmunity:
        return LOCTEXT("ESkillEffectType_GrantRealityImmunity", "Unknown Immunity"); // D-095
    case ESkillEffectType::ApplyBurnToTarget:
        return LOCTEXT("ESkillEffectType_ApplyBurnToTarget", "Apply Burn");
    case ESkillEffectType::ApplyChillToTarget:
        return LOCTEXT("ESkillEffectType_ApplyChillToTarget", "Apply Chill");
    case ESkillEffectType::ApplyStunToTarget:
        return LOCTEXT("ESkillEffectType_ApplyStunToTarget", "Apply Stun");
    case ESkillEffectType::CleanseSelf:
        return LOCTEXT("ESkillEffectType_CleanseSelf", "Cleanse Self");
    case ESkillEffectType::CleanseAllies:
        return LOCTEXT("ESkillEffectType_CleanseAllies", "Cleanse Allies");
    case ESkillEffectType::ExtraAction:
        return LOCTEXT("ESkillEffectType_ExtraAction", "Extra Turn"); // B-170d (Q-170f): a whole bonus turn
    case ESkillEffectType::GuaranteedCrit:
        return LOCTEXT("ESkillEffectType_GuaranteedCrit", "Guaranteed Crit");
    case ESkillEffectType::IgnoreDefense:
        return LOCTEXT("ESkillEffectType_IgnoreDefense", "Ignore Defense");
    case ESkillEffectType::DoubleHit:
        return LOCTEXT("ESkillEffectType_DoubleHit", "Double Hit");
    case ESkillEffectType::LastStand:
        return LOCTEXT("ESkillEffectType_LastStand", "Last Stand");
    case ESkillEffectType::IncreaseDamageTaken:
        return LOCTEXT("ESkillEffectType_IncreaseDamageTaken", "Increase Damage Taken");
    case ESkillEffectType::StatusIncrease:
        return LOCTEXT("ESkillEffectType_StatusIncrease", "Status Increase (Build Gauge)");
    case ESkillEffectType::StatusDecrease:
        return LOCTEXT("ESkillEffectType_StatusDecrease", "Status Decrease (Reduce Gauge)");
    case ESkillEffectType::EfficiencyBuff:
        return LOCTEXT("ESkillEffectType_EfficiencyBuff", "Efficiency Buff");
    case ESkillEffectType::EfficiencyDebuff:
        return LOCTEXT("ESkillEffectType_EfficiencyDebuff", "Efficiency Debuff");
    case ESkillEffectType::MaxHPBuff:
        return LOCTEXT("ESkillEffectType_MaxHPBuff", "Max HP Buff");
    case ESkillEffectType::MaxHPDebuff:
        return LOCTEXT("ESkillEffectType_MaxHPDebuff", "Max HP Debuff");
    case ESkillEffectType::CritDamageDebuff:
        return LOCTEXT("ESkillEffectType_CritDamageDebuff", "Crit Damage Debuff");
    case ESkillEffectType::ReflexBuff:
        return LOCTEXT("ESkillEffectType_ReflexBuff", "Reflex Buff");
    case ESkillEffectType::ReflexDebuff:
        return LOCTEXT("ESkillEffectType_ReflexDebuff", "Reflex Debuff");
    case ESkillEffectType::HealingReceivedBuff:
        return LOCTEXT("ESkillEffectType_HealingReceivedBuff", "Healing Boost");
    case ESkillEffectType::PushBack:
        return LOCTEXT("ESkillEffectType_PushBack", "Push Back (one row)");
    case ESkillEffectType::PullForward:
        return LOCTEXT("ESkillEffectType_PullForward", "Pull Forward (one row)");
    case ESkillEffectType::GrantElementImmunity:
        return LOCTEXT("ESkillEffectType_GrantElementImmunity", "Element Immunity");
    case ESkillEffectType::ApplyDOTOnHit:
        return LOCTEXT("ESkillEffectType_ApplyDOTOnHit", "Apply Damage over Time");
    // B-042j2 (D-092): the absorbing family.
    case ESkillEffectType::NegateDamage:
        return LOCTEXT("ESkillEffectType_NegateDamage", "Barrier");
    case ESkillEffectType::NegatePhysical:
        return LOCTEXT("ESkillEffectType_NegatePhysical", "Shield");
    case ESkillEffectType::NegateElement:
        return LOCTEXT("ESkillEffectType_NegateElement", "Ward");
    case ESkillEffectType::NegateElemental:
        return LOCTEXT("ESkillEffectType_NegateElemental", "Elemental Ward");
    case ESkillEffectType::NegateDamageTotal:
        return LOCTEXT("ESkillEffectType_NegateDamageTotal", "Barrier (in total)");
    case ESkillEffectType::NegateElementTotal:
        return LOCTEXT("ESkillEffectType_NegateElementTotal", "Ward (in total)");
    // B-064a: Ice's status break, named by Crown's rule (Q-064a2).
    case ESkillEffectType::IceBreak:
        return FormatBreakName(GetIceName()); // Q-064a2 (Crown, 2026-11-12): "Ice Break"
    // B-170d (Q-170f, Q-170g): the action effects.
    case ESkillEffectType::GainActions:
        return LOCTEXT("ESkillEffectType_GainActions", "Gain Action");
    case ESkillEffectType::LoseActions:
        return LOCTEXT("ESkillEffectType_LoseActions", "Lose Action");
    case ESkillEffectType::MaxActionsBuff:
        return LOCTEXT("ESkillEffectType_MaxActionsBuff", "Max Actions Up");
    case ESkillEffectType::MaxActionsDebuff:
        return LOCTEXT("ESkillEffectType_MaxActionsDebuff", "Max Actions Down");
    }
    return FText::GetEmpty();
}

FText GetCharacterClassName(ECharacterClass CharacterClass)
{
    switch (CharacterClass)
    {
    case ECharacterClass::Generic:
        return LOCTEXT("ECharacterClass_Generic", "Generic");
    case ECharacterClass::Caster:
        return LOCTEXT("ECharacterClass_Caster", "Refractor");
    case ECharacterClass::Resonator:
        return LOCTEXT("ECharacterClass_Resonator", "Resonator");
    }
    return FText::GetEmpty();
}

FText GetDefenseTypeName(EDefenseType DefenseType)
{
    switch (DefenseType)
    {
    case EDefenseType::None:
        return LOCTEXT("EDefenseType_None", "None");
    case EDefenseType::Block:
        return LOCTEXT("EDefenseType_Block", "Guard");
    case EDefenseType::Parry:
        return LOCTEXT("EDefenseType_Parry", "Guard"); // legacy value: every guard is a Guard (Q-182)
    case EDefenseType::Dodge:
        return LOCTEXT("EDefenseType_Dodge", "Dodge");
    }
    return FText::GetEmpty();
}

FText GetFlawlessParryName()
{
    return LOCTEXT("FlawlessParry", "Flawless Parry");
}

FText GetCombatRowName(ECombatRow Row)
{
    switch (Row)
    {
    case ECombatRow::Back:
        return LOCTEXT("ECombatRow_Back", "Back Row");
    case ECombatRow::Middle:
        return LOCTEXT("ECombatRow_Middle", "Middle Row");
    case ECombatRow::Front:
        return LOCTEXT("ECombatRow_Front", "Front Row");
    }
    return FText::GetEmpty();
}

FText GetQuestKindName(EQuestKind Kind)
{
    switch (Kind)
    {
    case EQuestKind::Hidden:
        return LOCTEXT("EQuestKind_Hidden", "Hidden");
    case EQuestKind::Main:
        return LOCTEXT("EQuestKind_Main", "Main");
    case EQuestKind::Side:
        return LOCTEXT("EQuestKind_Side", "Side");
    }
    return FText::GetEmpty();
}

FText GetSpellSchoolName(ESpellSchool School)
{
    switch (School)
    {
    case ESpellSchool::Conjuration:
        return LOCTEXT("ESpellSchool_Conjuration", "Conjuration");
    case ESpellSchool::Enhancement:
        return LOCTEXT("ESpellSchool_Enhancement", "Enhancement");
    case ESpellSchool::Restoration:
        return LOCTEXT("ESpellSchool_Restoration", "Restoration");
    case ESpellSchool::Destruction:
        return LOCTEXT("ESpellSchool_Destruction", "Destruction"); // D-163: brought back (B-374)
    case ESpellSchool::Manipulation:
        return LOCTEXT("ESpellSchool_Manipulation", "Manipulation");
    case ESpellSchool::Impairment:
        return LOCTEXT("ESpellSchool_Impairment", "Impairment");
    }
    return FText::GetEmpty();
}

FText GetMerchantTypeName(EMerchantType MerchantType)
{
    switch (MerchantType)
    {
    case EMerchantType::Blacksmith:
        return LOCTEXT("EMerchantType_Blacksmith", "Blacksmith (weapons, augment stones)");
    case EMerchantType::Jeweler:
        return LOCTEXT("EMerchantType_Jeweler", "Jeweler (adapters, item slots)");
    case EMerchantType::CombatMaster:
        return LOCTEXT("EMerchantType_CombatMaster", "Combat Master (abilities)");
    case EMerchantType::SpellShop:
        return LOCTEXT("EMerchantType_SpellShop", "Spell Shop (spells)");
    case EMerchantType::Spiritualist:
        return LOCTEXT("EMerchantType_Spiritualist", "Spiritualist (evolutions, crystals, repairs)");
    case EMerchantType::StoneSmith:
        return LOCTEXT("EMerchantType_StoneSmith", "Stone Smith (augment stones)");
    case EMerchantType::LoadoutVendor:
        return LOCTEXT("EMerchantType_LoadoutVendor", "Loadout Vendor (purchase loadout)");
    }
    return FText::GetEmpty();
}

// OBSOLETE (B-333): typed essences retire (D-125).
FText GetEssenceName(EEssenceType Essence)
{
    switch (Essence)
    {
    case EEssenceType::Fire:
        return LOCTEXT("EEssenceType_Fire", "Fire Essence");
    case EEssenceType::Water:
        return LOCTEXT("EEssenceType_Water", "Water Essence");
    case EEssenceType::Lightning:
        return LOCTEXT("EEssenceType_Lightning", "Lightning Essence");
    case EEssenceType::Wind:
        return LOCTEXT("EEssenceType_Wind", "Wind Essence");
    case EEssenceType::Earth:
        return LOCTEXT("EEssenceType_Earth", "Earth Essence");
    case EEssenceType::Light:
        return LOCTEXT("EEssenceType_Light", "Light Essence");
    case EEssenceType::Darkness:
        return LOCTEXT("EEssenceType_Darkness", "Darkness Essence");
    case EEssenceType::Void:
        return LOCTEXT("EEssenceType_Void", "Void Essence");
    case EEssenceType::Reality:
        return LOCTEXT("EEssenceType_Reality", "Unknown Essence"); // D-095
    case EEssenceType::Generic:
        return LOCTEXT("EEssenceType_Generic", "Quartz Essence"); // D-105
    case EEssenceType::Mind:
        return LOCTEXT("EEssenceType_Mind", "Mind Essence");
    case EEssenceType::Body:
        return LOCTEXT("EEssenceType_Body", "Body Essence");
    case EEssenceType::Spirit:
        return LOCTEXT("EEssenceType_Spirit", "Spirit Essence");
    case EEssenceType::Ability:
        return LOCTEXT("EEssenceType_Ability", "Ability Essence");
    }
    return FText::GetEmpty();
}

FText GetCurrencyName(ECurrencyType Currency)
{
    switch (Currency)
    {
    // B-366 (D-159): the names swap; the enum values keep theirs (see ECurrencyType).
    case ECurrencyType::Gold:
        return LOCTEXT("ECurrencyType_Gold", "Prisms");
    case ECurrencyType::Prisms:
        return LOCTEXT("ECurrencyType_Prisms", "Gold");
    case ECurrencyType::Diamond:
        return LOCTEXT("ECurrencyType_Diamond", "Diamond");
    case ECurrencyType::EssenceTyped: // OBSOLETE (B-333)
        return LOCTEXT("ECurrencyType_EssenceTyped", "Typed Essence");
    case ECurrencyType::GearEssence:
        return LOCTEXT("ECurrencyType_GearEssence", "Gear Essence");
    case ECurrencyType::SkillEssence:
        return LOCTEXT("ECurrencyType_SkillEssence", "Skill Essence");
    case ECurrencyType::ElementalEssence:
        return LOCTEXT("ECurrencyType_ElementalEssence", "Elemental Essence");
    case ECurrencyType::PillarEssence:
        return LOCTEXT("ECurrencyType_PillarEssence", "Pillar Essence");
    case ECurrencyType::MentorTokens:
        return LOCTEXT("ECurrencyType_MentorTokens", "Mentor Tokens");
    }
    return FText::GetEmpty();
}

FText GetAdapterWord()
{
    return LOCTEXT("Adapter", "Adapter");
}

FText GetAdapterWordPlural()
{
    return LOCTEXT("Adapters", "Adapters");
}

FText GetActiveAdapterWord()
{
    return LOCTEXT("ActiveAdapter", "Active adapter");
}

FText GetYourselfWord()
{
    return LOCTEXT("Yourself", "yourself");
}

FText GetActionsWord()
{
    return LOCTEXT("Actions", "Actions");
}

FText GetEndTurnVerb()
{
    return LOCTEXT("VerbEndTurn", "End Turn");
}

FText GetInventoryVerbName(EInventoryVerb Verb)
{
    return GetInventoryVerbName(Verb, ECurrencyType::Gold);
}

FText GetInventoryVerbName(EInventoryVerb Verb, ECurrencyType ConvertPays)
{
    switch (Verb)
    {
    case EInventoryVerb::Give:
        return LOCTEXT("VerbGive", "Give");
    case EInventoryVerb::Store:
        return LOCTEXT("VerbStore", "Store");
    case EInventoryVerb::Take:
        return LOCTEXT("VerbTake", "Take");
    case EInventoryVerb::Dismantle:
        return LOCTEXT("VerbDismantle", "Dismantle");
    case EInventoryVerb::Convert:
        // B-366b: pays the currency of the place (Q-366), named from the table (D-159).
        return FText::Format(LOCTEXT("VerbConvert", "Convert to {0}"), GetCurrencyName(ConvertPays));
    }
    return FText::GetEmpty();
}

FText GetAdapterFormName(EAdapterForm Form)
{
    switch (Form)
    {
    case EAdapterForm::Ring:
        return LOCTEXT("EAdapterForm_Ring", "Ring");
    case EAdapterForm::Pistol:
        return LOCTEXT("EAdapterForm_Pistol", "Pistol");
    case EAdapterForm::Trainer:
        return LOCTEXT("EAdapterForm_Trainer", "Trainer");
    default:
        return GetAdapterWord();
    }
}
} // namespace WoRGameNames

namespace
{
    /** The elements whose break and DoT are named after them (Q-064a2): every element but Generic (inherit / physical),
     *  Reality (D-095: players never see its name) and None. */
    bool HasElementEffectNames(ESpellElement Element)
    {
        return Element != ESpellElement::Generic && Element != ESpellElement::Reality && Element != ESpellElement::None;
    }
}

FText WoRGameNames::FormatBreakName(const FText &ElementName)
{
    return FText::Format(LOCTEXT("ElementBreakFormat", "{0} Break"), ElementName);
}

FText WoRGameNames::FormatDoTName(const FText &ElementName)
{
    return FText::Format(LOCTEXT("ElementDoTFormat", "{0} DoT"), ElementName);
}

FText WoRGameNames::GetIceName()
{
    return LOCTEXT("DamageType_Ice", "Ice");
}

FText WoRGameNames::GetElementBreakName(ESpellElement Element)
{
    // Q-064a2 (Crown, 2026-11-12): "<Element> Break", each on the trigger that fires for its element (B-248).
    return HasElementEffectNames(Element) ? FormatBreakName(GetElementName(Element)) : FText::GetEmpty();
}

FText WoRGameNames::GetElementDoTName(ESpellElement Element)
{
    return HasElementEffectNames(Element) ? FormatDoTName(GetElementName(Element)) : FText::GetEmpty();
}

FText WoRGameNames::GetCrystalTypeName(ECrystalType Type)
{
    switch (Type)
    {
    case ECrystalType::Garnet:
        return LOCTEXT("ECrystalType_Garnet", "Garnet");
    case ECrystalType::Sapphire:
        return LOCTEXT("ECrystalType_Sapphire", "Sapphire");
    case ECrystalType::Citrine:
        return LOCTEXT("ECrystalType_Citrine", "Citrine");
    case ECrystalType::Emerald:
        return LOCTEXT("ECrystalType_Emerald", "Emerald");
    case ECrystalType::Amber:
        return LOCTEXT("ECrystalType_Amber", "Amber");
    case ECrystalType::Opal:
        return LOCTEXT("ECrystalType_Opal", "Opal");
    case ECrystalType::Onyx:
        return LOCTEXT("ECrystalType_Onyx", "Onyx");
    case ECrystalType::Amethyst:
        return LOCTEXT("ECrystalType_Amethyst", "Amethyst");
    case ECrystalType::Iolite:
        return LOCTEXT("ECrystalType_Iolite", "Iolite");
    case ECrystalType::Quartz:
        return LOCTEXT("ECrystalType_Quartz", "Quartz");
    case ECrystalType::DamageStone:
        return LOCTEXT("ECrystalType_DamageStone", "Damage Stone");
    case ECrystalType::HealingStone:
        return LOCTEXT("ECrystalType_HealingStone", "Healing Stone");
    case ECrystalType::MaxHPStone:
        return LOCTEXT("ECrystalType_MaxHPStone", "Max HP Stone");
    case ECrystalType::MaxEPStone:
        return LOCTEXT("ECrystalType_MaxEPStone", "Max EP Stone");
    case ECrystalType::DurabilityStone:
        return LOCTEXT("ECrystalType_DurabilityStone", "Durability Stone");
    case ECrystalType::StatStone:
        return LOCTEXT("ECrystalType_StatStone", "Stat Stone");
    // Retired single-stat kinds (B-353): still named, since an old asset can hold one until it is normalized.
    case ECrystalType::DefenseStone:
        return LOCTEXT("ECrystalType_DefenseStone", "Defense Stone");
    case ECrystalType::CritStone:
        return LOCTEXT("ECrystalType_CritStone", "Crit Stone");
    case ECrystalType::TurnSpeedStone:
        return LOCTEXT("ECrystalType_TurnSpeedStone", "Turn Speed Stone");
    case ECrystalType::StatusStone:
        return LOCTEXT("ECrystalType_StatusStone", "Status Stone");
    case ECrystalType::EfficiencyStone:
        return LOCTEXT("ECrystalType_EfficiencyStone", "Efficiency Stone");
    case ECrystalType::SpellDamageStone:
        return LOCTEXT("ECrystalType_SpellDamageStone", "Spell Damage Stone");
    case ECrystalType::ResistanceStone:
        return LOCTEXT("ECrystalType_ResistanceStone", "Resistance Stone");
    case ECrystalType::SpellSpeedStone:
        return LOCTEXT("ECrystalType_SpellSpeedStone", "Spell Speed Stone");
    case ECrystalType::ActionSpeedStone:
        return LOCTEXT("ECrystalType_ActionSpeedStone", "Action Speed Stone");
    case ECrystalType::LuckStone:
        return LOCTEXT("ECrystalType_LuckStone", "Luck Stone");
    case ECrystalType::ReflexStone:
        return LOCTEXT("ECrystalType_ReflexStone", "Reflex Stone");
    default:
        return FText::GetEmpty();
    }
}

FText WoRGameNames::GetWeaponTypeName(EWeaponType Type)
{
    switch (Type)
    {
    case EWeaponType::Sword:
        return LOCTEXT("EWeaponType_Sword", "Sword");
    case EWeaponType::Greatsword:
        return LOCTEXT("EWeaponType_Greatsword", "Greatsword");
    case EWeaponType::Spear:
        return LOCTEXT("EWeaponType_Spear", "Spear");
    case EWeaponType::Staff:
        return LOCTEXT("EWeaponType_Staff", "Staff");
    case EWeaponType::Dagger:
        return LOCTEXT("EWeaponType_Dagger", "Dagger");
    case EWeaponType::Axe:
        return LOCTEXT("EWeaponType_Axe", "Axe");
    case EWeaponType::Hammer:
        return LOCTEXT("EWeaponType_Hammer", "Hammer");
    case EWeaponType::Bow:
        return LOCTEXT("EWeaponType_Bow", "Bow");
    case EWeaponType::Gauntlets:
        return LOCTEXT("EWeaponType_Gauntlets", "Gauntlets");
    case EWeaponType::Scythe:
        return LOCTEXT("EWeaponType_Scythe", "Scythe");
    case EWeaponType::Gun:
        return LOCTEXT("EWeaponType_Gun", "Gun");
    default:
        return FText::GetEmpty();
    }
}

FText WoRGameNames::GetWieldModeName(EWeaponWieldMode Mode)
{
    switch (Mode)
    {
    case EWeaponWieldMode::Dual:
        return LOCTEXT("EWeaponWieldMode_Dual", "dual");
    case EWeaponWieldMode::OffHandShield:
        return LOCTEXT("EWeaponWieldMode_OffHandShield", "with shield");
    case EWeaponWieldMode::Single:
    default:
        return FText::GetEmpty();
    }
}

FText WoRGameNames::GetSubStatName(ESubStat Stat)
{
    switch (Stat)
    {
    case ESubStat::Efficiency:
        return LOCTEXT("ESubStat_Efficiency", "Efficiency");
    case ESubStat::SpellDamage:
        return LOCTEXT("ESubStat_SpellDamage", "Spell Damage");
    case ESubStat::StatusMultiplier:
        return LOCTEXT("ESubStat_StatusMultiplier", "Status Multiplier");
    case ESubStat::CritDamage:
        return LOCTEXT("ESubStat_CritDamage", "Crit Damage");
    case ESubStat::SpellSpeed:
        return LOCTEXT("ESubStat_SpellSpeed", "Spell Speed");
    case ESubStat::Defense:
        return LOCTEXT("ESubStat_Defense", "Defence");
    case ESubStat::ActionSpeed:
        return LOCTEXT("ESubStat_ActionSpeed", "Action Speed");
    case ESubStat::RawDamage:
        return LOCTEXT("ESubStat_RawDamage", "Raw Damage");
    case ESubStat::Resistance:
        return LOCTEXT("ESubStat_Resistance", "Resistance");
    case ESubStat::TurnSpeed: // OBSOLETE (B-170h): retired; named for an old asset's text only
        return LOCTEXT("ESubStat_TurnSpeed", "Turn Speed");
    case ESubStat::Luck:
        return LOCTEXT("ESubStat_Luck", "Luck");
    case ESubStat::Reflex:
        return LOCTEXT("ESubStat_Reflex", "Reflex");
    default:
        return FText::GetEmpty();
    }
}

FText WoRGameNames::GetTierLetter(EItemTier Tier)
{
    switch (Tier)
    {
    case EItemTier::F_Tier:
        return LOCTEXT("EItemTier_F", "F");
    case EItemTier::E_Tier:
        return LOCTEXT("EItemTier_E", "E");
    case EItemTier::D_Tier:
        return LOCTEXT("EItemTier_D", "D");
    case EItemTier::C_Tier:
        return LOCTEXT("EItemTier_C", "C");
    case EItemTier::B_Tier:
        return LOCTEXT("EItemTier_B", "B");
    case EItemTier::A_Tier:
        return LOCTEXT("EItemTier_A", "A");
    case EItemTier::S_Tier:
        return LOCTEXT("EItemTier_S", "S");
    default:
        return FText::GetEmpty();
    }
}

#undef LOCTEXT_NAMESPACE
