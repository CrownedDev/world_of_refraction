// JoinSeatWidget.cpp

#include "UI/Party/JoinSeatWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Character/CharacterData.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/RunStateComponent.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "JoinSeat"

namespace JoinSeatStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace JoinSeatBuild
{
	UTextBlock *AddButton(UWidgetTree *Tree, UVerticalBox *Box, UButton *&OutButton)
	{
		OutButton = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = JoinSeatStyle::FontSize;
		Label->SetFont(Font);
		OutButton->AddChild(Label);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(OutButton))
		{
			BoxSlot->SetPadding(FMargin(JoinSeatStyle::RowPadding));
		}
		return Label;
	}

	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = JoinSeatStyle::FontSize;
		Text->SetFont(Font);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(JoinSeatStyle::RowPadding));
		}
		return Text;
	}

	UPartySessionSubsystem *PartySessionOf(const UUserWidget *Widget)
	{
		const UGameInstance *GI = Widget ? Widget->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	}
}

UJoinSeatWidget *UJoinSeatWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UJoinSeatWidget *Widget = CreateWidget<UJoinSeatWidget>(PC, UJoinSeatWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(JoinSeatStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UJoinSeatWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(JoinSeatStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		RequestsText = JoinSeatBuild::AddText(WidgetTree, Box);
		for (int32 SlotIndex = 0; SlotIndex < PartyConstants::MAX_PARTY_MEMBERS; ++SlotIndex)
		{
			UButton *Button = nullptr;
			SeatTexts.Add(JoinSeatBuild::AddButton(WidgetTree, Box, Button));
			SeatButtons.Add(Button);
		}
		SeatButtons[0]->SetVisibility(ESlateVisibility::Collapsed); // the leader's slot is never an AI seat
		SeatButtons[1]->OnClicked.AddDynamic(this, &UJoinSeatWidget::HandleSlot1Clicked);
		SeatButtons[2]->OnClicked.AddDynamic(this, &UJoinSeatWidget::HandleSlot2Clicked);

		UButton *RefuseButton = nullptr;
		JoinSeatBuild::AddButton(WidgetTree, Box, RefuseButton)->SetText(LOCTEXT("Refuse", "Refuse the first request"));
		RefuseButton->OnClicked.AddDynamic(this, &UJoinSeatWidget::HandleRefuseClicked);

		UButton *CloseButton = nullptr;
		JoinSeatBuild::AddButton(WidgetTree, Box, CloseButton)->SetText(LOCTEXT("Close", "Close"));
		CloseButton->OnClicked.AddDynamic(this, &UJoinSeatWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UJoinSeatWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UPartySessionSubsystem *PartySession = JoinSeatBuild::PartySessionOf(this))
	{
		PartySession->OnPartyChanged.AddUniqueDynamic(this, &UJoinSeatWidget::HandlePartyChanged);
	}
	Refresh();
}

void UJoinSeatWidget::NativeDestruct()
{
	if (UPartySessionSubsystem *PartySession = JoinSeatBuild::PartySessionOf(this))
	{
		PartySession->OnPartyChanged.RemoveDynamic(this, &UJoinSeatWidget::HandlePartyChanged);
	}
	Super::NativeDestruct();
}

void UJoinSeatWidget::Refresh()
{
	const URunStateComponent *Run = URunStateComponent::Find(this);
	const UPartySessionSubsystem *PartySession = JoinSeatBuild::PartySessionOf(this);
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	if (!RequestsText)
	{
		return;
	}
	if (!Run || !Party)
	{
		RequestsText->SetText(LOCTEXT("NoRun", "No run or party here"));
		for (UButton *Button : SeatButtons)
		{
			if (Button) Button->SetIsEnabled(false);
		}
		return;
	}
	FText Requests = LOCTEXT("NoRequests", "Nobody is waiting to join");
	const TArray<FRunJoinRequest> &Waiting = Run->GetJoinRequests();
	for (int32 Index = 0; Index < Waiting.Num(); ++Index)
	{
		const FText Line = FText::Format(LOCTEXT("RequestLine", "Player {0} wants to join with {1}"), Waiting[Index].PlayerId,
										 FText::FromString(Waiting[Index].Character ? Waiting[Index].Character->Name : TEXT("?")));
		Requests = Index == 0 ? Line : FText::Format(LOCTEXT("TwoLines", "{0}\n{1}"), Requests, Line);
	}
	RequestsText->SetText(Requests);

	const bool bAnyRequest = Run->GetJoinRequests().Num() > 0;
	for (int32 SlotIndex = 1; SlotIndex < SeatTexts.Num(); ++SlotIndex)
	{
		const bool bSeat = Party->Members.IsValidIndex(SlotIndex) && Party->Members[SlotIndex].bAISeat;
		const FText Name = Party->Members.IsValidIndex(SlotIndex) && Party->Members[SlotIndex].CharacterData
							   ? FText::FromString(Party->Members[SlotIndex].CharacterData->Name)
							   : LOCTEXT("EmptySlot", "empty");
		SeatTexts[SlotIndex]->SetText(bSeat ? FText::Format(LOCTEXT("GiveSeat", "Give slot {0} ({1}, AI seat) to the first request"), SlotIndex, Name)
											: FText::Format(LOCTEXT("NotASeat", "Slot {0} ({1}): not an AI seat"), SlotIndex, Name));
		SeatButtons[SlotIndex]->SetIsEnabled(bSeat && bAnyRequest);
	}
}

FString UJoinSeatWidget::GetJoinSeatString() const
{
	FString Out = RequestsText ? RequestsText->GetText().ToString() : FString();
	for (int32 SlotIndex = 1; SlotIndex < SeatTexts.Num(); ++SlotIndex)
	{
		Out += TEXT("\n") + SeatTexts[SlotIndex]->GetText().ToString();
	}
	return Out;
}

int32 UJoinSeatWidget::FirstRequestPlayerId() const
{
	const URunStateComponent *Run = URunStateComponent::Find(this);
	return Run && Run->GetJoinRequests().Num() > 0 ? Run->GetJoinRequests()[0].PlayerId : INDEX_NONE;
}

void UJoinSeatWidget::GiveSeat(int32 SlotIndex)
{
	URunStateComponent *Run = URunStateComponent::Find(this);
	const int32 PlayerId = FirstRequestPlayerId();
	if (!Run || PlayerId == INDEX_NONE)
	{
		UE_LOG(LogWoR, Display, TEXT("[JoinSeat] nobody is waiting to join"));
		return;
	}
	Run->AcceptJoin(PlayerId, SlotIndex);
	Refresh();
}

void UJoinSeatWidget::HandleSlot1Clicked() { GiveSeat(1); }
void UJoinSeatWidget::HandleSlot2Clicked() { GiveSeat(2); }

void UJoinSeatWidget::HandleRefuseClicked()
{
	URunStateComponent *Run = URunStateComponent::Find(this);
	const int32 PlayerId = FirstRequestPlayerId();
	if (Run && PlayerId != INDEX_NONE)
	{
		Run->RefuseJoin(PlayerId);
	}
	Refresh();
}

void UJoinSeatWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

void UJoinSeatWidget::HandlePartyChanged(UParty * /*Party*/)
{
	Refresh();
}

static FAutoConsoleCommandWithWorld GJoinSeatUICommand(
	TEXT("wor.UI.JoinSeat"),
	TEXT("PLACEHOLDER UI (D-150), host only: choose which AI seat a joining player takes."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!UJoinSeatWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogWoR, Warning, TEXT("[JoinSeat] wor.UI.JoinSeat: no player controller"));
			}
		}));

#undef LOCTEXT_NAMESPACE
