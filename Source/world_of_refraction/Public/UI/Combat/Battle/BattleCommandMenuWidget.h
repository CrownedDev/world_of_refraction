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

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Equipment/Crystals/FBrokenCrystalPayload.h" // UFUNCTION handler parameter type
#include "UI/Combat/PieMenuButtonData.h"
#include "BattleCommandMenuWidget.generated.h"

class AActor;
class UButton;
class UTextBlock;
class UCombatCommandMenuSubsystem;
class UBattleCommandMenuWidget;
class UCrystalManager;

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

UCLASS()
class WORLD_OF_REFRACTION_API UBattleCommandMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Rows built once: the largest view (the main menu, up to 13 buttons) plus room. */
	static constexpr int32 MAX_ROWS = 16;

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

	/** Debug (wor.UI.Print): showing or not, the title and every row as shown. */
	FString GetCommandMenuString() const;

	bool IsShowing() const { return bShowing; }
	const FText &GetTitle() const { return Title; }
	const TArray<FText> &GetRowLabels() const { return RowLabels; }

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

	void PushToWidgets();

	TArray<FPieMenuButtonData> Rows;
	TArray<FText> RowLabels;
	FText Title;
	bool bShowing = false;
	bool bBound = false;

	TWeakObjectPtr<UCombatCommandMenuSubsystem> BoundMenu;
	TWeakObjectPtr<UCrystalManager> BoundCrystals;
	TArray<FText> DurabilityLines;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DurabilityText;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> RowButtons;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> RowTexts;

	UPROPERTY()
	TArray<TObjectPtr<UBattleMenuRowRelay>> RowRelays;
};
