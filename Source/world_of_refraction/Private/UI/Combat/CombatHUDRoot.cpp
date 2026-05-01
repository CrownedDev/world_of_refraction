// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/CombatHUDRoot.h"
#include "UI/Combat/CharacterPanelWidget.h"
#include "UI/Combat/TurnOrderStripWidget.h"
#include "UI/Combat/DefensePromptWidget.h"
#include "CombatActionMenuBase.h"
#include "CombatOrchestrator.h"
#include "Components/PanelWidget.h"

void UCombatHUDRoot::InitialiseForCombat(ACombatOrchestrator *Orchestrator,
										 const TArray<AActor *> &Team0,
										 const TArray<AActor *> &Team1)
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

	// 1. Spawn character panels for each team
	if (!CharacterPanelClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatHUDRoot] CharacterPanelClass not set — skipping panel spawn"));
	}
	else
	{
		auto SpawnPanelsForTeam = [this](UPanelWidget *Container, const TArray<AActor *> &Members, int32 TeamIndex)
		{
			if (!Container)
			{
				UE_LOG(LogTemp, Warning, TEXT("[CombatHUDRoot] Team%d container not bound — check WBP layout"), TeamIndex);
				return;
			}

			Container->ClearChildren();

			for (AActor *Member : Members)
			{
				if (!Member)
				{
					continue;
				}

				UCharacterPanelWidget *Panel = CreateWidget<UCharacterPanelWidget>(this, CharacterPanelClass);
				if (!Panel)
				{
					UE_LOG(LogTemp, Warning, TEXT("[CombatHUDRoot] CreateWidget failed for team %d member"), TeamIndex);
					continue;
				}

				Container->AddChild(Panel);
				Panel->InitialiseForActor(Member);
				SpawnedPanels.Add(Panel);
			}
		};

		SpawnPanelsForTeam(PlayerTeamContainer, Team0, 0);
		SpawnPanelsForTeam(EnemyTeamContainer, Team1, 1);
	}

	// 2. Initialise turn order strip (self-contained — owns its own slot lifecycle + TurnManager binding)
	if (TurnOrderStrip)
	{
		TurnOrderStrip->InitialiseForCombat();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[CombatHUDRoot] No TurnOrderStrip bound — skipping (optional)"));
	}

	// 3. Initialise defense prompt (self-contained — binds to DefenseSystem)
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

	UE_LOG(LogTemp, Log, TEXT("[CombatHUDRoot] Initialised — Team0=%d Team1=%d (panels spawned: %d)"),
		   Team0.Num(), Team1.Num(), SpawnedPanels.Num());
}

void UCombatHUDRoot::TeardownForCombatEnd()
{
	if (!bInitialised)
	{
		return;
	}

	// 1. Tear down character panels
	for (UCharacterPanelWidget *Panel : SpawnedPanels)
	{
		if (Panel)
		{
			Panel->TeardownPanel();
			Panel->RemoveFromParent();
		}
	}
	SpawnedPanels.Reset();

	if (PlayerTeamContainer)
	{
		PlayerTeamContainer->ClearChildren();
	}
	if (EnemyTeamContainer)
	{
		EnemyTeamContainer->ClearChildren();
	}

	// 2. Tear down turn order strip
	if (TurnOrderStrip)
	{
		TurnOrderStrip->TeardownStrip();
	}

	// 3. Tear down defense prompt
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
