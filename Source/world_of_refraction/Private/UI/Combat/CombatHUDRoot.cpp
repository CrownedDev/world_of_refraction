// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/CombatHUDRoot.h"
#include "UI/Combat/CharacterPanelWidget.h"
#include "UI/Combat/TurnOrderStripWidget.h"
#include "UI/Combat/DefensePromptWidget.h"
#include "CombatActionMenuBase.h"
#include "CombatOrchestrator.h"
#include "Components/PanelWidget.h"

void UCombatHUDRoot::InitialiseForCombat(ACombatOrchestrator* Orchestrator,
                                         const TArray<AActor*>& Team0,
                                         const TArray<AActor*>& Team1)
{
	if (bInitialised)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatHUDRoot] InitialiseForCombat called twice; ignoring second call"));
		return;
	}

	CurrentOrchestrator = Orchestrator;

	// TODO Phase 1: spawn character panels into Team0Panel and Team1Panel
	// TODO Phase 1: TurnOrderStrip->InitialiseForCombat()
	// TODO Phase 1: DefensePrompt->InitialiseForCombat()
	// TODO Phase 1: CommandMenu native binding (existing pattern, unchanged)

	bInitialised = true;
	UE_LOG(LogTemp, Log, TEXT("[CombatHUDRoot] Initialised (stub) — Team0=%d Team1=%d"),
	       Team0.Num(), Team1.Num());
}

void UCombatHUDRoot::TeardownForCombatEnd()
{
	if (!bInitialised)
	{
		return;
	}

	// TODO Phase 1: panel teardown loop
	// TODO Phase 1: TurnOrderStrip->TeardownStrip
	// TODO Phase 1: DefensePrompt->TeardownPrompt

	SpawnedPanels.Reset();
	CurrentOrchestrator.Reset();
	bInitialised = false;

	UE_LOG(LogTemp, Log, TEXT("[CombatHUDRoot] Torn down (stub)"));
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
