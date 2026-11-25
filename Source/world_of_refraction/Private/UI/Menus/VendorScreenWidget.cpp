// VendorScreenWidget.cpp

#include "UI/Menus/VendorScreenWidget.h"

#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Infusion/ElementColors.h"
#include "Inventory/InventoryConstants.h"
#include "Party/Party.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/LoadoutV2GridModel.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRIconTable.h"
#include "UI/WoRRootLayout.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRVendorScreenWidget"

namespace VendorScreenStyle
{
	// This screen's own layout (B-616: screen_mockups/hub/vendors/v2, the v5 three columns, at 1080p); the shared look is
	// WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float LeftWidth = 600.0f;
	constexpr float MiddleWidth = 618.0f;
	constexpr float RightWidth = 504.0f;
	constexpr float PanelGap = 27.0f;
	constexpr float PanelPadding = 24.0f;
	// B-608: compact rows (name, tag, tier, count and price on one line); buttons keep the roomier padding.
	constexpr float RowPaddingX = 15.0f;
	constexpr float RowPaddingY = 7.0f;
	constexpr float RowPadding = 15.0f;
	constexpr float ElementMarkWidth = 9.0f;
	constexpr float TierWidth = 60.0f;
	// B-487: icon sizes (B-608: the row icon smaller, for the one-line row).
	constexpr float RowIconSize = 28.0f;
	/** B-608: the row's "+" (Buy). */
	constexpr float AddButtonWidth = 42.0f;
	/** B-608: the cart's lines scroll past this height, so Total, After purchase and Buy stay in view. */
	constexpr float CartListMaxHeight = 190.0f; // TODO: tune in PIE (about six lines at 1080p)
	constexpr float TierBadgeSize = 28.0f;
	constexpr float CountWidth = 75.0f;
	// 20 marks must fit the row's words column (the left column less the icon, tier, count and "+": about 320).
	constexpr float PipSize = 12.0f;
	constexpr float PipGap = 4.0f;
	/** The Spiritualist has the most services (D-193). */
	constexpr int32 MaxServiceTabs = 6;
	/** Between two money chips on the top line. */
	const TCHAR *const ChipGap = TEXT("        ");
	// B-616b: the v2 tile grid, in the loadout grid's look.
	constexpr int32 GridColumns = WoRLoadoutV2Grid::COLUMNS;
	constexpr float TileSize = 93.0f;
	constexpr float TilePadding = 4.0f;
	constexpr float TileIconSize = 60.0f;
	constexpr float TileMarkInset = 6.0f;
	constexpr float FilterIconSize = 36.0f;
	constexpr float FilterPadding = 9.0f;
	constexpr float LockedTileOpacity = 0.35f;

	FLinearColor MarkColor(ESpellElement Element)
	{
		if (Element == ESpellElement::None)
		{
			return OffColor;
		}
		// Darkness is near-black: on a dark row it takes the pale outline colour instead (D-106).
		return ElementColors::NeedsTextOutline(Element) ? ElementColors::TextOutline : ElementColors::GetColorForElement(Element);
	}
}

UVendorScreenWidget::UVendorScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its rows and tabs are plain borders
}

UVendorScreenWidget *UVendorScreenWidget::Open(APlayerController *PC, EMerchantType InVendor, const UMerchantData *InMerchant)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[VendorScreen] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	if (WoRVendorScreen::GetTabs(InVendor).IsEmpty())
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[VendorScreen] The %s has no shop screen (D-193)"), *WoRGameNames::GetMerchantTypeName(InVendor).ToString());
		return nullptr;
	}
	UVendorScreenWidget *Screen = Cast<UVendorScreenWidget>(Root->PushToMenu(UVendorScreenWidget::StaticClass()));
	if (Screen)
	{
		Screen->SetVendor(InVendor, InMerchant, PC->GetPawn());
	}
	return Screen;
}

TSharedRef<SWidget> UVendorScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UVendorScreenWidget::BuildTree()
{
	using namespace VendorScreenStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(ScreenColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	// Top: the vendor and what it deals in, the money on the right.
	UHorizontalBox *Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox *Heading = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TitleText = MakeText(WidgetTree, TitleFontSize, TextColor, true);
	VendorLineText = MakeText(WidgetTree, SubtitleFontSize, DimColor);
	AddToColumn(Heading, TitleText);
	AddToColumn(Heading, VendorLineText);
	AddToRow(Header, Heading)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ChipsText = MakeText(WidgetTree, RowFontSize, TextColor, true);
	AddToRow(Header, ChipsText);
	AddToColumn(Main, Header);

	// The service tabs, then the character tabs.
	UHorizontalBox *ServiceRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(ServiceRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabPrev", "{TabPrev}")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (int32 Index = 0; Index < MaxServiceTabs; ++Index)
	{
		UTextBlock *Label = nullptr;
		ServiceTabs.Add(MakeTab(WidgetTree, Label));
		ServiceTabLabels.Add(Label);
		AddToRow(ServiceRow, ServiceTabs.Last(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	AddToRow(ServiceRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabNext", "{TabNext}")), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(Main, ServiceRow, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	UHorizontalBox *MemberRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(MemberRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabPrev", "{SubTabPrev}")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (int32 Index = 0; Index < PartyConstants::MAX_PARTY_MEMBERS; ++Index)
	{
		UTextBlock *Label = nullptr;
		MemberTabs.Add(MakeTab(WidgetTree, Label));
		MemberTabLabels.Add(Label);
		AddToRow(MemberRow, MemberTabs.Last(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	AddToRow(MemberRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabNext", "{SubTabNext}")), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(Main, MemberRow, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	MemberTabRow = MemberRow;

	// The rows, and the focused row's details.
	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body, FMargin(0.0f, SectionGap, 0.0f, SectionGap))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// The note above, then the rows; B-616b: on a grid tab the tile panel in the rows' place (the hidden one takes no room).
	UVerticalBox *Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	EmptyText = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(Left, EmptyText, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	RowScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	RowScroll->SetScrollBarVisibility(ESlateVisibility::Visible); // B-608: a long list shows its bar
	AddToColumn(Left, RowScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// B-616b: the tile panel: "Stock" and its count, the filters, the tiles, the legend.
	UBorder *GridFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	GridFrame->SetBrushColor(PanelColor);
	GridFrame->SetPadding(FMargin(PanelPadding));
	UVerticalBox *GridColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	GridFrame->SetContent(GridColumn);
	UHorizontalBox *GridHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	GridTitleText = MakeText(WidgetTree, RowFontSize, TextColor, true);
	GridCountText = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToRow(GridHeader, GridTitleText, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(GridHeader, GridCountText, FMargin(0.0f), VAlign_Center);
	AddToColumn(GridColumn, GridHeader, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	FilterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(GridColumn, FilterRow, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	GridScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	GridScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	GridPanel = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	GridPanel->SetSlotPadding(FMargin(TilePadding));
	GridScroll->AddChild(GridPanel);
	AddToColumn(GridColumn, GridScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox *Legend = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UHorizontalBox *CartKey = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *CartKeyText = MakeText(WidgetTree, SmallFontSize, DimColor);
	CartKeyText->SetText(LOCTEXT("LegendCart", "in the cart"));
	AddToRow(CartKey, MakeTileCorner(WidgetTree), FMargin(0.0f, 0.0f, WordGap, 0.0f), VAlign_Center);
	AddToRow(CartKey, CartKeyText, FMargin(0.0f, 0.0f, SectionGap, 0.0f), VAlign_Center);
	AddToRow(Legend, CartKey);
	LegendCart = CartKey;
	UTextBlock *LockKeyText = MakeText(WidgetTree, SmallFontSize, DimColor);
	LockKeyText->SetText(LOCTEXT("LegendLock", "locked: why is in the details"));
	AddToRow(Legend, MakeTileLock(WidgetTree), FMargin(0.0f, 0.0f, WordGap, 0.0f), VAlign_Center);
	AddToRow(Legend, LockKeyText, FMargin(0.0f), VAlign_Center);
	AddToColumn(GridColumn, Legend, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	GridColumnWidget = GridFrame;
	AddToColumn(Left, GridFrame)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Body, MakeSized(WidgetTree, LeftWidth, 0.0f, Left), FMargin(0.0f), VAlign_Fill);

	// B-616 middle: the focused thing, what it is and does, and the tab's rule.
	UBorder *Middle = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Middle->SetBrushColor(PanelColor);
	Middle->SetPadding(FMargin(PanelPadding));
	UVerticalBox *MiddleColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Middle->SetContent(MiddleColumn);
	DetailTitle = MakeWrappedText(WidgetTree, NameFontSize, TextColor, true);
	AddToColumn(MiddleColumn, DetailTitle, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	DetailKind = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(MiddleColumn, DetailKind, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	DetailLines = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(MiddleColumn, DetailLines, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	// B-487: the thing's effect sentence beside its effect icon.
	DetailEffectIcon = MakeIcon(WidgetTree, TierBadgeSize);
	DetailEffectText = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	UHorizontalBox *EffectRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(EffectRow, DetailEffectIcon, FMargin(0.0f, 0.0f, WordGap, 0.0f), VAlign_Top);
	AddToRow(EffectRow, DetailEffectText, FMargin(0.0f), VAlign_Top)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	DetailEffectRow = EffectRow;
	AddToColumn(MiddleColumn, EffectRow, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	SetShown(DetailEffectRow, false);
	RuleText = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(MiddleColumn, RuleText);
	AddToRow(Body, MakeSized(WidgetTree, MiddleWidth, 0.0f, Middle), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	// B-616 right, "Details": what the press costs or gives and what changes, the question, then (Buy) the cart.
	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	UVerticalBox *PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(PanelColumn);
	UTextBlock *DetailsHeading = MakeText(WidgetTree, RowFontSize, TextColor, true);
	DetailsHeading->SetText(LOCTEXT("DetailsHeading", "Details"));
	AddToColumn(PanelColumn, DetailsHeading, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	PayLabel = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, PayLabel);
	DetailPay = MakeWrappedText(WidgetTree, RowFontSize, TextColor, true);
	AddToColumn(PanelColumn, DetailPay, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	DetailChanges = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	AddToColumn(PanelColumn, DetailChanges, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	BuyForButton = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BuyForButton->SetBrushColor(PlateColor);
	BuyForButton->SetPadding(FMargin(RowPadding));
	BuyForText = MakeHintText(WidgetTree, LineFontSize, TextColor);
	BuyForButton->SetContent(BuyForText);
	AddToColumn(PanelColumn, BuyForButton, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	ConfirmText = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	AddToColumn(PanelColumn, ConfirmText, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	ConfirmHint = MakeHintBar(WidgetTree, LineFontSize, CyanColor, /*bBar=*/false); // B-483: caps, clickable; no bar (it sits in the page)
	WoRKeyHints::SetHint(ConfirmHint, LOCTEXT("ConfirmHint", "{Confirm}  Confirm        {Back}  Cancel"));
	AddToColumn(PanelColumn, ConfirmHint, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	WholeStackText = MakeHintText(WidgetTree, LineFontSize, CyanColor);
	AddToColumn(PanelColumn, WholeStackText, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	StatusText = MakeWrappedText(WidgetTree, LineFontSize, GoodColor);
	AddToColumn(PanelColumn, StatusText);
	DetailPanel = MakeSized(WidgetTree, RightWidth, 0.0f, Panel);

	// Buy: the cart, under the details.
	UBorder *CartFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CartFrame->SetBrushColor(PanelColor);
	CartFrame->SetPadding(FMargin(PanelPadding));
	UVerticalBox *CartColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	CartFrame->SetContent(CartColumn);
	UTextBlock *CartHeading = MakeText(WidgetTree, RowFontSize, DimColor, true);
	CartHeading->SetText(LOCTEXT("CartHeading", "Cart"));
	AddToColumn(CartColumn, CartHeading, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	// B-608: the lines scroll in a box of their own; Total, After purchase and Buy stay under it.
	CartLinesText = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	UScrollBox *CartScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	CartScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	CartScroll->AddChild(CartLinesText);
	USizeBox *CartListBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	CartListBox->SetMaxDesiredHeight(CartListMaxHeight);
	CartListBox->SetContent(CartScroll);
	AddToColumn(CartColumn, CartListBox, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	CartTotalText = MakeWrappedText(WidgetTree, LineFontSize, TextColor, true);
	AddToColumn(CartColumn, CartTotalText, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	PurchaseButton = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PurchaseButton->SetPadding(FMargin(RowPadding));
	PurchaseButton->SetHorizontalAlignment(HAlign_Center);
	PurchaseText = MakeHintText(WidgetTree, RowFontSize, TextColor, true);
	WoRKeyHints::SetHint(PurchaseText, LOCTEXT("Purchase", "{Toggle}  Buy"));
	PurchaseButton->SetContent(PurchaseText);
	AddToColumn(CartColumn, PurchaseButton);
	CartPanel = MakeSized(WidgetTree, RightWidth, 0.0f, CartFrame);

	// B-608: the side fills the body's height and no more: the details scroll in what the cart leaves, so nothing runs
	// over the hint bar.
	UVerticalBox *Side = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	DetailScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	DetailScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	DetailScroll->AddChild(DetailPanel);
	AddToColumn(Side, DetailScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToColumn(Side, CartPanel, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToRow(Body, Side, FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	HintsText = MakeHintBar(WidgetTree); // B-481 / B-483: key caps of the device in use, each clickable
	AddToColumn(Main, HintsText)->SetHorizontalAlignment(HAlign_Right);
}

UVendorScreenWidget::FRowWidgets UVendorScreenWidget::BuildRow()
{
	using namespace VendorScreenStyle;

	FRowWidgets Widgets;
	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(RowPaddingX, RowPaddingY));
	UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Line);

	UBorder *ElementMark = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	AddToRow(Line, MakeSized(WidgetTree, ElementMarkWidth, 0.0f, ElementMark), FMargin(0.0f, 0.0f, RowPaddingX, 0.0f), VAlign_Fill);
	UImage *Icon = MakeIcon(WidgetTree, RowIconSize); // B-487: a weapon's type, a crystal's element diamond
	AddToRow(Line, Icon, FMargin(0.0f, 0.0f, RowPaddingX, 0.0f), VAlign_Center);
	// B-608: the name, then its kind (or why it's refused: "Already known") as a small tag on the same line.
	UVerticalBox *Words = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox *NameLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
	UTextBlock *Sub = MakeText(WidgetTree, SmallFontSize, DimColor);
	AddToRow(NameLine, Name, FMargin(0.0f, 0.0f, WordGap, 0.0f), VAlign_Center);
	AddToRow(NameLine, Sub, FMargin(0.0f), VAlign_Center);
	AddToColumn(Words, NameLine);
	UHorizontalBox *PipRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 Index = 0; Index < InventoryConstants::ITEM_SLOTS_MAX; ++Index)
	{
		UBorder *Pip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		AddToRow(PipRow, MakeSized(WidgetTree, PipSize, PipSize, Pip), FMargin(0.0f, 0.0f, PipGap, 0.0f));
		Widgets.Pips.Add(Pip);
	}
	AddToColumn(Words, PipRow, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	Widgets.PipRow = PipRow;
	AddToRow(Line, Words, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock *Tier = MakeText(WidgetTree, LineFontSize, DimColor, true);
	UImage *TierBadge = MakeIcon(WidgetTree, TierBadgeSize); // B-487: in the tier letter's place when the icon table has it
	UHorizontalBox *TierCell = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(TierCell, TierBadge);
	AddToRow(TierCell, Tier);
	AddToRow(Line, MakeSized(WidgetTree, TierWidth, 0.0f, TierCell), FMargin(0.0f), VAlign_Center);
	UTextBlock *Count = MakeText(WidgetTree, LineFontSize, CyanColor, true);
	// B-616: no price column; the price is in Details (mockup v2).
	AddToRow(Line, MakeSized(WidgetTree, CountWidth, 0.0f, Count), FMargin(0.0f, 0.0f, RowPaddingX, 0.0f), VAlign_Center);
	UBorder *AddButton = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	AddButton->SetBrushColor(PlateColor);
	AddButton->SetHorizontalAlignment(HAlign_Center);
	UTextBlock *Plus = MakeText(WidgetTree, RowFontSize, TextColor, true);
	Plus->SetText(LOCTEXT("RowAdd", "+"));
	AddButton->SetContent(Plus);
	AddToRow(Line, MakeSized(WidgetTree, AddButtonWidth, 0.0f, AddButton), FMargin(0.0f), VAlign_Center);

	if (UScrollBoxSlot *RowSlot = Cast<UScrollBoxSlot>(RowScroll->AddChild(Frame)))
	{
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, LineGap));
	}
	Widgets.Frame = Frame;
	Widgets.ElementMark = ElementMark;
	Widgets.Name = Name;
	Widgets.Sub = Sub;
	Widgets.Tier = Tier;
	Widgets.Count = Count;
	Widgets.Icon = Icon;
	Widgets.TierBadge = TierBadge;
	Widgets.AddButton = AddButton;
	return Widgets;
}

UVendorScreenWidget::FTileWidgets UVendorScreenWidget::BuildTile(int32 Index)
{
	using namespace VendorScreenStyle;

	FTileWidgets Widgets;
	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	UOverlay *Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Frame->SetContent(MakeSized(WidgetTree, TileSize, TileSize, Layers));
	const auto AddLayer = [Layers](UWidget *Layer, EHorizontalAlignment HAlign, EVerticalAlignment VAlign, float Inset)
	{
		UOverlaySlot *LayerSlot = Layers->AddChildToOverlay(Layer);
		LayerSlot->SetHorizontalAlignment(HAlign);
		LayerSlot->SetVerticalAlignment(VAlign);
		LayerSlot->SetPadding(FMargin(Inset));
	};
	UImage *Icon = MakeIcon(WidgetTree, TileIconSize);
	UImage *Centre = nullptr;
	AddLayer(MakeIconStack(WidgetTree, Icon, TileIconSize, Centre), HAlign_Center, VAlign_Center, 0.0f);
	UTextBlock *Word = MakeText(WidgetTree, SmallFontSize, DimColor);
	Word->SetAutoWrapText(true); // a thing with no picture shows its name, which may be two words
	Word->SetJustification(ETextJustify::Center);
	AddLayer(Word, HAlign_Center, VAlign_Center, TileMarkInset);
	UImage *TierBadge = MakeIcon(WidgetTree, TierBadgeSize);
	AddLayer(TierBadge, HAlign_Left, VAlign_Bottom, TileMarkInset);
	UTextBlock *TierText = MakeText(WidgetTree, SmallFontSize, DimColor, true);
	AddLayer(TierText, HAlign_Left, VAlign_Bottom, TileMarkInset);
	UTextBlock *Count = MakeText(WidgetTree, SmallFontSize, TextColor, true);
	AddLayer(Count, HAlign_Right, VAlign_Bottom, TileMarkInset);
	UWidget *InCart = MakeTileCorner(WidgetTree);
	AddLayer(InCart, HAlign_Right, VAlign_Top, 0.0f);
	UWidget *Lock = MakeTileLock(WidgetTree); // a refused line never sits in the cart: the corner is free
	AddLayer(Lock, HAlign_Right, VAlign_Top, TileMarkInset);
	GridPanel->AddChildToUniformGrid(Frame, Index / GridColumns, Index % GridColumns);

	Widgets.Frame = Frame;
	Widgets.Icon = Icon;
	Widgets.IconCentre = Centre;
	Widgets.Word = Word;
	Widgets.TierBadge = TierBadge;
	Widgets.TierText = TierText;
	Widgets.Count = Count;
	Widgets.InCart = InCart;
	Widgets.Lock = Lock;
	return Widgets;
}

void UVendorScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	ResetState();
}

void UVendorScreenWidget::SetVendor(EMerchantType InVendor, const UMerchantData *InMerchant, AActor *InPayer)
{
	Vendor = InVendor;
	Merchant = InMerchant ? InMerchant : WoRVendorScreen::FindMerchant(GetWorld(), InVendor);
	Payer = InPayer ? InPayer : GetOwningPlayerPawn();
	ResetState();
	UE_LOG(LogWoRUI, Log, TEXT("[VendorScreen] Showing the %s (%d member(s))"), *WoRGameNames::GetMerchantTypeName(Vendor).ToString(), Members.Num());
}

void UVendorScreenWidget::ResetState()
{
	Tabs = WoRVendorScreen::GetTabs(Vendor);
	TabIndex = FMath::Max(0, Tabs.IndexOfByPredicate([](EVendorScreenTab Tab) { return WoRVendorScreen::IsBuilt(Tab); }));
	MemberIndex = 0;
	SelectedRow = 0;
	bConfirming = false;
	bConfirmingAll = false;
	bWholeStack = false; // the stack holds a pooled screen: nothing carries over from the last open
	MergeTargets.Reset();
	UpgradeTargets.Reset();
	Cart.Reset();
	Filters.Reset();
	bOnFilters = false;
	Status = FText::GetEmpty();
	RefreshData();
	RefreshSelection();
}

bool UVendorScreenWidget::NativeOnHandleBackAction()
{
	if (IsOnFilters() && !bConfirming && !bConfirmingAll)
	{
		MoveInGrid(0, 1); // B-616b: Back on the filters goes down to the tiles, not out of the shop
		return true;
	}
	if (!bConfirming && !bConfirmingAll)
	{
		return Super::NativeOnHandleBackAction();
	}
	bConfirming = false;
	bConfirmingAll = false;
	RefreshSelection();
	return true;
}

UWidget *UVendorScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UVendorScreenWidget *>(this);
}

FName UVendorScreenWidget::MemberKey() const
{
	return Members.IsValidIndex(MemberIndex) ? Members[MemberIndex].Key : NAME_None;
}

const FVendorScreenRow *UVendorScreenWidget::FocusedRow() const
{
	return Page.Rows.IsValidIndex(SelectedRow) ? &Page.Rows[SelectedRow] : nullptr;
}

void UVendorScreenWidget::RefreshData()
{
	using namespace VendorScreenStyle;

	UWorld *World = GetWorld();
	Members = WoRLoadoutScreen::BuildMembers(World);
	MemberIndex = Members.Num() > 0 ? FMath::Clamp(MemberIndex, 0, Members.Num() - 1) : 0;
	const bool bBuy = IsBuy();
	const bool bMerge = GetTab() == EVendorScreenTab::Merge; // its Row.Item is the target, not a stack
	switch (GetTab())
	{
	case EVendorScreenTab::Buy:
		Page = WoRVendorScreen::BuildBuyPage(World, Merchant.Get(), Payer.Get(), MemberKey(), Cart);
		break;
	case EVendorScreenTab::ItemSlots:
		Page = WoRVendorScreen::BuildItemSlotsPage(World, Payer.Get());
		break;
	case EVendorScreenTab::Repair:
		Page = WoRVendorScreen::BuildRepairPage(World, Payer.Get());
		break;
	case EVendorScreenTab::Curses:
		Page = WoRVendorScreen::BuildCursesPage(World, Payer.Get());
		break;
	case EVendorScreenTab::Merge:
		Page = WoRVendorScreen::BuildMergePage(World, MemberKey(), MergeTargets);
		break;
	case EVendorScreenTab::Upgrade:
		Page = WoRVendorScreen::BuildUpgradePage(World, MemberKey(), UpgradeTargets);
		break;
	default:
		Page = WoRVendorScreen::BuildPage(World, Vendor, GetTab(), MemberKey());
		break;
	}
	SelectedRow = Page.Rows.Num() > 0 ? FMath::Clamp(SelectedRow, 0, Page.Rows.Num() - 1) : 0;
	// B-616b: the filters are this page's kinds; the picked one stays while its kind is still here, else All.
	const FVendorGridFilter Picked = Filters.IsValidIndex(FilterIndex) ? Filters[FilterIndex] : FVendorGridFilter();
	Filters = IsTileGrid() ? WoRVendorScreen::GetGridFilters(Page.Rows) : TArray<FVendorGridFilter>();
	FilterIndex = FMath::Max(0, Filters.IndexOfByKey(Picked));
	bOnFilters = bOnFilters && Filters.Num() > 1;
	UpdateShownRows();

	if (!TitleText || !RowScroll)
	{
		return; // headless: no tree
	}
	RefreshCart();
	SetShown(MemberTabRow, HasMemberTabs());
	TitleText->SetText(WoRGameNames::GetMerchantTypeName(Vendor));
	VendorLineText->SetText(WoRVendorScreen::GetVendorLine(Vendor));
	TArray<FText> Chips;
	for (const FPauseMenuChip &Chip : WoRVendorScreen::BuildChips(World))
	{
		Chips.Add(FText::Format(LOCTEXT("Chip", "{0} {1}"), Chip.Value, Chip.Label));
	}
	ChipsText->SetText(FText::Join(FText::FromString(ChipGap), Chips));

	for (int32 Index = 0; Index < ServiceTabs.Num(); ++Index)
	{
		const bool bShown = Tabs.IsValidIndex(Index);
		SetShown(ServiceTabs[Index], bShown);
		if (bShown)
		{
			const bool bLit = Index == TabIndex;
			ServiceTabLabels[Index]->SetText(WoRVendorScreen::GetTabName(Tabs[Index]));
			ServiceTabs[Index]->SetBrushColor(bLit ? FocusColor : PlateColor);
			ServiceTabLabels[Index]->SetColorAndOpacity(FSlateColor(bLit ? TextColor : WoRVendorScreen::IsBuilt(Tabs[Index]) ? DimColor : OffColor));
		}
	}
	for (int32 Index = 0; Index < MemberTabs.Num(); ++Index)
	{
		const bool bShown = Members.IsValidIndex(Index);
		SetShown(MemberTabs[Index], bShown);
		if (bShown)
		{
			MemberTabLabels[Index]->SetText(Members[Index].Name);
			MemberTabs[Index]->SetBrushColor(Index == MemberIndex ? FocusColor : PlateColor);
			MemberTabLabels[Index]->SetColorAndOpacity(FSlateColor(Index == MemberIndex ? TextColor : DimColor));
		}
	}
	const FText AboveRows = Page.EmptyNote.IsEmpty() ? Page.Intro : Page.EmptyNote;
	EmptyText->SetText(AboveRows);
	SetShown(EmptyText, !AboveRows.IsEmpty());
	FText Hints;
	switch (GetTab())
	{
	case EVendorScreenTab::Buy:
		Hints = LOCTEXT("HintsBuy", "{Arrows}  Move        {Confirm}  Add        {Second}  Take one back        {SubTabs}  For        {Toggle}  Buy        {Tabs}  Service        {Back}  Leave");
		break;
	case EVendorScreenTab::ItemSlots:
		Hints = LOCTEXT("HintsItemSlots", "{Tabs}  Service        {Confirm}  Buy        {Back}  Leave");
		break;
	case EVendorScreenTab::Repair:
		Hints = LOCTEXT("HintsRepair", "{Tabs}  Service        {Confirm}  Repair this        {Toggle}  Repair all        {Back}  Leave");
		break;
	case EVendorScreenTab::Curses:
		Hints = LOCTEXT("HintsCurses", "{Tabs}  Service        {Confirm}  Clear        {Back}  Leave");
		break;
	case EVendorScreenTab::Merge:
		Hints = LOCTEXT("HintsMerge", "{SubTabs}  Character        {Tabs}  Service        Left / Right  Tier        {Confirm}  Merge        {Back}  Leave");
		break;
	case EVendorScreenTab::Upgrade:
		Hints = LOCTEXT("HintsUpgrade", "{SubTabs}  Character        {Tabs}  Service        {Second}  Tier        {Confirm}  Upgrade        {Back}  Leave");
		break;
	default:
		Hints = FText::FormatNamed(LOCTEXT("Hints", "{SubTabs}  Character        {Tabs}  Service        {Confirm}  {Verb}        {Toggle}  Whole stack        {Back}  Leave"), TEXT("Verb"),
							  WoRVendorScreen::GetTabName(GetTab()));
		break;
	}
	if (IsOnFilters())
	{
		Hints = LOCTEXT("HintsFilters", "{Arrows}  Filter        {Confirm}  To the tiles        {Back}  Back"); // B-616b
	}
	WoRKeyHints::SetHint(HintsText, Hints);
	SetShown(RowScroll, !IsTileGrid());
	SetShown(GridColumnWidget, IsTileGrid());
	RefreshGrid();

	while (RowWidgets.Num() < Page.Rows.Num())
	{
		RowWidgets.Add(BuildRow());
	}
	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		const FRowWidgets &Widgets = RowWidgets[Index];
		const bool bShown = Page.Rows.IsValidIndex(Index);
		SetShown(Widgets.Frame.Get(), bShown);
		if (!bShown || !Widgets.ElementMark.IsValid() || !Widgets.Name.IsValid() || !Widgets.Sub.IsValid() || !Widgets.PipRow.IsValid() ||
			!Widgets.Tier.IsValid() || !Widgets.Count.IsValid())
		{
			continue;
		}
		const FVendorScreenRow &Row = Page.Rows[Index];
		const bool bRefused = !Row.Refusal.IsEmpty();
		Widgets.ElementMark->SetBrushColor(MarkColor(Row.Element));
		Widgets.Name->SetText(Row.Name);
		Widgets.Name->SetColorAndOpacity(FSlateColor(bRefused ? OffColor : TextColor));
		Widgets.Sub->SetText(bRefused ? Row.Refusal : Row.KindLine);
		Widgets.Sub->SetColorAndOpacity(FSlateColor(bRefused ? WarnColor : DimColor));
		SetShown(Widgets.PipRow.Get(), Row.SlotsMax > 0);
		for (int32 Pip = 0; Pip < Widgets.Pips.Num(); ++Pip)
		{
			if (UBorder *Mark = Widgets.Pips[Pip].Get())
			{
				Mark->SetBrushColor(Pip < Row.Slots ? CyanColor : Pip < Row.SlotsMax ? TrackColor : FLinearColor::Transparent);
			}
		}
		// B-487: the row's icon, and its tier as a badge when the icon table has it (else the letter, as before).
		SetIcon(Widgets.Icon.Get(), WoRIcons::Find(Row.Icon));
		const bool bTierBadge = SetIcon(Widgets.TierBadge.Get(), Row.bTierBadge ? WoRIcons::GetTierBadge(Row.TierValue) : nullptr);
		Widgets.Tier->SetText(bTierBadge ? FText::GetEmpty() : Row.Tier);
		// Sell and Dismantle: the stack's size. Buy: how many the cart holds, and a refused line still shows its price.
		// Repair: how worn the piece is.
		Widgets.Count->SetText(!Row.State.IsEmpty()						  ? Row.State
							   : Row.Row.Count > (bBuy ? 0 : 1) && !bMerge ? FText::Format(LOCTEXT("StackCount", "x{0}"), FText::AsNumber(Row.Row.Count))
																		   : FText::GetEmpty());
		Widgets.Count->SetColorAndOpacity(FSlateColor(Row.bStateIsBad ? WarnColor : CyanColor));
		// B-608: the "+" only on Buy; a refused line keeps its place (the columns stay lined up) but shows none, and
		// a Hidden one takes no click.
		if (UBorder *AddButton = Widgets.AddButton.Get())
		{
			AddButton->SetVisibility(!bBuy ? ESlateVisibility::Collapsed : bRefused ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
		}
	}
}

void UVendorScreenWidget::UpdateShownRows()
{
	ShownRows.Reset();
	const FVendorGridFilter Filter = Filters.IsValidIndex(FilterIndex) ? Filters[FilterIndex] : FVendorGridFilter();
	for (int32 Index = 0; Index < Page.Rows.Num(); ++Index)
	{
		if (WoRVendorScreen::PassesFilter(Page.Rows[Index], Filter))
		{
			ShownRows.Add(Index);
		}
	}
	if (!ShownRows.IsEmpty() && !ShownRows.Contains(SelectedRow))
	{
		SelectedRow = ShownRows[0];
	}
}

void UVendorScreenWidget::RefreshGrid()
{
	using namespace VendorScreenStyle;

	if (!GridPanel || !IsTileGrid())
	{
		return; // headless, or the rows' tab
	}
	const bool bBuy = IsBuy();
	GridTitleText->SetText(WoRVendorScreen::GetGridTitle(GetTab()));
	GridCountText->SetText(WoRVendorScreen::GetGridCount(Vendor, GetTab(), ShownRows.Num()));

	// The filters, made as they are first needed: All, then each kind on the page (a picture where the icon table has one).
	SetShown(FilterRow, Filters.Num() > 1);
	while (FilterTabs.Num() < Filters.Num())
	{
		UBorder *Tab = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Tab->SetPadding(FMargin(FilterPadding));
		UHorizontalBox *Content = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Tab->SetContent(Content);
		UImage *Icon = MakeIcon(WidgetTree, FilterIconSize);
		UTextBlock *Label = MakeText(WidgetTree, SmallFontSize, TextColor, true);
		AddToRow(Content, Icon, FMargin(0.0f), VAlign_Center);
		AddToRow(Content, Label, FMargin(0.0f), VAlign_Center);
		AddToRow(FilterRow, Tab, FMargin(0.0f, 0.0f, TabGap, 0.0f));
		FilterTabs.Add(Tab);
		FilterIcons.Add(Icon);
		FilterLabels.Add(Label);
	}
	for (int32 Index = 0; Index < FilterTabs.Num(); ++Index)
	{
		const bool bShown = Filters.IsValidIndex(Index);
		SetShown(FilterTabs[Index], bShown);
		if (!bShown)
		{
			continue;
		}
		FilterTabs[Index]->SetBrushColor(Index == FilterIndex ? FocusColor : PlateColor);
		const FWoRIcon Icon = WoRVendorScreen::GetFilterIcon(Filters[Index]);
		const bool bPicture = SetIcon(FilterIcons[Index], WoRIcons::Find(Icon));
		FilterLabels[Index]->SetText(Icon.Name);
		SetShown(FilterLabels[Index], !bPicture);
	}

	// The tiles, made as they are first needed and refilled after.
	while (TileWidgets.Num() < ShownRows.Num())
	{
		TileWidgets.Add(BuildTile(TileWidgets.Num()));
	}
	for (int32 Index = 0; Index < TileWidgets.Num(); ++Index)
	{
		const FTileWidgets &Tile = TileWidgets[Index];
		const bool bShown = ShownRows.IsValidIndex(Index);
		SetShown(Tile.Frame.Get(), bShown);
		if (!bShown || !Tile.Icon.IsValid() || !Tile.Word.IsValid() || !Tile.TierText.IsValid() || !Tile.Count.IsValid())
		{
			continue;
		}
		const FVendorScreenRow &Row = Page.Rows[ShownRows[Index]];
		const bool bRefused = !Row.Refusal.IsEmpty();
		const bool bPicture = SetIcon(Tile.Icon.Get(), WoRIcons::Find(Row.Icon));
		SetIcon(Tile.IconCentre.Get(), bPicture ? WoRIcons::FindCentre(Row.Icon) : nullptr);
		Tile.Icon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, bRefused ? LockedTileOpacity : 1.0f));
		Tile.Word->SetText(Row.Name);
		Tile.Word->SetColorAndOpacity(FSlateColor(bRefused ? OffColor : DimColor));
		SetShown(Tile.Word.Get(), !bPicture);
		// The tier as its badge where the row has one, else in letters (Upgrade: "F → E").
		const bool bBadge = SetIcon(Tile.TierBadge.Get(), Row.bTierBadge ? WoRIcons::GetTierBadge(Row.TierValue) : nullptr);
		Tile.TierText->SetText(Row.Tier);
		SetShown(Tile.TierText.Get(), !bBadge && !Row.Tier.IsEmpty());
		// Buy: how many the cart holds (the violet corner); elsewhere the stack's size.
		Tile.Count->SetText(FText::Format(LOCTEXT("TileCount", "x{0}"), FText::AsNumber(Row.Row.Count)));
		SetShown(Tile.Count.Get(), Row.Row.Count > 1);
		SetShown(Tile.InCart.Get(), bBuy && Row.Row.Count > 0);
		SetShown(Tile.Lock.Get(), bRefused);
	}
	SetShown(LegendCart, bBuy);
}

int32 UVendorScreenWidget::FindTileUnder(const FPointerEvent &MouseEvent) const
{
	const int32 Tile = WoRMenuStyle::FindScrolledRowUnder(GridScroll, FMath::Min(TileWidgets.Num(), ShownRows.Num()),
														  [this](int32 Index) -> const UWidget * { return TileWidgets[Index].Frame.Get(); },
														  MouseEvent.GetScreenSpacePosition());
	return Tile == INDEX_NONE ? INDEX_NONE : ShownRows[Tile];
}

void UVendorScreenWidget::MoveInGrid(int32 DeltaX, int32 DeltaY)
{
	using VendorScreenStyle::GridColumns;
	if (!IsTileGrid())
	{
		MoveRow(DeltaY); // a list: Up / Down only
		return;
	}
	if (bOnFilters)
	{
		if (DeltaX != 0)
		{
			PickFilter(((FilterIndex + DeltaX) % Filters.Num() + Filters.Num()) % Filters.Num());
		}
		else if (DeltaY > 0)
		{
			bOnFilters = false; // back down to the tiles
			RefreshData();
			RefreshSelection();
		}
		return;
	}
	const int32 At = ShownRows.IndexOfByKey(SelectedRow);
	if (DeltaY < 0 && At != INDEX_NONE && At < GridColumns && Filters.Num() > 1)
	{
		bOnFilters = true; // Up off the top row
		bConfirming = false;
		RefreshData();
		RefreshSelection();
		return;
	}
	const int32 To = WoRLoadoutV2Grid::Move(At, ShownRows.Num(), GridColumns, DeltaX, DeltaY);
	if (ShownRows.IsValidIndex(To))
	{
		SelectRow(ShownRows[To]);
	}
}

void UVendorScreenWidget::PickFilter(int32 Index)
{
	if (!Filters.IsValidIndex(Index))
	{
		return;
	}
	FilterIndex = Index;
	bConfirming = false;
	Status = FText::GetEmpty();
	UpdateShownRows();
	if (!ShownRows.IsEmpty())
	{
		SelectedRow = ShownRows[0];
	}
	RefreshData();
	RefreshSelection();
}

void UVendorScreenWidget::PressSecond()
{
	if (IsBuy())
	{
		RemoveFocusedFromCart();
	}
	else if (GetTab() == EVendorScreenTab::Upgrade)
	{
		MoveUpgradeTarget(1);
	}
}

void UVendorScreenWidget::RefreshSelection(bool bScrollIntoView)
{
	using namespace VendorScreenStyle;

	const FVendorScreenRow *Focused = FocusedRow();
	if (!Focused)
	{
		bConfirming = false;
	}
	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		if (UBorder *Frame = RowWidgets[Index].Frame.Get())
		{
			Frame->SetBrushColor(Focused && Index == SelectedRow ? FocusColor : PlateColor);
		}
	}
	// B-616b: on the filters no tile is lit (the keys are up there).
	for (int32 Index = 0; Index < TileWidgets.Num(); ++Index)
	{
		if (UBorder *Frame = TileWidgets[Index].Frame.Get())
		{
			const bool bLit = Focused && !IsOnFilters() && ShownRows.IsValidIndex(Index) && ShownRows[Index] == SelectedRow;
			Frame->SetBrushColor(bLit ? FocusColor : PlateColor);
		}
	}
	if (!DetailTitle)
	{
		return; // headless: no tree
	}
	SetShown(DetailPanel, Focused != nullptr || !Status.IsEmpty() || bConfirmingAll);
	const bool bBuy = IsBuy();
	const bool bSlots = IsItemSlots();
	const bool bSpirit = IsSpiritualistService();
	const FVendorScreenRow Row = Focused ? *Focused : FVendorScreenRow();
	const int32 Count = bBuy || bSpirit ? 1 : WoRVendorScreen::GetTakeCount(Row, bWholeStack); // Buy's Row.Count is the cart's, not a stack
	const FText What = Row.Tier.IsEmpty() ? Row.Name : FText::Format(LOCTEXT("NameAndTier", "{0} ({1})"), Row.Name, Row.Tier);
	FText Title;
	switch (GetTab())
	{
	case EVendorScreenTab::Buy:
		Title = What;
		break;
	case EVendorScreenTab::ItemSlots:
		Title = LOCTEXT("DetailItemSlot", "Buy slot");
		break;
	case EVendorScreenTab::Curses:
		Title = FText::Format(LOCTEXT("DetailCurse", "Clear {0}"), Row.Name);
		break;
	case EVendorScreenTab::Merge:
		Title = FText::Format(LOCTEXT("DetailMerge", "Merge into {0}"), What);
		break;
	default:
		Title = FText::Format(LOCTEXT("DetailTitle", "{0} {1}"), WoRVendorScreen::GetTabName(GetTab()), What);
		break;
	}
	DetailTitle->SetText(Focused ? Title : FText::GetEmpty());
	PayLabel->SetText(bBuy || bSlots || bSpirit ? LOCTEXT("Price", "Price") : LOCTEXT("YouGet", "You get"));
	SetShown(PayLabel, Focused != nullptr);
	DetailPay->SetText(!Row.Refusal.IsEmpty() ? Row.Refusal : Count > 1 ? FText::Format(LOCTEXT("PayEach", "{0} each, for {1}"), Row.Pay, FText::AsNumber(Count)) : Row.Pay);
	DetailPay->SetColorAndOpacity(FSlateColor(Row.Refusal.IsEmpty() ? TextColor : WarnColor));

	// Buy: what the thing is and does, and who it is bought for. Item slots: the pouch now and after.
	TArray<FText> Lines;
	// B-487: with an effect icon in the icon table, the effect sentence gets its own line beside it; else it stays the
	// last of the lines, as before.
	UTexture2D *EffectIcon = (bBuy || bSlots || bSpirit) && Focused && !Row.Note.IsEmpty() ? WoRIcons::Find(Row.EffectIcon) : nullptr;
	if ((bBuy || bSlots || bSpirit) && Focused)
	{
		if ((bBuy || bSpirit) && !Row.Refusal.IsEmpty() && !Row.Pay.IsEmpty())
		{
			Lines.Add(Row.Pay); // the reason took the price's place
		}
		for (const FLoadoutScreenDetail &Detail : Row.Details)
		{
			Lines.Add(Detail.Label.IsEmpty() ? Detail.Value : FText::Format(LOCTEXT("DetailLine", "{0}:  {1}"), Detail.Label, Detail.Value));
		}
		if (!Row.Note.IsEmpty() && !EffectIcon)
		{
			Lines.Add(Row.Note);
		}
	}
	DetailLines->SetText(FText::Join(FText::FromString(TEXT("\n")), Lines));
	SetShown(DetailLines, !Lines.IsEmpty());
	DetailKind->SetText(Focused ? Row.KindLine : FText::GetEmpty());
	SetShown(DetailKind, Focused && !Row.KindLine.IsEmpty());
	// B-616: what the press changes, "Tier:  C → A".
	TArray<FText> Changes;
	for (const FVendorScreenChange &Change : Row.Changes)
	{
		Changes.Add(FText::Format(LOCTEXT("ChangeLine", "{0}:  {1}  →  {2}"), Change.Label, Change.Old, Change.New));
	}
	DetailChanges->SetText(FText::Join(FText::FromString(TEXT("\n")), Changes));
	SetShown(DetailChanges, Focused && !Changes.IsEmpty());
	SetIcon(DetailEffectIcon, EffectIcon);
	DetailEffectText->SetText(EffectIcon ? Row.Note : FText::GetEmpty());
	SetShown(DetailEffectRow, EffectIcon != nullptr);
	const bool bBuyFor = bBuy && Members.IsValidIndex(MemberIndex);
	SetShown(BuyForButton, bBuyFor);
	if (bBuyFor)
	{
		WoRKeyHints::SetHint(BuyForText, FText::FormatNamed(LOCTEXT("BuyingFor", "{SubTabs}  For:  {Who}"), TEXT("Who"), Members[MemberIndex].Name));
	}

	EVendorService Service = EVendorService::Sell;
	const bool bService = WoRVendorScreen::GetService(GetTab(), Service);
	const bool bAsking = bConfirmingAll || (bConfirming && Focused && (bSlots || bSpirit || bService));
	ConfirmText->SetText(!bAsking		  ? FText::GetEmpty()
						 : bConfirmingAll ? WoRVendorScreen::GetRepairAllConfirmText(Page)
						 : bSlots		  ? WoRVendorScreen::GetItemSlotConfirmText(Row)
						 : bSpirit		  ? WoRVendorScreen::GetConfirmText(GetTab(), Row)
										  : WoRVendorServices::GetConfirmText(GetWorld(), Service, MemberKey(), Row.Row));
	SetShown(ConfirmText, bAsking);
	SetShown(ConfirmHint, bAsking);
	// Sell and Dismantle: the whole-stack toggle. Repair: X repairs everything.
	const bool bRepair = GetTab() == EVendorScreenTab::Repair;
	const bool bStack = Focused && bService && Row.Row.Count > 1;
	SetShown(WholeStackText, bStack || (bRepair && !Page.Rows.IsEmpty()));
	if (bRepair)
	{
		WoRKeyHints::SetHint(WholeStackText, Page.AllRefusal.IsEmpty() ? FText::FormatNamed(LOCTEXT("RepairAllLine", "{Toggle}  Repair all:  {Pay}"), TEXT("Pay"), Page.AllPay)
														  : FText::FormatNamed(LOCTEXT("RepairAllRefused", "{Toggle}  Repair all:  {Pay} ({Why})"), TEXT("Pay"), Page.AllPay, TEXT("Why"), Page.AllRefusal));
	}
	else
	{
		WoRKeyHints::SetHint(WholeStackText, bWholeStack ? FText::FormatNamed(LOCTEXT("WholeStackOn", "{Toggle}  Whole stack: on (all {Count})"), TEXT("Count"), FText::AsNumber(Row.Row.Count))
											: LOCTEXT("WholeStackOff", "{Toggle}  Whole stack: off (one at a time)"));
	}
	RuleText->SetText(Page.Rule);
	SetShown(RuleText, !Page.Rule.IsEmpty());
	StatusText->SetText(Status);
	StatusText->SetColorAndOpacity(FSlateColor(bStatusIsRefusal ? WarnColor : GoodColor));
	SetShown(StatusText, !Status.IsEmpty());

	if (bScrollIntoView && IsTileGrid())
	{
		const int32 Tile = ShownRows.IndexOfByKey(SelectedRow);
		if (UWidget *TileFrame = GridScroll && TileWidgets.IsValidIndex(Tile) ? TileWidgets[Tile].Frame.Get() : nullptr)
		{
			GridScroll->ScrollWidgetIntoView(TileFrame, /*AnimateScroll=*/false);
		}
	}
	else if (bScrollIntoView && RowScroll && RowWidgets.IsValidIndex(SelectedRow))
	{
		if (UWidget *RowFrame = RowWidgets[SelectedRow].Frame.Get())
		{
			RowScroll->ScrollWidgetIntoView(RowFrame, /*AnimateScroll=*/false);
		}
	}
}

void UVendorScreenWidget::RefreshCart()
{
	using namespace VendorScreenStyle;

	if (!CartPanel)
	{
		return; // headless: no tree
	}
	SetShown(CartPanel, IsBuy());
	UWorld *World = GetWorld();
	TArray<FText> Lines;
	for (const FVendorCartLine &Line : Cart)
	{
		Lines.Add(WoRVendorScreen::GetCartLineText(World, Line));
	}
	CartLinesText->SetText(Cart.IsEmpty() ? LOCTEXT("CartNothing", "Nothing yet.") : FText::Join(FText::FromString(TEXT("\n")), Lines));
	CartLinesText->SetColorAndOpacity(FSlateColor(Cart.IsEmpty() ? OffColor : TextColor));

	const FVendorCartSummary Summary = WoRVendorScreen::BuildCartSummary(World, Payer.Get(), Cart);
	const bool bCanBuy = !Cart.IsEmpty() && Summary.Refusal.IsEmpty();
	CartTotalText->SetText(Summary.Refusal.IsEmpty() ? FText::Format(LOCTEXT("CartTotal", "Total:  {0}\nAfter purchase:  {1}"), Summary.Total, Summary.After)
													 : FText::Format(LOCTEXT("CartTotalShort", "Total:  {0}\n{1}"), Summary.Total, Summary.Refusal));
	CartTotalText->SetColorAndOpacity(FSlateColor(Summary.Refusal.IsEmpty() ? TextColor : WarnColor));
	SetShown(CartTotalText, !Cart.IsEmpty());
	PurchaseButton->SetBrushColor(bCanBuy ? FocusColor : PlateColor);
	PurchaseText->SetColorAndOpacity(FSlateColor(bCanBuy ? TextColor : OffColor));
}

void UVendorScreenWidget::AddFocusedToCart()
{
	const FVendorScreenRow *Focused = FocusedRow();
	if (!IsBuy() || !Focused || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	const FVendorScreenRow Row = *Focused; // a copy: the page is rebuilt below
	Status = WoRVendorScreen::AddToCart(Payer.Get(), Row, Members[MemberIndex], Cart);
	bStatusIsRefusal = !Status.IsEmpty();
	RefreshData();
	RefreshSelection(/*bScrollIntoView=*/false);
}

void UVendorScreenWidget::RemoveFocusedFromCart()
{
	const FVendorScreenRow *Focused = FocusedRow();
	if (IsBuy() && Focused && WoRVendorScreen::RemoveFromCart(*Focused, MemberKey(), Cart))
	{
		Status = FText::GetEmpty();
		RefreshData();
		RefreshSelection(/*bScrollIntoView=*/false);
	}
}

void UVendorScreenWidget::Purchase()
{
	if (!IsBuy())
	{
		return;
	}
	const FText Total = WoRVendorScreen::BuildCartSummary(GetWorld(), Payer.Get(), Cart).Total;
	const FText Failure = WoRVendorScreen::PurchaseCart(GetWorld(), Payer.Get(), Cart);
	bStatusIsRefusal = !Failure.IsEmpty();
	Status = bStatusIsRefusal ? Failure : FText::Format(LOCTEXT("DoneBuy", "Bought for {0}."), Total);
	UE_LOG(LogWoRUI, Log, TEXT("[VendorScreen] Purchase: %s"), *Status.ToString());
	RefreshData();
	RefreshSelection(/*bScrollIntoView=*/false);
}

void UVendorScreenWidget::MoveRow(int32 Delta)
{
	SelectRow(SelectedRow + Delta);
}

void UVendorScreenWidget::SelectRow(int32 Index, bool bScrollIntoView)
{
	if (ShownRows.Contains(Index) && Index != SelectedRow) // B-616b: a row its filter hides can't be lit
	{
		SelectedRow = Index;
		bConfirming = false;
		bConfirmingAll = false;
		Status = FText::GetEmpty();
		RefreshSelection(bScrollIntoView);
	}
}

void UVendorScreenWidget::MoveTab(int32 Delta)
{
	if (Tabs.Num() < 2)
	{
		return;
	}
	TabIndex = ((TabIndex + Delta) % Tabs.Num() + Tabs.Num()) % Tabs.Num();
	SelectedRow = 0;
	Filters.Reset(); // another page: its own kinds, from All
	bOnFilters = false;
	bConfirming = false;
	bConfirmingAll = false;
	Status = FText::GetEmpty();
	RefreshData();
	RefreshSelection();
}

void UVendorScreenWidget::MoveMember(int32 Delta)
{
	if (Members.Num() < 2 || (!HasMemberTabs() && !IsBuy()))
	{
		return; // Item slots and Repair: the rows name the members; curses are the player's
	}
	MemberIndex = ((MemberIndex + Delta) % Members.Num() + Members.Num()) % Members.Num();
	SelectedRow = IsBuy() ? SelectedRow : 0; // Buy: the stock is the same list, whoever it is bought for
	bConfirming = false;
	bConfirmingAll = false;
	MergeTargets.Reset(); // another member's crystals: each kind starts again at the highest it can make
	UpgradeTargets.Reset();
	Status = FText::GetEmpty();
	RefreshData();
	RefreshSelection();
}

void UVendorScreenWidget::Confirm()
{
	if (IsBuy())
	{
		AddFocusedToCart(); // nothing is spent until Purchase: the cart is the question
		return;
	}
	if (bConfirmingAll)
	{
		DoRepairAll(); // A answers "Repair everything?" as the second X does
		return;
	}
	const FVendorScreenRow *Focused = FocusedRow();
	const bool bSlots = IsItemSlots();
	const bool bSpirit = IsSpiritualistService();
	EVendorService Service = EVendorService::Sell;
	if (!Focused || (!bSlots && !bSpirit && !WoRVendorScreen::GetService(GetTab(), Service)))
	{
		return;
	}
	const FVendorScreenRow Row = *Focused;
	if (!Row.Refusal.IsEmpty())
	{
		bConfirming = false;
		Status = Row.Refusal;
		bStatusIsRefusal = true;
		RefreshSelection();
		return;
	}
	if (!bConfirming)
	{
		// D-016: ask first; the second press does it.
		bConfirming = true;
		Status = FText::GetEmpty();
		RefreshSelection();
		return;
	}
	bConfirming = false;
	const int32 Count = WoRVendorScreen::GetTakeCount(Row, bWholeStack);
	const FText Failure = bSlots	? WoRVendorScreen::BuyItemSlot(GetWorld(), Payer.Get(), Row)
						  : bSpirit ? WoRVendorScreen::RunService(GetWorld(), Payer.Get(), MemberKey(), GetTab(), Row)
									: WoRVendorServices::Run(GetWorld(), Vendor, Service, MemberKey(), Row.Row, Count);
	bStatusIsRefusal = !Failure.IsEmpty();
	if (bStatusIsRefusal)
	{
		Status = Failure;
	}
	else if (bSpirit)
	{
		Status = WoRVendorScreen::GetDoneText(GetTab(), Row);
	}
	else if (bSlots)
	{
		Status = FText::Format(LOCTEXT("DoneItemSlot", "Pouch: {1} slots."), Row.Name, FText::AsNumber(Row.Slots + 1));
	}
	else if (Count > 1)
	{
		Status = Service == EVendorService::Sell
					 ? FText::Format(LOCTEXT("DoneSellMany", "Sold {0} x {1} for {2} each."), FText::AsNumber(Count), Row.Name, Row.Pay)
					 : FText::Format(LOCTEXT("DoneDismantleMany", "Dismantled {0} x {1} for {2} each."), FText::AsNumber(Count), Row.Name, Row.Pay);
	}
	else
	{
		Status = Service == EVendorService::Sell ? FText::Format(LOCTEXT("DoneSell", "Sold {0} for {1}."), Row.Name, Row.Pay)
												 : FText::Format(LOCTEXT("DoneDismantle", "Dismantled {0} for {1}."), Row.Name, Row.Pay);
	}
	UE_LOG(LogWoRUI, Log, TEXT("[VendorScreen] %s"), *Status.ToString());
	RefreshData();
	RefreshSelection();
}

void UVendorScreenWidget::PressRepairAll()
{
	if (GetTab() != EVendorScreenTab::Repair)
	{
		return;
	}
	if (bConfirmingAll)
	{
		DoRepairAll();
		return;
	}
	bConfirming = false;
	if (!Page.AllRefusal.IsEmpty())
	{
		Status = Page.AllRefusal;
		bStatusIsRefusal = true;
	}
	else
	{
		bConfirmingAll = true; // D-016: ask first; the second press does it
		Status = FText::GetEmpty();
	}
	RefreshSelection(/*bScrollIntoView=*/false);
}

void UVendorScreenWidget::DoRepairAll()
{
	bConfirmingAll = false;
	const FText AllPay = Page.AllPay;
	const FText Failure = WoRVendorScreen::RepairAll(GetWorld(), Payer.Get());
	bStatusIsRefusal = !Failure.IsEmpty();
	Status = bStatusIsRefusal ? Failure : FText::Format(LOCTEXT("DoneRepairAll", "All repaired for {0}."), AllPay);
	UE_LOG(LogWoRUI, Log, TEXT("[VendorScreen] Repair all: %s"), *Status.ToString());
	RefreshData();
	RefreshSelection();
}

void UVendorScreenWidget::MoveMergeTarget(int32 Delta)
{
	const FVendorScreenRow *Focused = FocusedRow();
	if (GetTab() != EVendorScreenTab::Merge || !Focused)
	{
		return;
	}
	MergeTargets.Add(Focused->Row.Item.Type, WoRVendorScreen::StepMergeTarget(Focused->Row.Item.Tier, Delta));
	bConfirming = false; // the question was about the other tier: ask again
	Status = FText::GetEmpty();
	RefreshData();
	RefreshSelection(/*bScrollIntoView=*/false);
}

void UVendorScreenWidget::MoveUpgradeTarget(int32 Delta)
{
	const FVendorScreenRow *Focused = FocusedRow();
	if (GetTab() != EVendorScreenTab::Upgrade || !Focused)
	{
		return;
	}
	UpgradeTargets.Add(Focused->Row.GearID, WoRVendorScreen::StepUpgradeTarget(Focused->Row.Tier, Focused->UpgradeTo, Delta));
	bConfirming = false; // the question was about the other tier: ask again
	Status = FText::GetEmpty();
	RefreshData();
	RefreshSelection(/*bScrollIntoView=*/false);
}

void UVendorScreenWidget::ToggleWholeStack()
{
	EVendorService Service;
	if (!WoRVendorScreen::GetService(GetTab(), Service))
	{
		return; // nothing here is a stack
	}
	bWholeStack = !bWholeStack;
	bConfirming = false; // the question was about the other amount: ask again
	RefreshSelection(/*bScrollIntoView=*/false);
}

FReply UVendorScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	// B-616b (Q-616b): on a tile grid all four arrows move (Buy's add / take back went to A and Y / R, Upgrade's tier to
	// Y / R); a list keeps Up / Down.
	if (IsUpKey(Key) || IsDownKey(Key) || (IsTileGrid() && (IsLeftKey(Key) || IsRightKey(Key))))
	{
		MoveInGrid(IsLeftKey(Key) ? -1 : IsRightKey(Key) ? 1 : 0, IsDownKey(Key) ? 1 : IsUpKey(Key) ? -1 : 0);
		return FReply::Handled();
	}
	const bool bBuy = IsBuy();
	if (GetTab() == EVendorScreenTab::Merge && (IsLeftKey(Key) || IsRightKey(Key)))
	{
		MoveMergeTarget(IsRightKey(Key) ? 1 : -1); // the tier to make
		return FReply::Handled();
	}
	if (IsOnFilters() && (IsSecondActionKey(Key) || IsToggleKey(Key)))
	{
		return FReply::Handled(); // no tile is lit up there: nothing for them to act on
	}
	if (IsSecondActionKey(Key))
	{
		PressSecond(); // Buy: take one back; Upgrade: the tier. Held, it repeats.
		return FReply::Handled();
	}
	if (InKeyEvent.IsRepeat())
	{
		return FReply::Handled(); // the keys below act once per press
	}
	if (IsConfirmKey(Key))
	{
		if (IsOnFilters())
		{
			MoveInGrid(0, 1); // down to the tiles
		}
		else
		{
			Confirm();
		}
		return FReply::Handled();
	}
	if (IsToggleKey(Key))
	{
		if (bBuy)
		{
			Purchase();
		}
		else if (GetTab() == EVendorScreenTab::Repair)
		{
			PressRepairAll();
		}
		else
		{
			ToggleWholeStack();
		}
		return FReply::Handled();
	}
	if (IsTabPrevKey(Key) || IsTabNextKey(Key))
	{
		MoveTab(IsTabNextKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsSubTabPrevKey(Key) || IsSubTabNextKey(Key))
	{
		MoveMember(IsSubTabNextKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsBackKey(Key))
	{
		// Reached only when CommonUI's own Back action didn't take the key (BP_CommonInputData unset, B-103).
		HandleBackAction();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

int32 UVendorScreenWidget::FindRowUnder(const FPointerEvent &MouseEvent) const
{
	if (IsTileGrid())
	{
		return FindTileUnder(MouseEvent); // B-616b: the hidden rows keep their last places
	}
	// B-602: the rows' places as scrolled now (a click after a wheel scroll picked the row that had been there).
	return WoRMenuStyle::FindScrolledRowUnder(RowScroll, FMath::Min(RowWidgets.Num(), Page.Rows.Num()),
											  [this](int32 Index) -> const UWidget * { return RowWidgets[Index].Frame.Get(); },
											  MouseEvent.GetScreenSpacePosition());
}

FReply UVendorScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks, so a resting cursor doesn't fight the keys.
	if (!InMouseEvent.GetCursorDelta().IsZero())
	{
		const int32 Over = FindRowUnder(InMouseEvent);
		if (Over != INDEX_NONE && IsOnFilters())
		{
			bOnFilters = false; // B-616b: a tile under the mouse takes the keys back to the tiles
			RefreshData();
			RefreshSelection(/*bScrollIntoView=*/false);
		}
		SelectRow(Over, /*bScrollIntoView=*/false); // the list mustn't move under the cursor
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UVendorScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	const bool bBuy = IsBuy();
	if (bBuy && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && FindRowUnder(InMouseEvent) != INDEX_NONE)
	{
		SelectRow(FindRowUnder(InMouseEvent), /*bScrollIntoView=*/false);
		RemoveFocusedFromCart(); // as Left
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	if (bBuy && PurchaseButton && PurchaseButton->GetCachedGeometry().IsUnderLocation(Position))
	{
		Purchase();
		return FReply::Handled();
	}
	// The line sits in the detail panel: hidden with it, it keeps the place it last had.
	// B-608: the details scroll; the line counts only where its scroll box shows it (a scrolled-away line's place may lie
	// over the cart).
	if (bBuy && BuyForButton && DetailPanel && DetailPanel->IsVisible() && BuyForButton->GetCachedGeometry().IsUnderLocation(Position) &&
		DetailScroll && DetailScroll->GetCachedGeometry().IsUnderLocation(Position))
	{
		MoveMember(1);
		return FReply::Handled();
	}
	for (int32 Index = 0; Index < ServiceTabs.Num() && Index < Tabs.Num(); ++Index)
	{
		if (ServiceTabs[Index] && ServiceTabs[Index]->GetCachedGeometry().IsUnderLocation(Position))
		{
			if (Index != TabIndex) // a click on the shown tab changes nothing
			{
				MoveTab(Index - TabIndex);
			}
			return FReply::Handled();
		}
	}
	// Buy and Item slots hide the character tabs; a hidden tab keeps the place it last had.
	for (int32 Index = 0; HasMemberTabs() && Index < MemberTabs.Num() && Index < Members.Num(); ++Index)
	{
		if (MemberTabs[Index] && MemberTabs[Index]->GetCachedGeometry().IsUnderLocation(Position))
		{
			if (Index != MemberIndex)
			{
				MoveMember(Index - MemberIndex);
			}
			return FReply::Handled();
		}
	}
	// B-616b: a filter tab picks its filter.
	for (int32 Index = 0; IsTileGrid() && Filters.Num() > 1 && Index < FilterTabs.Num() && Index < Filters.Num(); ++Index)
	{
		if (FilterTabs[Index] && FilterTabs[Index]->GetCachedGeometry().IsUnderLocation(Position))
		{
			bOnFilters = false;
			PickFilter(Index);
			return FReply::Handled();
		}
	}
	const int32 OverRow = FindRowUnder(InMouseEvent);
	if (OverRow != INDEX_NONE && IsTileGrid())
	{
		if (bOnFilters)
		{
			bOnFilters = false; // the click takes the keys back to the tiles
			RefreshData();
			RefreshSelection(/*bScrollIntoView=*/false);
		}
		ClickRow(OverRow, bBuy && OverRow == SelectedRow); // the lit tile is A (Buy: adds one)
		return FReply::Handled();
	}
	if (OverRow != INDEX_NONE)
	{
		const UBorder *AddButton = RowWidgets[OverRow].AddButton.Get();
		ClickRow(OverRow, bBuy && AddButton && AddButton->IsVisible() && AddButton->GetCachedGeometry().IsUnderLocation(Position));
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

EVendorRowClick UVendorScreenWidget::GetRowClick(bool bBuy, bool bOnAddButton, bool bAlreadySelected)
{
	if (bBuy)
	{
		return bOnAddButton ? EVendorRowClick::Add : EVendorRowClick::Select; // adding is A, Right or the "+"
	}
	return bAlreadySelected ? EVendorRowClick::Confirm : EVendorRowClick::Select;
}

void UVendorScreenWidget::ClickRow(int32 Index, bool bOnAddButton)
{
	if (!Page.Rows.IsValidIndex(Index))
	{
		return;
	}
	const EVendorRowClick Click = GetRowClick(IsBuy(), bOnAddButton, Index == SelectedRow);
	SelectRow(Index, /*bScrollIntoView=*/false);
	if (Click == EVendorRowClick::Add)
	{
		AddFocusedToCart();
	}
	else if (Click == EVendorRowClick::Confirm)
	{
		Confirm();
	}
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GVendorScreenOpenCommand(
	TEXT("wor.UI.VendorScreen"),
	TEXT("B-456a to B-456d: open the vendor screen. Usage: wor.UI.VendorScreen [Blacksmith|Jeweler|Spiritualist] (default Blacksmith)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		EMerchantType Vendor = EMerchantType::Blacksmith;
		if (Args.Num() > 0)
		{
			const int64 Value = StaticEnum<EMerchantType>()->GetValueByNameString(Args[0]);
			if (Value == INDEX_NONE)
			{
				UE_LOG(LogWoRUI, Warning, TEXT("[VendorScreen] Unknown vendor '%s' - use Blacksmith, Jeweler or Spiritualist"), *Args[0]);
				return;
			}
			Vendor = static_cast<EMerchantType>(Value);
		}
		if (!UVendorScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr, Vendor))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[VendorScreen] wor.UI.VendorScreen: not opened (no hub controller with a root layout, or no shop screen)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
