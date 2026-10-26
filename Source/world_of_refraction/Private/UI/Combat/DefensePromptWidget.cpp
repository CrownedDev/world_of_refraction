// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Combat/DefensePromptWidget.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

namespace DefensePromptText
{
	constexpr const TCHAR *Cue = TEXT("Get ready...");
	constexpr const TCHAR *Window = TEXT("DEFEND! Guard / Dodge");
	constexpr const TCHAR *Missed = TEXT("Missed");
	constexpr int32 NativeFontSize = 32; // TODO: style pass
}

TSharedRef<SWidget> UDefensePromptWidget::RebuildWidget()
{
	// Native fallback layout — only when no WBP skin supplied a tree.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UTextBlock *Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptText"));
		Text->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = DefensePromptText::NativeFontSize;
		Text->SetFont(Font);
		WidgetTree->RootWidget = Text;
		PromptText = Text;
	}
	return Super::RebuildWidget();
}

void UDefensePromptWidget::InitialiseForCombat()
{
	if (bBound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefensePromptWidget] InitialiseForCombat called twice; ignoring"));
		return;
	}

	ClearFlags(RF_Transactional);

	UDefenseSystem *DefenseSystem = nullptr;
	if (UGameInstance *GI = GetGameInstance())
	{
		DefenseSystem = GI->GetSubsystem<UDefenseSystem>();
	}
	if (!DefenseSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefensePromptWidget] DefenseSystem subsystem not available"));
		return;
	}

	DefenseSystem->OnDefenseCueTriggered.AddDynamic(this, &UDefensePromptWidget::HandleDefenseCueTriggered);
	DefenseSystem->OnDefenseWindowOpened.AddDynamic(this, &UDefensePromptWidget::HandleDefenseWindowOpened);
	DefenseSystem->OnDefenseWindowClosed.AddDynamic(this, &UDefensePromptWidget::HandleDefenseWindowClosed);
	CachedDefenseSystem = DefenseSystem;

	HideNow();
	bBound = true;
	UE_LOG(LogTemp, Log, TEXT("[DefensePromptWidget] Initialised - bound to DefenseSystem"));
}

void UDefensePromptWidget::TeardownPrompt()
{
	if (!bBound)
	{
		return;
	}

	if (UDefenseSystem *DefenseSystem = CachedDefenseSystem.Get())
	{
		DefenseSystem->OnDefenseCueTriggered.RemoveDynamic(this, &UDefensePromptWidget::HandleDefenseCueTriggered);
		DefenseSystem->OnDefenseWindowOpened.RemoveDynamic(this, &UDefensePromptWidget::HandleDefenseWindowOpened);
		DefenseSystem->OnDefenseWindowClosed.RemoveDynamic(this, &UDefensePromptWidget::HandleDefenseWindowClosed);
	}

	ClearHideTimer();
	CachedDefenseSystem.Reset();
	bBound = false;
}

FString UDefensePromptWidget::GetPromptStateString() const
{
	return FString::Printf(TEXT("[DefensePrompt] bound=%s visible=%s text=\"%s\""),
						   bBound ? TEXT("yes") : TEXT("no"),
						   GetVisibility() == ESlateVisibility::Collapsed ? TEXT("no") : TEXT("yes"),
						   PromptText ? *PromptText->GetText().ToString() : TEXT("<no PromptText>"));
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
	// GC path: don't reach for the world's timer manager — the world may be going too.
	HideTimerHandle.Invalidate();
	if (bBound)
	{
		TeardownPrompt();
	}
	Super::BeginDestroy();
}

void UDefensePromptWidget::HandleDefenseCueTriggered(AActor *Defender, float TimeUntilWindow)
{
	if (IsLocallyControlledDefender(Defender))
	{
		ShowCue(Defender, TimeUntilWindow);
	}
}

void UDefensePromptWidget::HandleDefenseWindowOpened(AActor *Defender, float AttackSize, float WindowDuration)
{
	if (IsLocallyControlledDefender(Defender))
	{
		ShowPrompt(Defender, WindowDuration, AttackSize);
	}
}

void UDefensePromptWidget::HandleDefenseWindowClosed(AActor *Defender, const FDefenseResult &Result)
{
	if (IsLocallyControlledDefender(Defender))
	{
		HidePrompt(Result);
	}
}

void UDefensePromptWidget::ShowCue_Implementation(AActor *Defender, float TimeUntilWindow)
{
	SetPromptText(DefensePromptText::Cue);
}

void UDefensePromptWidget::ShowPrompt_Implementation(AActor *Defender, float WindowDuration, float AttackSize)
{
	SetPromptText(DefensePromptText::Window);
}

void UDefensePromptWidget::HidePrompt_Implementation(const FDefenseResult &Result)
{
	const FString Outcome = Result.bSuccess
								? UEnum::GetDisplayValueAsText(Result.DefenseType).ToString() + TEXT("!")
								: FString(DefensePromptText::Missed);
	SetPromptText(Outcome);

	ClearHideTimer();
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HideTimerHandle, this, &UDefensePromptWidget::HideNow,
										  ResultDisplaySeconds, /*bLoop=*/false);
	}
}

bool UDefensePromptWidget::IsLocallyControlledDefender(const AActor *Defender) const
{
	const APawn *Pawn = Cast<APawn>(Defender);
	return Pawn && Pawn->IsLocallyControlled() && Pawn->IsPlayerControlled();
}

void UDefensePromptWidget::SetPromptText(const FString &Text)
{
	ClearHideTimer();
	if (PromptText)
	{
		PromptText->SetText(FText::FromString(Text));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UDefensePromptWidget::HideNow()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UDefensePromptWidget::ClearHideTimer()
{
	if (!HideTimerHandle.IsValid())
	{
		return;
	}
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}
}
