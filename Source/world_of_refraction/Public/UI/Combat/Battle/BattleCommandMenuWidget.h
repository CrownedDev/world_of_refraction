// BattleCommandMenuWidget.h
// PLACEHOLDER UI (B-391, Q-391): the new battle HUD's command menu, plain C++ (no .uasset). A view of
// UCombatCommandMenuSubsystem, which keeps every rule: this widget lists the buttons the subsystem broadcasts
// (OnCommandMenuReady) with each one's key ("[1 / A] Attack", ACombatPlayerController::GetMenuKeys), and a click
// calls HandleSelection. The keys themselves are bound by the controller (B-093, B-391). CommonUI replaces it (D-024).
//
// Added straight to the viewport by UCombatHUD (wor.UI.NewHUD 1). A fixed pool of rows is built once in
// RebuildWidget; each broadcast only rewrites texts and shows / hides rows (TransBuffer rule). No Tick.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Combat/PieMenuButtonData.h"
#include "BattleCommandMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UCombatCommandMenuSubsystem;
class UBattleCommandMenuWidget;

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

	/** Debug (wor.UI.Print): showing or not, the title and every row as shown. */
	FString GetCommandMenuString() const;

	bool IsShowing() const { return bShowing; }
	const FText &GetTitle() const { return Title; }
	const TArray<FText> &GetRowLabels() const { return RowLabels; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleMenuReady(const TArray<FPieMenuButtonData> &Buttons);

	UFUNCTION()
	void HandleMenuClosed();

	void PushToWidgets();

	TArray<FPieMenuButtonData> Rows;
	TArray<FText> RowLabels;
	FText Title;
	bool bShowing = false;
	bool bBound = false;

	TWeakObjectPtr<UCombatCommandMenuSubsystem> BoundMenu;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> RowButtons;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> RowTexts;

	UPROPERTY()
	TArray<TObjectPtr<UBattleMenuRowRelay>> RowRelays;
};
