// PartyControlWidget.cpp

#include "UI/Party/PartyControlWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/CharacterData.h"
#include "Combat/Grid/CombatGridConstants.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Party/PartySessionSubsystem.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Durability/GearRepair.h"
#include "Player/WoRPlayerState.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "PartyControl"

namespace PartyControlStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace PartyControlBuild
{
	UTextBlock *AddButton(UWidgetTree *Tree, UVerticalBox *Box, UButton *&OutButton)
	{
		OutButton = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = PartyControlStyle::FontSize;
		Label->SetFont(Font);
		OutButton->AddChild(Label);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(OutButton))
		{
			BoxSlot->SetPadding(FMargin(PartyControlStyle::RowPadding));
		}
		return Label;
	}

	UPartySessionSubsystem *PartySessionOf(const UUserWidget *Widget)
	{
		const UGameInstance *GI = Widget ? Widget->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	}
}

UPartyControlWidget *UPartyControlWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UPartyControlWidget *Widget = CreateWidget<UPartyControlWidget>(PC, UPartyControlWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(PartyControlStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UPartyControlWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(PartyControlStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		for (int32 SlotIndex = 0; SlotIndex < PartyConstants::MAX_PARTY_MEMBERS; ++SlotIndex)
		{
			UButton *Button = nullptr;
			SlotTexts.Add(PartyControlBuild::AddButton(WidgetTree, Box, Button));
			SlotButtons.Add(Button);
		}
		SlotButtons[1]->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleSlot1Clicked);
		SlotButtons[2]->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleSlot2Clicked);

		for (int32 SlotIndex = 0; SlotIndex < PartyConstants::MAX_PARTY_MEMBERS; ++SlotIndex)
		{
			UButton *Button = nullptr;
			FormationTexts.Add(PartyControlBuild::AddButton(WidgetTree, Box, Button));
			FormationButtons.Add(Button);
		}
		FormationButtons[0]->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleSlot0FormationClicked);
		FormationButtons[1]->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleSlot1FormationClicked);
		FormationButtons[2]->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleSlot2FormationClicked);

		UButton *FallButton = nullptr;
		FallSettingText = PartyControlBuild::AddButton(WidgetTree, Box, FallButton);
		FallButton->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleFallSettingClicked);

		UButton *ReviveButton = nullptr;
		PartyControlBuild::AddButton(WidgetTree, Box, ReviveButton)->SetText(LOCTEXT("ReviveFallen", "Rest: revive the fallen"));
		ReviveButton->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleReviveFallenClicked);

		UButton *RepairButton = nullptr;
		RepairGearText = PartyControlBuild::AddButton(WidgetTree, Box, RepairButton);
		RepairButton->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleRepairGearClicked);

		UButton *CloseButton = nullptr;
		PartyControlBuild::AddButton(WidgetTree, Box, CloseButton)->SetText(LOCTEXT("Close", "Close"));
		CloseButton->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UPartyControlWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UPartySessionSubsystem *PartySession = PartyControlBuild::PartySessionOf(this))
	{
		PartySession->OnPartyChanged.AddUniqueDynamic(this, &UPartyControlWidget::HandlePartyChanged);
	}
	Refresh();
}

void UPartyControlWidget::NativeDestruct()
{
	if (UPartySessionSubsystem *PartySession = PartyControlBuild::PartySessionOf(this))
	{
		PartySession->OnPartyChanged.RemoveDynamic(this, &UPartyControlWidget::HandlePartyChanged);
	}
	Super::NativeDestruct();
}

void UPartyControlWidget::Refresh()
{
	const UPartySessionSubsystem *PartySession = PartyControlBuild::PartySessionOf(this);
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;

	for (int32 SlotIndex = 0; SlotIndex < SlotTexts.Num(); ++SlotIndex)
	{
		const FPartyMember *Member = Party && Party->Members.IsValidIndex(SlotIndex) ? &Party->Members[SlotIndex] : nullptr;
		SlotButtons[SlotIndex]->SetVisibility(Member ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		SlotButtons[SlotIndex]->SetIsEnabled(SlotIndex != PartyConstants::LEADER_SLOT);
		if (!Member)
		{
			continue;
		}
		const FText Name = Member->CharacterData ? FText::FromString(Member->CharacterData->Name) : LOCTEXT("NoData", "<no data>");
		const FText Control = SlotIndex == PartyConstants::LEADER_SLOT ? LOCTEXT("Leader", "You (leader)")
							  : Member->bPlayerControlled       ? LOCTEXT("Player", "You")
																: LOCTEXT("AI", "AI");
		const FText Fallen = Member->IsFallen() ? LOCTEXT("FallenTag", " (fallen)") : FText::GetEmpty();
		SlotTexts[SlotIndex]->SetText(FText::Format(LOCTEXT("SlotRow", "{0}{2}: played by {1}"), Name, Control, Fallen));
	}

	for (int32 SlotIndex = 0; SlotIndex < FormationTexts.Num(); ++SlotIndex)
	{
		const FPartyMember *Member = Party && Party->Members.IsValidIndex(SlotIndex) ? &Party->Members[SlotIndex] : nullptr;
		FormationButtons[SlotIndex]->SetVisibility(Member ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!Member)
		{
			continue;
		}
		const FText Name = Member->CharacterData ? FText::FromString(Member->CharacterData->Name) : LOCTEXT("NoData", "<no data>");
		const FText Cell = Member->HasFormation()
							   ? FText::Format(LOCTEXT("Cell", "{0}, column {1}"),
											   WoRGameNames::GetCombatRowName(Member->PreferredRow),
											   FText::AsNumber(Member->PreferredColumn))
							   : LOCTEXT("NoCell", "auto");
		FormationTexts[SlotIndex]->SetText(FText::Format(LOCTEXT("FormationRow", "{0} starts: {1}"), Name, Cell));
	}

	if (FallSettingText)
	{
		const bool bLetAIFinish = PartySession && PartySession->GetLetAIFinish();
		FallSettingText->SetText(FText::Format(LOCTEXT("FallRow", "When your fighters fall: {0}"),
											   bLetAIFinish ? LOCTEXT("LetAIFinish", "Let AI finish") : LOCTEXT("TakeOver", "Take over")));
	}

	if (RepairGearText)
	{
		const int32 RepairGold = Party ? GearRepair::GetPartyRepairCost(*Party) : 0;
		RepairGearText->SetText(RepairGold > 0
									? FText::Format(LOCTEXT("RepairGear", "Rest: repair gear ({0} Gold)"), RepairGold)
									: LOCTEXT("NothingToRepair", "Rest: no gear to repair"));
	}
}

FString UPartyControlWidget::GetPartyControlString() const
{
	FString Out = TEXT("[PartyControl]");
	for (int32 SlotIndex = 0; SlotIndex < SlotTexts.Num(); ++SlotIndex)
	{
		if (SlotButtons[SlotIndex]->GetVisibility() != ESlateVisibility::Collapsed)
		{
			Out += FString::Printf(TEXT("\n  [%d] %s"), SlotIndex, *SlotTexts[SlotIndex]->GetText().ToString());
		}
	}
	for (int32 SlotIndex = 0; SlotIndex < FormationTexts.Num(); ++SlotIndex)
	{
		if (FormationButtons[SlotIndex]->GetVisibility() != ESlateVisibility::Collapsed)
		{
			Out += FString::Printf(TEXT("\n  [%d] %s"), SlotIndex, *FormationTexts[SlotIndex]->GetText().ToString());
		}
	}
	if (FallSettingText)
	{
		Out += TEXT("\n  ") + FallSettingText->GetText().ToString();
	}
	if (RepairGearText)
	{
		Out += TEXT("\n  ") + RepairGearText->GetText().ToString();
	}
	return Out;
}

void UPartyControlWidget::ToggleSlot(int32 SlotIndex)
{
	if (UPartySessionSubsystem *PartySession = PartyControlBuild::PartySessionOf(this))
	{
		PartySession->SetMemberPlayerControlled(SlotIndex, !PartySession->IsMemberPlayerControlled(SlotIndex));
	}
}

void UPartyControlWidget::CycleFormation(int32 SlotIndex)
{
	UPartySessionSubsystem *PartySession = PartyControlBuild::PartySessionOf(this);
	if (!PartySession)
	{
		return;
	}
	// Unplaced starts the cycle at Back/0; a placed member steps to the next cell.
	ECombatRow Row = ECombatRow::Back;
	int32 Column = 0;
	int32 CellIndex = 0;
	if (PartySession->GetMemberFormation(SlotIndex, Row, Column))
	{
		const int32 Current = static_cast<int32>(Row) * CombatGridConstants::GRID_COLUMNS + Column;
		CellIndex = (Current + 1) % CombatGridConstants::POSITIONS_PER_TEAM;
	}
	PartySession->SetMemberFormation(SlotIndex, static_cast<ECombatRow>(CellIndex / CombatGridConstants::GRID_COLUMNS),
									 CellIndex % CombatGridConstants::GRID_COLUMNS);
}

void UPartyControlWidget::HandleSlot0FormationClicked() { CycleFormation(0); }

void UPartyControlWidget::HandleSlot1FormationClicked() { CycleFormation(1); }

void UPartyControlWidget::HandleSlot2FormationClicked() { CycleFormation(2); }

void UPartyControlWidget::HandleSlot1Clicked() { ToggleSlot(1); }

void UPartyControlWidget::HandleSlot2Clicked() { ToggleSlot(2); }

void UPartyControlWidget::HandleFallSettingClicked()
{
	if (UPartySessionSubsystem *PartySession = PartyControlBuild::PartySessionOf(this))
	{
		PartySession->SetLetAIFinish(!PartySession->GetLetAIFinish());
	}
}

void UPartyControlWidget::HandleReviveFallenClicked()
{
	if (UPartySessionSubsystem *PartySession = PartyControlBuild::PartySessionOf(this))
	{
		PartySession->ReviveFallenMembers(); // OnPartyChanged refreshes this screen
	}
}

void UPartyControlWidget::HandleRepairGearClicked()
{
	UPartySessionSubsystem *PartySession = PartyControlBuild::PartySessionOf(this);
	UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	const AWoRPlayerState *PlayerState = GetOwningPlayerState<AWoRPlayerState>();
	UCurrencyComponent *Wallet = PlayerState ? PlayerState->GetWallet() : nullptr;
	int32 Spent = 0;
	if (!Party || !Wallet || !GearRepair::RepairParty(*Party, *Wallet, Spent))
	{
		UE_LOG(LogWoR, Display, TEXT("[PartyControl] Repair: nothing repaired (nothing worn, or not enough Gold)"));
	}
	Refresh(); // a repair changes no party membership, so OnPartyChanged doesn't fire
}

void UPartyControlWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

void UPartyControlWidget::HandlePartyChanged(UParty *Party)
{
	Refresh();
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetPartyControlString());
}

static FAutoConsoleCommandWithWorld GPartyControlUICommand(
	TEXT("wor.UI.PartyControl"),
	TEXT("PLACEHOLDER UI: open the party control screen (who plays each member, and what happens when your fighters fall)."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!UPartyControlWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogTemp, Warning, TEXT("[PartyControl] wor.UI.PartyControl: no player controller"));
			}
		}));

#undef LOCTEXT_NAMESPACE
