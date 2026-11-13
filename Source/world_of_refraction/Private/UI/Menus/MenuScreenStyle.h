// MenuScreenStyle.h
// B-439 / B-440a: the look and the small builders every C++-built menu screen shares (the game menu, the loadout
// screen), so the screens read as one set. The mockup kit's tokens (screen_mockups/_kit/wor_ui.py) at 1080p:
// Slate units, DPI-scaled.
// TODO: the Hidris art pass replaces the flat colours and fonts (UI_Theme.md).

#pragma once

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CoreMinimal.h"
#include "InputCoreTypes.h"

namespace WoRMenuStyle
{
	constexpr float ScreenPadding = 72.0f;
	constexpr float LineGap = 9.0f;
	constexpr float SectionGap = 18.0f;
	constexpr float WordGap = 9.0f;

	constexpr int32 TitleFontSize = 38;
	constexpr int32 SubtitleFontSize = 20;
	constexpr int32 RowFontSize = 22;
	constexpr int32 NameFontSize = 29;
	constexpr int32 LineFontSize = 19;
	constexpr int32 SmallFontSize = 17;

	inline const FLinearColor ScreenColor = FLinearColor::FromSRGBColor(FColor(16, 18, 25, 240));
	inline const FLinearColor PanelColor = FLinearColor::FromSRGBColor(FColor(20, 22, 28));
	inline const FLinearColor PlateColor = FLinearColor::FromSRGBColor(FColor(46, 46, 46));
	inline const FLinearColor TrackColor = FLinearColor::FromSRGBColor(FColor(42, 45, 53));
	inline const FLinearColor FocusColor = FLinearColor::FromSRGBColor(FColor(51, 127, 141));
	inline const FLinearColor CyanColor = FLinearColor::FromSRGBColor(FColor(159, 236, 245));
	inline const FLinearColor TextColor = FLinearColor::FromSRGBColor(FColor(238, 241, 245));
	inline const FLinearColor DimColor = FLinearColor::FromSRGBColor(FColor(169, 175, 184));
	inline const FLinearColor OffColor = FLinearColor::FromSRGBColor(FColor(107, 112, 120));
	inline const FLinearColor HPColor = FLinearColor::FromSRGBColor(FColor(224, 64, 127));
	inline const FLinearColor LowHPColor = FLinearColor::FromSRGBColor(FColor(255, 143, 180));
	inline const FLinearColor EPColor = FLinearColor::FromSRGBColor(FColor(111, 214, 228));
	inline const FLinearColor WarnColor = FLinearColor::FromSRGBColor(FColor(242, 162, 58));
	inline const FLinearColor ValueBackingColor = FLinearColor::FromSRGBColor(FColor(16, 18, 25, 170));

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
	/** Sub-tabs, e.g. a character (UIScreens, shared menu controls): LT / RT, Z / C. */
	inline bool IsSubTabPrevKey(const FKey &Key) { return Key == EKeys::Z || Key == EKeys::Gamepad_LeftTrigger; }
	inline bool IsSubTabNextKey(const FKey &Key) { return Key == EKeys::C || Key == EKeys::Gamepad_RightTrigger; }

	inline UTextBlock *MakeText(UWidgetTree *Tree, int32 FontSize, const FLinearColor &Color, bool bBold = false)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
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

	inline void SetShown(UWidget *Widget, bool bShown)
	{
		if (Widget)
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}
