// InviteScreenWidget.cpp

#include "UI/Coop/InviteScreenWidget.h"

#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Online/WoRSessionSubsystem.h"
#include "Party/Party.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRInviteScreenWidget"

namespace InviteScreenStyle
{
	// This screen's own layout (mockup 1_invite, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float FriendsWidth = 828.0f;
	constexpr float SeatsWidth = 660.0f;
	constexpr float ColumnGap = 120.0f;
	constexpr float RowHeight = 81.0f;
	constexpr float RowGap = 12.0f;
	constexpr float RowPadding = 24.0f;
	constexpr float SessionRowGap = 21.0f;
	constexpr float HeaderGap = 36.0f;
	constexpr float ValueWidth = 330.0f;
}

UInviteScreenWidget::UInviteScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its rows are plain borders
}

UInviteScreenWidget *UInviteScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[InviteScreen] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<UInviteScreenWidget>(Root->PushToMenu(UInviteScreenWidget::StaticClass()));
}

TSharedRef<SWidget> UInviteScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UInviteScreenWidget::BuildTree()
{
	using namespace InviteScreenStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(ScreenColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	TitleText = MakeText(WidgetTree, TitleFontSize, TextColor, true);
	SubtitleText = MakeText(WidgetTree, SubtitleFontSize, DimColor);
	AddToColumn(Main, TitleText);
	AddToColumn(Main, SubtitleText, FMargin(0.0f, LineGap, 0.0f, HeaderGap));
	MessageText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	AddToColumn(Main, MessageText, FMargin(0.0f, 0.0f, 0.0f, LineGap));

	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBox *FriendsColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *FriendsHeading = MakeText(WidgetTree, LineFontSize, DimColor, true);
	FriendsHeading->SetText(LOCTEXT("Friends", "Friends"));
	AddToColumn(FriendsColumn, FriendsHeading, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	FriendList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddToColumn(FriendsColumn, FriendList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	FriendsNoteText = MakeWrappedText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(FriendsColumn, FriendsNoteText, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToRow(Body, MakeSized(WidgetTree, FriendsWidth, 0.0f, FriendsColumn), FMargin(0.0f), VAlign_Fill);

	UVerticalBox *SeatsColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *SeatsHeading = MakeText(WidgetTree, LineFontSize, DimColor, true);
	SeatsHeading->SetText(LOCTEXT("Seats", "Seats"));
	AddToColumn(SeatsColumn, SeatsHeading, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	SeatList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToColumn(SeatsColumn, SeatList);
	for (int32 SlotIndex = 0; SlotIndex <= PartyConstants::MAX_PARTY_MEMBERS; ++SlotIndex) // the seats, then the session row
	{
		SeatWidgets.Add(BuildRow(SeatList, SeatsWidth));
	}
	AddToRow(Body, MakeSized(WidgetTree, SeatsWidth, 0.0f, SeatsColumn), FMargin(ColumnGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	UTextBlock *Hints = MakeText(WidgetTree, LineFontSize, DimColor);
	Hints->SetText(LOCTEXT("Hints", "A  Invite / Host        X  Friends / Seats        B / Esc  Back"));
	AddToColumn(Main, Hints, FMargin(0.0f, SectionGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Right);
}

UInviteScreenWidget::FRowWidgets UInviteScreenWidget::BuildRow(UPanelWidget *List, float Width)
{
	using namespace InviteScreenStyle;

	FRowWidgets Widgets;
	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(RowPadding, 0.0f));
	Frame->SetVerticalAlignment(VAlign_Center);
	UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Line);
	UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
	AddToRow(Line, Name, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock *Value = MakeText(WidgetTree, LineFontSize, DimColor, true);
	Value->SetJustification(ETextJustify::Right);
	AddToRow(Line, MakeSized(WidgetTree, ValueWidth, 0.0f, Value), FMargin(0.0f), VAlign_Center);
	UWidget *Sized = MakeSized(WidgetTree, Width, RowHeight, Frame);
	if (UScrollBox *Scroll = Cast<UScrollBox>(List))
	{
		if (UScrollBoxSlot *RowSlot = Cast<UScrollBoxSlot>(Scroll->AddChild(Sized)))
		{
			RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, RowGap));
		}
	}
	else if (UVerticalBox *Column = Cast<UVerticalBox>(List))
	{
		AddToColumn(Column, Sized, FMargin(0.0f, 0.0f, 0.0f, RowGap));
	}
	Widgets.Frame = Frame;
	Widgets.Name = Name;
	Widgets.Value = Value;
	return Widgets;
}

void UInviteScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	Invited.Reset();
	Message = FText::GetEmpty();
	UGameInstance *GI = GetGameInstance();
	if (UWoRSessionSubsystem *Sessions = GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr)
	{
		Sessions->OnFriendsRead.Remove(FriendsReadHandle);
		FriendsReadHandle = Sessions->OnFriendsRead.AddUObject(this, &UInviteScreenWidget::HandleFriendsRead);
	}
	WoRInviteScreen::ReadFriends(GetWorld()); // refreshes again when it lands
	RefreshData();
	UE_LOG(LogWoRUI, Log, TEXT("[InviteScreen] Opened (%d friend(s), %d seat(s))"), Page.Friends.Num(), Page.Seats.Num());
}

void UInviteScreenWidget::NativeOnDeactivated()
{
	UGameInstance *GI = GetGameInstance();
	if (UWoRSessionSubsystem *Sessions = GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr)
	{
		Sessions->OnFriendsRead.Remove(FriendsReadHandle);
	}
	FriendsReadHandle.Reset();
	Super::NativeOnDeactivated();
}

UWidget *UInviteScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UInviteScreenWidget *>(this); // the screen itself takes the keys (NativeOnKeyDown)
}

void UInviteScreenWidget::HandleFriendsRead()
{
	RefreshData();
}

void UInviteScreenWidget::RefreshData()
{
	using namespace InviteScreenStyle;

	Page = WoRInviteScreen::BuildPage(GetWorld(), Invited);
	FriendRow = Page.Friends.Num() > 0 ? FMath::Clamp(FriendRow, 0, Page.Friends.Num() - 1) : 0;
	SeatRow = FMath::Clamp(SeatRow, 0, Page.Seats.Num()); // Seats.Num() is the session row
	if (Page.Friends.IsEmpty())
	{
		bSeatsSide = true; // nothing to invite: the seats and the session row are what A can do
	}
	if (!TitleText)
	{
		return; // headless
	}
	TitleText->SetText(Page.Title);
	SubtitleText->SetText(Page.Subtitle);
	MessageText->SetText(Message);
	SetShown(MessageText, !Message.IsEmpty());
	FriendsNoteText->SetText(Page.FriendsNote);

	while (FriendWidgets.Num() < Page.Friends.Num())
	{
		FriendWidgets.Add(BuildRow(FriendList, FriendsWidth));
	}
	for (int32 Index = 0; Index < FriendWidgets.Num(); ++Index)
	{
		const FRowWidgets &Widgets = FriendWidgets[Index];
		const bool bShown = Page.Friends.IsValidIndex(Index);
		if (UBorder *Frame = Widgets.Frame.Get())
		{
			SetShown(Frame->GetParent(), bShown);
		}
		if (bShown && Widgets.Name.IsValid() && Widgets.Value.IsValid())
		{
			Widgets.Name->SetText(Page.Friends[Index].Name);
			Widgets.Value->SetText(Page.Friends[Index].Status);
			Widgets.Value->SetColorAndOpacity(FSlateColor(Page.Friends[Index].bInvited ? WarnColor : DimColor));
		}
	}

	for (int32 Index = 0; Index < SeatWidgets.Num(); ++Index)
	{
		const FRowWidgets &Widgets = SeatWidgets[Index];
		const bool bSession = IsSessionRow(Index);
		const bool bShown = bSession || Page.Seats.IsValidIndex(Index);
		if (UBorder *Frame = Widgets.Frame.Get())
		{
			SetShown(Frame->GetParent(), bShown);
		}
		if (!bShown || !Widgets.Name.IsValid() || !Widgets.Value.IsValid())
		{
			continue;
		}
		Widgets.Name->SetText(bSession ? Page.SessionAction : Page.Seats[Index].Name);
		Widgets.Value->SetText(bSession ? FText::GetEmpty() : Page.Seats[Index].Who);
		Widgets.Value->SetColorAndOpacity(FSlateColor(!bSession && Page.Seats[Index].bYours ? CyanColor : DimColor));
	}
	RefreshSelection();
}

void UInviteScreenWidget::RefreshSelection()
{
	using namespace InviteScreenStyle;

	for (int32 Index = 0; Index < FriendWidgets.Num(); ++Index)
	{
		if (UBorder *Frame = FriendWidgets[Index].Frame.Get())
		{
			Frame->SetBrushColor(!bSeatsSide && Index == FriendRow ? FocusColor : PlateColor);
		}
	}
	for (int32 Index = 0; Index < SeatWidgets.Num(); ++Index)
	{
		if (UBorder *Frame = SeatWidgets[Index].Frame.Get())
		{
			Frame->SetBrushColor(bSeatsSide && Index == SeatRow ? FocusColor : PlateColor);
		}
	}
	if (!bSeatsSide && FriendList && FriendWidgets.IsValidIndex(FriendRow) && FriendWidgets[FriendRow].Frame.IsValid())
	{
		FriendList->ScrollWidgetIntoView(FriendWidgets[FriendRow].Frame.Get(), /*AnimateScroll=*/false);
	}
}

void UInviteScreenWidget::MoveSelection(int32 Delta)
{
	int32 &Row = bSeatsSide ? SeatRow : FriendRow;
	const int32 Count = bSeatsSide ? Page.Seats.Num() + 1 : Page.Friends.Num();
	if (Count > 0)
	{
		Row = ((Row + Delta) % Count + Count) % Count;
		Message = FText::GetEmpty();
		WoRMenuStyle::SetShown(MessageText, false);
		RefreshSelection();
	}
}

void UInviteScreenWidget::SwitchSide()
{
	if (!bSeatsSide || !Page.Friends.IsEmpty())
	{
		bSeatsSide = !bSeatsSide;
		RefreshSelection();
	}
}

void UInviteScreenWidget::PressAction()
{
	if (!bSeatsSide)
	{
		if (Page.Friends.IsValidIndex(FriendRow))
		{
			Message = WoRInviteScreen::Invite(GetWorld(), FriendRow);
			if (Message.IsEmpty())
			{
				Invited.Add(Page.Friends[FriendRow].Name.ToString());
			}
			RefreshData();
		}
		return;
	}
	if (IsSessionRow(SeatRow))
	{
		Message = WoRInviteScreen::ToggleHosting(GetWorld());
		RefreshData();
	}
	// A seat itself has no action here: who plays it is set on the party screen and by a friend joining.
}

FReply UInviteScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveSelection(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsConfirmKey(Key) || IsToggleKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			IsConfirmKey(Key) ? PressAction() : SwitchSide();
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

bool UInviteScreenWidget::FindRowUnder(const FPointerEvent &MouseEvent, bool &bOutSeats, int32 &OutRow) const
{
	const FVector2D Where = MouseEvent.GetScreenSpacePosition();
	if (FriendList && FriendList->GetCachedGeometry().IsUnderLocation(Where))
	{
		for (int32 Index = 0; Index < FriendWidgets.Num() && Index < Page.Friends.Num(); ++Index)
		{
			const UBorder *Frame = FriendWidgets[Index].Frame.Get();
			if (Frame && Frame->GetCachedGeometry().IsUnderLocation(Where))
			{
				bOutSeats = false;
				OutRow = Index;
				return true;
			}
		}
	}
	for (int32 Index = 0; Index < SeatWidgets.Num() && Index <= Page.Seats.Num(); ++Index)
	{
		const UBorder *Frame = SeatWidgets[Index].Frame.Get();
		if (Frame && Frame->GetCachedGeometry().IsUnderLocation(Where))
		{
			bOutSeats = true;
			OutRow = Index;
			return true;
		}
	}
	return false;
}

FReply UInviteScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	bool bSeats = false;
	int32 Row = INDEX_NONE;
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !FindRowUnder(InMouseEvent, bSeats, Row))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	// The first click lights a row; a click on the lit row is A, so a stray click doesn't send or stop anything.
	if (bSeats == bSeatsSide && Row == GetSelectedRow())
	{
		PressAction();
	}
	else
	{
		bSeatsSide = bSeats;
		(bSeats ? SeatRow : FriendRow) = Row;
		RefreshSelection();
	}
	return FReply::Handled();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GInviteScreenOpenCommand(
	TEXT("wor.UI.PlayTogether"),
	TEXT("B-459b: open the Play together screen (friends, seats, host / stop hosting)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UInviteScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[InviteScreen] wor.UI.PlayTogether: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
