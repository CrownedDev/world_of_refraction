// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/TurnOrderStripWidget.h"
#include "UI/Combat/TurnOrderSlotWidget.h"
#include "TurnManager.h"
#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"

void UTurnOrderStripWidget::InitialiseForCombat()
{
	if (bInitialised)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TurnOrderStripWidget] InitialiseForCombat called twice; ignoring"));
		return;
	}

	// TODO Phase 1: cache TurnManager subsystem
	// TODO Phase 1: SpawnSlots() — pre-create PreviewCount + 1 slot widgets
	// TODO Phase 1: bind TurnManager::OnTurnStarted -> HandleTurnStarted
	// TODO Phase 1: initial RefreshSlots() if combat already in progress

	bInitialised = true;
	UE_LOG(LogTemp, Log, TEXT("[TurnOrderStripWidget] Initialised (stub)"));
}

void UTurnOrderStripWidget::TeardownStrip()
{
	if (!bInitialised)
	{
		return;
	}

	// TODO Phase 1: unbind TurnManager delegate using Get() not IsValid()
	// TODO Phase 1: clear Slots array, remove from SlotContainer

	Slots.Reset();
	CachedTurnManager.Reset();
	bInitialised = false;
}

void UTurnOrderStripWidget::NativeDestruct()
{
	if (bInitialised)
	{
		TeardownStrip();
	}
	Super::NativeDestruct();
}

void UTurnOrderStripWidget::BeginDestroy()
{
	if (bInitialised)
	{
		TeardownStrip();
	}
	Super::BeginDestroy();
}

void UTurnOrderStripWidget::HandleTurnStarted(AActor* Actor, int32 TurnNumber)
{
	// TODO Phase 1: RefreshSlots() — pull PreviewTurnOrder from TurnManager
}

void UTurnOrderStripWidget::SpawnSlots()
{
	// TODO Phase 1: create PreviewCount + 1 instances of SlotWidgetClass
	// TODO Phase 1: add each to SlotContainer
}

void UTurnOrderStripWidget::RefreshSlots()
{
	// TODO Phase 1: get current actor + PreviewTurnOrder(PreviewCount)
	// TODO Phase 1: call InitialiseSlot on each, mark slot 0 as active
}
