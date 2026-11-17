// WoRGameNames.cpp
// One case per enum value; see WoRGameNames.h.

#include "WoRGameNames.h"
#include "Inventory/InventoryVerbs.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Character/EWorldPillar.h"
#include "Character/Forms/CharacterFormData.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Grid/ECombatRow.h"
#include "Currency/CurrencyTypes.h"
#include "Equipment/Crystals/CrystalType.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Hub/InteractPrompt.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Field/FieldSkillTypes.h"
#include "Equipment/Weapons/EWeaponWieldMode.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/ItemTier.h"
#include "Memory/HiddenQuestData.h"
#include "Merchant/MerchantData.h"
#include "Shop/VendorServices.h"
#include "Trial/TrialRestSpot.h"
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
        return LOCTEXT("ESkillEffectType_DOT", "DoT");
    case ESkillEffectType::SpeedDebuff:
        return LOCTEXT("ESkillEffectType_SpeedDebuff", "Speed Down");
    case ESkillEffectType::DefenseDebuff:
        return LOCTEXT("ESkillEffectType_DefenseDebuff", "Defence Down");
    case ESkillEffectType::CritDebuff:
        return LOCTEXT("ESkillEffectType_CritDebuff", "Crit Chance Down");
    case ESkillEffectType::EnergyDebuff:
        return LOCTEXT("ESkillEffectType_EnergyDebuff", "EP Down");
    case ESkillEffectType::SkipTurn:
        return LOCTEXT("ESkillEffectType_SkipTurn", "Skip Turn");
    case ESkillEffectType::RandomDebuff:
        return LOCTEXT("ESkillEffectType_RandomDebuff", "Random Stat Down");
    case ESkillEffectType::BurstDamage:
        return LOCTEXT("ESkillEffectType_BurstDamage", "Burst Damage");
    case ESkillEffectType::MindBuff:
        return LOCTEXT("ESkillEffectType_MindBuff", "Mind Up");
    case ESkillEffectType::MindDebuff:
        return LOCTEXT("ESkillEffectType_MindDebuff", "Mind Down");
    case ESkillEffectType::BodyBuff:
        return LOCTEXT("ESkillEffectType_BodyBuff", "Body Up");
    case ESkillEffectType::BodyDebuff:
        return LOCTEXT("ESkillEffectType_BodyDebuff", "Body Down");
    case ESkillEffectType::SpiritBuff:
        return LOCTEXT("ESkillEffectType_SpiritBuff", "Spirit Up");
    case ESkillEffectType::SpiritDebuff:
        return LOCTEXT("ESkillEffectType_SpiritDebuff", "Spirit Down");
    case ESkillEffectType::SpellCostBuff:
        return LOCTEXT("ESkillEffectType_SpellCostBuff", "Spell Cost Up");
    case ESkillEffectType::SpellCostDebuff:
        return LOCTEXT("ESkillEffectType_SpellCostDebuff", "Spell Cost Down");
    case ESkillEffectType::SpellDamageBuff:
        return LOCTEXT("ESkillEffectType_SpellDamageBuff", "Spell Damage Up");
    case ESkillEffectType::SpellDamageDebuff:
        return LOCTEXT("ESkillEffectType_SpellDamageDebuff", "Spell Damage Down");
    case ESkillEffectType::StatusMultiplierBuff:
        return LOCTEXT("ESkillEffectType_StatusMultiplierBuff", "Status Multiplier Up");
    case ESkillEffectType::StatusMultiplierDebuff:
        return LOCTEXT("ESkillEffectType_StatusMultiplierDebuff", "Status Multiplier Down");
    case ESkillEffectType::CritChanceBuff:
        return LOCTEXT("ESkillEffectType_CritChanceBuff", "Crit Chance Up");
    case ESkillEffectType::CritChanceDebuff:
        return LOCTEXT("ESkillEffectType_CritChanceDebuff", "Crit Chance Down");
    case ESkillEffectType::SpellSpeedBuff:
        return LOCTEXT("ESkillEffectType_SpellSpeedBuff", "Spell Speed Up");
    case ESkillEffectType::SpellSpeedDebuff:
        return LOCTEXT("ESkillEffectType_SpellSpeedDebuff", "Spell Speed Down");
    case ESkillEffectType::DefenseBuff:
        return LOCTEXT("ESkillEffectType_DefenseBuff", "Defence Up");
    case ESkillEffectType::ActionSpeedBuff:
        return LOCTEXT("ESkillEffectType_ActionSpeedBuff", "Action Speed Up");
    case ESkillEffectType::ActionSpeedDebuff:
        return LOCTEXT("ESkillEffectType_ActionSpeedDebuff", "Action Speed Down");
    case ESkillEffectType::RawDamageBuff:
        return LOCTEXT("ESkillEffectType_RawDamageBuff", "Raw Damage Up");
    case ESkillEffectType::RawDamageDebuff:
        return LOCTEXT("ESkillEffectType_RawDamageDebuff", "Raw Damage Down");
    case ESkillEffectType::MaxEnergyBuff:
        return LOCTEXT("ESkillEffectType_MaxEnergyBuff", "Max EP Up");
    case ESkillEffectType::MaxEnergyDebuff:
        return LOCTEXT("ESkillEffectType_MaxEnergyDebuff", "Max EP Down");
    case ESkillEffectType::ResistanceBuff:
        return LOCTEXT("ESkillEffectType_ResistanceBuff", "Resistance Up");
    case ESkillEffectType::ResistanceDebuff:
        return LOCTEXT("ESkillEffectType_ResistanceDebuff", "Resistance Down");
    case ESkillEffectType::SpellSizeBuff:
        return LOCTEXT("ESkillEffectType_SpellSizeBuff", "Spell Size Up");
    case ESkillEffectType::SpellSizeDebuff:
        return LOCTEXT("ESkillEffectType_SpellSizeDebuff", "Spell Size Down");
    case ESkillEffectType::TurnSpeedBuff:
        return LOCTEXT("ESkillEffectType_TurnSpeedBuff", "Turn Speed Up");
    case ESkillEffectType::TurnSpeedDebuff:
        return LOCTEXT("ESkillEffectType_TurnSpeedDebuff", "Turn Speed Down");
    case ESkillEffectType::LuckBuff:
        return LOCTEXT("ESkillEffectType_LuckBuff", "Luck Up");
    case ESkillEffectType::LuckDebuff:
        return LOCTEXT("ESkillEffectType_LuckDebuff", "Luck Down");
    case ESkillEffectType::DamageBuff:
        return LOCTEXT("ESkillEffectType_DamageBuff", "Damage Up");
    case ESkillEffectType::DamageDebuff:
        return LOCTEXT("ESkillEffectType_DamageDebuff", "Damage Down");
    case ESkillEffectType::SpeedBuff:
        return LOCTEXT("ESkillEffectType_SpeedBuff", "Speed Up");
    case ESkillEffectType::EnergyRestore:
        return LOCTEXT("ESkillEffectType_EnergyRestore", "EP Restore");
    case ESkillEffectType::EnergyDrain:
        return LOCTEXT("ESkillEffectType_EnergyDrain", "EP Drain");
    case ESkillEffectType::HealthRestore:
        return LOCTEXT("ESkillEffectType_HealthRestore", "Health Restore");
    case ESkillEffectType::Heal:
        return LOCTEXT("ESkillEffectType_Heal", "Heal");
    case ESkillEffectType::RetaliationDamage:
        return LOCTEXT("ESkillEffectType_RetaliationDamage", "Retaliation Damage");
    case ESkillEffectType::SelfDamage:
        return LOCTEXT("ESkillEffectType_SelfDamage", "Self Damage");
    case ESkillEffectType::Cleanse:
        return LOCTEXT("ESkillEffectType_Cleanse", "Cleanse");
    case ESkillEffectType::RemoveSpeedDebuff:
        return LOCTEXT("ESkillEffectType_RemoveSpeedDebuff", "Remove Speed Debuff");
    case ESkillEffectType::RemoveDamageDebuff:
        return LOCTEXT("ESkillEffectType_RemoveDamageDebuff", "Remove Damage Debuff");
    case ESkillEffectType::RemoveDefenseDebuff:
        return LOCTEXT("ESkillEffectType_RemoveDefenseDebuff", "Remove Defence Debuff");
    case ESkillEffectType::Stun:
        return LOCTEXT("ESkillEffectType_Stun", "Stun");
    case ESkillEffectType::HealBlock:
        return LOCTEXT("ESkillEffectType_HealBlock", "Heal Block");
    case ESkillEffectType::Silenced:
        return LOCTEXT("ESkillEffectType_Silenced", "Silenced");
    case ESkillEffectType::RandomSkill:
        return LOCTEXT("ESkillEffectType_RandomSkill", "Confuse");
    case ESkillEffectType::ModifyDamageDealt:
        return LOCTEXT("ESkillEffectType_ModifyDamageDealt", "Damage Up");
    case ESkillEffectType::ReduceDamageTaken:
        return LOCTEXT("ESkillEffectType_ReduceDamageTaken", "Damage Taken Down");
    case ESkillEffectType::ModifyHealing:
        return LOCTEXT("ESkillEffectType_ModifyHealing", "Healing Up");
    case ESkillEffectType::ModifyCritChance:
        return LOCTEXT("ESkillEffectType_ModifyCritChance", "Crit Chance Up");
    case ESkillEffectType::ModifyCritDamage:
        return LOCTEXT("ESkillEffectType_ModifyCritDamage", "Crit Damage Up");
    case ESkillEffectType::ModifyEnergyCost:
        return LOCTEXT("ESkillEffectType_ModifyEnergyCost", "EP Cost Down");
    case ESkillEffectType::ModifyTurnSpeed:
        return LOCTEXT("ESkillEffectType_ModifyTurnSpeed", "Turn Speed Up");
    case ESkillEffectType::ModifyStatusResist:
        return LOCTEXT("ESkillEffectType_ModifyStatusResist", "Status Resist Up");
    case ESkillEffectType::RestoreHPPercent:
        return LOCTEXT("ESkillEffectType_RestoreHPPercent", "Restore HP %");
    case ESkillEffectType::RestoreEnergyPercent:
        return LOCTEXT("ESkillEffectType_RestoreEnergyPercent", "Restore EP %");
    case ESkillEffectType::DrainHP:
        return LOCTEXT("ESkillEffectType_DrainHP", "Drain HP");
    case ESkillEffectType::DrainEnergy:
        return LOCTEXT("ESkillEffectType_DrainEnergy", "Drain EP");
    case ESkillEffectType::DamageReflect:
        return LOCTEXT("ESkillEffectType_DamageReflect", "Reflect");
    case ESkillEffectType::ReflectPhysicalDamage:
        return LOCTEXT("ESkillEffectType_ReflectPhysicalDamage", "Reflect Physical");
    case ESkillEffectType::ReflectSpellDamage:
        return LOCTEXT("ESkillEffectType_ReflectSpellDamage", "Reflect Spell");
    case ESkillEffectType::Lifesteal:
        return LOCTEXT("ESkillEffectType_Lifesteal", "Lifesteal");
    case ESkillEffectType::AbsorbDamage:
        return LOCTEXT("ESkillEffectType_AbsorbDamage", "Barrier");
    case ESkillEffectType::GrantDOTImmunity:
        return LOCTEXT("ESkillEffectType_GrantDOTImmunity", "DoT Immunity");
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
        return LOCTEXT("ESkillEffectType_IgnoreDefense", "Ignore Defence");
    case ESkillEffectType::DoubleHit:
        return LOCTEXT("ESkillEffectType_DoubleHit", "Double Hit");
    case ESkillEffectType::LastStand:
        return LOCTEXT("ESkillEffectType_LastStand", "Last Stand");
    case ESkillEffectType::IncreaseDamageTaken:
        return LOCTEXT("ESkillEffectType_IncreaseDamageTaken", "Damage Taken Up");
    case ESkillEffectType::StatusIncrease:
        return LOCTEXT("ESkillEffectType_StatusIncrease", "Buildup Up");
    case ESkillEffectType::StatusDecrease:
        return LOCTEXT("ESkillEffectType_StatusDecrease", "Buildup Down");
    case ESkillEffectType::EfficiencyBuff:
        return LOCTEXT("ESkillEffectType_EfficiencyBuff", "Efficiency Up");
    case ESkillEffectType::EfficiencyDebuff:
        return LOCTEXT("ESkillEffectType_EfficiencyDebuff", "Efficiency Down");
    case ESkillEffectType::MaxHPBuff:
        return LOCTEXT("ESkillEffectType_MaxHPBuff", "Max HP Up");
    case ESkillEffectType::MaxHPDebuff:
        return LOCTEXT("ESkillEffectType_MaxHPDebuff", "Max HP Down");
    case ESkillEffectType::CritDamageDebuff:
        return LOCTEXT("ESkillEffectType_CritDamageDebuff", "Crit Damage Down");
    case ESkillEffectType::ReflexBuff:
        return LOCTEXT("ESkillEffectType_ReflexBuff", "Reflex Up");
    case ESkillEffectType::ReflexDebuff:
        return LOCTEXT("ESkillEffectType_ReflexDebuff", "Reflex Down");
    case ESkillEffectType::HealingReceivedBuff:
        return LOCTEXT("ESkillEffectType_HealingReceivedBuff", "Healing Boost");
    case ESkillEffectType::PushBack:
        return LOCTEXT("ESkillEffectType_PushBack", "Push Back");
    case ESkillEffectType::PullForward:
        return LOCTEXT("ESkillEffectType_PullForward", "Pull Forward");
    case ESkillEffectType::GrantElementImmunity:
        return LOCTEXT("ESkillEffectType_GrantElementImmunity", "Element Immunity");
    case ESkillEffectType::ApplyDOTOnHit:
        return LOCTEXT("ESkillEffectType_ApplyDOTOnHit", "Apply DoT");
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

FText GetBaseFormName(EPlayerBaseForm Form)
{
    return Form == EPlayerBaseForm::Quinn ? LOCTEXT("EPlayerBaseForm_Quinn", "Female Nobody") : LOCTEXT("EPlayerBaseForm_Manny", "Male Nobody");
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

FText GetFieldMovementName(EFieldMovementType Type)
{
    switch (Type)
    {
    case EFieldMovementType::Dash:
        return LOCTEXT("FieldMove_Dash", "Dash");
    case EFieldMovementType::EnhancedSprint:
        return LOCTEXT("FieldMove_EnhancedSprint", "Enhanced Sprint");
    case EFieldMovementType::Glide:
        return LOCTEXT("FieldMove_Glide", "Glide");
    case EFieldMovementType::Flight:
        return LOCTEXT("FieldMove_Flight", "Flight");
    case EFieldMovementType::Teleport:
        return LOCTEXT("FieldMove_Teleport", "Teleport");
    case EFieldMovementType::HighJump:
        return LOCTEXT("FieldMove_HighJump", "High Jump");
    case EFieldMovementType::AirDash:
        return LOCTEXT("FieldMove_AirDash", "Air Dash");
    case EFieldMovementType::SuperLaunch:
        return LOCTEXT("FieldMove_SuperLaunch", "Super Launch");
    case EFieldMovementType::PhaseStep:
        return LOCTEXT("FieldMove_PhaseStep", "Phase Step");
    case EFieldMovementType::MarkAndRecall:
        return LOCTEXT("FieldMove_MarkAndRecall", "Mark and Recall");
    case EFieldMovementType::Regroup:
        return LOCTEXT("FieldMove_Regroup", "Regroup");
    }
    return FText::GetEmpty();
}

FText GetQuestKindName(EQuestKind Kind)
{
    switch (Kind)
    {
    case EQuestKind::Hidden:
        return LOCTEXT("EQuestKind_Hidden", "Secret");
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
        return LOCTEXT("EMerchantType_Blacksmith", "Blacksmith");
    case EMerchantType::Jeweler:
        return LOCTEXT("EMerchantType_Jeweler", "Jeweler");
    case EMerchantType::CombatMaster:
        return LOCTEXT("EMerchantType_CombatMaster", "Combat Master");
    case EMerchantType::SpellShop:
        return LOCTEXT("EMerchantType_SpellShop", "Spell Shop");
    case EMerchantType::Spiritualist:
        return LOCTEXT("EMerchantType_Spiritualist", "Spiritualist");
    case EMerchantType::StoneSmith:
        return LOCTEXT("EMerchantType_StoneSmith", "Stone Smith");
    case EMerchantType::LoadoutVendor:
        return LOCTEXT("EMerchantType_LoadoutVendor", "Loadout Vendor");
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

FText GetHubPlaceName()
{
    return LOCTEXT("HubPlaceName", "The Hub");
}

FText GetYouWord()
{
    return LOCTEXT("You", "You");
}

FText GetYourTurnText()
{
    return LOCTEXT("YourTurn", "Your turn");
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
    case ECurrencyType::SkillEssence: // B-550 (D-235): one balance
        return LOCTEXT("ECurrencyType_DormantEssence", "Dormant Essence");
    case ECurrencyType::ElementalEssence:
        return LOCTEXT("ECurrencyType_ElementalEssence", "Elemental Essence");
    case ECurrencyType::PillarEssence:
        return LOCTEXT("ECurrencyType_PillarEssence", "Stat Essence"); // D-235: the player calls it Stat essence
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
        return LOCTEXT("ECrystalType_DurabilityStone", "Wear Stone");
    case ECrystalType::StatStone:
        return LOCTEXT("ECrystalType_StatStone", "Stat Stone");
    // Retired single-stat kinds (B-353): still named, since an old asset can hold one until it is normalized.
    case ECrystalType::DefenseStone:
        return LOCTEXT("ECrystalType_DefenseStone", "Defence Stone");
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
    case EWeaponType::Shield:
        return LOCTEXT("EWeaponType_Shield", "Shield");
    default:
        return FText::GetEmpty();
    }
}

namespace
{
    /** Q-513: a pair of one type. Gauntlets are a pair already; a bow is never dual. */
    FText GetDualWeaponTypeName(EWeaponType Type)
    {
        switch (Type)
        {
        case EWeaponType::Sword:
            return LOCTEXT("DualWeapon_Sword", "Dual Swords");
        case EWeaponType::Greatsword:
            return LOCTEXT("DualWeapon_Greatsword", "Dual Greatswords");
        case EWeaponType::Spear:
            return LOCTEXT("DualWeapon_Spear", "Dual Spears");
        case EWeaponType::Staff:
            return LOCTEXT("DualWeapon_Staff", "Dual Staves");
        case EWeaponType::Dagger:
            return LOCTEXT("DualWeapon_Dagger", "Dual Daggers");
        case EWeaponType::Axe:
            return LOCTEXT("DualWeapon_Axe", "Dual Axes");
        case EWeaponType::Hammer:
            return LOCTEXT("DualWeapon_Hammer", "Dual Hammers");
        case EWeaponType::Scythe:
            return LOCTEXT("DualWeapon_Scythe", "Dual Scythes");
        case EWeaponType::Gun:
            return LOCTEXT("DualWeapon_Gun", "Dual Guns");
        default:
            return WoRGameNames::GetWeaponTypeName(Type); // Gauntlets, Bow
        }
    }
}

FText WoRGameNames::GetHeldWeaponTypeName(const UWeaponData *Weapon, const UWeaponData *OffHand)
{
    if (!Weapon)
    {
        return FText::GetEmpty();
    }
    // B-515: a shield piece in the off hand; the OffHandShield wield mode is the OBSOLETE baked-in shield.
    if ((OffHand && OffHand->WeaponType == EWeaponType::Shield) || Weapon->WieldMode == EWeaponWieldMode::OffHandShield)
    {
        return FText::Format(LOCTEXT("WeaponWithShield", "{0} and {1}"), GetWeaponTypeName(Weapon->WeaponType),
                             GetWeaponTypeName(EWeaponType::Shield));
    }
    return OffHand || Weapon->WieldMode == EWeaponWieldMode::Dual ? GetDualWeaponTypeName(Weapon->WeaponType)
                                                                  : GetWeaponTypeName(Weapon->WeaponType);
}

FText WoRGameNames::GetHeldPiecesName(const UWeaponData *MainHand, const UWeaponData *OffHand)
{
    const auto PieceName = [](const UWeaponData *Piece)
    {
        return Piece->GetDisplayName().IsEmpty() ? GetWeaponTypeName(Piece->WeaponType) : FText::FromString(Piece->GetDisplayName());
    };
    if (!MainHand)
    {
        return FText::GetEmpty();
    }
    return OffHand ? FText::Format(LOCTEXT("HeldPiecesPlus", "{0} + {1}"), PieceName(MainHand), PieceName(OffHand)) : PieceName(MainHand); // Crown: a plus, not "and"
}

FText WoRGameNames::GetAdapterName(const URingData *Adapter, const FRuntimeAttachedItem &Socket)
{
    if (!Adapter)
    {
        return FText::GetEmpty();
    }
    const FText EmptyAdapterName = FText::Format(LOCTEXT("AdapterBySocket", "{0} {1}"), GetAdapterWord(), GetAdapterFormName(Adapter->Form));
    // D-226 (2): an adapter with a name of its own (its Name, not empty, not the class default, not the empty adapter's
    // own reading) shows it whatever is in the socket, as "King's Ring".
    const bool bOwnName = !Adapter->Name.IsEmpty() && Adapter->Name != GetDefault<URingData>()->Name &&
                          Adapter->Name != EmptyAdapterName.ToString();
    if (bOwnName)
    {
        return FText::FromString(Adapter->Name);
    }
    FText InSocket;
    if (Socket.IsEvolution() && Socket.Evolution.Item)
    {
        const UEvolutionItemData *Item = Socket.Evolution.Item;
        InSocket = Item->ItemName.IsEmpty() ? GetCrystalTypeName(Item->CrystalType) : FText::FromString(Item->ItemName);
    }
    else if (!Socket.IsEmpty() && !Socket.IsEvolution() && Socket.GetElement() != ESpellElement::None)
    {
        InSocket = GetElementName(Socket.GetElement());
    }
    // The form word last, as "Phoenix Ring": an empty one is "Adapter Ring" (D-223).
    return InSocket.IsEmpty() ? EmptyAdapterName : FText::Format(LOCTEXT("AdapterBySocket", "{0} {1}"), InSocket, GetAdapterFormName(Adapter->Form));
}

void WoRGameNames::NumberRepeatedNames(TArray<FText> &Names, TConstArrayView<EItemTier> Tiers, bool bTierLetter)
{
    const bool bByTier = Tiers.Num() == Names.Num();
    constexpr int32 NO_TIER = -1;
    const auto TierOf = [&](int32 Index) { return bByTier ? static_cast<int32>(Tiers[Index]) : NO_TIER; };
    // The strip's letter: which names are held by more than one tier.
    TMap<FString, TSet<int32>> TiersByName;
    for (int32 Index = 0; bTierLetter && Index < Names.Num(); ++Index)
    {
        if (!Names[Index].IsEmpty())
        {
            TiersByName.FindOrAdd(Names[Index].ToString()).Add(TierOf(Index));
        }
    }
    TMap<FString, int32> Seen;
    for (int32 Index = 0; Index < Names.Num(); ++Index)
    {
        FText &Name = Names[Index];
        if (Name.IsEmpty())
        {
            continue;
        }
        const FString Key = Name.ToString();
        const int32 Count = ++Seen.FindOrAdd(FString::Printf(TEXT("%s|%d"), *Key, TierOf(Index)));
        if (bByTier && TiersByName.Contains(Key) && TiersByName[Key].Num() > 1 && !GetTierLetter(Tiers[Index]).IsEmpty())
        {
            Name = FText::Format(LOCTEXT("RepeatedNameTier", "{0}  {1}"), Name, GetTierLetter(Tiers[Index]));
        }
        if (Count > 1)
        {
            Name = FText::Format(LOCTEXT("RepeatedName", "{0} {1}"), Name, FText::AsNumber(Count));
        }
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

FText WoRGameNames::GetVendorServiceName(EVendorService Service)
{
    switch (Service)
    {
    case EVendorService::Sell:
        return LOCTEXT("EVendorService_Sell", "Sell");
    case EVendorService::Dismantle:
        return LOCTEXT("EVendorService_Dismantle", "Dismantle");
    default:
        return FText::GetEmpty();
    }
}

FText WoRGameNames::GetInteractVerbName(EInteractVerb Verb)
{
    switch (Verb)
    {
    case EInteractVerb::Talk:
        return LOCTEXT("EInteractVerb_Talk", "Talk");
    case EInteractVerb::Enter:
        return LOCTEXT("EInteractVerb_Enter", "Enter");
    case EInteractVerb::Return:
        return LOCTEXT("EInteractVerb_Return", "Return");
    case EInteractVerb::Invite:
        return LOCTEXT("EInteractVerb_Invite", "Invite");
    case EInteractVerb::Dismiss:
        return LOCTEXT("EInteractVerb_Dismiss", "Dismiss");
    case EInteractVerb::Open:
        return LOCTEXT("EInteractVerb_Open", "Open");
    case EInteractVerb::Read:
        return LOCTEXT("EInteractVerb_Read", "Read");
    case EInteractVerb::Challenge:
        return LOCTEXT("EInteractVerb_Challenge", "Challenge");
    case EInteractVerb::PickUp:
        return LOCTEXT("EInteractVerb_PickUp", "Pick up");
    case EInteractVerb::Rest:
        return LOCTEXT("EInteractVerb_Rest", "Rest");
    case EInteractVerb::None:
    default:
        return FText::GetEmpty();
    }
}

FText WoRGameNames::GetRestServiceName(ERestService Service)
{
    // Placeholder words until the art names the objects (D-214: an anvil; a ritual tree; a shrine; V himself or his mirror).
    switch (Service)
    {
    case ERestService::Blacksmith:
        return LOCTEXT("ERestService_Blacksmith", "Anvil");
    case ERestService::Spiritualist:
        return LOCTEXT("ERestService_Spiritualist", "Ritual tree");
    case ERestService::Jeweler:
        return LOCTEXT("ERestService_Jeweler", "Crystal pedestal");
    case ERestService::PillarShrine:
        return LOCTEXT("ERestService_PillarShrine", "Shrine");
    case ERestService::Extract:
        return LOCTEXT("ERestService_Extract", "V");
    case ERestService::None:
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

FText WoRGameNames::GetElementLetter(ESpellElement Element)
{
    switch (Element)
    {
    case ESpellElement::Generic:
        return LOCTEXT("ElementLetter_Generic", "Ge");
    case ESpellElement::Fire:
        return LOCTEXT("ElementLetter_Fire", "Fi");
    case ESpellElement::Water:
        return LOCTEXT("ElementLetter_Water", "Wa");
    case ESpellElement::Earth:
        return LOCTEXT("ElementLetter_Earth", "Ea");
    case ESpellElement::Wind:
        return LOCTEXT("ElementLetter_Wind", "Wi");
    case ESpellElement::Light:
        return LOCTEXT("ElementLetter_Light", "Li");
    case ESpellElement::Darkness:
        return LOCTEXT("ElementLetter_Darkness", "Da");
    case ESpellElement::Lightning:
        return LOCTEXT("ElementLetter_Lightning", "Lt");
    case ESpellElement::Void:
        return LOCTEXT("ElementLetter_Void", "Vo");
    case ESpellElement::Reality:
        return LOCTEXT("ElementLetter_Reality", "?"); // D-095: players never see "Reality"
    case ESpellElement::None:
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

FText WoRGameNames::GetPillarName(EWorldPillar Pillar)
{
    switch (Pillar)
    {
    case EWorldPillar::Mind:
        return LOCTEXT("EWorldPillar_Mind", "Mind");
    case EWorldPillar::Body:
        return LOCTEXT("EWorldPillar_Body", "Body");
    case EWorldPillar::Spirit:
    default:
        return LOCTEXT("EWorldPillar_Spirit", "Spirit");
    }
}

#undef LOCTEXT_NAMESPACE
