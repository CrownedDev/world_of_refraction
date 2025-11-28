// WBPCommandGrid.cpp
// 2-column grid implementation
// World of Refraction - Combat UI

#include "UI/Combat/WBPCommandGrid.h"
#include "UI/Combat/WBPCommandButton.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"

UWBPCommandGrid::UWBPCommandGrid(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWBPCommandGrid::NativeConstruct()
{
	Super::NativeConstruct();
}

void UWBPCommandGrid::SetButtons(const TArray<FCommandButtonData>& InButtonData)
{
	// Clear existing buttons
	ClearButtons();

	if (!GridPanel || !ButtonWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WBPCommandGrid] Cannot populate - missing GridPanel or ButtonWidgetClass"));
		return;
	}

	// Create buttons for each data entry
	for (int32 i = 0; i < InButtonData.Num(); i++)
	{
		UWBPCommandButton* Button = CreateButtonWidget();
		if (Button)
		{
			// Set data
			FCommandButtonData Data = InButtonData[i];
			Data.bIsSelected = (i == 0); // First button selected by default
			Button->SetButtonData(Data);

			// Bind click event
			Button->OnClicked.AddDynamic(this, &UWBPCommandGrid::HandleButtonClicked);

			// Calculate grid position
			int32 Row = GetRow(i);
			int32 Column = GetColumn(i);

			// Add to grid
			UUniformGridSlot* GridSlot = GridPanel->AddChildToUniformGrid(Button, Row, Column);
			if (GridSlot)
			{
				GridSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
				GridSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
			}

			ButtonWidgets.Add(Button);
		}
	}

	// Set max index
	MaxIndex = ButtonWidgets.Num() - 1;

	// Reset selection to first button
	CurrentIndex = 0;

	// Notify Blueprint
	OnButtonsPopulated(ButtonWidgets.Num());

	UE_LOG(LogTemp, Log, TEXT("[WBPCommandGrid] Populated with %d buttons in %d rows"),
		ButtonWidgets.Num(), (ButtonWidgets.Num() + ColumnCount - 1) / ColumnCount);
}

void UWBPCommandGrid::ClearButtons()
{
	// Unbind events and remove widgets
	for (UWBPCommandButton* Button : ButtonWidgets)
	{
		if (Button)
		{
			Button->OnClicked.RemoveAll(this);
			Button->RemoveFromParent();
		}
	}

	ButtonWidgets.Empty();
	CurrentIndex = 0;
	MaxIndex = -1;
	GridType = EGridType::None;
}

bool UWBPCommandGrid::GetButtonDataAtIndex(int32 Index, FCommandButtonData& OutData) const
{
	if (Index >= 0 && Index < ButtonWidgets.Num() && ButtonWidgets[Index])
	{
		OutData = ButtonWidgets[Index]->GetButtonData();
		return true;
	}
	return false;
}

void UWBPCommandGrid::NavigateUp()
{
	if (ButtonWidgets.Num() == 0)
	{
		return;
	}

	// Move up by 2 (one row)
	int32 NewIndex = CurrentIndex - ColumnCount;

	// Only move if valid
	if (NewIndex >= 0)
	{
		// Skip disabled buttons
		if (ButtonWidgets[NewIndex] && ButtonWidgets[NewIndex]->IsButtonEnabled())
		{
			SetSelectedIndex(NewIndex);
		}
	}
}

void UWBPCommandGrid::NavigateDown()
{
	if (ButtonWidgets.Num() == 0)
	{
		return;
	}

	// Move down by 2 (one row)
	int32 NewIndex = CurrentIndex + ColumnCount;

	// Only move if valid
	if (NewIndex <= MaxIndex)
	{
		// Skip disabled buttons
		if (ButtonWidgets[NewIndex] && ButtonWidgets[NewIndex]->IsButtonEnabled())
		{
			SetSelectedIndex(NewIndex);
		}
	}
}

void UWBPCommandGrid::NavigateLeft()
{
	if (ButtonWidgets.Num() == 0)
	{
		return;
	}

	// Can only move left if in right column
	if (!IsLeftColumn(CurrentIndex))
	{
		int32 NewIndex = CurrentIndex - 1;

		if (NewIndex >= 0 && ButtonWidgets[NewIndex] && ButtonWidgets[NewIndex]->IsButtonEnabled())
		{
			SetSelectedIndex(NewIndex);
		}
	}
}

void UWBPCommandGrid::NavigateRight()
{
	if (ButtonWidgets.Num() == 0)
	{
		return;
	}

	// Can only move right if in left column and there's a button to the right
	if (IsLeftColumn(CurrentIndex))
	{
		int32 NewIndex = CurrentIndex + 1;

		if (NewIndex <= MaxIndex && ButtonWidgets[NewIndex] && ButtonWidgets[NewIndex]->IsButtonEnabled())
		{
			SetSelectedIndex(NewIndex);
		}
	}
}

void UWBPCommandGrid::SetSelectedIndex(int32 Index)
{
	if (Index < 0 || Index > MaxIndex)
	{
		return;
	}

	// Check if button is enabled
	if (ButtonWidgets[Index] && !ButtonWidgets[Index]->IsButtonEnabled())
	{
		return;
	}

	int32 OldIndex = CurrentIndex;
	CurrentIndex = Index;

	UpdateSelectionVisuals(OldIndex, CurrentIndex);

	// Broadcast selection change
	OnSelectionChanged.Broadcast(OldIndex, CurrentIndex);

	// Notify Blueprint for animations
	OnSelectionAnimated(OldIndex, CurrentIndex);
}

bool UWBPCommandGrid::GetSelectedButtonData(FCommandButtonData& OutData) const
{
	return GetButtonDataAtIndex(CurrentIndex, OutData);
}

void UWBPCommandGrid::ConfirmSelection()
{
	if (CurrentIndex >= 0 && CurrentIndex <= MaxIndex && ButtonWidgets[CurrentIndex])
	{
		ButtonWidgets[CurrentIndex]->SimulateClick();
	}
}

void UWBPCommandGrid::HandleButtonClicked(const FCommandButtonData& ButtonData)
{
	// Find the index of the clicked button
	for (int32 i = 0; i < ButtonWidgets.Num(); i++)
	{
		if (ButtonWidgets[i] && ButtonWidgets[i]->GetButtonData().DisplayName.EqualTo(ButtonData.DisplayName))
		{
			// Update selection if different
			if (i != CurrentIndex)
			{
				SetSelectedIndex(i);
			}
			break;
		}
	}

	// Broadcast click event
	OnButtonClicked.Broadcast(ButtonData);
}

void UWBPCommandGrid::UpdateSelectionVisuals(int32 OldIndex, int32 NewIndex)
{
	// Deselect old
	if (OldIndex >= 0 && OldIndex < ButtonWidgets.Num() && ButtonWidgets[OldIndex])
	{
		ButtonWidgets[OldIndex]->SetSelected(false);
	}

	// Select new
	if (NewIndex >= 0 && NewIndex < ButtonWidgets.Num() && ButtonWidgets[NewIndex])
	{
		ButtonWidgets[NewIndex]->SetSelected(true);
	}
}

UWBPCommandButton* UWBPCommandGrid::CreateButtonWidget()
{
	if (!ButtonWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[WBPCommandGrid] ButtonWidgetClass is not set!"));
		return nullptr;
	}

	UWBPCommandButton* Button = CreateWidget<UWBPCommandButton>(this, ButtonWidgetClass);
	return Button;
}
