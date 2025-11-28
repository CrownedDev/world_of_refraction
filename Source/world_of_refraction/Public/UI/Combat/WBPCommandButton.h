// WBPCommandButton.h
// Single selectable command button for Kingdom Hearts-style menu
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommandMenuTypes.h"
#include "WBPCommandButton.generated.h"

class UTextBlock;
class UImage;
class UBorder;
class UButton;

/**
 * Base class for command menu buttons
 * Handles selection state, enabled/disabled, and click events
 * 
 * Blueprint should bind:
 * - BackgroundBorder: Border for highlight effect
 * - ButtonText: TextBlock for display name
 * - IconImage: Image for optional icon (can be hidden)
 * - EquippedIndicator: Image/Border for "currently equipped" state (rings)
 * - ClickButton: Invisible button overlay for mouse interaction
 */
UCLASS(Abstract, Blueprintable)
class WORLD_OF_REFRACTION_API UWBPCommandButton : public UUserWidget
{
	GENERATED_BODY()

public:
	UWBPCommandButton(const FObjectInitializer& ObjectInitializer);

	// ==================== SETUP ====================

	/** Initialize button with data */
	UFUNCTION(BlueprintCallable, Category = "Command Button")
	void SetButtonData(const FCommandButtonData& InData);

	/** Get current button data */
	UFUNCTION(BlueprintPure, Category = "Command Button")
	const FCommandButtonData& GetButtonData() const { return ButtonData; }

	// ==================== STATE ====================

	/** Set selected/highlighted state */
	UFUNCTION(BlueprintCallable, Category = "Command Button")
	void SetSelected(bool bInSelected);

	/** Is this button currently selected? */
	UFUNCTION(BlueprintPure, Category = "Command Button")
	bool IsSelected() const { return bIsSelected; }

	/** Set enabled/disabled state */
	UFUNCTION(BlueprintCallable, Category = "Command Button")
	void SetButtonEnabled(bool bInEnabled);

	/** Is this button enabled? */
	UFUNCTION(BlueprintPure, Category = "Command Button")
	bool IsButtonEnabled() const { return bIsEnabled; }

	/** Set equipped indicator visibility (for rings) */
	UFUNCTION(BlueprintCallable, Category = "Command Button")
	void SetEquippedIndicatorVisible(bool bVisible);

	// ==================== EVENTS ====================

	/** Called when this button is clicked */
	UPROPERTY(BlueprintAssignable, Category = "Command Button|Events")
	FOnCommandButtonClicked OnClicked;

	/** Simulate a click on this button */
	UFUNCTION(BlueprintCallable, Category = "Command Button")
	void SimulateClick();

protected:
	// ==================== UUserWidget ====================

	virtual void NativeConstruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// ==================== BLUEPRINT BINDABLE WIDGETS ====================

	/** Background border for highlight effect */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Command Button|Widgets")
	UBorder* BackgroundBorder;

	/** Text display */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Command Button|Widgets")
	UTextBlock* ButtonText;

	/** Optional icon image */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Command Button|Widgets")
	UImage* IconImage;

	/** Equipped indicator (for rings showing current ring) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Command Button|Widgets")
	UBorder* EquippedIndicator;

	// ==================== VISUAL SETTINGS ====================

	/** Color when selected/highlighted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command Button|Appearance")
	FLinearColor SelectedColor = FLinearColor(0.2f, 0.4f, 0.8f, 1.0f);

	/** Color when not selected (normal state) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command Button|Appearance")
	FLinearColor NormalColor = FLinearColor(0.1f, 0.1f, 0.1f, 0.8f);

	/** Color when disabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command Button|Appearance")
	FLinearColor DisabledColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.5f);

	/** Text color when selected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command Button|Appearance")
	FSlateColor SelectedTextColor = FSlateColor(FLinearColor::White);

	/** Text color when not selected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command Button|Appearance")
	FSlateColor NormalTextColor = FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));

	/** Text color when disabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command Button|Appearance")
	FSlateColor DisabledTextColor = FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f));

	/** Equipped indicator color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Command Button|Appearance")
	FLinearColor EquippedColor = FLinearColor(0.8f, 0.6f, 0.0f, 1.0f);

	// ==================== BLUEPRINT IMPLEMENTABLE ====================

	/** Called when visual state needs updating - implement in Blueprint for animations */
	UFUNCTION(BlueprintImplementableEvent, Category = "Command Button|Events")
	void OnVisualStateChanged(bool bSelected, bool bEnabled);

	/** Called on mouse hover - implement in Blueprint for hover effects */
	UFUNCTION(BlueprintImplementableEvent, Category = "Command Button|Events")
	void OnHoverStateChanged(bool bIsHovered);

private:
	/** Current button data */
	UPROPERTY()
	FCommandButtonData ButtonData;

	/** Current selection state */
	bool bIsSelected = false;

	/** Current enabled state */
	bool bIsEnabled = true;

	/** Update visual appearance based on current state */
	void UpdateVisuals();
};
