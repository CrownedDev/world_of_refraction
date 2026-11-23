// BattleCommandMenuWidget.h
// PLACEHOLDER UI (B-391, Q-391): the new battle HUD's command menu, plain C++ (no .uasset). A view of
// UCombatCommandMenuSubsystem, which keeps every rule: this widget lists the buttons the subsystem broadcasts
// (OnCommandMenuReady) with each one's key ("[1 / A] Attack", ACombatPlayerController::GetMenuKeys), and a click
// calls HandleSelection. The keys themselves are bound by the controller (B-093, B-391). CommonUI replaces it (D-024).
//
// Added straight to the viewport by UCombatHUD (wor.UI.NewHUD 1). A fixed pool of rows is built once in
// RebuildWidget; each broadcast only rewrites texts and shows / hides rows (TransBuffer rule). No Tick.
// B-391c: a durability header above the rows, as the old UDurabilityHeaderWidget: the acting character's ring ("AD")
// and weapon crystal ("WD") durability (BattleHUDView::ReadDurability), refreshed on each menu open and on
// UCrystalManager's durability and break events for that character.
//
// B-436g (D-258, D-260; refine_final/11_menu_frame.png, 12_menu_frame_submenus.png): one frame as wide as the full party
// card, stacked just above it by UBattleHUDWidget (SetCommandMenu). An "Act ●●○" tab on its top edge at every depth (the
// pips left the card; GetActionsLine), a header past the main menu (UCombatCommandMenuSubsystem::GetViewTitle), then big
// rows: the key of the device in use, an icon, the name, an item's "x2", the cost as pips (not on the main menu), and a
// greyed row's reason under its name. The main menu's rows are Attack, Skills and Items; its other buttons and Back
// elsewhere are clickable corner hints (UBattleMenuHintsWidget, bottom right). Infusion rows stay clickable rows at the
// foot of the target list. No "Your turn" title (D-260).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Equipment/Crystals/FBrokenCrystalPayload.h" // UFUNCTION handler parameter type
#include "InputCoreTypes.h"
#include "UI/Combat/PieMenuButtonData.h"
#include "BattleCommandMenuWidget.generated.h"

class AActor;
class UBorder;
class UButton;
class UImage;
class UTextBlock;
class UCombatCommandMenuSubsystem;
class UCommonInputSubsystem;
class UBattleCommandMenuWidget;
class UCrystalManager;
enum class ECommonInputType : uint8;

/** Routes one row button's click (UButton::OnClicked has no parameters) to its menu with the row's index. */
UCLASS()
class WORLD_OF_REFRACTION_API UBattleMenuRowRelay : public UObject
{
	GENERATED_BODY()

public:
	int32 RowIndex = INDEX_NONE;
	TWeakObjectPtr<UBattleCommandMenuWidget> Menu;

	UFUNCTION()
	void HandleClicked();

	/** B-400b: a hovered row highlights its target (the camera follows, B-400). */
	UFUNCTION()
	void HandleHovered();
};

/** B-436g (D-258): one corner hint, "R1  Defend": its key for the device in use (empty: none), its word, and the menu row
 *  a click presses. */
struct FBattleMenuHint
{
	FText Key;
	FKey KeyId; // drawn as the rows' cap (QA: the hints' keys must read as well as the rows')
	FText Label;
	FText Reason;
	int32 RowIndex = INDEX_NONE;
	bool bEnabled = true;
};

/** B-436g (D-258): the command menu's corner hints, bottom right, right-aligned: on the main menu every button but
 *  Attack, Skills and Items (Switch weapon, Change ring, the steps, End Turn, Flee, Defend); past it, Back. A click
 *  presses the menu's row. Its own widget so the HUD can put it in the other corner; made by the menu
 *  (UBattleCommandMenuWidget::GetHintsWidget), a fixed pool built once. */
UCLASS()
class WORLD_OF_REFRACTION_API UBattleMenuHintsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 MAX_HINTS = 10;

	/** Show these hints (none: collapsed). Kept, so a push before the widget is built shows once it is. */
	void SetHints(UBattleCommandMenuWidget *Menu, const TArray<FBattleMenuHint> &InHints);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void PushToWidgets();

	TArray<FBattleMenuHint> Hints;
	TWeakObjectPtr<UBattleCommandMenuWidget> Owner;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> Buttons;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> KeyCaps;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> KeyTexts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> LabelTexts;

	UPROPERTY()
	TArray<TObjectPtr<UBattleMenuRowRelay>> Relays;
};

UCLASS()
class WORLD_OF_REFRACTION_API UBattleCommandMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Rows built once: the largest view (the main menu, up to 13 buttons) plus room. */
	static constexpr int32 MAX_ROWS = 16;

	/** B-436g: the corner hints, made once on the first call (the HUD places it). */
	UBattleMenuHintsWidget *GetHintsWidget();

	/** B-436g (D-260): re-read the Act tab from the acting character (UBattleHUDWidget calls it on its pool events). */
	void RefreshActionsLine();

	/** B-436g (D-258): whether a row of this view is a corner hint, not a frame row: on the main menu anything but
	 *  Attack, Skills and Items; past it, Back. */
	static bool IsCornerHint(EPieMenuCategory Category, bool bAtMainMenu);

	/** Bind to the command menu subsystem; shows the menu at once if it is already open (turn 1 fires inside
	 *  InitializeCombat, before the HUD exists). Once per widget. */
	void Initialise();

	/** Unbind. Safe to call twice; NativeDestruct calls it too. */
	void Teardown();

	/** Press the row as a click would: its button goes to the subsystem's HandleSelection. Ignores a header, a
	 *  disabled row and an index past the shown rows. */
	void PressRow(int32 RowIndex);

	/** B-400b: the mouse is over the row. A per-actor target row in an open single-target picker highlights that
	 *  target (UCombatCommandMenuSubsystem::HighlightTarget), as hover or focus on the old pie menu's buttons does
	 *  (B-400); any other row does nothing. The rows aren't focusable (menu keys, B-391), so hover is the only hook. */
	void HoverRow(int32 RowIndex);

	/** B-400b (debug, specs): the actor a per-actor target row picks; null for any other row. */
	AActor *GetRowTarget(int32 RowIndex) const;

	/** B-582b (D-267): the lit row: the one W / S last moved to, else the highlighted target's row; INDEX_NONE when
	 *  none (the main menu until W / S is pressed). It draws violet, as the highlighted target's row did. */
	int32 GetLitRow() const;
	/** B-582b: W (Direction < 0) / S (> 0): light the previous / next frame row (not a header, a greyed row or a corner
	 *  hint), wrapping; the first press lights the first / last. A target row also highlights its target, as hover. */
	void MoveLitRow(int32 Direction);
	/** B-582b: Enter / Space: press the lit row as a click; with none lit, the picker's highlighted target. False: nothing. */
	bool PressLitRow();
	/** B-582b: A (Direction < 0) / D (> 0): the lit row's value (the infusion source or level), or in a target
	 *  picker with a target row lit, the previous / next target (as Q / E). */
	void ChangeLitRowValue(int32 Direction);

	/** Debug (wor.UI.Print): showing or not, the tab, the header, every row as shown and the corner hints. */
	FString GetCommandMenuString() const;

	bool IsShowing() const { return bShowing; }
	/** B-436g (D-260): the header ("Skills > Abilities", "Attack > choose a target"); empty on the main menu. */
	const FText &GetTitle() const { return Title; }
	/** Every row, frame row or corner hint, with its key for the device in use: "[G] Defend", "Attack". Debug, specs. */
	const TArray<FText> &GetRowLabels() const { return RowLabels; }
	/** B-436g (D-260): the tab, "Act: ●●○ · Skills 1/3" (ActionPipsText::MakeCardActionsLine). */
	const FText &GetActionsLine() const { return ActionsLine; }
	/** B-436g: the corner hints of this view, in row order. */
	const TArray<FBattleMenuHint> &GetHints() const { return Hints; }

	/** B-391c: the durability header's lines for the acting character ("AD 3 / 5", "WD 2 / 4"); empty for none. */
	const TArray<FText> &GetDurabilityLines() const { return DurabilityLines; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleMenuReady(const TArray<FPieMenuButtonData> &Buttons);

	UFUNCTION()
	void HandleMenuClosed();

	UFUNCTION()
	void HandleCrystalDurabilityChanged(AActor *Actor, UObject *Holder, int32 NewDurability, int32 MaxDurability);

	UFUNCTION()
	void HandleCrystalBroken(AActor *Actor, UObject *Holder, FBrokenCrystalPayload Payload);

	/** B-391c: re-read the header for the menu's current actor (empty when none). */
	void RefreshDurability();

	/** B-436g: each row's key for the device in use, RowLabels and the corner hints, from Rows. */
	void RefreshKeys();

	UFUNCTION()
	void HandleTargetHighlighted(AActor *Actor, AActor *Target, int32 TargetCount);

	/** B-436g: the last input moved between keyboard and gamepad: the keys shown follow it. */
	void HandleInputMethodChanged(ECommonInputType InputType);

	void PushToWidgets();

	/** B-582b: a row W / S can light. */
	bool IsNavigableRow(int32 RowIndex) const;

	TArray<FPieMenuButtonData> Rows;
	/** B-582b: the row W / S or a target highlight last lit; kept through a rebuild of the same view (an infusion cycle). */
	int32 LitRow = INDEX_NONE;
	TArray<FText> RowLabels;
	/** B-436g: each row's key for the device in use (invalid: none). */
	TArray<FKey> RowKeys;
	TArray<FBattleMenuHint> Hints;
	FText Title;
	FText ActionsLine;
	bool bAtMainMenu = false;
	bool bShowing = false;
	bool bBound = false;

	TWeakObjectPtr<UCombatCommandMenuSubsystem> BoundMenu;
	TWeakObjectPtr<UCrystalManager> BoundCrystals;
	TWeakObjectPtr<UCommonInputSubsystem> BoundInput;
	FDelegateHandle InputChangedHandle;
	TArray<FText> DurabilityLines;

	UPROPERTY()
	TObjectPtr<UBattleMenuHintsWidget> HintsWidget;

	UPROPERTY()
	TObjectPtr<UTextBlock> TabText;

	UPROPERTY()
	TObjectPtr<UWidget> HeaderBlock;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DurabilityText;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> RowButtons;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> RowKeyCaps;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> RowKeyTexts;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> RowIcons;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> RowTexts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> RowReasonTexts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> RowQuantityTexts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> RowCostTexts;

	UPROPERTY()
	TArray<TObjectPtr<UBattleMenuRowRelay>> RowRelays;
};
