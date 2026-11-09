// ActionPipsDebug.cpp

#include "Debug/Combat/ActionPipsDebug.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Actions/ActionPipsRules.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float ACTIONS_DEBUG_SCREEN_SECONDS = 8.0f;

	TArray<UCharacterDataComponent *> ActionsDebugCombatants(const UObject *WorldContextObject)
	{
		TArray<UCharacterDataComponent *> Result;
		UWorld *World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		if (!World)
		{
			return Result;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (UCharacterDataComponent *Char = It->FindComponentByClass<UCharacterDataComponent>())
			{
				Result.Add(Char);
			}
		}
		return Result;
	}

	FAutoConsoleCommandWithWorld GPrintActionsCmd(
		TEXT("wor.Actions.Print"),
		TEXT("DEBUG: print each combatant's banked actions, Max Actions (action points and progress) and regain."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UActionPipsDebug::PrintActions(World); }));

	FAutoConsoleCommandWithWorldAndArgs GAddActionPointsCmd(
		TEXT("wor.Actions.AddPoints"),
		TEXT("DEBUG: wor.Actions.AddPoints <N> adds N debug action points to every combatant (negative removes them)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
		{
			if (Args.Num() < 1 || !Args[0].IsNumeric())
			{
				UE_LOG(LogWoRCombat, Warning, TEXT("[ActionPipsDebug] Usage: wor.Actions.AddPoints <N>"));
				return;
			}
			UActionPipsDebug::AddDebugActionPoints(World, FCString::Atoi(*Args[0]));
		}));

	FAutoConsoleCommandWithWorld GResetActionsCmd(
		TEXT("wor.Actions.Reset"),
		TEXT("DEBUG: set every combatant to its fight-start actions (the regain, capped at Max)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UActionPipsDebug::ResetActionsForFight(World); }));
}

FString UActionPipsDebug::GetActionsString(AActor *Actor)
{
	const UCharacterDataComponent *Char = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!Char)
	{
		return TEXT("no CharacterDataComponent");
	}
	const FWoRActionPointLayers Layers = Char->GetActionPointLayers();
	// B-170d: Max after the timed Max Actions buffs / debuffs, and the gains and losses waiting for a turn start.
	return FString::Printf(TEXT("actions %d / %s | Max now %d (buffs %+d) | points %s | regain %d (Reflex share %.2f) | waiting %s"),
						   Char->CurrentActions, *ActionPipsRules::MaxActionsString(Layers.Compose()), Char->GetMaxActions(),
						   Char->GetMaxActionsChange(), *Layers.ToString(), Char->GetActionRegain(), Char->GetReflexRegainShare(),
						   *Char->GetWaitingActionChangesString());
}

void UActionPipsDebug::PrintActions(const UObject *WorldContextObject)
{
	const TArray<UCharacterDataComponent *> Combatants = ActionsDebugCombatants(WorldContextObject);
	if (Combatants.Num() == 0)
	{
		UE_LOG(LogWoRCombat, Log, TEXT("[ActionPipsDebug] No combatants in the world"));
		return;
	}
	for (const UCharacterDataComponent *Char : Combatants)
	{
		const FString Line = FString::Printf(TEXT("%s: %s"), *GetNameSafe(Char->GetOwner()), *GetActionsString(Char->GetOwner()));
		UE_LOG(LogWoRCombat, Log, TEXT("[ActionPipsDebug] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, ACTIONS_DEBUG_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}
	// B-170c: the current turn's spends, from each running fight.
	UWorld *World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	for (TActorIterator<ACombatOrchestrator> It(World); It; ++It)
	{
		if (It->GetCombatState() == ECombatState::InProgress)
		{
			const FString Line = It->GetTurnSpendsString();
			UE_LOG(LogWoRCombat, Log, TEXT("[ActionPipsDebug] %s"), *Line);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(INDEX_NONE, ACTIONS_DEBUG_SCREEN_SECONDS, FColor::Cyan, Line);
			}
		}
	}
}

void UActionPipsDebug::AddDebugActionPoints(const UObject *WorldContextObject, int32 Points)
{
	for (UCharacterDataComponent *Char : ActionsDebugCombatants(WorldContextObject))
	{
		Char->DebugActionPoints += Points;
	}
	PrintActions(WorldContextObject);
}

void UActionPipsDebug::ResetActionsForFight(const UObject *WorldContextObject)
{
	for (UCharacterDataComponent *Char : ActionsDebugCombatants(WorldContextObject))
	{
		Char->ResetActionsForFight();
	}
	PrintActions(WorldContextObject);
}
