// MenuScreenStyle.h
// B-439 / B-440a: the look and the small builders every C++-built menu screen shares (the game menu, the loadout
// screen), so the screens read as one set. The mockup kit's tokens (screen_mockups/_kit/wor_ui.py) at 1080p:
// Slate units, DPI-scaled.
// TODO: the Hidris art pass replaces the flat colours and fonts (UI_Theme.md).

#pragma once

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UI/Menus/MenuChipRow.h"
#include "UI/WoRKeyHintText.h"

namespace WoRMenuStyle
{
	constexpr float ScreenPadding = 72.0f;
	constexpr float LineGap = 9.0f;
	constexpr float SectionGap = 18.0f;
	constexpr float WordGap = 9.0f;
	constexpr float TabPaddingX = 21.0f;
	constexpr float TabPaddingY = 9.0f;
	constexpr float TabGap = 9.0f;
	constexpr float BarWithValueHeight = 30.0f;

	constexpr int32 TitleFontSize = 38;
	constexpr int32 SubtitleFontSize = 20;
	constexpr int32 RowFontSize = 22;
	constexpr int32 NameFontSize = 29;
	constexpr int32 LineFontSize = 19;
	constexpr int32 SmallFontSize = 17;

	inline const FLinearColor ScreenColor = FLinearColor::FromSRGBColor(FColor(16, 18, 25, 240));
	/** The front screens (title, credits): nothing of the level shows through.
	 *  TODO: the nebula and the hexagon vortex replace the flat colour (D-202, B-467b). */
	inline const FLinearColor FrontBackdropColor = FLinearColor::FromSRGBColor(FColor(11, 7, 20));
	inline const FLinearColor PanelColor = FLinearColor::FromSRGBColor(FColor(20, 22, 28));
	inline const FLinearColor PlateColor = FLinearColor::FromSRGBColor(FColor(46, 46, 46));
	inline const FLinearColor TrackColor = FLinearColor::FromSRGBColor(FColor(42, 45, 53));
	// D-201 (Crown): the accent is violet (#5B3A99, the focused row and tab) and violet light (#CDB8FF, key labels and
	// marks); was teal. CyanColor keeps its name so every screen reads it unchanged.
	inline const FLinearColor FocusColor = FLinearColor::FromSRGBColor(FColor(0x5B, 0x3A, 0x99));
	inline const FLinearColor CyanColor = FLinearColor::FromSRGBColor(FColor(0xCD, 0xB8, 0xFF));
	/** B-482: a chip or row under the mouse: between the plate and the focus violet. */
	inline const FLinearColor HoverColor = FLinearColor::FromSRGBColor(FColor(70, 62, 96));
	inline const FLinearColor TextColor = FLinearColor::FromSRGBColor(FColor(238, 241, 245));
	inline const FLinearColor DimColor = FLinearColor::FromSRGBColor(FColor(169, 175, 184));
	inline const FLinearColor OffColor = FLinearColor::FromSRGBColor(FColor(107, 112, 120));
	// D-194 (Crown, 2026-11-13): HP and EP bars are a darker red and a darker blue, so the numbers on them read.
	// TODO: take the exact shades from UI_Theme.md once the UI Designer sets them (the battle HUD uses the same).
	inline const FLinearColor HPColor = FLinearColor::FromSRGBColor(FColor(168, 36, 60));
	inline const FLinearColor LowHPColor = FLinearColor::FromSRGBColor(FColor(255, 143, 180));
	inline const FLinearColor EPColor = FLinearColor::FromSRGBColor(FColor(38, 110, 168));
	/** An HP bar that is low: a brighter red, still dark enough for the white value on it (D-197). */
	inline const FLinearColor LowHPBarColor = FLinearColor::FromSRGBColor(FColor(204, 52, 52));
	inline const FLinearColor WarnColor = FLinearColor::FromSRGBColor(FColor(242, 162, 58));
	inline const FLinearColor GoodColor = FLinearColor::FromSRGBColor(FColor(110, 200, 110));

	inline const FName BoldTypeface(TEXT("Bold"));

	// Menu keys (InputMap.md, Menus). The Menu button that opened a screen closes it too.
	inline bool IsUpKey(const FKey &Key)
	{
		return Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up;
	}
	inline bool IsDownKey(const FKey &Key)
	{
		return Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down;
	}
	inline bool IsConfirmKey(const FKey &Key)
	{
		return Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom;
	}
	inline bool IsBackKey(const FKey &Key)
	{
		return Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right;
	}
	inline bool IsLeftKey(const FKey &Key)
	{
		return Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left;
	}
	inline bool IsRightKey(const FKey &Key)
	{
		return Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right;
	}
	/** A screen's own switch (the inventory's "Whole stack"): X on both. */
	inline bool IsToggleKey(const FKey &Key) { return Key == EKeys::X || Key == EKeys::Gamepad_FaceButton_Left; }
	/** A screen's second action (UIScreens.md: R / Y). */
	inline bool IsSecondActionKey(const FKey &Key) { return Key == EKeys::R || Key == EKeys::Gamepad_FaceButton_Top; }
	/** Page tabs, the top row (UIScreens, shared menu controls): LB / RB, Q / E. */
	inline bool IsTabPrevKey(const FKey &Key) { return Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder; }
	inline bool IsTabNextKey(const FKey &Key) { return Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder; }
	/** Sub-tabs, e.g. a character (UIScreens, shared menu controls): LT / RT, Z / C. */
	inline bool IsSubTabPrevKey(const FKey &Key) { return Key == EKeys::Z || Key == EKeys::Gamepad_LeftTrigger; }
	inline bool IsSubTabNextKey(const FKey &Key) { return Key == EKeys::C || Key == EKeys::Gamepad_RightTrigger; }

	inline UTextBlock *MakeText(UWidgetTree *Tree, int32 FontSize, const FLinearColor &Color, bool bBold = false,
								TSubclassOf<UTextBlock> TextClass = UTextBlock::StaticClass())
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(TextClass);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		if (bBold)
		{
			Font.TypefaceFontName = BoldTypeface;
		}
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}

	/** B-481: a text for key hints (a footer, a prompt). Fill it with WoRKeyHints::SetHint and a pattern
	 *  ("{Confirm}  Select        {Back}  Back"): it shows the keys of the device in use and follows it live. */
	inline UTextBlock *MakeHintText(UWidgetTree *Tree, int32 FontSize, const FLinearColor &Color, bool bBold = false)
	{
		return MakeText(Tree, FontSize, Color, bBold, UWoRKeyHintText::StaticClass());
	}

	inline UTextBlock *MakeWrappedText(UWidgetTree *Tree, int32 FontSize, const FLinearColor &Color, bool bBold = false)
	{
		UTextBlock *Text = MakeText(Tree, FontSize, Color, bBold);
		Text->SetAutoWrapText(true);
		return Text;
	}

	inline UProgressBar *MakeBar(UWidgetTree *Tree, const FLinearColor &FillColor)
	{
		UProgressBar *Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		FProgressBarStyle Style = Bar->GetWidgetStyle();
		Style.BackgroundImage = FSlateColorBrush(TrackColor);
		Style.FillImage = FSlateColorBrush(FLinearColor::White);
		Bar->SetWidgetStyle(Style);
		Bar->SetFillColorAndOpacity(FillColor);
		return Bar;
	}

	/** Content in a box of a fixed width (and height, when above zero). */
	inline USizeBox *MakeSized(UWidgetTree *Tree, float Width, float Height, UWidget *Content)
	{
		USizeBox *Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Width);
		if (Height > 0.0f)
		{
			Box->SetHeightOverride(Height);
		}
		Box->AddChild(Content);
		return Box;
	}

	inline UHorizontalBoxSlot *AddToRow(UHorizontalBox *Row, UWidget *Child, const FMargin &Padding = FMargin(0.0f),
										EVerticalAlignment VAlign = VAlign_Center)
	{
		UHorizontalBoxSlot *BoxSlot = Row->AddChildToHorizontalBox(Child);
		BoxSlot->SetPadding(Padding);
		BoxSlot->SetVerticalAlignment(VAlign);
		return BoxSlot;
	}

	inline UVerticalBoxSlot *AddToColumn(UVerticalBox *Column, UWidget *Child, const FMargin &Padding = FMargin(0.0f))
	{
		UVerticalBoxSlot *BoxSlot = Column->AddChildToVerticalBox(Child);
		BoxSlot->SetPadding(Padding);
		return BoxSlot;
	}

	/** A bar with its value centred inside it as plain white text, no backing box (D-185, D-197); the bars are
	 *  dark enough for it to read (D-194). Size it with MakeSized: tall enough for the value (BarWithValueHeight). */
	inline UWidget *MakeBarWithValue(UWidgetTree *Tree, const FLinearColor &FillColor, UProgressBar *&OutBar, UTextBlock *&OutValue)
	{
		OutBar = MakeBar(Tree, FillColor);
		OutValue = MakeText(Tree, SmallFontSize, FLinearColor::White, true);
		UOverlay *BarWithValue = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		BarWithValue->AddChildToOverlay(OutBar)->SetHorizontalAlignment(HAlign_Fill);
		UOverlaySlot *ValueSlot = BarWithValue->AddChildToOverlay(OutValue);
		ValueSlot->SetHorizontalAlignment(HAlign_Center);
		ValueSlot->SetVerticalAlignment(VAlign_Center);
		return BarWithValue;
	}

	/** A tab: a plate with a bold label (lit by the screen when it is the shown one). */
	inline UBorder *MakeTab(UWidgetTree *Tree, UTextBlock *&OutLabel)
	{
		UBorder *Tab = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Tab->SetBrushColor(PlateColor);
		Tab->SetPadding(FMargin(TabPaddingX, TabPaddingY));
		OutLabel = MakeText(Tree, RowFontSize, DimColor, true);
		Tab->SetContent(OutLabel);
		return Tab;
	}

	/** The small key label beside a tab row, as a key-hint pattern (B-481): "{SubTabPrev}" reads "LT" or "Z". */
	inline UTextBlock *MakeKeyLabel(UWidgetTree *Tree, const FText &Pattern)
	{
		UTextBlock *Text = MakeHintText(Tree, SmallFontSize, CyanColor, true);
		WoRKeyHints::SetHint(Text, Pattern);
		return Text;
	}

	inline void SetShown(UWidget *Widget, bool bShown)
	{
		if (Widget)
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	// ==================== CHIP ROW (B-482; FChipRow / FChipState: UI/Menus/MenuChipRow.h) ====================

	/** Builds a row of Count chips between PrevKeyPattern and NextKeyPattern (key-hint patterns: "{TabPrev}"). */
	inline UHorizontalBox *MakeChipRow(UWidgetTree *Tree, int32 Count, const FText &PrevKeyPattern, const FText &NextKeyPattern, FChipRow &OutRow)
	{
		UHorizontalBox *Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddToRow(Row, MakeKeyLabel(Tree, PrevKeyPattern), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UTextBlock *Label = nullptr;
			UBorder *Chip = MakeTab(Tree, Label);
			UTextBlock *Note = MakeText(Tree, SmallFontSize, OffColor);
			AddToRow(Row, Chip, FMargin(0.0f, 0.0f, TabGap, 0.0f));
			AddToRow(Row, Note, FMargin(0.0f, 0.0f, TabGap, 0.0f), VAlign_Top);
			OutRow.Chips.Add(Chip);
			OutRow.Labels.Add(Label);
			OutRow.Notes.Add(Note);
		}
		AddToRow(Row, MakeKeyLabel(Tree, NextKeyPattern), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
		return Row;
	}

	/** Names and colours the chips: one per state (the rest hidden); Current is filled, Hovered lit. */
	inline void SetChips(const FChipRow &Row, TConstArrayView<FChipState> States, int32 Current, int32 Hovered = INDEX_NONE)
	{
		for (int32 Index = 0; Index < Row.Chips.Num(); ++Index)
		{
			UBorder *Chip = Row.Chips[Index].Get();
			UTextBlock *Label = Row.Labels[Index].Get();
			UTextBlock *Note = Row.Notes[Index].Get();
			const bool bShown = States.IsValidIndex(Index);
			SetShown(Chip, bShown);
			SetShown(Note, bShown && !States[Index].Note.IsEmpty());
			if (!bShown || !Chip || !Label || !Note)
			{
				continue;
			}
			const FChipState &State = States[Index];
			const bool bCurrent = Index == Current && !State.bGreyed;
			Chip->SetBrushColor(bCurrent ? FocusColor : Index == Hovered && !State.bGreyed ? HoverColor : PlateColor);
			Label->SetText(State.Name);
			Label->SetColorAndOpacity(FSlateColor(State.bGreyed ? OffColor : bCurrent ? TextColor : DimColor));
			Note->SetText(State.Note);
		}
	}

	/** The shown chip under ScreenPosition; INDEX_NONE for none (and before the row is drawn). */
	inline int32 FindChipUnder(const FChipRow &Row, const FVector2D &ScreenPosition)
	{
		for (int32 Index = 0; Index < Row.Chips.Num(); ++Index)
		{
			const UBorder *Chip = Row.Chips[Index].Get();
			if (Chip && Chip->IsVisible() && Chip->GetCachedGeometry().IsUnderLocation(ScreenPosition))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
}
