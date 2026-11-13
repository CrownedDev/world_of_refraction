// VendorScreenWidget.h
// B-456a, B-456b (D-193, D-181, UIScreens #15 to #18, mockups screen_mockups/hub/vendors/): the vendor screen. A
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
// What it shows comes from WoRVendorScreen (VendorScreenModel.h), which the specs read.
// Not built yet: Item slots, Repair, Curses, Merge (listed, and say so); icons.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/VendorScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "VendorScreenWidget.generated.h"

class AActor;
class APlayerController;
class UBorder;
class UMerchantData;
class UScrollBox;
class UTextBlock;
class UWidget;

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
	/** First press: asks (a refused row says its reason instead). Second press: runs the service. */
	void Confirm();
	void ToggleWholeStack();
	/** Buy: the focused stock line into the cart for the member bought for (a refused line says why), or one press's
	 *  worth of it back out. */
	void AddFocusedToCart();
	void RemoveFocusedFromCart();
	/** Buy: buys the cart. */
	void Purchase();
	/** Cancels a pending confirm, then closes. */
	void PressBack() { HandleBackAction(); }

	EMerchantType GetVendor() const { return Vendor; }
	EVendorScreenTab GetTab() const { return Tabs.IsValidIndex(TabIndex) ? Tabs[TabIndex] : EVendorScreenTab::Buy; }
	int32 GetMemberIndex() const { return MemberIndex; }
	int32 GetSelectedRow() const { return SelectedRow; }
	bool IsConfirming() const { return bConfirming; }
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
		TWeakObjectPtr<UTextBlock> Tier;
		TWeakObjectPtr<UTextBlock> Count;
		TWeakObjectPtr<UTextBlock> Pay;
	};

	void BuildTree();
	FRowWidgets BuildRow();
	/** Back to the vendor's first built tab and first row, nothing pending. */
	void ResetState();
	/** The members and the shown tab, read fresh from the model; the heading, the tabs and the rows. */
	void RefreshData();
	/** The focused row's colour and the detail panel. */
	void RefreshSelection(bool bScrollIntoView = true);
	/** The cart's lines, total and whether it can be bought. */
	void RefreshCart();
	bool IsBuy() const { return GetTab() == EVendorScreenTab::Buy; }
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
	bool bWholeStack = false;
	FText Status;
	bool bStatusIsRefusal = false;
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
};
