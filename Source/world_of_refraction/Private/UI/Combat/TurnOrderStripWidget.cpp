// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/TurnOrderStripWidget.h"
#include "Player/TurnStateComponent.h"
#include "UI/Combat/TurnOrderSlotWidget.h"
#include "Combat/TurnManager.h"
#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"

void UTurnOrderStripWidget::InitialiseForCombat()
{
	if (bInitialised)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TurnOrderStrip] InitialiseForCombat called twice; ignoring"));
		return;
	}

	// Prevent TransBuffer from holding stale refs across PIE end.
	// Same pattern as UCombatActionMenuBase::StripTransactionalFlags.
	ClearFlags(RF_Transactional);

	if (!SlotContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TurnOrderStrip] SlotContainer not bound — check WBP layout"));
		return;
	}

	if (!SlotWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TurnOrderStrip] SlotWidgetClass not set — assign WBP_TurnOrderSlot_New in WBP defaults"));
		return;
	}

	if (UGameInstance *GI = GetGameInstance())
	{
		CachedTurnManager = GI->GetSubsystem<UTurnManager>();
	}

	if (!CachedTurnManager.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TurnOrderStrip] TurnManager subsystem not available"));
		return;
	}

	// Pre-spawn slots once
	SpawnSlots();

	// Bind to turn changes: the GameState's turn state when there is one (B-228, works on clients), else the
	// server-only TurnManager (standalone, headless).
	CachedTurnState = UTurnStateComponent::Find(this);
	if (UTurnStateComponent *TurnState = CachedTurnState.Get())
	{
		TurnState->OnTurnStateChanged.AddDynamic(this, &UTurnOrderStripWidget::HandleTurnStateChanged);
	}
	else
	{
		CachedTurnManager->OnTurnStarted.AddDynamic(this, &UTurnOrderStripWidget::HandleTurnStarted);
	}

	// Seed the turn number before the first RefreshSlots: from the mirror when there is one (a HUD built mid-fight or
	// a late join reads the real number), else turn 1. The turn handlers overwrite it on every turn after.
	const UTurnStateComponent *SeedState = CachedTurnState.Get();
	CurrentTurnNumber = SeedState ? FMath::Max(1, SeedState->GetTurnNumber()) : 1;

	// Initial population — handles "spawned mid-combat" case
	RefreshSlots();

	bInitialised = true;

	UE_LOG(LogTemp, Log, TEXT("[TurnOrderStrip] Initialised with %d slots"), Slots.Num());
}

void UTurnOrderStripWidget::TeardownStrip()
{
	if (!bInitialised)
	{
		return;
	}

	if (UTurnManager *TurnMgr = CachedTurnManager.Get())
	{
		TurnMgr->OnTurnStarted.RemoveDynamic(this, &UTurnOrderStripWidget::HandleTurnStarted);
	}
	if (UTurnStateComponent *TurnState = CachedTurnState.Get())
	{
		TurnState->OnTurnStateChanged.RemoveDynamic(this, &UTurnOrderStripWidget::HandleTurnStateChanged);
	}
	CachedTurnState.Reset();

	if (SlotContainer)
	{
		SlotContainer->ClearChildren();
	}
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

void UTurnOrderStripWidget::HandleTurnStarted(AActor *Actor, int32 TurnNumber)
{
	CurrentTurnNumber = TurnNumber;
	RefreshSlots();
}

void UTurnOrderStripWidget::HandleTurnStateChanged()
{
	if (const UTurnStateComponent *TurnState = CachedTurnState.Get())
	{
		CurrentTurnNumber = TurnState->GetTurnNumber();
	}
	RefreshSlots();
}

void UTurnOrderStripWidget::SpawnSlots()
{
	const int32 TotalSlots = PreviewCount + 1; // +1 for current actor

	for (int32 i = 0; i < TotalSlots; ++i)
	{
		UTurnOrderSlotWidget *SlotWidget = CreateWidget<UTurnOrderSlotWidget>(this, SlotWidgetClass);
		if (SlotWidget)
		{
			SlotContainer->AddChild(SlotWidget);
			Slots.Add(SlotWidget);
		}
	}
}
void UTurnOrderStripWidget::RefreshSlots()
{
	if (Slots.Num() == 0)
	{
		return;
	}
	// B-228: the replicated turn state, the same on every player's screen.
	if (const UTurnStateComponent *TurnState = CachedTurnState.Get())
	{
		if (Slots[0])
		{
			Slots[0]->InitialiseSlot(TurnState->GetCurrentActor(), CurrentTurnNumber, /*bIsActive=*/true,
									 /*bIsBonus=*/TurnState->GetCurrentTurnIsBonus());
		}
		const TArray<FTurnStateSlot> &Upcoming = TurnState->GetUpcoming();
		for (int32 i = 0; i < PreviewCount; ++i)
		{
			const int32 SlotIndex = i + 1;
			if (!Slots.IsValidIndex(SlotIndex) || !Slots[SlotIndex])
			{
				continue;
			}
			const bool bIsBonus = Upcoming.IsValidIndex(i) && Upcoming[i].bIsBonusTurn;
			AActor *PreviewActor = Upcoming.IsValidIndex(i) ? Upcoming[i].Actor.Get() : nullptr;
			Slots[SlotIndex]->InitialiseSlot(PreviewActor, CurrentTurnNumber + i + 1, /*bIsActive=*/false, bIsBonus);
		}
		return;
	}

	UTurnManager *TurnMgr = CachedTurnManager.Get();
	if (!TurnMgr)
	{
		return;
	}

	AActor *CurrentActor = TurnMgr->GetCurrentActor();
	TArray<FPreviewTurnEntry> Upcoming = TurnMgr->PreviewTurnOrder(PreviewCount);

	// Slot 0 = current actor. The current turn can itself BE a bonus turn (Emerald immediate /
	// self-target, where the bonus is taken now rather than appearing as an upcoming slot) — so
	// read the TurnManager's per-turn transient instead of hardcoding false.
	if (Slots[0])
	{
		Slots[0]->InitialiseSlot(CurrentActor, CurrentTurnNumber, /*bIsActive=*/true, /*bIsBonus=*/TurnMgr->GetCurrentTurnIsBonus());
	}

	// Slots 1..N = upcoming
	for (int32 i = 0; i < PreviewCount; ++i)
	{
		const int32 SlotIndex = i + 1;
		if (!Slots.IsValidIndex(SlotIndex) || !Slots[SlotIndex])
		{
			continue;
		}

		const bool bIsBonus = Upcoming.IsValidIndex(i) && Upcoming[i].bIsBonusTurn;
		AActor *PreviewActor = Upcoming.IsValidIndex(i) ? Upcoming[i].Actor : nullptr;
		Slots[SlotIndex]->InitialiseSlot(PreviewActor, CurrentTurnNumber + i + 1, /*bIsActive=*/false, bIsBonus);
	}
}