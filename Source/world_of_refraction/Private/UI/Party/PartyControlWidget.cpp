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
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Player/WoRPlayerState.h"
#include "Pool/PoolSubsystem.h"
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

	const UPoolSubsystem *PoolOf(const UUserWidget *Widget)
	{
		const UGameInstance *GI = Widget ? Widget->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	}

	/** The armoury piece after Current; after the last one comes none (an invalid ID), then the first. */
	template <typename EntryType>
	FGuid NextArmouryPiece(const TArray<EntryType> &Armoury, const FGuid &Current)
	{
		const int32 Index = Armoury.IndexOfByPredicate([&Current](const EntryType &Entry) { return Entry.PersistentID == Current; });
		const int32 Next = Index == INDEX_NONE ? 0 : Index + 1;
		return Armoury.IsValidIndex(Next) ? Armoury[Next].PersistentID : FGuid();
	}

	FText RunGearName(const UEquipmentDataBase *Piece)
	{
		return Piece ? FText::FromString(Piece->Name) : LOCTEXT("NoRunGear", "none");
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

		for (int32 SlotIndex = 0; SlotIndex < PartyConstants::MAX_PARTY_MEMBERS; ++SlotIndex)
		{
			UButton *WeaponButton = nullptr;
			RunWeaponTexts.Add(PartyControlBuild::AddButton(WidgetTree, Box, WeaponButton));
			RunWeaponButtons.Add(WeaponButton);
			UButton *RingButton = nullptr;
			RunRingTexts.Add(PartyControlBuild::AddButton(WidgetTree, Box, RingButton));
			RunRingButtons.Add(RingButton);
		}
		// The leader is never a run-only member, so slot 0 has no handlers.
		RunWeaponButtons[1]->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleSlot1RunWeaponClicked);
		RunWeaponButtons[2]->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleSlot2RunWeaponClicked);
		RunRingButtons[1]->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleSlot1RunRingClicked);
		RunRingButtons[2]->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleSlot2RunRingClicked);

		UButton *FallButton = nullptr;
		FallSettingText = PartyControlBuild::AddButton(WidgetTree, Box, FallButton);
		FallButton->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleFallSettingClicked);

		UButton *ReviveButton = nullptr;
		PartyControlBuild::AddButton(WidgetTree, Box, ReviveButton)->SetText(LOCTEXT("ReviveFallen", "Rest: revive the fallen"));
		ReviveButton->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleReviveFallenClicked);

		UButton *RepairButton = nullptr;
		RepairGearText = PartyControlBuild::AddButton(WidgetTree, Box, RepairButton);
		RepairButton->OnClicked.AddDynamic(this, &UPartyControlWidget::HandleRepairGearClicked);

		GearClashText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GearClashes"));
		FSlateFontInfo ClashFont = GearClashText->GetFont();
		ClashFont.Size = PartyControlStyle::FontSize;
		GearClashText->SetFont(ClashFont);
		if (UVerticalBoxSlot *ClashSlot = Box->AddChildToVerticalBox(GearClashText))
		{
			ClashSlot->SetPadding(FMargin(PartyControlStyle::RowPadding));
		}

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

	const UPoolSubsystem *Pool = PartyControlBuild::PoolOf(this);
	for (int32 SlotIndex = 0; SlotIndex < RunWeaponTexts.Num(); ++SlotIndex)
	{
		const FPartyMember *Member = Party && Party->Members.IsValidIndex(SlotIndex) ? &Party->Members[SlotIndex] : nullptr;
		const bool bShow = Member && Member->bRunOnly && Pool;
		RunWeaponButtons[SlotIndex]->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		RunRingButtons[SlotIndex]->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bShow)
		{
			continue;
		}
		const FText Name = Member->CharacterData ? FText::FromString(Member->CharacterData->Name) : LOCTEXT("NoData", "<no data>");
		const FWeaponInventoryEntry *Weapon = Pool->GetOwnedWeapons().FindByPredicate(
			[Member](const FWeaponInventoryEntry &Entry) { return Entry.PersistentID == Member->RunWeapon; });
		const FRingInventoryEntry *Ring = Pool->GetOwnedRings().FindByPredicate(
			[Member](const FRingInventoryEntry &Entry) { return Entry.PersistentID == Member->RunRing; });
		RunWeaponTexts[SlotIndex]->SetText(FText::Format(LOCTEXT("RunWeaponRow", "{0}'s weapon (from the next fight): {1}"), Name,
														 PartyControlBuild::RunGearName(Weapon ? Weapon->Weapon : nullptr)));
		RunRingTexts[SlotIndex]->SetText(FText::Format(LOCTEXT("RunRingRow", "{0}'s ring (from the next fight): {1}"), Name,
													   PartyControlBuild::RunGearName(Ring ? Ring->Ring : nullptr)));
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

	if (GearClashText)
	{
		TArray<FText> Lines;
		for (const FPoolGearClash &Clash : Pool ? Pool->GetClashes() : TArray<FPoolGearClash>())
		{
			Lines.Add(FText::Format(LOCTEXT("GearClashRow", "{0} is worn by {1}: taken out of {2}'s loadout"),
									Clash.ItemName, Clash.WearerName, Clash.LoserName));
		}
		GearClashText->SetText(Lines.Num() > 0
								   ? FText::Join(FText::FromString(TEXT("\n")), Lines)
								   : LOCTEXT("NoGearClash", "Shared gear: each piece has one wearer"));
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
	for (int32 SlotIndex = 0; SlotIndex < RunWeaponTexts.Num(); ++SlotIndex)
	{
		if (RunWeaponButtons[SlotIndex]->GetVisibility() != ESlateVisibility::Collapsed)
		{
			Out += FString::Printf(TEXT("\n  [%d] %s\n  [%d] %s"), SlotIndex, *RunWeaponTexts[SlotIndex]->GetText().ToString(),
								   SlotIndex, *RunRingTexts[SlotIndex]->GetText().ToString());
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
	if (GearClashText)
	{
		Out += TEXT("\n  ") + GearClashText->GetText().ToString();
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

void UPartyControlWidget::CycleRunGear(int32 SlotIndex, bool bWeapon)
{
	UPartySessionSubsystem *PartySession = PartyControlBuild::PartySessionOf(this);
	const UPoolSubsystem *Pool = PartyControlBuild::PoolOf(this);
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	if (!Pool || !Party || !Party->Members.IsValidIndex(SlotIndex))
	{
		return;
	}
	// A piece another member wears is offered too; the one-wearer rule (B-091) settles it at the next spawn.
	const FPartyMember &Member = Party->Members[SlotIndex];
	const FGuid Weapon = bWeapon ? PartyControlBuild::NextArmouryPiece(Pool->GetOwnedWeapons(), Member.RunWeapon) : Member.RunWeapon;
	const FGuid Ring = bWeapon ? Member.RunRing : PartyControlBuild::NextArmouryPiece(Pool->GetOwnedRings(), Member.RunRing);
	PartySession->SetRunMemberGear(SlotIndex, Weapon, Ring); // OnPartyChanged refreshes this screen
}

void UPartyControlWidget::HandleSlot1RunWeaponClicked() { CycleRunGear(1, true); }

void UPartyControlWidget::HandleSlot2RunWeaponClicked() { CycleRunGear(2, true); }

void UPartyControlWidget::HandleSlot1RunRingClicked() { CycleRunGear(1, false); }

void UPartyControlWidget::HandleSlot2RunRingClicked() { CycleRunGear(2, false); }

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
