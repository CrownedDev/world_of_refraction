// VendorScreenWidget.h
// B-456a to B-456c (D-193, D-181, UIScreens #15 to #18, mockups screen_mockups/hub/vendors/): the vendor screen. A
// CommonUI screen on the root layout's menu layer. Talking to a vendor opens it (UMerchantShopSubsystem); so does
// wor.UI.VendorScreen <vendor>. Replaces the old shop window (UShopWindowWidget) and the placeholder
// UVendorServiceWidget (wor.UI.Vendor), both kept until the PIE pass.
//
// Top: the vendor, the money, its service tabs (LB / RB) and the character tabs (LT / RT). Left: what the service
// takes from that character, one row each, a refused row greyed with its reason. Right: the focused row, what it
// gives and, after the first A, what is confirmed (D-016: ask first, the second A does it). X switches a stack
// between one and the whole stack.
// Buy (B-456b, mockup 1_shop_buy.png): the rows are the vendor's stock; the character tabs give way to "Buying for"
// (R / Y, or LT / RT). A (or Right) puts the focused line in the cart for that member, Left takes it back, X buys the
// cart. The cart is its own panel: each line with who it is for, the total and what is left after.
// Item slots (B-456c, mockup 2_jeweler_item_slots.png): a row per party member (no character tabs), the pouch's slots
// as pips and the next one's price; A asks, the second A buys one slot.
// The Spiritualist (B-456d, mockups 3_spiritualist_repair.png, 4_spiritualist_curses.png, 6_merge_crystals.png):
// Repair lists each worn piece of the party's gear (A asks, the second A repairs it; X asks to repair everything);
// Curses lists the player's curses (A asks, the second A clears it); Merge lists the member's crystal kinds (LT / RT),
// Left / Right picks the tier to make, A asks, the second A merges.
// Upgrade (B-616, mockup v2/3_upgrade.png): the member's weapons and adapters; Left / Right picks the tier to raise it
// to, A asks, the second A raises it step by step (each step paying its own cost).
// B-616: three columns (mockup screen_mockups/hub/vendors/v2): the rows left, the focused thing in the middle,
// Details right (the price, what changes, the question, Buy's cart).
// What it shows comes from WoRVendorScreen (VendorScreenModel.h), which the specs read.
// B-616b (Q-616b): on Buy, Sell, Dismantle and Upgrade the left column is the v2 tile grid (filters, five to a row,
// tier badge, count, the cart corner and the lock, the legend under it); Item slots, Repair, Curses and Merge keep rows.
// Not built: Merge's two columns (one list with the tier on Left / Right instead); the mockup's Y Sort.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/VendorScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "VendorScreenWidget.generated.h"

class AActor;
class APlayerController;
class UBorder;
class UHorizontalBox;
class UImage;
class UMerchantData;
class UScrollBox;
class UTextBlock;
class UUniformGridPanel;
class UWidget;

/** B-608: what a left click on a stock row does. */
enum class EVendorRowClick : uint8
{
	/** Only selects it (and shows its details). */
	Select,
	/** Buy: the row's "+": one more into the cart. */
	Add,
	/** Another service: a click on the row already selected confirms, as A. */
	Confirm
};

UCLASS()
class WORLD_OF_REFRACTION_API UVendorScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	UVendorScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout, showing Vendor. Merchant stocks the Buy tab (null: the vendor of
	 *  that kind in the level); PC's pawn pays. */
	static UVendorScreenWidget *Open(APlayerController *PC, EMerchantType Vendor, const UMerchantData *Merchant = nullptr);

	/** Shows Vendor's tabs, on its first built one, with nothing pending and an empty cart. InMerchant and InPayer
	 *  as Open's (null: the owning player's pawn). */
	void SetVendor(EMerchantType InVendor, const UMerchantData *InMerchant = nullptr, AActor *InPayer = nullptr);

	// What the keys and the mouse do (the specs call these too).
	void MoveRow(int32 Delta);
	void SelectRow(int32 Index, bool bScrollIntoView = true);
	/** The next / previous service or character; wraps, and drops a pending confirm. */
	void MoveTab(int32 Delta);
	void MoveMember(int32 Delta);
	/** First press: asks (a refused row says its reason instead). Second press: runs the service, or buys the slot. */
	void Confirm();
	void ToggleWholeStack();
	/** Buy: the focused stock line into the cart for the member bought for (a refused line says why), or one press's
	 *  worth of it back out. */
	void AddFocusedToCart();
	void RemoveFocusedFromCart();
	/** Buy: buys the cart. */
	void Purchase();
	/** Repair (B-456d): the first press asks to repair everything (or says why it can't), the second does it. */
	void PressRepairAll();
	/** Merge (B-456d): the focused crystal's tier to make, one up (Delta 1) or down, E to S, wrapping. */
	void MoveMergeTarget(int32 Delta);
	/** Upgrade (B-616): the focused piece's target tier, one up (Delta 1) or down, from the next tier to S, wrapping. */
	void MoveUpgradeTarget(int32 Delta);
	/** Cancels a pending confirm, then closes. */
	void PressBack() { HandleBackAction(); }
	/** B-608: a left click on row Index (bOnAddButton: on its "+"), as GetRowClick says. */
	void ClickRow(int32 Index, bool bOnAddButton);
	/** B-608: a click on a Buy row only selects it; its "+" adds. On another service, a click on the selected row
	 *  confirms (the first selects). */
	static EVendorRowClick GetRowClick(bool bBuy, bool bOnAddButton, bool bAlreadySelected);

	// B-616b (mockup hub/vendors/v2, Q-616b): on Buy, Sell, Dismantle and Upgrade the left column is a grid of tiles,
	// five to a row, under filter tabs (All, then each kind of thing shown). The arrows move tile to tile; Up off the
	// top row reaches the filters (Left / Right turn them, Down or A back). A click on a tile lights it, on the lit one
	// is A (Buy: adds one).
	bool IsTileGrid() const { return WoRVendorScreen::HasTileGrid(GetTab()); }
	void MoveInGrid(int32 DeltaX, int32 DeltaY);
	/** The filter tab Index (0 is All): only its rows are tiles; the first of them is lit. */
	void PickFilter(int32 Index);
	bool IsOnFilters() const { return bOnFilters && IsTileGrid(); }
	const TArray<FVendorGridFilter> &GetFilters() const { return Filters; }
	int32 GetFilterIndex() const { return FilterIndex; }
	/** The tiles in order: each one's row in GetPage().Rows. */
	const TArray<int32> &GetShownRows() const { return ShownRows; }
	/** Buy (Q-616b): Y / R takes one of the lit line back out of the cart (Left moves now). Upgrade: Y / R aims one tier
	 *  further (wrapping). Elsewhere nothing. */
	void PressSecond();

	EMerchantType GetVendor() const { return Vendor; }
	EVendorScreenTab GetTab() const { return Tabs.IsValidIndex(TabIndex) ? Tabs[TabIndex] : EVendorScreenTab::Buy; }
	int32 GetMemberIndex() const { return MemberIndex; }
	int32 GetSelectedRow() const { return SelectedRow; }
	bool IsConfirming() const { return bConfirming; }
	bool IsConfirmingAll() const { return bConfirmingAll; }
	bool IsWholeStack() const { return bWholeStack; }
	const FVendorScreenPage &GetPage() const { return Page; }
	const TArray<FVendorCartLine> &GetCart() const { return Cart; }
	const FText &GetStatus() const { return Status; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	/** One row's widgets. Rows are made as a page first needs them and refilled after (never rebuilt). */
	struct FRowWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UBorder> ElementMark;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> Sub;
		/** Item slots: one mark per slot the pouch can have, lit for each it has. */
		TWeakObjectPtr<UWidget> PipRow;
		TArray<TWeakObjectPtr<UBorder>> Pips;
		TWeakObjectPtr<UTextBlock> Tier;
		TWeakObjectPtr<UTextBlock> Count;
		/** B-487: the row's icon before its name, and its tier badge in the tier letter's place. */
		TWeakObjectPtr<UImage> Icon;
		TWeakObjectPtr<UImage> TierBadge;
		/** B-608, Buy: the "+" a click adds with. */
		TWeakObjectPtr<UBorder> AddButton;
	};

	/** B-616b: one tile's widgets, made as a page first needs them and refilled after. */
	struct FTileWidgets
	{
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UImage> Icon;
		TWeakObjectPtr<UImage> IconCentre;
		/** The icon's name where the icon table has no picture. */
		TWeakObjectPtr<UTextBlock> Word;
		TWeakObjectPtr<UImage> TierBadge;
		/** The tier in letters where the row has no badge ("F", Upgrade's "F → E"). */
		TWeakObjectPtr<UTextBlock> TierText;
		TWeakObjectPtr<UTextBlock> Count;
		/** Buy: the violet corner of a line in the cart. */
		TWeakObjectPtr<UWidget> InCart;
		TWeakObjectPtr<UWidget> Lock;
	};

	void BuildTree();
	FRowWidgets BuildRow();
	FTileWidgets BuildTile(int32 Index);
	/** B-616b: the filters and the tiles from Page (the rows' column is hidden on a grid tab). */
	void RefreshGrid();
	/** B-616b: ShownRows from Page and the filter; the lit row kept when it is still shown, else the first. */
	void UpdateShownRows();
	int32 FindTileUnder(const FPointerEvent &MouseEvent) const;
	/** Back to the vendor's first built tab and first row, nothing pending. */
	void ResetState();
	/** The members and the shown tab, read fresh from the model; the heading, the tabs and the rows. */
	void RefreshData();
	/** The focused row's colour and the detail panel. */
	void RefreshSelection(bool bScrollIntoView = true);
	/** The cart's lines, total and whether it can be bought. */
	void RefreshCart();
	bool IsBuy() const { return GetTab() == EVendorScreenTab::Buy; }
	bool IsItemSlots() const { return GetTab() == EVendorScreenTab::ItemSlots; }
	/** Repair, Curses, Merge (B-456d), Upgrade (B-616) and the skills' Upgrade (B-616c): the WoRVendorScreen::RunService tabs. */
	bool IsSpiritualistService() const
	{
		return GetTab() == EVendorScreenTab::Repair || GetTab() == EVendorScreenTab::Curses || GetTab() == EVendorScreenTab::Merge ||
			   GetTab() == EVendorScreenTab::Upgrade || GetTab() == EVendorScreenTab::SkillUpgrade;
	}
	/** Sell, Dismantle, Merge and Upgrade act on one character's things; Buy says who it is for in the panel, Item slots
	 *  and Repair in their rows, and curses are the player's. */
	bool HasMemberTabs() const
	{
		return GetTab() == EVendorScreenTab::Sell || GetTab() == EVendorScreenTab::Dismantle || GetTab() == EVendorScreenTab::Merge ||
			   GetTab() == EVendorScreenTab::Upgrade || GetTab() == EVendorScreenTab::SkillUpgrade;
	}
	/** Repair: X asks to repair everything, the second X (or A) does it. */
	void DoRepairAll();
	FName MemberKey() const;
	const FVendorScreenRow *FocusedRow() const;
	int32 FindRowUnder(const FPointerEvent &MouseEvent) const;

	EMerchantType Vendor = EMerchantType::Blacksmith;
	TArray<EVendorScreenTab> Tabs;
	TArray<FLoadoutScreenMember> Members;
	FVendorScreenPage Page;
	int32 TabIndex = 0;
	int32 MemberIndex = 0;
	int32 SelectedRow = 0;
	bool bConfirming = false;
	/** Repair: "Repair everything?" is asked. */
	bool bConfirmingAll = false;
	bool bWholeStack = false;
	/** Merge: the tier picked for each crystal kind (a kind not in it aims at the highest it can make). */
	TMap<ECrystalType, EItemTier> MergeTargets;
	/** Upgrade: the tier picked for each piece (a piece not in it aims at its next tier). */
	TMap<FGuid, EItemTier> UpgradeTargets;
	FText Status;
	bool bStatusIsRefusal = false;
	// B-616b: the grid's filter and the rows it shows.
	TArray<FVendorGridFilter> Filters;
	int32 FilterIndex = 0;
	bool bOnFilters = false;
	TArray<int32> ShownRows;
	/** Buy: whose stock (held, so the stock lines the page and the cart point at stay loaded), whose wallet, and
	 *  what is picked so far. */
	UPROPERTY(Transient)
	TObjectPtr<const UMerchantData> Merchant;
	TWeakObjectPtr<AActor> Payer;
	TArray<FVendorCartLine> Cart;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> VendorLineText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ChipsText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ServiceTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ServiceTabLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> MemberTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> MemberTabLabels;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EmptyText;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RowScroll;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> DetailPanel;
	/** B-608: the details scroll in what the cart leaves. */
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> DetailScroll;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailTitle;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> MemberTabRow;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PayLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailPay;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailLines;
	/** B-616: the middle's kind line, and Details' "what changes" lines. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailKind;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailChanges;
	// B-487: the thing's effect sentence beside its effect icon (shown when the icon table has that icon).
	UPROPERTY(Transient)
	TObjectPtr<UWidget> DetailEffectRow;
	UPROPERTY(Transient)
	TObjectPtr<UImage> DetailEffectIcon;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailEffectText;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> BuyForButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BuyForText;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> CartPanel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CartLinesText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CartTotalText;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PurchaseButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PurchaseText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmHint;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RuleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WholeStackText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintsText;

	TArray<FRowWidgets> RowWidgets;

	// B-616b: the grid's panel (in the rows' place), its heading, filters, tiles and legend.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> GridColumnWidget;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GridTitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GridCountText;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> FilterRow;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> FilterTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> FilterIcons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> FilterLabels;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> GridScroll;
	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> GridPanel;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> LegendCart;
	TArray<FTileWidgets> TileWidgets;
};
