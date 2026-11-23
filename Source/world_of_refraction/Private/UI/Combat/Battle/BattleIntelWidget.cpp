// BattleIntelWidget.cpp

#include "UI/Combat/Battle/BattleIntelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRKeyHintText.h"

#define LOCTEXT_NAMESPACE "BattleIntel"

namespace BattleIntelStyle
{
	using namespace WoRMenuStyle;
	// TODO: style pass (placeholder until the CommonUI screens, D-024). Slate units at 1080p, DPI-scaled.
	/** B-587 (D-262 a): the panel's content width. With its padding and the HUD's 24 screen margin the panel fills the
	 *  right third of 1920 (640), leaving the left two-thirds to the close-up. Was 960, centred (B-586). */
	constexpr float PanelWidth = 580.0f;
	constexpr float PanelPadding = 18.0f;
	constexpr float RowGap = 2.0f;
	constexpr float LabelIndent = 4.0f;
	constexpr float ColumnGap = 30.0f;
	constexpr float TurnsWidth = 140.0f;
	constexpr float BarGap = 4.0f;
	inline const FLinearColor PartyTabColor = FLinearColor::FromSRGBColor(FColor(38, 34, 56));
	inline const FLinearColor EnemyTabColor = FLinearColor::FromSRGBColor(FColor(64, 30, 34));
	inline const FLinearColor EnemyTabTextColor = FLinearColor::FromSRGBColor(FColor(240, 150, 140));
	inline const FLinearColor WorseColor = FLinearColor::FromSRGBColor(FColor(240, 110, 100));
	inline const FLinearColor BuildupColor = FLinearColor::FromSRGBColor(FColor(230, 70, 60));

	FLinearColor GetToneColor(EBattleIntelTone Tone)
	{
		switch (Tone)
		{
		case EBattleIntelTone::Better:
			return GoodColor;
		case EBattleIntelTone::Worse:
			return WorseColor;
		case EBattleIntelTone::Unknown:
			return OffColor;
		default:
			return TextColor;
		}
	}
}

namespace
{
	USizeBox *MakeBarRow(UWidgetTree *Tree, const FLinearColor &Fill, UProgressBar *&OutBar, UTextBlock *&OutValue)
	{
		USizeBox *Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetHeightOverride(WoRMenuStyle::BarWithValueHeight);
		Box->AddChild(WoRMenuStyle::MakeBarWithValue(Tree, Fill, OutBar, OutValue));
		return Box;
	}

	void SetTextShown(UTextBlock *Text, const FText &Value)
	{
		if (Text)
		{
			Text->SetText(Value);
			WoRMenuStyle::SetShown(Text, !Value.IsEmpty());
		}
	}
}

// ========================================
// LAYOUT (built once)
// ========================================

TSharedRef<SWidget> UBattleIntelWidget::RebuildWidget()
{
	using namespace BattleIntelStyle;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("IntelPanel"));
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(PanelPadding));
		WidgetTree->RootWidget = Panel;
		UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(MakeSized(WidgetTree, PanelWidth, 0.0f, Column));

		UHorizontalBox *TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (int32 Index = 0; Index < MAX_TABS; ++Index)
		{
			UTextBlock *Label = nullptr;
			UBorder *Tab = MakeTab(WidgetTree, Label);
			Label->SetJustification(ETextJustify::Center);
			// B-587: the narrower panel shrinks a long name to its tab instead of pushing the row wider.
			UScaleBox *Fit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
			Fit->SetStretch(EStretch::ScaleToFit);
			Fit->SetStretchDirection(EStretchDirection::DownOnly);
			Fit->AddChild(Label);
			Tab->SetContent(Fit);
			Tab->OnMouseButtonDownEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UBattleIntelWidget, HandleTabMouseDown));
			AddToRow(TabRow, Tab, FMargin(0.0f, 0.0f, TabGap, 0.0f))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Tabs.Add(Tab);
			TabLabels.Add(Label);
		}
		AddToColumn(Column, TabRow, FMargin(0.0f, 0.0f, 0.0f, SectionGap));

		Heading = MakeText(WidgetTree, NameFontSize, TextColor, true);
		AddToColumn(Column, Heading);
		Subline = MakeText(WidgetTree, SmallFontSize, DimColor);
		AddToColumn(Column, Subline);
		ActionsLine = MakeText(WidgetTree, SmallFontSize, DimColor);
		AddToColumn(Column, ActionsLine, FMargin(0.0f, 0.0f, 0.0f, LineGap));

		UProgressBar *Bar = nullptr;
		UTextBlock *Value = nullptr;
		AddToColumn(Column, MakeBarRow(WidgetTree, HPColor, Bar, Value), FMargin(0.0f, BarGap));
		HPBar = Bar;
		HPValue = Value;
		EPRow = MakeBarRow(WidgetTree, EPColor, Bar, Value);
		AddToColumn(Column, EPRow, FMargin(0.0f, BarGap));
		EPBar = Bar;
		EPValue = Value;
		AddToColumn(Column, MakeBarRow(WidgetTree, BuildupColor, Bar, Value), FMargin(0.0f, BarGap));
		BuildupBar = Bar;
		BuildupValue = Value;

		UHorizontalBox *PillarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddSection(Column, LOCTEXT("Pillars", "Pillars"), PillarsSection)->AddChildToVerticalBox(PillarRow);
		AddRows(PillarRow, PILLAR_COUNT, /*bTurnsColumn=*/false, PillarRows);

		UHorizontalBox *StatColumns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddSection(Column, LOCTEXT("Stats", "Stats"), StatsSection)->AddChildToVerticalBox(StatColumns);
		UVerticalBox *Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UVerticalBox *Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddToRow(StatColumns, Left, FMargin(0.0f, 0.0f, ColumnGap, 0.0f), VAlign_Top)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AddToRow(StatColumns, Right, FMargin(0.0f), VAlign_Top)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AddRows(Left, STAT_ROWS_PER_COLUMN, false, StatRowsLeft);
		AddRows(Right, STAT_ROWS_PER_COLUMN, false, StatRowsRight);

		AddRows(AddSection(Column, LOCTEXT("WeakTo", "Weak to"), WeakSection), MAX_ELEMENT_ROWS, false, WeakRows);
		AddRows(AddSection(Column, LOCTEXT("Resists", "Resists"), ResistSection), MAX_ELEMENT_ROWS, false, ResistRows);
		AddRows(AddSection(Column, LOCTEXT("Effects", "Effects"), EffectsSection), MAX_EFFECT_ROWS, /*bTurnsColumn=*/true, EffectRows);

		UTextBlock *Footer = MakeHintText(WidgetTree, SmallFontSize, DimColor);
		WoRKeyHints::SetHint(Footer, LOCTEXT("Footer", "{Tabs}  next tab        {Details}  close"));
		AddToColumn(Column, Footer, FMargin(0.0f, SectionGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Right);
	}
	return Super::RebuildWidget();
}

UVerticalBox *UBattleIntelWidget::AddSection(UVerticalBox *Parent, const FText &Title, TObjectPtr<UWidget> &OutSection)
{
	using namespace BattleIntelStyle;
	UVerticalBox *Section = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *TitleText = MakeText(WidgetTree, LineFontSize, CyanColor, true);
	TitleText->SetText(Title);
	AddToColumn(Section, TitleText, FMargin(0.0f, 0.0f, 0.0f, RowGap));
	AddToColumn(Parent, Section, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	OutSection = Section;
	return Section;
}

void UBattleIntelWidget::AddRows(UPanelWidget *Parent, int32 Count, bool bTurnsColumn, FBattleIntelRowPool &OutPool)
{
	using namespace BattleIntelStyle;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FBattleIntelRowWidgets Widgets;
		UHorizontalBox *Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Widgets.Root = Row;
		Widgets.Label = MakeText(WidgetTree, LineFontSize, TextColor);
		AddToRow(Row, Widgets.Label, FMargin(LabelIndent, RowGap, WordGap, RowGap))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Widgets.Before = MakeText(WidgetTree, SmallFontSize, DimColor);
		AddToRow(Row, Widgets.Before);
		Widgets.Arrow = MakeText(WidgetTree, SmallFontSize, DimColor);
		Widgets.Arrow->SetText(FText::AsCultureInvariant(TEXT(" → ")));
		AddToRow(Row, Widgets.Arrow);
		Widgets.Now = MakeText(WidgetTree, LineFontSize, TextColor, true);
		AddToRow(Row, Widgets.Now, FMargin(0.0f, 0.0f, WordGap, 0.0f));
		if (bTurnsColumn)
		{
			Widgets.Turns = MakeText(WidgetTree, SmallFontSize, DimColor);
			Widgets.Turns->SetJustification(ETextJustify::Right);
			AddToRow(Row, MakeSized(WidgetTree, TurnsWidth, 0.0f, Widgets.Turns));
		}
		if (UHorizontalBox *Across = Cast<UHorizontalBox>(Parent))
		{
			AddToRow(Across, Row, FMargin(0.0f, 0.0f, ColumnGap, 0.0f))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		else if (UVerticalBox *Down = Cast<UVerticalBox>(Parent))
		{
			AddToColumn(Down, Row);
		}
		OutPool.Rows.Add(Widgets);
	}
}

// ========================================
// PUSH (texts and visibility only)
// ========================================

void UBattleIntelWidget::PushRows(const FBattleIntelRowPool &Pool, const TArray<FBattleIntelRow> &Rows)
{
	using namespace BattleIntelStyle;
	for (int32 Index = 0; Index < Pool.Rows.Num(); ++Index)
	{
		const FBattleIntelRowWidgets &Widgets = Pool.Rows[Index];
		const bool bUsed = Rows.IsValidIndex(Index);
		SetShown(Widgets.Root, bUsed);
		if (!bUsed || !Widgets.Label || !Widgets.Now)
		{
			continue;
		}
		const FBattleIntelRow &Row = Rows[Index];
		const FLinearColor ToneColor = GetToneColor(Row.Tone);
		const bool bChanged = !Row.Before.IsEmpty();
		Widgets.Label->SetText(Row.Label);
		// A stat an effect moved is coloured on its name too, as the picture; a "?" line is greyed whole.
		Widgets.Label->SetColorAndOpacity(FSlateColor(bChanged || Row.Tone == EBattleIntelTone::Unknown ? ToneColor : TextColor));
		SetTextShown(Widgets.Before, Row.Before);
		SetShown(Widgets.Arrow, bChanged);
		Widgets.Now->SetText(Row.Now);
		Widgets.Now->SetColorAndOpacity(FSlateColor(ToneColor));
		if (Widgets.Turns)
		{
			Widgets.Turns->SetText(Row.Turns);
		}
	}
}

void UBattleIntelWidget::Push(const TArray<FBattleIntelPage> &Pages, int32 Selected)
{
	using namespace BattleIntelStyle;
	if (!Heading)
	{
		return; // not built yet
	}
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		const bool bUsed = Pages.IsValidIndex(Index);
		Tabs[Index]->SetVisibility(bUsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bUsed)
		{
			continue;
		}
		const bool bSelected = Index == Selected;
		const bool bEnemy = Pages[Index].bEnemy;
		Tabs[Index]->SetBrushColor(bSelected ? FocusColor : bEnemy ? EnemyTabColor : PartyTabColor);
		TabLabels[Index]->SetText(Pages[Index].Tab);
		TabLabels[Index]->SetColorAndOpacity(FSlateColor(bSelected ? TextColor : bEnemy ? EnemyTabTextColor : DimColor));
	}
	if (!Pages.IsValidIndex(Selected))
	{
		return;
	}
	const FBattleIntelPage &Page = Pages[Selected];
	Heading->SetText(Page.Heading);
	SetTextShown(Subline, Page.Subline);
	SetTextShown(ActionsLine, Page.ActionsLine);
	HPBar->SetPercent(Page.HPFraction);
	HPValue->SetText(Page.HPLine);
	SetShown(EPRow, Page.bShowEP);
	EPBar->SetPercent(Page.EPFraction);
	EPValue->SetText(Page.EPLine);
	BuildupBar->SetPercent(Page.BuildupFraction);
	BuildupValue->SetText(Page.BuildupLine);

	SetShown(PillarsSection, !Page.bEnemy);
	SetShown(StatsSection, !Page.bEnemy);
	SetShown(WeakSection, Page.bEnemy);
	SetShown(ResistSection, Page.bEnemy);
	PushRows(PillarRows, Page.Pillars);
	const int32 LeftCount = FMath::Min(Page.Stats.Num(), STAT_ROWS_PER_COLUMN);
	PushRows(StatRowsLeft, TArray<FBattleIntelRow>(Page.Stats.GetData(), LeftCount));
	PushRows(StatRowsRight, TArray<FBattleIntelRow>(Page.Stats.GetData() + LeftCount, Page.Stats.Num() - LeftCount));
	PushRows(WeakRows, Page.WeakTo);
	PushRows(ResistRows, Page.Resists);

	TArray<FBattleIntelRow> Effects = Page.Effects;
	if (Effects.IsEmpty())
	{
		Effects.AddDefaulted_GetRef().Label = LOCTEXT("NoEffects", "None");
	}
	else if (Effects.Num() > MAX_EFFECT_ROWS)
	{
		const int32 Hidden = Effects.Num() - (MAX_EFFECT_ROWS - 1);
		Effects.SetNum(MAX_EFFECT_ROWS - 1);
		Effects.AddDefaulted_GetRef().Label = FText::Format(LOCTEXT("MoreEffects", "+{0} more"), FText::AsNumber(Hidden));
	}
	PushRows(EffectRows, Effects);
}

FEventReply UBattleIntelWidget::HandleTabMouseDown(FGeometry MyGeometry, const FPointerEvent &MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FEventReply(false);
	}
	// One handler for every tab: the tab under the pointer is the one clicked.
	const int32 Clicked = Tabs.IndexOfByPredicate([&MouseEvent](const TObjectPtr<UBorder> &Tab)
	{
		return Tab && Tab->IsVisible() && Tab->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition());
	});
	if (Clicked == INDEX_NONE)
	{
		return FEventReply(false);
	}
	OnTabClicked.ExecuteIfBound(Clicked);
	return FEventReply(true);
}

#undef LOCTEXT_NAMESPACE
