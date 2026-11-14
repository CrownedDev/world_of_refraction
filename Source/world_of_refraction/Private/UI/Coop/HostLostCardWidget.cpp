// HostLostCardWidget.cpp

#include "UI/Coop/HostLostCardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Online/HostSuccession.h"
#include "TimerManager.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRHostLostCard"

namespace HostLostCardStyle
{
	// This card's own layout (mockup 3_host_lost, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float CardWidth = 840.0f;
	constexpr float CardPadding = 36.0f;
	constexpr float PipSize = 21.0f;
	constexpr float PipGap = 27.0f;
	/** Over the HUD and the menus: nothing else can be done while the run moves. */
	constexpr int32 ZOrder = 100;
	/** A world with no local player controller yet is looked at again this often, this many times. */
	constexpr float PlayerRetrySeconds = 0.25f;
	constexpr int32 PlayerRetryLimit = 8;
	inline const FLinearColor BackdropColor(0.0f, 0.0f, 0.0f, 0.6f);
}

UHostLostCardWidget::UHostLostCardWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true); // B / Esc
}

TSharedRef<SWidget> UHostLostCardWidget::RebuildWidget()
{
	using namespace HostLostCardStyle;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
		Backdrop->SetBrushColor(BackdropColor);
		Backdrop->SetPadding(FMargin(ScreenPadding));
		WidgetTree->RootWidget = Backdrop;
		UVerticalBox *Screen = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Backdrop->SetContent(Screen);

		UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(CardPadding));
		UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);
		TitleText = MakeText(WidgetTree, NameFontSize, TextColor, true);
		HostText = MakeWrappedText(WidgetTree, RowFontSize, TextColor);
		StatusText = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
		KeptText = MakeWrappedText(WidgetTree, LineFontSize, GoodColor);
		for (UTextBlock *Text : {TitleText.Get(), HostText.Get(), StatusText.Get(), KeptText.Get()})
		{
			Text->SetJustification(ETextJustify::Center);
		}
		AddToColumn(Column, TitleText)->SetHorizontalAlignment(HAlign_Center);
		AddToColumn(Column, HostText, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
		UHorizontalBox *PipRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (int32 Index = 0; Index < WoRHostHandOverScreens::CARD_PIPS; ++Index)
		{
			UBorder *Pip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Pip->SetBrushColor(TrackColor);
			AddToRow(PipRow, MakeSized(WidgetTree, PipSize, PipSize, Pip), FMargin(Index == 0 ? 0.0f : PipGap, 0.0f, 0.0f, 0.0f));
			Pips.Add(Pip);
		}
		AddToColumn(Column, PipRow, FMargin(0.0f, SectionGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Center);
		AddToColumn(Column, StatusText, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
		AddToColumn(Column, KeptText, FMargin(0.0f, SectionGap, 0.0f, 0.0f));

		UVerticalBoxSlot *CardSlot = AddToColumn(Screen, MakeSized(WidgetTree, CardWidth, 0.0f, Panel));
		CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CardSlot->SetHorizontalAlignment(HAlign_Center);
		CardSlot->SetVerticalAlignment(VAlign_Center);
		HintText = MakeHintText(WidgetTree, LineFontSize, DimColor); // B-481: the keys of the device in use
		WoRKeyHints::SetHint(HintText, LOCTEXT("StopHint", "{Back}  Stop waiting"));
		AddToColumn(Screen, HintText)->SetHorizontalAlignment(HAlign_Right);
	}
	return Super::RebuildWidget();
}

void UHostLostCardWidget::Refresh(const FHostLostCard &InCard)
{
	using namespace HostLostCardStyle;
	Card = InCard;
	if (!TitleText)
	{
		return; // headless
	}
	TitleText->SetText(Card.Title);
	HostText->SetText(Card.HostLine);
	StatusText->SetText(Card.StatusLine);
	StatusText->SetColorAndOpacity(FSlateColor(Card.bGaveUp ? WarnColor : DimColor));
	KeptText->SetText(Card.KeptLine);
	for (int32 Index = 0; Index < Pips.Num(); ++Index)
	{
		if (Pips[Index])
		{
			Pips[Index]->SetBrushColor(Index < Card.LitPips ? CyanColor : TrackColor);
		}
	}
	SetShown(HintText, Card.bCanStop);
}

void UHostLostCardWidget::PressStop()
{
	UGameInstance *GI = GetGameInstance();
	UWoRHostHandOverSubsystem *HandOver = GI ? GI->GetSubsystem<UWoRHostHandOverSubsystem>() : nullptr;
	if (Card.bCanStop && HandOver)
	{
		HandOver->StopWaiting(); // the step goes to None and the presenter hides this card
	}
}

FReply UHostLostCardWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	if (WoRMenuStyle::IsBackKey(InKeyEvent.GetKey()))
	{
		if (!InKeyEvent.IsRepeat())
		{
			PressStop();
		}
		return FReply::Handled();
	}
	return FReply::Handled(); // nothing else reaches the world while the run moves
}

void UHostLostCardPresenter::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	if (UWoRHostHandOverSubsystem *HandOver = Collection.InitializeDependency<UWoRHostHandOverSubsystem>())
	{
		StepChangedHandle = HandOver->OnStepChanged.AddWeakLambda(this, [this](EHostHandOverStep) { Sync(); });
	}
	WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UHostLostCardPresenter::HandleWorldActorsInitialized);
}

void UHostLostCardPresenter::Deinitialize()
{
	if (UWoRHostHandOverSubsystem *HandOver = GetGameInstance() ? GetGameInstance()->GetSubsystem<UWoRHostHandOverSubsystem>() : nullptr)
	{
		HandOver->OnStepChanged.Remove(StepChangedHandle);
	}
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
	Super::Deinitialize();
}

void UHostLostCardPresenter::HandleWorldActorsInitialized(const FActorsInitializedParams &Params)
{
	UWorld *World = Params.World;
	if (!World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	// The player controller comes after the actors: look once the world has ticked.
	PlayerRetries = 0;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { Sync(); }));
}

void UHostLostCardPresenter::Sync()
{
	UWorld *World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const FHostLostCard Next = WoRHostHandOverScreens::BuildCard(World);
	if (!Next.bShown)
	{
		Hide();
		return;
	}
	UHostLostCardWidget *Shown = Card.Get();
	if (!Shown || Shown->GetWorld() != World || !Shown->IsInViewport())
	{
		APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!World || World->bIsTearingDown)
		{
			return; // going: the next world-up call shows the card
		}
		if (!PC || !PC->IsLocalController())
		{
			// A client's controller can come a little after the world: look again shortly, a few times.
			if (PlayerRetries++ < HostLostCardStyle::PlayerRetryLimit)
			{
				FTimerHandle Retry;
				World->GetTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(this, [this]() { Sync(); }),
												  HostLostCardStyle::PlayerRetrySeconds, false);
			}
			return;
		}
		PlayerRetries = 0;
		Shown = CreateWidget<UHostLostCardWidget>(PC, UHostLostCardWidget::StaticClass());
		if (!Shown)
		{
			return;
		}
		Shown->AddToViewport(HostLostCardStyle::ZOrder);
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(Shown->TakeWidget());
		PC->SetInputMode(InputMode);
		Card = Shown;
		UE_LOG(LogWoRUI, Log, TEXT("[HostLostCard] Shown"));
	}
	Shown->Refresh(Next);
}

void UHostLostCardPresenter::Hide()
{
	UHostLostCardWidget *Shown = Card.Get();
	Card.Reset();
	if (!Shown)
	{
		return;
	}
	if (APlayerController *PC = Shown->GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
	Shown->RemoveFromParent();
	UE_LOG(LogWoRUI, Log, TEXT("[HostLostCard] Hidden"));
}

#undef LOCTEXT_NAMESPACE
