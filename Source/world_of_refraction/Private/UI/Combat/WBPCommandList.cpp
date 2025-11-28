// WBPCommandList.cpp
// Vertical command list implementation
// World of Refraction - Combat UI

#include "UI/Combat/WBPCommandList.h"
#include "UI/Combat/WBPCommandButton.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

UWBPCommandList::UWBPCommandList(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWBPCommandList::NativeConstruct()
{
	Super::NativeConstruct();
}

void UWBPCommandList::SetButtons(const TArray<FCommandButtonData>& InButtonData)
{
	// Clear existing buttons
	ClearButtons();

	if (!ButtonContainer || !ButtonWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WBPCommandList] Cannot populate - missing ButtonContainer or ButtonWidgetClass"));
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
			Button->OnClicked.AddDynamic(this, &UWBPCommandList::HandleButtonClicked);

			// Add to container
			UVerticalBoxSlot* ListSlot = ButtonContainer->AddChildToVerticalBox(Button);
			if (ListSlot)
			{
				ListSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 2.0f));
			}

			ButtonWidgets.Add(Button);
		}
	}

	// Reset selection to first button
	CurrentIndex = 0;

	// Notify Blueprint
	OnButtonsPopulated(ButtonWidgets.Num());

	UE_LOG(LogTemp, Log, TEXT("[WBPCommandList] Populated with %d buttons"), ButtonWidgets.Num());
}

void UWBPCommandList::ClearButtons()
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
}

bool UWBPCommandList::GetButtonDataAtIndex(int32 Index, FCommandButtonData& OutData) const
{
	if (Index >= 0 && Index < ButtonWidgets.Num() && ButtonWidgets[Index])
	{
		OutData = ButtonWidgets[Index]->GetButtonData();
		return true;
	}
	return false;
}

void UWBPCommandList::NavigateUp()
{
	if (ButtonWidgets.Num() == 0)
	{
		return;
	}

	int32 OldIndex = CurrentIndex;
	int32 NewIndex = CurrentIndex - 1;

	if (NewIndex < 0)
	{
		NewIndex = bWrapNavigation ? ButtonWidgets.Num() - 1 : 0;
	}

	// Skip disabled buttons
	int32 Attempts = 0;
	while (Attempts < ButtonWidgets.Num())
	{
		if (ButtonWidgets[NewIndex] && ButtonWidgets[NewIndex]->IsButtonEnabled())
		{
			break;
		}

		NewIndex = NewIndex - 1;
		if (NewIndex < 0)
		{
			NewIndex = bWrapNavigation ? ButtonWidgets.Num() - 1 : 0;
		}
		Attempts++;
	}

	if (NewIndex != OldIndex)
	{
		SetSelectedIndex(NewIndex);
	}
}

void UWBPCommandList::NavigateDown()
{
	if (ButtonWidgets.Num() == 0)
	{
		return;
	}

	int32 OldIndex = CurrentIndex;
	int32 NewIndex = CurrentIndex + 1;

	if (NewIndex >= ButtonWidgets.Num())
	{
		NewIndex = bWrapNavigation ? 0 : ButtonWidgets.Num() - 1;
	}

	// Skip disabled buttons
	int32 Attempts = 0;
	while (Attempts < ButtonWidgets.Num())
	{
		if (ButtonWidgets[NewIndex] && ButtonWidgets[NewIndex]->IsButtonEnabled())
		{
			break;
		}

		NewIndex = NewIndex + 1;
		if (NewIndex >= ButtonWidgets.Num())
		{
			NewIndex = bWrapNavigation ? 0 : ButtonWidgets.Num() - 1;
		}
		Attempts++;
	}

	if (NewIndex != OldIndex)
	{
		SetSelectedIndex(NewIndex);
	}
}

void UWBPCommandList::SetSelectedIndex(int32 Index)
{
	if (Index < 0 || Index >= ButtonWidgets.Num())
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

bool UWBPCommandList::GetSelectedButtonData(FCommandButtonData& OutData) const
{
	return GetButtonDataAtIndex(CurrentIndex, OutData);
}

void UWBPCommandList::ConfirmSelection()
{
	if (CurrentIndex >= 0 && CurrentIndex < ButtonWidgets.Num() && ButtonWidgets[CurrentIndex])
	{
		ButtonWidgets[CurrentIndex]->SimulateClick();
	}
}

void UWBPCommandList::HandleButtonClicked(const FCommandButtonData& ButtonData)
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

void UWBPCommandList::UpdateSelectionVisuals(int32 OldIndex, int32 NewIndex)
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

UWBPCommandButton* UWBPCommandList::CreateButtonWidget()
{
	if (!ButtonWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[WBPCommandList] ButtonWidgetClass is not set!"));
		return nullptr;
	}

	UWBPCommandButton* Button = CreateWidget<UWBPCommandButton>(this, ButtonWidgetClass);
	return Button;
}
