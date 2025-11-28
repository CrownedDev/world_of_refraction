// WBPCommandButton.cpp
// Single selectable command button implementation
// World of Refraction - Combat UI

#include "UI/Combat/WBPCommandButton.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

UWBPCommandButton::UWBPCommandButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Enable mouse interaction
	SetIsFocusable(true);
}

void UWBPCommandButton::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize visuals
	UpdateVisuals();
}

void UWBPCommandButton::SetButtonData(const FCommandButtonData& InData)
{
	ButtonData = InData;

	// Update text
	if (ButtonText)
	{
		ButtonText->SetText(ButtonData.DisplayName);
	}

	// Update icon
	if (IconImage)
	{
		if (ButtonData.Icon)
		{
			IconImage->SetBrushFromTexture(ButtonData.Icon);
			IconImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			IconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update enabled state
	bIsEnabled = ButtonData.bIsEnabled;
	bIsSelected = ButtonData.bIsSelected;

	// Update equipped indicator
	SetEquippedIndicatorVisible(ButtonData.bIsCurrentlyEquipped);

	// Refresh visuals
	UpdateVisuals();
}

void UWBPCommandButton::SetSelected(bool bInSelected)
{
	if (bIsSelected == bInSelected)
	{
		return;
	}

	bIsSelected = bInSelected;
	ButtonData.bIsSelected = bInSelected;
	UpdateVisuals();
}

void UWBPCommandButton::SetButtonEnabled(bool bInEnabled)
{
	if (bIsEnabled == bInEnabled)
	{
		return;
	}

	bIsEnabled = bInEnabled;
	ButtonData.bIsEnabled = bInEnabled;
	UpdateVisuals();
}

void UWBPCommandButton::SetEquippedIndicatorVisible(bool bVisible)
{
	if (EquippedIndicator)
	{
		EquippedIndicator->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

		if (bVisible)
		{
			EquippedIndicator->SetBrushColor(EquippedColor);
		}
	}
}

void UWBPCommandButton::SimulateClick()
{
	if (bIsEnabled)
	{
		OnClicked.Broadcast(ButtonData);
	}
}

void UWBPCommandButton::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	// Notify Blueprint for hover effects
	OnHoverStateChanged(true);
}

void UWBPCommandButton::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	// Notify Blueprint for hover effects
	OnHoverStateChanged(false);
}

FReply UWBPCommandButton::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bIsEnabled)
	{
		SimulateClick();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UWBPCommandButton::UpdateVisuals()
{
	// Update background color
	if (BackgroundBorder)
	{
		FLinearColor BackgroundColor;

		if (!bIsEnabled)
		{
			BackgroundColor = DisabledColor;
		}
		else if (bIsSelected)
		{
			BackgroundColor = SelectedColor;
		}
		else
		{
			BackgroundColor = NormalColor;
		}

		BackgroundBorder->SetBrushColor(BackgroundColor);
	}

	// Update text color
	if (ButtonText)
	{
		FSlateColor TextColor;

		if (!bIsEnabled)
		{
			TextColor = DisabledTextColor;
		}
		else if (bIsSelected)
		{
			TextColor = SelectedTextColor;
		}
		else
		{
			TextColor = NormalTextColor;
		}

		ButtonText->SetColorAndOpacity(TextColor);
	}

	// Notify Blueprint for additional visual updates (animations, etc.)
	OnVisualStateChanged(bIsSelected, bIsEnabled);
}
