// LoadoutV2ScreenWidget.cpp

#include "UI/Menus/LoadoutV2ScreenWidget.h"

#include "Algo/IndexOf.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/LoadoutConstants.h"
#include "Inventory/GearGive.h"
#include "Party/Party.h"
#include "Pool/PoolSubsystem.h"
#include "Misc/ScopeExit.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/LoadoutApparelModel.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRIconTable.h"
#include "UI/WoRRootLayout.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutV2Screen"

bool ULoadoutV2ScreenWidget::bSimpleView = false;

namespace LoadoutV2ScreenStyle
{
	// This screen's own layout (mockups menus/loadout/v2, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float ListWidth = 840.0f;
	constexpr float BoxWidth = 860.0f;
	constexpr float ColumnGap = 48.0f;
	constexpr float RowPadding = 18.0f;
	constexpr float RowGap = 10.0f;
	constexpr float NameColumnWidth = 460.0f;
	/** The list panel's tallest, until the screen has been drawn once and its own area's height is known (RefreshView). */
	constexpr float ListMaxHeight = 700.0f;
	// D-248 (mockup menus/loadout/v4/1b): while a weapon slot is being filled the list narrows to the slots and the
	// grid takes the rest of the row; the two widths and the gap add up to ListWidth + ColumnGap + BoxWidth. Step 2: the
	// details are a card beside the lit tile (the details box, moved there), so the grid has the box's width too.
	constexpr float GridGap = 24.0f;
	constexpr float GridSlotsWidth = 520.0f;
	constexpr float GridWidth = ListWidth + ColumnGap + BoxWidth - GridSlotsWidth - GridGap;
	constexpr float GridNameColumnWidth = 290.0f;
	// The mockup's tile (72 px at 720p). Ten of them with their padding, the panel's padding and the scroll bar's room
	// fill GridWidth.
	constexpr float TileSize = 108.0f;
	constexpr float TilePadding = 3.0f;
	constexpr float TileIconSize = 72.0f;
	/** A tile with the padding around it: the grid's row and column step. */
	constexpr float TilePitch = TileSize + TilePadding * 2.0f;
	// The card: the details box beside the lit tile, in a thin light frame so it reads over the tiles.
	constexpr float CardWidth = 520.0f;
	constexpr float CardGap = 6.0f;
	constexpr float CardFrameWidth = 2.0f;
	/** The room a line of the card's text has: the card less its frame and its padding. The lines wrap at a width set
	 *  from this (not at the width they were last drawn at), so the card's height is right the moment it is filled. */
	constexpr float CardTextWidth = CardWidth - CardFrameWidth * 2.0f - BoxPadding * 2.0f;
	// D-249: the inventory's tabs, in the row's order (the inventory screen's own Loadout tab is not one of them).
	const EInventoryScreenTab InventoryTabOrder[] = {EInventoryScreenTab::Gear, EInventoryScreenTab::Pouch, EInventoryScreenTab::Chest};
	constexpr int32 INVENTORY_TAB_COUNT = UE_ARRAY_COUNT(InventoryTabOrder);
	constexpr float TileMarkInset = 6.0f;
	constexpr float TileTickSize = 24.0f;
	constexpr float GridTabIconSize = 36.0f;
	constexpr float GridTabPadding = 9.0f;
	constexpr float LockedTileOpacity = 0.35f;
	// The lock, drawn from two plain blocks until the icon table has one: a shackle over a body.
	constexpr float LockBodyWidth = 16.0f;
	constexpr float LockBodyHeight = 11.0f;
	constexpr float LockShackleWidth = 8.0f;
	constexpr float LockShackleHeight = 6.0f;
	/** A tile that can't go in the slot: darker than a plate. */
	inline const FLinearColor LockedTileColor = FLinearColor::FromSRGBColor(FColor(30, 31, 36));

	/** The worn tick: a small violet corner with a tick in it. */
	UWidget *MakeTick(UWidgetTree *Tree)
	{
		UBorder *Corner = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Corner->SetBrushColor(FocusColor);
		Corner->SetHorizontalAlignment(HAlign_Center);
		Corner->SetVerticalAlignment(VAlign_Center);
		UTextBlock *Mark = MakeText(Tree, SmallFontSize, TextColor, true);
		Mark->SetText(NSLOCTEXT("WoRLoadoutV2Screen", "WornTick", "✓"));
		Corner->SetContent(Mark);
		return MakeSized(Tree, TileTickSize, TileTickSize, Corner);
	}

	/** The lock: a narrow block (the shackle) on a wider one (the body). */
	UWidget *MakeLock(UWidgetTree *Tree)
	{
		UVerticalBox *Lock = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		const auto AddBlock = [Tree, Lock](float Width, float Height)
		{
			UBorder *Block = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Block->SetBrushColor(DimColor);
			AddToColumn(Lock, MakeSized(Tree, Width, Height, Block))->SetHorizontalAlignment(HAlign_Center);
		};
		AddBlock(LockShackleWidth, LockShackleHeight);
		AddBlock(LockBodyWidth, LockBodyHeight);
		return Lock;
	}
	constexpr float BoxPadding = 24.0f;
	// B-485: icon sizes (the mockups' 22 px at 720p, here at 1080p).
	constexpr float RowIconSize = 36.0f;
	constexpr float BadgeSize = 28.0f;
	constexpr float BoxIconSize = 28.0f;
	constexpr float BoxLineGap = 3.0f; // B-487b: the box's rows sit close (Crown)
	/** Spaces per depth of an effect-tree line in the details box. */
	constexpr int32 IndentSpaces = 3;

	/** B-482b: a row's name with what its socket holds in brackets: "Quarry Maul (Garnet)"; the name alone when empty. */
	FText NameWithSocket(const FText &Name, const FText &SocketName)
	{
		return SocketName.IsEmpty() ? Name : FText::Format(LOCTEXT("NameWithSocket", "{0} ({1})"), Name, SocketName);
	}

	FText JoinNames(const TArray<FText> &Names)
	{
		return Names.IsEmpty() ? LOCTEXT("NoneShort", "none") : FText::Join(LOCTEXT("NameSeparator", ", "), Names);
	}

	FText WeaponName(const FWeaponInventoryEntry &Weapon)
	{
		return Weapon.Weapon ? FText::FromString(Weapon.Weapon->GetDisplayName()) : FText::GetEmpty();
	}

	/** Whether the Add / Edit window's slot holds something in the draft. */
	bool IsSlotFilled(const FLoadoutV2Draft &Draft, ELoadoutV2Slot GearSlot)
	{
		switch (GearSlot)
		{
		case ELoadoutV2Slot::MainHand:
			return Draft.MainHand.IsValid() && Draft.MainHand.Weapon;
		case ELoadoutV2Slot::OffHand:
			return Draft.OffHand.IsValid() && Draft.OffHand.Weapon;
		case ELoadoutV2Slot::Adapter:
			return Draft.Adapter.Ring != nullptr;
		case ELoadoutV2Slot::Evolution:
			return Draft.Socket1.IsEvolution() && Draft.Socket1.Evolution.Item;
		case ELoadoutV2Slot::Socket1:
			return !Draft.Socket1.IsEmpty();
		case ELoadoutV2Slot::Socket2:
		default:
			return !Draft.Socket2.IsEmpty();
		}
	}

	/** B-531 (D-230): the piece lines from First on, exactly-same pieces with the same verdict as one line with a count
	 *  (the Inventory tab's helper, WoRLoadoutV2::GroupStacks). */
	void StackPieceLines(TArray<FLoadoutV2WindowItem> &Items, int32 First)
	{
		TArray<FString> Keys;
		for (int32 Index = First; Index < Items.Num(); ++Index)
		{
			const FLoadoutV2WindowItem &Item = Items[Index];
			const FString Key = Item.Weapon.IsValid() ? WoRLoadoutV2::GetStackKey(Item.Weapon) : Item.Adapter.Ring ? WoRLoadoutV2::GetStackKey(Item.Adapter) : FString();
			Keys.Add(Key.IsEmpty() ? Key : Key + TEXT("|") + Item.Refusal.ToString() + TEXT("|") + Item.Line.ToString());
		}
		TArray<FLoadoutV2WindowItem> Stacked(Items.GetData(), First);
		for (const FLoadoutV2Stack &Stack : WoRLoadoutV2::GroupStacks(Keys))
		{
			FLoadoutV2WindowItem &Line = Stacked.Add_GetRef(Items[First + Stack.First]);
			Line.Count = Stack.Count;
			Line.Name = WoRLoadoutV2::GetStackedName(Line.Name, Stack.Count);
		}
		Items = MoveTemp(Stacked);
	}

	FText AttachmentText(const FRuntimeAttachedItem &Item)
	{
		if (Item.IsEmpty())
		{
			return LOCTEXT("SlotEmpty", "—");
		}
		if (Item.IsEvolution())
		{
			return Item.Evolution.Item ? FText::FromString(Item.Evolution.Item->ItemName) : LOCTEXT("SocketEvolution", "Evolution");
		}
		if (Item.IsAugmentStone())
		{
			return LOCTEXT("SocketStone", "Stone");
		}
		// B-503u: the crystal's name and tier ("Garnet (F)")
		return FText::Format(LOCTEXT("SocketCrystalNamed", "{0} ({1})"), WoRGameNames::GetCrystalTypeName(Item.Crystal.Id.Type), WoRGameNames::GetTierLetter(Item.Crystal.Id.Tier));
	}

	/** What A again on a filled socket takes off (today's rule, WoRLoadoutScreen::GetTakeOffSocketWarning, said for A):
	 *  an evolution goes back with wear; a gem or a stone comes out alone and is used up (CL-28b, B-533). Empty with
	 *  OutRefusal when the socket can't be emptied. */
	FText SocketRemoveWarning(const FLoadoutScreenRow &Row, int32 Socket, FText &OutRefusal)
	{
		if (WoRLoadoutScreen::GetTakeOffSocketWarning(Row, Socket, OutRefusal).IsEmpty())
		{
			return FText::GetEmpty();
		}
		const FLoadoutScreenSocket &Taken = Row.Sockets[Socket];
		return Socket == LoadoutScreenSockets::CRYSTAL && Taken.bEvolution
				   ? FText::FormatNamed(LOCTEXT("WarnEvolutionOff", "{Confirm} again: {What} goes back to your evolutions with some wear (it breaks at none)."), TEXT("What"), Taken.Name)
				   : FText::FormatNamed(LOCTEXT("WarnSocketOff", "{Confirm} again: {What} comes out and is destroyed. The other socket keeps its own."), TEXT("What"), Taken.Name);
	}

	/** B-440f: an apparel piece's short line: its tier and what its socket holds ("B · Garnet", "C · empty"). */
	FText ApparelLine(const FLoadoutScreenRow &Row)
	{
		const FLoadoutScreenSocket *Socket = Row.Sockets.IsValidIndex(LoadoutScreenSockets::CRYSTAL) ? &Row.Sockets[LoadoutScreenSockets::CRYSTAL] : nullptr;
		return FText::Format(LOCTEXT("ApparelLine", "{0} · {1}"), Row.Tier, Socket && !Socket->bEmpty ? Socket->Name : LOCTEXT("SocketEmpty", "empty"));
	}

	/** B-482: a click that would confirm an armed ask (Remove all gear, a socket's Remove) waits this long after the
	 *  click that armed it, so a double click only asks. */
	constexpr double ARMED_CLICK_DELAY_SECONDS = 0.5;

	/** Cosmetics isn't built yet: greyed. */
	TArray<FChipState> BuildTabChips()
	{
		// B-482b: no "later" note; a greyed chip already says it isn't in use.
		return {FChipState{LOCTEXT("ChipInventory", "Inventory")}, // B-531 (D-230)
				FChipState{LOCTEXT("ChipWeapons", "Equipment")}, FChipState{LOCTEXT("ChipSkills", "Skills")}, // B-476d: built
				FChipState{LOCTEXT("ChipApparel", "Apparel")}, FChipState{LOCTEXT("ChipCosmetics", "Cosmetics"), true}};
	}

	/** The option lines Y opens, in order. */
	const ELoadoutV2Option OptionOrder[] = {ELoadoutV2Option::SetActive, ELoadoutV2Option::SavedLoadouts, ELoadoutV2Option::RemoveAllGear,
											ELoadoutV2Option::Help, ELoadoutV2Option::CosmeticsShortcuts};
	constexpr int32 OptionCount = UE_ARRAY_COUNT(OptionOrder);
}

ULoadoutV2ScreenWidget::ULoadoutV2ScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself
}

ULoadoutV2ScreenWidget *ULoadoutV2ScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[LoadoutV2] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<ULoadoutV2ScreenWidget>(Root->PushToMenu(ULoadoutV2ScreenWidget::StaticClass()));
}

ULoadoutV2ScreenWidget *ULoadoutV2ScreenWidget::OpenWithLoadout(APlayerController *PC)
{
	ULoadoutV2ScreenWidget *Screen = Open(PC);
	if (Screen)
	{
		if (!Screen->bLoadoutOpen)
		{
			Screen->ToggleLoadout();
		}
		Screen->FocusLoadoutSide();
	}
	return Screen;
}

TSharedRef<SWidget> ULoadoutV2ScreenWidget::RebuildWidget()
{
	using namespace LoadoutV2ScreenStyle;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
		Screen->SetBrushColor(ScreenColor);
		Screen->SetPadding(FMargin(ScreenPadding));
		WidgetTree->RootWidget = Screen;
		UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Screen->SetContent(Main);

		// B-482 (mockup 1_weapons_generic): each character and each tab is a framed chip, LT / RT and LB / RB beside
		// their row; the loadout's name top right; "Cells used" under them.
		UHorizontalBox *Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox *ChipRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		// B-483b: each row in its own panel frame, as wide as its chips (more characters will join the first).
		AddToColumn(ChipRows, MakeChipRowPanel(WidgetTree, PartyConstants::MAX_PARTY_MEMBERS, LOCTEXT("KeySubTabPrev", "{SubTabPrev}"),
											   LOCTEXT("KeySubTabNext", "{SubTabNext}"), MemberChips))
			->SetHorizontalAlignment(HAlign_Left);
		// D-249 (mockups menus/inventory/v3): one tab line: the loadout's tabs first, shown only while the loadout is in,
		// then the inventory's (Gear, Pouch, Chest), always there. LB / RB turn the tabs of the side the keys are on.
		UHorizontalBox *TabLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		LoadoutTabsPanel = MakeChipRowPanel(WidgetTree, LoadoutV2Chip_Count, LOCTEXT("KeyTabPrev", "{TabPrev}"), LOCTEXT("KeyTabNext", "{TabNext}"), TabChips);
		AddToRow(TabLine, LoadoutTabsPanel, FMargin(0.0f, 0.0f, SectionGap, 0.0f));
		AddToRow(TabLine, MakeChipRowPanel(WidgetTree, INVENTORY_TAB_COUNT, LOCTEXT("KeyTabPrev", "{TabPrev}"), LOCTEXT("KeyTabNext", "{TabNext}"), InvTabChips));
		AddToColumn(ChipRows, TabLine, FMargin(0.0f, LineGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Left);
		AddToRow(Header, ChipRows, FMargin(0.0f), VAlign_Top)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		// Top right: the loadout in use (while the loadout is in), and under it the Loadout button (Y, or a click).
		UVerticalBox *HeaderRight = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		LoadoutNameText = MakeText(WidgetTree, RowFontSize, TextColor, true);
		AddToColumn(HeaderRight, LoadoutNameText)->SetHorizontalAlignment(HAlign_Right);
		LoadoutButton = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		LoadoutButton->SetBrushColor(PlateColor);
		LoadoutButton->SetPadding(FMargin(TabPaddingX, TabPaddingY));
		LoadoutButtonText = MakeHintText(WidgetTree, RowFontSize, TextColor, true); // not a hint bar: the button takes the click itself
		LoadoutButton->SetContent(LoadoutButtonText);
		AddToColumn(HeaderRight, LoadoutButton, FMargin(0.0f, LineGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Right);
		AddToRow(Header, HeaderRight, FMargin(0.0f), VAlign_Top);
		// B-482b: as wide as the list and the box, so the loadout's title ends where the box does (it was cut off).
		AddToColumn(Main, MakeSized(WidgetTree, ListWidth + ColumnGap + BoxWidth, 0.0f, Header))->SetHorizontalAlignment(HAlign_Left);
		// B-476c (UIScreens B-479c): the list on the left, the box on the right. B-483: each area in one panel frame:
		// the left side (Cells used, a list's title, the lines) in a panel like the details box's.
		UHorizontalBox *Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UBorder *ListPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ListPanel->SetBrushColor(PanelColor);
		ListPanel->SetPadding(FMargin(BoxPadding));
		UVerticalBox *ListColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ListPanel->SetContent(ListColumn);
		CellsText = MakeText(WidgetTree, RowFontSize, TextColor, true);
		AddToColumn(ListColumn, CellsText, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		TitleText = MakeText(WidgetTree, LineFontSize, DimColor, true);
		AddToColumn(ListColumn, TitleText, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		// B-440e: the saved loadout's new name; shown only while renaming.
		NameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("LoadoutNameBox"));
		NameBox->SetHintText(LOCTEXT("NameHint", "Loadout name"));
		NameBox->OnTextCommitted.AddDynamic(this, &ULoadoutV2ScreenWidget::HandleNameCommitted);
		AddToColumn(ListColumn, NameBox, FMargin(0.0f, 0.0f, 0.0f, LineGap)); // no wrapper: collapsed, it leaves no gap
		SetShown(NameBox, false);
		ListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		// Crown (2026-11-18): a list longer than the panel scrolls inside it and is cut at its edge. The panel hugs a
		// short list as before; a long one stops at the height RefreshView reads from the area.
		ListScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		ListScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Never); // the wheel moves the selection (NativeOnMouseWheel); the view follows it
		ListScroll->AddChild(ListBox);
		AddToColumn(ListColumn, ListScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ListSizer = MakeSized(WidgetTree, ListWidth, 0.0f, ListPanel);
		ListSizer->SetMaxDesiredHeight(ListMaxHeight);
		ListArea = Columns;
		AddToRow(Columns, ListSizer, FMargin(0.0f, 0.0f, ColumnGap, 0.0f), VAlign_Top);
		UBorder *Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Box->SetBrushColor(PanelColor);
		Box->SetPadding(FMargin(BoxPadding));
		UVerticalBox *BoxColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Box->SetContent(BoxColumn);
		// The header. D-248 (Crown): a piece's icon and name, then under it a quiet line, its type with the tier in
		// brackets ("Sword (F)"); no tier badge. (B-487b had the type above the name and the tier as a badge.)
		UHorizontalBox *BoxTitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		BoxTitleIcon = MakeIcon(WidgetTree, BoxIconSize);
		BoxTitleText = MakeText(WidgetTree, RowFontSize, TextColor, true);
		BoxTitleTier = MakeIcon(WidgetTree, BadgeSize);
		AddToRow(BoxTitleRow, BoxTitleIcon, FMargin(0.0f, 0.0f, WordGap, 0.0f), VAlign_Center);
		AddToRow(BoxTitleRow, BoxTitleText, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AddToRow(BoxTitleRow, BoxTitleTier, FMargin(WordGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
		AddToColumn(BoxColumn, BoxTitleRow, FMargin(0.0f, 0.0f, 0.0f, BoxLineGap));
		BoxTypeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		BoxTypeText = MakeText(WidgetTree, SmallFontSize, DimColor);
		AddToRow(BoxTypeRow, BoxTypeText, FMargin(0.0f), VAlign_Center);
		AddToColumn(BoxColumn, BoxTypeRow, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		BoxLines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddToColumn(BoxColumn, BoxLines);

		// D-248 step 1: the picture grid, between the list and the box; shown only while a weapon slot is being filled
		// (RefreshGrid). Its header (the slot, the piece count), the filter tabs, the tiles in a scroll box, the legend.
		UBorder *GridPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		GridPanel->SetBrushColor(PanelColor);
		GridPanel->SetPadding(FMargin(BoxPadding));
		UVerticalBox *GridColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		GridPanel->SetContent(GridColumn);
		UHorizontalBox *GridHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		GridHeaderRow = GridHeader;
		GridTitleText = MakeText(WidgetTree, RowFontSize, TextColor, true);
		GridCountText = MakeText(WidgetTree, LineFontSize, DimColor);
		AddToRow(GridHeader, GridTitleText, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AddToRow(GridHeader, GridCountText, FMargin(0.0f), VAlign_Center);
		AddToColumn(GridColumn, GridHeader, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		GridTabsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddToColumn(GridColumn, GridTabsRow, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
		// The wheel: over a grid with more rows than fit, the scroll box takes it and scrolls (its default); otherwise
		// it reaches NativeOnMouseWheel and moves the lit tile one step.
		GridScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		GridScroll->OnUserScrolled.AddUniqueDynamic(this, &ULoadoutV2ScreenWidget::HandleGridScrolled); // the card moves with its tile
		GridTilePanel = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
		GridTilePanel->SetSlotPadding(FMargin(TilePadding));
		GridScroll->AddChild(GridTilePanel);
		AddToColumn(GridColumn, GridScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UHorizontalBox *GridLegend = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *WornWord = MakeText(WidgetTree, SmallFontSize, DimColor);
		WornWord->SetText(LOCTEXT("GridLegendWorn", "worn"));
		UTextBlock *LockWord = MakeText(WidgetTree, SmallFontSize, DimColor);
		LockWord->SetText(LOCTEXT("GridLegendLocked", "can't go in this slot (why: in the details)"));
		AddToRow(GridLegend, MakeTick(WidgetTree), FMargin(0.0f, 0.0f, WordGap, 0.0f));
		AddToRow(GridLegend, WornWord, FMargin(0.0f, 0.0f, SectionGap, 0.0f));
		AddToRow(GridLegend, MakeLock(WidgetTree), FMargin(0.0f, 0.0f, WordGap, 0.0f));
		AddToRow(GridLegend, LockWord);
		AddToColumn(GridColumn, GridLegend, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		GridSizer = MakeSized(WidgetTree, GridWidth, 0.0f, GridPanel);
		GridSizer->SetMaxDesiredHeight(ListMaxHeight);
		AddToRow(Columns, GridSizer, FMargin(0.0f), VAlign_Top);
		SetShown(GridSizer, false);

		// D-248 step 2: the details box sits on a layer over the columns, not in their row, so it can be the card
		// beside a tile (PlaceBox). Outside the grid it is placed where the row had it: right of the list, at the top.
		// The frame around it is the panel's colour there (unseen) and light while it is the card.
		BoxFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		BoxFrame->SetBrushColor(PanelColor);
		BoxFrame->SetPadding(FMargin(0.0f));
		BoxFrame->SetContent(Box);
		DetailsBox = BoxFrame; // hidden as one when the box has nothing to say
		BoxSizer = MakeSized(WidgetTree, BoxWidth, 0.0f, BoxFrame);
		UCanvasPanel *BoxLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		BoxLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible); // the layer covers the columns: only what is on it takes the mouse
		BoxSizer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot *BoxSlot = BoxLayer->AddChildToCanvas(BoxSizer);
		BoxSlot->SetAutoSize(true);
		BoxSlot->SetPosition(FVector2D(ListWidth + ColumnGap, 0.0f));
		UOverlay *ColumnsLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		for (UWidget *Layer : {static_cast<UWidget *>(Columns), static_cast<UWidget *>(BoxLayer)})
		{
			UOverlaySlot *LayerSlot = ColumnsLayers->AddChildToOverlay(Layer);
			LayerSlot->SetHorizontalAlignment(HAlign_Fill);
			LayerSlot->SetVerticalAlignment(VAlign_Fill);
		}
		AddToColumn(Main, ColumnsLayers, FMargin(0.0f, SectionGap, 0.0f, 0.0f))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		CompareText = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
		AddToColumn(Main, CompareText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		MessageText = MakeHintText(WidgetTree, LineFontSize, WarnColor); // B-481: a message may name a key ("{Confirm} again: ...")
		MessageText->SetAutoWrapText(true);
		AddToColumn(Main, MessageText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		// B-483b: the footer's bar hugs its hints (the shared hint bar draws it) and ends where the details box does.
		HintsText = MakeHintBar(WidgetTree); // B-481 / B-483: key caps of the device in use, each clickable
		AddToColumn(Main, MakeSized(WidgetTree, ListWidth + ColumnGap + BoxWidth, 0.0f, HintsText), FMargin(0.0f, LineGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Left);
	}
	return Super::RebuildWidget();
}

void ULoadoutV2ScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	Message = FText::GetEmpty();
	Mode = ELoadoutV2Mode::List;
	bEditing = false;
	Draft = FLoadoutV2Draft();
	Kinds.Reset();
	Slots.Reset();
	Items.Reset();
	PickedSocket = INDEX_NONE;
	bComparing = false;
	bRemoveArmed = false;
	bRemoveAllArmed = false;
	bRenaming = false;
	WoRMenuStyle::SetShown(NameBox, false);
	SkillEntry = 0; // B-476d: the Skills tab opens on its entries
	SkillRowSelected = 0;
	bOnInvFilters = false;
	if (Tab == ELoadoutV2Tab::Inventory)
	{
		Tab = ELoadoutV2Tab::Weapons; // D-249: the loadout's own Inventory tab is gone
	}
	if (Tab == ELoadoutV2Tab::Skills)
	{
		Mode = ELoadoutV2Mode::SkillEntries;
		Selected = 0;
	}
	RefreshData();
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] Opened: %s; characters: %s; tabs: %s"), *WoRLoadoutV2::GetWeaponsPageString(Page),
		   *WoRMenuStyle::GetChipRowString(MemberChips), *WoRMenuStyle::GetChipRowString(TabChips));
}

UWidget *ULoadoutV2ScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<ULoadoutV2ScreenWidget *>(this); // the screen itself takes the keys (NativeOnKeyDown)
}

bool ULoadoutV2ScreenWidget::FindActive(ULoadoutComponent *&OutLoadout, UInventoryComponent *&OutInventory, const FCombatLoadout *&OutActive) const
{
	OutLoadout = nullptr;
	OutInventory = nullptr;
	OutActive = nullptr;
	if (!Members.IsValidIndex(MemberIndex))
	{
		return false;
	}
	OutLoadout = WoRLoadoutScreen::FindMemberLoadout(GetWorld(), Members[MemberIndex], OutInventory);
	if (!OutLoadout || !OutInventory)
	{
		return false;
	}
	OutActive = &OutInventory->SavedLoadouts[OutInventory->ActiveLoadoutIndex]; // FindMemberLoadout checked the index
	return true;
}

void ULoadoutV2ScreenWidget::RefreshData()
{
	SetHovered(INDEX_NONE, INDEX_NONE, INDEX_NONE); // B-482: the lines may have changed under the mouse; its next move lights again
	HoveredGridTab = INDEX_NONE;
	Members = WoRLoadoutScreen::BuildMembers(GetWorld());
	MemberIndex = Members.Num() > 0 ? FMath::Clamp(MemberIndex, 0, Members.Num() - 1) : 0;
	RefreshInventorySide(); // D-249: the inventory's tab for the shown member
	Page = FLoadoutV2WeaponsPage();
	InventoryPage = FLoadoutV2InventoryPage();
	LoadoutName = FText::GetEmpty();
	ApparelRows.Reset();
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (FindActive(Loadout, Inventory, Active))
	{
		LoadoutName = WoRLoadoutScreen::FormatLoadoutTitle(Active->LoadoutName, Inventory->ActiveLoadoutIndex + 1, Inventory->SavedLoadouts.Num()); // "Bruiser (2 of 5)"
		Page = WoRLoadoutV2::BuildWeaponsPage(*Active, Members[MemberIndex].Class);
		// B-440f: the apparel places as today's gear page lists them (worn first, then the empty ones).
		ApparelRows = WoRLoadoutScreen::BuildGearPage(*Active, Members[MemberIndex].Class).Rows.FilterByPredicate(&WoRLoadoutApparel::IsApparelRow);
		if (IsOnSkillsTab())
		{
			RefreshSkills(); // B-476d
		}
		else if (Mode == ELoadoutV2Mode::WindowKind)
		{
			Kinds = bEditing ? WoRLoadoutV2::BuildEditKinds(*Active, Members[MemberIndex].Class, EditEntry) // B-531 (D-230)
							 : WoRLoadoutV2::BuildAddKinds(*Active, Members[MemberIndex].Class);
		}
		else if (Mode == ELoadoutV2Mode::WindowItems || Mode == ELoadoutV2Mode::WindowAsk)
		{
			BuildItems();
		}
		else if (Mode == ELoadoutV2Mode::Saved)
		{
			Saved = WoRLoadoutSaved::BuildList(*Inventory);
		}
		else if (Mode == ELoadoutV2Mode::ApparelItems)
		{
			BuildApparelItems();
		}
		else if (IsOnInventoryList())
		{
			RefreshInventory(*Inventory, *Active);
		}
	}
	else
	{
		Kinds.Reset();
		Items.Reset();
		Saved = FLoadoutSavedList();
		SkillsPage = FLoadoutV2SkillsPage(); // B-476d
		SkillRows.Reset();
		SkillChoices.Reset();
		if (Mode == ELoadoutV2Mode::SkillRows || Mode == ELoadoutV2Mode::SkillPicker)
		{
			Mode = ELoadoutV2Mode::SkillEntries; // the member left: no slots to change
			Selected = 0;
		}
		bRenaming = false; // the member left: no name to keep, and the keys go back to the screen
		WoRMenuStyle::SetShown(NameBox, false);
		if (IsAdding() || Mode == ELoadoutV2Mode::Saved || Mode == ELoadoutV2Mode::ApparelItems)
		{
			Mode = ELoadoutV2Mode::List; // the member left: nothing to edit
		}
		if (Message.IsEmpty())
		{
			Message = Members.Num() == 0 ? LOCTEXT("NoParty", "There is no party to show.") : LOCTEXT("MemberNotHere", "This character isn't here.");
		}
	}
	Selected = GetLineCount() > 0 ? FMath::Clamp(Selected, 0, GetLineCount() - 1) : 0;
	if (const FLoadoutV2InventoryRow *Row = GetInventoryRow(Selected); Row && !Row->IsPiece() && Selected + 1 < GetLineCount())
	{
		++Selected; // a group's header has nothing to show: its first piece
	}
	if (!Items.IsValidIndex(PickedSocket))
	{
		PickedSocket = INDEX_NONE;
	}
	RefreshView();
}

const FLoadoutV2Entry *ULoadoutV2ScreenWidget::GetFocusedEntry() const
{
	const int32 Line = Mode == ELoadoutV2Mode::List ? Selected : ListSelected;
	return Tab == ELoadoutV2Tab::Weapons && Page.Entries.IsValidIndex(Line) ? &Page.Entries[Line] : nullptr; // the Apparel tab lists no entry
}

int32 ULoadoutV2ScreenWidget::GetWornApparelCount() const
{
	return ApparelRows.FilterByPredicate([](const FLoadoutScreenRow &Row) { return Row.Kind == ELoadoutScreenRowKind::Apparel; }).Num();
}

const FLoadoutScreenRow *ULoadoutV2ScreenWidget::GetFocusedApparelRow() const
{
	if (Tab != ELoadoutV2Tab::Apparel)
	{
		return nullptr;
	}
	const int32 Line = Mode == ELoadoutV2Mode::List ? Selected : ListSelected;
	const int32 Worn = GetWornApparelCount();
	// The worn pieces come first; the Add apparel frame after them stands for the first empty place.
	return ApparelRows.IsValidIndex(FMath::Min(Line, Worn)) ? &ApparelRows[FMath::Min(Line, Worn)] : nullptr;
}

void ULoadoutV2ScreenWidget::RefreshSkills()
{
	SkillsPage = FLoadoutV2SkillsPage();
	SkillRows.Reset();
	SkillChoices.Reset();
	if (!Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	SkillsPage = WoRLoadoutV2Skills::Build(WoRLoadoutSkills::BuildSkillsPage(GetWorld(), Members[MemberIndex]), Page);
	if (!SkillsPage.Source.Refusal.IsEmpty() && Message.IsEmpty())
	{
		Message = SkillsPage.Source.Refusal; // why there are no entries (the member isn't in this level)
	}
	if (Mode == ELoadoutV2Mode::SkillEntries)
	{
		return;
	}
	if (!SkillsPage.Entries.IsValidIndex(SkillEntry))
	{
		Mode = ELoadoutV2Mode::SkillEntries; // the entry went (gear changed elsewhere)
		Selected = 0;
		return;
	}
	SkillRows = WoRLoadoutV2Skills::GetRows(SkillsPage, SkillEntry);
	if (Mode != ELoadoutV2Mode::SkillPicker)
	{
		return;
	}
	const FLoadoutScreenRow *Row = SkillRows.IsValidIndex(SkillRowSelected) ? WoRLoadoutV2Skills::FindRow(SkillsPage, SkillRows[SkillRowSelected]) : nullptr;
	if (!Row)
	{
		Mode = ELoadoutV2Mode::SkillRows;
		Selected = 0;
		return;
	}
	SkillChoices = WoRLoadoutSkills::BuildSkillChoices(GetWorld(), Members[MemberIndex], SkillsPage.Source, SkillRows[SkillRowSelected].Tab, *Row);
}

void ULoadoutV2ScreenWidget::LogSkillsTab() const
{
	UE_LOG(LogWoRUI, Log, TEXT("%s"), *WoRLoadoutV2Skills::GetSkillsString(SkillsPage)); // B-476d debug: the tab as shown
}

void ULoadoutV2ScreenWidget::RefreshInventorySide()
{
	InvFilters = WoRInventoryScreen::GetFilters(InvTab);
	if (!InvFilters.Contains(InvFilter))
	{
		InvFilter = EInventoryScreenFilter::All; // the tab has no such filter (the Pouch has none)
	}
	bOnInvFilters = bOnInvFilters && InvFilters.Num() > 0;
	InvPage = Members.IsValidIndex(MemberIndex) ? WoRInventoryScreen::BuildPage(GetWorld(), Members[MemberIndex].Key, InvTab, InvFilter, InvSort) : FInventoryScreenPage();
	InvSelected = InvPage.Tiles.Num() > 0 ? FMath::Clamp(InvSelected, 0, InvPage.Tiles.Num() - 1) : 0;
	HoveredInvTile = INDEX_NONE; // the tiles may have changed under the mouse; its next move lights again
}

TArray<FLoadoutV2GridTile> ULoadoutV2ScreenWidget::GetInventoryTiles() const
{
	TArray<FLoadoutV2GridTile> Tiles;
	for (const FInventoryScreenTile &Each : InvPage.Tiles)
	{
		FLoadoutV2GridTile &Tile = Tiles.AddDefaulted_GetRef();
		Tile.Icon = Each.Icon;
		if (Tile.Icon.Name.IsEmpty())
		{
			Tile.Icon.Name = Each.Mark; // the word on a tile with no picture: the inventory screen's letter
		}
		Tile.Count = Each.IsStack() ? FMath::Max(Each.Row.Count, 1) : 1;
		Tile.bWorn = Each.bWorn;
	}
	return Tiles;
}

void ULoadoutV2ScreenWidget::RefreshDetails()
{
	Details.Reset();
	if (IsInventoryBox())
	{
		// D-249: the lit or hovered inventory tile, as the inventory screen's panel words it: its name, under it its
		// kind and tier, its lines and its note.
		const int32 TileIndex = GetInventoryBoxTile();
		if (bOnInvFilters || !InvPage.Tiles.IsValidIndex(TileIndex))
		{
			return;
		}
		const FInventoryScreenTile &Tile = InvPage.Tiles[TileIndex];
		FLoadoutV2DetailLine &Piece = Details.AddDefaulted_GetRef();
		Piece.Kind = ELoadoutV2DetailKind::Piece;
		Piece.Label = Tile.Name;
		Piece.Value = Tile.Tier;
		Piece.Icon = Tile.Icon;
		Piece.TypeName = Tile.KindLine;
		Piece.bHasTier = !Tile.Tier.IsEmpty();
		Piece.Tier = Tile.TierValue;
		for (const FLoadoutScreenDetail &Detail : Tile.Details)
		{
			if (Detail.Label.IsEmpty())
			{
				Details.Add(WoRLoadoutV2::MakeNoteLine(Detail.Value));
				continue;
			}
			FLoadoutV2DetailLine &Line = Details.AddDefaulted_GetRef();
			Line.Label = Detail.Label;
			Line.Value = Detail.Value;
		}
		if (!Tile.Note.IsEmpty())
		{
			Details.Add(WoRLoadoutV2::MakeNoteLine(Tile.Note));
		}
		return;
	}
	if (Mode == ELoadoutV2Mode::WindowItems || Mode == ELoadoutV2Mode::WindowAsk)
	{
		// B-484: in a slot's list, the highlighted (or hovered) piece alone.
		if (Items.IsValidIndex(GetBoxLine()))
		{
			Details = BuildItemDetails(Items[GetBoxLine()]);
		}
		return;
	}
	if (Mode == ELoadoutV2Mode::WindowSlots)
	{
		// B-503u (Crown): the box is the lit or hovered slot's own piece, as B-484's lists; never the whole entry.
		if (Slots.IsValidIndex(GetBoxLine()))
		{
			Details = BuildSlotDetails(Slots[GetBoxLine()]);
		}
		return;
	}
	if (IsOnSkillsTab())
	{
		// B-476d: the box shows the focused entry's slots, or what the highlighted skill (a slot's, a pick's) does.
		if (Mode == ELoadoutV2Mode::SkillEntries)
		{
			for (const FLoadoutV2SkillRowRef &Ref : WoRLoadoutV2Skills::GetRows(SkillsPage, Selected))
			{
				if (const FLoadoutScreenRow *Row = WoRLoadoutV2Skills::FindRow(SkillsPage, Ref))
				{
					FLoadoutV2DetailLine Line;
					Line.Label = Row->Skill ? Row->Name : LOCTEXT("EmptySkillSlot", "Empty slot");
					Line.Value = Row->KindLine;
					Details.Add(MoveTemp(Line));
				}
			}
			return;
		}
		const int32 Line = GetBoxLine(); // the line under the mouse, else the focused one (as B-484's lists)
		const FLoadoutScreenRow *Row = Mode == ELoadoutV2Mode::SkillPicker ? (SkillChoices.IsValidIndex(Line) ? &SkillChoices[Line].Row : nullptr)
									 : SkillRows.IsValidIndex(Line)		   ? WoRLoadoutV2Skills::FindRow(SkillsPage, SkillRows[Line])
																		   : nullptr;
		if (Row)
		{
			Details = WoRLoadoutV2Skills::BuildSkillDetails(Row->Skill, bSimpleView, Row->KindLine);
		}
		return;
	}
	if (const FLoadoutV2InventoryRow *Row = GetInventoryRow(GetBoxLine()))
	{
		// B-531: the lit or hovered piece alone, as a slot's list shows it.
		switch (Row->Kind)
		{
		case ELoadoutV2InventoryRowKind::Weapon:
			Details = WoRLoadoutV2::BuildWeaponDetails(Row->Weapon, bSimpleView);
			break;
		case ELoadoutV2InventoryRowKind::Adapter:
			Details = WoRLoadoutV2::BuildAdapterDetails(Row->Adapter, bSimpleView);
			break;
		case ELoadoutV2InventoryRowKind::Evolution:
			Details = WoRLoadoutV2::BuildAttachmentDetails(WoRLoadoutV2::MakeEvolutionAttachment(Row->Evolution), bSimpleView);
			break;
		case ELoadoutV2InventoryRowKind::Crystal:
			Details = WoRLoadoutV2::BuildAttachmentDetails(Row->Crystal, bSimpleView);
			break;
		default:
			break;
		}
		return;
	}
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	const FLoadoutV2Entry *Entry = GetFocusedEntry();
	if (Entry && FindActive(Loadout, Inventory, Active))
	{
		Details = WoRLoadoutV2::BuildEntryDetails(*Active, *Entry, bSimpleView);
	}
}

// ==================== LINES ====================

int32 ULoadoutV2ScreenWidget::GetLineCount() const
{
	switch (Mode)
	{
	case ELoadoutV2Mode::Options:
		return LoadoutV2ScreenStyle::OptionCount;
	case ELoadoutV2Mode::Saved:
		return Saved.Rows.Num();
	case ELoadoutV2Mode::WindowKind:
		return Kinds.Num();
	case ELoadoutV2Mode::WindowSlots:
		return Slots.Num();
	case ELoadoutV2Mode::SkillEntries:
		return SkillsPage.Entries.Num();
	case ELoadoutV2Mode::SkillRows:
		return SkillRows.Num();
	case ELoadoutV2Mode::SkillPicker:
		return SkillChoices.Num();
	case ELoadoutV2Mode::WindowItems:
	case ELoadoutV2Mode::WindowAsk:
	case ELoadoutV2Mode::ApparelItems:
		return Items.Num();
	case ELoadoutV2Mode::List:
	default:
		if (Tab == ELoadoutV2Tab::Inventory)
		{
			return InventoryPage.Rows.Num();
		}
		if (Tab == ELoadoutV2Tab::Apparel)
		{
			const int32 Worn = GetWornApparelCount();
			return Worn + (Worn < ApparelRows.Num() ? 1 : 0); // the Add apparel frame last, while a place is free
		}
		return Page.Entries.Num() + (Page.bCanAdd ? 1 : 0); // the Add weapon frame last
	}
}

FText ULoadoutV2ScreenWidget::GetLineName(int32 Line) const
{
	using namespace LoadoutV2ScreenStyle;
	switch (Mode)
	{
	case ELoadoutV2Mode::Options:
		switch (OptionOrder[Line])
		{
		case ELoadoutV2Option::SetActive:
			return LOCTEXT("OptionSetActive", "Set active");
		case ELoadoutV2Option::SavedLoadouts:
			return LOCTEXT("OptionSaved", "Saved loadouts");
		case ELoadoutV2Option::RemoveAllGear:
			return LOCTEXT("OptionRemoveAll", "Remove all gear");
		case ELoadoutV2Option::Help:
			return LOCTEXT("OptionHelp", "Help");
		case ELoadoutV2Option::CosmeticsShortcuts:
		default:
			return LOCTEXT("OptionCosmetics", "Cosmetics shortcuts");
		}
	case ELoadoutV2Mode::Saved:
		return Saved.Rows.IsValidIndex(Line) ? Saved.Rows[Line].Name : FText::GetEmpty();
	case ELoadoutV2Mode::WindowKind:
		return Kinds.IsValidIndex(Line) ? Kinds[Line].Name : FText::GetEmpty();
	case ELoadoutV2Mode::WindowSlots:
	{
		if (!Slots.IsValidIndex(Line))
		{
			return FText::GetEmpty();
		}
		// Crown (2026-11-18): a filled slot leads with its piece (the slot's word is the quiet text beside it,
		// GetLineDetail); an empty one leads with the slot's word.
		// D-248: the tier is in the quiet line under the name (GetLineSubName), not a badge or a letter after it.
		return LoadoutV2ScreenStyle::IsSlotFilled(Draft, Slots[Line]) ? GetSlotValue(Slots[Line]) : GetSlotName(Slots[Line]);
	}
	case ELoadoutV2Mode::SkillEntries:
		return SkillsPage.Entries.IsValidIndex(Line) ? SkillsPage.Entries[Line].Name : FText::GetEmpty();
	case ELoadoutV2Mode::SkillRows:
	{
		const FLoadoutScreenRow *Row = SkillRows.IsValidIndex(Line) ? WoRLoadoutV2Skills::FindRow(SkillsPage, SkillRows[Line]) : nullptr;
		return Row && Row->Skill ? Row->Name : LOCTEXT("EmptySlotName", "Empty slot");
	}
	case ELoadoutV2Mode::SkillPicker:
		return SkillChoices.IsValidIndex(Line) ? SkillChoices[Line].Row.Name : FText::GetEmpty();
	case ELoadoutV2Mode::WindowItems:
	case ELoadoutV2Mode::WindowAsk:
	case ELoadoutV2Mode::ApparelItems:
		if (!Items.IsValidIndex(Line))
		{
			return FText::GetEmpty();
		}
	{
		// B-484: EQUIPPED / PICKED is the row's tag (GetLineTag). B-485: the tier is a badge when the table has one.
		if (Items[Line].TierText.IsEmpty())
		{
			return Items[Line].Name;
		}
		const FLineIcons Icons = GetLineIcons(Line);
		const bool bTierBadge = Icons.bHasTier && WoRIcons::GetTierBadge(Icons.Tier) != nullptr;
		return bTierBadge ? Items[Line].Name : FText::Format(LOCTEXT("SocketItemName", "{0}  {1}"), Items[Line].Name, Items[Line].TierText);
	}
	case ELoadoutV2Mode::List:
	default:
		if (const FLoadoutV2InventoryRow *Row = GetInventoryRow(Line))
		{
			// As the Equipment list: the tier is a badge when the table has one, else its letter after the name.
			return !Row->bHasTier || WoRIcons::GetTierBadge(Row->Tier) ? Row->Name
																	  : FText::Format(LOCTEXT("InventoryNameTier", "{0}  {1}"), Row->Name, WoRGameNames::GetTierLetter(Row->Tier));
		}
		if (Tab == ELoadoutV2Tab::Apparel)
		{
			if (Line < GetWornApparelCount() && ApparelRows.IsValidIndex(Line))
			{
				const FLoadoutScreenRow &Row = ApparelRows[Line];
				const FLoadoutScreenSocket *Socket = Row.Sockets.IsValidIndex(LoadoutScreenSockets::CRYSTAL) ? &Row.Sockets[LoadoutScreenSockets::CRYSTAL] : nullptr;
				return NameWithSocket(Row.Name, Socket && !Socket->bEmpty ? Socket->Name : FText::GetEmpty());
			}
			return LOCTEXT("AddApparel", "+ Add apparel");
		}
		if (Page.Entries.IsValidIndex(Line))
		{
			const FLoadoutV2Entry &Entry = Page.Entries[Line];
			// B-485: the socket shows as a badge when the icon table has one; else its name in brackets, as before.
			const bool bBadge = Entry.SocketBadges.Num() > 0 && FindIcon(Entry.SocketBadges[0]) != nullptr;
			const FText Name = bBadge ? Entry.Name : NameWithSocket(Entry.Name, Entry.SocketName);
			// D-248: a weapon's or adapter's tier is in the quiet line under its name (GetLineSubName). B-516b stays for
			// the character's own evolution: a badge when the table has one, else its letter after the name.
			return Entry.Kind != ELoadoutV2EntryKind::Evolution || WoRIcons::GetTierBadge(Entry.Tier)
					   ? Name
					   : FText::Format(LOCTEXT("EntryNameTier", "{0}  {1}"), Name, WoRGameNames::GetTierLetter(Entry.Tier));
		}
		return LOCTEXT("AddWeapon", "+ Add equipment");
	}
}

FText ULoadoutV2ScreenWidget::GetLineSubName(int32 Line) const
{
	if (Mode == ELoadoutV2Mode::WindowSlots && Slots.IsValidIndex(Line) && LoadoutV2ScreenStyle::IsSlotFilled(Draft, Slots[Line]))
	{
		switch (Slots[Line])
		{
		case ELoadoutV2Slot::MainHand:
			return WoRLoadoutV2Grid::GetTypeTierLine(WoRGameNames::GetHeldWeaponTypeName(Draft.MainHand.Weapon), Draft.MainHand.Tier);
		case ELoadoutV2Slot::OffHand:
			return WoRLoadoutV2Grid::GetTypeTierLine(WoRGameNames::GetHeldWeaponTypeName(Draft.OffHand.Weapon), Draft.OffHand.Tier);
		case ELoadoutV2Slot::Adapter:
			return WoRLoadoutV2Grid::GetTypeTierLine(WoRGameNames::GetAdapterWord(), Draft.Adapter.Tier);
		default:
			return FText::GetEmpty(); // a socket's text has its tier ("Garnet (F)")
		}
	}
	if (Mode == ELoadoutV2Mode::List && Tab == ELoadoutV2Tab::Weapons && Page.Entries.IsValidIndex(Line))
	{
		const FLoadoutV2Entry &Entry = Page.Entries[Line];
		return Entry.Kind == ELoadoutV2EntryKind::Weapon	? WoRLoadoutV2Grid::GetTypeTierLine(Entry.TypeIcon.Name, Entry.Tier) // a pair: what the two make, the main hand's tier
			 : Entry.Kind == ELoadoutV2EntryKind::Adapter ? WoRLoadoutV2Grid::GetTypeTierLine(WoRGameNames::GetAdapterWord(), Entry.Tier)
														  : FText::GetEmpty();
	}
	return FText::GetEmpty();
}

FText ULoadoutV2ScreenWidget::GetLineDetail(int32 Line) const
{
	using namespace LoadoutV2ScreenStyle;
	switch (Mode)
	{
	case ELoadoutV2Mode::Options:
	{
		const FLoadoutV2Entry *Entry = GetFocusedEntry();
		switch (OptionOrder[Line])
		{
		case ELoadoutV2Option::SetActive:
			return Entry && Entry->Kind == ELoadoutV2EntryKind::Weapon && !Entry->bActive
					   ? FText::FormatNamed(LOCTEXT("OptionSetActiveLine", "{What} becomes the active weapon ({SetActive})"), TEXT("What"), Entry->Name)
					   : LOCTEXT("OptionSetActiveOff", "Focus a weapon entry that isn't active");
		case ELoadoutV2Option::SavedLoadouts:
			return LOCTEXT("OptionSavedList", "Use another, make a new one, duplicate or rename"); // B-440e
		case ELoadoutV2Option::RemoveAllGear:
			return bRemoveAllArmed ? LOCTEXT("OptionRemoveAllArmed", "{Confirm} again: every entry but the first comes out")
								   : LOCTEXT("OptionRemoveAllLine", "Asks first");
		case ELoadoutV2Option::Help:
			return LOCTEXT("OptionHelpLine", "What each part of this screen does");
		case ELoadoutV2Option::CosmeticsShortcuts:
		default:
			return LOCTEXT("OptionCosmeticsLine", "Later");
		}
	}
	case ELoadoutV2Mode::Saved:
		return Saved.Rows.IsValidIndex(Line) ? Saved.Rows[Line].Note : FText::GetEmpty();
	case ELoadoutV2Mode::WindowKind:
		return Kinds.IsValidIndex(Line) ? Kinds[Line].Refusal : FText::GetEmpty();
	case ELoadoutV2Mode::WindowSlots:
		if (!Slots.IsValidIndex(Line))
		{
			return FText::GetEmpty();
		}
		return LoadoutV2ScreenStyle::IsSlotFilled(Draft, Slots[Line]) ? GetSlotName(Slots[Line]) : GetSlotValue(Slots[Line]); // filled: which slot it is
	case ELoadoutV2Mode::SkillEntries:
		return SkillsPage.Entries.IsValidIndex(Line)
				   ? FText::Format(LOCTEXT("SkillEntryCount", "{0}/{1}"), SkillsPage.Entries[Line].Filled, SkillsPage.Entries[Line].Total)
				   : FText::GetEmpty();
	case ELoadoutV2Mode::SkillRows:
	{
		const FLoadoutScreenRow *Row = SkillRows.IsValidIndex(Line) ? WoRLoadoutV2Skills::FindRow(SkillsPage, SkillRows[Line]) : nullptr;
		return Row ? Row->KindLine : FText::GetEmpty();
	}
	case ELoadoutV2Mode::SkillPicker:
		if (!SkillChoices.IsValidIndex(Line))
		{
			return FText::GetEmpty();
		}
		return SkillChoices[Line].Refusal.IsEmpty() ? SkillChoices[Line].Row.KindLine : SkillChoices[Line].Refusal;
	case ELoadoutV2Mode::WindowItems:
	case ELoadoutV2Mode::WindowAsk:
	case ELoadoutV2Mode::ApparelItems:
		if (!Items.IsValidIndex(Line))
		{
			return FText::GetEmpty();
		}
		return Items[Line].Refusal.IsEmpty() ? Items[Line].Line : Items[Line].Refusal;
	case ELoadoutV2Mode::List:
	default:
		if (Tab == ELoadoutV2Tab::Inventory)
		{
			return FText::GetEmpty(); // the name says it all ("Adapter Ring x5"); the box has the rest
		}
		if (Tab == ELoadoutV2Tab::Apparel)
		{
			const int32 Worn = GetWornApparelCount();
			return Line < Worn && ApparelRows.IsValidIndex(Line)
					   ? FText::GetEmpty() // B-482b: the socket is in the name's brackets; the tier is in the box
					   : FText::Format(LOCTEXT("AddApparelLine", "{0}/{1} left"), FText::AsNumber(ApparelRows.Num() - Worn), FText::AsNumber(ApparelRows.Num()));
		}
		if (Page.Entries.IsValidIndex(Line))
		{
			return FText::GetEmpty(); // B-484: ACTIVE is the row's tag (GetLineTag)
		}
		return Page.AddFrameLine;
	}
}

UTexture2D *ULoadoutV2ScreenWidget::FindIcon(const FLoadoutV2Icon &Icon)
{
	return WoRIcons::Find(Icon); // B-487: the shared lookup
}

ULoadoutV2ScreenWidget::FLineIcons ULoadoutV2ScreenWidget::GetLineIcons(int32 Line) const
{
	FLineIcons Icons;
	if (const FLoadoutV2InventoryRow *Row = GetInventoryRow(Line))
	{
		// As the Add window's lines: a weapon's or adapter's icon at the start; a crystal's or evolution's badge after the name.
		const bool bPieceIcon = Row->Kind == ELoadoutV2InventoryRowKind::Weapon || Row->Kind == ELoadoutV2InventoryRowKind::Adapter;
		(bPieceIcon ? Icons.Type : Icons.Socket1) = Row->Icon;
		Icons.bHasTier = Row->bHasTier;
		Icons.Tier = Row->Tier;
		return Icons;
	}
	if (Mode == ELoadoutV2Mode::List || Mode == ELoadoutV2Mode::SkillEntries)
	{
		if (const FLoadoutV2Entry *Entry = GetRowEntry(Line))
		{
			Icons.Type = Entry->TypeIcon;
			Icons.Socket1 = Entry->SocketBadges.IsValidIndex(0) ? Entry->SocketBadges[0] : FLoadoutV2Icon();
			Icons.Socket2 = Entry->SocketBadges.IsValidIndex(1) ? Entry->SocketBadges[1] : FLoadoutV2Icon();
			// D-248: a weapon's or adapter's tier is written under its name (GetLineSubName), no badge. B-516b's badge
			// stays on the character's own evolution.
			Icons.bHasTier = Mode == ELoadoutV2Mode::List && Entry->Kind == ELoadoutV2EntryKind::Evolution;
			Icons.Tier = Entry->Tier;
		}
		return Icons;
	}
	if (Mode == ELoadoutV2Mode::WindowSlots && Slots.IsValidIndex(Line) && LoadoutV2ScreenStyle::IsSlotFilled(Draft, Slots[Line]))
	{
		// A filled slot's piece: its icon. D-248: its tier is written under its name (GetLineSubName), no badge.
		switch (Slots[Line])
		{
		case ELoadoutV2Slot::MainHand:
		case ELoadoutV2Slot::OffHand:
		{
			const FWeaponInventoryEntry &Piece = Slots[Line] == ELoadoutV2Slot::MainHand ? Draft.MainHand : Draft.OffHand;
			Icons.Type.Kind = ELoadoutV2IconKind::WeaponType;
			Icons.Type.WeaponType = Piece.Weapon->WeaponType;
			Icons.Type.Name = WoRGameNames::GetHeldWeaponTypeName(Piece.Weapon);
			break;
		}
		case ELoadoutV2Slot::Adapter:
		{
			FRingInventoryEntry Adapter = Draft.Adapter;
			Adapter.AttachedItem = Draft.Socket1; // the entry's socket, as the draft holds it
			Icons.Type = WoRLoadoutV2::MakeAdapterIcon(Adapter);
			break;
		}
		case ELoadoutV2Slot::Evolution:
		case ELoadoutV2Slot::Socket1:
		case ELoadoutV2Slot::Socket2:
		default:
		{
			const FRuntimeAttachedItem &Item = Slots[Line] == ELoadoutV2Slot::Socket2 ? Draft.Socket2 : Draft.Socket1;
			Icons.Socket1 = WoRLoadoutV2::MakeSocketIcon(Item); // no tier badge: the text has it ("Garnet (F)", AttachmentText)
			break;
		}
		}
		return Icons;
	}
	if ((Mode != ELoadoutV2Mode::WindowItems && Mode != ELoadoutV2Mode::WindowAsk) || !Items.IsValidIndex(Line))
	{
		return Icons;
	}
	const FLoadoutV2WindowItem &Item = Items[Line];
	if (Item.Weapon.IsValid() && Item.Weapon.Weapon)
	{
		Icons.Type.Kind = ELoadoutV2IconKind::WeaponType;
		Icons.Type.WeaponType = Item.Weapon.Weapon->WeaponType;
		Icons.Type.Name = WoRGameNames::GetHeldWeaponTypeName(Item.Weapon.Weapon); // B-513
		return Icons;
	}
	if (Item.Adapter.Ring)
	{
		Icons.Type = WoRLoadoutV2::MakeAdapterIcon(Item.Adapter); // B-530 (D-229): the frame with its crystal
		return Icons;
	}
	// A crystal, stone or evolution line: its badge, and its tier.
	const FRuntimeAttachedItem Attachment = Item.Evolution.InstanceID.IsValid() ? WoRLoadoutV2::MakeEvolutionAttachment(Item.Evolution) : Item.Socket.Attachment;
	if (!Attachment.IsEmpty())
	{
		Icons.Socket1 = WoRLoadoutV2::MakeSocketIcon(Attachment);
		Icons.bHasTier = WoRLoadoutV2::GetAttachmentTier(Attachment, Icons.Tier);
	}
	return Icons;
}

FText ULoadoutV2ScreenWidget::GetLineTag(int32 Line) const
{
	switch (Mode)
	{
	case ELoadoutV2Mode::WindowItems:
	case ELoadoutV2Mode::WindowAsk:
	case ELoadoutV2Mode::ApparelItems:
		if (!Items.IsValidIndex(Line))
		{
			return FText::GetEmpty();
		}
		return Items[Line].bEquipped ? LOCTEXT("EquippedTag", "EQUIPPED") : Line == PickedSocket ? LOCTEXT("PickedTag", "PICKED") : FText::GetEmpty();
	case ELoadoutV2Mode::List:
	case ELoadoutV2Mode::SkillEntries:
	{
		const FLoadoutV2Entry *Entry = GetRowEntry(Line);
		return Entry && Entry->bActive ? LOCTEXT("ActiveTag", "ACTIVE") : FText::GetEmpty();
	}
	default:
		return FText::GetEmpty();
	}
}

const FLoadoutV2Entry *ULoadoutV2ScreenWidget::GetRowEntry(int32 Line) const
{
	if (Mode == ELoadoutV2Mode::SkillEntries)
	{
		Line = SkillsPage.Entries.IsValidIndex(Line) ? SkillsPage.Entries[Line].WeaponEntry : INDEX_NONE;
	}
	else if (Mode != ELoadoutV2Mode::List || Tab != ELoadoutV2Tab::Weapons)
	{
		return nullptr;
	}
	return Page.Entries.IsValidIndex(Line) ? &Page.Entries[Line] : nullptr;
}

TArray<FLoadoutV2DetailLine> ULoadoutV2ScreenWidget::BuildSlotDetails(ELoadoutV2Slot GearSlot) const
{
	switch (GearSlot)
	{
	case ELoadoutV2Slot::MainHand:
		return Draft.MainHand.IsValid() ? WoRLoadoutV2::BuildWeaponDetails(Draft.MainHand, bSimpleView)
										: TArray<FLoadoutV2DetailLine>{WoRLoadoutV2::MakeNoteLine(LOCTEXT("MainHandTakes", "Pick the weapon this entry holds."))};
	case ELoadoutV2Slot::OffHand:
		return WoRLoadoutV2::BuildOffHandDetails(Draft.OffHand, bSimpleView);
	case ELoadoutV2Slot::Adapter:
		return Draft.Adapter.Ring ? WoRLoadoutV2::BuildAdapterDetails(Draft.Adapter, bSimpleView)
								  : TArray<FLoadoutV2DetailLine>{WoRLoadoutV2::MakeNoteLine(LOCTEXT("AdapterTakes", "Pick the adapter this entry holds."))};
	case ELoadoutV2Slot::Evolution:
		return Draft.Socket1.IsEmpty() ? TArray<FLoadoutV2DetailLine>{WoRLoadoutV2::MakeNoteLine(LOCTEXT("EvolutionTakes", "Pick the evolution."))}
									   : WoRLoadoutV2::BuildAttachmentDetails(Draft.Socket1, bSimpleView);
	case ELoadoutV2Slot::Socket1:
		return WoRLoadoutV2::BuildSocketDetails(Draft.Socket1, /*bFirstSocket=*/true, bSimpleView);
	case ELoadoutV2Slot::Socket2:
	default:
		return WoRLoadoutV2::BuildSocketDetails(Draft.Socket2, /*bFirstSocket=*/false, bSimpleView);
	}
}

TArray<FLoadoutV2DetailLine> ULoadoutV2ScreenWidget::BuildItemDetails(const FLoadoutV2WindowItem &Item) const
{
	if (Item.Weapon.IsValid())
	{
		return WoRLoadoutV2::BuildWeaponDetails(Item.Weapon, bSimpleView);
	}
	if (Item.Adapter.IsValid())
	{
		return WoRLoadoutV2::BuildAdapterDetails(Item.Adapter, bSimpleView);
	}
	if (Item.Evolution.InstanceID.IsValid())
	{
		return WoRLoadoutV2::BuildAttachmentDetails(WoRLoadoutV2::MakeEvolutionAttachment(Item.Evolution), bSimpleView);
	}
	return WoRLoadoutV2::BuildAttachmentDetails(Item.Socket.Attachment, bSimpleView); // a socket line; nothing for a note
}

bool ULoadoutV2ScreenWidget::IsLineRefused(int32 Line) const
{
	using namespace LoadoutV2ScreenStyle;
	switch (Mode)
	{
	case ELoadoutV2Mode::Options:
	{
		const ELoadoutV2Option Option = OptionOrder[Line];
		const FLoadoutV2Entry *Entry = GetFocusedEntry();
		const bool bSetActive = Option == ELoadoutV2Option::SetActive && Entry && Entry->Kind == ELoadoutV2EntryKind::Weapon && !Entry->bActive;
		return Option == ELoadoutV2Option::CosmeticsShortcuts || (Option == ELoadoutV2Option::SetActive && !bSetActive);
	}
	case ELoadoutV2Mode::WindowKind:
		return Kinds.IsValidIndex(Line) && !Kinds[Line].Refusal.IsEmpty();
	case ELoadoutV2Mode::SkillPicker:
		return SkillChoices.IsValidIndex(Line) && !SkillChoices[Line].Refusal.IsEmpty();
	case ELoadoutV2Mode::WindowItems:
	case ELoadoutV2Mode::WindowAsk:
	case ELoadoutV2Mode::ApparelItems:
		return Items.IsValidIndex(Line) && !Items[Line].Refusal.IsEmpty();
	default:
	{
		const FLoadoutV2InventoryRow *Row = GetInventoryRow(Line);
		return Row && !Row->IsPiece(); // B-531: a group's header, dim: nothing to pick
	}
	}
}

FText ULoadoutV2ScreenWidget::GetTitle() const
{
	if (Mode == ELoadoutV2Mode::List)
	{
		return FText::GetEmpty();
	}
	if (Mode == ELoadoutV2Mode::Options)
	{
		return LOCTEXT("TitleOptions", "Options");
	}
	if (IsOnSkillsTab())
	{
		const FText Header = SkillsPage.Entries.IsValidIndex(SkillEntry) ? SkillsPage.Entries[SkillEntry].Header : FText::GetEmpty();
		return Mode == ELoadoutV2Mode::SkillEntries ? FText::GetEmpty()
			 : Mode == ELoadoutV2Mode::SkillPicker	? FText::Format(LOCTEXT("TitlePicker", "{0} \u00B7 pick a skill"), Header)
													: Header;
	}
	if (Mode == ELoadoutV2Mode::Saved)
	{
		return FText::Format(LOCTEXT("TitleSaved", "Saved loadouts ({0} of {1} of your own)"), FText::AsNumber(Saved.CustomCount), FText::AsNumber(Saved.MaxCustom));
	}
	if (Mode == ELoadoutV2Mode::ApparelItems)
	{
		const FLoadoutScreenRow *Row = GetFocusedApparelRow();
		return Row && Row->Kind == ELoadoutScreenRowKind::Apparel ? FText::Format(LOCTEXT("TitleEdit", "Edit · {0}"), Row->Name) : LOCTEXT("TitleAddApparel", "Add apparel");
	}
	const FText Window = bEditing ? FText::Format(LOCTEXT("TitleEdit", "Edit · {0}"), EditEntry.Name) : LOCTEXT("TitleAdd", "Add equipment");
	if (Mode == ELoadoutV2Mode::WindowItems || Mode == ELoadoutV2Mode::WindowAsk)
	{
		return Slots.IsValidIndex(SlotIndex) ? FText::Format(LOCTEXT("TitleSlot", "{0} · {1}"), Window, GetSlotName(Slots[SlotIndex])) : Window;
	}
	return Window;
}

FText ULoadoutV2ScreenWidget::GetHints() const
{
	if (!bOnLoadoutSide && IsInventoryGrid())
	{
		// D-249: the keys are on the inventory's side.
		return bLoadoutOpen ? LOCTEXT("HintsInventorySideOpen", "{Arrows}  Left at the edge: loadout        {Tabs}  Tab        {SubTabs}  Character        {Help}  Sort        {Second}  Close loadout        {Back}  Back")
							: LOCTEXT("HintsInventorySide", "{Tabs}  Tab        {SubTabs}  Character        {Help}  Sort        {Second}  Loadout        {Back}  Back");
	}
	switch (Mode)
	{
	case ELoadoutV2Mode::Options:
		return LOCTEXT("HintsOptions", "{Confirm}  Select        {Back}  Close");
	case ELoadoutV2Mode::Saved:
		if (bRenaming)
		{
			return LOCTEXT("HintsRenaming", "Enter  Keep the name        Esc  Cancel");
		}
		return Saved.Rows.IsValidIndex(Selected) && Saved.Rows[Selected].IsNewLine()
				   ? LOCTEXT("HintsSavedNew", "{Confirm}  New loadout        {Back}  Back")
				   : LOCTEXT("HintsSaved", "{Confirm}  Use        {Toggle}  Duplicate        {Second}  Rename        {Back}  Back");
	case ELoadoutV2Mode::WindowKind:
		return LOCTEXT("HintsKind", "{Confirm}  Pick        {Back}  Cancel");
	case ELoadoutV2Mode::WindowSlots:
		return LOCTEXT("HintsSlots", "{Confirm}  Open        {Second}  Confirm        {Back}  Cancel");
	case ELoadoutV2Mode::WindowItems:
		if (Items.IsValidIndex(Selected) && Items[Selected].bRestore)
		{
			return LOCTEXT("HintsItemsRestore", "{Confirm}  Restore        {Second}  Confirm        {Back}  Back");
		}
		if (IsGridOpen())
		{
			// D-248 step 1: the tab keys turn the grid's filter.
			return Items.IsValidIndex(Selected) && Items[Selected].bEquipped
					   ? LOCTEXT("HintsGridEquipped", "{Confirm}  Remove        {Toggle}  Compare        {Tabs}  Filter        {Second}  Confirm        {Back}  Back")
					   : LOCTEXT("HintsGrid", "{Confirm}  Pick        {Toggle}  Compare        {Tabs}  Filter        {Second}  Confirm        {Back}  Back");
		}
		return Items.IsValidIndex(Selected) && Items[Selected].bEquipped
				   ? LOCTEXT("HintsItemsEquipped", "{Confirm}  Remove        {Toggle}  Compare        {Second}  Confirm        {Back}  Back")
				   : LOCTEXT("HintsItems", "{Confirm}  Pick        {Toggle}  Compare        {Second}  Confirm        {Back}  Back");
	case ELoadoutV2Mode::WindowAsk:
		return LOCTEXT("HintsAsk", "{Confirm}  Attach        {Back}  Not now");
	case ELoadoutV2Mode::SkillEntries:
		return LOCTEXT("HintsSkillEntries", "{Confirm}  Slots        {Tabs}  Tab        {SubTabs}  Character        {Back}  Back");
	case ELoadoutV2Mode::SkillRows:
		return LOCTEXT("HintsSkillRows", "{Confirm}  Change        {Toggle}  {ViewName}        {Second}  Take out        {Back}  Back"); // X the view, as on Weapons; Y / R takes the lit skill out (B-488)
	case ELoadoutV2Mode::SkillPicker:
		return LOCTEXT("HintsSkillPicker", "{Confirm}  Put in slot        {Toggle}  {ViewName}        {Back}  Cancel");
	case ELoadoutV2Mode::ApparelItems:
		return Items.IsValidIndex(Selected) && Items[Selected].bEquipped ? LOCTEXT("HintsApparelEquipped", "{Confirm}  Remove        {Back}  Back")
																		 : LOCTEXT("HintsApparelItems", "{Confirm}  Pick        {Back}  Back");
	case ELoadoutV2Mode::List:
	default:
	{
		if (IsOnAddFrame() || (Tab == ELoadoutV2Tab::Apparel && Selected >= GetWornApparelCount()))
		{
			return LOCTEXT("HintsFrame", "{Confirm}  Equip        {Second}  Options        {SubTabs}  Character        {Back}  Back");
		}
		if (Tab == ELoadoutV2Tab::Inventory)
		{
			return LOCTEXT("HintsInventory", "{Confirm}  Wear        {Toggle}  {ViewName}        {Tabs}  Tab        {SubTabs}  Character        {Back}  Back");
		}
		const FLoadoutV2Entry *Entry = GetFocusedEntry();
		const bool bCanSetActive = Entry && Entry->Kind == ELoadoutV2EntryKind::Weapon && !Entry->bActive && Page.Entries.ContainsByPredicate(
																														[](const FLoadoutV2Entry &Each) { return Each.bActive; });
		if (Tab == ELoadoutV2Tab::Apparel)
		{
			return LOCTEXT("HintsApparelList", "{Confirm}  Change / Remove        {Toggle}  {ViewName}        {Second}  Options        {Back}  Back");
		}
		return bCanSetActive ? LOCTEXT("HintsListActive", "{Confirm}  Edit        {Toggle}  {ViewName}        {SetActive}  Set active        {Second}  Options        {Back}  Back")
							 : LOCTEXT("HintsList", "{Confirm}  Edit        {Toggle}  {ViewName}        {Second}  Options        {Back}  Back");
	}
	}
}

// ==================== VIEW ====================

void ULoadoutV2ScreenWidget::RefreshView()
{
	using namespace LoadoutV2ScreenStyle;
	++FullRefreshCount;
	if (!IsLoadoutAtTopLevel())
	{
		// D-249: the loadout is in the middle of something (Options, a window, a slot's list): it is in, and the keys are its.
		bLoadoutOpen = true;
		bOnLoadoutSide = true;
	}
	RefreshDetails();
	if (!ListBox)
	{
		return; // headless: no tree
	}
	// B-482: the character chips and the tab chips (the current one filled, the one under the mouse lit).
	TArray<FChipState> MemberStates;
	for (const FLoadoutScreenMember &Member : Members)
	{
		MemberStates.Add(FChipState{Member.Name});
	}
	SetChips(MemberChips, MemberStates, MemberIndex, HoveredMemberChip);
	SetChips(TabChips, BuildTabChips(),
			 Tab == ELoadoutV2Tab::Apparel ? LoadoutV2Chip_Apparel : Tab == ELoadoutV2Tab::Skills ? LoadoutV2Chip_Skills : Tab == ELoadoutV2Tab::Inventory ? LoadoutV2Chip_Inventory : LoadoutV2Chip_Weapons,
			 HoveredTabChip);
	// D-249: the loadout's own Inventory tab is gone (the inventory is the grid beside it); its chip is hidden.
	// OBSOLETE (D-249; delete after Crown's PIE pass): ELoadoutV2Tab::Inventory, RefreshInventory, WearFromInventory, IsOnInventoryList.
	SetShown(TabChips.Chips[LoadoutV2Chip_Inventory].Get(), false);
	SetShown(TabChips.Notes[LoadoutV2Chip_Inventory].Get(), false);
	// The loadout's tabs and its name show only while it is in; the inventory's tabs are always there.
	SetShown(LoadoutTabsPanel, bLoadoutOpen);
	SetShown(LoadoutNameText, bLoadoutOpen);
	TArray<FChipState> InvTabStates;
	for (const EInventoryScreenTab Each : InventoryTabOrder)
	{
		InvTabStates.Add(FChipState{WoRInventoryScreen::GetTabName(Each)});
	}
	SetChips(InvTabChips, InvTabStates, static_cast<int32>(Algo::IndexOf(InventoryTabOrder, InvTab)), HoveredInvTab);
	LoadoutButton->SetBrushColor(bLoadoutOpen ? FocusColor : PlateColor);
	// The key is named only where it does this: with the keys on the loadout's side Y is the loadout's own (Options,
	// Confirm), and the button is closed with a click.
	WoRKeyHints::SetHint(LoadoutButtonText, !bLoadoutOpen	   ? LOCTEXT("LoadoutButtonOpen", "{Second}  Loadout")
										   : bOnLoadoutSide ? LOCTEXT("LoadoutButtonClosePlain", "Close loadout")
															: LOCTEXT("LoadoutButtonClose", "{Second}  Close loadout"));
	LoadoutNameText->SetText(LoadoutName);
	CellsText->SetText(IsOnSkillsTab() ? LOCTEXT("SkillsByEntry", "Skills by entry") : Page.CellsLine); // mockup 6: cells are the Weapons tab's
	TitleText->SetText(GetTitle());
	SetShown(TitleText, Mode != ELoadoutV2Mode::List && Mode != ELoadoutV2Mode::SkillEntries);
	// D-248 step 1: while a weapon slot is being filled its choices are the grid's tiles, and the list keeps showing
	// the entry's slots with the one being filled lit. So the rows below are drawn as the slots list; the real mode,
	// selection and hover (a tile's) come back before the box is drawn.
	const bool bGrid = IsGridOpen();
	RefreshGrid(bGrid);
	TOptional<TGuardValue<ELoadoutV2Mode>> RowsMode;
	TOptional<TGuardValue<int32>> RowsSelected;
	TOptional<TGuardValue<int32>> RowsHovered;
	if (bGrid)
	{
		RowsMode.Emplace(Mode, ELoadoutV2Mode::WindowSlots);
		RowsSelected.Emplace(Selected, SlotIndex);
		RowsHovered.Emplace(HoveredLine, INDEX_NONE);
	}
	const int32 Count = GetLineCount();
	while (LineRows.Num() < Count)
	{
		// Rows are made the first time a list needs them and refilled after, never rebuilt.
		UBorder *Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Row->SetPadding(FMargin(RowPadding));
		UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->SetContent(Line);
		UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
		UTextBlock *Detail = MakeHintText(WidgetTree, LineFontSize, DimColor); // B-481: a line may name a key
		// B-484: the name, the detail and the tag each have their own space; text too long for its space ends in "...".
		UTextBlock *Tag = MakeText(WidgetTree, SmallFontSize, CyanColor, true);
		Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Detail->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		// B-485: the weapon-type icon, the name, then the two socket badges and a tier badge (each hidden until the
		// icon table has it); the name column cuts what doesn't fit.
		UImage *TypeIcon = MakeIcon(WidgetTree, RowIconSize);
		UImage *TypeCentre = nullptr;
		UOverlay *TypeStack = MakeIconStack(WidgetTree, TypeIcon, RowIconSize, TypeCentre); // B-530: an adapter's crystal
		UImage *Socket1Icon = MakeIcon(WidgetTree, BadgeSize);
		UImage *Socket2Icon = MakeIcon(WidgetTree, BadgeSize);
		UImage *TierIcon = MakeIcon(WidgetTree, BadgeSize);
		// D-248 (Crown): under a piece's name, its type with the tier in brackets ("Sword (F)"); hidden on other lines.
		UTextBlock *SubName = MakeText(WidgetTree, SmallFontSize, DimColor);
		SubName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		UVerticalBox *NameStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddToColumn(NameStack, Name);
		AddToColumn(NameStack, SubName);
		UHorizontalBox *NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddToRow(NameRow, TypeStack, FMargin(0.0f, 0.0f, WordGap, 0.0f), VAlign_Center);
		AddToRow(NameRow, NameStack, FMargin(0.0f), VAlign_Center);
		AddToRow(NameRow, TierIcon, FMargin(WordGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
		AddToRow(NameRow, Socket1Icon, FMargin(WordGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
		AddToRow(NameRow, Socket2Icon, FMargin(WordGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
		USizeBox *NameColumn = MakeSized(WidgetTree, NameColumnWidth, 0.0f, NameRow);
		NameColumn->SetClipping(EWidgetClipping::ClipToBounds);
		AddToRow(Line, NameColumn, FMargin(0.0f), VAlign_Center);
		AddToRow(Line, Detail, FMargin(SectionGap, 0.0f, 0.0f, 0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AddToRow(Line, Tag, FMargin(SectionGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
		AddToColumn(ListBox, Row, FMargin(0.0f, 0.0f, 0.0f, RowGap));
		LineRows.Add(Row);
		LineNames.Add(Name);
		LineSubNames.Add(SubName);
		LineNameColumns.Add(NameColumn);
		LineDetails.Add(Detail);
		LineTags.Add(Tag);
		LineTypeIcons.Add(TypeIcon);
		LineTypeCentres.Add(TypeCentre);
		LineSocket1Icons.Add(Socket1Icon);
		LineSocket2Icons.Add(Socket2Icon);
		LineTierIcons.Add(TierIcon);
	}
	for (int32 Index = 0; Index < LineRows.Num(); ++Index)
	{
		const bool bShown = Index < Count;
		SetShown(LineRows[Index], bShown);
		if (!bShown)
		{
			continue;
		}
		const bool bRefused = IsLineRefused(Index);
		LineRows[Index]->SetBrushColor(GetLineColor(Index, bGrid));
		// A weapon line in a slot's list has its type after its name ("Simple Sword · Sword"); the box keeps the bare name.
		const bool bWeaponLine = (Mode == ELoadoutV2Mode::WindowItems || Mode == ELoadoutV2Mode::WindowAsk) && Items.IsValidIndex(Index) && Items[Index].Weapon.IsValid();
		LineNames[Index]->SetText(bWeaponLine ? WoRLoadoutV2::GetWeaponRowName(GetLineName(Index), Items[Index].Weapon.Weapon) : GetLineName(Index));
		LineNames[Index]->SetColorAndOpacity(FSlateColor(bRefused ? OffColor : TextColor)); // refused: stays, greyed
		const FText SubName = GetLineSubName(Index);
		LineSubNames[Index]->SetText(SubName);
		SetShown(LineSubNames[Index], !SubName.IsEmpty());
		LineNameColumns[Index]->SetWidthOverride(GridNameColumnWidth); // D-249: the list always has the narrow column beside the grid
		WoRKeyHints::SetHint(LineDetails[Index], GetLineDetail(Index));
		// B-482b: on the entries list the detail is the row's right end ("ACTIVE", "1/5 left"), in the accent colour.
		const bool bListMode = Mode == ELoadoutV2Mode::List || Mode == ELoadoutV2Mode::SkillEntries; // B-476d: "4/9" as "1/5 left"
		LineDetails[Index]->SetJustification(bListMode ? ETextJustify::Right : ETextJustify::Left);
		LineDetails[Index]->SetColorAndOpacity(FSlateColor(bRefused && (IsAdding() || Mode == ELoadoutV2Mode::ApparelItems) ? WarnColor : bListMode ? CyanColor : DimColor));
		const FText Tag = GetLineTag(Index);
		LineTags[Index]->SetText(Tag);
		SetShown(LineTags[Index], !Tag.IsEmpty());
		// B-485: the row's icons; each is hidden when the icon table has none for it.
		const FLineIcons Icons = GetLineIcons(Index);
		const bool bTypeIcon = SetIcon(LineTypeIcons[Index], FindIcon(Icons.Type), Icons.Type.Name);
		SetIcon(LineTypeCentres[Index], bTypeIcon ? WoRIcons::FindCentre(Icons.Type) : nullptr, Icons.Type.Name);
		const bool bSocketBadge = SetIcon(LineSocket1Icons[Index], FindIcon(Icons.Socket1), Icons.Socket1.Name);
		SetIcon(LineSocket2Icons[Index], bSocketBadge ? FindIcon(Icons.Socket2) : nullptr, Icons.Socket2.Name); // the pair shows together
		const bool bTierBadge = SetIcon(LineTierIcons[Index], Icons.bHasTier ? WoRIcons::GetTierBadge(Icons.Tier) : nullptr,
										FText::Format(LOCTEXT("TierTooltip", "Tier {0}"), WoRGameNames::GetTierLetter(Icons.Tier)));
		// A name with a badge after it keeps its own width, so the badge sits right after it; with none it fills the
		// column and a long one ends in "...", as before the icons.
		// (The name sits in a stack with its quiet line: the stack is what the row sizes.)
		if (UHorizontalBoxSlot *NameSlot = Cast<UHorizontalBoxSlot>(LineNames[Index]->GetParent()->Slot))
		{
			NameSlot->SetSize(FSlateChildSize(bSocketBadge || bTierBadge ? ESlateSizeRule::Automatic : ESlateSizeRule::Fill));
		}
	}

	// The list's panel ends where its area does.
	// shortcut: the area's height is read on a refresh, so a window resized while a long list is open keeps the old
	// height until the next key or mouse move; follow the geometry itself if that shows.
	const float AreaHeight = ListArea->GetCachedGeometry().GetLocalSize().Y;
	ListSizer->SetMaxDesiredHeight(AreaHeight > 0.0f ? AreaHeight : ListMaxHeight);
	// The rows are drawn: back to the real mode, selection and hover (the grid's, when it shows).
	RowsHovered.Reset();
	RowsSelected.Reset();
	RowsMode.Reset();

	ScrollToSelection();
	RefreshBox();
}

void ULoadoutV2ScreenWidget::RefreshSelection()
{
	using namespace LoadoutV2ScreenStyle;
	// UI_Theme 5a: a move redraws only what changed. Only the selection or the hover moved, so the lines and tiles
	// keep their text and pictures: the old and the new one are recoloured, and the card is filled again.
	++LightRefreshCount;
	RefreshDetails();
	if (!ListBox)
	{
		return; // headless: no tree
	}
	const bool bSlotGrid = IsGridOpen();
	const int32 LineCount = FMath::Min(bSlotGrid ? Slots.Num() : GetLineCount(), LineRows.Num()); // beside a slot's grid the lines are the slots
	for (int32 Index = 0; Index < LineCount; ++Index)
	{
		SetItemColor(LineRows[Index], GetLineColor(Index, bSlotGrid));
	}
	const int32 TileCount = FMath::Min(bSlotGrid ? Items.Num() : InvPage.Tiles.Num(), GridTiles.Num());
	for (int32 Index = 0; Index < TileCount; ++Index)
	{
		SetItemColor(GridTiles[Index], GetTileColor(Index, bSlotGrid && !Items[Index].Refusal.IsEmpty(), bSlotGrid));
	}
	ScrollToSelection();
	RefreshBox();
}

void ULoadoutV2ScreenWidget::RefreshHover()
{
	// The chips and the filter tabs are coloured by the full view; a hovered line or tile only needs the light one.
	const uint32 ChipKey = HashCombine(HashCombine(GetTypeHash(HoveredMemberChip), GetTypeHash(HoveredTabChip)), HashCombine(GetTypeHash(HoveredGridTab), GetTypeHash(HoveredInvTab)));
	if (ChipKey != HoverChipKey)
	{
		HoverChipKey = ChipKey;
		RefreshView();
		return;
	}
	RefreshSelection();
}

FLinearColor ULoadoutV2ScreenWidget::GetLineColor(int32 Index, bool bSlotGrid) const
{
	using namespace LoadoutV2ScreenStyle;
	// Beside a slot's grid the lines are the entry's slots: the one being filled is lit, and the mouse lights tiles.
	const int32 Lit = bSlotGrid ? SlotIndex : Selected;
	const int32 Hovered = bSlotGrid ? static_cast<int32>(INDEX_NONE) : HoveredLine;
	// D-249: with the keys on the inventory's side the list's own line is only marked, not lit.
	const bool bKeysHere = bOnLoadoutSide || bSlotGrid;
	return Index == Lit ? (bKeysHere ? FocusColor : HoverColor) : Index == Hovered ? HoverColor : PlateColor;
}

FLinearColor ULoadoutV2ScreenWidget::GetTileColor(int32 Index, bool bLocked, bool bSlotGrid) const
{
	using namespace LoadoutV2ScreenStyle;
	const int32 Lit = bSlotGrid ? Selected : bOnLoadoutSide || bOnInvFilters ? static_cast<int32>(INDEX_NONE) : InvSelected;
	const int32 Hovered = bSlotGrid ? HoveredLine : HoveredInvTile; // in a slot's grid the hovered line is a tile
	return Index == Lit ? FocusColor : Index == Hovered ? HoverColor : bLocked ? LockedTileColor : PlateColor;
}

void ULoadoutV2ScreenWidget::SetItemColor(UBorder *Item, const FLinearColor &Color)
{
	if (Item && Item->GetBrushColor() != Color)
	{
		Item->SetBrushColor(Color);
	}
}

void ULoadoutV2ScreenWidget::ScrollToSelection()
{
	using namespace LoadoutV2ScreenStyle;
	const bool bSlotGrid = IsGridOpen();
	// The list's lit line is brought into view when the selection or the list changes, not on every refresh (a hover
	// refreshes too, and would pull a list dragged by its scroll bar back).
	const int32 Line = bSlotGrid ? SlotIndex : Selected;
	const int32 LineCount = bSlotGrid ? Slots.Num() : GetLineCount();
	const uint32 ListKey = HashCombine(HashCombine(GetTypeHash(Line), GetTypeHash(LineCount)), HashCombine(GetTypeHash(static_cast<uint8>(Mode)), GetTypeHash(static_cast<uint8>(Tab))));
	if (ListKey != ScrolledViewKey && bLoadoutOpen && LineRows.IsValidIndex(Line) && Line < LineCount)
	{
		ScrolledViewKey = ListKey;
		ListScroll->ScrollWidgetIntoView(LineRows[Line], /*AnimateScroll=*/false);
	}
	// The grid's lit tile, the same way. The offset is set at once (ScrollWidgetIntoView waits a frame), so the card is
	// placed beside the tile where it will be drawn (PlaceBox). Before the grid has been drawn its height isn't known:
	// it opens at the top anyway, and the scroll is tried again once it is drawn (SettleViewNextFrame).
	const int32 Tile = bSlotGrid ? Selected : bOnLoadoutSide || bOnInvFilters ? static_cast<int32>(INDEX_NONE) : InvSelected;
	const int32 TileCount = bSlotGrid ? Items.Num() : InvPage.Tiles.Num();
	const int32 Columns = GetGridColumns();
	const uint32 GridKey = HashCombine(HashCombine(HashCombine(GetTypeHash(Tile), GetTypeHash(TileCount)), HashCombine(GetTypeHash(SlotIndex), GetTypeHash(bSlotGrid))),
									   HashCombine(HashCombine(GetTypeHash(Columns), GetTypeHash(static_cast<uint8>(InvTab))),
												   HashCombine(GetTypeHash(static_cast<uint8>(InvFilter)), HashCombine(GetTypeHash(bGridFiltered), GetTypeHash(static_cast<uint8>(GridFilter))))));
	const float ViewHeight = GridScroll->GetCachedGeometry().GetLocalSize().Y;
	if (GridKey != GridScrolledKey && Tile >= 0 && Tile < TileCount && ViewHeight > 0.0f)
	{
		GridScrolledKey = GridKey;
		GridScroll->SetScrollOffset(WoRLoadoutV2Grid::ScrollToRow(GridScroll->GetScrollOffset(), Tile / Columns, TilePitch, ViewHeight));
	}
}

void ULoadoutV2ScreenWidget::RefreshBox()
{
	using namespace LoadoutV2ScreenStyle;
	// The box: the focused entry's details, or in the window the entry being built.
	// A box line: its text, whether it is lit, and (B-485) its icon and a tier badge.
	struct FBoxLine
	{
		FText Text;
		bool bLit = false;
		FLoadoutV2Icon Icon;
		bool bHasTier = false;
		EItemTier Tier = EItemTier::F_Tier;
		/** A quiet line: a piece's "Type (Tier)" under its name (D-248), a note. */
		bool bQuiet = false;
		/** D-248: why a tile's piece can't go in the slot, in the warning colour. */
		bool bWarn = false;
	};
	TArray<FBoxLine> BoxText;
	FBoxLine BoxType; // the header's quiet line: the piece's icon and type
	FBoxLine BoxHeader;
	FText &BoxTitle = BoxHeader.Text;
	// B-484: in a slot's list the box is the highlighted piece's own details (Details, from RefreshDetails), not the
	// entry's slots; a line with nothing to show (a note) falls back to the slots.
	const bool bPieceBox = (Mode == ELoadoutV2Mode::WindowItems || Mode == ELoadoutV2Mode::WindowAsk || Mode == ELoadoutV2Mode::WindowSlots || IsOnInventoryList() || IsInventoryBox()) &&
						   !Details.IsEmpty(); // B-503u: the slot rows too; B-531: the Inventory tab's pieces; D-249: an inventory tile
	if (IsInventoryBox() && Details.IsEmpty())
	{
		// D-249: the keys are on the inventory's side and no tile is lit (an empty tab, the filter row): no card.
	}
	else if (IsAdding() && !bPieceBox)
	{
		BoxTitle = bEditing ? EditEntry.Name : LOCTEXT("BoxNewEntry", "New entry");
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			const bool bLit = Mode != ELoadoutV2Mode::WindowSlots && Index == SlotIndex;
			BoxText.Add({FText::Format(LOCTEXT("BoxSlotLine", "{0}:  {1}"), GetSlotName(Slots[Index]), GetSlotValue(Slots[Index])), bLit});
		}
	}
	else if (Tab == ELoadoutV2Tab::Apparel && !bPieceBox)
	{
		// B-440f: the focused piece (in the list of pieces: the lit one), or the Add apparel frame.
		const FLoadoutScreenRow *Place = GetFocusedApparelRow();
		const FLoadoutScreenRow *Row = Mode == ELoadoutV2Mode::ApparelItems ? (Items.IsValidIndex(GetBoxLine()) ? &Items[GetBoxLine()].Socket.Row : nullptr) : Place;
		if (Row && Row->Kind == ELoadoutScreenRowKind::Apparel)
		{
			BoxTitle = Row->Name;
			if (!bSimpleView)
			{
				BoxText.Add({FText::Format(LOCTEXT("BoxApparelKind", "{0}  {1}"), Row->KindLine, Row->Tier), true});
				for (const FLoadoutScreenSocket &Socket : Row->Sockets)
				{
					BoxText.Add({FText::Format(LOCTEXT("BoxStat", "{0}   {1}"), Socket.Label, Socket.bEmpty ? LOCTEXT("SocketEmpty", "empty") : Socket.Name), false});
				}
			}
			// TODO: B-476d: the piece's HP / Defence / Amp lines (tier-scaled in ULoadoutComponent) and its effects' text.
			for (const FLoadoutScreenDetail &Detail : Row->Details)
			{
				BoxText.Add({Detail.Label.IsEmpty() ? Detail.Value : FText::Format(LOCTEXT("BoxStat", "{0}   {1}"), Detail.Label, Detail.Value), false});
			}
			if (!bSimpleView)
			{
				BoxText.Add({Row->CostLine, false});
			}
		}
		// B-482: on the Add apparel frame the box stays empty (and so hidden): the frame says what it does.
	}
	else
	{
		const FLoadoutV2Entry *Entry = GetFocusedEntry();
		// B-482: nothing on the Add weapon frame. On a slot row the title stays the slot's word (the row leads with the piece).
		const bool bSlotBox = bPieceBox && Mode == ELoadoutV2Mode::WindowSlots && Slots.IsValidIndex(GetBoxLine());
		BoxTitle = bSlotBox ? GetSlotName(Slots[GetBoxLine()]) : bPieceBox ? GetLineName(GetBoxLine()) : Entry ? Entry->Name : FText::GetEmpty();
		if (Mode == ELoadoutV2Mode::SkillEntries && SkillsPage.Entries.IsValidIndex(Selected))
		{
			BoxTitle = SkillsPage.Entries[Selected].Header; // "Ashen Blade + Cinder Edge · 4/9 skills"
		}
		else if ((Mode == ELoadoutV2Mode::SkillRows || Mode == ELoadoutV2Mode::SkillPicker) && Details.IsEmpty())
		{
			BoxTitle = LOCTEXT("BoxEmptySlot", "Empty slot: pick a skill for it");
		}
		for (const FLoadoutV2DetailLine &Line : Details)
		{
			FBoxLine BoxLine;
			BoxLine.Icon = Line.Icon;
			switch (Line.Kind)
			{
			case ELoadoutV2DetailKind::Piece:
			{
				// D-248 (Crown): a piece is two lines: its name with its icon; then under it, quiet, its type with the tier
				// in brackets ("Sword (F)"). No tier badge. (B-487b had the type above and the tier as a badge.)
				BoxLine.bLit = true;
				BoxLine.Text = Line.TypeName.IsEmpty() && Line.bHasTier ? FText::Format(LOCTEXT("BoxPiece", "{0}  {1}"), Line.Label, Line.Value) : Line.Label;
				BoxText.Add(BoxLine);
				if (!Line.TypeName.IsEmpty())
				{
					FBoxLine TypeLine{Line.bHasTier ? WoRLoadoutV2Grid::GetTypeTierLine(Line.TypeName, Line.Tier) : Line.TypeName};
					TypeLine.bQuiet = true;
					BoxText.Add(TypeLine);
				}
				continue;
			}
			case ELoadoutV2DetailKind::Effect:
				BoxLine.Text = FText::Format(LOCTEXT("BoxEffect", "{0}{1}"), FText::FromString(FString::ChrN(Line.Depth * IndentSpaces, TEXT(' '))), Line.Value);
				break;
			case ELoadoutV2DetailKind::Socket:
				// B-487b: with its badge drawn a socket needs no label: "Garnet (F)", "Empty". Without one the words stay.
				if (FindIcon(Line.Icon))
				{
					BoxLine.Text = Line.bHasTier ? FText::Format(LOCTEXT("BoxSocketFilled", "{0} ({1})"), Line.Icon.Name, WoRGameNames::GetTierLetter(Line.Tier)) : Line.Value;
					break;
				}
				[[fallthrough]];
			case ELoadoutV2DetailKind::Stat:
			case ELoadoutV2DetailKind::SkillSlots:
			default:
				BoxLine.Text = FText::Format(LOCTEXT("BoxStat", "{0}   {1}"), Line.Label, Line.Value);
				break;
			}
			BoxText.Add(BoxLine);
		}
		// B-487b: the first piece's own lines are the header, in place of a plain name above them.
		if (!Details.IsEmpty() && Details[0].Kind == ELoadoutV2DetailKind::Piece)
		{
			BoxHeader = BoxText[0];
			BoxText.RemoveAt(0);
			if (BoxText.Num() > 0 && BoxText[0].bQuiet)
			{
				BoxType = BoxText[0]; // its "Type (Tier)" line, under the name
				BoxText.RemoveAt(0);
			}
		}
		// D-248 step 1: a tile has no line of its own, so what the list's line said is the box's first line: why the
		// piece can't go in the slot (a locked tile), else its cells and whether the off hand comes off.
		if (IsGridOpen() && Items.IsValidIndex(GetBoxLine()))
		{
			const FLoadoutV2WindowItem &Item = Items[GetBoxLine()];
			FBoxLine Note{Item.Refusal.IsEmpty() ? Item.Line : Item.Refusal};
			Note.bWarn = !Item.Refusal.IsEmpty();
			Note.bQuiet = !Note.bWarn;
			if (!Note.Text.IsEmpty())
			{
				BoxText.Insert(Note, 0);
			}
		}
	}
	const auto GetTierTooltip = [](EItemTier Tier) { return FText::Format(LOCTEXT("TierTooltip", "Tier {0}"), WoRGameNames::GetTierLetter(Tier)); };
	BoxTitleText->SetText(BoxTitle);
	BoxTypeText->SetText(BoxType.Text);
	SetIcon(BoxTitleIcon, FindIcon(BoxHeader.Icon), BoxHeader.Icon.Name); // beside the name
	SetShown(BoxTypeRow, !BoxType.Text.IsEmpty());
	const bool bTitleTier = SetIcon(BoxTitleTier, BoxHeader.bHasTier ? WoRIcons::GetTierBadge(BoxHeader.Tier) : nullptr, GetTierTooltip(BoxHeader.Tier));
	if (UHorizontalBoxSlot *TitleSlot = Cast<UHorizontalBoxSlot>(BoxTitleText->Slot))
	{
		TitleSlot->SetSize(FSlateChildSize(bTitleTier ? ESlateSizeRule::Automatic : ESlateSizeRule::Fill)); // as the lines below
	}
	// B-482: a box with nothing to say is hidden (the Add frames); Hidden, not collapsed, so the list keeps its place.
	DetailsBox->SetVisibility(BoxTitle.IsEmpty() && BoxText.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	while (BoxTexts.Num() < BoxText.Num())
	{
		// B-485: a line is a row: its icon, its text, a tier badge.
		UHorizontalBox *BoxRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UImage *Icon = MakeIcon(WidgetTree, BoxIconSize);
		UTextBlock *Text = MakeText(WidgetTree, LineFontSize, TextColor); // wrapped at a set width in the fill below
		UImage *TierIcon = MakeIcon(WidgetTree, BadgeSize);
		AddToRow(BoxRow, Icon, FMargin(0.0f, 0.0f, WordGap, 0.0f), VAlign_Top);
		AddToRow(BoxRow, Text, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AddToRow(BoxRow, TierIcon, FMargin(WordGap, 0.0f, 0.0f, 0.0f), VAlign_Center);
		AddToColumn(BoxLines, BoxRow, FMargin(0.0f, 0.0f, 0.0f, BoxLineGap));
		BoxRows.Add(BoxRow);
		BoxIcons.Add(Icon);
		BoxTexts.Add(Text);
		BoxTierIcons.Add(TierIcon);
	}
	// With any icon in the box, a line without one keeps the icon's room, so the lines stay aligned.
	const bool bAnyBoxIcon = BoxText.ContainsByPredicate([](const FBoxLine &Line) { return FindIcon(Line.Icon) != nullptr; });
	for (int32 Index = 0; Index < BoxTexts.Num(); ++Index)
	{
		const bool bShown = BoxText.IsValidIndex(Index);
		SetShown(BoxRows[Index], bShown);
		if (bShown)
		{
			const FBoxLine &Line = BoxText[Index];
			BoxTexts[Index]->SetText(Line.Text);
			BoxTexts[Index]->SetWrapTextAt(CardTextWidth - (bAnyBoxIcon ? BoxIconSize + WordGap : 0.0f)); // UI_Theme 5a: measured this frame
			BoxTexts[Index]->SetColorAndOpacity(FSlateColor(Line.bWarn ? WarnColor : Line.bLit ? CyanColor : Line.bQuiet ? DimColor : TextColor));
			SetIcon(BoxIcons[Index], FindIcon(Line.Icon), Line.Icon.Name, /*bKeepSpace=*/bAnyBoxIcon);
			const bool bTier = SetIcon(BoxTierIcons[Index], Line.bHasTier ? WoRIcons::GetTierBadge(Line.Tier) : nullptr, GetTierTooltip(Line.Tier));
			// A name with its tier badge keeps its own width, so the badge sits right after it; other lines fill and wrap.
			if (UHorizontalBoxSlot *TextSlot = Cast<UHorizontalBoxSlot>(BoxTexts[Index]->Slot))
			{
				TextSlot->SetSize(FSlateChildSize(bTier ? ESlateSizeRule::Automatic : ESlateSizeRule::Fill));
			}
		}
	}
	PlaceBox(IsGridOpen()); // D-248 step 2, D-249: the card, beside the lit tile or level with the lit line

	FText Compare;
	if (Mode == ELoadoutV2Mode::WindowItems && bComparing && Items.IsValidIndex(Selected) && Slots.IsValidIndex(SlotIndex))
	{
		const ELoadoutV2Slot GearSlot = Slots[SlotIndex];
		const FWeaponInventoryEntry *Equipped = nullptr;
		ULoadoutComponent *Loadout = nullptr;
		UInventoryComponent *Inventory = nullptr;
		const FCombatLoadout *Active = nullptr;
		FLoadoutV2Draft Current;
		if (bEditing && FindActive(Loadout, Inventory, Active))
		{
			Current = WoRLoadoutV2::DraftFromEntry(*Active, EditEntry);
			Equipped = GearSlot == ELoadoutV2Slot::MainHand ? &Current.MainHand : GearSlot == ELoadoutV2Slot::OffHand ? &Current.OffHand : nullptr;
		}
		else if (!bEditing && GearSlot == ELoadoutV2Slot::MainHand && FindActive(Loadout, Inventory, Active))
		{
			// A new entry: against the weapon in hand (a Generic's second weapon when it is the ACTIVE entry).
			const bool bSecondInHand = Page.Entries.ContainsByPredicate([](const FLoadoutV2Entry &Entry)
																		{ return Entry.bActive && Entry.Place == ELoadoutV2Place::Secondary; });
			Equipped = bSecondInHand ? &Active->SecondaryWeapon.WeaponEntry : &Active->PrimaryWeapon.WeaponEntry;
		}
		if ((GearSlot == ELoadoutV2Slot::MainHand || GearSlot == ELoadoutV2Slot::OffHand) && Equipped && Equipped->IsValid() && Items[Selected].Weapon.IsValid())
		{
			const FLoadoutV2Compare Result = WoRLoadoutV2::BuildCompare(*Equipped, Items[Selected].Weapon);
			Compare = FText::Format(LOCTEXT("CompareLine", "Equipped {0} / Selected {1} · Power {2} / {3} · Cells change {4} · Built-in gained: {5} · lost: {6}"),
									WeaponName(*Equipped), Items[Selected].Name, FText::AsNumber(Result.EquippedPower), FText::AsNumber(Result.SelectedPower),
									FText::AsNumber(Result.CellsChange), JoinNames(Result.BuiltInsGained), JoinNames(Result.BuiltInsLost));
		}
		else
		{
			// TODO: B-476d: Compare for adapters, evolutions and socket pieces (UIScreens: power, element, cells, passive).
			Compare = LOCTEXT("CompareNothing", "Compare shows two weapons: nothing equipped here to set beside it.");
		}
	}
	CompareText->SetText(Compare);
	SetShown(CompareText, !Compare.IsEmpty());
	WoRKeyHints::SetHint(MessageText, Message);
	SetShown(MessageText, !Message.IsEmpty());
	// B-481: the footer is a pattern; {ViewName} is the view X switches to.
	FFormatNamedArguments HintArguments;
	HintArguments.Add(TEXT("ViewName"), bSimpleView ? LOCTEXT("ViewFull", "Full") : LOCTEXT("ViewSimple", "Simple"));
	WoRKeyHints::SetHint(HintsText, GetHints(), HintArguments);
}

// ==================== THE GRID (D-248 step 1) ====================

bool ULoadoutV2ScreenWidget::IsGridOpen() const
{
	return Mode == ELoadoutV2Mode::WindowItems && Slots.IsValidIndex(SlotIndex) &&
		   (Slots[SlotIndex] == ELoadoutV2Slot::MainHand || Slots[SlotIndex] == ELoadoutV2Slot::OffHand);
}

TArray<FLoadoutV2GridTile> ULoadoutV2ScreenWidget::GetGridTiles() const
{
	TArray<FLoadoutV2GridTile> Tiles;
	for (const FLoadoutV2WindowItem &Item : Items)
	{
		FLoadoutV2GridTile &Tile = Tiles.AddDefaulted_GetRef();
		if (Item.Weapon.Weapon)
		{
			Tile.Icon.Kind = ELoadoutV2IconKind::WeaponType;
			Tile.Icon.WeaponType = Item.Weapon.Weapon->WeaponType;
			Tile.Icon.Name = WoRGameNames::GetWeaponTypeName(Item.Weapon.Weapon->WeaponType);
		}
		Tile.Count = Item.Count;
		Tile.bWorn = Item.bWorn;
		Tile.bLocked = !Item.Refusal.IsEmpty();
	}
	return Tiles;
}

void ULoadoutV2ScreenWidget::ApplyGridFilter()
{
	TArray<EWeaponType> Types;
	GridPieceCount = 0;
	for (const FLoadoutV2WindowItem &Item : Items)
	{
		if (Item.Weapon.Weapon)
		{
			Types.Add(Item.Weapon.Weapon->WeaponType);
			GridPieceCount += Item.Count;
		}
	}
	GridTypes = WoRLoadoutV2Grid::GetTypesPresent(Types);
	bGridFiltered = bGridFiltered && GridTypes.Contains(GridFilter); // a type that left the list: back to All
	if (bGridFiltered)
	{
		Items.RemoveAll([this](const FLoadoutV2WindowItem &Item) { return !Item.Weapon.Weapon || Item.Weapon.Weapon->WeaponType != GridFilter; });
	}
}

void ULoadoutV2ScreenWidget::LogGrid() const
{
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] grid, %s, filter %s, %d piece(s):\n%s"), Slots.IsValidIndex(SlotIndex) ? *GetSlotName(Slots[SlotIndex]).ToString() : TEXT("?"),
		   bGridFiltered ? *WoRGameNames::GetWeaponTypeName(GridFilter).ToString() : TEXT("All"), GridPieceCount,
		   *WoRLoadoutV2Grid::GetGridString(GetGridTiles(), WoRLoadoutV2Grid::COLUMNS, Selected));
}

void ULoadoutV2ScreenWidget::MoveInGrid(int32 DeltaX, int32 DeltaY)
{
	if (!IsGridOpen())
	{
		return;
	}
	const int32 Target = WoRLoadoutV2Grid::Move(Selected, Items.Num(), WoRLoadoutV2Grid::COLUMNS, DeltaX, DeltaY);
	if (Target != INDEX_NONE && Target != Selected)
	{
		MoveSelection(Target - Selected);
	}
}

void ULoadoutV2ScreenWidget::ClickGridTab(int32 GridTab)
{
	if (!IsGridOpen() || GridTab < 0 || GridTab > GridTypes.Num())
	{
		return;
	}
	const bool bFiltered = GridTab > 0;
	const EWeaponType Type = bFiltered ? GridTypes[GridTab - 1] : GridFilter;
	if (bFiltered == bGridFiltered && (!bFiltered || Type == GridFilter))
	{
		return;
	}
	bGridFiltered = bFiltered;
	GridFilter = Type;
	Selected = 0;
	bComparing = false;
	bRemoveArmed = false;
	Message = FText::GetEmpty();
	RefreshData();
	LogGrid();
}

void ULoadoutV2ScreenWidget::CycleGridFilter(int32 Delta)
{
	const int32 TabCount = GridTypes.Num() + 1; // All, then each type
	const int32 Current = bGridFiltered ? GridTypes.IndexOfByKey(GridFilter) + 1 : 0;
	ClickGridTab(((Current + (Delta < 0 ? -1 : 1)) % TabCount + TabCount) % TabCount);
}

int32 ULoadoutV2ScreenWidget::FindTileUnder(const FVector2D &ScreenPosition) const
{
	// A tile scrolled out of the panel is not under the mouse.
	if (!GridScroll || !GridScroll->GetCachedGeometry().IsUnderLocation(ScreenPosition))
	{
		return INDEX_NONE;
	}
	const int32 Count = FMath::Min(IsGridOpen() ? Items.Num() : InvPage.Tiles.Num(), GridTiles.Num()); // a slot's choices, or the inventory's tab
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (GridTiles[Index] && GridTiles[Index]->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 ULoadoutV2ScreenWidget::FindGridTabUnder(const FVector2D &ScreenPosition) const
{
	const int32 Count = FMath::Min(IsGridOpen() ? GridTypes.Num() + 1 : InvFilters.Num(), GridTabs.Num()); // the weapon types, or the inventory's filters
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (GridTabs[Index] && GridTabs[Index]->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void ULoadoutV2ScreenWidget::RefreshGrid(bool bSlotGrid)
{
	using namespace LoadoutV2ScreenStyle;
	// D-249: the grid is always there. With the loadout in, the list has its column on the left and the grid the rest
	// of the row; with it out the grid has the whole row.
	SetShown(ListSizer, bLoadoutOpen);
	SetShown(GridSizer, true);
	ListSizer->SetWidthOverride(GridSlotsWidth);
	if (UHorizontalBoxSlot *ListSlot = Cast<UHorizontalBoxSlot>(ListSizer->Slot))
	{
		ListSlot->SetPadding(FMargin(0.0f, 0.0f, GridGap, 0.0f));
	}
	GridSizer->SetWidthOverride(bLoadoutOpen ? GridWidth : ListWidth + ColumnGap + BoxWidth);
	const float AreaHeight = ListArea->GetCachedGeometry().GetLocalSize().Y;
	GridSizer->SetMaxDesiredHeight(AreaHeight > 0.0f ? AreaHeight : ListMaxHeight); // as the list's panel

	// What the grid shows: a weapon slot's choices while one is being filled (D-248), else the inventory's tab (D-249).
	const TArray<FLoadoutV2GridTile> Tiles = bSlotGrid ? GetGridTiles() : GetInventoryTiles();
	const int32 Columns = GetGridColumns();
	if (bSlotGrid)
	{
		GridTitleText->SetText(GetSlotName(Slots[SlotIndex]));
		GridCountText->SetText(FText::Format(LOCTEXT("GridCount", "{0} {0}|plural(one=weapon,other=weapons)"), GridPieceCount));
	}
	else
	{
		GridTitleText->SetText(InvPage.Heading.IsEmpty() ? WoRInventoryScreen::GetTabName(InvTab) : InvPage.Heading);
		GridCountText->SetText(Tiles.IsEmpty() ? InvPage.EmptyNote
											   : FText::Format(LOCTEXT("InvCount", "{0} {0}|plural(one=tile,other=tiles) · Sort: {1}"), Tiles.Num(), WoRInventoryScreen::GetSortName(InvSort)));
	}

	// The filter tabs. A slot's grid: All, then a picture for each weapon type present (its word where the icon table
	// has no picture). The inventory: the tab's own filter row, in words (none on the Pouch).
	const int32 TabCount = bSlotGrid ? GridTypes.Num() + 1 : InvFilters.Num();
	while (GridTabs.Num() < TabCount)
	{
		UBorder *GridTab = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		GridTab->SetPadding(FMargin(GridTabPadding));
		GridTab->SetVerticalAlignment(VAlign_Center);
		UHorizontalBox *Content = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		GridTab->SetContent(Content);
		UImage *Icon = MakeIcon(WidgetTree, GridTabIconSize);
		UTextBlock *Label = MakeText(WidgetTree, SmallFontSize, TextColor, true);
		AddToRow(Content, Icon);
		AddToRow(Content, Label);
		AddToRow(GridTabsRow, GridTab, FMargin(0.0f, 0.0f, TabGap, 0.0f));
		GridTabs.Add(GridTab);
		GridTabIcons.Add(Icon);
		GridTabLabels.Add(Label);
	}
	const int32 CurrentTab = bSlotGrid ? (bGridFiltered ? GridTypes.IndexOfByKey(GridFilter) + 1 : 0) : InvFilters.IndexOfByKey(InvFilter);
	for (int32 Index = 0; Index < GridTabs.Num(); ++Index)
	{
		const bool bTabShown = Index < TabCount;
		SetShown(GridTabs[Index], bTabShown);
		if (!bTabShown)
		{
			continue;
		}
		GridTabs[Index]->SetBrushColor(Index == CurrentTab ? FocusColor : Index == HoveredGridTab ? HoverColor : PlateColor);
		FLoadoutV2Icon Icon;
		FText Word = LOCTEXT("GridTabAll", "All");
		if (!bSlotGrid)
		{
			Word = WoRInventoryScreen::GetFilterName(InvFilters[Index]);
		}
		else if (Index > 0)
		{
			Icon.Kind = ELoadoutV2IconKind::WeaponType;
			Icon.WeaponType = GridTypes[Index - 1];
			Icon.Name = WoRGameNames::GetWeaponTypeName(Icon.WeaponType);
			Word = Icon.Name;
		}
		const bool bPicture = SetIcon(GridTabIcons[Index], FindIcon(Icon), Icon.Name);
		GridTabLabels[Index]->SetText(Word);
		SetShown(GridTabLabels[Index], !bPicture);
	}

	// The tiles, made the first time they are needed and refilled after. Their rows and columns are set again when the
	// grid's width changes (the loadout in or out).
	if (GridColumnsLaidOut != Columns)
	{
		for (int32 Index = 0; Index < GridTiles.Num(); ++Index)
		{
			if (UUniformGridSlot *TileSlot = Cast<UUniformGridSlot>(GridTiles[Index]->Slot))
			{
				TileSlot->SetRow(Index / Columns);
				TileSlot->SetColumn(Index % Columns);
			}
		}
		GridColumnsLaidOut = Columns;
	}
	while (GridTiles.Num() < Tiles.Num())
	{
		const int32 Index = GridTiles.Num();
		UBorder *Tile = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		UOverlay *Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Tile->SetContent(MakeSized(WidgetTree, TileSize, TileSize, Layers));
		const auto AddLayer = [Layers](UWidget *Layer, EHorizontalAlignment HAlign, EVerticalAlignment VAlign, float Inset)
		{
			UOverlaySlot *LayerSlot = Layers->AddChildToOverlay(Layer);
			LayerSlot->SetHorizontalAlignment(HAlign);
			LayerSlot->SetVerticalAlignment(VAlign);
			LayerSlot->SetPadding(FMargin(Inset));
		};
		UImage *Icon = MakeIcon(WidgetTree, TileIconSize);
		UTextBlock *Word = MakeText(WidgetTree, SmallFontSize, DimColor);
		UWidget *Tick = MakeTick(WidgetTree);
		UWidget *Lock = MakeLock(WidgetTree);
		UTextBlock *CountText = MakeText(WidgetTree, SmallFontSize, TextColor, true);
		AddLayer(Icon, HAlign_Center, VAlign_Center, 0.0f);
		AddLayer(Word, HAlign_Center, VAlign_Center, 0.0f);
		AddLayer(Tick, HAlign_Left, VAlign_Top, 0.0f);
		AddLayer(Lock, HAlign_Right, VAlign_Top, TileMarkInset);
		AddLayer(CountText, HAlign_Right, VAlign_Bottom, TileMarkInset);
		GridTilePanel->AddChildToUniformGrid(Tile, Index / Columns, Index % Columns);
		GridTiles.Add(Tile);
		GridTileIcons.Add(Icon);
		GridTileWords.Add(Word);
		GridTileTicks.Add(Tick);
		GridTileLocks.Add(Lock);
		GridTileCounts.Add(CountText);
	}
	for (int32 Index = 0; Index < GridTiles.Num(); ++Index)
	{
		const bool bTileShown = Tiles.IsValidIndex(Index);
		SetShown(GridTiles[Index], bTileShown);
		if (!bTileShown)
		{
			continue;
		}
		const FLoadoutV2GridTile &Tile = Tiles[Index];
		GridTiles[Index]->SetBrushColor(GetTileColor(Index, Tile.bLocked, bSlotGrid));
		const bool bPicture = SetIcon(GridTileIcons[Index], FindIcon(Tile.Icon), Tile.Icon.Name);
		GridTileIcons[Index]->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, Tile.bLocked ? LockedTileOpacity : 1.0f));
		GridTileWords[Index]->SetText(Tile.Icon.Name);
		GridTileWords[Index]->SetColorAndOpacity(FSlateColor(Tile.bLocked ? OffColor : DimColor));
		SetShown(GridTileWords[Index], !bPicture);
		SetShown(GridTileTicks[Index], Tile.bWorn);
		SetShown(GridTileLocks[Index], Tile.bLocked);
		GridTileCounts[Index]->SetText(FText::Format(LOCTEXT("GridTileCount", "x{0}"), FText::AsNumber(Tile.Count)));
		SetShown(GridTileCounts[Index], Tile.Count > 1);
	}
}

void ULoadoutV2ScreenWidget::SettleViewNextFrame()
{
	if (bSettlingView || bSettlePending)
	{
		return; // this refresh is the settling one, or one is already asked for
	}
	bSettlePending = true;
	// The core ticker, not a world timer: it runs while the game is paused too. One shot (it returns false).
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		bSettlePending = false;
		if (ListBox)
		{
			TGuardValue<bool> Settling(bSettlingView, true);
			RefreshView();
		}
		return false;
	}));
}

void ULoadoutV2ScreenWidget::HandleGridScrolled(float /*CurrentOffset*/)
{
	// The wheel or the scroll bar moved the tiles: the card is placed again beside its tile. (The tile under a resting
	// mouse is found again on the mouse's next move.)
	RefreshSelection();
}

void ULoadoutV2ScreenWidget::PlaceBox(bool bSlotGrid)
{
	using namespace LoadoutV2ScreenStyle;
	UCanvasPanelSlot *BoxSlot = Cast<UCanvasPanelSlot>(BoxSizer->Slot);
	if (!BoxSlot)
	{
		return;
	}
	// D-249: the details box is always a card now (the grid has the room it stood in): beside the lit or hovered tile
	// when it shows a tile's piece, else at the grid's left edge, level with the lit line of the loadout's list. It
	// lies over tiles, so the mouse and the wheel go through it to them.
	BoxSizer->SetWidthOverride(CardWidth);
	BoxSizer->SetVisibility(ESlateVisibility::HitTestInvisible);
	BoxFrame->SetBrushColor(CyanColor);
	BoxFrame->SetPadding(FMargin(CardFrameWidth));
	const FGeometry &Area = ListArea->GetCachedGeometry();
	const bool bDrawn = Area.GetLocalSize().Y > 0.0f;
	const float AreaHeight = bDrawn ? Area.GetLocalSize().Y : ListMaxHeight;
	const float GridLeft = bLoadoutOpen ? GridSlotsWidth + GridGap : 0.0f;
	BoxSizer->ForceLayoutPrepass(); // the card's size with the lines just written
	const FVector2D CardSize = BoxSizer->GetDesiredSize();

	const bool bInventoryTile = !bSlotGrid && IsInventoryBox() && !bOnInvFilters;
	const int32 Tile = bSlotGrid ? GetBoxLine() : bInventoryTile ? GetInventoryBoxTile() : static_cast<int32>(INDEX_NONE);
	const int32 TileCount = bSlotGrid ? Items.Num() : InvPage.Tiles.Num();
	if (Tile >= 0 && Tile < TileCount)
	{
		// The tiles' view inside the columns' area: inside the grid panel's padding, under its header and its filter
		// tabs. UI_Theme 5a: measured here from what those two ask for (not read from the last drawn frame), so the card
		// is in its place the same frame, the first one too.
		const int32 Columns = GetGridColumns();
		GridSizer->ForceLayoutPrepass();
		const FVector2D ViewOrigin(GridLeft + BoxPadding, BoxPadding + GridHeaderRow->GetDesiredSize().Y + LineGap + GridTabsRow->GetDesiredSize().Y + SectionGap);
		// The card keeps to the tiles' width (not over the scroll bar) and may use the room down to the area's bottom
		// (past the tiles' own view, over the legend).
		const FVector2D ViewSize(TilePitch * Columns, FMath::Max(AreaHeight - ViewOrigin.Y, 0.0));
		const FVector2D TilePosition = WoRLoadoutV2Grid::GetTilePosition(Tile, Columns, TilePitch, TilePadding, GridScroll->GetScrollOffset());
		const WoRLoadoutV2Grid::FCardPlace Place = WoRLoadoutV2Grid::PlaceCard(TilePosition, TileSize, CardSize, ViewSize, CardGap);
		BoxSlot->SetPosition(ViewOrigin + Place.Position);
		UE_LOG(LogWoRUI, Verbose, TEXT("[LoadoutV2] card for tile %d at %s (%s, %s)"), Tile, *Place.Position.ToString(), Place.bLeft ? TEXT("left") : TEXT("right"),
			   Place.bUp ? TEXT("bottoms level") : TEXT("tops level"));
	}
	else
	{
		// A line of the loadout's list: level with that line, kept inside the area.
		// shortcut: the line's place is the one it was last drawn at, so on the frame the list itself scrolls the card
		// is level with where the line was; measure the lines (their heights differ) if a long list shows it.
		const int32 Line = GetBoxLine();
		const bool bLineDrawn = bDrawn && LineRows.IsValidIndex(Line) && LineRows[Line]->GetCachedGeometry().GetLocalSize().Y > 0.0f;
		const double LineTop = bLineDrawn ? Area.AbsoluteToLocal(LineRows[Line]->GetCachedGeometry().GetAbsolutePosition()).Y : 0.0;
		BoxSlot->SetPosition(FVector2D(GridLeft + CardGap, FMath::Clamp(LineTop, 0.0, FMath::Max(AreaHeight - CardSize.Y, 0.0))));
	}
	// UI_Theme 5a: the card is measured and placed this frame. Only the screen's own height can't be known before its
	// first draw (how far down the card may go, how far the grid must scroll): that one time, one more refresh follows.
	if (!bDrawn)
	{
		SettleViewNextFrame();
	}
}

// ==================== THE INVENTORY'S SIDE (D-249 piece 1) ====================

void ULoadoutV2ScreenWidget::ToggleLoadout()
{
	if (!IsLoadoutAtTopLevel())
	{
		Message = LOCTEXT("LoadoutBusy", "Finish or leave what the loadout is doing first.");
		RefreshView();
		return;
	}
	bLoadoutOpen = !bLoadoutOpen;
	if (!bLoadoutOpen)
	{
		bOnLoadoutSide = false; // the keys have nowhere else to be
	}
	Message = FText::GetEmpty();
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] loadout %s; keys on the %s side; %d tiles to a row"), bLoadoutOpen ? TEXT("in") : TEXT("out"),
		   bOnLoadoutSide ? TEXT("loadout's") : TEXT("inventory's"), GetGridColumns());
	RefreshData();
}

bool ULoadoutV2ScreenWidget::FocusLoadoutSide()
{
	if (!bLoadoutOpen || bOnLoadoutSide)
	{
		return false;
	}
	bOnLoadoutSide = true;
	bOnInvFilters = false;
	HoveredInvTile = INDEX_NONE;
	Message = FText::GetEmpty();
	RefreshView();
	return true;
}

bool ULoadoutV2ScreenWidget::FocusInventorySide()
{
	if (!bOnLoadoutSide || !IsLoadoutAtTopLevel())
	{
		return false;
	}
	bOnLoadoutSide = false;
	SetHovered(INDEX_NONE, INDEX_NONE, INDEX_NONE);
	Message = FText::GetEmpty();
	RefreshView();
	return true;
}

void ULoadoutV2ScreenWidget::MoveInventoryTile(int32 DeltaX, int32 DeltaY)
{
	if (bOnLoadoutSide || !IsInventoryGrid())
	{
		return;
	}
	const int32 Columns = GetGridColumns();
	Message = FText::GetEmpty();
	if (bOnInvFilters)
	{
		// On the filter row: Left / Right pick the filter, Down goes back to the tiles.
		if (DeltaY > 0)
		{
			bOnInvFilters = false;
			RefreshView();
		}
		else if (DeltaX != 0)
		{
			SelectInventoryFilter(InvFilters.IndexOfByKey(InvFilter) + (DeltaX > 0 ? 1 : -1));
		}
		return;
	}
	if (DeltaX < 0 && InvSelected % Columns == 0 && FocusLoadoutSide())
	{
		return; // off the grid's left edge: the loadout's side, when it is in (else Left runs on into the row before)
	}
	if (DeltaY < 0 && InvSelected < Columns)
	{
		if (InvFilters.Num() > 0)
		{
			bOnInvFilters = true; // off the top row: the filter row
			RefreshView();
		}
		return;
	}
	const int32 Target = WoRLoadoutV2Grid::Move(InvSelected, InvPage.Tiles.Num(), Columns, DeltaX, DeltaY);
	if (Target != INDEX_NONE && Target != InvSelected)
	{
		InvSelected = Target;
		RefreshSelection(); // only the lit tile moved
	}
}

void ULoadoutV2ScreenWidget::SelectInventoryTab(EInventoryScreenTab NewTab)
{
	if (NewTab == InvTab || !IsInventoryGrid())
	{
		return;
	}
	InvTab = NewTab;
	InvFilter = EInventoryScreenFilter::All;
	InvSelected = 0;
	bOnInvFilters = false;
	Message = FText::GetEmpty();
	RefreshData();
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] inventory tab %s:\n%s"), *WoRInventoryScreen::GetTabName(InvTab).ToString(),
		   *WoRLoadoutV2Grid::GetGridString(GetInventoryTiles(), GetGridColumns(), InvSelected));
}

void ULoadoutV2ScreenWidget::MoveInventoryTab(int32 Delta)
{
	using namespace LoadoutV2ScreenStyle;
	const int32 Current = FMath::Max(0, static_cast<int32>(Algo::IndexOf(InventoryTabOrder, InvTab)));
	SelectInventoryTab(InventoryTabOrder[((Current + (Delta < 0 ? -1 : 1)) % INVENTORY_TAB_COUNT + INVENTORY_TAB_COUNT) % INVENTORY_TAB_COUNT]);
}

void ULoadoutV2ScreenWidget::SelectInventoryFilter(int32 Index)
{
	if (!InvFilters.IsValidIndex(Index) || InvFilters[Index] == InvFilter || !IsInventoryGrid())
	{
		return;
	}
	InvFilter = InvFilters[Index];
	InvSelected = 0;
	RefreshData();
}

void ULoadoutV2ScreenWidget::CycleInventorySort()
{
	if (!IsInventoryGrid())
	{
		return;
	}
	InvSort = WoRInventoryScreen::GetNextSort(InvSort);
	InvSelected = 0;
	RefreshData();
}

// ==================== KEYS ====================

void ULoadoutV2ScreenWidget::MoveSelection(int32 Delta)
{
	const int32 Count = GetLineCount();
	if (Count > 0 && Mode != ELoadoutV2Mode::WindowAsk)
	{
		Selected = ((Selected + Delta) % Count + Count) % Count;
		if (const FLoadoutV2InventoryRow *Row = GetInventoryRow(Selected); Row && !Row->IsPiece() && Count > 1)
		{
			Selected = ((Selected + (Delta < 0 ? -1 : 1)) % Count + Count) % Count; // B-531: past a group's header
		}
		Message = FText::GetEmpty();
		const bool bWasArmed = bRemoveArmed;
		bRemoveArmed = false;
		bRemoveAllArmed = false;
		if (bWasArmed && Mode == ELoadoutV2Mode::WindowItems)
		{
			RefreshData(); // the EQUIPPED line drops its "A again" warning
			return;
		}
		RefreshSelection(); // only the lit line moved
	}
}

void ULoadoutV2ScreenWidget::MoveMember(int32 Delta)
{
	if ((Mode != ELoadoutV2Mode::List && Mode != ELoadoutV2Mode::SkillEntries) || Members.Num() < 2)
	{
		return;
	}
	MemberIndex = ((MemberIndex + Delta) % Members.Num() + Members.Num()) % Members.Num();
	Selected = 0;
	Message = FText::GetEmpty();
	RefreshData();
}

// ==================== THE MOUSE (B-482) ====================

void ULoadoutV2ScreenWidget::ClickTabChip(int32 Chip)
{
	using namespace LoadoutV2ScreenStyle;
	if (Mode != ELoadoutV2Mode::List && Mode != ELoadoutV2Mode::SkillEntries)
	{
		return;
	}
	if (Chip == LoadoutV2Chip_Weapons || Chip == LoadoutV2Chip_Skills || Chip == LoadoutV2Chip_Apparel)
	{
		// B-476d: the chips are the tabs in order. D-249: the Inventory chip is hidden and answers nothing (the
		// inventory is the grid beside the loadout).
		const ELoadoutV2Tab Target = Chip == LoadoutV2Chip_Skills	 ? ELoadoutV2Tab::Skills
								   : Chip == LoadoutV2Chip_Apparel ? ELoadoutV2Tab::Apparel
																   : ELoadoutV2Tab::Weapons;
		if (Target != Tab)
		{
			ShowTab(Target);
		}
	}
	else if (Chip == LoadoutV2Chip_Cosmetics)
	{
		Message = LOCTEXT("ChipNotYet", "That tab comes later.");
		RefreshView();
	}
}

void ULoadoutV2ScreenWidget::ClickMemberChip(int32 Chip)
{
	if ((Mode == ELoadoutV2Mode::List || Mode == ELoadoutV2Mode::SkillEntries) && Members.IsValidIndex(Chip) && Chip != MemberIndex)
	{
		MoveMember(Chip - MemberIndex);
	}
}

void ULoadoutV2ScreenWidget::ClickLine(int32 Line)
{
	if (Line < 0 || Line >= GetLineCount() || Mode == ELoadoutV2Mode::WindowAsk)
	{
		return;
	}
	if (Line == Selected)
	{
		// A second click on the focused line does A. But not straight after the click that armed an ask (Remove all
		// gear, a socket's Remove): a double click must not confirm what it hasn't shown yet. B-476d: nor go on through the
		// Skills tab's slots and picker that click just opened (it would put the first skill in the slot).
		const double Now = FPlatformTime::Seconds();
		const bool bJustOpened = Mode == ELoadoutV2Mode::SkillRows || Mode == ELoadoutV2Mode::SkillPicker;
		if ((bRemoveAllArmed || bRemoveArmed || bJustOpened) && Now - LastClickActionSeconds < LoadoutV2ScreenStyle::ARMED_CLICK_DELAY_SECONDS)
		{
			return;
		}
		LastClickActionSeconds = Now;
		PressA();
	}
	else
	{
		MoveSelection(Line - Selected);
	}
}

int32 ULoadoutV2ScreenWidget::FindLineUnder(const FVector2D &ScreenPosition) const
{
	// A line scrolled out of the panel is not under the mouse, wherever its own place would be; nor is any line while
	// the loadout is out (D-249: its list is not drawn, its last place is stale).
	if (!bLoadoutOpen || (ListScroll && !ListScroll->GetCachedGeometry().IsUnderLocation(ScreenPosition)))
	{
		return INDEX_NONE;
	}
	const int32 Count = FMath::Min(IsGridOpen() ? Slots.Num() : GetLineCount(), LineRows.Num()); // beside the grid the rows are the slots
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (LineRows[Index] && LineRows[Index]->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool ULoadoutV2ScreenWidget::SetHovered(int32 MemberChip, int32 TabChip, int32 Line)
{
	if (MemberChip == HoveredMemberChip && TabChip == HoveredTabChip && Line == HoveredLine)
	{
		return false;
	}
	HoveredMemberChip = MemberChip;
	HoveredTabChip = TabChip;
	HoveredLine = Line;
	return true;
}

FReply ULoadoutV2ScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse lights things, so a resting cursor doesn't fight the keys. Chips answer on the list only.
	if (!bRenaming && !InMouseEvent.GetCursorDelta().IsZero())
	{
		const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
		const bool bList = Mode == ELoadoutV2Mode::List || Mode == ELoadoutV2Mode::SkillEntries; // a tab's top level (B-476d)
		// D-248: while the grid shows, the hovered line is a tile (the box follows it), and a filter tab lights too.
		const bool bGrid = IsGridOpen();
		// D-249: the grid's filter tabs light in both its uses; an inventory tile and an inventory tab light too (the
		// card follows the tile while the keys are on the inventory's side).
		const int32 GridTab = FindGridTabUnder(Position);
		const int32 InvTile = bGrid ? static_cast<int32>(INDEX_NONE) : FindTileUnder(Position);
		const int32 InvTabChip = bGrid ? static_cast<int32>(INDEX_NONE) : WoRMenuStyle::FindChipUnder(InvTabChips, Position);
		const bool bOtherChanged = GridTab != HoveredGridTab || InvTile != HoveredInvTile || InvTabChip != HoveredInvTab;
		HoveredGridTab = GridTab;
		HoveredInvTile = InvTile;
		HoveredInvTab = InvTabChip;
		if (SetHovered(bList ? WoRMenuStyle::FindChipUnder(MemberChips, Position) : INDEX_NONE,
					   bList && bLoadoutOpen ? WoRMenuStyle::FindChipUnder(TabChips, Position) : INDEX_NONE, bGrid ? FindTileUnder(Position) : FindLineUnder(Position)) ||
			bOtherChanged)
		{
			RefreshHover(); // a chip or a filter tab lights through the full view; a line or a tile through the light one
		}
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void ULoadoutV2ScreenWidget::NativeOnMouseLeave(const FPointerEvent &InMouseEvent)
{
	const bool bTabWasLit = HoveredGridTab != INDEX_NONE || HoveredInvTile != INDEX_NONE || HoveredInvTab != INDEX_NONE;
	HoveredGridTab = INDEX_NONE;
	HoveredInvTile = INDEX_NONE;
	HoveredInvTab = INDEX_NONE;
	if (SetHovered(INDEX_NONE, INDEX_NONE, INDEX_NONE) || bTabWasLit)
	{
		RefreshView();
	}
	HoverChipKey = 0;
	Super::NativeOnMouseLeave(InMouseEvent);
}

FReply ULoadoutV2ScreenWidget::NativeOnMouseWheel(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (bRenaming || InMouseEvent.GetWheelDelta() == 0.0f)
	{
		return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	}
	const int32 Step = InMouseEvent.GetWheelDelta() > 0.0f ? -1 : 1; // wheel up: the line (or tile) before, as Up
	if (!bOnLoadoutSide && IsInventoryGrid())
	{
		// D-249: on the inventory's side (the grid had nothing to scroll, or it would have taken the wheel): the lit tile.
		const int32 Target = InvPage.Tiles.Num() > 0 ? FMath::Clamp(InvSelected + Step, 0, InvPage.Tiles.Num() - 1) : 0;
		if (Target != InvSelected && !bOnInvFilters)
		{
			InvSelected = Target;
			RefreshSelection();
		}
		return FReply::Handled();
	}
	MoveSelection(Step);
	return FReply::Handled();
}

FReply ULoadoutV2ScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (bRenaming || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent); // typing a name: the box keeps the mouse
	}
	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	// D-249: the Loadout button, top right: the loadout in or out, from either side.
	if (LoadoutButton && LoadoutButton->GetCachedGeometry().IsUnderLocation(Position))
	{
		ToggleLoadout();
		return FReply::Handled();
	}
	if (Mode == ELoadoutV2Mode::List || Mode == ELoadoutV2Mode::SkillEntries)
	{
		const int32 MemberChip = WoRMenuStyle::FindChipUnder(MemberChips, Position);
		if (MemberChip != INDEX_NONE)
		{
			ClickMemberChip(MemberChip);
			return FReply::Handled();
		}
		const int32 TabChip = bLoadoutOpen ? WoRMenuStyle::FindChipUnder(TabChips, Position) : static_cast<int32>(INDEX_NONE); // not drawn while the loadout is out
		if (TabChip != INDEX_NONE)
		{
			FocusLoadoutSide(); // D-249: a click on the loadout's side brings the keys there
			ClickTabChip(TabChip);
			return FReply::Handled();
		}
	}
	if (IsInventoryGrid() && (!bOnLoadoutSide || IsLoadoutAtTopLevel()))
	{
		// D-249: the inventory's side: one of its tabs, a filter, a tile. The click brings the keys there.
		const int32 InvTabChip = WoRMenuStyle::FindChipUnder(InvTabChips, Position);
		const int32 FilterTab = FindGridTabUnder(Position);
		const int32 Tile = FindTileUnder(Position);
		if (InvTabChip != INDEX_NONE || FilterTab != INDEX_NONE || Tile != INDEX_NONE)
		{
			FocusInventorySide();
			if (InvTabChip != INDEX_NONE)
			{
				SelectInventoryTab(LoadoutV2ScreenStyle::InventoryTabOrder[InvTabChip]);
			}
			else if (FilterTab != INDEX_NONE)
			{
				SelectInventoryFilter(FilterTab);
			}
			else
			{
				InvSelected = Tile;
				bOnInvFilters = false;
				Message = FText::GetEmpty();
				RefreshView();
			}
			return FReply::Handled();
		}
	}
	if (IsGridOpen())
	{
		// D-248: a filter tab, a tile (as a line: focus, then A), or a slot row beside the grid (that slot opens).
		const int32 GridTab = FindGridTabUnder(Position);
		const int32 Tile = FindTileUnder(Position);
		const int32 SlotRow = FindLineUnder(Position);
		if (GridTab != INDEX_NONE)
		{
			ClickGridTab(GridTab);
		}
		else if (Tile != INDEX_NONE)
		{
			ClickLine(Tile);
		}
		else if (SlotRow != INDEX_NONE && SlotRow != SlotIndex)
		{
			HandleBackAction(); // back to the slots
			Selected = SlotRow;
			PressA(); // and into the clicked one
		}
		else
		{
			return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
		}
		return FReply::Handled();
	}
	const int32 Line = FindLineUnder(Position);
	if (Line != INDEX_NONE)
	{
		FocusLoadoutSide(); // D-249: a click on the loadout's list brings the keys there
		ClickLine(Line);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void ULoadoutV2ScreenWidget::SwitchTab(int32 Delta)
{
	if (Mode != ELoadoutV2Mode::List && Mode != ELoadoutV2Mode::SkillEntries)
	{
		return;
	}
	// B-476d: Weapons | Skills | Apparel, in a ring (Cosmetics later). D-249: the loadout's own Inventory tab is out
	// of the ring (it is gone; the inventory is the grid beside the loadout).
	const int32 First = static_cast<int32>(ELoadoutV2Tab::Weapons);
	const int32 TabCount = static_cast<int32>(ELoadoutV2Tab::Last) - First + 1;
	const int32 Current = FMath::Max(static_cast<int32>(Tab) - First, 0);
	ShowTab(static_cast<ELoadoutV2Tab>(First + ((Current + (Delta < 0 ? -1 : 1)) % TabCount + TabCount) % TabCount));
}

void ULoadoutV2ScreenWidget::ShowTab(ELoadoutV2Tab NewTab)
{
	Tab = NewTab;
	Mode = Tab == ELoadoutV2Tab::Skills ? ELoadoutV2Mode::SkillEntries : ELoadoutV2Mode::List; // the tab's top level
	Selected = 0;
	Message = FText::GetEmpty();
	RefreshData();
	if (IsOnSkillsTab())
	{
		LogSkillsTab();
	}
	else if (IsOnInventoryList() && Members.IsValidIndex(MemberIndex))
	{
		UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] Inventory tab, %s:\n%s"), *Members[MemberIndex].Name.ToString(), *WoRLoadoutV2::GetInventoryPageString(InventoryPage));
	}
}

void ULoadoutV2ScreenWidget::PutPickedSkill()
{
	if (!SkillChoices.IsValidIndex(Selected) || !SkillRows.IsValidIndex(SkillRowSelected) || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	const FLoadoutScreenChoice Choice = SkillChoices[Selected];
	if (!Choice.Refusal.IsEmpty())
	{
		Message = Choice.Refusal; // greyed with its reason; stays in the picker
		RefreshView();
		return;
	}
	const FLoadoutV2SkillRowRef Ref = SkillRows[SkillRowSelected];
	const FLoadoutScreenRow *Row = WoRLoadoutV2Skills::FindRow(SkillsPage, Ref);
	if (!Row)
	{
		return;
	}
	// Today's Skills page's change (B-440d): through ULoadoutComponent::TryChangeGear, the picks written to the pieces.
	const FLoadoutSkillTab SkillTab = SkillsPage.Source.Tabs[Ref.Tab];
	const FLoadoutScreenRow SlotRow = *Row;
	Message = WoRLoadoutSkills::ApplySkill(GetWorld(), Members[MemberIndex], SkillTab, SlotRow, Choice.Row.Skill);
	if (Message.IsEmpty())
	{
		UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: %s put in a slot of %s"), *Members[MemberIndex].Name.ToString(), *Choice.Row.Name.ToString(),
			   *SkillTab.Title.ToString());
	}
	Mode = ELoadoutV2Mode::SkillRows;
	Selected = SkillRowSelected;
	RefreshData();
}

void ULoadoutV2ScreenWidget::PressA()
{
	switch (Mode)
	{
	case ELoadoutV2Mode::ApparelItems:
		PickApparel();
		return;
	case ELoadoutV2Mode::SkillEntries:
		if (!SkillsPage.Entries.IsValidIndex(Selected))
		{
			return;
		}
		SkillEntry = Selected;
		Mode = ELoadoutV2Mode::SkillRows;
		Selected = 0;
		Message = FText::GetEmpty();
		RefreshData();
		return;
	case ELoadoutV2Mode::SkillRows:
		if (!SkillRows.IsValidIndex(Selected))
		{
			return;
		}
		SkillRowSelected = Selected;
		Mode = ELoadoutV2Mode::SkillPicker;
		Selected = 0;
		Message = FText::GetEmpty();
		RefreshData();
		if (SkillChoices.IsEmpty())
		{
			Message = LOCTEXT("NoSkillToPick", "No learned skill fits this slot.");
			RefreshView();
		}
		return;
	case ELoadoutV2Mode::SkillPicker:
		PutPickedSkill();
		return;
	case ELoadoutV2Mode::List:
		if (Tab == ELoadoutV2Tab::Inventory)
		{
			WearFromInventory();
		}
		else if (Tab == ELoadoutV2Tab::Apparel)
		{
			OpenApparelItems();
		}
		else if (IsOnAddFrame())
		{
			OpenWindowForNew();
		}
		else if (const FLoadoutV2Entry *Entry = GetFocusedEntry())
		{
			OpenWindowForEntry(*Entry);
		}
		return;
	case ELoadoutV2Mode::Options:
		SelectOption();
		return;
	case ELoadoutV2Mode::Saved:
		UseSaved();
		return;
	case ELoadoutV2Mode::WindowKind:
		if (!Kinds.IsValidIndex(Selected))
		{
			return;
		}
		if (!Kinds[Selected].Refusal.IsEmpty())
		{
			Message = Kinds[Selected].Refusal;
			RefreshView();
			return;
		}
		StartDraft(Kinds[Selected].Kind);
		return;
	case ELoadoutV2Mode::WindowSlots:
		if (!Slots.IsValidIndex(Selected))
		{
			return;
		}
		SlotIndex = Selected;
		Mode = ELoadoutV2Mode::WindowItems;
		Selected = 0;
		bComparing = false;
		bRemoveArmed = false;
		PickedSocket = INDEX_NONE;
		bGridFiltered = false; // D-248: a slot's grid opens on All
		Message = FText::GetEmpty();
		RefreshData();
		if (Items.IsEmpty())
		{
			Message = LOCTEXT("NothingToPick", "You own nothing that goes here.");
			RefreshView();
		}
		if (IsGridOpen())
		{
			LogGrid();
		}
		return;
	case ELoadoutV2Mode::WindowItems:
		if (!Items.IsValidIndex(Selected))
		{
			return;
		}
		if (Items[Selected].bEquipped || Items[Selected].bRestore)
		{
			RemoveEquipped(); // B-534: the Restore line is done there too, without the arm step
		}
		else
		{
			PickItem();
		}
		return;
	case ELoadoutV2Mode::WindowAsk:
		ApplyWindowDraft();
		return;
	default:
		return;
	}
}

void ULoadoutV2ScreenWidget::PressCompare()
{
	if (Mode == ELoadoutV2Mode::List || Mode == ELoadoutV2Mode::SkillRows || Mode == ELoadoutV2Mode::SkillPicker) // B-476d: a skill's text too
	{
		bSimpleView = !bSimpleView; // UIScreens B-479c: X switches the view, remembered
		RefreshView();
		return;
	}
	if (Mode == ELoadoutV2Mode::WindowItems)
	{
		bComparing = !bComparing;
		RefreshView();
		return;
	}
	if (Mode == ELoadoutV2Mode::Saved)
	{
		DuplicateSaved();
	}
}

void ULoadoutV2ScreenWidget::PressConfirm()
{
	if (Mode == ELoadoutV2Mode::List)
	{
		if (!Members.IsValidIndex(MemberIndex))
		{
			return;
		}
		ListSelected = Selected;
		Mode = ELoadoutV2Mode::Options;
		Selected = 0;
		bRemoveAllArmed = false;
		Message = FText::GetEmpty();
		RefreshView();
		return;
	}
	if (Mode == ELoadoutV2Mode::WindowSlots || Mode == ELoadoutV2Mode::WindowItems)
	{
		ConfirmWindow();
		return;
	}
	if (Mode == ELoadoutV2Mode::SkillRows)
	{
		// B-488: the first screen's X on a skill, kept: the lit slot's skill comes out. Nothing is lost (an innate spell
		// stays learned; a replaced built-in comes back, Q-440d), so one press.
		const FLoadoutScreenRow *Row = SkillRows.IsValidIndex(Selected) ? WoRLoadoutV2Skills::FindRow(SkillsPage, SkillRows[Selected]) : nullptr;
		if (Row && Members.IsValidIndex(MemberIndex)) // an empty slot or a piece's own skill: ApplySkill says why not
		{
			const FLoadoutSkillTab SkillTab = SkillsPage.Source.Tabs[SkillRows[Selected].Tab];
			const FLoadoutScreenRow SlotRow = *Row;
			Message = WoRLoadoutSkills::ApplySkill(GetWorld(), Members[MemberIndex], SkillTab, SlotRow, nullptr);
			RefreshData();
		}
		return;
	}
	if (Mode == ELoadoutV2Mode::Saved)
	{
		StartRename();
	}
}

void ULoadoutV2ScreenWidget::PressSetActive()
{
	if (Mode != ELoadoutV2Mode::List && Mode != ELoadoutV2Mode::Options)
	{
		return;
	}
	const FLoadoutV2Entry *Entry = GetFocusedEntry();
	if (!Entry || Entry->Kind != ELoadoutV2EntryKind::Weapon || !Members.IsValidIndex(MemberIndex))
	{
		Message = LOCTEXT("SetActiveNotWeapon", "Only a weapon entry can be set active.");
		RefreshView();
		return;
	}
	// Today's screen's rule (B-474, D-207): the weapon in hand is the entry a fight starts with.
	FLoadoutScreenRow Row;
	Row.Kind = ELoadoutScreenRowKind::Weapon;
	Row.Place = Entry->Place == ELoadoutV2Place::Secondary ? ELoadoutScreenPlace::Secondary : ELoadoutScreenPlace::Primary;
	Row.bActive = Entry->bActive || !Page.Entries.ContainsByPredicate([](const FLoadoutV2Entry &Each) { return Each.bActive; });
	const FText Name = Entry->Name;
	Message = WoRLoadoutScreen::SetRowActive(GetWorld(), Members[MemberIndex], Row);
	if (Message.IsEmpty())
	{
		UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: %s set active"), *Members[MemberIndex].Name.ToString(), *Name.ToString());
	}
	if (Mode == ELoadoutV2Mode::Options)
	{
		Mode = ELoadoutV2Mode::List;
		Selected = ListSelected;
	}
	RefreshData();
}

bool ULoadoutV2ScreenWidget::NativeOnHandleBackAction()
{
	Message = FText::GetEmpty();
	if (bRenaming)
	{
		CancelRename(); // B-440e
		return true;
	}
	if (!bOnLoadoutSide && bOnInvFilters)
	{
		bOnInvFilters = false; // D-249: off the inventory's filter row, back to its tiles
		RefreshView();
		return true;
	}
	switch (Mode)
	{
	case ELoadoutV2Mode::Options:
		Mode = ELoadoutV2Mode::List;
		Selected = ListSelected;
		bRemoveAllArmed = false;
		RefreshData();
		return true;
	case ELoadoutV2Mode::SkillPicker:
		Mode = ELoadoutV2Mode::SkillRows;
		Selected = SkillRowSelected;
		SkillChoices.Reset();
		RefreshData();
		return true;
	case ELoadoutV2Mode::SkillRows:
		Mode = ELoadoutV2Mode::SkillEntries;
		Selected = SkillEntry;
		RefreshData();
		return true;
	case ELoadoutV2Mode::ApparelItems:
		Mode = ELoadoutV2Mode::List;
		Selected = ListSelected;
		Items.Reset();
		RefreshData();
		return true;
	case ELoadoutV2Mode::Saved:
		Mode = ELoadoutV2Mode::Options; // one step back, on the Saved loadouts line
		// The cast: a C array's index type isn't int32.
		Selected = FMath::Max(0, static_cast<int32>(Algo::IndexOf(LoadoutV2ScreenStyle::OptionOrder, ELoadoutV2Option::SavedLoadouts)));
		RefreshView();
		return true;
	case ELoadoutV2Mode::WindowAsk:
		Mode = ELoadoutV2Mode::WindowItems; // Not now: the pick stays
		RefreshView();
		return true;
	case ELoadoutV2Mode::WindowItems:
		SetHovered(INDEX_NONE, INDEX_NONE, INDEX_NONE); // D-248: a hovered tile's index is not a slot row's
		HoveredGridTab = INDEX_NONE;
		Mode = ELoadoutV2Mode::WindowSlots;
		Selected = SlotIndex;
		Items.Reset();
		PickedSocket = INDEX_NONE;
		bComparing = false;
		bRemoveArmed = false;
		RefreshView();
		return true;
	case ELoadoutV2Mode::WindowKind:
	case ELoadoutV2Mode::WindowSlots:
		CloseWindow();
		return true;
	case ELoadoutV2Mode::List:
	default:
		return Super::NativeOnHandleBackAction();
	}
}

FReply ULoadoutV2ScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (bRenaming)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent); // B-440e: typing a name; the box keeps Enter and Esc
	}
	if (!bOnLoadoutSide && IsInventoryGrid())
	{
		// D-249: the keys on the inventory's side. The character keys and Back fall through to the shared handling.
		if (IsUpKey(Key) || IsDownKey(Key) || IsLeftKey(Key) || IsRightKey(Key))
		{
			MoveInventoryTile(IsRightKey(Key) ? 1 : IsLeftKey(Key) ? -1 : 0, IsDownKey(Key) ? 1 : IsUpKey(Key) ? -1 : 0);
			return FReply::Handled();
		}
		if (IsTabPrevKey(Key) || IsTabNextKey(Key) || IsSecondActionKey(Key) || IsHelpKey(Key) || IsConfirmKey(Key) || IsToggleKey(Key))
		{
			if (InKeyEvent.IsRepeat())
			{
				return FReply::Handled();
			}
			if (IsTabPrevKey(Key) || IsTabNextKey(Key))
			{
				MoveInventoryTab(IsTabNextKey(Key) ? 1 : -1);
			}
			else if (IsSecondActionKey(Key))
			{
				ToggleLoadout();
			}
			else if (IsHelpKey(Key))
			{
				CycleInventorySort();
			}
			else if (IsConfirmKey(Key) && bOnInvFilters)
			{
				bOnInvFilters = false; // the filter is already the lit one: back to its tiles
				RefreshView();
			}
			else if (IsConfirmKey(Key) && InvPage.Tiles.IsValidIndex(InvSelected))
			{
				// TODO: D-249 piece 2: Equip from here (the loadout comes in and asks for the slot); piece 3: Give / Store.
				Message = LOCTEXT("InventoryVerbsLater", "Equipping from here comes next. For now: open the loadout and pick a slot.");
				RefreshView();
			}
			return FReply::Handled(); // X does nothing on this side
		}
		if (Key == EKeys::V || Key == EKeys::Gamepad_LeftThumbstick || Key == EKeys::Six)
		{
			return FReply::Handled(); // the loadout's own keys (the view, Set active) do nothing from this side
		}
	}
	else if (bOnLoadoutSide && IsLoadoutAtTopLevel() && IsRightKey(Key))
	{
		FocusInventorySide(); // D-249: Right from the loadout's list: the inventory's side
		return FReply::Handled();
	}
	if (IsGridOpen())
	{
		// D-248 step 1: in the grid the arrows go tile to tile, and the tab keys turn the filter.
		if (IsUpKey(Key) || IsDownKey(Key) || IsLeftKey(Key) || IsRightKey(Key))
		{
			MoveInGrid(IsRightKey(Key) ? 1 : IsLeftKey(Key) ? -1 : 0, IsDownKey(Key) ? 1 : IsUpKey(Key) ? -1 : 0);
			return FReply::Handled();
		}
		if (IsTabPrevKey(Key) || IsTabNextKey(Key))
		{
			if (!InKeyEvent.IsRepeat())
			{
				CycleGridFilter(IsTabNextKey(Key) ? 1 : -1);
			}
			return FReply::Handled();
		}
	}
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveSelection(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsSubTabPrevKey(Key) || IsSubTabNextKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			MoveMember(IsSubTabNextKey(Key) ? 1 : -1);
		}
		return FReply::Handled();
	}
	if ((IsTabPrevKey(Key) || IsTabNextKey(Key)) && (Mode == ELoadoutV2Mode::List || Mode == ELoadoutV2Mode::SkillEntries)) // B-440f, B-476d
	{
		if (!InKeyEvent.IsRepeat())
		{
			SwitchTab(IsTabNextKey(Key) ? 1 : -1);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::V && (Mode == ELoadoutV2Mode::List || Mode == ELoadoutV2Mode::SkillRows || Mode == ELoadoutV2Mode::SkillPicker)) // the Details key on a keyboard (InputMap): the view, as X
	{
		if (!InKeyEvent.IsRepeat())
		{
			PressCompare();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_LeftThumbstick || Key == EKeys::Six) // B-474 (D-207): the fight's switch key
	{
		if (!InKeyEvent.IsRepeat())
		{
			PressSetActive();
		}
		return FReply::Handled();
	}
	if (IsConfirmKey(Key) || IsToggleKey(Key) || IsSecondActionKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			IsConfirmKey(Key) ? PressA() : IsToggleKey(Key) ? PressCompare() : PressConfirm();
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

// ==================== OPTIONS ====================

void ULoadoutV2ScreenWidget::SelectOption()
{
	using namespace LoadoutV2ScreenStyle;
	if (Selected < 0 || Selected >= OptionCount || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	switch (OptionOrder[Selected])
	{
	case ELoadoutV2Option::SetActive:
		PressSetActive();
		return;
	case ELoadoutV2Option::RemoveAllGear:
	{
		if (!bRemoveAllArmed)
		{
			bRemoveAllArmed = true; // asks first
			RefreshView();
			return;
		}
		bRemoveAllArmed = false;
		ULoadoutComponent *Loadout = nullptr;
		UInventoryComponent *Inventory = nullptr;
		const FCombatLoadout *Active = nullptr;
		if (!FindActive(Loadout, Inventory, Active))
		{
			Message = LOCTEXT("MemberNotHere", "This character isn't here.");
			RefreshView();
			return;
		}
		const ECharacterClass Class = Members[MemberIndex].Class;
		int32 Removed = 0;
		FText Refusal;
		if (!Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [Class, &Removed](FCombatLoadout &Changed) { Removed = WoRLoadoutV2::RemoveAllGear(Changed, Class); },
									Refusal))
		{
			Message = Refusal;
		}
		else
		{
			Message = Removed > 0 ? FText::Format(LOCTEXT("RemovedAll", "{0} entries came out. The first stays: every loadout keeps one."), FText::AsNumber(Removed))
								  : LOCTEXT("RemovedNone", "Nothing to remove: every loadout keeps its first entry.");
			UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: Remove all gear took out %d entries"), *Members[MemberIndex].Name.ToString(), Removed);
		}
		Mode = ELoadoutV2Mode::List;
		Selected = 0;
		RefreshData();
		return;
	}
	case ELoadoutV2Option::Help:
		Message = LOCTEXT("HelpText", "Left: your entries; the box shows the focused one. {Confirm} edits an entry, or adds one on the frame. {Toggle} switches the full and "
									  "simple view. {SetActive} puts a weapon entry in hand. Cells: only what sits in the sockets costs (crystal or stone 1, "
									  "evolution 2).");
		RefreshView();
		return;
	case ELoadoutV2Option::SavedLoadouts:
		OpenSaved();
		return;
	case ELoadoutV2Option::CosmeticsShortcuts:
	default:
		Message = LOCTEXT("OptionNotYet", "Not here yet.");
		RefreshView();
		return;
	}
}

// ==================== APPAREL (B-440f) ====================

void ULoadoutV2ScreenWidget::OpenApparelItems()
{
	if (!GetFocusedApparelRow())
	{
		return;
	}
	ListSelected = Selected;
	Mode = ELoadoutV2Mode::ApparelItems;
	Selected = 0;
	PickedSocket = INDEX_NONE;
	Message = FText::GetEmpty();
	RefreshData();
	if (Items.IsEmpty())
	{
		Mode = ELoadoutV2Mode::List;
		Selected = ListSelected;
		Message = LOCTEXT("NoApparelToPick", "You own no apparel that isn't worn already.");
		RefreshView();
	}
}

void ULoadoutV2ScreenWidget::BuildApparelItems()
{
	Items.Reset();
	const FLoadoutScreenRow *Row = GetFocusedApparelRow();
	if (!Row || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	if (Row->Kind == ELoadoutScreenRowKind::Apparel) // the worn piece first: A Remove
	{
		FLoadoutV2WindowItem &Worn = Items.AddDefaulted_GetRef();
		Worn.Name = Row->Name;
		Worn.Line = LoadoutV2ScreenStyle::ApparelLine(*Row);
		Worn.bEquipped = true;
		Worn.Socket.Row = *Row;
	}
	for (const FLoadoutScreenChoice &Choice : WoRLoadoutApparel::BuildApparelChoices(GetWorld(), Members[MemberIndex], *Row))
	{
		FLoadoutV2WindowItem &Item = Items.AddDefaulted_GetRef();
		Item.Name = Choice.Row.Name;
		Item.Line = LoadoutV2ScreenStyle::ApparelLine(Choice.Row);
		Item.Refusal = Choice.Refusal;
		Item.Socket = Choice;
	}
}

void ULoadoutV2ScreenWidget::PickApparel()
{
	const FLoadoutScreenRow *Row = GetFocusedApparelRow();
	if (!Row || !Items.IsValidIndex(Selected) || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	const FLoadoutScreenRow Place = *Row; // copies: RefreshData rebuilds the rows and the list
	const FLoadoutV2WindowItem Item = Items[Selected];
	Message = Item.bEquipped ? WoRLoadoutApparel::TakeOffRow(GetWorld(), Members[MemberIndex], Place)
							 : WoRLoadoutApparel::ApplyApparelChoice(GetWorld(), Members[MemberIndex], Place, Item.Socket);
	if (Message.IsEmpty())
	{
		UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: apparel %s %s"), *Members[MemberIndex].Name.ToString(), *Item.Name.ToString(),
			   Item.bEquipped ? TEXT("taken off") : TEXT("put on"));
		Mode = ELoadoutV2Mode::List;
		Selected = ListSelected; // RefreshData keeps it in range (a taken-off piece's line is gone)
		Items.Reset();
	}
	RefreshData();
}

// ==================== SAVED LOADOUTS (B-440e) ====================

void ULoadoutV2ScreenWidget::OpenSaved()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!FindActive(Loadout, Inventory, Active))
	{
		Message = LOCTEXT("MemberNotHere", "This character isn't here.");
		RefreshView();
		return;
	}
	Mode = ELoadoutV2Mode::Saved;
	Selected = FMath::Max(0, Inventory->ActiveLoadoutIndex); // the one in use lit
	Message = FText::GetEmpty();
	RefreshData();
}

void ULoadoutV2ScreenWidget::UseSaved()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!Saved.Rows.IsValidIndex(Selected) || !FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	const FLoadoutSavedRow Row = Saved.Rows[Selected];
	if (Row.IsNewLine())
	{
		int32 NewIndex = INDEX_NONE;
		Message = WoRLoadoutSaved::CreateNew(*Loadout, *Inventory, NewIndex);
		Selected = NewIndex != INDEX_NONE ? NewIndex : Selected; // the new one lit, ready for Y
		RefreshData();
		return;
	}
	Message = WoRLoadoutSaved::SetActive(*Loadout, *Inventory, Row.LoadoutIndex);
	if (Message.IsEmpty())
	{
		// The name is typed by the player and the message is shown as a key hint: its braces stay as typed.
		Message = FText::Format(LOCTEXT("SavedNowUsed", "{0} is now in use."), WoRKeyHints::Literal(Row.Name));
		Mode = ELoadoutV2Mode::List; // back to the entries, now of that loadout
		Selected = 0;
		ListSelected = 0;
	}
	RefreshData();
}

void ULoadoutV2ScreenWidget::DuplicateSaved()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!Saved.Rows.IsValidIndex(Selected) || Saved.Rows[Selected].IsNewLine() || !FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	int32 NewIndex = INDEX_NONE;
	Message = WoRLoadoutSaved::Duplicate(*Loadout, *Inventory, Saved.Rows[Selected].LoadoutIndex, NewIndex);
	Selected = NewIndex != INDEX_NONE ? NewIndex : Selected;
	RefreshData();
}

void ULoadoutV2ScreenWidget::StartRename()
{
	if (bRenaming || !Saved.Rows.IsValidIndex(Selected))
	{
		return;
	}
	if (Saved.Rows[Selected].IsNewLine())
	{
		Message = LOCTEXT("RenameNewLine", "Make the loadout first ({Confirm}), then name it.");
		RefreshView();
		return;
	}
	bRenaming = true;
	SetHovered(INDEX_NONE, INDEX_NONE, INDEX_NONE); // the mouse belongs to the name box now
	Message = FText::GetEmpty();
	if (NameBox)
	{
		NameBox->SetText(Saved.Rows[Selected].Name);
		NameBox->SetVisibility(ESlateVisibility::Visible); // not SetShown: the box takes clicks and the keys
		NameBox->SetKeyboardFocus();
	}
	RefreshView();
}

void ULoadoutV2ScreenWidget::CommitRename(const FText &NewName)
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (bRenaming && Saved.Rows.IsValidIndex(Selected) && FindActive(Loadout, Inventory, Active))
	{
		Message = WoRLoadoutSaved::Rename(*Loadout, *Inventory, Saved.Rows[Selected].LoadoutIndex, NewName);
	}
	CancelRename(); // closes the box; the message stays
}

void ULoadoutV2ScreenWidget::CancelRename()
{
	if (!bRenaming)
	{
		return;
	}
	bRenaming = false;
	if (NameBox)
	{
		WoRMenuStyle::SetShown(NameBox, false);
		if (UWidget *Target = GetDesiredFocusTarget())
		{
			Target->SetFocus(); // the keys come back to the screen
		}
	}
	RefreshData();
}

void ULoadoutV2ScreenWidget::HandleNameCommitted(const FText &Text, ETextCommit::Type CommitMethod)
{
	if (!bRenaming)
	{
		return; // hiding the box moves its focus away, which commits again
	}
	CommitMethod == ETextCommit::OnEnter ? CommitRename(Text) : CancelRename();
}

// ==================== THE ADD GEAR WINDOW ====================

void ULoadoutV2ScreenWidget::OpenWindowForNew()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	ListSelected = Selected;
	bEditing = false;
	EditEntry = FLoadoutV2Entry();
	Draft = FLoadoutV2Draft();
	Slots.Reset();
	Items.Reset();
	Mode = ELoadoutV2Mode::WindowKind;
	Selected = 0;
	Message = FText::GetEmpty();
	RefreshData();
	// Start on the first kind that fits (UIScreens: the window asks Weapon, Adapter or Evolution).
	const int32 FirstOpen = Kinds.IndexOfByPredicate([](const FLoadoutV2KindChoice &Kind) { return Kind.Refusal.IsEmpty(); });
	Selected = FirstOpen == INDEX_NONE ? 0 : FirstOpen;
	RefreshView();
}

void ULoadoutV2ScreenWidget::OpenWindowForEntry(const FLoadoutV2Entry &Entry)
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	ListSelected = Selected;
	bEditing = true;
	EditEntry = Entry;
	Draft = WoRLoadoutV2::DraftFromEntry(*Active, Entry);
	// B-531 (D-230): the same first question as "+ Add equipment"; the entry's own kind is lit (A: edit it as before).
	Slots.Reset();
	Items.Reset();
	Mode = ELoadoutV2Mode::WindowKind;
	Message = FText::GetEmpty();
	RefreshData();
	const int32 Own = Kinds.IndexOfByPredicate([this](const FLoadoutV2KindChoice &Kind) { return Kind.Kind == Draft.Kind; });
	Selected = Own == INDEX_NONE ? 0 : Own;
	RefreshView();
}

bool ULoadoutV2ScreenWidget::IsReplacingKind() const
{
	return bEditing && Draft.Kind != WoRLoadoutV2::GetDraftKind(EditEntry.Kind);
}

void ULoadoutV2ScreenWidget::StartDraft(ELoadoutV2DraftKind Kind)
{
	// An edit keeps the entry's pieces (OpenWindowForEntry's draft); another kind starts empty and replaces the entry
	// on Y Confirm only (WoRLoadoutV2::ReplaceEntry), so Back before then leaves it as it was.
	if (!bEditing || Kind != WoRLoadoutV2::GetDraftKind(EditEntry.Kind))
	{
		Draft = FLoadoutV2Draft();
		Draft.Kind = Kind;
	}
	Slots.Reset();
	switch (Kind)
	{
	case ELoadoutV2DraftKind::Weapon:
		Slots = {ELoadoutV2Slot::MainHand, ELoadoutV2Slot::OffHand, ELoadoutV2Slot::Socket1, ELoadoutV2Slot::Socket2};
		break;
	case ELoadoutV2DraftKind::Adapter:
		Slots = {ELoadoutV2Slot::Adapter, ELoadoutV2Slot::Socket1, ELoadoutV2Slot::Socket2};
		break;
	case ELoadoutV2DraftKind::Evolution:
	default:
		Slots = {ELoadoutV2Slot::Evolution};
		break;
	}
	SlotIndex = 0;
	Items.Reset();
	PickedSocket = INDEX_NONE;
	bComparing = false;
	bRemoveArmed = false;
	Mode = ELoadoutV2Mode::WindowSlots;
	Selected = 0;
	Message = FText::GetEmpty();
	RefreshView();
}

FText ULoadoutV2ScreenWidget::GetSlotName(ELoadoutV2Slot GearSlot) const
{
	switch (GearSlot)
	{
	case ELoadoutV2Slot::MainHand:
		return LOCTEXT("SlotMainHand", "Main hand");
	case ELoadoutV2Slot::OffHand:
		return LOCTEXT("SlotOffHand", "Off hand");
	case ELoadoutV2Slot::Adapter:
		return WoRGameNames::GetAdapterWord();
	case ELoadoutV2Slot::Evolution:
		return Members.IsValidIndex(MemberIndex) ? FText::Format(LOCTEXT("SlotEvolution", "{0}'s evolution"), Members[MemberIndex].Name)
												 : LOCTEXT("SlotEvolutionPlain", "Evolution");
	case ELoadoutV2Slot::Socket1:
		return LOCTEXT("SlotSocket1", "Augment socket 1");
	case ELoadoutV2Slot::Socket2:
	default:
		return LOCTEXT("SlotSocket2", "Augment socket 2");
	}
}

FText ULoadoutV2ScreenWidget::GetSlotValue(ELoadoutV2Slot GearSlot) const
{
	using namespace LoadoutV2ScreenStyle;
	switch (GearSlot)
	{
	case ELoadoutV2Slot::MainHand:
		return Draft.MainHand.IsValid() ? WeaponName(Draft.MainHand) : LOCTEXT("SlotPick", "pick one");
	case ELoadoutV2Slot::OffHand:
		return Draft.OffHand.IsValid() ? WeaponName(Draft.OffHand) : LOCTEXT("SlotOffHandEmpty", "empty (a weapon alone)");
	case ELoadoutV2Slot::Adapter:
		return Draft.Adapter.Ring ? WoRGameNames::GetAdapterName(Draft.Adapter.Ring, Draft.Socket1) : LOCTEXT("SlotPick", "pick one");
	case ELoadoutV2Slot::Evolution:
		return Draft.Socket1.IsEvolution() && Draft.Socket1.Evolution.Item ? FText::FromString(Draft.Socket1.Evolution.Item->ItemName) : LOCTEXT("SlotPick", "pick one");
	case ELoadoutV2Slot::Socket1:
		return AttachmentText(Draft.Socket1);
	case ELoadoutV2Slot::Socket2:
	default:
		return AttachmentText(Draft.Socket2);
	}
}

namespace LoadoutV2DraftSocket
{
	/** B-516: a new entry's piece is not in the loadout yet, so its sockets are read off a copy with the draft in
	 *  (Preview, the budget the socket lines count against). Empty on success; else why it can't be socketed yet. */
	FText FindDraftRow(const FCombatLoadout &Active, ECharacterClass Class, const FLoadoutV2Draft &Draft, const FLoadoutV2Entry *Replacing, FCombatLoadout &OutPreview,
					   FLoadoutScreenRow &OutRow)
	{
		const bool bAdapter = Draft.Kind == ELoadoutV2DraftKind::Adapter;
		const FGuid PieceID = bAdapter ? Draft.Adapter.PersistentID : Draft.Kind == ELoadoutV2DraftKind::Weapon ? Draft.MainHand.PersistentID : FGuid();
		if (!PieceID.IsValid())
		{
			return LOCTEXT("SocketPickPieceFirst", "Pick the piece first.");
		}
		OutPreview = Active;
		FText Refusal;
		// B-531: a piece replacing an entry (Replacing) is previewed in its place, as Y Confirm will put it.
		if (Replacing ? !WoRLoadoutV2::ReplaceEntry(OutPreview, Class, *Replacing, Draft, Refusal) : !WoRLoadoutV2::ApplyDraft(OutPreview, Class, Draft, Refusal))
		{
			return Refusal;
		}
		const ELoadoutScreenRowKind Kind = bAdapter ? ELoadoutScreenRowKind::Adapter : ELoadoutScreenRowKind::Weapon;
		const FLoadoutScreenPage Preview = WoRLoadoutScreen::BuildGearPage(OutPreview, Class);
		const FLoadoutScreenRow *Found = Preview.Rows.FindByPredicate([Kind, &PieceID](const FLoadoutScreenRow &Row) { return Row.Kind == Kind && Row.GearID == PieceID; });
		if (!Found)
		{
			return LOCTEXT("SocketPickPieceFirst", "Pick the piece first.");
		}
		OutRow = *Found;
		return FText::GetEmpty();
	}

	/** The draft's piece read again from the owned copy once its socket changed (the sockets are the piece's, Q-476c). */
	void RereadPiece(const UInventoryComponent &Inventory, FLoadoutV2Draft &Draft)
	{
		if (Draft.Kind == ELoadoutV2DraftKind::Adapter)
		{
			if (const FRingInventoryEntry *Owned = Inventory.FindOwnedRing(Draft.Adapter.PersistentID))
			{
				Draft.Adapter = *Owned;
				Draft.Socket1 = Owned->AttachedItem;
				Draft.Socket2 = Owned->StoneSocket;
			}
		}
		else if (Draft.Kind == ELoadoutV2DraftKind::Weapon)
		{
			if (const FWeaponInventoryEntry *Owned = Inventory.FindOwnedWeapon(Draft.MainHand.PersistentID))
			{
				Draft.MainHand = *Owned;
				Draft.Socket1 = Owned->AttachedItem;
				Draft.Socket2 = Owned->StoneSocket;
			}
		}
	}
}

bool ULoadoutV2ScreenWidget::FindScreenRow(FLoadoutScreenRow &OutRow) const
{
	if (!bEditing || IsReplacingKind() || !Members.IsValidIndex(MemberIndex))
	{
		return false; // a new piece (B-531: or one replacing the entry): its row is read off a preview (FindDraftRow)
	}
	const FLoadoutScreenPage GearPage = WoRLoadoutScreen::BuildGearPage(GetWorld(), Members[MemberIndex]);
	const ELoadoutScreenRowKind Kind = EditEntry.Kind == ELoadoutV2EntryKind::Adapter ? ELoadoutScreenRowKind::Adapter : ELoadoutScreenRowKind::Weapon;
	const ELoadoutScreenPlace Place = EditEntry.Place == ELoadoutV2Place::Secondary	 ? ELoadoutScreenPlace::Secondary
									  : EditEntry.Place == ELoadoutV2Place::Adapter ? ELoadoutScreenPlace::Adapter
																					 : ELoadoutScreenPlace::Primary;
	const FLoadoutScreenRow *Found = GearPage.Rows.FindByPredicate([this, Kind, Place](const FLoadoutScreenRow &Row)
																	{ return Row.Kind == Kind && Row.Place == Place && (Place != ELoadoutScreenPlace::Adapter || Row.AdapterIndex == EditEntry.AdapterIndex); });
	if (!Found)
	{
		return false;
	}
	OutRow = *Found;
	return true;
}

void ULoadoutV2ScreenWidget::BuildItems()
{
	using namespace LoadoutV2ScreenStyle;
	// The picked socket line is found again by its piece after the rebuild (the list can change order).
	const bool bHadPick = Items.IsValidIndex(PickedSocket);
	const FLoadoutScreenChoice PickedBefore = bHadPick ? Items[PickedSocket].Socket : FLoadoutScreenChoice();
	ON_SCOPE_EXIT
	{
		PickedSocket = bHadPick ? Items.IndexOfByPredicate([&PickedBefore](const FLoadoutV2WindowItem &Item)
														  { return !Item.bEquipped && Item.Socket.Crystal == PickedBefore.Crystal && Item.Socket.EvolutionID == PickedBefore.EvolutionID; })
								: INDEX_NONE;
	};
	Items.Reset();
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!Slots.IsValidIndex(SlotIndex) || !FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	const ECharacterClass Class = Members[MemberIndex].Class;
	const FLoadoutV2Draft Current = bEditing ? WoRLoadoutV2::DraftFromEntry(*Active, EditEntry) : FLoadoutV2Draft();
	// The draft with one piece changed, checked as Y Confirm would check it.
	const auto Check = [this, Active, Class](const FLoadoutV2Draft &Changed)
	{
		int32 Cells = 0;
		const FText Refusal = bEditing ? WoRLoadoutV2::CheckEntryEdit(*Active, Class, EditEntry, Changed, Cells) : WoRLoadoutV2::CheckDraft(*Active, Class, Changed, Cells);
		return TPair<FText, int32>(Refusal, Cells);
	};
	const auto CellsLine = [this](int32 Cells)
	{ return FText::Format(LOCTEXT("ItemCellsAfter", "Cells after: {0} of {1}"), FText::AsNumber(Cells), FText::AsNumber(Page.CellsBudget)); };
	const auto IsElsewhere = [Active, &Current](const FGuid &ID)
	{
		// In another entry of this loadout (this entry's own pieces may move between its hands).
		if (!ID.IsValid() || ID == Current.MainHand.PersistentID || ID == Current.OffHand.PersistentID || ID == Current.Adapter.PersistentID)
		{
			return false;
		}
		FCombatLoadout Probe = *Active;
		return UInventoryComponent::StripFromLoadout(Probe, ID);
	};

	switch (Slots[SlotIndex])
	{
	case ELoadoutV2Slot::MainHand:
	case ELoadoutV2Slot::OffHand:
	{
		const bool bMain = Slots[SlotIndex] == ELoadoutV2Slot::MainHand;
		const FWeaponInventoryEntry &InSlot = bMain ? Current.MainHand : Current.OffHand;
		if (InSlot.IsValid())
		{
			FLoadoutV2WindowItem &Equipped = Items.AddDefaulted_GetRef();
			Equipped.bEquipped = true;
			Equipped.bWorn = true;
			Equipped.Weapon = InSlot;
			Equipped.Name = WeaponName(InSlot);
			// B-513, B-515: what the two hands make ("Sword and Shield"); a weapon alone has its type after its name.
			Equipped.Line = bMain && Current.OffHand.IsValid() ? WoRGameNames::GetHeldWeaponTypeName(InSlot.Weapon, Current.OffHand.Weapon) : FText::GetEmpty();
		}
		const TArray<FLoadoutV2OffHandChoice> OffHands = bMain ? TArray<FLoadoutV2OffHandChoice>() : WoRLoadoutV2::BuildOffHandChoices(Draft.MainHand, Inventory->Weapons, Class);
		const int32 FirstPiece = Items.Num();
		ON_SCOPE_EXIT { ApplyGridFilter(); };					// D-248: declared first, so it runs after the stacking below
		ON_SCOPE_EXIT { StackPieceLines(Items, FirstPiece); }; // B-531 (D-230)
		for (const FWeaponInventoryEntry &Owned : Inventory->Weapons)
		{
			if (!Owned.Weapon || (InSlot.IsValid() && Owned.PersistentID == InSlot.PersistentID) || Owned.PersistentID == (bMain ? Draft.OffHand : Draft.MainHand).PersistentID)
			{
				continue;
			}
			FLoadoutV2WindowItem &Item = Items.AddDefaulted_GetRef();
			Item.Weapon = Owned;
			Item.Name = WeaponName(Owned);
			FLoadoutV2Draft Changed = Draft;
			bool bOffHandComesOff = false;
			if (bMain)
			{
				bOffHandComesOff = WoRLoadoutV2::SetMainHand(Changed, Owned);
			}
			else
			{
				Changed.OffHand = Owned;
			}
			const TPair<FText, int32> Result = Check(Changed);
			// The type is after the name (the row's draw, GetWeaponRowName).
			Item.Line = bOffHandComesOff ? FText::Format(LOCTEXT("WeaponItemLineOffHand", "{0} · the off hand comes off"), CellsLine(Result.Value)) : CellsLine(Result.Value);
			const FLoadoutV2OffHandChoice *Pair = OffHands.FindByPredicate([&Owned](const FLoadoutV2OffHandChoice &Each) { return Each.Weapon.PersistentID == Owned.PersistentID; });
			Item.bWorn = IsElsewhere(Owned.PersistentID); // D-248: worn in another entry: the tick on its tile
			Item.Refusal = Item.bWorn ? LOCTEXT("RefuseInOtherEntry", "In another entry.")
						   : Pair && !Pair->Refusal.IsEmpty() ? Pair->Refusal
															  : Result.Key;
		}
		return;
	}
	case ELoadoutV2Slot::Adapter:
	{
		if (Current.Adapter.IsValid())
		{
			FLoadoutV2WindowItem &Equipped = Items.AddDefaulted_GetRef();
			Equipped.bEquipped = true;
			Equipped.Adapter = Current.Adapter;
			Equipped.Name = WoRGameNames::GetAdapterName(Current.Adapter.Ring, Current.Socket1); // B-514b: "Fire Ring"
		}
		const int32 FirstPiece = Items.Num();
		ON_SCOPE_EXIT { StackPieceLines(Items, FirstPiece); }; // B-531 (D-230)
		if (!bEditing)
		{
			for (const FLoadoutV2PieceChoice &Choice : WoRLoadoutV2::BuildAdapterChoices(*Active, Class, Inventory->Rings))
			{
				FLoadoutV2WindowItem &Item = Items.AddDefaulted_GetRef();
				Item.Adapter = Choice.Adapter;
				Item.Name = Choice.Name;
				Item.Line = FText::Format(LOCTEXT("PieceItemLine", "{0} · {1}"), Choice.Line, CellsLine(Choice.CellsAfter));
				Item.Refusal = Choice.Refusal;
			}
			return;
		}
		for (const FRingInventoryEntry &Owned : Inventory->Rings)
		{
			if (!Owned.Ring || Owned.PersistentID == Current.Adapter.PersistentID)
			{
				continue;
			}
			FLoadoutV2WindowItem &Item = Items.AddDefaulted_GetRef();
			Item.Adapter = Owned;
			Item.Name = WoRGameNames::GetAdapterName(Owned.Ring, Owned.AttachedItem);
			FLoadoutV2Draft Changed = Draft;
			Changed.Adapter = Owned;
			Changed.Socket1 = Owned.AttachedItem; // the new adapter brings its own sockets (they live on the piece)
			Changed.Socket2 = Owned.StoneSocket;
			const TPair<FText, int32> Result = Check(Changed);
			Item.Line = FText::Format(LOCTEXT("PieceItemLine", "{0} · {1}"), WoRGameNames::GetAdapterFormName(Owned.Ring->Form), CellsLine(Result.Value));
			Item.Refusal = IsElsewhere(Owned.PersistentID) ? LOCTEXT("RefuseInOtherEntry", "In another entry.") : Result.Key;
		}
		return;
	}
	case ELoadoutV2Slot::Evolution:
	{
		const UEvolutionInventoryComponent *Evolutions = Inventory->GetOwner() ? Inventory->GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
		if (!Evolutions)
		{
			return;
		}
		TSet<FGuid> InUse;
		for (const FEvolutionInventoryEntry &Owned : Evolutions->Entries)
		{
			if (Inventory->IsEvolutionInUse(Owned.InstanceID))
			{
				InUse.Add(Owned.InstanceID);
			}
		}
		for (const FLoadoutV2PieceChoice &Choice : WoRLoadoutV2::BuildEvolutionChoices(*Active, Class, Evolutions->Entries, InUse))
		{
			FLoadoutV2WindowItem &Item = Items.AddDefaulted_GetRef();
			Item.Evolution = Choice.Evolution;
			Item.Name = Choice.Name;
			Item.Line = Choice.Line;
			Item.bEquipped = bEditing && Choice.Evolution.InstanceID == Current.EvolutionInstance;
			if (Item.bEquipped)
			{
				continue; // its own place: in use by this entry, not refused
			}
			if (bEditing && Choice.Refusal.IsEmpty() == false && !InUse.Contains(Choice.Evolution.InstanceID))
			{
				// An edit swaps the evolution in its place: check the swap, not a second one.
				FLoadoutV2Draft Changed = Draft;
				Changed.Socket1 = WoRLoadoutV2::MakeEvolutionAttachment(Choice.Evolution);
				Changed.EvolutionInstance = Choice.Evolution.InstanceID;
				Item.Refusal = Check(Changed).Key;
				continue;
			}
			Item.Refusal = Choice.Refusal;
		}
		Items.StableSort([](const FLoadoutV2WindowItem &A, const FLoadoutV2WindowItem &B) { return A.bEquipped && !B.bEquipped; });
		return;
	}
	case ELoadoutV2Slot::Socket1:
	case ELoadoutV2Slot::Socket2:
	default:
	{
		const int32 Socket = Slots[SlotIndex] == ELoadoutV2Slot::Socket1 ? LoadoutScreenSockets::CRYSTAL : LoadoutScreenSockets::STONE;
		FLoadoutScreenRow Row;
		FCombatLoadout Preview;
		const bool bNewEntry = !FindScreenRow(Row);
		const FText NotYet = bNewEntry ? LoadoutV2DraftSocket::FindDraftRow(*Active, Class, Draft, bEditing ? &EditEntry : nullptr, Preview, Row) : FText::GetEmpty();
		if (!NotYet.IsEmpty())
		{
			FLoadoutV2WindowItem &Note = Items.AddDefaulted_GetRef();
			Note.Name = AttachmentText(Socket == LoadoutScreenSockets::CRYSTAL ? Draft.Socket1 : Draft.Socket2);
			Note.Refusal = NotYet;
			return;
		}
		if (Row.Sockets.IsValidIndex(Socket) && !Row.Sockets[Socket].bEmpty)
		{
			FLoadoutV2WindowItem &Equipped = Items.AddDefaulted_GetRef();
			Equipped.bEquipped = true;
			Equipped.Name = Row.Sockets[Socket].Name;
			Equipped.Line = Row.Sockets[Socket].Label;
			const FLoadoutV2Draft &Piece = bNewEntry ? Draft : Current;
			Equipped.Socket.Attachment = Socket == LoadoutScreenSockets::CRYSTAL ? Piece.Socket1 : Piece.Socket2; // B-484: for the box
			if (bRemoveArmed)
			{
				FText Refusal;
				Equipped.Line = SocketRemoveWarning(Row, Socket, Refusal);
				Equipped.Refusal = Refusal;
			}
		}
		const bool bRestoreLine = WoRLoadoutScreen::IsRestoreOffered(Row, Socket);
		if (bRestoreLine)
		{
			FLoadoutV2WindowItem &Restore = Items.AddDefaulted_GetRef();
			Restore.bRestore = true;
			Restore.Name = LOCTEXT("RestoreLineName", "Restore");
			Restore.Line = WoRLoadoutScreen::DescribeRestore(GetWorld(), Members[MemberIndex], Row, Restore.Refusal);
			Restore.Line = Restore.Line.IsEmpty() ? Restore.Refusal : Restore.Line;
		}
		for (const FLoadoutScreenChoice &Choice : WoRLoadoutScreen::BuildSocketChoices(GetWorld(), Members[MemberIndex], Row, Socket, bNewEntry ? &Preview : nullptr))
		{
			FLoadoutV2WindowItem &Item = Items.AddDefaulted_GetRef();
			Item.Socket = Choice;
			Item.Name = Choice.Row.Name;
			Item.TierText = Choice.Row.Tier; // B-485: written after the name only when there is no tier badge
			Item.Line = FText::Format(LOCTEXT("SocketItemLine", "{0} · Cells after: {1} of {2}"), Choice.Row.KindLine, FText::AsNumber(Choice.NewSpent),
									  FText::AsNumber(Page.CellsBudget));
			Item.Refusal = Choice.Refusal;
		}
		if (Items.Num() == (bRestoreLine ? 2 : 1) && Items[0].bEquipped)
		{
			// Today's socket rule: a full socket is emptied first (its gem goes, CL-28b), then filled.
			Items[0].Line = bRemoveArmed ? Items[0].Line : FText::FormatNamed(LOCTEXT("SocketFullLine", "{What} · {Confirm} Remove to put another in"), TEXT("What"), Items[0].Line);
		}
		return;
	}
	}
}

void ULoadoutV2ScreenWidget::PickItem()
{
	if (!Items.IsValidIndex(Selected) || !Slots.IsValidIndex(SlotIndex))
	{
		return;
	}
	const FLoadoutV2WindowItem &Item = Items[Selected];
	if (!Item.Refusal.IsEmpty())
	{
		Message = Item.Refusal;
		RefreshView();
		return;
	}
	Message = FText::GetEmpty();
	if (!PutPieceInDraft(Slots[SlotIndex], Item))
	{
		PickedSocket = PickedSocket == Selected ? INDEX_NONE : Selected; // a socket goes in on Y Confirm
		RefreshView();
		return;
	}
	// A piece is picked: back to the slots, the draft shown in the box.
	SetHovered(INDEX_NONE, INDEX_NONE, INDEX_NONE); // D-248: a hovered tile's index is not a slot row's
	HoveredGridTab = INDEX_NONE;
	Mode = ELoadoutV2Mode::WindowSlots;
	Selected = FMath::Min(SlotIndex + 1, Slots.Num() - 1);
	Items.Reset();
	bComparing = false;
	RefreshView();
}

bool ULoadoutV2ScreenWidget::PutPieceInDraft(ELoadoutV2Slot GearSlot, const FLoadoutV2WindowItem &Item)
{
	switch (GearSlot)
	{
	case ELoadoutV2Slot::MainHand:
	{
		const FText OffHandName = LoadoutV2ScreenStyle::WeaponName(Draft.OffHand);
		if (WoRLoadoutV2::SetMainHand(Draft, Item.Weapon))
		{
			Message = FText::Format(LOCTEXT("OffHandCameOff", "{0} came off the off hand: it doesn't pair with {1}."), OffHandName, LoadoutV2ScreenStyle::WeaponName(Item.Weapon));
		}
		return true;
	}
	case ELoadoutV2Slot::OffHand:
		Draft.OffHand = Item.Weapon;
		return true;
	case ELoadoutV2Slot::Adapter:
		Draft.Adapter = Item.Adapter;
		Draft.Socket1 = Item.Adapter.AttachedItem;
		Draft.Socket2 = Item.Adapter.StoneSocket;
		return true;
	case ELoadoutV2Slot::Evolution:
		Draft.Socket1 = WoRLoadoutV2::MakeEvolutionAttachment(Item.Evolution);
		Draft.EvolutionInstance = Item.Evolution.InstanceID;
		return true;
	case ELoadoutV2Slot::Socket1:
	case ELoadoutV2Slot::Socket2:
	default:
		return false;
	}
}

// ==================== THE INVENTORY TAB (B-531, D-230) ====================

void ULoadoutV2ScreenWidget::RefreshInventory(const UInventoryComponent &Inventory, const FCombatLoadout &Active)
{
	const UEvolutionInventoryComponent *EvolutionInventory = Inventory.GetOwner() ? Inventory.GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
	const TArray<FEvolutionInventoryEntry> NoEvolutions;
	const TArray<FEvolutionInventoryEntry> &Evolutions = EvolutionInventory ? EvolutionInventory->Entries : NoEvolutions;
	// Worn: the active loadout's own evolution, or one sitting in a piece's socket (another saved loadout's seat isn't worn).
	TSet<FGuid> InUse{Active.PrimaryEvolutionInstance};
	const auto AddSocketed = [&InUse](const FRuntimeAttachedItem &Socket)
	{
		if (Socket.IsEvolution())
		{
			InUse.Add(Socket.Evolution.InstanceID);
		}
	};
	for (const FWeaponInventoryEntry &Owned : Inventory.Weapons)
	{
		AddSocketed(Owned.AttachedItem);
	}
	for (const FRingInventoryEntry &Owned : Inventory.Rings)
	{
		AddSocketed(Owned.AttachedItem);
	}
	// The pouch the member carries (a recruit carries the host's, B-361), as the socket lists read it.
	const UGameInstance *GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UPoolSubsystem *Pool = GameInstance ? GameInstance->GetSubsystem<UPoolSubsystem>() : nullptr;
	const TMap<FCrystalId, int32> NoPouch;
	const TMap<FCrystalId, int32> &Pouch = Pool ? Pool->GetCarriedItems(WoRGive::GetPouchKey(GetWorld(), Members[MemberIndex].Key)) : NoPouch;
	InventoryPage = WoRLoadoutV2::BuildInventoryPage(Active, Inventory.Weapons, Inventory.Rings, Evolutions, InUse, Pouch);
}

void ULoadoutV2ScreenWidget::WearFromInventory()
{
	const FLoadoutV2InventoryRow *Row = GetInventoryRow(Selected);
	if (!Row || !Row->IsPiece())
	{
		return;
	}
	if (Row->Kind == ELoadoutV2InventoryRowKind::Crystal)
	{
		Message = LOCTEXT("InventoryCrystal", "A crystal goes in a socket: open a piece's socket to put it in.");
		RefreshView();
		return;
	}
	const ELoadoutV2DraftKind Kind = Row->Kind == ELoadoutV2InventoryRowKind::Weapon	? ELoadoutV2DraftKind::Weapon
								   : Row->Kind == ELoadoutV2InventoryRowKind::Adapter ? ELoadoutV2DraftKind::Adapter
																					  : ELoadoutV2DraftKind::Evolution;
	FLoadoutV2WindowItem Piece; // the line's first piece (a stack's), as the window's list would pick it
	Piece.Weapon = Row->Weapon;
	Piece.Adapter = Row->Adapter;
	Piece.Evolution = Row->Evolution;
	const FText PieceName = Row->Name;
	OpenWindowForNew();
	const FLoadoutV2KindChoice *Choice = Kinds.FindByPredicate([Kind](const FLoadoutV2KindChoice &Each) { return Each.Kind == Kind; });
	if (Mode != ELoadoutV2Mode::WindowKind || !Choice || !Choice->Refusal.IsEmpty())
	{
		const FText Refusal = Choice ? Choice->Refusal : LOCTEXT("InventoryNoPlace", "There's no place for it.");
		CloseWindow();
		Message = Refusal;
		RefreshView();
		return;
	}
	StartDraft(Kind);
	if (Slots.IsValidIndex(0) && PutPieceInDraft(Slots[0], Piece))
	{
		Selected = FMath::Min(1, Slots.Num() - 1); // as a pick: on to the next slot, the draft in the box
		UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: Inventory tab, %s opened in Add equipment"), *Members[MemberIndex].Name.ToString(), *PieceName.ToString());
	}
	else
	{
		Message = FText::Format(LOCTEXT("InventoryPickIt", "Pick {0} in the first slot."), PieceName);
		UE_LOG(LogWoRUI, Warning, TEXT("[LoadoutV2] Inventory tab: %s couldn't be put in the window's first slot"), *PieceName.ToString());
	}
	RefreshView();
}

void ULoadoutV2ScreenWidget::RemoveEquipped()
{
	if (!Slots.IsValidIndex(SlotIndex) || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	const ELoadoutV2Slot GearSlot = Slots[SlotIndex];
	if (GearSlot == ELoadoutV2Slot::OffHand)
	{
		Draft.OffHand = FWeaponInventoryEntry(); // goes on Y Confirm (a twin-only main hand refuses then)
		Mode = ELoadoutV2Mode::WindowSlots;
		Selected = SlotIndex;
		Items.Reset();
		RefreshView();
		return;
	}
	if (GearSlot == ELoadoutV2Slot::Socket1 || GearSlot == ELoadoutV2Slot::Socket2)
	{
		const int32 Socket = GearSlot == ELoadoutV2Slot::Socket1 ? LoadoutScreenSockets::CRYSTAL : LoadoutScreenSockets::STONE;
		ULoadoutComponent *Loadout = nullptr;
		UInventoryComponent *Inventory = nullptr;
		const FCombatLoadout *Active = nullptr;
		FLoadoutScreenRow Row;
		FCombatLoadout Preview;
		const bool bNewEntry = !FindScreenRow(Row);
		if (!FindActive(Loadout, Inventory, Active) ||
			(bNewEntry && !LoadoutV2DraftSocket::FindDraftRow(*Active, Members[MemberIndex].Class, Draft, bEditing ? &EditEntry : nullptr, Preview, Row).IsEmpty()))
		{
			return;
		}
		const bool bRestore = Items.IsValidIndex(Selected) && Items[Selected].bRestore; // B-534: the line says what it spends
		if (!bRestore && !bRemoveArmed)
		{
			bRemoveArmed = true; // says what comes off first (Q-440c)
			FText Refusal;
			const FText Warning = LoadoutV2ScreenStyle::SocketRemoveWarning(Row, Socket, Refusal);
			Message = Warning.IsEmpty() ? Refusal : Warning;
			bRemoveArmed = !Warning.IsEmpty();
			RefreshData();
			return;
		}
		bRemoveArmed = false;
		Message = bRestore ? WoRLoadoutScreen::RestoreSocket(GetWorld(), Members[MemberIndex], Row)
						   : WoRLoadoutScreen::TakeOffSocket(GetWorld(), Members[MemberIndex], Row, Socket);
		if (Message.IsEmpty())
		{
			UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: socket %d of %s %s"), *Members[MemberIndex].Name.ToString(), Socket + 1,
				   bNewEntry ? *Row.Name.ToString() : *EditEntry.Name.ToString(), bRestore ? TEXT("restored") : TEXT("emptied"));
			// The piece changed: read its sockets again.
			if (bNewEntry)
			{
				LoadoutV2DraftSocket::RereadPiece(*Inventory, Draft);
			}
			else
			{
				Draft = WoRLoadoutV2::DraftFromEntry(Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex], EditEntry);
			}
			Selected = bRestore ? 0 : Selected; // the Restore line goes once full: a second A mustn't land on a choice
		}
		RefreshData();
		return;
	}
	// The main piece (a weapon, an adapter, the evolution): the whole entry comes out.
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	const ECharacterClass Class = Members[MemberIndex].Class;
	const FLoadoutV2Entry Entry = EditEntry;
	FText Refusal;
	FCombatLoadout Probe = *Active;
	if (!WoRLoadoutV2::RemoveEntry(Probe, Class, Entry, Refusal))
	{
		Message = Refusal;
		RefreshView();
		return;
	}
	bool bRemoved = false;
	if (!Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [Class, &Entry, &bRemoved](FCombatLoadout &Changed)
								{
									FText Ignored;
									bRemoved = WoRLoadoutV2::RemoveEntry(Changed, Class, Entry, Ignored);
								},
								Refusal))
	{
		Message = Refusal;
		RefreshView();
		return;
	}
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: %s taken out (%s)"), *Members[MemberIndex].Name.ToString(), *Entry.Name.ToString(), bRemoved ? TEXT("done") : TEXT("nothing"));
	Message = FText::Format(LOCTEXT("EntryRemoved", "{0} came out."), Entry.Name);
	CloseWindow();
}

void ULoadoutV2ScreenWidget::ConfirmWindow()
{
	if (!Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	// A socket line picked: it goes into the owned piece now (today's socket rule: the pouch pays, B-440c).
	if (Mode == ELoadoutV2Mode::WindowItems && Items.IsValidIndex(PickedSocket) && Slots.IsValidIndex(SlotIndex))
	{
		const int32 Socket = Slots[SlotIndex] == ELoadoutV2Slot::Socket1 ? LoadoutScreenSockets::CRYSTAL : LoadoutScreenSockets::STONE;
		ULoadoutComponent *Loadout = nullptr;
		UInventoryComponent *Inventory = nullptr;
		const FCombatLoadout *Active = nullptr;
		FLoadoutScreenRow Row;
		FCombatLoadout Preview;
		const bool bNewEntry = !FindScreenRow(Row);
		if (!FindActive(Loadout, Inventory, Active) ||
			(bNewEntry && !LoadoutV2DraftSocket::FindDraftRow(*Active, Members[MemberIndex].Class, Draft, bEditing ? &EditEntry : nullptr, Preview, Row).IsEmpty()))
		{
			return;
		}
		Message = WoRLoadoutScreen::ApplySocketChoice(GetWorld(), Members[MemberIndex], Row, Socket, Items[PickedSocket].Socket);
		if (Message.IsEmpty())
		{
			// B-516: a new entry's piece is socketed as it waits in the window; the entry goes in on Y Confirm.
			UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: %s socketed into %s"), *Members[MemberIndex].Name.ToString(), *Items[PickedSocket].Name.ToString(),
				   bNewEntry ? *Row.Name.ToString() : *EditEntry.Name.ToString());
			Active = &Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			if (bNewEntry)
			{
				LoadoutV2DraftSocket::RereadPiece(*Inventory, Draft);
			}
			else
			{
				Draft = WoRLoadoutV2::DraftFromEntry(*Active, EditEntry);
			}
			Mode = ELoadoutV2Mode::WindowSlots;
			Selected = SlotIndex;
			Items.Reset();
			PickedSocket = INDEX_NONE;
		}
		RefreshData();
		return;
	}
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	const ECharacterClass Class = Members[MemberIndex].Class;
	int32 CellsAfter = 0;
	const FText Refusal = bEditing ? WoRLoadoutV2::CheckEntryEdit(*Active, Class, EditEntry, Draft, CellsAfter) : WoRLoadoutV2::CheckDraft(*Active, Class, Draft, CellsAfter);
	if (!Refusal.IsEmpty())
	{
		Message = Refusal;
		RefreshView();
		return;
	}
	const FLoadoutV2Draft Before = bEditing ? WoRLoadoutV2::DraftFromEntry(*Active, EditEntry) : FLoadoutV2Draft();
	const bool bNewEvolution = Draft.Kind == ELoadoutV2DraftKind::Evolution && (!bEditing || Before.EvolutionInstance != Draft.EvolutionInstance);
	if (bNewEvolution)
	{
		// UIScreens: attaching the evolution asks first.
		const FText EvolutionName = Draft.Socket1.Evolution.Item ? FText::FromString(Draft.Socket1.Evolution.Item->ItemName) : FText::GetEmpty();
		Message = FText::Format(LOCTEXT("AskEvolution", "Attach {0} to {1}? Uses {2} cells: {3} left → {4} left."), EvolutionName, Members[MemberIndex].Name,
								FText::AsNumber(LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST), FText::AsNumber(FMath::Max(0, Page.CellsBudget - Page.CellsUsed)),
								FText::AsNumber(FMath::Max(0, Page.CellsBudget - CellsAfter)));
		Mode = ELoadoutV2Mode::WindowAsk;
		RefreshView();
		return;
	}
	ApplyWindowDraft();
}

void ULoadoutV2ScreenWidget::ApplyWindowDraft()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!FindActive(Loadout, Inventory, Active))
	{
		Message = LOCTEXT("MemberNotHere", "This character isn't here.");
		Mode = ELoadoutV2Mode::List;
		RefreshData();
		return;
	}
	const ECharacterClass Class = Members[MemberIndex].Class;
	const FLoadoutV2Draft Applied = Draft;
	const FLoadoutV2Entry Entry = EditEntry;
	const bool bEdit = bEditing;
	bool bApplied = false;
	FText ApplyRefusal;
	FText Refusal;
	const bool bDone = Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [&Applied, &Entry, bEdit, Class, &bApplied, &ApplyRefusal](FCombatLoadout &Changed)
											  {
												  bApplied = bEdit ? WoRLoadoutV2::ReplaceEntry(Changed, Class, Entry, Applied, ApplyRefusal)
																   : WoRLoadoutV2::ApplyDraft(Changed, Class, Applied, ApplyRefusal);
											  },
											  Refusal);
	if (bDone && !bApplied)
	{
		Message = ApplyRefusal; // the loadout changed since the window read it: nothing went in
		Mode = ELoadoutV2Mode::WindowSlots;
		Selected = SlotIndex;
		RefreshData();
		return;
	}
	if (!bDone)
	{
		Message = Refusal;
		Mode = ELoadoutV2Mode::WindowSlots;
		Selected = SlotIndex;
		RefreshView();
		return;
	}
	const UObject *Piece = Applied.Socket1.Evolution.Item;
	if (Applied.Kind == ELoadoutV2DraftKind::Weapon)
	{
		Piece = Applied.MainHand.Weapon;
	}
	else if (Applied.Kind == ELoadoutV2DraftKind::Adapter)
	{
		Piece = Applied.Adapter.Ring;
	}
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: entry %s (%s)"), *Members[MemberIndex].Name.ToString(), bEdit ? TEXT("changed") : TEXT("added"), *GetNameSafe(Piece));
	Message = FText::GetEmpty();
	CloseWindow();
}

void ULoadoutV2ScreenWidget::CloseWindow()
{
	Mode = ELoadoutV2Mode::List;
	bEditing = false;
	Kinds.Reset();
	Slots.Reset();
	Items.Reset();
	PickedSocket = INDEX_NONE;
	bComparing = false;
	bRemoveArmed = false;
	Selected = ListSelected;
	RefreshData();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GLoadoutV2OpenCommand(
	TEXT("wor.UI.LoadoutV2"),
	TEXT("B-476b/c: open the loadout screen, as the game menu's Loadout page does: A Edit / Equip, X Simple / Full, L3 / 6 Set active, Y Options; "
		 "in the Add equipment window A Pick / Remove, X Compare, Y Confirm."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!ULoadoutV2ScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[LoadoutV2] wor.UI.LoadoutV2: not opened (no hub controller with a root layout)"));
		}
	}));

static FAutoConsoleCommandWithWorld GLoadoutApparelPrintCommand(
	TEXT("wor.UI.LoadoutApparel.Print"),
	TEXT("B-440f: print each party member's owned apparel and what the active loadout wears (the v2 loadout screen's Apparel tab)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		for (const FLoadoutScreenMember &Member : WoRLoadoutScreen::BuildMembers(World))
		{
			UE_LOG(LogWoRUI, Display, TEXT("[LoadoutApparel] %s"), *WoRLoadoutApparel::GetMemberApparelString(World, Member));
		}
	}));

static FAutoConsoleCommandWithWorld GLoadoutSavedPrintCommand(
	TEXT("wor.UI.LoadoutSaved.Print"),
	TEXT("B-440e: print each party member's saved loadouts as the v2 loadout screen's Options > Saved loadouts list shows them."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		for (const FLoadoutScreenMember &Member : WoRLoadoutScreen::BuildMembers(World))
		{
			ULoadoutComponent *Loadout = nullptr;
			UInventoryComponent *Inventory = nullptr;
			if (WoRLoadoutSaved::FindComponents(World, Member, Loadout, Inventory))
			{
				UE_LOG(LogWoRUI, Display, TEXT("[LoadoutSaved] %s"), *WoRLoadoutSaved::GetSavedListString(*Inventory));
			}
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
