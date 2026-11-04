// DuplicateGearWidget.cpp

#include "UI/Hub/DuplicateGearWidget.h"

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
#include "Inventory/ItemTier.h"

#include "Currency/CurrencyTypes.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "DuplicateGear"

namespace DuplicateGearStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace DuplicateGearBuild
{
	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = DuplicateGearStyle::FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(DuplicateGearStyle::RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = DuplicateGearStyle::FontSize;
		Text->SetFont(Font);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(DuplicateGearStyle::RowPadding));
		}
		return Button;
	}

	AActor *OwnerOf(const UUserWidget *Widget)
	{
		const APlayerController *PC = Widget ? Widget->GetOwningPlayer() : nullptr;
		return PC ? PC->GetPawn() : nullptr;
	}

	const UInventoryComponent *InventoryOf(const UUserWidget *Widget)
	{
		const AActor *Owner = OwnerOf(Widget);
		return Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
	}

	/** "Name (C-Tier, B-Quality)" for an owned weapon or ring, or empty when it is gone. */
	FText DescribeCopy(const UInventoryComponent &Inv, const FGuid &ID, bool bRing)
	{
		// TODO: gear and tier names are FStrings; FText with B-127 (D-061).
		const auto Describe = [](const FString &Name, EItemTier Tier, EItemQuality Quality)
		{
			return FText::Format(LOCTEXT("Copy", "{0} ({1}, {2})"), FText::FromString(Name), FText::FromString(TierHelpers::GetTierName(Tier)),
								 StaticEnum<EItemQuality>()->GetDisplayNameTextByValue(static_cast<int64>(Quality)));
		};
		if (bRing)
		{
			const FRingInventoryEntry *Ring = Inv.FindOwnedRing(ID);
			return Ring && Ring->Ring ? Describe(Ring->Ring->Name, Ring->Tier, Ring->Quality) : FText::GetEmpty();
		}
		const FWeaponInventoryEntry *Weapon = Inv.FindOwnedWeapon(ID);
		return Weapon && Weapon->Weapon ? Describe(Weapon->Weapon->Name, Weapon->Tier, Weapon->Quality) : FText::GetEmpty();
	}
}

UDuplicateGearWidget *UDuplicateGearWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UDuplicateGearWidget *Widget = CreateWidget<UDuplicateGearWidget>(PC, UDuplicateGearWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(DuplicateGearStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UDuplicateGearWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(DuplicateGearStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		PairText = DuplicateGearBuild::AddText(WidgetTree, Box);
		DuplicateGearBuild::AddButton(WidgetTree, Box, LOCTEXT("Next", "Next duplicate"))
			->OnClicked.AddDynamic(this, &UDuplicateGearWidget::HandleNextClicked);
		const FText ConvertLabel = FText::Format(LOCTEXT("Gold", "Convert the weaker copy to {0}"), WoRGameNames::GetCurrencyName(UEconomyService::GetPlaceCurrency(this)));
		GoldButton = DuplicateGearBuild::AddButton(WidgetTree, Box, ConvertLabel);
		GoldButton->OnClicked.AddDynamic(this, &UDuplicateGearWidget::HandleGoldClicked);
		DismantleButton = DuplicateGearBuild::AddButton(WidgetTree, Box, LOCTEXT("Dismantle", "Dismantle the weaker copy"));
		DismantleButton->OnClicked.AddDynamic(this, &UDuplicateGearWidget::HandleDismantleClicked);
		StatusLine = DuplicateGearBuild::AddText(WidgetTree, Box);
		DuplicateGearBuild::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))
			->OnClicked.AddDynamic(this, &UDuplicateGearWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UDuplicateGearWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

void UDuplicateGearWidget::Refresh()
{
	if (!PairText || !StatusLine || !GoldButton || !DismantleButton)
	{
		return; // not built yet: NativeConstruct refreshes
	}
	const UInventoryComponent *Inv = DuplicateGearBuild::InventoryOf(this);
	const TArray<FDuplicateGearPair> Pairs = Inv ? Inv->FindDuplicateGear() : TArray<FDuplicateGearPair>();
	SelectedIndex = Pairs.IsEmpty() ? 0 : FMath::Clamp(SelectedIndex, 0, Pairs.Num() - 1);

	FText Row = LOCTEXT("NoDuplicates", "No duplicate weapons or adapters");
	if (Pairs.IsValidIndex(SelectedIndex) && Inv)
	{
		const FDuplicateGearPair &Pair = Pairs[SelectedIndex];
		const FWeaponInventoryEntry *Weapon = Pair.bRing ? nullptr : Inv->FindOwnedWeapon(Pair.WeakerID);
		const FRingInventoryEntry *Ring = Pair.bRing ? Inv->FindOwnedRing(Pair.WeakerID) : nullptr;
		const int32 Gold = UEconomyService::GetDuplicateGoldForTier(Weapon ? Weapon->Tier : Ring ? Ring->Tier : EItemTier::F_Tier);
		Row = FText::Format(LOCTEXT("PairRow", "Keep: {0}\nWeaker: {1} ({2} {5})\n{3} of {4}"),
							DuplicateGearBuild::DescribeCopy(*Inv, Pair.KeptID, Pair.bRing),
							DuplicateGearBuild::DescribeCopy(*Inv, Pair.WeakerID, Pair.bRing), Gold, SelectedIndex + 1, Pairs.Num(),
							WoRGameNames::GetCurrencyName(UEconomyService::GetPlaceCurrency(this)));
	}
	PairText->SetText(Row);
	GoldButton->SetIsEnabled(!Pairs.IsEmpty());
	DismantleButton->SetIsEnabled(!Pairs.IsEmpty());
	StatusLine->SetText(StatusText);
}

FString UDuplicateGearWidget::GetDuplicateString() const
{
	FString Out = TEXT("[Duplicates] ") + (PairText ? PairText->GetText().ToString() : FString(TEXT("<not built>")));
	if (!StatusText.IsEmpty())
	{
		Out += TEXT("\n  ") + StatusText.ToString();
	}
	return Out;
}

void UDuplicateGearWidget::Resolve(bool bConvertToGold)
{
	const UInventoryComponent *Inv = DuplicateGearBuild::InventoryOf(this);
	const TArray<FDuplicateGearPair> Pairs = Inv ? Inv->FindDuplicateGear() : TArray<FDuplicateGearPair>();
	const UGameInstance *GI = GetGameInstance();
	UEconomyService *Economy = GI ? GI->GetSubsystem<UEconomyService>() : nullptr;
	if (!Pairs.IsValidIndex(SelectedIndex) || !Economy)
	{
		return;
	}
	const FDuplicateGearPair &Pair = Pairs[SelectedIndex];
	const FText Name = DuplicateGearBuild::DescribeCopy(*Inv, Pair.WeakerID, Pair.bRing);
	const bool bDone = Economy->ResolveDuplicate(DuplicateGearBuild::OwnerOf(this), Pair.WeakerID, bConvertToGold);
	StatusText = !bDone ? FText::Format(LOCTEXT("Failed", "Couldn't resolve {0}"), Name)
			   : bConvertToGold ? FText::Format(LOCTEXT("ToGold", "{0} converted to {1}"), Name, WoRGameNames::GetCurrencyName(UEconomyService::GetPlaceCurrency(this)))
								: FText::Format(LOCTEXT("Dismantled", "{0} dismantled"), Name);
	Refresh();
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetDuplicateString());
}

void UDuplicateGearWidget::HandleNextClicked()
{
	const UInventoryComponent *Inv = DuplicateGearBuild::InventoryOf(this);
	const int32 Count = Inv ? Inv->FindDuplicateGear().Num() : 0;
	SelectedIndex = Count > 0 ? (SelectedIndex + 1) % Count : 0;
	StatusText = FText::GetEmpty();
	Refresh();
}

void UDuplicateGearWidget::HandleGoldClicked()
{
	Resolve(/*bConvertToGold=*/true);
}

void UDuplicateGearWidget::HandleDismantleClicked()
{
	Resolve(/*bConvertToGold=*/false);
}

void UDuplicateGearWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

static FAutoConsoleCommandWithWorld GDuplicateGearUICommand(
	TEXT("wor.UI.Duplicates"),
	TEXT("PLACEHOLDER UI: convert the weaker copy of a duplicate weapon or adapter to Prisms (the run currency), or dismantle it (D-115)."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!UDuplicateGearWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Duplicates] wor.UI.Duplicates: no player controller"));
			}
		}));

#undef LOCTEXT_NAMESPACE
