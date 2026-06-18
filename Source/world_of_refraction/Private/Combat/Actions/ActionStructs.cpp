// ActionStructs.cpp
// Out-of-line FAction members that need the full skill/data types (the header only forward-declares
// them). UBT auto-includes this in the world_of_refraction module — no .Build.cs change needed.

#include "Combat/Actions/ActionStructs.h"
#include "Skills/Definitions/SkillDataBase.h"

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
	default:
		return TEXT("Unknown Action");
	}
}
