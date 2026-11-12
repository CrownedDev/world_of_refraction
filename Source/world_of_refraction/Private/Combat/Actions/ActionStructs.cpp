// ActionStructs.cpp
// Out-of-line FAction members that need the full skill/data types (the header only forward-declares
// them). UBT auto-includes this in the world_of_refraction module — no .Build.cs change needed.

#include "Combat/Actions/ActionStructs.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Combat/Actions/ActionPipsRules.h"

FString FAction::GetActionName() const
{
	switch (ActionType)
	{
	case EActionType::Spell:
		return SpellData ? TEXT("Spell") : TEXT("Unknown Spell");
	case EActionType::Ability:
		// Attacks fold into Ability (attack/ability merge); IsAttack() on the merged SkillData pointer
		// distinguishes a basic weapon attack from a slottable ability — restoring the "Basic Attack"
		// label the header-inline version couldn't produce (forward-declared type, no virtual call).
		if (!SkillData)
			return TEXT("Unknown Ability");
		return SkillData->IsAttack() ? TEXT("Basic Attack") : TEXT("Ability");
	case EActionType::Item:
		return ItemIdentity::GetDisplayName(ItemData);
	case EActionType::Defend:
		return TEXT("Defend");
	case EActionType::SwitchWeapon:
		return TEXT("Switch Weapon");
	case EActionType::Flee:
		return TEXT("Flee");
	case EActionType::EndTurn:
		return TEXT("End Turn");
	default:
		return TEXT("Unknown Action");
	}
}

int32 FAction::GetActionCost() const
{
	if (bIsDeferredFire)
	{
		return 0; // the ritual's cost was paid when it was armed
	}
	switch (ActionType)
	{
	case EActionType::Spell:
		return ActionPipsRules::ClampActionCost(SpellData ? SpellData->ActionCost : ActionPipsConstants::ACTION_COST_DEFAULT); // Q-170j
	case EActionType::Ability:
		return ActionPipsRules::ClampActionCost(SkillData ? SkillData->ActionCost : ActionPipsConstants::ACTION_COST_DEFAULT); // Q-170j
	case EActionType::Item:
		return ActionPipsRules::ClampActionCost(CrystalEffectTable::GetActionCost(ItemData)); // Q-170j
	default:
		return 0; // Defend, End Turn, Switch Weapon, Flee (Q-170c)
	}
}
