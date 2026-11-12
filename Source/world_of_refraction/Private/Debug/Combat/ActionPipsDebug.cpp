// ActionPipsDebug.cpp

#include "Debug/Combat/ActionPipsDebug.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Actions/ActionPipsRules.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Save/SaveSubsystem.h"
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

	FAutoConsoleCommandWithWorldAndArgs GHideEndTurnCmd(
		TEXT("wor.UI.HideEndTurn"),
		TEXT("PLACEHOLDER setting (B-170e, Crown 2026-11-12, until the D-024 settings screen): 1 = hide the End Turn button, ")
			TEXT("0 = show it (default). Saved; the next main menu reads it. With no argument, prints it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
		{
			UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
			USaveSubsystem *Save = GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
			if (!Save)
			{
				UE_LOG(LogWoRCombat, Warning, TEXT("[ActionPipsDebug] wor.UI.HideEndTurn: no save subsystem."));
				return;
			}
			if (Args.Num() > 0)
			{
				if (!Args[0].IsNumeric())
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[ActionPipsDebug] Usage: wor.UI.HideEndTurn <0|1>"));
					return;
				}
				Save->SetEndTurnButtonHidden(FCString::Atoi(*Args[0]) != 0);
			}
			UE_LOG(LogWoRCombat, Display, TEXT("[ActionPipsDebug] End Turn button: %s (saved with the next save; the next main menu reads it)"),
				   Save->IsEndTurnButtonHidden() ? TEXT("hidden") : TEXT("shown"));
		}));
}

FString UActionPipsDebug::GetActionsString(AActor *Actor)
{
	const UCharacterDataComponent *Char = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!Char)
	{
		return TEXT("no CharacterDataComponent");
	}
	const FWoRActionPointLayers Layers = Char->GetActionPointLayers();
	// B-170d: the Max shown is the live one (Max Actions Buff / Debuff included); the points line keeps its progress.
	// B-170i: the regain with its progress, "Regain 2 (Reflex 18.0 / 30.0)".
	return FString::Printf(TEXT("actions %d / Max %d (from points: %s, effects %+d) | points %s | %s | waiting %s"),
						   Char->CurrentActions, Char->GetMaxActions(), *ActionPipsRules::MaxActionsString(Layers.Compose()),
						   Char->GetMaxActionsChange(), *Layers.ToString(), *Char->GetActionRegainString(),
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
