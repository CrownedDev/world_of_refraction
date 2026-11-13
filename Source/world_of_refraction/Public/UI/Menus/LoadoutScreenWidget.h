// LoadoutScreenWidget.h
// B-440a (D-181, D-180, UIScreens #2, mockups screen_mockups/menus/loadout/): the loadout screen, gear page,
// read only. A CommonUI screen on the root layout's menu layer, opened from the game menu's Loadout page (Back
// returns there) or wor.UI.Loadout.
//
// Top left: the character tabs (LT / RT), the page tabs (Gear; Skills comes with B-440d) and the budget pips.
// Top right: the saved loadout's name. Left: one row per weapon, adapter, evolution seat and apparel piece, with
// its sockets and cost. Right: the focused row's details and its cost sum.
// What it shows comes from WoRLoadoutScreen (LoadoutScreenModel.h), which the specs read.
// Changing anything (gear, sockets, skills, saved loadouts) is B-440b to B-440e.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/WoRActivatableWidget.h"
#include "LoadoutScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UScrollBox;
class UTextBlock;
class UWidget;

UCLASS()
class WORLD_OF_REFRACTION_API ULoadoutScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	ULoadoutScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout (over the game menu, when that is what opened it). */
	static ULoadoutScreenWidget *Open(APlayerController *PC);

	// What the keys and the mouse do (the specs call these too).
	void MoveSelection(int32 Delta);
	void SelectRow(int32 Index, bool bScrollIntoView = true);
	/** The next / previous character; wraps. */
	void MoveMember(int32 Delta);
	void PressBack() { HandleBackAction(); }

	int32 GetSelectedRow() const { return SelectedRow; }
	int32 GetMemberIndex() const { return MemberIndex; }
	const FLoadoutScreenPage &GetPage() const { return Page; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent) override;

private:
	/** One gear row's widgets. Rows are made as a page first needs them and refilled after (never rebuilt). */
	struct FRowWidgets
	{
		TWeakObjectPtr<UWidget> Root;
		TWeakObjectPtr<UTextBlock> Heading;
		TWeakObjectPtr<UBorder> Frame;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UWidget> ActiveChip;
		TWeakObjectPtr<UTextBlock> KindLine;
		TArray<TWeakObjectPtr<UWidget>> SocketBoxes;
		TArray<TWeakObjectPtr<UTextBlock>> SocketNames;
		TArray<TWeakObjectPtr<UTextBlock>> SocketLabels;
		TWeakObjectPtr<UTextBlock> Cost;
	};

	void BuildTree();
	FRowWidgets BuildRow();
	/** The members and the shown member's page, read fresh from the model. */
	void RefreshData();
	/** The focused row's colour and the detail panel. */
	void RefreshSelection(bool bScrollIntoView = true);
	void FillRow(const FRowWidgets &Widgets, const FLoadoutScreenRow &Row, bool bHeading) const;
	int32 FindRowUnder(const FPointerEvent &MouseEvent) const;
	int32 FindMemberTabUnder(const FPointerEvent &MouseEvent) const;

	TArray<FLoadoutScreenMember> Members;
	FLoadoutScreenPage Page;
	int32 MemberIndex = 0;
	int32 SelectedRow = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> MemberTabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> MemberTabLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> BudgetPips;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BudgetText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LoadoutTitleText;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RowList;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RefusalText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailKindLine;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DetailLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DetailValues;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailCostLine;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailNote;

	TArray<FRowWidgets> RowWidgets;
};
