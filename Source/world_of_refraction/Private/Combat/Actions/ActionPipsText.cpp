// ActionPipsText.cpp

#include "Combat/Actions/ActionPipsText.h"

#include "Combat/Actions/ActionPipsRules.h"
#include "Combat/TurnManager.h"
#include "Combat/TurnRotation.h"
#include "Combat/WoRFightSubsystem.h"
#include "Engine/World.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "ActionPips"

EActionCostState ActionPipsText::GetCostState(int32 Cost, int32 CurrentActions, int32 MaxActions)
{
	if (ActionPipsRules::IsCostAboveMax(Cost, MaxActions))
	{
		return EActionCostState::AboveMax;
	}
	if (!ActionPipsRules::CanAfford(Cost, CurrentActions))
	{
		return EActionCostState::NeedsMore;
	}
	return EActionCostState::Affordable;
}

FText ActionPipsText::GetCostRefusedReason(int32 Cost, int32 CurrentActions, int32 MaxActions)
{
	switch (GetCostState(Cost, CurrentActions, MaxActions))
	{
	case EActionCostState::AboveMax:
		return LOCTEXT("CostAboveMax", "Costs more actions than you can hold.");
	case EActionCostState::NeedsMore:
		return FText::Format(LOCTEXT("CostNeeds", "Needs {0} {0}|plural(one=action,other=actions)."), FText::AsNumber(Cost));
	default:
		return FText::GetEmpty();
	}
}

FText ActionPipsText::WithCost(const FText &Label, int32 Cost)
{
	if (Cost <= 0)
	{
		return Label;
	}
	return FText::Format(LOCTEXT("LabelWithCost", "{0} ({1} {1}|plural(one=action,other=actions))"), Label, FText::AsNumber(Cost));
}

FText ActionPipsText::MakePips(int32 CurrentActions, int32 MaxActions)
{
	const int32 Filled = FMath::Max(0, CurrentActions);
	const int32 Slots = FMath::Max(Filled, FMath::Max(0, MaxActions));
	if (Slots > MAX_PIPS_SHOWN)
	{
		return FText::GetEmpty();
	}
	const FText FilledPip = LOCTEXT("PipFilled", "●");
	const FText EmptyPip = LOCTEXT("PipEmpty", "○");
	TArray<FText> Pips;
	for (int32 Index = 0; Index < Slots; ++Index)
	{
		Pips.Add(Index < Filled ? FilledPip : EmptyPip);
	}
	return FText::Join(FText::GetEmpty(), Pips);
}

FText ActionPipsText::MakeMaxActionsText(int32 MaxActions, int32 ActionPointProgress)
{
	if (MaxActions >= ActionPipsConstants::MAX_ACTIONS_CAP)
	{
		return FText::Format(LOCTEXT("MaxAtCap", "Max {0}"), FText::AsNumber(MaxActions));
	}
	return FText::Format(LOCTEXT("MaxWithProgress", "Max {0} (+{1}/{2})"), FText::AsNumber(MaxActions),
						 FText::AsNumber(FMath::Max(0, ActionPointProgress)), FText::AsNumber(ActionPipsConstants::ACTION_POINTS_PER_ACTION));
}

FText ActionPipsText::MakeActionsLine(int32 CurrentActions, int32 MaxActions, int32 ActionPointProgress)
{
	const FText Pips = MakePips(CurrentActions, MaxActions);
	const FText MaxText = MakeMaxActionsText(MaxActions, ActionPointProgress);
	const FText Count = FText::AsNumber(FMath::Max(0, CurrentActions));
	if (Pips.IsEmpty())
	{
		return FText::Format(LOCTEXT("ActionsLineNoPips", "{0} {1} · {2}"), WoRGameNames::GetActionsWord(), Count, MaxText);
	}
	return FText::Format(LOCTEXT("ActionsLine", "{0} {1} {2} · {3}"), WoRGameNames::GetActionsWord(), Pips, Count, MaxText);
}

FText ActionPipsText::GetDefendEndsTurnText()
{
	return LOCTEXT("DefendEndsTurn", "Ends your turn.");
}

FText ActionPipsText::GetEndTurnDescription()
{
	return LOCTEXT("EndTurnDescription", "Ends your turn; the actions left stay banked.");
}

bool ActionPipsText::IsActionPipsFight(const UObject *Context)
{
	const UWorld *World = Context ? Context->GetWorld() : nullptr;
	if (World && World->GetNetMode() == NM_Client)
	{
		return TurnRotation::IsActionPipsOn();
	}
	const UTurnManager *TurnManager = WoRFight::Get<UTurnManager>(Context);
	return TurnManager && TurnManager->IsRotationMode();
}

#undef LOCTEXT_NAMESPACE
