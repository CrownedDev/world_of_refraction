// RecruitCardWidget.cpp

#include "UI/Party/RecruitCardWidget.h"

#include "Components/Border.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Party/PartyInteractionComponent.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRRecruitCard"

namespace RecruitCardStyle
{
	// This screen's own layout (mockup hub/hud/3_recruit_card, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float CardWidth = 900.0f;
	constexpr float CardPadding = 54.0f;
	constexpr float LabelWidth = 300.0f;
	constexpr float ChoicePadding = 18.0f;
	/** "Invite for this run", or one line per other slot that can step out. */
	constexpr int32 MaxChoices = PartyConstants::MAX_PARTY_MEMBERS;
}

URecruitCardWidget::URecruitCardWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself
}

URecruitCardWidget *URecruitCardWidget::Open(APlayerController *PC, UPartyInteractionComponent *InRecruit)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root || !InRecruit)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[RecruitCard] Not opened: %s"), !InRecruit ? TEXT("no recruit") : TEXT("no AWoRHubController with a root layout"));
		return nullptr;
	}
	URecruitCardWidget *Screen = Cast<URecruitCardWidget>(Root->PushToMenu(URecruitCardWidget::StaticClass()));
	if (Screen)
	{
		Screen->SetRecruit(InRecruit);
	}
	return Screen;
}

void URecruitCardWidget::SetRecruit(UPartyInteractionComponent *InRecruit)
{
	Recruit = InRecruit;
	SelectedChoice = 0;
	Message = FText::GetEmpty();
	Refresh();
}

TSharedRef<SWidget> URecruitCardWidget::RebuildWidget()
{
	using namespace RecruitCardStyle;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
		Screen->SetBrushColor(ScreenColor);
		Screen->SetPadding(FMargin(ScreenPadding));
		WidgetTree->RootWidget = Screen;
		UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Screen->SetContent(Main);

		UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(CardPadding));
		UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);
		UTextBlock *Title = MakeText(WidgetTree, SmallFontSize, DimColor, true);
		Title->SetText(LOCTEXT("Title", "Join for this run?"));
		AddToColumn(Column, Title)->SetHorizontalAlignment(HAlign_Center);
		NameText = MakeText(WidgetTree, NameFontSize, TextColor, true);
		AddToColumn(Column, NameText, FMargin(0.0f, LineGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Center);
		ClassText = MakeText(WidgetTree, LineFontSize, DimColor);
		AddToColumn(Column, ClassText)->SetHorizontalAlignment(HAlign_Center);
		RuleText = MakeWrappedText(WidgetTree, RowFontSize, TextColor);
		AddToColumn(Column, RuleText, FMargin(0.0f, SectionGap, 0.0f, SectionGap));

		UHorizontalBox *PlayedByLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *PlayedByLabel = MakeText(WidgetTree, LineFontSize, DimColor);
		PlayedByLabel->SetText(LOCTEXT("PlayedBy", "Played by"));
		AddToRow(PlayedByLine, MakeSized(WidgetTree, LabelWidth, 0.0f, PlayedByLabel));
		PlayedByText = MakeText(WidgetTree, RowFontSize, CyanColor, true);
		AddToRow(PlayedByLine, PlayedByText, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PlayedByText->SetJustification(ETextJustify::Right);
		AddToColumn(Column, PlayedByLine);

		for (int32 Index = 0; Index < MaxChoices; ++Index)
		{
			UBorder *Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Row->SetBrushColor(PlateColor);
			Row->SetPadding(FMargin(ChoicePadding));
			UTextBlock *Label = MakeText(WidgetTree, RowFontSize, TextColor, true);
			Row->SetContent(Label);
			ChoiceRows.Add(Row);
			ChoiceLabels.Add(Label);
			AddToColumn(Column, Row, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		}
		MessageText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
		AddToColumn(Column, MessageText, FMargin(0.0f, SectionGap, 0.0f, 0.0f));

		UVerticalBoxSlot *CardSlot = AddToColumn(Main, MakeSized(WidgetTree, CardWidth, 0.0f, Panel));
		CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CardSlot->SetHorizontalAlignment(HAlign_Center);
		CardSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock *Hints = MakeText(WidgetTree, LineFontSize, DimColor);
		Hints->SetText(LOCTEXT("Hints", "Up / Down  Choose        A  Invite        B / Esc  Not now"));
		AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
	}
	return Super::RebuildWidget();
}

void URecruitCardWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	Message = FText::GetEmpty();
	Refresh();
	UE_LOG(LogWoRUI, Log, TEXT("[RecruitCard] Opened: %s"), *WoRRecruitCard::GetCardString(Card));
}

UWidget *URecruitCardWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<URecruitCardWidget *>(this); // the screen itself takes the keys (NativeOnKeyDown)
}

int32 URecruitCardWidget::GetChoiceCount() const
{
	if (!Card.Refusal.IsEmpty() || Card.Name.IsEmpty())
	{
		return 0;
	}
	return Card.bPartyFull ? Card.StepOut.Num() : 1;
}

FText URecruitCardWidget::GetChoiceText(int32 Choice) const
{
	if (!Card.bPartyFull)
	{
		return LOCTEXT("InviteForRun", "Invite for this run");
	}
	return Card.StepOut.IsValidIndex(Choice) ? FText::Format(LOCTEXT("InviteInPlaceOf", "Invite: {0} steps out"), Card.StepOut[Choice].Name)
											 : FText::GetEmpty();
}

void URecruitCardWidget::Refresh()
{
	using namespace RecruitCardStyle;
	const UPartyInteractionComponent *Source = Recruit.Get();
	Card = Source ? Source->BuildRecruitCard() : FRecruitCard();
	if (!Source)
	{
		Card.Refusal = LOCTEXT("NoRecruit", "There is no one here to invite.");
	}
	const int32 Count = GetChoiceCount();
	SelectedChoice = Count > 0 ? FMath::Clamp(SelectedChoice, 0, Count - 1) : 0;
	if (!NameText)
	{
		return; // headless
	}
	NameText->SetText(Card.Name);
	ClassText->SetText(Card.ClassName);
	RuleText->SetText(Card.RuleLine);
	PlayedByText->SetText(Card.PlayedByText);
	for (int32 Index = 0; Index < ChoiceRows.Num(); ++Index)
	{
		const bool bShown = Index < Count;
		SetShown(ChoiceRows[Index], bShown);
		if (bShown)
		{
			ChoiceLabels[Index]->SetText(GetChoiceText(Index));
			ChoiceRows[Index]->SetBrushColor(Index == SelectedChoice ? FocusColor : PlateColor);
		}
	}
	const FText Shown = Message.IsEmpty() ? Card.Refusal : Message;
	MessageText->SetText(Shown);
	SetShown(MessageText, !Shown.IsEmpty());
}

void URecruitCardWidget::MoveSelection(int32 Delta)
{
	const int32 Count = GetChoiceCount();
	if (Count > 0)
	{
		SelectedChoice = ((SelectedChoice + Delta) % Count + Count) % Count;
		Message = FText::GetEmpty();
		Refresh();
	}
}

void URecruitCardWidget::PressInvite()
{
	UPartyInteractionComponent *Source = Recruit.Get();
	if (!Source || GetChoiceCount() == 0)
	{
		Message = Card.Refusal;
		Refresh();
		return;
	}
	const bool bJoined = Card.bPartyFull ? Source->InviteInPlaceOf(Card.StepOut[SelectedChoice].SlotIndex) : Source->InviteForRun();
	UE_LOG(LogWoRUI, Log, TEXT("[RecruitCard] %s: %s"), *GetChoiceText(SelectedChoice).ToString(), bJoined ? TEXT("joined") : TEXT("refused"));
	if (bJoined)
	{
		DeactivateWidget();
		return;
	}
	Message = LOCTEXT("InviteRefused", "They can't join now.");
	Refresh();
}

FReply URecruitCardWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveSelection(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsConfirmKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			PressInvite();
		}
		return FReply::Handled();
	}
	if (IsBackKey(Key))
	{
		// Reached only when CommonUI's own Back action didn't take the key (BP_CommonInputData unset, B-103).
		if (!InKeyEvent.IsRepeat())
		{
			HandleBackAction();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GRecruitCardOpenCommand(
	TEXT("wor.UI.RecruitCard"),
	TEXT("B-471b: open the recruit card for the character the player stands beside (A invite, B not now)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		UPartyInteractionComponent *Near = PC ? UPartyInteractionComponent::FindNearestInRange(PC->GetPawn()) : nullptr;
		if (!URecruitCardWidget::Open(PC, Near))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[RecruitCard] wor.UI.RecruitCard: not opened (stand beside a recruit in a level with a hub controller)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
