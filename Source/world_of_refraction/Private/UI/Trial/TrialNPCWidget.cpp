// TrialNPCWidget.cpp

#include "UI/Trial/TrialNPCWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Trial/TrialNPC.h"
#include "Trial/TrialRestSpot.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "TrialNPCWidget"

namespace TrialNPCStyle
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

UTrialNPCWidget *UTrialNPCWidget::Open(APlayerController *PC, ATrialNPC *InNPC)
{
	UTrialNPCWidget *Widget = PC && InNPC ? CreateWidget<UTrialNPCWidget>(PC, UTrialNPCWidget::StaticClass()) : nullptr;
	if (!Widget)
	{
		return nullptr;
	}
	Widget->NPC = InNPC;
	Widget->AddToViewport(TrialNPCStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	UE_LOG(LogWoR, Log, TEXT("[TrialNPC] %s"), *Widget->GetTrialNPCWidgetString());
	return Widget;
}

TSharedRef<SWidget> UTrialNPCWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(TrialNPCStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		NameText = TrialNPCStyle::AddText(WidgetTree, Box);
		NoteText = TrialNPCStyle::AddText(WidgetTree, Box);
		ExtractButton = TrialNPCStyle::AddButton(WidgetTree, Box, LOCTEXT("Extract", "Extract")); // D-186, as the rest spot's
		ExtractButton->OnClicked.AddDynamic(this, &UTrialNPCWidget::HandleExtractClicked);
		ConfirmText = TrialNPCStyle::AddText(WidgetTree, Box);
		ConfirmButton = TrialNPCStyle::AddButton(WidgetTree, Box, LOCTEXT("ConfirmExtract", "Leave"));
		ConfirmButton->OnClicked.AddDynamic(this, &UTrialNPCWidget::HandleConfirmExtractClicked);
		TrialNPCStyle::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))->OnClicked.AddDynamic(this, &UTrialNPCWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UTrialNPCWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

void UTrialNPCWidget::Refresh()
{
	const ATrialNPC *Who = NPC.Get();
	const FText Refusal = WoRTrialNPC::GetExtractRefusal(GetWorld());
	const bool bOffersExtract = Who && Who->ExtractsParty();
	const bool bCanExtract = bOffersExtract && Refusal.IsEmpty();
	if (!bCanExtract)
	{
		bConfirmingExtract = false;
	}
	if (NameText)
	{
		NameText->SetText(Who ? Who->GetDisplayName() : FText::GetEmpty());
	}
	if (NoteText)
	{
		// Why the offer is greyed (a client, no run), or that this character has nothing to offer.
		const FText Note = bOffersExtract ? Refusal : LOCTEXT("NothingOffered", "Nothing to offer right now.");
		NoteText->SetText(Note);
		NoteText->SetVisibility(Note.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (ExtractButton)
	{
		ExtractButton->SetVisibility(bOffersExtract ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ExtractButton->SetIsEnabled(bCanExtract);
	}
	if (ConfirmText)
	{
		ConfirmText->SetText(bConfirmingExtract ? WoRRestSpot::GetLeaveConfirmText() : FText::GetEmpty()); // one confirm text for Extract
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetVisibility(bConfirmingExtract ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UTrialNPCWidget::HandleExtractClicked()
{
	bConfirmingExtract = true;
	Refresh();
}

void UTrialNPCWidget::HandleConfirmExtractClicked()
{
	if (WoRTrialNPC::Extract(GetWorld()))
	{
		CloseMenu(); // the run ends next tick
		return;
	}
	// Refused (the run can't be left): the panel stays and says so.
	bConfirmingExtract = false;
	Refresh();
	if (NoteText && WoRTrialNPC::GetExtractRefusal(GetWorld()).IsEmpty())
	{
		NoteText->SetText(LOCTEXT("ExtractFailed", "Couldn't extract."));
		NoteText->SetVisibility(ESlateVisibility::Visible);
	}
}

void UTrialNPCWidget::HandleCloseClicked()
{
	CloseMenu();
}

void UTrialNPCWidget::CloseMenu()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

FString UTrialNPCWidget::GetTrialNPCWidgetString() const
{
	const ATrialNPC *Who = NPC.Get();
	const FText Refusal = WoRTrialNPC::GetExtractRefusal(GetWorld());
	return FString::Printf(TEXT("Talk panel: %s; Extract: %s%s"), Who ? *Who->GetDisplayName().ToString() : TEXT("(gone)"),
						   !Who || !Who->ExtractsParty() ? TEXT("not offered") : Refusal.IsEmpty() ? TEXT("offered") : *Refusal.ToString(),
						   bConfirmingExtract ? TEXT(" (asking)") : TEXT(""));
}

#undef LOCTEXT_NAMESPACE
