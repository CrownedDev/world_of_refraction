// ActionPipsText.cpp

#include "Combat/Actions/ActionPipsText.h"

#include "Combat/Actions/ActionPipsRules.h"
#include "Combat/TurnManager.h"
#include "Combat/TurnRotation.h"
#include "Combat/WoRFightSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Save/SaveSubsystem.h"
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
		return FText::Format(LOCTEXT("CostNeeds", "Needs {0} {0}|plural(one=action,other=actions)."), Cost);
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
	return FText::Format(LOCTEXT("LabelWithCost", "{0} ({1} {1}|plural(one=action,other=actions))"), Label, Cost);
}

FText ActionPipsText::MakePips(int32 CurrentActions, int32 MaxActions)
{
	const int32 Filled = FMath::Max(0, CurrentActions);
	const int32 Slots = FMath::Max(Filled, FMath::Max(0, MaxActions));
	if (Slots > MAX_PIPS_SHOWN)
	{
		return FText::GetEmpty();
	}
	// Symbols, not words: nothing to translate.
	const FText FilledPip = FText::AsCultureInvariant(TEXT("●"));
	const FText EmptyPip = FText::AsCultureInvariant(TEXT("○"));
	const FText BonusPip = FText::AsCultureInvariant(TEXT("◆"));
	const int32 BankedUpToMax = Filled - GetBonusActions(CurrentActions, MaxActions);
	TArray<FText> Pips;
	for (int32 Index = 0; Index < Slots; ++Index)
	{
		Pips.Add(Index < BankedUpToMax ? FilledPip : Index < Filled ? BonusPip : EmptyPip);
	}
	return FText::Join(FText::GetEmpty(), Pips);
}

int32 ActionPipsText::GetBonusActions(int32 CurrentActions, int32 MaxActions)
{
	return FMath::Max(0, CurrentActions - FMath::Max(0, MaxActions));
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
	const FText Line = Pips.IsEmpty()
						   ? FText::Format(LOCTEXT("ActionsLineNoPips", "{0} {1} · {2}"), WoRGameNames::GetActionsWord(), Count, MaxText)
						   : FText::Format(LOCTEXT("ActionsLine", "{0} {1} {2} · {3}"), WoRGameNames::GetActionsWord(), Pips, Count, MaxText);
	const int32 Bonus = GetBonusActions(CurrentActions, MaxActions);
	if (Bonus == 0)
	{
		return Line;
	}
	return FText::Format(LOCTEXT("ActionsLineBonus", "{0} · {1} bonus, lost at turn end"), Line, FText::AsNumber(Bonus));
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

bool ActionPipsText::ShouldShowEndTurnButton(const UObject *Context)
{
	if (!IsActionPipsFight(Context))
	{
		return false;
	}
	const UWorld *World = Context ? Context->GetWorld() : nullptr;
	const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
	const USaveSubsystem *Save = GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
	return !Save || !Save->IsEndTurnButtonHidden();
}

#undef LOCTEXT_NAMESPACE
