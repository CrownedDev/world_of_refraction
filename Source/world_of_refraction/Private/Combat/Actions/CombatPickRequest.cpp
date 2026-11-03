// CombatPickRequest.cpp
// B-229. See CombatPickRequest.h.

#include "Combat/Actions/CombatPickRequest.h"
#include "Infusion/InfusionConstants.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "GameFramework/Actor.h"

namespace CombatPickInternal
{
	bool CarriesInfusion(EActionType Type)
	{
		return Type == EActionType::Spell || Type == EActionType::Ability;
	}

	template <typename TEnum>
	bool IsDefinedValue(TEnum Value)
	{
		const UEnum *Enum = StaticEnum<TEnum>();
		// IsValidEnumValue also accepts the generated _MAX entry; refuse it.
		return Enum && Enum->IsValidEnumValue(static_cast<int64>(Value)) && static_cast<int64>(Value) < Enum->GetMaxEnumValue();
	}
}

FCombatPickRequest FCombatPickRequest::FromAction(AActor *InActor, const FAction &Action)
{
	FCombatPickRequest Pick;
	Pick.Actor = InActor;
	Pick.ActionType = Action.ActionType;
	Pick.SpellData = Action.SpellData;
	Pick.SkillData = Action.SkillData;
	Pick.ItemData = Action.ItemData;
	Pick.SpellSource = Action.SpellSource;
	Pick.Targets.Reserve(Action.Targets.Num());
	for (AActor *Target : Action.Targets)
	{
		Pick.Targets.Add(Target);
	}
	Pick.InfusionSource = Action.SelectedSource;
	Pick.InfusionLevel = Action.GetChargeLevel();
	return Pick;
}

FAction FCombatPickRequest::ToAction() const
{
	FAction Action;
	Action.ActionType = ActionType;
	Action.SpellData = SpellData;
	Action.SkillData = SkillData;
	Action.ItemData = ItemData;
	Action.SpellSource = SpellSource;
	Action.Targets.Reserve(Targets.Num());
	for (const TObjectPtr<AActor> &Target : Targets)
	{
		Action.Targets.Add(Target.Get());
	}

	if (CombatPickInternal::CarriesInfusion(ActionType))
	{
		const int32 Level = FMath::Clamp(InfusionLevel, 0, InfusionConstants::MAX_INFUSION_LEVEL);
		Action.SelectedSource = InfusionSource;
		if (ActionType == EActionType::Spell)
		{
			Action.SpellInfusionLevel = Level;
		}
		else
		{
			Action.AbilityInfusionLevel = Level;
		}
	}
	return Action;
}

bool FCombatPickRequest::HasValidEnums() const
{
	return CombatPickInternal::IsDefinedValue(ActionType) && CombatPickInternal::IsDefinedValue(SpellSource) &&
		   CombatPickInternal::IsDefinedValue(InfusionSource) && CombatPickInternal::IsDefinedValue(ItemData.Type) &&
		   CombatPickInternal::IsDefinedValue(ItemData.Tier);
}

FString FCombatPickRequest::GetDebugString() const
{
	return FString::Printf(TEXT("%s: %s -> %d target(s), infusion %d (%s)"), *GetNameSafe(Actor.Get()),
						   *UEnum::GetValueAsString(ActionType), Targets.Num(), InfusionLevel,
						   *UEnum::GetValueAsString(InfusionSource));
}
