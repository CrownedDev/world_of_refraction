// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/DefensePromptWidget.h"
#include "DefenseSystem.h"
#include "Engine/GameInstance.h"

void UDefensePromptWidget::InitialiseForCombat()
{
	if (bBound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefensePromptWidget] InitialiseForCombat called twice; ignoring"));
		return;
	}

	// TODO Phase 1: cache UDefenseSystem subsystem
	// TODO Phase 1: bind OnDefenseWindowOpened -> HandleDefenseWindowOpened
	// TODO Phase 1: bind OnDefenseWindowClosed -> HandleDefenseWindowClosed

	bBound = true;
	UE_LOG(LogTemp, Log, TEXT("[DefensePromptWidget] Initialised (stub)"));
}

void UDefensePromptWidget::TeardownPrompt()
{
	if (!bBound)
	{
		return;
	}

	// TODO Phase 1: unbind delegates using Get() not IsValid()

	CachedDefenseSystem.Reset();
	bBound = false;
}

void UDefensePromptWidget::NativeDestruct()
{
	if (bBound)
	{
		TeardownPrompt();
	}
	Super::NativeDestruct();
}

void UDefensePromptWidget::BeginDestroy()
{
	if (bBound)
	{
		TeardownPrompt();
	}
	Super::BeginDestroy();
}

void UDefensePromptWidget::HandleDefenseWindowOpened(AActor* Defender, float AttackSize, float WindowDuration)
{
	// TODO Phase 1: filter by local player controlled actor
	// TODO Phase 1: ShowPrompt(Defender, WindowDuration, AttackSize)
}

void UDefensePromptWidget::HandleDefenseWindowClosed(AActor* Defender, const FDefenseResult& Result)
{
	// TODO Phase 1: HidePrompt(Result)
}
