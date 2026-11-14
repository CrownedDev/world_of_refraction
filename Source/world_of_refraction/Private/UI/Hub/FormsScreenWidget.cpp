// FormsScreenWidget.cpp

#include "UI/Hub/FormsScreenWidget.h"

#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRFormsScreenWidget"

namespace FormsScreenStyle
{
	// This screen's own layout (screen_mockups/hub/skins, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float ListWidth = 828.0f;
	constexpr float RowHeight = 111.0f;
	constexpr float RowGap = 15.0f;
	constexpr float RowPadding = 21.0f;
	constexpr float MeterWidth = 450.0f;
	constexpr float PanelGap = 45.0f;
	constexpr float PanelWidth = 903.0f;
	constexpr float PanelPadding = 30.0f;
	constexpr float HeaderGap = 36.0f;
	/** The unlocked meter's gold (mockup 1_forms). */
	inline const FLinearColor UnlockedColor = FLinearColor::FromSRGBColor(FColor(232, 196, 110));
}

UFormsScreenWidget::UFormsScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its rows are plain borders
}

UFormsScreenWidget *UFormsScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[FormsScreen] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<UFormsScreenWidget>(Root->PushToMenu(UFormsScreenWidget::StaticClass()));
}

TSharedRef<SWidget> UFormsScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UFormsScreenWidget::BuildTree()
{
	using namespace FormsScreenStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(ScreenColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	TitleText = MakeText(WidgetTree, TitleFontSize, TextColor, true);
	WearingText = MakeText(WidgetTree, SubtitleFontSize, DimColor);
	AddToColumn(Main, TitleText);
	AddToColumn(Main, WearingText, FMargin(0.0f, LineGap, 0.0f, HeaderGap));

	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox *ListColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MessageText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	AddToColumn(ListColumn, MessageText, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	RowList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddToColumn(ListColumn, RowList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Body, MakeSized(WidgetTree, ListWidth, 0.0f, ListColumn), FMargin(0.0f), VAlign_Fill);

	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	UVerticalBox *PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(PanelColumn);
	PanelName = MakeText(WidgetTree, NameFontSize, TextColor, true);
	PanelPercent = MakeText(WidgetTree, RowFontSize, CyanColor, true);
	PanelStatus = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	PiecesHeading = MakeText(WidgetTree, LineFontSize, DimColor, true);
	PiecesHeading->SetText(LOCTEXT("PiecesHeading", "Their pieces"));
	// TODO: B-444, the pieces as icon tiles (mockup 1_forms); a list of names for now.
	PiecesText = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	UTextBlock *About = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	About->SetText(LOCTEXT("About", "At 100% the form unlocks and their full loadout is revealed. A form changes looks and makes their bound gear work; it gives no stats."));
	UTextBlock *Where = MakeWrappedText(WidgetTree, SmallFontSize, OffColor);
	Where->SetText(LOCTEXT("Where", "Change form in the hub or at a rest spot."));
	AddToColumn(PanelColumn, PanelName);
	AddToColumn(PanelColumn, PanelPercent, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(PanelColumn, PanelStatus, FMargin(0.0f, LineGap, 0.0f, SectionGap));
	AddToColumn(PanelColumn, PiecesHeading);
	AddToColumn(PanelColumn, PiecesText, FMargin(0.0f, LineGap, 0.0f, SectionGap));
	AddToColumn(PanelColumn, About, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	AddToColumn(PanelColumn, Where);
	AddToRow(Body, MakeSized(WidgetTree, PanelWidth, 0.0f, Panel), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	UTextBlock *Hints = MakeText(WidgetTree, LineFontSize, DimColor);
	Hints->SetText(LOCTEXT("Hints", "A  Wear this form        Y  Base body        B / Esc  Back"));
	AddToColumn(Main, Hints, FMargin(0.0f, SectionGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Right);
}

UFormsScreenWidget::FRowWidgets UFormsScreenWidget::BuildRow()
{
	using namespace FormsScreenStyle;

	FRowWidgets Widgets;
	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(RowPadding));
	Frame->SetVerticalAlignment(VAlign_Center);
	UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Frame->SetContent(Column);
	UHorizontalBox *NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
	UTextBlock *Tag = MakeText(WidgetTree, SmallFontSize, CyanColor, true);
	AddToRow(NameRow, Name)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(NameRow, Tag, FMargin(0.0f), VAlign_Center);
	UTextBlock *Line = MakeText(WidgetTree, SmallFontSize, DimColor);
	UProgressBar *Bar = nullptr;
	UTextBlock *Percent = nullptr;
	UWidget *Meter = MakeSized(WidgetTree, MeterWidth, 0.0f, MakeBarWithValue(WidgetTree, CyanColor, Bar, Percent));
	AddToColumn(Column, NameRow);
	AddToColumn(Column, Line);
	AddToColumn(Column, Meter, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	if (UScrollBoxSlot *RowSlot = Cast<UScrollBoxSlot>(RowList->AddChild(MakeSized(WidgetTree, ListWidth, RowHeight, Frame))))
	{
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, RowGap));
	}

	Widgets.Frame = Frame;
	Widgets.Name = Name;
	Widgets.Tag = Tag;
	Widgets.Line = Line;
	Widgets.Meter = Meter;
	Widgets.Bar = Bar;
	Widgets.Percent = Percent;
	return Widgets;
}

void UFormsScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshData();
	UE_LOG(LogWoRUI, Log, TEXT("[FormsScreen] Opened (%d row(s))"), Page.Rows.Num());
}

UWidget *UFormsScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UFormsScreenWidget *>(this);
}

void UFormsScreenWidget::RefreshData()
{
	using namespace FormsScreenStyle;

	Page = WoRFormsScreen::BuildPage(GetWorld());
	SelectedRow = Page.Rows.Num() > 0 ? FMath::Clamp(SelectedRow, 0, Page.Rows.Num() - 1) : 0;
	if (TitleText)
	{
		TitleText->SetText(Page.Title);
		WearingText->SetText(Page.WearingLine);
		MessageText->SetText(Message);
		SetShown(MessageText, !Message.IsEmpty());
	}
	if (RowList)
	{
		while (RowWidgets.Num() < Page.Rows.Num())
		{
			RowWidgets.Add(BuildRow());
		}
	}
	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		const FRowWidgets &Widgets = RowWidgets[Index];
		const bool bShown = Page.Rows.IsValidIndex(Index);
		if (UBorder *Frame = Widgets.Frame.Get())
		{
			SetShown(Frame->GetParent(), bShown);
		}
		if (!bShown || !Widgets.Name.IsValid() || !Widgets.Tag.IsValid() || !Widgets.Line.IsValid() || !Widgets.Bar.IsValid() || !Widgets.Percent.IsValid())
		{
			continue;
		}
		const FFormsScreenRow &Row = Page.Rows[Index];
		Widgets.Name->SetText(Row.Name);
		Widgets.Name->SetColorAndOpacity(FSlateColor(Row.bMet ? TextColor : OffColor));
		Widgets.Tag->SetText(Row.bWorn ? LOCTEXT("Worn", "WORN") : Row.bUnlocked && Row.Form ? LOCTEXT("UnlockedTag", "unlocked") : FText::GetEmpty());
		Widgets.Tag->SetColorAndOpacity(FSlateColor(Row.bWorn ? CyanColor : UnlockedColor));
		Widgets.Line->SetText(Row.Line);
		SetShown(Widgets.Line.Get(), !Row.Line.IsEmpty());
		SetShown(Widgets.Meter.Get(), Row.Form && Row.bMet); // your own form has no meter; an unmet one shows nothing
		Widgets.Bar->SetPercent(Row.Progress);
		Widgets.Bar->SetFillColorAndOpacity(Row.bUnlocked ? UnlockedColor : CyanColor);
		Widgets.Percent->SetText(FText::AsPercent(Row.Progress));
	}
	RefreshSelection();
}

void UFormsScreenWidget::RefreshSelection()
{
	using namespace FormsScreenStyle;

	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		if (UBorder *Frame = RowWidgets[Index].Frame.Get())
		{
			Frame->SetBrushColor(Index == SelectedRow ? FocusColor : PlateColor);
		}
	}
	if (!PanelName || !Page.Rows.IsValidIndex(SelectedRow))
	{
		return; // headless, or nothing to show
	}
	const FFormsScreenRow &Row = Page.Rows[SelectedRow];
	PanelName->SetText(Row.Name);
	PanelPercent->SetText(FText::AsPercent(Row.Progress));
	SetShown(PanelPercent, Row.Form && Row.bMet);
	PanelPercent->SetColorAndOpacity(FSlateColor(Row.bUnlocked ? UnlockedColor : CyanColor));
	PanelStatus->SetText(Row.Status);
	TArray<FText> Lines;
	for (const FFormsScreenPiece &Piece : Row.Pieces)
	{
		Lines.Add(Piece.bWorn ? FText::Format(LOCTEXT("PieceWorn", "{0}  · worn"), Piece.Name)
				  : Piece.bOwned ? FText::Format(LOCTEXT("PieceOwned", "{0}  · owned"), Piece.Name)
								 : Piece.Name);
	}
	PiecesText->SetText(FText::Join(FText::FromString(TEXT("\n")), Lines));
	SetShown(PiecesHeading, Lines.Num() > 0);
	SetShown(PiecesText, Lines.Num() > 0);
	if (RowList && RowWidgets.IsValidIndex(SelectedRow) && RowWidgets[SelectedRow].Frame.IsValid())
	{
		RowList->ScrollWidgetIntoView(RowWidgets[SelectedRow].Frame.Get(), /*AnimateScroll=*/false);
	}
}

void UFormsScreenWidget::MoveSelection(int32 Delta)
{
	const int32 Count = Page.Rows.Num();
	if (Count > 0)
	{
		SelectRow(((SelectedRow + Delta) % Count + Count) % Count);
	}
}

void UFormsScreenWidget::SelectRow(int32 Index)
{
	if (!Page.Rows.IsValidIndex(Index) || Index == SelectedRow)
	{
		return;
	}
	SelectedRow = Index;
	if (!Message.IsEmpty())
	{
		Message = FText::GetEmpty(); // a refusal belongs to the row it was about
		WoRMenuStyle::SetShown(MessageText, false);
	}
	RefreshSelection();
}

void UFormsScreenWidget::PressWear()
{
	if (Page.Rows.IsValidIndex(SelectedRow))
	{
		Message = WoRFormsScreen::Wear(GetWorld(), Page.Rows[SelectedRow]);
		RefreshData();
	}
}

void UFormsScreenWidget::PressBaseBody()
{
	Message = WoRFormsScreen::SwitchBaseBody(GetWorld());
	RefreshData();
}

FReply UFormsScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveSelection(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsConfirmKey(Key) || IsSecondActionKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			IsConfirmKey(Key) ? PressWear() : PressBaseBody();
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

int32 UFormsScreenWidget::FindRowUnder(const FPointerEvent &MouseEvent) const
{
	// A row scrolled out of view keeps its geometry past the list's edge: only the list's own area counts.
	if (!RowList || !RowList->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < RowWidgets.Num() && Index < Page.Rows.Num(); ++Index)
	{
		const UBorder *Frame = RowWidgets[Index].Frame.Get();
		if (Frame && Frame->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FReply UFormsScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks a row, so a resting cursor doesn't fight the keys.
	if (!InMouseEvent.GetCursorDelta().IsZero())
	{
		SelectRow(FindRowUnder(InMouseEvent));
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UFormsScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	const int32 Row = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? FindRowUnder(InMouseEvent) : INDEX_NONE;
	if (Row == INDEX_NONE)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	SelectRow(Row);
	PressWear(); // a click is A on that row (InputMap: Select)
	return FReply::Handled();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GFormsScreenOpenCommand(
	TEXT("wor.UI.Forms"),
	TEXT("B-444: open the Spirit of {character} screen (forms and meters; A wear, Y base body)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UFormsScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[FormsScreen] wor.UI.Forms: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
