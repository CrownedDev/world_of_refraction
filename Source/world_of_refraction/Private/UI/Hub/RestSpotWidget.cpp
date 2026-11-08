// RestSpotWidget.cpp

#include "UI/Hub/RestSpotWidget.h"

#include "Player/WoRPlayerState.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Trial/RunRewards.h"
#include "Trial/TrialRestSpot.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Hub/InventoryWidget.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "RestSpotWidget"

namespace RestSpotStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;

	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(RowPadding));
		}
		return Button;
	}
}

URestSpotWidget *URestSpotWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	URestSpotWidget *Widget = CreateWidget<URestSpotWidget>(PC, URestSpotWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(RestSpotStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> URestSpotWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(RestSpotStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		RestSpotStyle::AddText(WidgetTree, Box)->SetText(LOCTEXT("Rested", "Rest spot: the party is restored."));
		PayoutText = RestSpotStyle::AddText(WidgetTree, Box);
		RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("Inventory", "Inventory"))
			->OnClicked.AddDynamic(this, &URestSpotWidget::HandleInventoryClicked);
		ReviveButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("Revive", "Revive the mini-boss"));
		ReviveButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleReviveClicked);
		ReviveLabel = Cast<UTextBlock>(ReviveButton->GetChildAt(0));
		LeaveButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("Leave", "Leave the trial"));
		LeaveButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleLeaveClicked);
		ConfirmText = RestSpotStyle::AddText(WidgetTree, Box);
		ConfirmButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("ConfirmLeave", "Confirm: leave"));
		ConfirmButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleConfirmLeaveClicked);
		RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))->OnClicked.AddDynamic(this, &URestSpotWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void URestSpotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

void URestSpotWidget::Refresh()
{
	const UGameInstance *GI = GetGameInstance();
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	// B-187d / B-187e: on a client the run lives on the host: no earnings here; Leave leaves the session (Q-187e).
	const bool bOnClient = GetWorld() && GetWorld()->GetNetMode() == NM_Client;
	if (UTextBlock *LeaveLabel = LeaveButton ? Cast<UTextBlock>(LeaveButton->GetChildAt(0)) : nullptr)
	{
		LeaveLabel->SetText(bOnClient ? LOCTEXT("LeaveSession", "Leave the session") : LOCTEXT("Leave", "Leave the trial"));
	}
	if (PayoutText)
	{
		PayoutText->SetVisibility(bOnClient ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		const FRunPayout Earned = Run ? Run->GetRunEndPayout() : FRunPayout();
		PayoutText->SetText(FText::Format(LOCTEXT("Earned", "Earned so far: {0} Gold, {1} Gear Essence, {2} Skill Essence (leaving now pays {3}%)."),
										  FText::AsNumber(Earned.HubGold), FText::AsNumber(Earned.GearEssence), FText::AsNumber(Earned.SkillEssence),
										  FText::AsNumber(FMath::RoundToInt(RunRewards::LEAVE_EARLY_SHARE * 100.0f))));
	}
	if (ConfirmText)
	{
		const FText Confirm = bOnClient ? LOCTEXT("LeaveSessionConfirm", "Leave your friends' run? You go back to your hub; this run's rewards stay with them.")
										: WoRRestSpot::GetLeaveConfirmText();
		ConfirmText->SetText(bConfirmingLeave ? Confirm : FText::GetEmpty());
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetVisibility(bConfirmingLeave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	// B-187b: only at a mini-boss's rest spot that can still be revived.
	// B-187e: a client shows what the host said (SetHostRevive).
	const bool bCanRevive = bOnClient ? bHostCanRevive : WoRRestSpot::CanReviveHere(GetWorld());
	if (ReviveButton)
	{
		ReviveButton->SetVisibility(bCanRevive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	const int32 RevivalNumber = bOnClient ? HostRevivalNumber : WoRRestSpot::GetNextRevivalNumber(GetWorld());
	if (ReviveLabel && bCanRevive)
	{
		ReviveLabel->SetText(FText::Format(LOCTEXT("ReviveCount", "Revive the mini-boss: one tier stronger, better drops ({0} of {1})"),
										   FText::AsNumber(RevivalNumber), FText::AsNumber(MiniBossRevival::MAX_REVIVALS)));
	}
}

void URestSpotWidget::SetHostRevive(bool bCanRevive, int32 RevivalNumber)
{
	bHostCanRevive = bCanRevive;
	HostRevivalNumber = RevivalNumber;
	Refresh();
}

void URestSpotWidget::HandleReviveClicked()
{
	UWorld *World = GetWorld();
	APlayerController *PC = GetOwningPlayer();
	CloseMenu();
	// B-187e: a client asks the host, whose run revives it.
	if (World && World->GetNetMode() == NM_Client)
	{
		if (AWoRPlayerState *Player = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr)
		{
			Player->ServerReviveMiniBoss();
		}
		return;
	}
	WoRRestSpot::ReviveMiniBoss(World);
}

FString URestSpotWidget::GetRestSpotWidgetString() const
{
	return FString::Printf(TEXT("[RestSpotMenu] %s | %s | host revive: %s (%d)"), PayoutText ? *PayoutText->GetText().ToString() : TEXT("-"),
						   bConfirmingLeave ? TEXT("confirming leave") : TEXT("menu"), bHostCanRevive ? TEXT("yes") : TEXT("no"), HostRevivalNumber);
}

void URestSpotWidget::HandleInventoryClicked()
{
	APlayerController *PC = GetOwningPlayer();
	CloseMenu();
	UInventoryWidget::Open(PC);
}

void URestSpotWidget::HandleLeaveClicked()
{
	bConfirmingLeave = true;
	Refresh();
}

void URestSpotWidget::HandleConfirmLeaveClicked()
{
	UWorld *World = GetWorld();
	APlayerController *PC = GetOwningPlayer();
	CloseMenu();
	// B-187e (Q-187e): a client leaves the session, not the host's run (Monster Hunter: one hunter abandons alone).
	if (World && World->GetNetMode() == NM_Client && PC)
	{
		UE_LOG(LogWoR, Log, TEXT("[RestSpot] A client leaves the session from the rest menu"));
		PC->ConsoleCommand(TEXT("disconnect"));
		return;
	}
	WoRRestSpot::LeaveRun(World);
}

void URestSpotWidget::HandleCloseClicked()
{
	CloseMenu();
}

void URestSpotWidget::CloseMenu()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

#undef LOCTEXT_NAMESPACE
