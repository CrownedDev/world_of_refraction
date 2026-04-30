// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/CharacterPanelWidget.h"
#include "CharacterDataComponent.h"
#include "StatusEffectManager.h"
#include "StatusEffect.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"

void UCharacterPanelWidget::InitialiseForActor(AActor* InActor)
{
	if (bBound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPanelWidget] InitialiseForActor called twice; ignoring"));
		return;
	}

	if (!InActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPanelWidget] InitialiseForActor called with null actor"));
		return;
	}

	BoundActor = InActor;

	// TODO Phase 1: cache CharacterDataComponent, StatusEffectManager
	// TODO Phase 1: bind delegates (HP/EP/Status/effects/death)
	// TODO Phase 1: set static text fields (name, class, element, world stats)
	// TODO Phase 1: initial HP/EP/Status snapshot

	bBound = true;
	UE_LOG(LogTemp, Log, TEXT("[CharacterPanelWidget] Initialised for %s (stub)"), *InActor->GetName());
}

void UCharacterPanelWidget::TeardownPanel()
{
	if (!bBound)
	{
		return;
	}

	// TODO Phase 1: unbind all delegates using Get() not IsValid()

	BoundActor.Reset();
	BoundCharData.Reset();
	BoundStatusManager.Reset();
	bBound = false;
}

void UCharacterPanelWidget::NativeDestruct()
{
	if (bBound)
	{
		TeardownPanel();
	}
	Super::NativeDestruct();
}

void UCharacterPanelWidget::BeginDestroy()
{
	if (bBound)
	{
		TeardownPanel();
	}
	Super::BeginDestroy();
}

// ========================================
// Delegate handler stubs — Phase 1 implements
// ========================================

void UCharacterPanelWidget::HandleHPChanged(int32 CurrentHP, int32 MaxHP) {}
void UCharacterPanelWidget::HandleEPChanged(int32 CurrentEP, int32 MaxEP) {}
void UCharacterPanelWidget::HandleStatusBuildupChanged(AActor* Target, float Current, float Max) {}
void UCharacterPanelWidget::HandleEffectApplied(AActor* Target, const FStatusEffect& Effect) {}
void UCharacterPanelWidget::HandleEffectRemoved(AActor* Target, const FStatusEffect& Effect) {}
void UCharacterPanelWidget::HandleEffectDurationChanged(AActor* Target, const FStatusEffect& Effect, int32 RemainingTurns) {}
void UCharacterPanelWidget::HandleDied(AActor* DeadActor) {}
