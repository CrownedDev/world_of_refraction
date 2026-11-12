// ItemSlotUpgradeWidget.cpp

#include "UI/Hub/ItemSlotUpgradeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/CharacterData.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryConstants.h"
#include "Inventory/ItemSlotUpgrade.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "ItemSlotUpgrade"

namespace ItemSlotUpgradeStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace ItemSlotUpgradeBuild
{
	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = ItemSlotUpgradeStyle::FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(ItemSlotUpgradeStyle::RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = ItemSlotUpgradeStyle::FontSize;
		Text->SetFont(Font);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(ItemSlotUpgradeStyle::RowPadding));
		}
		return Button;
	}
}

UItemSlotUpgradeWidget *UItemSlotUpgradeWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UItemSlotUpgradeWidget *Widget = CreateWidget<UItemSlotUpgradeWidget>(PC, UItemSlotUpgradeWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(ItemSlotUpgradeStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UItemSlotUpgradeWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(ItemSlotUpgradeStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		MemberText = ItemSlotUpgradeBuild::AddText(WidgetTree, Box);
		ItemSlotUpgradeBuild::AddButton(WidgetTree, Box, LOCTEXT("Next", "Next character"))
			->OnClicked.AddDynamic(this, &UItemSlotUpgradeWidget::HandleNextClicked);
		BuyButton = ItemSlotUpgradeBuild::AddButton(WidgetTree, Box, LOCTEXT("Buy", "Buy one item slot"));
		BuyButton->OnClicked.AddDynamic(this, &UItemSlotUpgradeWidget::HandleBuyClicked);
		StatusLine = ItemSlotUpgradeBuild::AddText(WidgetTree, Box);
		ItemSlotUpgradeBuild::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))
			->OnClicked.AddDynamic(this, &UItemSlotUpgradeWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UItemSlotUpgradeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

UPoolSubsystem *UItemSlotUpgradeWidget::GetPool() const
{
	const UGameInstance *GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
}

TArray<UItemSlotUpgradeWidget::FMember> UItemSlotUpgradeWidget::GatherMembers() const
{
	TArray<FMember> Members;
	const UGameInstance *GI = GetGameInstance();
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	if (!Party)
	{
		return Members;
	}
	for (const FPartyMember &Member : Party->Members)
	{
		if (Member.CharacterData)
		{
			Members.Add({UPoolSubsystem::GetCharacterKey(Member), FText::FromString(Member.CharacterData->Name)});
		}
	}
	return Members;
}

void UItemSlotUpgradeWidget::Refresh()
{
	const TArray<FMember> Members = GatherMembers();
	MemberIndex = Members.IsEmpty() ? 0 : MemberIndex % Members.Num();
	const UPoolSubsystem *Pool = GetPool();
	const UCurrencyComponent *Wallet = CurrencyWallet::FindFor(GetOwningPlayerPawn());
	const FText Currency = WoRGameNames::GetCurrencyName(ItemSlotUpgrade::GetCurrency());
	const bool bHasMember = Pool && !Members.IsEmpty();
	const int32 Slots = bHasMember ? Pool->GetItemSlotCount(Members[MemberIndex].Key) : 0;
	if (MemberText)
	{
		if (!bHasMember)
		{
			MemberText->SetText(LOCTEXT("NoParty", "No party member to upgrade."));
		}
		else if (ItemSlotUpgrade::CanGrow(Slots))
		{
			MemberText->SetText(FText::Format(LOCTEXT("MemberRow", "{0}: {1} of {2} item slots. Next slot: {3} {4}. You have {5} {4}."),
											  Members[MemberIndex].Name, FText::AsNumber(Slots), FText::AsNumber(InventoryConstants::ITEM_SLOTS_MAX),
											  FText::AsNumber(ItemSlotUpgrade::GetPrice(Slots)), Currency,
											  FText::AsNumber(Wallet ? Wallet->GetBalance(ItemSlotUpgrade::GetCurrency()) : 0)));
		}
		else
		{
			MemberText->SetText(FText::Format(LOCTEXT("MemberFull", "{0}: {1} of {2} item slots (full)."), Members[MemberIndex].Name,
											  FText::AsNumber(Slots), FText::AsNumber(InventoryConstants::ITEM_SLOTS_MAX)));
		}
	}
	if (BuyButton)
	{
		BuyButton->SetIsEnabled(bHasMember && ItemSlotUpgrade::CanGrow(Slots) && ItemSlotUpgrade::IsAvailableHere(this));
	}
	if (StatusLine)
	{
		StatusLine->SetText(ItemSlotUpgrade::IsAvailableHere(this) ? Status
																	: LOCTEXT("InRunStatus", "Item slots are sold in the hub, not during a run."));
	}
}

FString UItemSlotUpgradeWidget::GetItemSlotUpgradeString() const
{
	return FString::Printf(TEXT("[ItemSlotUpgrade] %s | %s"), MemberText ? *MemberText->GetText().ToString() : TEXT("-"),
						   StatusLine ? *StatusLine->GetText().ToString() : TEXT("-"));
}

void UItemSlotUpgradeWidget::HandleNextClicked()
{
	++MemberIndex;
	Status = FText::GetEmpty();
	Refresh();
}

void UItemSlotUpgradeWidget::HandleBuyClicked()
{
	const TArray<FMember> Members = GatherMembers();
	UPoolSubsystem *Pool = GetPool();
	if (!Pool || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	FText Refusal;
	if (ItemSlotUpgrade::TryBuy(this, *Pool, Members[MemberIndex].Key, CurrencyWallet::FindFor(GetOwningPlayerPawn()), Refusal))
	{
		Status = FText::Format(LOCTEXT("Bought", "{0}'s pouch now holds {1} item types."), Members[MemberIndex].Name,
							   FText::AsNumber(Pool->GetItemSlotCount(Members[MemberIndex].Key)));
	}
	else
	{
		Status = Refusal;
	}
	Refresh();
	UE_LOG(LogWoR, Log, TEXT("%s"), *GetItemSlotUpgradeString());
}

void UItemSlotUpgradeWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

static FAutoConsoleCommandWithWorld GItemSlotUpgradeUICommand(
	TEXT("wor.UI.ItemSlots"),
	TEXT("PLACEHOLDER UI (B-239): the hub vendor's item slot upgrades (one more slot in a character's pouch, for hub Gold)."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!UItemSlotUpgradeWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogWoR, Warning, TEXT("[ItemSlotUpgrade] wor.UI.ItemSlots: no player controller"));
			}
		}));

#undef LOCTEXT_NAMESPACE
