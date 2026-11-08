// RestSpotWidget.cpp

#include "UI/Hub/RestSpotWidget.h"

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
#include "Online/HostSuccession.h"
#include "UI/Hub/InventoryWidget.h"
#include "UI/Hub/LevelDropWidget.h"

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
		RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("LevelUp", "Level up"))
			->OnClicked.AddDynamic(this, &URestSpotWidget::HandleLevelUpClicked); // B-363
		RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("Inventory", "Inventory"))
			->OnClicked.AddDynamic(this, &URestSpotWidget::HandleInventoryClicked);
		ReviveButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("Revive", "Revive the mini-boss"));
		ReviveButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleReviveClicked);
		ReviveLabel = Cast<UTextBlock>(ReviveButton->GetChildAt(0));
		HandOverButton = RestSpotStyle::AddButton(WidgetTree, Box, LOCTEXT("HandOver", "Hand over host (the next player hosts; you rejoin)"));
		HandOverButton->OnClicked.AddDynamic(this, &URestSpotWidget::HandleHandOverClicked);
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
	// B-187d: on a client the run lives on the host: no earnings or Leave here until B-187e routes them.
	const bool bOnClient = GetWorld() && GetWorld()->GetNetMode() == NM_Client;
	if (LeaveButton)
	{
		LeaveButton->SetVisibility(bOnClient ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
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
		ConfirmText->SetText(bConfirmingLeave ? WoRRestSpot::GetLeaveConfirmText() : FText::GetEmpty());
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetVisibility(bConfirmingLeave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (HandOverButton)
	{
		// B-234c: only a host with another player can hand over.
		const UWoRHostHandOverSubsystem *HandOver = GI ? GI->GetSubsystem<UWoRHostHandOverSubsystem>() : nullptr;
		const UWorld *World = GetWorld();
		const bool bCanHandOver = HandOver && World && WoRHostSuccession::CanHandOverFrom(World->GetNetMode(), HandOver->GetKnownOrder().Num());
		HandOverButton->SetVisibility(bCanHandOver ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	// B-187b: only at a mini-boss's rest spot that can still be revived.
	const bool bCanRevive = WoRRestSpot::CanReviveHere(GetWorld());
	if (ReviveButton)
	{
		ReviveButton->SetVisibility(bCanRevive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ReviveLabel && bCanRevive && Run)
	{
		const int32 Done = Run->GetEncounterRevivals(Run->GetLastRestSpot());
		ReviveLabel->SetText(FText::Format(LOCTEXT("ReviveCount", "Revive the mini-boss: one tier stronger, better drops ({0} of {1})"),
										   FText::AsNumber(Done + 1), FText::AsNumber(MiniBossRevival::MAX_REVIVALS)));
	}
}

void URestSpotWidget::HandleReviveClicked()
{
	UWorld *World = GetWorld();
	CloseMenu();
	WoRRestSpot::ReviveMiniBoss(World);
}

FString URestSpotWidget::GetRestSpotWidgetString() const
{
	return FString::Printf(TEXT("[RestSpotMenu] %s | %s"), PayoutText ? *PayoutText->GetText().ToString() : TEXT("-"),
						   bConfirmingLeave ? TEXT("confirming leave") : TEXT("menu"));
}

void URestSpotWidget::HandleLevelUpClicked()
{
	APlayerController *PC = GetOwningPlayer();
	CloseMenu();
	ULevelDropWidget::Open(PC); // B-363 (D-156): place and buy pillar points here
}

void URestSpotWidget::HandleInventoryClicked()
{
	APlayerController *PC = GetOwningPlayer();
	WoRRestSpot::StepAway(GetWorld()); // review: the rest spot's levelling ends with its menu
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
	CloseMenu();
	WoRRestSpot::LeaveRun(World);
}

void URestSpotWidget::HandleHandOverClicked()
{
	UWorld *World = GetWorld();
	UGameInstance *GI = GetGameInstance();
	UWoRHostHandOverSubsystem *HandOver = GI ? GI->GetSubsystem<UWoRHostHandOverSubsystem>() : nullptr;
	CloseMenu();
	if (HandOver)
	{
		HandOver->BeginPlannedHandOver(World);
	}
}

void URestSpotWidget::HandleCloseClicked()
{
	WoRRestSpot::StepAway(GetWorld()); // review: closing the menu walks away from the rest spot
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
