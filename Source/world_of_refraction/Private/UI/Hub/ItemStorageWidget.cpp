// ItemStorageWidget.cpp

#include "UI/Hub/ItemStorageWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Pool/PoolSubsystem.h"

#define LOCTEXT_NAMESPACE "ItemStorage"

namespace ItemStorageStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace ItemStorageBuild
{
	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = ItemStorageStyle::FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(ItemStorageStyle::RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = ItemStorageStyle::FontSize;
		Text->SetFont(Font);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(ItemStorageStyle::RowPadding));
		}
		return Button;
	}

	// TODO: B-127 - item names as FText once ItemIdentity returns them.
	FText ItemName(const FCrystalId &Id) { return FText::FromString(ItemIdentity::GetDisplayName(Id)); }
}

UItemStorageWidget *UItemStorageWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UItemStorageWidget *Widget = CreateWidget<UItemStorageWidget>(PC, UItemStorageWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(ItemStorageStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UItemStorageWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(ItemStorageStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		SlotsText = ItemStorageBuild::AddText(WidgetTree, Box);
		ItemText = ItemStorageBuild::AddText(WidgetTree, Box);
		ItemStorageBuild::AddButton(WidgetTree, Box, LOCTEXT("Next", "Next item"))
			->OnClicked.AddDynamic(this, &UItemStorageWidget::HandleNextClicked);
		MoveOneButton = ItemStorageBuild::AddButton(WidgetTree, Box, LOCTEXT("MoveOne", "Move one"));
		MoveOneButton->OnClicked.AddDynamic(this, &UItemStorageWidget::HandleMoveOneClicked);
		MoveAllButton = ItemStorageBuild::AddButton(WidgetTree, Box, LOCTEXT("MoveAll", "Move the stack"));
		MoveAllButton->OnClicked.AddDynamic(this, &UItemStorageWidget::HandleMoveAllClicked);
		StatusLine = ItemStorageBuild::AddText(WidgetTree, Box);
		ItemStorageBuild::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))
			->OnClicked.AddDynamic(this, &UItemStorageWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UItemStorageWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

UPoolSubsystem *UItemStorageWidget::GetPool() const
{
	const UGameInstance *GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
}

TArray<UItemStorageWidget::FStackChoice> UItemStorageWidget::GatherChoices() const
{
	TArray<FStackChoice> Choices;
	const UPoolSubsystem *Pool = GetPool();
	if (!Pool)
	{
		return Choices;
	}
	for (const TPair<FCrystalId, int32> &Pair : Pool->GetCarriedItems())
	{
		Choices.Add({Pair.Key, Pair.Value, true});
	}
	for (const TMap<FCrystalId, int32> *Storage : {&Pool->GetCrystals(), &Pool->GetStones()})
	{
		for (const TPair<FCrystalId, int32> &Pair : *Storage)
		{
			Choices.Add({Pair.Key, Pair.Value, false});
		}
	}
	return Choices;
}

void UItemStorageWidget::Refresh()
{
	const UPoolSubsystem *Pool = GetPool();
	const TArray<FStackChoice> Choices = GatherChoices();
	SelectedIndex = Choices.IsEmpty() ? 0 : FMath::Clamp(SelectedIndex, 0, Choices.Num() - 1);

	SlotsText->SetText(Pool ? FText::Format(LOCTEXT("SlotsRow", "Carried: {0} of {1} item slots"), Pool->GetUsedItemSlots(),
											Pool->GetItemSlotCount())
							: LOCTEXT("NoPool", "No item storage"));
	if (Choices.IsEmpty())
	{
		ItemText->SetText(LOCTEXT("NoItems", "No items carried or stored"));
	}
	else
	{
		const FStackChoice &Choice = Choices[SelectedIndex];
		ItemText->SetText(FText::Format(LOCTEXT("ItemRow", "{0}: {1} x{2} ({3} of {4}), moves to {5}"),
										Choice.bCarried ? LOCTEXT("Carried", "Carried") : LOCTEXT("Stored", "In storage"),
										ItemStorageBuild::ItemName(Choice.Id), Choice.Count, SelectedIndex + 1, Choices.Num(),
										Choice.bCarried ? LOCTEXT("ToStorage", "storage") : LOCTEXT("ToCarried", "the carried slots")));
	}
	MoveOneButton->SetIsEnabled(!Choices.IsEmpty());
	MoveAllButton->SetIsEnabled(!Choices.IsEmpty());
	StatusLine->SetText(StatusText);
}

FString UItemStorageWidget::GetItemStorageString() const
{
	FString Out = TEXT("[ItemStorage] ") + SlotsText->GetText().ToString() + TEXT("\n  ") + ItemText->GetText().ToString();
	if (!StatusText.IsEmpty())
	{
		Out += TEXT("\n  ") + StatusText.ToString();
	}
	return Out;
}

void UItemStorageWidget::MoveSelected(int32 Count)
{
	UPoolSubsystem *Pool = GetPool();
	const TArray<FStackChoice> Choices = GatherChoices();
	if (!Pool || !Choices.IsValidIndex(SelectedIndex))
	{
		return;
	}
	const FStackChoice &Choice = Choices[SelectedIndex];
	const int32 Moving = FMath::Min(Count, Choice.Count);
	const bool bMoved = Choice.bCarried ? Pool->MoveToStorage(Choice.Id, Moving) : Pool->MoveToCarried(Choice.Id, Moving);
	const FText Name = ItemStorageBuild::ItemName(Choice.Id);
	StatusText = bMoved ? FText::Format(LOCTEXT("Moved", "Moved {0} x{1}"), Name, Moving)
		: FText::Format(LOCTEXT("NoRoom", "No room for {0} x{1}: free a slot first (or move fewer)"), Name, Moving);
	Refresh();
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetItemStorageString());
}

void UItemStorageWidget::HandleNextClicked()
{
	const int32 Count = GatherChoices().Num();
	SelectedIndex = Count > 0 ? (SelectedIndex + 1) % Count : 0;
	StatusText = FText::GetEmpty();
	Refresh();
}

void UItemStorageWidget::HandleMoveOneClicked() { MoveSelected(1); }

void UItemStorageWidget::HandleMoveAllClicked() { MoveSelected(MAX_int32); }

void UItemStorageWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

static FAutoConsoleCommandWithWorld GItemStorageUICommand(
	TEXT("wor.UI.ItemStorage"),
	TEXT("PLACEHOLDER UI: move crystals and stones between the party's carried item slots and storage (B-203)."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!UItemStorageWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogTemp, Warning, TEXT("[ItemStorage] wor.UI.ItemStorage: no player controller"));
			}
		}));

#undef LOCTEXT_NAMESPACE
