// BattleHUDCard.cpp
// B-503k: a party card in a fight keeps the gear tooltip on hover but no gear line (Crown: no "Generic Primary Weapon").
// B-503m: the HP and EP numbers are drawn on their bars (no separate lines); the build-up bar has a quiet label.

#include "UI/Combat/Battle/BattleHUDCard.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Combat/Battle/BattleHUDView.h"

#define LOCTEXT_NAMESPACE "BattleHUD" // the card's texts kept their keys when it left the battle HUD

namespace
{
	void SetTextSafe(UTextBlock *Text, const FText &Value)
	{
		if (Text)
		{
			Text->SetText(Value);
		}
	}

	/** A card bar; B-503m: its empty part in a quiet tint, so an empty bar reads as part of the card. */
	UProgressBar *MakeBar(UWidgetTree *Tree, const FLinearColor &Fill)
	{
		UProgressBar *Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		Bar->SetFillColorAndOpacity(Fill);
		FProgressBarStyle Style = Bar->GetWidgetStyle();
		Style.BackgroundImage.TintColor = FSlateColor(BattleCardStyle::EmptyBarColor);
		Bar->SetWidgetStyle(Style);
		return Bar;
	}

	/** B-503m: a pool's bar with its number drawn on it ("HP 129 / 129"); returns the row, to show or hide. */
	USizeBox *BuildNumberBar(UWidgetTree *Tree, UVerticalBox *Rows, const FLinearColor &Fill, TObjectPtr<UProgressBar> &OutBar,
							 TObjectPtr<UTextBlock> &OutText)
	{
		using namespace BattleCardStyle;

		USizeBox *BarSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarSize->SetHeightOverride(NumberBarHeight);
		UOverlay *Layers = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		BarSize->AddChild(Layers);
		OutBar = MakeBar(Tree, Fill);
		if (UOverlaySlot *BarSlot = Layers->AddChildToOverlay(OutBar))
		{
			BarSlot->SetHorizontalAlignment(HAlign_Fill);
			BarSlot->SetVerticalAlignment(VAlign_Fill);
		}
		OutText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = OutText->GetFont();
		Font.Size = BarFontSize;
		OutText->SetFont(Font);
		OutText->SetColorAndOpacity(FSlateColor(TextColor));
		OutText->SetShadowOffset(FVector2D(1.0f, 1.0f)); // readable on the fill and on the empty part
		OutText->SetShadowColorAndOpacity(BarTextShadowColor);
		if (UOverlaySlot *TextSlot = Layers->AddChildToOverlay(OutText))
		{
			TextSlot->SetHorizontalAlignment(HAlign_Center);
			TextSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot *RowSlot = Rows->AddChildToVerticalBox(BarSize))
		{
			RowSlot->SetPadding(FMargin(RowPadding));
		}
		return BarSize;
	}

	/** The buildup bar, with a row of crack marks laid over it: spacers weighted by the gaps between
	 *  BattleHUDView::GetCrackPositions, a thin mark at each. Marks are Hidden (not Collapsed) so the row keeps its spacing.
	 *  B-503m: a quiet "Status" label before it, so the empty bar isn't a mystery. */
	void BuildBuildupBar(UWidgetTree *Tree, FBattleHUDCard &Card, UVerticalBox *Rows)
	{
		using namespace BattleCardStyle;

		UHorizontalBox *Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo LabelFont = Label->GetFont();
		LabelFont.Size = BuildupLabelFontSize;
		Label->SetFont(LabelFont);
		Label->SetColorAndOpacity(FSlateColor(BuildupLabelColor));
		Label->SetText(LOCTEXT("BuildupBarLabel", "Status"));
		if (UHorizontalBoxSlot *LabelSlot = Row->AddChildToHorizontalBox(Label))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			LabelSlot->SetPadding(FMargin(0.0f, 0.0f, BuildupLabelGap, 0.0f));
		}

		USizeBox *BarSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarSize->SetHeightOverride(BuildupBarHeight);
		if (UHorizontalBoxSlot *BarRowSlot = Row->AddChildToHorizontalBox(BarSize))
		{
			BarRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BarRowSlot->SetVerticalAlignment(VAlign_Center);
		}
		UOverlay *BarLayers = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		BarSize->AddChild(BarLayers);

		Card.BuildupBar = MakeBar(Tree, Card.BuildupColour);
		if (UOverlaySlot *BarSlot = BarLayers->AddChildToOverlay(Card.BuildupBar))
		{
			BarSlot->SetHorizontalAlignment(HAlign_Fill);
			BarSlot->SetVerticalAlignment(VAlign_Fill);
		}
		UHorizontalBox *CrackRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		CrackRow->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot *CrackRowSlot = BarLayers->AddChildToOverlay(CrackRow))
		{
			CrackRowSlot->SetHorizontalAlignment(HAlign_Fill);
			CrackRowSlot->SetVerticalAlignment(VAlign_Fill);
		}

		const auto AddGap = [Tree, CrackRow](float Weight)
		{
			USpacer *Gap = Tree->ConstructWidget<USpacer>(USpacer::StaticClass());
			if (UHorizontalBoxSlot *GapSlot = CrackRow->AddChildToHorizontalBox(Gap))
			{
				FSlateChildSize GapSize(ESlateSizeRule::Fill);
				GapSize.Value = Weight;
				GapSlot->SetSize(GapSize);
			}
		};
		float PreviousPosition = 0.0f;
		for (const float Position : BattleHUDView::GetCrackPositions())
		{
			AddGap(Position - PreviousPosition);
			USizeBox *MarkSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			MarkSize->SetWidthOverride(CrackWidth);
			UBorder *Mark = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Mark->SetBrushColor(CrackColor);
			MarkSize->AddChild(Mark);
			MarkSize->SetVisibility(ESlateVisibility::Hidden);
			if (UHorizontalBoxSlot *MarkSlot = CrackRow->AddChildToHorizontalBox(MarkSize))
			{
				MarkSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				MarkSlot->SetVerticalAlignment(VAlign_Fill);
			}
			Card.CrackMarks.Add(MarkSize);
			PreviousPosition = Position;
		}
		AddGap(1.0f - PreviousPosition);

		if (UVerticalBoxSlot *BuildupSlot = Rows->AddChildToVerticalBox(Row))
		{
			BuildupSlot->SetPadding(FMargin(RowPadding));
		}
	}
}

FBattleCardParts BattleHUDCard::GetParts(EBattleCardLevel Level, bool bParty)
{
	FBattleCardParts Parts;
	Parts.bHPBar = Level != EBattleCardLevel::Hub;
	Parts.bBuildupBar = Level == EBattleCardLevel::Battle; // D-216: the trial's bar is always empty after a fight (SB-18)
	Parts.bHPLine = Parts.bEPLine = Level != EBattleCardLevel::Hub && bParty;
	Parts.bBuildupLine = Parts.bGear = Parts.bActions = Level == EBattleCardLevel::Battle && bParty;
	Parts.bStatus = Level == EBattleCardLevel::Battle;
	Parts.bFixedWidth = Level != EBattleCardLevel::Hub; // the hub's name-only card shrinks to the name
	return Parts;
}

UTextBlock *BattleHUDCard::AddText(UWidgetTree *Tree, UVerticalBox *Box, int32 FontSize)
{
	UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(BattleCardStyle::TextColor));
	Text->SetAutoWrapText(true);
	if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
	{
		BoxSlot->SetPadding(FMargin(BattleCardStyle::RowPadding));
	}
	return Text;
}

UBorder *BattleHUDCard::Build(UWidgetTree *Tree, FBattleHUDCard &Card, EBattleCardLevel Level)
{
	using namespace BattleCardStyle;

	const FBattleCardParts Parts = GetParts(Level, Card.bParty);
	Card.Frame = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card.Frame->SetBrushColor(CardColor);
	Card.Frame->SetPadding(FMargin(CardPadding));
	// B-391c: a card with gear is hit-testable so its gear tooltip shows on hover; the others let clicks through.
	Card.Frame->SetVisibility(Parts.bGear ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
	if (Parts.bGear)
	{
		Card.Frame->SetToolTipText(Card.GearTooltip); // the battle HUD's RefreshGear ran before the widgets existed
	}
	USizeBox *Width = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (Parts.bFixedWidth)
	{
		Width->SetWidthOverride(CardWidth);
	}
	Card.WidthBox = Width;
	Card.Frame->SetContent(Width);
	UVerticalBox *Rows = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Width->AddChild(Rows);

	Card.NameText = AddText(Tree, Rows, NameFontSize);
	Card.NameText->SetAutoWrapText(Parts.bFixedWidth); // wrapping needs a width: a fitted card keeps the name on one line
	Card.DetailText = AddText(Tree, Rows, LineFontSize); // D-229: the kind name ("Fire Refractor")

	// B-503m: the numbers sit on the bars (HP on the HP bar, EP on its own bar); an enemy's HP bar has no number (Q-391).
	if (Parts.bHPBar && Parts.bHPLine)
	{
		BuildNumberBar(Tree, Rows, PartyHPColor, Card.HPBar, Card.HPText);
	}
	else if (Parts.bHPBar)
	{
		USizeBox *BarSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarSize->SetHeightOverride(BarHeight);
		Card.HPBar = MakeBar(Tree, Card.bParty ? PartyHPColor : EnemyHPColor);
		BarSize->AddChild(Card.HPBar);
		if (UVerticalBoxSlot *BarSlot = Rows->AddChildToVerticalBox(BarSize))
		{
			BarSlot->SetPadding(FMargin(RowPadding));
		}
	}
	if (Parts.bEPLine)
	{
		Card.EPRow = BuildNumberBar(Tree, Rows, EPBarColor, Card.EPBar, Card.EPText);
		Card.EPRow->SetVisibility(Card.bShowEP ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Parts.bBuildupBar)
	{
		BuildBuildupBar(Tree, Card, Rows);
	}
	if (Parts.bBuildupLine)
	{
		Card.BuildupText = AddText(Tree, Rows, LineFontSize);
	}
	if (Parts.bActions)
	{
		Card.ActionsText = AddText(Tree, Rows, LineFontSize);	   // B-170e
		Card.ActionsText->SetVisibility(ESlateVisibility::HitTestInvisible); // B-170h2: every fight banks actions
	}
	if (Parts.bStatus)
	{
		Card.StatusText = AddText(Tree, Rows, LineFontSize);
	}
	return Card.Frame;
}

void BattleHUDCard::Push(const FBattleHUDCard &Card)
{
	using namespace BattleCardStyle;

	const FText Name = Card.bActing ? FText::Format(LOCTEXT("ActingName", "> {0}"), Card.Name) : Card.Name;
	SetTextSafe(Card.NameText, Name);
	if (Card.NameText)
	{
		Card.NameText->SetColorAndOpacity(FSlateColor(Card.bActing ? ActingColor : TextColor));
	}
	SetTextSafe(Card.DetailText, Card.DetailLine);
	if (Card.DetailText)
	{
		Card.DetailText->SetVisibility(Card.DetailLine.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Card.HPBar)
	{
		Card.HPBar->SetPercent(Card.HPFraction);
	}
	SetTextSafe(Card.HPText, Card.HPLine);
	if (Card.EPBar)
	{
		Card.EPBar->SetPercent(Card.EPFraction);
	}
	SetTextSafe(Card.EPText, Card.EPLine);
	if (Card.EPText)
	{
		Card.EPText->SetColorAndOpacity(FSlateColor(Card.EPColour));
	}
	SetTextSafe(Card.ActionsText, Card.ActionsLine);
	SetTextSafe(Card.StatusText, Card.StatusLine);
	if (Card.Frame)
	{
		Card.Frame->SetRenderOpacity(Card.bAlive ? 1.0f : DownedCardOpacity);
	}
	if (Card.BuildupBar)
	{
		Card.BuildupBar->SetPercent(Card.BuildupFraction);
		Card.BuildupBar->SetFillColorAndOpacity(Card.BuildupColour);
	}
	for (int32 Index = 0; Index < Card.CrackMarks.Num(); ++Index)
	{
		if (UWidget *Mark = Card.CrackMarks[Index])
		{
			Mark->SetVisibility(Index < Card.CracksShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
	}
	SetTextSafe(Card.BuildupText, Card.BuildupLine);
}

void BattleHUDCard::SetCompact(const FBattleHUDCard &Card, bool bCompact)
{
	using namespace BattleCardStyle;

	const auto ShowIf = [](UWidget *Widget, bool bShow)
	{
		if (Widget)
		{
			Widget->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	};
	if (Card.WidthBox)
	{
		Card.WidthBox->SetWidthOverride(bCompact ? CompactCardWidth : CardWidth);
	}
	ShowIf(Card.DetailText, !bCompact && !Card.DetailLine.IsEmpty());
	ShowIf(Card.EPRow, !bCompact && Card.bShowEP);
	ShowIf(Card.BuildupText, !bCompact);
	ShowIf(Card.ActionsText, !bCompact);
	ShowIf(Card.StatusText, !bCompact);
}

#undef LOCTEXT_NAMESPACE
