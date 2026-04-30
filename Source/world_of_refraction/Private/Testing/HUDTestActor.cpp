// Copyright Epic Games, Inc. All Rights Reserved.

#include "Testing/HUDTestActor.h"
#include "UI/Combat/CharacterPanelWidget.h"
#include "UI/Combat/TurnOrderStripWidget.h"
#include "Blueprint/UserWidget.h"
#include "CharacterDataComponent.h"
#include "StatusEffectManager.h"
#include "ESpellElement.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

AHUDTestActor::AHUDTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AHUDTestActor::SpawnCharacterPanel()
{
	UWorld *World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[HUDTest] SpawnCharacterPanel requires PIE — start Play mode first."));
		return;
	}

	if (!CharacterPanelClass)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[HUDTest] CharacterPanelClass not set — assign WBP_CharacterPanel_New in Details panel."));
		return;
	}

	AActor *ResolvedTarget = TargetActor.LoadSynchronous();
	if (!ResolvedTarget)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[HUDTest] TargetActor not set or could not be resolved."));
		return;
	}

	APlayerController *PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] No player controller available."));
		return;
	}

	UCharacterPanelWidget *Panel = CreateWidget<UCharacterPanelWidget>(PC, CharacterPanelClass);
	if (!Panel)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] CreateWidget returned null."));
		return;
	}

	Panel->AddToViewport(100); // Sit above existing HUD for testing
	Panel->InitialiseForActor(ResolvedTarget);
	SpawnedPanels.Add(Panel);

	UE_LOG(LogTemp, Log, TEXT("[HUDTest] Spawned character panel for %s"),
		   *ResolvedTarget->GetName());
}

void AHUDTestActor::ClearAllPanels()
{
	int32 Cleared = 0;
	for (UCharacterPanelWidget *Panel : SpawnedPanels)
	{
		if (Panel)
		{
			Panel->TeardownPanel();
			Panel->RemoveFromParent();
			Cleared++;
		}
	}
	for (UTurnOrderStripWidget *Strip : SpawnedStrips)
	{
		if (Strip)
		{
			Strip->TeardownStrip();
			Strip->RemoveFromParent();
			Cleared++;
		}
	}
	SpawnedPanels.Reset();
	SpawnedStrips.Reset();
	UE_LOG(LogTemp, Log, TEXT("[HUDTest] Cleared %d widget(s)"), Cleared);
}

void AHUDTestActor::SpawnTurnOrderStrip()
{
	UWorld *World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] SpawnTurnOrderStrip requires PIE."));
		return;
	}

	if (!TurnOrderStripClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] TurnOrderStripClass not set — assign WBP_TurnOrderStrip_New in Details panel."));
		return;
	}

	APlayerController *PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	UTurnOrderStripWidget *Strip = CreateWidget<UTurnOrderStripWidget>(PC, TurnOrderStripClass);
	if (!Strip)
	{
		return;
	}

	Strip->AddToViewport(100);
	Strip->InitialiseForCombat();
	SpawnedStrips.Add(Strip);

	UE_LOG(LogTemp, Log, TEXT("[HUDTest] Spawned turn order strip"));
}

void AHUDTestActor::ApplyTestDamage()
{
	UWorld *World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] ApplyTestDamage requires PIE."));
		return;
	}

	AActor *ResolvedTarget = TargetActor.LoadSynchronous();
	if (!ResolvedTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] TargetActor not set."));
		return;
	}

	UCharacterDataComponent *CharComp = ResolvedTarget->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] %s has no CharacterDataComponent."), *ResolvedTarget->GetName());
		return;
	}

	CharComp->ServerTakeDamage(TestDamageAmount);
	UE_LOG(LogTemp, Log, TEXT("[HUDTest] Applied %d damage to %s"), TestDamageAmount, *ResolvedTarget->GetName());
}

void AHUDTestActor::SpendTestEnergy()
{
	UWorld *World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] SpendTestEnergy requires PIE."));
		return;
	}

	AActor *ResolvedTarget = TargetActor.LoadSynchronous();
	if (!ResolvedTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] TargetActor not set."));
		return;
	}

	UCharacterDataComponent *CharComp = ResolvedTarget->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] %s has no CharacterDataComponent."), *ResolvedTarget->GetName());
		return;
	}

	CharComp->ServerSpendEnergy(TestEnergyCost);
	UE_LOG(LogTemp, Log, TEXT("[HUDTest] Spent %d energy on %s"), TestEnergyCost, *ResolvedTarget->GetName());
}

void AHUDTestActor::AddTestStatusBuildup()
{
	UWorld *World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] AddTestStatusBuildup requires PIE."));
		return;
	}

	AActor *ResolvedTarget = TargetActor.LoadSynchronous();
	if (!ResolvedTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] TargetActor not set."));
		return;
	}

	UStatusEffectManager *StatusMgr = nullptr;
	if (UGameInstance *GI = World->GetGameInstance())
	{
		StatusMgr = GI->GetSubsystem<UStatusEffectManager>();
	}

	if (!StatusMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HUDTest] StatusEffectManager subsystem not available."));
		return;
	}

	StatusMgr->AddStatusBuildup(ResolvedTarget, ResolvedTarget, TestStatusBuildup, EStatusType::DOT, ESpellElement::Generic);
	UE_LOG(LogTemp, Log, TEXT("[HUDTest] Added %.1f status buildup to %s"), TestStatusBuildup, *ResolvedTarget->GetName());
}