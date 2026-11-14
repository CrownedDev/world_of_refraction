// RewardPopupsWidget.cpp

#include "UI/Trial/RewardPopupsWidget.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/Menus/MenuScreenStyle.h"

#define LOCTEXT_NAMESPACE "WoRRewardPopups"

namespace RewardPopupsStyle
{
	// The mockup's popup (run/run_end/make.py, 1280x720, at 1080p): 380 wide, an edge bar in the kind's colour.
	using namespace WoRMenuStyle;
	constexpr float PopupWidth = 570.0f;
	constexpr float EdgeBarWidth = 6.0f;
	constexpr float PopupPaddingX = 24.0f;
	constexpr float PopupPaddingY = 12.0f;
	constexpr float PopupGap = 12.0f;
	/** Often enough for a smooth-enough fade on a placeholder; the timer stops when nothing is up. */
	constexpr float UpdateSeconds = 0.05f;
}

TSharedRef<SWidget> URewardPopupsWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PopupStack"));
		WidgetTree->RootWidget = Stack;
	}
	return Super::RebuildWidget();
}

void URewardPopupsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible); // never takes a click
	const UGameInstance *GameInstance = GetGameInstance();
	if (URewardNoticeSubsystem *Notices = GameInstance ? GameInstance->GetSubsystem<URewardNoticeSubsystem>() : nullptr)
	{
		PostedHandle = Notices->OnPosted.AddUObject(this, &URewardPopupsWidget::HandlePosted);
	}
	TakeNotices(); // what the fight left waiting
}

void URewardPopupsWidget::NativeDestruct()
{
	const UGameInstance *GameInstance = GetGameInstance();
	if (URewardNoticeSubsystem *Notices = GameInstance ? GameInstance->GetSubsystem<URewardNoticeSubsystem>() : nullptr)
	{
		Notices->OnPosted.Remove(PostedHandle);
		// The level is going (back from the field to a fight, or to the hub): the unshown ones wait for the next HUD.
		for (const FRewardNotice &Notice : Waiting)
		{
			Notices->Post(Notice);
		}
	}
	Waiting.Reset();
	PostedHandle.Reset();
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PopupTimer);
	}
	Super::NativeDestruct();
}

void URewardPopupsWidget::HandlePosted()
{
	TakeNotices();
}

double URewardPopupsWidget::GetNowSeconds() const
{
	const UWorld *World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

void URewardPopupsWidget::TakeNotices()
{
	const UGameInstance *GameInstance = GetGameInstance();
	if (URewardNoticeSubsystem *Notices = GameInstance ? GameInstance->GetSubsystem<URewardNoticeSubsystem>() : nullptr)
	{
		Waiting.Append(Notices->TakeWaiting());
	}
	UpdatePopups(GetNowSeconds());
	UWorld *World = GetWorld();
	if (World && (!Shown.IsEmpty() || !Waiting.IsEmpty()) && !World->GetTimerManager().IsTimerActive(PopupTimer))
	{
		World->GetTimerManager().SetTimer(PopupTimer, FTimerDelegate::CreateUObject(this, &URewardPopupsWidget::HandleTimer),
										  RewardPopupsStyle::UpdateSeconds, /*bLoop=*/true);
	}
}

void URewardPopupsWidget::HandleTimer()
{
	UpdatePopups(GetNowSeconds());
	if (Shown.IsEmpty() && Waiting.IsEmpty())
	{
		if (UWorld *World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PopupTimer);
		}
	}
}

void URewardPopupsWidget::UpdatePopups(double NowSeconds)
{
	using namespace WoRRewardNotice;
	for (int32 Index = Shown.Num() - 1; Index >= 0; --Index)
	{
		const double Age = NowSeconds - Shown[Index].ShownAtSeconds;
		UWidget *Box = Shown[Index].Box.Get();
		if (Age >= SHOWN_SECONDS)
		{
			if (Box)
			{
				Box->RemoveFromParent();
			}
			Shown.RemoveAt(Index);
			continue;
		}
		if (Box)
		{
			const double FadeStart = SHOWN_SECONDS - FADE_SECONDS;
			Box->SetRenderOpacity(Age <= FadeStart ? 1.0f : static_cast<float>(FMath::Clamp((SHOWN_SECONDS - Age) / FADE_SECONDS, 0.0, 1.0)));
		}
	}
	ShowNext(NowSeconds);
}

void URewardPopupsWidget::ShowNext(double NowSeconds)
{
	while (!Waiting.IsEmpty() && Shown.Num() < WoRRewardNotice::MAX_SHOWN)
	{
		FShownPopup Popup;
		Popup.Notice = Waiting[0];
		Popup.ShownAtSeconds = NowSeconds;
		Popup.Box = MakePopupBox(Popup.Notice);
		Waiting.RemoveAt(0);
		Shown.Add(MoveTemp(Popup));
	}
}

UWidget *URewardPopupsWidget::MakePopupBox(const FRewardNotice &Notice)
{
	using namespace RewardPopupsStyle;
	if (!Stack || !WidgetTree)
	{
		return nullptr; // headless: the list alone
	}
	const FLinearColor KindColor = WoRRewardNotice::GetColor(Notice.Kind);
	UBorder *Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Box->SetBrushColor(PanelColor);
	Box->SetPadding(FMargin(0.0f));
	UHorizontalBox *Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Box->SetContent(Row);

	UBorder *EdgeBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	EdgeBar->SetBrushColor(KindColor);
	AddToRow(Row, MakeSized(WidgetTree, EdgeBarWidth, 0.0f, EdgeBar), FMargin(0.0f), VAlign_Fill);

	UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox *HeadRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *Head = MakeText(WidgetTree, SmallFontSize, KindColor, true);
	Head->SetText(Notice.Head);
	AddToRow(HeadRow, Head, FMargin(0.0f, 0.0f, WordGap, 0.0f));
	if (Notice.bOnlyYou)
	{
		UTextBlock *OnlyYou = MakeText(WidgetTree, SmallFontSize, CyanColor);
		OnlyYou->SetText(LOCTEXT("OnlyYou", "only you see this")); // D-171
		AddToRow(HeadRow, OnlyYou);
	}
	AddToColumn(Column, HeadRow);
	if (!Notice.Body.IsEmpty())
	{
		UTextBlock *Body = MakeWrappedText(WidgetTree, LineFontSize, TextColor, true);
		Body->SetText(Notice.Body);
		AddToColumn(Column, Body);
	}
	UHorizontalBoxSlot *ColumnSlot = AddToRow(Row, Column, FMargin(PopupPaddingX, PopupPaddingY));
	ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	USizeBox *Popup = MakeSized(WidgetTree, PopupWidth, 0.0f, Box);
	AddToColumn(Stack, Popup, FMargin(0.0f, 0.0f, 0.0f, PopupGap));
	return Popup;
}

TArray<FRewardNotice> URewardPopupsWidget::GetShown() const
{
	TArray<FRewardNotice> Notices;
	for (const FShownPopup &Popup : Shown)
	{
		Notices.Add(Popup.Notice);
	}
	return Notices;
}

FString URewardPopupsWidget::GetPopupsString() const
{
	TArray<FString> Lines;
	for (const FShownPopup &Popup : Shown)
	{
		Lines.Add(WoRRewardNotice::GetNoticeString(Popup.Notice));
	}
	return FString::Printf(TEXT("[RewardPopups] %d shown, %d waiting: %s"), Shown.Num(), Waiting.Num(), *FString::Join(Lines, TEXT(" / ")));
}

#undef LOCTEXT_NAMESPACE
