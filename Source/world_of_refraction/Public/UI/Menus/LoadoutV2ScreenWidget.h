// LoadoutV2ScreenWidget.h
// B-476b (D-206, D-209, D-210; UIScreens.md #2, mockups menus/loadout/v2), first part: the v2 loadout screen's Weapons
// tab, built beside today's ULoadoutScreenWidget (which stays until Crown's PIE pass). LT / RT the character; the
// list of entries (names only, the two sockets, ACTIVE on the weapon in hand) with the Add weapon frame at the bottom
// ("1/5 left"); "Cells used 4 of 5" at the top. A on the frame opens the Add gear window's Main hand list
// (WoRLoadoutV2::BuildMainHandChoices: refused pieces stay listed, greyed, with the reason); A picks, X Compare sets
// the equipped weapon beside the lit one, Y Confirm puts the entry in (ULoadoutComponent::TryChangeGear), B Cancel.
// Not yet (B-476c onward): editing an entry, the Off hand and socket rows, the Apparel and Skills tabs, Y Options,
// X Details. Opened by wor.UI.LoadoutV2.

#pragma once

#include "CoreMinimal.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/LoadoutV2Model.h"
#include "UI/WoRActivatableWidget.h"
#include "LoadoutV2ScreenWidget.generated.h"

class APlayerController;
class UBorder;
class UTextBlock;
class UVerticalBox;

UCLASS()
class WORLD_OF_REFRACTION_API ULoadoutV2ScreenWidget : public UWoRActivatableWidget
{
	GENERATED_BODY()

public:
	ULoadoutV2ScreenWidget(const FObjectInitializer &ObjectInitializer);

	/** Pushes the screen onto PC's root layout; null without one. */
	static ULoadoutV2ScreenWidget *Open(APlayerController *PC);

	// What the keys do (the specs call these too).
	void MoveSelection(int32 Delta);
	void MoveMember(int32 Delta);
	/** A: on the Add weapon frame, opens the Main hand list; in it, picks the lit piece. */
	void PressA();
	/** X: in the window, Compare the equipped weapon with the lit one (again: hide it). */
	void PressCompare();
	/** Y: in the window, Confirm the picked piece as a new entry. */
	void PressConfirm();
	/** B, Esc: closes the window, else the screen. */
	void PressBack() { HandleBackAction(); }

	const FLoadoutV2WeaponsPage &GetPage() const { return Page; }
	bool IsAdding() const { return bAdding; }
	const TArray<FLoadoutV2MainHandChoice> &GetChoices() const { return Choices; }
	int32 GetSelected() const { return Selected; }
	int32 GetPicked() const { return Picked; }
	const FText &GetMessage() const { return Message; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget *NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent) override;
	virtual bool NativeOnHandleBackAction() override;

private:
	/** The members and the shown member's page (and the window's list while adding), read fresh. */
	void RefreshData();
	void RefreshView();
	/** The list's lines: the entries then the frame, or the window's pieces. */
	int32 GetLineCount() const;
	FText GetLineName(int32 Line) const;
	FText GetLineDetail(int32 Line) const;
	bool IsLineRefused(int32 Line) const;
	void CloseWindow();

	TArray<FLoadoutScreenMember> Members;
	int32 MemberIndex = 0;
	FLoadoutV2WeaponsPage Page;
	int32 Selected = 0;
	bool bAdding = false;
	TArray<FLoadoutV2MainHandChoice> Choices;
	/** The window's picked piece (INDEX_NONE: none yet). */
	int32 Picked = INDEX_NONE;
	bool bComparing = false;
	FText Message;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MemberText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CellsText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CompareText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintsText;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ListBox;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> LineRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LineNames;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LineDetails;
};
