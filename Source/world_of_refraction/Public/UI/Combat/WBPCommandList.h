// WBPCommandList.h
// Vertical command list with up/down navigation
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommandMenuTypes.h"
#include "WBPCommandList.generated.h"

class UVerticalBox;
class UWBPCommandButton;

/**
 * Vertical list of command buttons with up/down navigation
 * Used for main menu and spell schools list
 * 
 * Blueprint should bind:
 * - ButtonContainer: VerticalBox to hold buttons
 */
UCLASS(Abstract, Blueprintable)
class WORLD_OF_REFRACTION_API UWBPCommandList : public UUserWidget
{
	GENERATED_BODY()

public:
	UWBPCommandList(const FObjectInitializer& ObjectInitializer);

	// ==================== POPULATION ====================

	/** Set buttons from array of data */
	UFUNCTION(BlueprintCallable, Category = "Command List")
	void SetButtons(const TArray<FCommandButtonData>& InButtonData);

	/** Clear all buttons */
	UFUNCTION(BlueprintCallable, Category = "Command List")
	void ClearButtons();

	/** Get number of buttons */
	UFUNCTION(BlueprintPure, Category = "Command List")
	int32 GetButtonCount() const { return ButtonWidgets.Num(); }

	/** Get button data at index */
	UFUNCTION(BlueprintPure, Category = "Command List")
	bool GetButtonDataAtIndex(int32 Index, FCommandButtonData& OutData) const;

	// ==================== NAVIGATION ====================

	/** Move selection up */
	UFUNCTION(BlueprintCallable, Category = "Command List|Navigation")
	void NavigateUp();

	/** Move selection down */
	UFUNCTION(BlueprintCallable, Category = "Command List|Navigation")
	void NavigateDown();

	/** Set selection to specific index */
	UFUNCTION(BlueprintCallable, Category = "Command List|Navigation")
	void SetSelectedIndex(int32 Index);

	/** Get currently selected index */
	UFUNCTION(BlueprintPure, Category = "Command List|Navigation")
	int32 GetSelectedIndex() const { return CurrentIndex; }

	/** Get currently selected button data */
	UFUNCTION(BlueprintPure, Category = "Command List|Navigation")
	bool GetSelectedButtonData(FCommandButtonData& OutData) const;

	/** Confirm current selection (triggers click) */
	UFUNCTION(BlueprintCallable, Category = "Command List|Navigation")
	void ConfirmSelection();

	// ==================== EVENTS ====================

	/** Called when selection changes */
	UPROPERTY(BlueprintAssignable, Category = "Command List|Events")
	FOnSelectionChanged OnSelectionChanged;

	/** Called when a button is clicked/confirmed */
	UPROPERTY(BlueprintAssignable, Category = "Command List|Events")
	FOnCommandButtonClicked OnButtonClicked;

protected:
	// ==================== UUserWidget ====================

	virtual void NativeConstruct() override;

	// ==================== BLUEPRINT BINDABLE WIDGETS ====================

	/** Container for button widgets */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Command List|Widgets")
	UVerticalBox* ButtonContainer;

	// ==================== SETTINGS ====================

	/** Button widget class to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command List|Settings")
	TSubclassOf<UWBPCommandButton> ButtonWidgetClass;

	/** Wrap navigation (bottom -> top, top -> bottom) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command List|Settings")
	bool bWrapNavigation = false;

	// ==================== BLUEPRINT IMPLEMENTABLE ====================

	/** Called when buttons are populated - implement for animations */
	UFUNCTION(BlueprintImplementableEvent, Category = "Command List|Events")
	void OnButtonsPopulated(int32 ButtonCount);

	/** Called when selection changes - implement for animations */
	UFUNCTION(BlueprintImplementableEvent, Category = "Command List|Events")
	void OnSelectionAnimated(int32 OldIndex, int32 NewIndex);

private:
	/** Currently spawned button widgets */
	UPROPERTY()
	TArray<UWBPCommandButton*> ButtonWidgets;

	/** Currently selected index */
	int32 CurrentIndex = 0;

	/** Handle button click from child */
	UFUNCTION()
	void HandleButtonClicked(const FCommandButtonData& ButtonData);

	/** Update selection visuals */
	void UpdateSelectionVisuals(int32 OldIndex, int32 NewIndex);

	/** Create button widget */
	UWBPCommandButton* CreateButtonWidget();
};
