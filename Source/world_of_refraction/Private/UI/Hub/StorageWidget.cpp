// StorageWidget.cpp

#include "UI/Hub/StorageWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/CharacterData.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GearGive.h"
#include "Inventory/ItemTier.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"

#define LOCTEXT_NAMESPACE "Storage"

namespace StorageStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace StorageBuild
{
	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = StorageStyle::FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(StorageStyle::RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = StorageStyle::FontSize;
		Text->SetFont(Font);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(StorageStyle::RowPadding));
		}
		return Button;
	}

	// TODO: gear and tier names are FStrings; FText with B-127 (D-061).
	FText GearLabel(const FString &Name, EItemTier Tier)
	{
		return FText::Format(LOCTEXT("GearLabel", "{0} ({1})"), FText::FromString(Name), FText::FromString(TierHelpers::GetTierName(Tier)));
	}

	FText StackLabel(const FCrystalId &Id, int32 Count)
	{
		return FText::Format(LOCTEXT("StackLabel", "{0} x{1}"), FText::FromString(ItemIdentity::GetDisplayName(Id)), Count);
	}
}

UStorageWidget *UStorageWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UStorageWidget *Widget = CreateWidget<UStorageWidget>(PC, UStorageWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(StorageStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UStorageWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(StorageStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		MemberText = StorageBuild::AddText(WidgetTree, Box);
		StorageBuild::AddButton(WidgetTree, Box, LOCTEXT("NextCharacter", "Next character"))
			->OnClicked.AddDynamic(this, &UStorageWidget::HandleNextCharacterClicked);
		ItemText = StorageBuild::AddText(WidgetTree, Box);
		StorageBuild::AddButton(WidgetTree, Box, LOCTEXT("NextItem", "Next item"))->OnClicked.AddDynamic(this, &UStorageWidget::HandleNextItemClicked);
		MoveOneButton = StorageBuild::AddButton(WidgetTree, Box, LOCTEXT("MoveOne", "Put / Take"));
		MoveOneButton->OnClicked.AddDynamic(this, &UStorageWidget::HandleMoveOneClicked);
		MoveStackButton = StorageBuild::AddButton(WidgetTree, Box, LOCTEXT("MoveStack", "Move the stack"));
		MoveStackButton->OnClicked.AddDynamic(this, &UStorageWidget::HandleMoveStackClicked);
		StatusLine = StorageBuild::AddText(WidgetTree, Box);
		StorageBuild::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))->OnClicked.AddDynamic(this, &UStorageWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UStorageWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

UPoolSubsystem *UStorageWidget::GetPool() const
{
	const UGameInstance *GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
}

TArray<UStorageWidget::FMember> UStorageWidget::GatherMembers() const
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

FName UStorageWidget::MemberKey() const
{
	const TArray<FMember> Members = GatherMembers();
	return Members.IsValidIndex(MemberIndex) ? Members[MemberIndex].Key : NAME_None;
}

TArray<UStorageWidget::FStorageChoice> UStorageWidget::GatherChoices() const
{
	TArray<FStorageChoice> Choices;
	const UPoolSubsystem *Pool = GetPool();
	if (!Pool)
	{
		return Choices;
	}
	const FName Member = MemberKey();
	const FName Chest = UPoolSubsystem::GetChestKey();
	// The member's gear and pouch first, then the chest's gear and the stored stacks.
	for (const bool bMine : {true, false})
	{
		const FName Side = bMine ? Member : Chest;
		if (Side.IsNone())
		{
			continue;
		}
		for (const FWeaponInventoryEntry &Weapon : Pool->GetOwnedWeapons())
		{
			if (Pool->GetGearOwner(Weapon.PersistentID) == Side && Weapon.Weapon)
			{
				FStorageChoice Choice;
				Choice.GearID = Weapon.PersistentID;
				Choice.Label = StorageBuild::GearLabel(Weapon.Weapon->Name, Weapon.Tier);
				Choice.bMine = bMine;
				Choices.Add(Choice);
			}
		}
		for (const FRingInventoryEntry &Ring : Pool->GetOwnedRings())
		{
			if (Pool->GetGearOwner(Ring.PersistentID) == Side && Ring.Ring)
			{
				FStorageChoice Choice;
				Choice.GearID = Ring.PersistentID;
				Choice.Label = StorageBuild::GearLabel(Ring.Ring->Name, Ring.Tier);
				Choice.bMine = bMine;
				Choices.Add(Choice);
			}
		}
		if (bMine)
		{
			for (const TPair<FCrystalId, int32> &Stack : Pool->GetCarriedItems(Member))
			{
				FStorageChoice Choice;
				Choice.Item = Stack.Key;
				Choice.Count = Stack.Value;
				Choice.Label = StorageBuild::StackLabel(Stack.Key, Stack.Value);
				Choice.bMine = true;
				Choices.Add(Choice);
			}
		}
		else
		{
			for (const TMap<FCrystalId, int32> *Storage : {&Pool->GetCrystals(), &Pool->GetStones()})
			{
				for (const TPair<FCrystalId, int32> &Stack : *Storage)
				{
					FStorageChoice Choice;
					Choice.Item = Stack.Key;
					Choice.Count = Stack.Value;
					Choice.Label = StorageBuild::StackLabel(Stack.Key, Stack.Value);
					Choice.bMine = false;
					Choices.Add(Choice);
				}
			}
		}
	}
	return Choices;
}

void UStorageWidget::Refresh()
{
	const TArray<FMember> Members = GatherMembers();
	MemberIndex = Members.IsEmpty() ? 0 : FMath::Clamp(MemberIndex, 0, Members.Num() - 1);
	const TArray<FStorageChoice> Choices = GatherChoices();
	SelectedIndex = Choices.IsEmpty() ? 0 : FMath::Clamp(SelectedIndex, 0, Choices.Num() - 1);

	MemberText->SetText(Members.IsEmpty() ? LOCTEXT("NoParty", "No party member at the chest")
										  : FText::Format(LOCTEXT("MemberRow", "{0} at the chest ({1} of {2} characters)"), Members[MemberIndex].Name,
														  MemberIndex + 1, Members.Num()));
	if (Choices.IsEmpty())
	{
		ItemText->SetText(LOCTEXT("Nothing", "Nothing to put or take"));
	}
	else
	{
		const FStorageChoice &Choice = Choices[SelectedIndex];
		ItemText->SetText(FText::Format(LOCTEXT("ItemRow", "{0}: {1} ({2} of {3}), {4}"),
										Choice.bMine ? LOCTEXT("Mine", "Mine") : LOCTEXT("InChest", "In the chest"), Choice.Label, SelectedIndex + 1,
										Choices.Num(), Choice.bMine ? LOCTEXT("ToChest", "put in the chest") : LOCTEXT("FromChest", "take")));
	}
	const bool bCanMove = !Choices.IsEmpty() && !Members.IsEmpty();
	MoveOneButton->SetIsEnabled(bCanMove);
	MoveStackButton->SetIsEnabled(bCanMove && !Choices[SelectedIndex].IsGear());
	StatusLine->SetText(StatusText);
}

FString UStorageWidget::GetStorageString() const
{
	FString Out = TEXT("[Storage] ") + MemberText->GetText().ToString() + TEXT("\n  ") + ItemText->GetText().ToString();
	if (!StatusText.IsEmpty())
	{
		Out += TEXT("\n  ") + StatusText.ToString();
	}
	return Out;
}

void UStorageWidget::MoveSelected(int32 Count)
{
	UPoolSubsystem *Pool = GetPool();
	const TArray<FStorageChoice> Choices = GatherChoices();
	if (!Pool || !Choices.IsValidIndex(SelectedIndex))
	{
		return;
	}
	const FStorageChoice Choice = Choices[SelectedIndex];
	const FName Member = MemberKey();
	FText Refusal;
	FText Moved = Choice.Label; // a partial stack move names what moved, not the whole stack
	if (Choice.IsGear())
	{
		Refusal = Choice.bMine ? WoRGive::TryPutInChest(GetWorld(), Member, Choice.GearID) : WoRGive::TryTakeFromChest(GetWorld(), Member, Choice.GearID);
	}
	else
	{
		// Into the pouch, a whole stack moves as far as its slot has room (at least one is tried, so a
		// full slot reports "No room").
		const int32 Room = Choice.bMine ? Choice.Count : FMath::Max(1, Pool->GetCarryRoom(Member, Choice.Item));
		const int32 Moving = FMath::Min3(Count, Choice.Count, Room);
		const bool bMoved = Choice.bMine ? Pool->MoveToStorage(Member, Choice.Item, Moving) : Pool->MoveToCarried(Member, Choice.Item, Moving);
		Refusal = bMoved ? FText::GetEmpty() : FText::Format(LOCTEXT("NoRoom", "No room for {0}: free a slot first"), Choice.Label);
		Moved = StorageBuild::StackLabel(Choice.Item, Moving);
	}
	StatusText = Refusal.IsEmpty() ? FText::Format(Choice.bMine ? LOCTEXT("Put", "Put {0} in the chest") : LOCTEXT("Took", "Took {0}"), Moved)
								   : Refusal;
	Refresh();
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetStorageString());
}

void UStorageWidget::HandleNextCharacterClicked()
{
	const int32 Count = GatherMembers().Num();
	MemberIndex = Count > 0 ? (MemberIndex + 1) % Count : 0;
	SelectedIndex = 0;
	StatusText = FText::GetEmpty();
	Refresh();
}

void UStorageWidget::HandleNextItemClicked()
{
	const int32 Count = GatherChoices().Num();
	SelectedIndex = Count > 0 ? (SelectedIndex + 1) % Count : 0;
	StatusText = FText::GetEmpty();
	Refresh();
}

void UStorageWidget::HandleMoveOneClicked() { MoveSelected(1); }

void UStorageWidget::HandleMoveStackClicked() { MoveSelected(MAX_int32); }

void UStorageWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

static FAutoConsoleCommandWithWorld GStorageUICommand(
	TEXT("wor.UI.Storage"),
	TEXT("PLACEHOLDER UI: the shared chest: put a character's unworn gear or pouch stacks in, take the chest's gear or items out (B-324)."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!UStorageWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Storage] wor.UI.Storage: no player controller"));
			}
		}));

#undef LOCTEXT_NAMESPACE
