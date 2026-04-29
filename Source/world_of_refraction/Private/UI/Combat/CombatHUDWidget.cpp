// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/CombatHUDWidget.h"
#include "CombatOrchestrator.h"
#include "TurnManager.h"
#include "ActionStructs.h"
#include "Components/PanelWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"

UCombatHUDWidget::UCombatHUDWidget(const FObjectInitializer &ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCombatHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Spawn slot widgets up front so they're ready before first turn
	InitialiseTurnSlots();
}

void UCombatHUDWidget::NativeDestruct()
{
	UnbindFromOrchestrator();
	ClearTurnSlots();
	Super::NativeDestruct();
}

void UCombatHUDWidget::BeginDestroy()
{
	// Defensive: NativeDestruct doesn't always fire on PIE end. BeginDestroy
	// is the last guaranteed callback before GC, so unbind here too. Safe to
	// call multiple times because UnbindFromOrchestrator resets the weak ptrs.
	UnbindFromOrchestrator();
	Super::BeginDestroy();
}

void UCombatHUDWidget::BindToOrchestrator(ACombatOrchestrator *Orchestrator)
{
	if (!Orchestrator)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatHUD] BindToOrchestrator called with null"));
		return;
	}

	// Defensive unbind first
	UnbindFromOrchestrator();

	CurrentOrchestrator = Orchestrator;

	// Cache turn manager
	if (UGameInstance *GI = GetGameInstance())
	{
		CachedTurnManager = GI->GetSubsystem<UTurnManager>();
	}

	if (!CachedTurnManager.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatHUD] No TurnManager subsystem available"));
		return;
	}

	// Bind events
	CachedTurnManager->OnTurnStarted.AddDynamic(this, &UCombatHUDWidget::HandleTurnStarted);
	Orchestrator->OnActionExecuted.AddDynamic(this, &UCombatHUDWidget::HandleActionExecuted);

	UE_LOG(LogTemp, Log, TEXT("[CombatHUD] Bound to orchestrator"));
}

void UCombatHUDWidget::UnbindFromOrchestrator()
{
	// Use Get() (not IsValid()) — if the underlying object is being torn down,
	// IsValid may return false but the delegate binding may still hold a ref.
	// RemoveDynamic is safe even on a stale-but-not-yet-GC'd pointer.
	if (UTurnManager *TM = CachedTurnManager.Get())
	{
		TM->OnTurnStarted.RemoveDynamic(this, &UCombatHUDWidget::HandleTurnStarted);
	}

	if (ACombatOrchestrator *Orch = CurrentOrchestrator.Get())
	{
		Orch->OnActionExecuted.RemoveDynamic(this, &UCombatHUDWidget::HandleActionExecuted);
	}

	CurrentOrchestrator.Reset();
	CachedTurnManager.Reset();
}

void UCombatHUDWidget::HandleTurnStarted(AActor *Actor, int32 TurnNumber)
{
	CurrentTurnNumber = TurnNumber;
	UpdateTurnSlots(Actor);
}

void UCombatHUDWidget::HandleActionExecuted(AActor *Actor, const FActionResult &Result)
{
	// Refresh on action complete in case slot data depends on post-action state.
	// Currently no-op visually because TurnStarted will fire next.
	// Reserved for future: animations, "just acted" highlight clear, etc.
}

void UCombatHUDWidget::InitialiseTurnSlots()
{
	if (!TurnOrderStrip)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatHUD] No TurnOrderStrip widget bound — check WBP widget naming"));
		return;
	}

	if (!TurnSlotWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatHUD] No TurnSlotWidgetClass set — set it in WBP_CombatHUD defaults"));
		return;
	}

	// Clear existing if any (safe to call repeatedly)
	ClearTurnSlots();

	const int32 NumSlots = FMath::Max(1, TurnPreviewRange);

	for (int32 i = 0; i < NumSlots; ++i)
	{
		UUserWidget *SlotWidget = CreateWidget<UUserWidget>(this, TurnSlotWidgetClass);
		if (!SlotWidget)
			continue;

		TurnOrderStrip->AddChild(SlotWidget);
		SpawnedSlots.Add(SlotWidget);
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatHUD] Initialised %d turn slots"), SpawnedSlots.Num());
}

void UCombatHUDWidget::ClearTurnSlots()
{
	for (UUserWidget *SlotWidget : SpawnedSlots)
	{
		if (SlotWidget)
		{
			SlotWidget->RemoveFromParent();
		}
	}
	SpawnedSlots.Reset();

	if (TurnOrderStrip)
	{
		TurnOrderStrip->ClearChildren();
	}
}

void UCombatHUDWidget::UpdateTurnSlots(AActor *CurrentActor)
{
	if (SpawnedSlots.Num() == 0)
	{
		// Lazy init in case construct ran before BindWidget resolved
		InitialiseTurnSlots();
		if (SpawnedSlots.Num() == 0)
			return;
	}

	if (!CurrentOrchestrator.IsValid() || !CachedTurnManager.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatHUD] UpdateTurnSlots called but not bound"));
		return;
	}

	// Build the list: slot 0 = current actor, slots 1..N = preview of upcoming.
	// PreviewTurnOrder simulates *future* turns AFTER the current one already taken,
	// so it does NOT include the current actor — we prepend it ourselves.
	TArray<AActor *> DisplayList;
	DisplayList.Add(CurrentActor);

	const int32 NumPreview = FMath::Max(0, SpawnedSlots.Num() - 1);
	if (NumPreview > 0)
	{
		TArray<AActor *> Preview = CachedTurnManager->PreviewTurnOrder(NumPreview);
		DisplayList.Append(Preview);
	}

	// Apply to slots
	for (int32 i = 0; i < SpawnedSlots.Num(); ++i)
	{
		UUserWidget *SlotWidget = SpawnedSlots[i];
		if (!SlotWidget)
			continue;

		AActor *SlotActor = (i < DisplayList.Num()) ? DisplayList[i] : nullptr;

		if (!SlotActor)
		{
			SlotWidget->SetVisibility(ESlateVisibility::Hidden);
			continue;
		}

		SlotWidget->SetVisibility(ESlateVisibility::Visible);

		const int32 TeamIndex = CachedTurnManager->GetActorTeam(SlotActor);
		const bool bIsActive = (i == 0); // Slot 0 is the active turn
		const int32 SlotTurnNumber = CurrentTurnNumber + i;

		InitialiseSlot(SlotWidget, SlotActor, bIsActive, SlotTurnNumber, TeamIndex);
	}
}

void UCombatHUDWidget::InitialiseSlot(UUserWidget *SlotWidget, AActor *Actor, bool bActive, int32 TurnNumber, int32 InTeamIndex)
{
	if (!SlotWidget)
		return;

	// Call the BP function "Initialise Slot" on the slot widget.
	// Signature must match WBP_TurnOrderSlot::InitialiseSlot(Actor, bActive, TurnNumber, InTeamIndex).
	struct FInitialiseSlotParams
	{
		AActor *Actor;
		bool bActive;
		int32 TurnNumber;
		int32 InTeamIndex;
	};

	FInitialiseSlotParams Params;
	Params.Actor = Actor;
	Params.bActive = bActive;
	Params.TurnNumber = TurnNumber;
	Params.InTeamIndex = InTeamIndex;

	UFunction *Func = SlotWidget->FindFunction(TEXT("InitialiseSlot"));
	if (!Func)
	{
		// Fallback: try the underscored version some BP devs use
		Func = SlotWidget->FindFunction(TEXT("Initialise_Slot"));
	}

	if (Func)
	{
		SlotWidget->ProcessEvent(Func, &Params);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatHUD] WBP_TurnOrderSlot has no 'InitialiseSlot' function"));
	}
}