// BattleHUDCard.cpp
// B-503k: a party card in a fight keeps the gear tooltip on hover but no gear line (Crown: no "Generic Primary Weapon").
// B-503m: the HP and EP numbers are drawn on their bars (no separate lines); B-585 (D-257): the build-up bar's too.

#include "UI/Combat/Battle/BattleHUDCard.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
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

	/** A bar's number, centred over what Layers already holds. */
	UTextBlock *AddBarNumber(UWidgetTree *Tree, UOverlay *Layers)
	{
		using namespace BattleCardStyle;

		UTextBlock *Number = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Number->GetFont();
		Font.Size = BarFontSize;
		Font.OutlineSettings.OutlineSize = BarTextOutlineSize; // B-436b: readable on the fill and on the empty part
		Font.OutlineSettings.OutlineColor = BarTextOutlineColor;
		Number->SetFont(Font);
		Number->SetColorAndOpacity(FSlateColor(TextColor));
		if (UOverlaySlot *TextSlot = Layers->AddChildToOverlay(Number))
		{
			TextSlot->SetHorizontalAlignment(HAlign_Center);
			TextSlot->SetVerticalAlignment(VAlign_Center);
		}
		return Number;
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
		OutText = AddBarNumber(Tree, Layers);
		if (UVerticalBoxSlot *RowSlot = Rows->AddChildToVerticalBox(BarSize))
		{
			RowSlot->SetPadding(FMargin(RowPadding));
		}
		return BarSize;
	}

	/** B-592b: the HP-cost ghost, laid over the card's HP bar under its number: a lead spacer, the chunk, a tail
	 *  spacer, weighted by Push so the chunk ends where the fill ends. Hidden until a cost shows. */
	void BuildHPGhost(UWidgetTree *Tree, FBattleHUDCard &Card)
	{
		UOverlay *Layers = Card.HPBar ? Cast<UOverlay>(Card.HPBar->GetParent()) : nullptr;
		if (!Layers)
		{
			return;
		}
		Card.HPGhostRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Card.HPGhostRow->SetVisibility(ESlateVisibility::Collapsed);
		Card.HPGhost = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		UWidget *const Parts[] = {Tree->ConstructWidget<USpacer>(USpacer::StaticClass()), Card.HPGhost.Get(),
								  Tree->ConstructWidget<USpacer>(USpacer::StaticClass())};
		for (UWidget *Part : Parts)
		{
			if (UHorizontalBoxSlot *PartSlot = Card.HPGhostRow->AddChildToHorizontalBox(Part))
			{
				PartSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				PartSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
		if (UOverlaySlot *RowSlot = Cast<UOverlaySlot>(Layers->InsertChildAt(1, Card.HPGhostRow))) // over the bar, under the number
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	/** B-592b: weights the ghost row so the chunk is the last HPGhostFraction of the fill, in its colour. */
	void PushHPGhost(const FBattleHUDCard &Card)
	{
		using namespace BattleCardStyle;

		if (!Card.HPGhostRow || !Card.HPGhost)
		{
			return;
		}
		const float Fill = FMath::Clamp(Card.HPFraction, 0.0f, 1.0f);
		const float Ghost = FMath::Clamp(Card.HPGhostFraction, 0.0f, Fill); // a lethal cost covers what is left
		Card.HPGhostRow->SetVisibility(Ghost > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		const float Weights[] = {Fill - Ghost, Ghost, 1.0f - Fill};
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Weights)); ++Index)
		{
			const UWidget *Part = Card.HPGhostRow->GetChildAt(Index);
			if (UHorizontalBoxSlot *PartSlot = Part ? Cast<UHorizontalBoxSlot>(Part->Slot) : nullptr)
			{
				FSlateChildSize Size(ESlateSizeRule::Fill);
				Size.Value = Weights[Index];
				PartSlot->SetSize(Size);
			}
		}
		Card.HPGhost->SetBrushColor(Card.bHPGhostKills ? HPGhostKillColor : HPGhostColor);
	}

	/** The buildup bar, with a row of crack marks laid over it: spacers weighted by the gaps between
	 *  BattleHUDView::GetCrackPositions, a thin mark at each. Marks are Hidden (not Collapsed) so the row keeps its spacing.
	 *  B-585 (D-257): as tall as the card's HP bar, with no label; with bNumber its number on it, over the cracks. */
	void BuildBuildupBar(UWidgetTree *Tree, FBattleHUDCard &Card, UVerticalBox *Rows, bool bNumber)
	{
		using namespace BattleCardStyle;

		USizeBox *BarSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarSize->SetHeightOverride(bNumber ? NumberBarHeight : BarHeight);
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
		if (bNumber)
		{
			Card.BuildupText = AddBarNumber(Tree, BarLayers);
		}

		if (UVerticalBoxSlot *BuildupSlot = Rows->AddChildToVerticalBox(BarSize))
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
	Parts.bBuildupLine = Parts.bGear = Level == EBattleCardLevel::Battle && bParty;
	Parts.bActions = false; // B-436g (D-260): the pips moved to the command menu's tab; OBSOLETE with ActionsText, kept until PIE (docs/PIE/Sweep.md)
	Parts.bStatus = Level == EBattleCardLevel::Battle && !bParty; // D-261 (B-586): a party member's effects are in Intel
	// B-436c drew the target marker's tier here; B-582c (Crown, 2026-11-23): no tier line on the floating card. TierLine
	// stays on the card data for wor.UI.Print.
	Parts.bTier = false;
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
	// B-582c QA: the fill is the brush's own (Push swaps it), not SetBrushColor, which would also tint the acting frame.
	Card.Frame->SetBrush(FSlateColorBrush(CardColor));
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
	Card.NameText->SetJustification(ETextJustify::Center); // B-582c (Crown): names centred on every card
	Card.DetailText = AddText(Tree, Rows, LineFontSize); // D-229: the kind name ("Fire Refractor")

	// B-503m: the numbers sit on the bars (HP on the HP bar, EP on its own bar); an enemy's HP bar has no number (Q-391).
	if (Parts.bHPBar && Parts.bHPLine)
	{
		BuildNumberBar(Tree, Rows, PartyHPColor, Card.HPBar, Card.HPText);
		BuildHPGhost(Tree, Card);
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
		BuildBuildupBar(Tree, Card, Rows, Parts.bBuildupLine);
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
	if (Parts.bTier)
	{
		Card.TierText = AddText(Tree, Rows, LineFontSize);
		Card.TierText->SetJustification(ETextJustify::Center);
	}
	return Card.Frame;
}

void BattleHUDCard::Push(const FBattleHUDCard &Card)
{
	using namespace BattleCardStyle;

	SetTextSafe(Card.NameText, Card.Name);
	if (Card.Frame) // B-582c (Crown): the acting party card's frame, in its selector colour, says whose turn it is
	{
		if (Card.bActing && Card.bParty)
		{
			Card.Frame->SetBrush(FSlateRoundedBoxBrush(CardColor, 0.0f, Card.ActingOutline, ActingFrameWidth));
		}
		else
		{
			Card.Frame->SetBrush(FSlateColorBrush(CardColor));
		}
	}
	SetTextSafe(Card.DetailText, Card.DetailLine);
	if (Card.DetailText)
	{
		Card.DetailText->SetVisibility(Card.DetailLine.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Card.HPBar)
	{
		Card.HPBar->SetPercent(Card.HPFraction);
		if (Card.bParty) // B-436b
		{
			Card.HPBar->SetFillColorAndOpacity(Card.bLowHP ? LowHPColor : PartyHPColor);
		}
	}
	if (!Card.bLowHP)
	{
		SetHPPulse(Card, 1.0f); // a healed card stops dimming at once
	}
	SetTextSafe(Card.HPText, Card.HPLine);
	PushHPGhost(Card);
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
	if (Card.StatusText) // B-582c QA: no effects, no empty row under the bars
	{
		Card.StatusText->SetVisibility(Card.StatusLine.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	SetTextSafe(Card.TierText, Card.TierLine);
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
	ShowIf(Card.ActionsText, !bCompact);
	ShowIf(Card.StatusText, !bCompact && !Card.StatusLine.IsEmpty());
}

void BattleHUDCard::SetHPPulse(const FBattleHUDCard &Card, float Opacity)
{
	if (Card.HPBar)
	{
		Card.HPBar->SetRenderOpacity(Card.bLowHP ? Opacity : 1.0f);
	}
	if (Card.HPGhost)
	{
		Card.HPGhost->SetRenderOpacity(Card.HPGhostFraction > 0.0f ? Opacity : 1.0f);
	}
}

#undef LOCTEXT_NAMESPACE
