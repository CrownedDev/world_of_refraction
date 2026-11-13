// VendorScreenWidget.cpp

#include "UI/Menus/VendorScreenWidget.h"

#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Infusion/ElementColors.h"
#include "Party/Party.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRVendorScreenWidget"

namespace VendorScreenStyle
{
	// This screen's own layout (screen_mockups/hub/vendors, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float ListWidth = 1068.0f;
	constexpr float RowPadding = 15.0f;
	constexpr float ElementMarkWidth = 9.0f;
	constexpr float TierWidth = 60.0f;
	constexpr float CountWidth = 75.0f;
	constexpr float PayWidth = 300.0f;
	constexpr float PanelGap = 45.0f;
	constexpr float PanelWidth = 664.0f;
	constexpr float PanelPadding = 30.0f;
	/** The Spiritualist has the most services (D-193). */
	constexpr int32 MaxServiceTabs = 6;
	/** Between two money chips on the top line. */
	const TCHAR *const ChipGap = TEXT("        ");

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
	AddToRow(ServiceRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabPrev", "LB")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (int32 Index = 0; Index < MaxServiceTabs; ++Index)
	{
		UTextBlock *Label = nullptr;
		ServiceTabs.Add(MakeTab(WidgetTree, Label));
		ServiceTabLabels.Add(Label);
		AddToRow(ServiceRow, ServiceTabs.Last(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	AddToRow(ServiceRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabNext", "RB")), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(Main, ServiceRow, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	UHorizontalBox *MemberRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(MemberRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabPrev", "LT")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (int32 Index = 0; Index < PartyConstants::MAX_PARTY_MEMBERS; ++Index)
	{
		UTextBlock *Label = nullptr;
		MemberTabs.Add(MakeTab(WidgetTree, Label));
		MemberTabLabels.Add(Label);
		AddToRow(MemberRow, MemberTabs.Last(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	AddToRow(MemberRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabNext", "RT")), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(Main, MemberRow, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	MemberTabRow = MemberRow;

	// The rows, and the focused row's details.
	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body, FMargin(0.0f, SectionGap, 0.0f, SectionGap))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBox *ListColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	EmptyText = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(ListColumn, EmptyText, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	RowScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddToColumn(ListColumn, RowScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Body, MakeSized(WidgetTree, ListWidth, 0.0f, ListColumn), FMargin(0.0f), VAlign_Fill);

	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	UVerticalBox *PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(PanelColumn);
	DetailTitle = MakeWrappedText(WidgetTree, NameFontSize, TextColor, true);
	AddToColumn(PanelColumn, DetailTitle, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	PayLabel = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, PayLabel);
	DetailPay = MakeWrappedText(WidgetTree, RowFontSize, TextColor, true);
	AddToColumn(PanelColumn, DetailPay, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	// Buy: what the thing is, and who it is bought for.
	DetailLines = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, DetailLines, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	BuyForButton = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BuyForButton->SetBrushColor(PlateColor);
	BuyForButton->SetPadding(FMargin(RowPadding));
	BuyForText = MakeText(WidgetTree, LineFontSize, TextColor);
	BuyForButton->SetContent(BuyForText);
	AddToColumn(PanelColumn, BuyForButton, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	ConfirmText = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	AddToColumn(PanelColumn, ConfirmText, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	ConfirmHint = MakeText(WidgetTree, LineFontSize, CyanColor, true);
	ConfirmHint->SetText(LOCTEXT("ConfirmHint", "A / Enter  Confirm        B / Esc  Cancel"));
	AddToColumn(PanelColumn, ConfirmHint, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	WholeStackText = MakeText(WidgetTree, LineFontSize, CyanColor);
	AddToColumn(PanelColumn, WholeStackText, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	RuleText = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, RuleText);
	StatusText = MakeWrappedText(WidgetTree, LineFontSize, GoodColor);
	AddToColumn(PanelColumn, StatusText, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	DetailPanel = MakeSized(WidgetTree, PanelWidth, 0.0f, Panel);

	// Buy: the cart, under the details.
	UBorder *CartFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CartFrame->SetBrushColor(PanelColor);
	CartFrame->SetPadding(FMargin(PanelPadding));
	UVerticalBox *CartColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	CartFrame->SetContent(CartColumn);
	UTextBlock *CartHeading = MakeText(WidgetTree, RowFontSize, DimColor, true);
	CartHeading->SetText(LOCTEXT("CartHeading", "Cart"));
	AddToColumn(CartColumn, CartHeading, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	CartLinesText = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	AddToColumn(CartColumn, CartLinesText, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	CartTotalText = MakeWrappedText(WidgetTree, LineFontSize, TextColor, true);
	AddToColumn(CartColumn, CartTotalText, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	PurchaseButton = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PurchaseButton->SetPadding(FMargin(RowPadding));
	PurchaseButton->SetHorizontalAlignment(HAlign_Center);
	PurchaseText = MakeText(WidgetTree, RowFontSize, TextColor, true);
	PurchaseText->SetText(LOCTEXT("Purchase", "X  Purchase"));
	PurchaseButton->SetContent(PurchaseText);
	AddToColumn(CartColumn, PurchaseButton);
	CartPanel = MakeSized(WidgetTree, PanelWidth, 0.0f, CartFrame);

	UVerticalBox *Side = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToColumn(Side, DetailPanel);
	AddToColumn(Side, CartPanel, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToRow(Body, Side, FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Top);

	HintsText = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(Main, HintsText)->SetHorizontalAlignment(HAlign_Right);
}

UVendorScreenWidget::FRowWidgets UVendorScreenWidget::BuildRow()
{
	using namespace VendorScreenStyle;

	FRowWidgets Widgets;
	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(RowPadding));
	UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Line);

	UBorder *ElementMark = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	AddToRow(Line, MakeSized(WidgetTree, ElementMarkWidth, 0.0f, ElementMark), FMargin(0.0f, 0.0f, RowPadding, 0.0f), VAlign_Fill);
	UVerticalBox *Words = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
	UTextBlock *Sub = MakeText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(Words, Name);
	AddToColumn(Words, Sub);
	AddToRow(Line, Words)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock *Tier = MakeText(WidgetTree, LineFontSize, DimColor, true);
	AddToRow(Line, MakeSized(WidgetTree, TierWidth, 0.0f, Tier));
	UTextBlock *Count = MakeText(WidgetTree, LineFontSize, CyanColor, true);
	AddToRow(Line, MakeSized(WidgetTree, CountWidth, 0.0f, Count));
	UTextBlock *Pay = MakeText(WidgetTree, RowFontSize, TextColor, true);
	Pay->SetJustification(ETextJustify::Right);
	AddToRow(Line, MakeSized(WidgetTree, PayWidth, 0.0f, Pay));

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
	Widgets.Pay = Pay;
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
	bWholeStack = false; // the stack holds a pooled screen: nothing carries over from the last open
	Cart.Reset();
	Status = FText::GetEmpty();
	RefreshData();
	RefreshSelection();
}

bool UVendorScreenWidget::NativeOnHandleBackAction()
{
	if (!bConfirming)
	{
		return Super::NativeOnHandleBackAction();
	}
	bConfirming = false;
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
	Page = bBuy ? WoRVendorScreen::BuildBuyPage(World, Merchant.Get(), Payer.Get(), MemberKey(), Cart) : WoRVendorScreen::BuildPage(World, Vendor, GetTab(), MemberKey());
	SelectedRow = Page.Rows.Num() > 0 ? FMath::Clamp(SelectedRow, 0, Page.Rows.Num() - 1) : 0;

	if (!TitleText || !RowScroll)
	{
		return; // headless: no tree
	}
	RefreshCart();
	SetShown(MemberTabRow, !bBuy); // Buy says who it is for in the panel ("Buying for")
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
	EmptyText->SetText(Page.EmptyNote);
	SetShown(EmptyText, !Page.EmptyNote.IsEmpty());
	HintsText->SetText(bBuy ? LOCTEXT("HintsBuy", "LB / RB  Service        A / Enter  Add        Left  Take back        R / Y  Buying for        X  Purchase        B / Esc  Leave")
							: FText::Format(LOCTEXT("Hints", "LT / RT  Character        LB / RB  Service        A / Enter  {0}        X  Whole stack        B / Esc  Leave"),
											WoRVendorScreen::GetTabName(GetTab())));

	while (RowWidgets.Num() < Page.Rows.Num())
	{
		RowWidgets.Add(BuildRow());
	}
	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		const FRowWidgets &Widgets = RowWidgets[Index];
		const bool bShown = Page.Rows.IsValidIndex(Index);
		SetShown(Widgets.Frame.Get(), bShown);
		if (!bShown || !Widgets.ElementMark.IsValid() || !Widgets.Name.IsValid() || !Widgets.Sub.IsValid() || !Widgets.Tier.IsValid() ||
			!Widgets.Count.IsValid() || !Widgets.Pay.IsValid())
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
		Widgets.Tier->SetText(Row.Tier);
		// Sell and Dismantle: the stack's size. Buy: how many the cart holds, and a refused line still shows its price.
		Widgets.Count->SetText(Row.Row.Count > (bBuy ? 0 : 1) ? FText::Format(LOCTEXT("StackCount", "x{0}"), FText::AsNumber(Row.Row.Count)) : FText::GetEmpty());
		Widgets.Pay->SetText(bRefused && !bBuy ? FText::GetEmpty() : Row.PayMoney.IsEmpty() ? Row.Pay : FText::Format(LOCTEXT("PayAndMore", "{0} +"), Row.PayMoney));
		Widgets.Pay->SetColorAndOpacity(FSlateColor(bRefused ? OffColor : TextColor));
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
	if (!DetailTitle)
	{
		return; // headless: no tree
	}
	SetShown(DetailPanel, Focused != nullptr || !Status.IsEmpty());
	const bool bBuy = IsBuy();
	const FVendorScreenRow Row = Focused ? *Focused : FVendorScreenRow();
	const int32 Count = bBuy ? 1 : WoRVendorScreen::GetTakeCount(Row, bWholeStack); // Buy's Row.Count is the cart's, not a stack
	const FText What = Row.Tier.IsEmpty() ? Row.Name : FText::Format(LOCTEXT("NameAndTier", "{0} ({1})"), Row.Name, Row.Tier);
	DetailTitle->SetText(!Focused ? FText::GetEmpty() : bBuy ? What : FText::Format(LOCTEXT("DetailTitle", "{0} {1}"), WoRVendorScreen::GetTabName(GetTab()), What));
	PayLabel->SetText(bBuy ? LOCTEXT("Price", "Price") : LOCTEXT("YouGet", "You get"));
	SetShown(PayLabel, Focused != nullptr);
	DetailPay->SetText(!Row.Refusal.IsEmpty() ? Row.Refusal : Count > 1 ? FText::Format(LOCTEXT("PayEach", "{0} each, for {1}"), Row.Pay, FText::AsNumber(Count)) : Row.Pay);
	DetailPay->SetColorAndOpacity(FSlateColor(Row.Refusal.IsEmpty() ? TextColor : WarnColor));

	// Buy: what the thing is and does, and who it is bought for.
	TArray<FText> Lines;
	if (bBuy && Focused)
	{
		if (!Row.Refusal.IsEmpty())
		{
			Lines.Add(Row.Pay); // the reason took the price's place
		}
		Lines.Add(Row.KindLine);
		for (const FLoadoutScreenDetail &Detail : Row.Details)
		{
			Lines.Add(FText::Format(LOCTEXT("DetailLine", "{0}:  {1}"), Detail.Label, Detail.Value));
		}
		if (!Row.Note.IsEmpty())
		{
			Lines.Add(Row.Note);
		}
	}
	DetailLines->SetText(FText::Join(FText::FromString(TEXT("\n")), Lines));
	SetShown(DetailLines, !Lines.IsEmpty());
	const bool bBuyFor = bBuy && Members.IsValidIndex(MemberIndex);
	SetShown(BuyForButton, bBuyFor);
	if (bBuyFor)
	{
		BuyForText->SetText(FText::Format(LOCTEXT("BuyingFor", "R / Y  Buying for:  {0}"), Members[MemberIndex].Name));
	}

	EVendorService Service;
	const bool bAsking = bConfirming && Focused && WoRVendorScreen::GetService(GetTab(), Service);
	ConfirmText->SetText(bAsking ? WoRVendorServices::GetConfirmText(GetWorld(), Service, MemberKey(), Row.Row) : FText::GetEmpty());
	SetShown(ConfirmText, bAsking);
	SetShown(ConfirmHint, bAsking);
	const bool bStack = Focused && !bBuy && Row.Row.Count > 1;
	SetShown(WholeStackText, bStack);
	WholeStackText->SetText(bWholeStack ? FText::Format(LOCTEXT("WholeStackOn", "X  Whole stack: on (all {0})"), FText::AsNumber(Row.Row.Count))
										: LOCTEXT("WholeStackOff", "X  Whole stack: off (one at a time)"));
	RuleText->SetText(Page.Rule);
	SetShown(RuleText, !Page.Rule.IsEmpty());
	StatusText->SetText(Status);
	StatusText->SetColorAndOpacity(FSlateColor(bStatusIsRefusal ? WarnColor : GoodColor));
	SetShown(StatusText, !Status.IsEmpty());

	if (bScrollIntoView && RowScroll && RowWidgets.IsValidIndex(SelectedRow))
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
	if (Page.Rows.IsValidIndex(Index) && Index != SelectedRow)
	{
		SelectedRow = Index;
		bConfirming = false;
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
	bConfirming = false;
	Status = FText::GetEmpty();
	RefreshData();
	RefreshSelection();
}

void UVendorScreenWidget::MoveMember(int32 Delta)
{
	if (Members.Num() < 2)
	{
		return;
	}
	MemberIndex = ((MemberIndex + Delta) % Members.Num() + Members.Num()) % Members.Num();
	SelectedRow = IsBuy() ? SelectedRow : 0; // Buy: the stock is the same list, whoever it is bought for
	bConfirming = false;
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
	const FVendorScreenRow *Focused = FocusedRow();
	EVendorService Service;
	if (!Focused || !WoRVendorScreen::GetService(GetTab(), Service))
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
	const FText Failure = WoRVendorServices::Run(GetWorld(), Vendor, Service, MemberKey(), Row.Row, Count);
	bStatusIsRefusal = !Failure.IsEmpty();
	if (bStatusIsRefusal)
	{
		Status = Failure;
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

void UVendorScreenWidget::ToggleWholeStack()
{
	bWholeStack = !bWholeStack;
	bConfirming = false; // the question was about the other amount: ask again
	RefreshSelection(/*bScrollIntoView=*/false);
}

FReply UVendorScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveRow(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	const bool bBuy = IsBuy();
	if (bBuy && (IsLeftKey(Key) || IsRightKey(Key)))
	{
		// Buy: Right adds one more of the focused line (as A), Left takes one back; held, they repeat.
		IsRightKey(Key) ? AddFocusedToCart() : RemoveFocusedFromCart();
		return FReply::Handled();
	}
	if (InKeyEvent.IsRepeat())
	{
		return FReply::Handled(); // the keys below act once per press
	}
	if (IsConfirmKey(Key))
	{
		Confirm();
		return FReply::Handled();
	}
	if (IsToggleKey(Key))
	{
		bBuy ? Purchase() : ToggleWholeStack();
		return FReply::Handled();
	}
	if (bBuy && IsSecondActionKey(Key))
	{
		MoveMember(1); // "Buying for"
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
	// A row scrolled out of view keeps its geometry past the list's edge: only the list's own area counts.
	if (!RowScroll || !RowScroll->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
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

FReply UVendorScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks, so a resting cursor doesn't fight the keys.
	if (!InMouseEvent.GetCursorDelta().IsZero())
	{
		SelectRow(FindRowUnder(InMouseEvent), /*bScrollIntoView=*/false); // the list mustn't move under the cursor
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
	if (bBuy && BuyForButton && BuyForButton->GetCachedGeometry().IsUnderLocation(Position))
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
	// Buy hides the character tabs; a hidden tab keeps the place it last had.
	for (int32 Index = 0; !bBuy && Index < MemberTabs.Num() && Index < Members.Num(); ++Index)
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
	const int32 OverRow = FindRowUnder(InMouseEvent);
	if (OverRow != INDEX_NONE)
	{
		// A click on a row chooses it, as moving onto it and pressing A does (a second click confirms).
		SelectRow(OverRow, /*bScrollIntoView=*/false);
		Confirm();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GVendorScreenOpenCommand(
	TEXT("wor.UI.VendorScreen"),
	TEXT("B-456a: open the vendor screen. Usage: wor.UI.VendorScreen [Blacksmith|Jeweler|Spiritualist] (default Blacksmith)."),
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
