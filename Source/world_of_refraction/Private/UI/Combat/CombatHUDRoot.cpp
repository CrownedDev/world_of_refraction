// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/CombatHUDRoot.h"
#include "UI/Combat/TurnOrderStripWidget.h"
#include "UI/Combat/DefensePromptWidget.h"
#include "CombatActionMenuBase.h"
#include "CombatOrchestrator.h"

void UCombatHUDRoot::InitialiseForCombat(ACombatOrchestrator *Orchestrator)
{
	if (bInitialised)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatHUDRoot] InitialiseForCombat called twice; ignoring second call"));
		return;
	}

	if (!Orchestrator)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatHUDRoot] InitialiseForCombat called with null orchestrator"));
		return;
	}

	CurrentOrchestrator = Orchestrator;

	// Character panels are spawned and owned by BP_CombatOrchestrator, not this HUD.

	// 1. Initialise turn order strip (self-contained — owns its own slot lifecycle + TurnManager binding)
	if (TurnOrderStrip)
	{
		TurnOrderStrip->InitialiseForCombat();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatHUDRoot] No TurnOrderStrip bound — skipping (optional)"));
	}

	// 2. Initialise defense prompt (self-contained — binds to DefenseSystem)
	if (DefensePrompt)
	{
		DefensePrompt->InitialiseForCombat();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatHUDRoot] No DefensePrompt bound — skipping (optional)"));
	}

	// NOTE: CommandMenu is intentionally NOT initialised here.
	// Per April 2026 postmortem (widget-inside-widget GC issue), the command menu
	// remains a separate root viewport widget owned by BP_CombatOrchestrator,
	// not a child of this HUD. The CommandMenu BindWidgetOptional is left null
	// and is only present for future architectural flexibility.

	bInitialised = true;

	UE_LOG(LogTemp, Log, TEXT("[CombatHUDRoot] Initialised"));
}

void UCombatHUDRoot::TeardownForCombatEnd()
{
	if (!bInitialised)
	{
		return;
	}

	// 1. Tear down turn order strip
	if (TurnOrderStrip)
	{
		TurnOrderStrip->TeardownStrip();
	}

	// 2. Tear down defense prompt
	if (DefensePrompt)
	{
		DefensePrompt->TeardownPrompt();
	}

	CurrentOrchestrator.Reset();
	bInitialised = false;

	UE_LOG(LogTemp, Log, TEXT("[CombatHUDRoot] Torn down"));
}

void UCombatHUDRoot::NativeDestruct()
{
	// Defensive — orchestrator should call TeardownForCombatEnd first.
	if (bInitialised)
	{
		TeardownForCombatEnd();
	}
	Super::NativeDestruct();
}

void UCombatHUDRoot::BeginDestroy()
{
	// Last guaranteed callback before GC. NativeDestruct may not fire on PIE end.
	if (bInitialised)
	{
		TeardownForCombatEnd();
	}
	Super::BeginDestroy();
}
