// WoRGameNames.cpp
// One case per enum value; see WoRGameNames.h.

#include "WoRGameNames.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Grid/ECombatRow.h"
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
        return LOCTEXT("ESkillEffectType_ModifyEnergyCost", "Energy Cost Up");
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
        return LOCTEXT("ESkillEffectType_ExtraAction", "Extra Action");
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

FText GetSpellSchoolName(ESpellSchool School)
{
    switch (School)
    {
    case ESpellSchool::Destruction:
        return LOCTEXT("ESpellSchool_Destruction", "Destruction");
    case ESpellSchool::Enhancement:
        return LOCTEXT("ESpellSchool_Enhancement", "Enhancement");
    case ESpellSchool::Restoration:
        return LOCTEXT("ESpellSchool_Restoration", "Restoration");
    case ESpellSchool::Conjuration:
        return LOCTEXT("ESpellSchool_Conjuration", "Conjuration");
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
        return LOCTEXT("EMerchantType_Jeweler", "Jeweler (rings)");
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
}

#undef LOCTEXT_NAMESPACE
