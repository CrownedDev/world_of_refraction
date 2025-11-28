// WBPCommandGrid.h
// 2-column grid with 4-directional navigation
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommandMenuTypes.h"
#include "WBPCommandGrid.generated.h"

class UUniformGridPanel;
class UWBPCommandButton;

/**
 * 2-column grid for spells, abilities, items, and rings
 * Supports 4-directional navigation (Up/Down/Left/Right)
 * 
 * Layout:
 * ┌───────┬───────┐
 * │   0   │   1   │  Row 0
 * ├───────┼───────┤
 * │   2   │   3   │  Row 1
 * ├───────┼───────┤
 * │   4   │   5   │  Row 2
 * └───────┴───────┘
 * 
 * Navigation:
 * - Up/Down moves by 2 (skips row)
 * - Left/Right moves by 1 (within row)
 * 
 * Blueprint should bind:
 * - GridPanel: UniformGridPanel (2 columns)
 */
UCLASS(Abstract, Blueprintable)
class WORLD_OF_REFRACTION_API UWBPCommandGrid : public UUserWidget
{
	GENERATED_BODY()

public:
	UWBPCommandGrid(const FObjectInitializer& ObjectInitializer);

	// ==================== POPULATION ====================

	/** Set buttons from array of data (max 6 recommended) */
	UFUNCTION(BlueprintCallable, Category = "Command Grid")
	void SetButtons(const TArray<FCommandButtonData>& InButtonData);

	/** Clear all buttons */
	UFUNCTION(BlueprintCallable, Category = "Command Grid")
	void ClearButtons();

	/** Get number of buttons */
	UFUNCTION(BlueprintPure, Category = "Command Grid")
	int32 GetButtonCount() const { return ButtonWidgets.Num(); }

	/** Get button data at index */
	UFUNCTION(BlueprintPure, Category = "Command Grid")
	bool GetButtonDataAtIndex(int32 Index, FCommandButtonData& OutData) const;

	/** Set the type of content in this grid */
	UFUNCTION(BlueprintCallable, Category = "Command Grid")
	void SetGridType(EGridType InType) { GridType = InType; }

	/** Get current grid type */
	UFUNCTION(BlueprintPure, Category = "Command Grid")
	EGridType GetGridType() const { return GridType; }

	// ==================== NAVIGATION ====================

	/** Move selection up (by 2 indices) */
	UFUNCTION(BlueprintCallable, Category = "Command Grid|Navigation")
	void NavigateUp();

	/** Move selection down (by 2 indices) */
	UFUNCTION(BlueprintCallable, Category = "Command Grid|Navigation")
	void NavigateDown();

	/** Move selection left (by 1 index) */
	UFUNCTION(BlueprintCallable, Category = "Command Grid|Navigation")
	void NavigateLeft();

	/** Move selection right (by 1 index) */
	UFUNCTION(BlueprintCallable, Category = "Command Grid|Navigation")
	void NavigateRight();

	/** Set selection to specific index */
	UFUNCTION(BlueprintCallable, Category = "Command Grid|Navigation")
	void SetSelectedIndex(int32 Index);

	/** Get currently selected index */
	UFUNCTION(BlueprintPure, Category = "Command Grid|Navigation")
	int32 GetSelectedIndex() const { return CurrentIndex; }

	/** Get currently selected button data */
	UFUNCTION(BlueprintPure, Category = "Command Grid|Navigation")
	bool GetSelectedButtonData(FCommandButtonData& OutData) const;

	/** Confirm current selection (triggers click) */
	UFUNCTION(BlueprintCallable, Category = "Command Grid|Navigation")
	void ConfirmSelection();

	// ==================== EVENTS ====================

	/** Called when selection changes */
	UPROPERTY(BlueprintAssignable, Category = "Command Grid|Events")
	FOnSelectionChanged OnSelectionChanged;

	/** Called when a button is clicked/confirmed */
	UPROPERTY(BlueprintAssignable, Category = "Command Grid|Events")
	FOnCommandButtonClicked OnButtonClicked;

protected:
	// ==================== UUserWidget ====================

	virtual void NativeConstruct() override;

	// ==================== BLUEPRINT BINDABLE WIDGETS ====================

	/** Grid container for button widgets */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Command Grid|Widgets")
	UUniformGridPanel* GridPanel;

	// ==================== SETTINGS ====================

	/** Button widget class to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command Grid|Settings")
	TSubclassOf<UWBPCommandButton> ButtonWidgetClass;

	/** Number of columns (default 2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command Grid|Settings")
	int32 ColumnCount = 2;

	// ==================== BLUEPRINT IMPLEMENTABLE ====================

	/** Called when buttons are populated - implement for animations */
	UFUNCTION(BlueprintImplementableEvent, Category = "Command Grid|Events")
	void OnButtonsPopulated(int32 ButtonCount);

	/** Called when selection changes - implement for animations */
	UFUNCTION(BlueprintImplementableEvent, Category = "Command Grid|Events")
	void OnSelectionAnimated(int32 OldIndex, int32 NewIndex);

private:
	/** Currently spawned button widgets */
	UPROPERTY()
	TArray<UWBPCommandButton*> ButtonWidgets;

	/** Currently selected index */
	int32 CurrentIndex = 0;

	/** Maximum valid index */
	int32 MaxIndex = -1;

	/** Current content type */
	EGridType GridType = EGridType::None;

	/** Handle button click from child */
	UFUNCTION()
	void HandleButtonClicked(const FCommandButtonData& ButtonData);

	/** Update selection visuals */
	void UpdateSelectionVisuals(int32 OldIndex, int32 NewIndex);

	/** Create button widget */
	UWBPCommandButton* CreateButtonWidget();

	/** Get row for index */
	int32 GetRow(int32 Index) const { return Index / ColumnCount; }

	/** Get column for index */
	int32 GetColumn(int32 Index) const { return Index % ColumnCount; }

	/** Check if index is in left column */
	bool IsLeftColumn(int32 Index) const { return GetColumn(Index) == 0; }

	/** Check if index is in right column */
	bool IsRightColumn(int32 Index) const { return GetColumn(Index) == ColumnCount - 1; }
};
