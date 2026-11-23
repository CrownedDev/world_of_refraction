// BattleCommandMenuWidget.cpp

#include "UI/Combat/Battle/BattleCommandMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionPipsText.h"
#include "Combat/CombatPlayerController.h"
#include "Combat/WoRFightSubsystem.h"
#include "CommonInputSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Equipment/Crystals/CrystalManager.h"
#include "GameFramework/Actor.h"
#include "Infusion/InfusionConstants.h"
#include "UI/Combat/Battle/BattleHUDButton.h"
#include "UI/Combat/Battle/BattleHUDCard.h"
#include "UI/Combat/Battle/BattleHUDView.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRKeyHintText.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "BattleCommandMenu"

namespace BattleCommandMenuStyle
{
	// TODO: tune in PIE against refine_final/11 and 12 (placeholder until the CommonUI screens, D-024). Slate units,
	// DPI-scaled. Colours are the menus' (WoRMenuStyle).
	/** D-260: the frame is exactly the full party card's width (BattleCardStyle, shared with the hub and trial). */
	constexpr float FrameWidth = BattleCardStyle::CardWidth + 2.0f * BattleCardStyle::CardPadding;
	constexpr float FramePadding = 6.0f;
	constexpr float FrameOutline = 1.0f;
	constexpr float RowGap = 3.0f;
	constexpr float RowPaddingX = 8.0f;
	constexpr float RowPaddingY = 5.0f;
	/** A cut corner drawn as a rounded one (the pictures cut the top-left corner of the tab and each row). */
	constexpr float CornerRadius = 6.0f;
	constexpr float KeyCapSize = 22.0f;
	constexpr float KeyCapOutline = 1.5f;
	constexpr float IconSize = 20.0f;
	constexpr float InnerGap = 8.0f;
	/** The rows scroll past this height, a scroll bar showing (a long item or target list). B-436g QA: 420 cut the 12th
	 *  item; 600 holds 16 one-line rows and still clears the top-left at 720p (the turn order is top right). */
	constexpr float MaxListHeight = 600.0f;
	constexpr float HintGap = 6.0f;
	/** B-436g QA: the corner hints sit on a dark plate in the frame's style, so they read on a bright floor. */
	constexpr float HintPlatePadding = 8.0f;
	constexpr int32 TabFontSize = 20; // B-436g QA: the drawing's tab is as tall as a row; 14 read small
	constexpr int32 HeaderFontSize = 13;
	constexpr int32 RowFontSize = 17;
	constexpr int32 KeyFontSize = 12;
	constexpr int32 SmallFontSize = 11;
	constexpr int32 HintFontSize = 15;
	const FLinearColor TabColor = FLinearColor::FromSRGBColor(FColor(30, 24, 46));
	const FLinearColor DurabilityColor(0.85f, 0.85f, 0.7f); // B-391c
	// shortcut: the face-button colours repeat WoRKeyHintText.cpp's (private there), share them when the menus' caps do.
	const FLinearColor FaceAColor = FLinearColor::FromSRGBColor(FColor(108, 203, 95));
	const FLinearColor FaceBColor = FLinearColor::FromSRGBColor(FColor(235, 87, 87));
	const FLinearColor FaceXColor = FLinearColor::FromSRGBColor(FColor(91, 141, 239));
	const FLinearColor FaceYColor = FLinearColor::FromSRGBColor(FColor(242, 201, 76));
}

namespace BattleCommandMenuBuild
{
	using namespace BattleCommandMenuStyle;

	/** A rounded top-left corner, the rest square: the pictures' cut corner. */
	FVector4 CutCorner() { return FVector4(CornerRadius, 0.0f, 0.0f, 0.0f); }

	/** The short name the pictures use: A / B / X / Y, LB / RB, the D-pad and stick by name; a keyboard key as UE names it. */
	FText ShortKeyName(const FKey &Key)
	{
		if (Key == EKeys::Gamepad_FaceButton_Bottom) return INVTEXT("A");
		if (Key == EKeys::Gamepad_FaceButton_Right) return INVTEXT("B");
		if (Key == EKeys::Gamepad_FaceButton_Left) return INVTEXT("X");
		if (Key == EKeys::Gamepad_FaceButton_Top) return INVTEXT("Y");
		if (Key == EKeys::Gamepad_LeftShoulder) return INVTEXT("LB");
		if (Key == EKeys::Gamepad_RightShoulder) return INVTEXT("RB");
		if (Key == EKeys::Gamepad_LeftThumbstick) return INVTEXT("L3");
		if (Key == EKeys::Gamepad_DPad_Down) return LOCTEXT("DPadDown", "D-pad down");
		if (Key == EKeys::Gamepad_DPad_Right) return LOCTEXT("DPadRight", "D-pad right");
		if (Key == EKeys::Gamepad_DPad_Up) return LOCTEXT("DPadUp", "D-pad up"); // B-588: Position
		if (Key == EKeys::BackSpace) return LOCTEXT("Backspace", "Backspace"); // as the other menus' hints (WoRKeyHintText)
		return Key.GetDisplayName(/*bLongDisplayName=*/false);
	}

	/** A face button's letter in its colour; any other key in the text colour. */
	FLinearColor KeyTextColor(const FKey &Key)
	{
		if (Key == EKeys::Gamepad_FaceButton_Bottom) return FaceAColor;
		if (Key == EKeys::Gamepad_FaceButton_Right) return FaceBColor;
		if (Key == EKeys::Gamepad_FaceButton_Left) return FaceXColor;
		if (Key == EKeys::Gamepad_FaceButton_Top) return FaceYColor;
		return WoRMenuStyle::TextColor;
	}

	/** A face button: a ring in its colour (the pictures' A / X / Y); any other key: a framed cap. */
	const FSlateBrush &KeyCapBrush(const FKey &Key)
	{
		static const FSlateRoundedBoxBrush Framed(WoRMenuStyle::PanelColor, CornerRadius * 0.5f, WoRMenuStyle::DimColor, FrameOutline);
		static const FSlateRoundedBoxBrush RingA(FLinearColor::Transparent, KeyCapSize * 0.5f, FaceAColor, KeyCapOutline);
		static const FSlateRoundedBoxBrush RingB(FLinearColor::Transparent, KeyCapSize * 0.5f, FaceBColor, KeyCapOutline);
		static const FSlateRoundedBoxBrush RingX(FLinearColor::Transparent, KeyCapSize * 0.5f, FaceXColor, KeyCapOutline);
		static const FSlateRoundedBoxBrush RingY(FLinearColor::Transparent, KeyCapSize * 0.5f, FaceYColor, KeyCapOutline);
		if (Key == EKeys::Gamepad_FaceButton_Bottom) return RingA;
		if (Key == EKeys::Gamepad_FaceButton_Right) return RingB;
		if (Key == EKeys::Gamepad_FaceButton_Left) return RingX;
		if (Key == EKeys::Gamepad_FaceButton_Top) return RingY;
		return Framed;
	}

	/** A small row at the foot of the list, no plate: the infusion controls and a section header ("Allies"). */
	bool IsFooterRow(EPieMenuCategory Category)
	{
		return Category == EPieMenuCategory::CycleLevel || Category == EPieMenuCategory::CycleSource ||
			   Category == EPieMenuCategory::SectionHeader;
	}

	/** The row's words: the infusion level row says what it is (refine_final/12: "Infusion: Weapon crystal Lv 1"). */
	FText RowName(const FPieMenuButtonData &Row)
	{
		return Row.Category == EPieMenuCategory::CycleLevel ? FText::Format(LOCTEXT("InfusionRow", "Infusion: {0}"), Row.DisplayName)
															: Row.DisplayName;
	}

	/** The row's look: plate, violet on hover (the highlighted target uses Highlighted); a footer row has none. */
	const FButtonStyle &RowStyle(bool bHighlighted, bool bFooter)
	{
		static const FSlateRoundedBoxBrush Plate(WoRMenuStyle::PlateColor, CutCorner());
		static const FSlateRoundedBoxBrush Focus(WoRMenuStyle::FocusColor, CutCorner(), WoRMenuStyle::CyanColor, FrameOutline);
		static const FSlateRoundedBoxBrush Hover(WoRMenuStyle::HoverColor, CutCorner());
		static const FSlateNoResource None;
		const auto Make = [](const FSlateBrush &Normal, const FSlateBrush &Hovered)
		{
			FButtonStyle Style;
			Style.SetNormal(Normal).SetHovered(Hovered).SetPressed(Focus).SetDisabled(Normal);
			Style.SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f));
			return Style;
		};
		static const FButtonStyle Row = Make(Plate, Focus);
		static const FButtonStyle Highlighted = Make(Focus, Focus);
		static const FButtonStyle Footer = Make(None, Hover);
		return bFooter ? Footer : bHighlighted ? Highlighted : Row;
	}

	UTextBlock *MakeText(UWidgetTree *Tree, int32 Size, const FLinearColor &Color, bool bBold = false)
	{
		UTextBlock *Text = WoRMenuStyle::MakeText(Tree, Size, Color, bBold);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible); // a click lands on the row's button
		return Text;
	}
}

void UBattleMenuRowRelay::HandleClicked()
{
	if (UBattleCommandMenuWidget *Owner = Menu.Get())
	{
		Owner->PressRow(RowIndex);
	}
}

void UBattleMenuRowRelay::HandleHovered()
{
	if (UBattleCommandMenuWidget *Owner = Menu.Get())
	{
		Owner->HoverRow(RowIndex);
	}
}

// ========================================
// B-436g: THE CORNER HINTS
// ========================================

void UBattleMenuHintsWidget::SetHints(UBattleCommandMenuWidget *Menu, const TArray<FBattleMenuHint> &InHints)
{
	Owner = Menu;
	Hints = InHints;
	PushToWidgets();
}

void UBattleMenuHintsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	PushToWidgets();
}

TSharedRef<SWidget> UBattleMenuHintsWidget::RebuildWidget()
{
	using namespace BattleCommandMenuStyle;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		ClearFlags(RF_Transactional); // TransBuffer rule, as the menu
		// B-436g QA: a dark plate in the frame's style behind the hints, so they read on any floor.
		static const FSlateRoundedBoxBrush PlateBrush(WoRMenuStyle::PanelColor, BattleCommandMenuBuild::CutCorner(), WoRMenuStyle::OffColor, FrameOutline);
		UBorder *Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HintsPlate"));
		Plate->SetBrush(PlateBrush);
		Plate->SetPadding(FMargin(HintPlatePadding));
		Plate->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = Plate;
		UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Hints"));
		Column->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Plate->SetContent(Column);
		for (int32 Index = 0; Index < MAX_HINTS; ++Index)
		{
			UButton *Button = WidgetTree->ConstructWidget<UBattleHUDButton>(UBattleHUDButton::StaticClass());
			Button->SetStyle(BattleCommandMenuBuild::RowStyle(false, /*bFooter=*/true));
			UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			// The key as the rows draw it: a ring in a face button's colour, else a framed cap (PushToWidgets).
			UBorder *Cap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Cap->SetHorizontalAlignment(HAlign_Center);
			Cap->SetVerticalAlignment(VAlign_Center);
			Cap->SetPadding(FMargin(RowPaddingY, 0.0f));
			Cap->SetVisibility(ESlateVisibility::HitTestInvisible);
			UTextBlock *Key = BattleCommandMenuBuild::MakeText(WidgetTree, KeyFontSize, WoRMenuStyle::TextColor, /*bBold=*/true);
			Cap->SetContent(Key);
			USizeBox *CapSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			CapSize->SetHeightOverride(KeyCapSize);
			CapSize->SetMinDesiredWidth(KeyCapSize);
			CapSize->AddChild(Cap);
			UTextBlock *Label = BattleCommandMenuBuild::MakeText(WidgetTree, HintFontSize, WoRMenuStyle::TextColor, /*bBold=*/true);
			WoRMenuStyle::AddToRow(Line, CapSize, FMargin(0.0f, 0.0f, InnerGap, 0.0f));
			WoRMenuStyle::AddToRow(Line, Label);
			Button->AddChild(Line);
			WoRMenuStyle::AddToColumn(Column, Button, FMargin(0.0f, HintGap * 0.5f))->SetHorizontalAlignment(HAlign_Right);
			UBattleMenuRowRelay *Relay = NewObject<UBattleMenuRowRelay>(this);
			Button->OnClicked.AddDynamic(Relay, &UBattleMenuRowRelay::HandleClicked);
			Buttons.Add(Button);
			KeyCaps.Add(Cap);
			KeyTexts.Add(Key);
			LabelTexts.Add(Label);
			Relays.Add(Relay);
		}
	}
	return Super::RebuildWidget();
}

void UBattleMenuHintsWidget::PushToWidgets()
{
	SetVisibility(Hints.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		UButton *Button = Buttons[Index];
		if (!Button || !KeyCaps[Index] || !KeyTexts[Index] || !LabelTexts[Index] || !Relays[Index])
		{
			continue;
		}
		if (!Hints.IsValidIndex(Index))
		{
			Button->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const FBattleMenuHint &Hint = Hints[Index];
		Relays[Index]->Menu = Owner;
		Relays[Index]->RowIndex = Hint.RowIndex;
		KeyTexts[Index]->SetText(Hint.Key);
		KeyTexts[Index]->SetColorAndOpacity(FSlateColor(BattleCommandMenuBuild::KeyTextColor(Hint.KeyId)));
		KeyCaps[Index]->SetBrush(BattleCommandMenuBuild::KeyCapBrush(Hint.KeyId));
		WoRMenuStyle::SetShown(KeyCaps[Index]->GetParent(), !Hint.Key.IsEmpty());
		LabelTexts[Index]->SetText(Hint.Label);
		LabelTexts[Index]->SetColorAndOpacity(FSlateColor(Hint.bEnabled ? WoRMenuStyle::TextColor : WoRMenuStyle::OffColor));
		Button->SetIsEnabled(Hint.bEnabled);
		Button->SetToolTipText(Hint.Reason); // a greyed hint says why (Flee in a boss fight)
		Button->SetVisibility(ESlateVisibility::Visible);
	}
}

// ========================================
// LIFECYCLE
// ========================================

void UBattleCommandMenuWidget::Initialise()
{
	if (bBound)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[BattleCommandMenu] Initialise called twice; ignoring"));
		return;
	}
	ClearFlags(RF_Transactional); // TransBuffer rule, as the old menu

	UCombatCommandMenuSubsystem *Menu = WoRFight::Get<UCombatCommandMenuSubsystem>(this);
	if (!Menu)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[BattleCommandMenu] No command menu subsystem: the menu stays empty"));
		return;
	}
	BoundMenu = Menu;
	Menu->OnCommandMenuReady.AddDynamic(this, &UBattleCommandMenuWidget::HandleMenuReady);
	Menu->OnCommandMenuClosed.AddDynamic(this, &UBattleCommandMenuWidget::HandleMenuClosed);
	Menu->OnTargetHighlighted.AddDynamic(this, &UBattleCommandMenuWidget::HandleTargetHighlighted); // B-436g: its row turns violet
	// B-436g: the keys shown follow the device in use (none headless: the keyboard's).
	if (UCommonInputSubsystem *Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer()))
	{
		BoundInput = Input;
		InputChangedHandle = Input->OnInputMethodChangedNative.AddUObject(this, &UBattleCommandMenuWidget::HandleInputMethodChanged);
	}
	// B-391c: the durability header follows per-cast wear and breaks, as the old header (a run-wide service; the weak
	// pointer is only for the unbind).
	if (UGameInstance *GameInstance = GetGameInstance())
	{
		if (UCrystalManager *Crystals = GameInstance->GetSubsystem<UCrystalManager>())
		{
			BoundCrystals = Crystals;
			Crystals->OnCrystalDurabilityChanged.AddDynamic(this, &UBattleCommandMenuWidget::HandleCrystalDurabilityChanged);
			Crystals->OnCrystalBroken.AddDynamic(this, &UBattleCommandMenuWidget::HandleCrystalBroken);
		}
	}
	bBound = true;

	// Replay: the first player turn opens the menu inside InitializeCombat, before the HUD is built.
	if (Menu->IsOpen())
	{
		Menu->RefreshMenu();
	}
}

void UBattleCommandMenuWidget::Teardown()
{
	if (!bBound)
	{
		return;
	}
	if (UCombatCommandMenuSubsystem *Menu = BoundMenu.Get())
	{
		Menu->OnCommandMenuReady.RemoveDynamic(this, &UBattleCommandMenuWidget::HandleMenuReady);
		Menu->OnCommandMenuClosed.RemoveDynamic(this, &UBattleCommandMenuWidget::HandleMenuClosed);
		Menu->OnTargetHighlighted.RemoveDynamic(this, &UBattleCommandMenuWidget::HandleTargetHighlighted);
	}
	BoundMenu.Reset();
	if (UCommonInputSubsystem *Input = BoundInput.Get())
	{
		Input->OnInputMethodChangedNative.Remove(InputChangedHandle);
	}
	BoundInput.Reset();
	InputChangedHandle.Reset();
	if (UCrystalManager *Crystals = BoundCrystals.Get())
	{
		Crystals->OnCrystalDurabilityChanged.RemoveDynamic(this, &UBattleCommandMenuWidget::HandleCrystalDurabilityChanged);
		Crystals->OnCrystalBroken.RemoveDynamic(this, &UBattleCommandMenuWidget::HandleCrystalBroken);
	}
	BoundCrystals.Reset();
	bBound = false;
}

void UBattleCommandMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	PushToWidgets();
}

void UBattleCommandMenuWidget::NativeDestruct()
{
	Teardown();
	Super::NativeDestruct();
}

TSharedRef<SWidget> UBattleCommandMenuWidget::RebuildWidget()
{
	using namespace BattleCommandMenuStyle;
	using BattleCommandMenuBuild::MakeText;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// B-436g (D-260): the Act tab on the frame's top edge, then the frame, exactly the full card's width.
		UVerticalBox *Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Menu"));
		Column->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = Column;

		static const FSlateRoundedBoxBrush TabBrush(TabColor, BattleCommandMenuBuild::CutCorner(), WoRMenuStyle::CyanColor, FrameOutline);
		UBorder *Tab = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ActTab"));
		Tab->SetBrush(TabBrush);
		Tab->SetPadding(FMargin(RowPaddingX * 1.5f, RowPaddingY));
		Tab->SetVisibility(ESlateVisibility::HitTestInvisible);
		TabText = MakeText(WidgetTree, TabFontSize, WoRMenuStyle::CyanColor, /*bBold=*/true);
		Tab->SetContent(TabText);
		WoRMenuStyle::AddToColumn(Column, Tab)->SetHorizontalAlignment(HAlign_Left);

		static const FSlateRoundedBoxBrush FrameBrush(WoRMenuStyle::PanelColor, 0.0f, WoRMenuStyle::OffColor, FrameOutline);
		UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
		Frame->SetBrush(FrameBrush);
		Frame->SetPadding(FMargin(FramePadding));
		Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UVerticalBox *Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Body"));
		Frame->SetContent(Body);
		WoRMenuStyle::AddToColumn(Column, WoRMenuStyle::MakeSized(WidgetTree, FrameWidth, 0.0f, Frame))->SetHorizontalAlignment(HAlign_Left);

		// The header past the main menu ("Skills > Abilities"), a rule under it.
		UVerticalBox *Header = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Header"));
		TitleText = MakeText(WidgetTree, HeaderFontSize, WoRMenuStyle::DimColor, /*bBold=*/true);
		WoRMenuStyle::AddToColumn(Header, TitleText, FMargin(RowPaddingX, RowPaddingY * 0.5f));
		UBorder *Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Rule->SetBrushColor(WoRMenuStyle::TrackColor);
		USizeBox *RuleHeight = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		RuleHeight->SetHeightOverride(FrameOutline);
		RuleHeight->AddChild(Rule);
		WoRMenuStyle::AddToColumn(Header, RuleHeight, FMargin(0.0f, 0.0f, 0.0f, RowGap * 2.0f));
		Header->SetVisibility(ESlateVisibility::HitTestInvisible);
		WoRMenuStyle::AddToColumn(Body, Header);
		HeaderBlock = Header;

		// B-391c: the durability line (the old header), small, above the rows.
		DurabilityText = MakeText(WidgetTree, SmallFontSize, DurabilityColor);
		WoRMenuStyle::AddToColumn(Body, DurabilityText, FMargin(RowPaddingX, 0.0f, RowPaddingX, RowGap));

		UScrollBox *Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Rows"));
		Scroll->SetScrollBarVisibility(ESlateVisibility::Visible); // B-436g QA: a cut-off row must look scrollable
		USizeBox *ListHeight = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ListHeight->SetMaxDesiredHeight(MaxListHeight);
		ListHeight->AddChild(Scroll);
		WoRMenuStyle::AddToColumn(Body, ListHeight);

		for (int32 Index = 0; Index < MAX_ROWS; ++Index)
		{
			UButton *Button = WidgetTree->ConstructWidget<UBattleHUDButton>(UBattleHUDButton::StaticClass());
			Button->SetStyle(BattleCommandMenuBuild::RowStyle(false, false));
			UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

			// The key: a ring in a face button's colour, else a framed cap.
			UBorder *Cap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Cap->SetHorizontalAlignment(HAlign_Center);
			Cap->SetVerticalAlignment(VAlign_Center);
			Cap->SetPadding(FMargin(0.0f));
			Cap->SetVisibility(ESlateVisibility::HitTestInvisible);
			UTextBlock *KeyText = MakeText(WidgetTree, KeyFontSize, WoRMenuStyle::TextColor, /*bBold=*/true);
			Cap->SetContent(KeyText);
			USizeBox *CapSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			CapSize->SetHeightOverride(KeyCapSize);
			CapSize->SetMinDesiredWidth(KeyCapSize); // a ring is round; "Backspace" widens its cap
			CapSize->AddChild(Cap);
			WoRMenuStyle::AddToRow(Line, CapSize, FMargin(0.0f, 0.0f, InnerGap, 0.0f));

			UImage *Icon = WoRMenuStyle::MakeIcon(WidgetTree, IconSize);
			WoRMenuStyle::AddToRow(Line, Icon, FMargin(0.0f, 0.0f, InnerGap, 0.0f));

			// The name, wrapping rather than overflowing, and a greyed row's reason under it.
			UVerticalBox *Words = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Words->SetVisibility(ESlateVisibility::HitTestInvisible);
			UTextBlock *Label = MakeText(WidgetTree, RowFontSize, WoRMenuStyle::TextColor, /*bBold=*/true);
			Label->SetAutoWrapText(true);
			UTextBlock *Reason = MakeText(WidgetTree, SmallFontSize, WoRMenuStyle::WarnColor);
			Reason->SetAutoWrapText(true);
			WoRMenuStyle::AddToColumn(Words, Label);
			WoRMenuStyle::AddToColumn(Words, Reason);
			WoRMenuStyle::AddToRow(Line, Words)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

			UTextBlock *Quantity = MakeText(WidgetTree, HeaderFontSize, WoRMenuStyle::DimColor, /*bBold=*/true);
			WoRMenuStyle::AddToRow(Line, Quantity, FMargin(InnerGap, 0.0f, 0.0f, 0.0f));
			UTextBlock *Cost = MakeText(WidgetTree, HeaderFontSize, WoRMenuStyle::CyanColor);
			WoRMenuStyle::AddToRow(Line, Cost, FMargin(InnerGap, 0.0f, 0.0f, 0.0f));

			Button->AddChild(Line);
			if (UButtonSlot *Inner = Cast<UButtonSlot>(Line->Slot))
			{
				Inner->SetPadding(FMargin(RowPaddingX, RowPaddingY));
				Inner->SetHorizontalAlignment(HAlign_Fill);
			}
			if (UScrollBoxSlot *RowSlot = Cast<UScrollBoxSlot>(Scroll->AddChild(Button)))
			{
				RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, RowGap));
				RowSlot->SetHorizontalAlignment(HAlign_Fill);
			}
			UBattleMenuRowRelay *Relay = NewObject<UBattleMenuRowRelay>(this);
			Relay->RowIndex = Index;
			Relay->Menu = this;
			Button->OnClicked.AddDynamic(Relay, &UBattleMenuRowRelay::HandleClicked);
			Button->OnHovered.AddDynamic(Relay, &UBattleMenuRowRelay::HandleHovered); // B-400b
			RowButtons.Add(Button);
			RowKeyCaps.Add(Cap);
			RowKeyTexts.Add(KeyText);
			RowIcons.Add(Icon);
			RowTexts.Add(Label);
			RowReasonTexts.Add(Reason);
			RowQuantityTexts.Add(Quantity);
			RowCostTexts.Add(Cost);
			RowRelays.Add(Relay);
		}
	}
	return Super::RebuildWidget();
}

// ========================================
// STATE (plain data; runs unbuilt)
// ========================================

void UBattleCommandMenuWidget::HandleMenuReady(const TArray<FPieMenuButtonData> &Buttons)
{
	const UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	bAtMainMenu = Menu && Menu->IsAtMainMenu();
	// B-582b: the same view rebuilt (an infusion row pressed, the main menu refreshed) keeps its lit row; a new one starts unlit.
	const FText NewTitle = Menu ? Menu->GetViewTitle() : FText::GetEmpty();
	if (!NewTitle.EqualTo(Title) || Buttons.Num() != Rows.Num() || !bShowing)
	{
		LitRow = INDEX_NONE;
	}
	Rows = Buttons;
	if (Rows.Num() > MAX_ROWS)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[BattleCommandMenu] %d buttons but %d rows: the rest are not shown"), Rows.Num(), MAX_ROWS);
		Rows.SetNum(MAX_ROWS);
	}
	RefreshKeys();
	// D-260: no "Your turn" title; past the main menu the header says where you are.
	Title = Menu ? Menu->GetViewTitle() : FText::GetEmpty();
	RefreshDurability(); // B-391c: per menu open, as the old header
	bShowing = true;
	RefreshActionsLine();
	PushToWidgets();
}

void UBattleCommandMenuWidget::HandleMenuClosed()
{
	Rows.Reset();
	LitRow = INDEX_NONE;
	RowLabels.Reset();
	RowKeys.Reset();
	Hints.Reset();
	Title = FText::GetEmpty();
	DurabilityLines.Reset();
	bShowing = false;
	PushToWidgets();
}

bool UBattleCommandMenuWidget::IsCornerHint(EPieMenuCategory Category, bool bAtMainMenu)
{
	if (bAtMainMenu)
	{
		return Category != EPieMenuCategory::Attack && Category != EPieMenuCategory::Skills && Category != EPieMenuCategory::Items;
	}
	return Category == EPieMenuCategory::Back;
}

void UBattleCommandMenuWidget::RefreshKeys()
{
	TArray<EPieMenuCategory> Categories;
	TArray<bool> Enabled;
	for (const FPieMenuButtonData &Row : Rows)
	{
		Categories.Add(Row.Category);
		Enabled.Add(Row.bEnabled && Row.Category != EPieMenuCategory::SectionHeader);
	}
	const UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	const TArray<int32> KeyForRow = BattleHUDView::AssignMenuKeys(Categories, Enabled, bAtMainMenu,
																  Menu && Menu->IsPositionPickerOpen()); // B-588
	const TArray<FCombatMenuKey> &Keys = ACombatPlayerController::GetMenuKeys();
	const bool bGamepad = WoRKeyHints::IsGamepad(this); // the device in use only (D-258)

	RowKeys.Reset();
	RowLabels.Reset();
	Hints.Reset();
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FPieMenuButtonData &Row = Rows[Index];
		const FCombatMenuKey *Key = Keys.IsValidIndex(KeyForRow[Index]) ? &Keys[KeyForRow[Index]] : nullptr;
		const FKey Shown = Key ? (bGamepad ? Key->Gamepad : Key->Keyboard) : FKey();
		const FText KeyName = Shown.IsValid() ? BattleCommandMenuBuild::ShortKeyName(Shown) : FText::GetEmpty();
		const FText Name = BattleCommandMenuBuild::RowName(Row);
		const FText Reason = Row.bEnabled ? FText::GetEmpty() : Row.Description; // only a greyed row says why
		const FText Named = Reason.IsEmpty() ? Name : FText::Format(LOCTEXT("RowWithReason", "{0}  ({1})"), Name, Reason);
		RowKeys.Add(Shown);
		RowLabels.Add(KeyName.IsEmpty() ? Named : FText::Format(LOCTEXT("RowWithKey", "[{0}] {1}"), KeyName, Named));
		if (IsCornerHint(Row.Category, bAtMainMenu))
		{
			FBattleMenuHint &Hint = Hints.AddDefaulted_GetRef();
			Hint.Key = KeyName;
			Hint.KeyId = Shown;
			Hint.Label = Name;
			Hint.Reason = Reason;
			Hint.RowIndex = Index;
			Hint.bEnabled = Row.bEnabled;
		}
	}
}

void UBattleCommandMenuWidget::RefreshActionsLine()
{
	// D-260: the pips live on the menu's tab now, for whoever the menu is open for (D-259's rules, MakeCardActionsLine).
	const UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	const AActor *Acting = Menu ? Menu->GetCurrentActor() : nullptr;
	const UCharacterDataComponent *Data = Acting ? Acting->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	ActionsLine = Data ? ActionPipsText::MakeCardActionsLine(Data->CurrentActions, Data->GetMaxActions(), Data->SkillsUsedThisTurn)
					   : FText::GetEmpty();
	if (TabText)
	{
		TabText->SetText(ActionsLine);
		WoRMenuStyle::SetShown(TabText->GetParent(), !ActionsLine.IsEmpty());
	}
}

UBattleMenuHintsWidget *UBattleCommandMenuWidget::GetHintsWidget()
{
	if (!HintsWidget)
	{
		HintsWidget = CreateWidget<UBattleMenuHintsWidget>(this, UBattleMenuHintsWidget::StaticClass());
		if (HintsWidget)
		{
			HintsWidget->SetHints(this, bShowing ? Hints : TArray<FBattleMenuHint>());
		}
		else
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[BattleCommandMenu] Could not make the corner hints widget: Defend, Flee, End Turn and Back have no hints"));
		}
	}
	return HintsWidget;
}

void UBattleCommandMenuWidget::HandleTargetHighlighted(AActor *Actor, AActor *Target, int32 TargetCount)
{
	if (bShowing)
	{
		// B-582b: a new highlight (hover, Q / E, A / D, the picker's default) lights its row.
		const int32 TargetRow = Target ? Rows.IndexOfByPredicate([Target](const FPieMenuButtonData &Row)
															   { return Row.Category == EPieMenuCategory::Target && Row.Targets.IsEmpty() && Row.DataReference == Target; })
									   : INDEX_NONE;
		if (TargetRow != INDEX_NONE)
		{
			LitRow = TargetRow;
		}
		PushToWidgets(); // the lit row turns violet
	}
}

bool UBattleCommandMenuWidget::IsNavigableRow(int32 RowIndex) const
{
	return Rows.IsValidIndex(RowIndex) && Rows[RowIndex].bEnabled && Rows[RowIndex].Category != EPieMenuCategory::SectionHeader &&
		   !IsCornerHint(Rows[RowIndex].Category, bAtMainMenu);
}

int32 UBattleCommandMenuWidget::GetLitRow() const
{
	if (IsNavigableRow(LitRow))
	{
		return LitRow;
	}
	const UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	const AActor *Highlighted = Menu ? Menu->GetHighlightedTarget() : nullptr;
	for (int32 Index = 0; Highlighted && Index < Rows.Num(); ++Index)
	{
		if (GetRowTarget(Index) == Highlighted)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UBattleCommandMenuWidget::MoveLitRow(int32 Direction)
{
	const int32 Count = Rows.Num();
	if (!bShowing || Count == 0 || Direction == 0)
	{
		return;
	}
	const int32 Step = Direction > 0 ? 1 : -1;
	const int32 From = GetLitRow();
	int32 Index = From != INDEX_NONE ? From : (Step > 0 ? Count - 1 : 0); // unlit: the first step lands on the first / last
	for (int32 Tried = 0; Tried < Count; ++Tried)
	{
		Index = (Index + Step + Count) % Count;
		if (IsNavigableRow(Index))
		{
			LitRow = Index;
			HoverRow(Index); // a target row highlights its target: the marker and camera follow, as hover
			PushToWidgets();
			return;
		}
	}
}

bool UBattleCommandMenuWidget::PressLitRow()
{
	const int32 Lit = GetLitRow();
	if (Lit != INDEX_NONE)
	{
		PressRow(Lit);
		return true;
	}
	UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	return bShowing && Menu && Menu->ConfirmHighlightedTarget();
}

void UBattleCommandMenuWidget::ChangeLitRowValue(int32 Direction)
{
	UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	const int32 Lit = GetLitRow();
	if (!bShowing || !Menu || Direction == 0)
	{
		return;
	}
	const EPieMenuCategory Category = Rows.IsValidIndex(Lit) ? Rows[Lit].Category : EPieMenuCategory::None;
	if (Category == EPieMenuCategory::CycleLevel)
	{
		// The levels go round (0, 1, 2): back one is forward all but one.
		const int32 Presses = Direction > 0 ? 1 : InfusionConstants::MAX_INFUSION_LEVEL;
		for (int32 Press = 0; Press < Presses; ++Press)
		{
			PressRow(Lit);
		}
		return;
	}
	if (Category == EPieMenuCategory::CycleSource)
	{
		PressRow(Lit); // shortcut: A steps the source forward too (the sources have no previous step), add one if Crown asks
		return;
	}
	if (Lit == INDEX_NONE || GetRowTarget(Lit))
	{
		Menu->CycleHighlightedTarget(Direction); // as Q / E; nothing outside a single-target picker
	}
}

void UBattleCommandMenuWidget::HandleInputMethodChanged(ECommonInputType InputType)
{
	if (bShowing)
	{
		RefreshKeys();
		PushToWidgets();
	}
}

void UBattleCommandMenuWidget::RefreshDurability()
{
	const UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	DurabilityLines = BattleHUDView::MakeDurabilityLines(BattleHUDView::ReadDurability(Menu ? Menu->GetCurrentActor() : nullptr));
}

void UBattleCommandMenuWidget::HandleCrystalDurabilityChanged(AActor *Actor, UObject *Holder, int32 NewDurability, int32 MaxDurability)
{
	const UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	if (!bShowing || !Menu || Actor != Menu->GetCurrentActor())
	{
		return;
	}
	RefreshDurability();
	PushToWidgets();
}

void UBattleCommandMenuWidget::HandleCrystalBroken(AActor *Actor, UObject *Holder, FBrokenCrystalPayload Payload)
{
	// A break can switch the active ring (the manager switches first, then broadcasts), so re-read everything.
	HandleCrystalDurabilityChanged(Actor, Holder, 0, 0);
}

void UBattleCommandMenuWidget::PressRow(int32 RowIndex)
{
	if (!Rows.IsValidIndex(RowIndex))
	{
		return;
	}
	const FPieMenuButtonData Pressed = Rows[RowIndex]; // HandleSelection can rebroadcast and replace Rows
	if (!Pressed.bEnabled || Pressed.Category == EPieMenuCategory::SectionHeader)
	{
		return;
	}
	if (UCombatCommandMenuSubsystem *Menu = BoundMenu.Get())
	{
		Menu->HandleSelection(Pressed);
	}
}

void UBattleCommandMenuWidget::HoverRow(int32 RowIndex)
{
	AActor *Target = GetRowTarget(RowIndex);
	UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	if (Target && Menu && Rows[RowIndex].bEnabled)
	{
		Menu->HighlightTarget(Target); // ignored unless its open picker offers this target
	}
}

AActor *UBattleCommandMenuWidget::GetRowTarget(int32 RowIndex) const
{
	if (!Rows.IsValidIndex(RowIndex))
	{
		return nullptr;
	}
	// A per-actor target button; a group confirm carries Targets instead (as UCombatCommandButtonWidget, B-400).
	const FPieMenuButtonData &Row = Rows[RowIndex];
	return Row.Category == EPieMenuCategory::Target && Row.Targets.IsEmpty() ? Cast<AActor>(Row.DataReference) : nullptr;
}

void UBattleCommandMenuWidget::PushToWidgets()
{
	using namespace BattleCommandMenuStyle;
	SetVisibility(bShowing ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (TabText)
	{
		TabText->SetText(ActionsLine);
		WoRMenuStyle::SetShown(TabText->GetParent(), !ActionsLine.IsEmpty());
	}
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
	if (HeaderBlock)
	{
		HeaderBlock->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (DurabilityText)
	{
		DurabilityText->SetText(FText::Join(LOCTEXT("DurabilitySeparator", "   "), DurabilityLines));
		DurabilityText->SetVisibility(bShowing && !DurabilityLines.IsEmpty() ? ESlateVisibility::HitTestInvisible
																			  : ESlateVisibility::Collapsed);
	}
	const int32 Lit = GetLitRow(); // B-582b: W / S's row, else the highlighted target's
	for (int32 Index = 0; Index < RowButtons.Num(); ++Index)
	{
		UButton *Button = RowButtons[Index];
		UTextBlock *Label = RowTexts.IsValidIndex(Index) ? RowTexts[Index].Get() : nullptr;
		UBorder *Cap = RowKeyCaps.IsValidIndex(Index) ? RowKeyCaps[Index].Get() : nullptr;
		UTextBlock *KeyText = RowKeyTexts.IsValidIndex(Index) ? RowKeyTexts[Index].Get() : nullptr;
		UImage *Icon = RowIcons.IsValidIndex(Index) ? RowIcons[Index].Get() : nullptr;
		UTextBlock *Reason = RowReasonTexts.IsValidIndex(Index) ? RowReasonTexts[Index].Get() : nullptr;
		UTextBlock *Quantity = RowQuantityTexts.IsValidIndex(Index) ? RowQuantityTexts[Index].Get() : nullptr;
		UTextBlock *Cost = RowCostTexts.IsValidIndex(Index) ? RowCostTexts[Index].Get() : nullptr;
		if (!Button || !Label || !Cap || !KeyText || !Icon || !Reason || !Quantity || !Cost)
		{
			continue;
		}
		if (!bShowing || !Rows.IsValidIndex(Index) || IsCornerHint(Rows[Index].Category, bAtMainMenu))
		{
			Button->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const FPieMenuButtonData &Row = Rows[Index];
		const bool bFooter = BattleCommandMenuBuild::IsFooterRow(Row.Category);
		const bool bHighlighted = Index == Lit;
		Button->SetStyle(BattleCommandMenuBuild::RowStyle(bHighlighted, bFooter));

		const FKey Key = RowKeys.IsValidIndex(Index) ? RowKeys[Index] : FKey();
		WoRMenuStyle::SetShown(Cap->GetParent(), Key.IsValid());
		if (Key.IsValid())
		{
			Cap->SetBrush(BattleCommandMenuBuild::KeyCapBrush(Key));
			KeyText->SetText(BattleCommandMenuBuild::ShortKeyName(Key));
			KeyText->SetColorAndOpacity(FSlateColor(BattleCommandMenuBuild::KeyTextColor(Key)));
		}

		Icon->SetVisibility(Row.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Row.Icon)
		{
			Icon->SetBrushFromTexture(Row.Icon);
		}

		Label->SetText(BattleCommandMenuBuild::RowName(Row));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = bFooter ? HeaderFontSize : RowFontSize;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(!Row.bEnabled ? WoRMenuStyle::OffColor : bFooter ? WoRMenuStyle::DimColor : WoRMenuStyle::TextColor));

		// D-260: a greyed row says why under its name ("Needs 3 actions."); nothing else shows a description.
		const bool bShowReason = !Row.bEnabled && !bFooter && !Row.Description.IsEmpty();
		Reason->SetText(bShowReason ? Row.Description : FText::GetEmpty());
		Reason->SetVisibility(bShowReason ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		Quantity->SetText(FText::Format(LOCTEXT("ItemCount", "x{0}"), FText::AsNumber(Row.Quantity)));
		Quantity->SetVisibility(Row.Quantity > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		// D-260: the cost as pips at the right (a number past ActionPipsText::MAX_PIPS_SHOWN); none on the main menu.
		const bool bShowCost = !bAtMainMenu && Row.ActionCost > 0;
		const FText Pips = ActionPipsText::MakePips(Row.ActionCost, Row.ActionCost);
		Cost->SetText(Pips.IsEmpty() ? FText::AsNumber(Row.ActionCost) : Pips);
		Cost->SetColorAndOpacity(FSlateColor(Row.bEnabled ? WoRMenuStyle::CyanColor : WoRMenuStyle::OffColor));
		Cost->SetVisibility(bShowCost ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		Button->SetIsEnabled(Row.Category != EPieMenuCategory::SectionHeader && Row.bEnabled);
		Button->SetVisibility(ESlateVisibility::Visible);
	}
	if (HintsWidget)
	{
		HintsWidget->SetHints(this, bShowing ? Hints : TArray<FBattleMenuHint>());
	}
}

// ========================================
// DEBUG
// ========================================

FString UBattleCommandMenuWidget::GetCommandMenuString() const
{
	FString Out = FString::Printf(TEXT("[BattleCommandMenu] bound=%s showing=%s main=%s rows=%d tab=\"%s\" header=\"%s\" durability: %s"),
								  bBound ? TEXT("yes") : TEXT("no"), bShowing ? TEXT("yes") : TEXT("no"), bAtMainMenu ? TEXT("yes") : TEXT("no"),
								  Rows.Num(), *ActionsLine.ToString(), *Title.ToString(),
								  DurabilityLines.IsEmpty() ? TEXT("none")
															: *FText::Join(FText::FromString(TEXT(", ")), DurabilityLines).ToString());
	Out += FString::Printf(TEXT(" lit=%d"), GetLitRow()); // B-582b: W / S's row (-1: none)
	for (int32 Index = 0; Index < RowLabels.Num(); ++Index)
	{
		const bool bHint = Rows.IsValidIndex(Index) && IsCornerHint(Rows[Index].Category, bAtMainMenu);
		const FString Extra = !Rows.IsValidIndex(Index) ? FString()
							  : FString::Printf(TEXT("%s%s%s"), Rows[Index].Quantity > 0 ? *FString::Printf(TEXT(" x%d"), Rows[Index].Quantity) : TEXT(""),
												!bAtMainMenu && Rows[Index].ActionCost > 0 ? *FString::Printf(TEXT(" cost %d"), Rows[Index].ActionCost) : TEXT(""),
												Rows[Index].bEnabled ? TEXT("") : TEXT(" (greyed)"));
		Out += FString::Printf(TEXT("\n  [%d]%s %s%s"), Index, bHint ? TEXT(" hint") : TEXT(""), *RowLabels[Index].ToString(), *Extra);
	}
	return Out;
}

#undef LOCTEXT_NAMESPACE
