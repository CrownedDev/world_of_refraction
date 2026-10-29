// DismantleWidget.cpp

#include "UI/Hub/DismantleWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Currency/EconomyService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "Dismantle"

namespace DismantleStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace DismantleBuild
{
	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = DismantleStyle::FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(DismantleStyle::RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = DismantleStyle::FontSize;
		Text->SetFont(Font);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(DismantleStyle::RowPadding));
		}
		return Button;
	}

	AActor *OwnerOf(const UUserWidget *Widget)
	{
		const APlayerController *PC = Widget ? Widget->GetOwningPlayer() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}

	UEconomyService *EconomyOf(const UUserWidget *Widget)
	{
		const UGameInstance *GI = Widget ? Widget->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UEconomyService>() : nullptr;
	}
}

UDismantleWidget *UDismantleWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UDismantleWidget *Widget = CreateWidget<UDismantleWidget>(PC, UDismantleWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(DismantleStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UDismantleWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(DismantleStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		ItemText = DismantleBuild::AddText(WidgetTree, Box);
		DismantleBuild::AddButton(WidgetTree, Box, LOCTEXT("Next", "Next item"))
			->OnClicked.AddDynamic(this, &UDismantleWidget::HandleNextClicked);
		DismantleButton = DismantleBuild::AddButton(WidgetTree, Box, LOCTEXT("Dismantle", "Dismantle"));
		DismantleButton->OnClicked.AddDynamic(this, &UDismantleWidget::HandleDismantleClicked);
		WarningText = DismantleBuild::AddText(WidgetTree, Box);
		ConfirmButton = DismantleBuild::AddButton(WidgetTree, Box, LOCTEXT("Confirm", "Dismantle anyway"));
		ConfirmButton->OnClicked.AddDynamic(this, &UDismantleWidget::HandleConfirmClicked);
		CancelButton = DismantleBuild::AddButton(WidgetTree, Box, LOCTEXT("Cancel", "Cancel"));
		CancelButton->OnClicked.AddDynamic(this, &UDismantleWidget::HandleCancelClicked);
		StatusLine = DismantleBuild::AddText(WidgetTree, Box);
		DismantleBuild::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))
			->OnClicked.AddDynamic(this, &UDismantleWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UDismantleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

TArray<UDismantleWidget::FDismantleChoice> UDismantleWidget::GatherChoices() const
{
	TArray<FDismantleChoice> Choices;
	const AActor *Owner = DismantleBuild::OwnerOf(this);
	const UInventoryComponent *Inv = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inv)
	{
		return Choices;
	}
	for (const FWeaponInventoryEntry &Entry : Inv->Weapons)
	{
		if (Entry.Weapon)
		{
			Choices.Add({Entry.PersistentID, FText::Format(LOCTEXT("WeaponRow", "Weapon: {0}"), FText::FromString(Entry.Weapon->Name)), false});
		}
	}
	for (const FRingInventoryEntry &Entry : Inv->Rings)
	{
		if (Entry.Ring)
		{
			Choices.Add({Entry.PersistentID, FText::Format(LOCTEXT("AdapterRow", "{0}: {1}"), WoRGameNames::GetAdapterWord(), FText::FromString(Entry.Ring->Name)), true});
		}
	}
	return Choices;
}

void UDismantleWidget::Refresh()
{
	const TArray<FDismantleChoice> Choices = GatherChoices();
	if (Choices.IsEmpty())
	{
		SelectedIndex = 0;
		bConfirming = false;
	}
	else
	{
		SelectedIndex = FMath::Clamp(SelectedIndex, 0, Choices.Num() - 1);
	}

	ItemText->SetText(Choices.IsEmpty() ? LOCTEXT("NoItems", "No weapons or adapters to dismantle")
										: FText::Format(LOCTEXT("ItemRow", "{0} ({1} of {2})"), Choices[SelectedIndex].Label,
														SelectedIndex + 1, Choices.Num()));
	DismantleButton->SetIsEnabled(!Choices.IsEmpty() && !bConfirming);

	const FText Warning = bConfirming ? UEconomyService::GetDismantleWarning(DismantleBuild::OwnerOf(this), Choices[SelectedIndex].PersistentID)
									  : FText::GetEmpty();
	WarningText->SetText(Warning);
	const ESlateVisibility ConfirmVisibility = bConfirming ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	WarningText->SetVisibility(ConfirmVisibility);
	ConfirmButton->SetVisibility(ConfirmVisibility);
	CancelButton->SetVisibility(ConfirmVisibility);
	StatusLine->SetText(StatusText);
}

FString UDismantleWidget::GetDismantleString() const
{
	FString Out = TEXT("[Dismantle] ") + ItemText->GetText().ToString();
	if (bConfirming)
	{
		Out += TEXT("\n  Warning: ") + WarningText->GetText().ToString();
	}
	if (!StatusText.IsEmpty())
	{
		Out += TEXT("\n  ") + StatusText.ToString();
	}
	return Out;
}

void UDismantleWidget::DismantleSelected()
{
	const TArray<FDismantleChoice> Choices = GatherChoices();
	UEconomyService *Economy = DismantleBuild::EconomyOf(this);
	AActor *Owner = DismantleBuild::OwnerOf(this);
	bConfirming = false;
	if (!Choices.IsValidIndex(SelectedIndex) || !Economy || !Owner)
	{
		return;
	}
	const FDismantleChoice &Choice = Choices[SelectedIndex];
	const bool bDone = Choice.bRing ? Economy->DismantleRing(Owner, Choice.PersistentID)
									: Economy->DismantleWeapon(Owner, Choice.PersistentID);
	StatusText = bDone ? FText::Format(LOCTEXT("Dismantled", "Dismantled {0}"), Choice.Label)
					   : FText::Format(LOCTEXT("DismantleFailed", "Couldn't dismantle {0}"), Choice.Label);
}

void UDismantleWidget::HandleNextClicked()
{
	const int32 Count = GatherChoices().Num();
	SelectedIndex = Count > 0 ? (SelectedIndex + 1) % Count : 0;
	bConfirming = false;
	StatusText = FText::GetEmpty();
	Refresh();
}

void UDismantleWidget::HandleDismantleClicked()
{
	const TArray<FDismantleChoice> Choices = GatherChoices();
	if (!Choices.IsValidIndex(SelectedIndex))
	{
		return;
	}
	// D-016: an equipped item asks first; anything else goes at once.
	bConfirming = !UEconomyService::GetDismantleWarning(DismantleBuild::OwnerOf(this), Choices[SelectedIndex].PersistentID).IsEmpty();
	if (!bConfirming)
	{
		DismantleSelected();
	}
	Refresh();
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetDismantleString());
}

void UDismantleWidget::HandleConfirmClicked()
{
	DismantleSelected();
	Refresh();
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetDismantleString());
}

void UDismantleWidget::HandleCancelClicked()
{
	bConfirming = false;
	Refresh();
}

void UDismantleWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

static FAutoConsoleCommandWithWorld GDismantleUICommand(
	TEXT("wor.UI.Dismantle"),
	TEXT("PLACEHOLDER UI: dismantle one of your weapons or adapters (warns when it is equipped in a loadout)."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!UDismantleWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Dismantle] wor.UI.Dismantle: no player controller"));
			}
		}));

#undef LOCTEXT_NAMESPACE
