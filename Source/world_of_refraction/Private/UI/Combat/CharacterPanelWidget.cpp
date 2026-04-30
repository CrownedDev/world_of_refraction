// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/CharacterPanelWidget.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "StatusEffectManager.h"
#include "StatusEffect.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"

void UCharacterPanelWidget::InitialiseForActor(AActor *InActor)
{
	if (bBound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPanel] InitialiseForActor called twice; ignoring"));
		return;
	}

	if (!InActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPanel] InitialiseForActor called with null actor"));
		return;
	}

	UCharacterDataComponent *CharComp = InActor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPanel] %s has no CharacterDataComponent"),
			   *InActor->GetName());
		return;
	}

	UStatusEffectManager *StatusMgr = nullptr;
	if (UGameInstance *GI = GetGameInstance())
	{
		StatusMgr = GI->GetSubsystem<UStatusEffectManager>();
	}

	BoundActor = InActor;
	BoundCharData = CharComp;
	BoundStatusManager = StatusMgr;

	// Bind component delegates
	CharComp->OnHPChanged.AddDynamic(this, &UCharacterPanelWidget::HandleHPChanged);
	CharComp->OnEPChanged.AddDynamic(this, &UCharacterPanelWidget::HandleEPChanged);
	CharComp->OnDied.AddDynamic(this, &UCharacterPanelWidget::HandleDied);

	// Bind global subsystem delegates (filtered per-actor inside the handlers)
	if (StatusMgr)
	{
		StatusMgr->OnStatusBuildupChanged.AddDynamic(this, &UCharacterPanelWidget::HandleStatusBuildupChanged);
		StatusMgr->OnEffectApplied.AddDynamic(this, &UCharacterPanelWidget::HandleEffectApplied);
		StatusMgr->OnEffectRemoved.AddDynamic(this, &UCharacterPanelWidget::HandleEffectRemoved);
		StatusMgr->OnEffectDurationChanged.AddDynamic(this, &UCharacterPanelWidget::HandleEffectDurationChanged);
	}

	bBound = true;

	// Static text
	ApplyStaticText();

	// Initial snapshot
	HandleHPChanged(CharComp->CurrentHP, CharComp->MaxHP);
	HandleEPChanged(CharComp->CurrentEP, CharComp->MaxEP);

	// Status starts at 0 — broadcast doesn't fire until first hit, so seed it
	SetBarSafe(StatusBar, 0.0f);
	SetTextSafe(StatusText, TEXT("0/100"));

	// Initial buff/debuff list (usually empty at combat start)
	RefreshBuffDebuffList();

	UE_LOG(LogTemp, Log, TEXT("[CharacterPanel] Initialised for %s"), *InActor->GetName());
}

void UCharacterPanelWidget::TeardownPanel()
{
	if (!bBound)
	{
		return;
	}

	// Unbind component delegates — Get() pattern for safety
	if (UCharacterDataComponent *CharComp = BoundCharData.Get())
	{
		CharComp->OnHPChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleHPChanged);
		CharComp->OnEPChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleEPChanged);
		CharComp->OnDied.RemoveDynamic(this, &UCharacterPanelWidget::HandleDied);
	}

	if (UStatusEffectManager *StatusMgr = BoundStatusManager.Get())
	{
		StatusMgr->OnStatusBuildupChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleStatusBuildupChanged);
		StatusMgr->OnEffectApplied.RemoveDynamic(this, &UCharacterPanelWidget::HandleEffectApplied);
		StatusMgr->OnEffectRemoved.RemoveDynamic(this, &UCharacterPanelWidget::HandleEffectRemoved);
		StatusMgr->OnEffectDurationChanged.RemoveDynamic(this, &UCharacterPanelWidget::HandleEffectDurationChanged);
	}

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
// Static text — BP can override
// ========================================

void UCharacterPanelWidget::ApplyStaticText_Implementation()
{
	UCharacterDataComponent *CharComp = BoundCharData.Get();
	if (!CharComp || !CharComp->CharacterData)
	{
		return;
	}

	UCharacterData *Data = CharComp->CharacterData;
	SetTextSafe(NameText, Data->CharacterName);
	// ClassElementText, WorldStatsText left to BP override (enum formatting)
}

// ========================================
// Delegate handlers
// ========================================

void UCharacterPanelWidget::HandleHPChanged(int32 CurrentHP, int32 MaxHP)
{
	const float Percent = (MaxHP > 0) ? (static_cast<float>(CurrentHP) / static_cast<float>(MaxHP)) : 0.0f;
	SetBarSafe(HPBar, Percent);
	SetTextSafe(HPText, FString::Printf(TEXT("%d/%d"), CurrentHP, MaxHP));
}

void UCharacterPanelWidget::HandleEPChanged(int32 CurrentEP, int32 MaxEP)
{
	const float Percent = (MaxEP > 0) ? (static_cast<float>(CurrentEP) / static_cast<float>(MaxEP)) : 0.0f;
	SetBarSafe(EPBar, Percent);
	SetTextSafe(EPText, FString::Printf(TEXT("%d/%d"), CurrentEP, MaxEP));
}

void UCharacterPanelWidget::HandleStatusBuildupChanged(AActor *Target, float Current, float Max)
{
	if (Target != BoundActor.Get())
	{
		return;
	}
	const float Percent = (Max > 0.0f) ? (Current / Max) : 0.0f;
	SetBarSafe(StatusBar, Percent);
	SetTextSafe(StatusText, FString::Printf(TEXT("%d/%d"), FMath::TruncToInt(Current), FMath::TruncToInt(Max)));
}

void UCharacterPanelWidget::HandleEffectApplied(AActor *Target, const FStatusEffect &Effect)
{
	if (Target != BoundActor.Get())
	{
		return;
	}
	RefreshBuffDebuffList();
}

void UCharacterPanelWidget::HandleEffectRemoved(AActor *Target, const FStatusEffect &Effect)
{
	if (Target != BoundActor.Get())
	{
		return;
	}
	RefreshBuffDebuffList();
}

void UCharacterPanelWidget::HandleEffectDurationChanged(AActor *Target, const FStatusEffect &Effect, int32 RemainingTurns)
{
	if (Target != BoundActor.Get())
	{
		return;
	}
	RefreshBuffDebuffList();
}

void UCharacterPanelWidget::HandleDied(AActor *DeadActor)
{
	if (DeadActor != BoundActor.Get())
	{
		return;
	}
	// Visual response is BP's responsibility; we just teardown bindings.
	TeardownPanel();
}

// ========================================
// Internal helpers
// ========================================

void UCharacterPanelWidget::RefreshBuffDebuffList()
{
	UStatusEffectManager *StatusMgr = BoundStatusManager.Get();
	AActor *Actor = BoundActor.Get();

	if (!StatusMgr || !Actor)
	{
		RebuildBuffDebuffList(TArray<FStatusEffect>());
		return;
	}

	RebuildBuffDebuffList(StatusMgr->GetActiveEffects(Actor));
}

void UCharacterPanelWidget::SetBarSafe(UProgressBar *Bar, float Percent)
{
	if (Bar)
	{
		Bar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
	}
}

void UCharacterPanelWidget::SetTextSafe(UTextBlock *Text, const FString &Value)
{
	if (Text)
	{
		Text->SetText(FText::FromString(Value));
	}
}