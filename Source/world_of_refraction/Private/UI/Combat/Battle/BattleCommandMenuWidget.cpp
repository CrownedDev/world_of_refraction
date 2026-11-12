// BattleCommandMenuWidget.cpp

#include "UI/Combat/Battle/BattleCommandMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Combat/CombatPlayerController.h"
#include "Combat/WoRFightSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Equipment/Crystals/CrystalManager.h"
#include "GameFramework/Actor.h"
#include "UI/Combat/Battle/BattleHUDButton.h"
#include "UI/Combat/Battle/BattleHUDView.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "BattleCommandMenu"

namespace BattleCommandMenuStyle
{
	// TODO: style pass (placeholder until the CommonUI screens, D-024). Slate units, DPI-scaled.
	constexpr float MenuWidth = 340.0f;
	constexpr float Padding = 8.0f;
	constexpr float RowPadding = 2.0f;
	constexpr int32 TitleFontSize = 16;
	constexpr int32 RowFontSize = 14;
	const FLinearColor BackgroundColor(0.0f, 0.0f, 0.0f, 0.7f);
	const FLinearColor HeaderColor(0.7f, 0.7f, 0.7f);
	const FLinearColor DurabilityColor(0.85f, 0.85f, 0.7f); // B-391c
}

void UBattleMenuRowRelay::HandleClicked()
{
	if (UBattleCommandMenuWidget *Owner = Menu.Get())
	{
		Owner->PressRow(RowIndex);
	}
}

void UBattleMenuRowRelay::HandleHovered()
{
	if (UBattleCommandMenuWidget *Owner = Menu.Get())
	{
		Owner->HoverRow(RowIndex);
	}
}

// ========================================
// LIFECYCLE
// ========================================

void UBattleCommandMenuWidget::Initialise()
{
	if (bBound)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[BattleCommandMenu] Initialise called twice; ignoring"));
		return;
	}
	ClearFlags(RF_Transactional); // TransBuffer rule, as the old menu

	UCombatCommandMenuSubsystem *Menu = WoRFight::Get<UCombatCommandMenuSubsystem>(this);
	if (!Menu)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[BattleCommandMenu] No command menu subsystem: the menu stays empty"));
		return;
	}
	BoundMenu = Menu;
	Menu->OnCommandMenuReady.AddDynamic(this, &UBattleCommandMenuWidget::HandleMenuReady);
	Menu->OnCommandMenuClosed.AddDynamic(this, &UBattleCommandMenuWidget::HandleMenuClosed);
	// B-391c: the durability header follows per-cast wear and breaks, as the old header (a run-wide service; the weak
	// pointer is only for the unbind).
	if (UGameInstance *GameInstance = GetGameInstance())
	{
		if (UCrystalManager *Crystals = GameInstance->GetSubsystem<UCrystalManager>())
		{
			BoundCrystals = Crystals;
			Crystals->OnCrystalDurabilityChanged.AddDynamic(this, &UBattleCommandMenuWidget::HandleCrystalDurabilityChanged);
			Crystals->OnCrystalBroken.AddDynamic(this, &UBattleCommandMenuWidget::HandleCrystalBroken);
		}
	}
	bBound = true;

	// Replay: the first player turn opens the menu inside InitializeCombat, before the HUD is built.
	if (Menu->IsOpen())
	{
		Menu->RefreshMenu();
	}
}

void UBattleCommandMenuWidget::Teardown()
{
	if (!bBound)
	{
		return;
	}
	if (UCombatCommandMenuSubsystem *Menu = BoundMenu.Get())
	{
		Menu->OnCommandMenuReady.RemoveDynamic(this, &UBattleCommandMenuWidget::HandleMenuReady);
		Menu->OnCommandMenuClosed.RemoveDynamic(this, &UBattleCommandMenuWidget::HandleMenuClosed);
	}
	BoundMenu.Reset();
	if (UCrystalManager *Crystals = BoundCrystals.Get())
	{
		Crystals->OnCrystalDurabilityChanged.RemoveDynamic(this, &UBattleCommandMenuWidget::HandleCrystalDurabilityChanged);
		Crystals->OnCrystalBroken.RemoveDynamic(this, &UBattleCommandMenuWidget::HandleCrystalBroken);
	}
	BoundCrystals.Reset();
	bBound = false;
}

void UBattleCommandMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	PushToWidgets();
}

void UBattleCommandMenuWidget::NativeDestruct()
{
	Teardown();
	Super::NativeDestruct();
}

TSharedRef<SWidget> UBattleCommandMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
		Background->SetBrushColor(BattleCommandMenuStyle::BackgroundColor);
		Background->SetPadding(FMargin(BattleCommandMenuStyle::Padding));
		WidgetTree->RootWidget = Background;

		USizeBox *Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Width->SetWidthOverride(BattleCommandMenuStyle::MenuWidth);
		Background->SetContent(Width);
		UVerticalBox *List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Width->AddChild(List);

		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		FSlateFontInfo TitleFont = TitleText->GetFont();
		TitleFont.Size = BattleCommandMenuStyle::TitleFontSize;
		TitleText->SetFont(TitleFont);
		if (UVerticalBoxSlot *TitleSlot = List->AddChildToVerticalBox(TitleText))
		{
			TitleSlot->SetPadding(FMargin(BattleCommandMenuStyle::RowPadding));
		}

		// B-391c: the durability header, under the title (the old header sat at the top of the menu).
		DurabilityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Durability"));
		FSlateFontInfo DurabilityFont = DurabilityText->GetFont();
		DurabilityFont.Size = BattleCommandMenuStyle::RowFontSize;
		DurabilityText->SetFont(DurabilityFont);
		DurabilityText->SetColorAndOpacity(FSlateColor(BattleCommandMenuStyle::DurabilityColor));
		if (UVerticalBoxSlot *DurabilitySlot = List->AddChildToVerticalBox(DurabilityText))
		{
			DurabilitySlot->SetPadding(FMargin(BattleCommandMenuStyle::RowPadding));
		}

		for (int32 Index = 0; Index < MAX_ROWS; ++Index)
		{
			UButton *Button = WidgetTree->ConstructWidget<UBattleHUDButton>(UBattleHUDButton::StaticClass());
			UTextBlock *Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			FSlateFontInfo RowFont = Label->GetFont();
			RowFont.Size = BattleCommandMenuStyle::RowFontSize;
			Label->SetFont(RowFont);
			Button->AddChild(Label);
			if (UVerticalBoxSlot *RowSlot = List->AddChildToVerticalBox(Button))
			{
				RowSlot->SetPadding(FMargin(BattleCommandMenuStyle::RowPadding));
			}
			UBattleMenuRowRelay *Relay = NewObject<UBattleMenuRowRelay>(this);
			Relay->RowIndex = Index;
			Relay->Menu = this;
			Button->OnClicked.AddDynamic(Relay, &UBattleMenuRowRelay::HandleClicked);
			Button->OnHovered.AddDynamic(Relay, &UBattleMenuRowRelay::HandleHovered); // B-400b
			RowButtons.Add(Button);
			RowTexts.Add(Label);
			RowRelays.Add(Relay);
		}
	}
	return Super::RebuildWidget();
}

// ========================================
// STATE (plain data; runs unbuilt)
// ========================================

void UBattleCommandMenuWidget::HandleMenuReady(const TArray<FPieMenuButtonData> &Buttons)
{
	const UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	const bool bAtMainMenu = Menu && Menu->IsAtMainMenu();
	Rows = Buttons;
	if (Rows.Num() > MAX_ROWS)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[BattleCommandMenu] %d buttons but %d rows: the rest are not shown"), Rows.Num(), MAX_ROWS);
		Rows.SetNum(MAX_ROWS);
	}

	TArray<EPieMenuCategory> Categories;
	TArray<bool> Enabled;
	for (const FPieMenuButtonData &Row : Rows)
	{
		Categories.Add(Row.Category);
		Enabled.Add(Row.bEnabled && Row.Category != EPieMenuCategory::SectionHeader);
	}
	const TArray<int32> KeyForRow = BattleHUDView::AssignMenuKeys(Categories, Enabled, bAtMainMenu);
	const TArray<FCombatMenuKey> &Keys = ACombatPlayerController::GetMenuKeys();

	RowLabels.Reset();
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FPieMenuButtonData &Row = Rows[Index];
		const FText Label = Row.Description.IsEmpty()
								? Row.DisplayName
								: FText::Format(LOCTEXT("RowWithDescription", "{0}  ({1})"), Row.DisplayName, Row.Description);
		const FCombatMenuKey *Key = Keys.IsValidIndex(KeyForRow[Index]) ? &Keys[KeyForRow[Index]] : nullptr;
		RowLabels.Add(Key ? BattleHUDView::WithKeyHint(Label, Key->Keyboard, Key->Gamepad) : Label);
	}

	const AActor *Acting = Menu ? Menu->GetCurrentActor() : nullptr;
	Title = Acting ? FText::Format(LOCTEXT("Title", "{0}'s turn"), BattleHUDView::GetCombatantName(Acting)) : FText::GetEmpty();
	RefreshDurability(); // B-391c: per menu open, as the old header
	bShowing = true;
	PushToWidgets();
}

void UBattleCommandMenuWidget::HandleMenuClosed()
{
	Rows.Reset();
	RowLabels.Reset();
	DurabilityLines.Reset();
	bShowing = false;
	PushToWidgets();
}

void UBattleCommandMenuWidget::RefreshDurability()
{
	const UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	DurabilityLines = BattleHUDView::MakeDurabilityLines(BattleHUDView::ReadDurability(Menu ? Menu->GetCurrentActor() : nullptr));
}

void UBattleCommandMenuWidget::HandleCrystalDurabilityChanged(AActor *Actor, UObject *Holder, int32 NewDurability, int32 MaxDurability)
{
	const UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	if (!bShowing || !Menu || Actor != Menu->GetCurrentActor())
	{
		return;
	}
	RefreshDurability();
	PushToWidgets();
}

void UBattleCommandMenuWidget::HandleCrystalBroken(AActor *Actor, UObject *Holder, FBrokenCrystalPayload Payload)
{
	// A break can switch the active ring (the manager switches first, then broadcasts), so re-read everything.
	HandleCrystalDurabilityChanged(Actor, Holder, 0, 0);
}

void UBattleCommandMenuWidget::PressRow(int32 RowIndex)
{
	if (!Rows.IsValidIndex(RowIndex))
	{
		return;
	}
	const FPieMenuButtonData Pressed = Rows[RowIndex]; // HandleSelection can rebroadcast and replace Rows
	if (!Pressed.bEnabled || Pressed.Category == EPieMenuCategory::SectionHeader)
	{
		return;
	}
	if (UCombatCommandMenuSubsystem *Menu = BoundMenu.Get())
	{
		Menu->HandleSelection(Pressed);
	}
}

void UBattleCommandMenuWidget::HoverRow(int32 RowIndex)
{
	AActor *Target = GetRowTarget(RowIndex);
	UCombatCommandMenuSubsystem *Menu = BoundMenu.Get();
	if (Target && Menu && Rows[RowIndex].bEnabled)
	{
		Menu->HighlightTarget(Target); // ignored unless its open picker offers this target
	}
}

AActor *UBattleCommandMenuWidget::GetRowTarget(int32 RowIndex) const
{
	if (!Rows.IsValidIndex(RowIndex))
	{
		return nullptr;
	}
	// A per-actor target button; a group confirm carries Targets instead (as UCombatCommandButtonWidget, B-400).
	const FPieMenuButtonData &Row = Rows[RowIndex];
	return Row.Category == EPieMenuCategory::Target && Row.Targets.IsEmpty() ? Cast<AActor>(Row.DataReference) : nullptr;
}

void UBattleCommandMenuWidget::PushToWidgets()
{
	SetVisibility(bShowing ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (TitleText)
	{
		TitleText->SetText(Title);
	}
	if (DurabilityText)
	{
		DurabilityText->SetText(FText::Join(LOCTEXT("DurabilitySeparator", "   "), DurabilityLines));
		DurabilityText->SetVisibility(bShowing && !DurabilityLines.IsEmpty() ? ESlateVisibility::HitTestInvisible
																			  : ESlateVisibility::Collapsed);
	}
	for (int32 Index = 0; Index < RowButtons.Num(); ++Index)
	{
		UButton *Button = RowButtons[Index];
		UTextBlock *Label = RowTexts.IsValidIndex(Index) ? RowTexts[Index].Get() : nullptr;
		if (!Button || !Label)
		{
			continue;
		}
		if (!bShowing || !Rows.IsValidIndex(Index))
		{
			Button->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const FPieMenuButtonData &Row = Rows[Index];
		const bool bHeader = Row.Category == EPieMenuCategory::SectionHeader;
		Label->SetText(RowLabels.IsValidIndex(Index) ? RowLabels[Index] : Row.DisplayName);
		Label->SetColorAndOpacity(FSlateColor(bHeader ? BattleCommandMenuStyle::HeaderColor : Row.ButtonTint));
		Button->SetIsEnabled(!bHeader && Row.bEnabled);
		Button->SetVisibility(ESlateVisibility::Visible);
	}
}

// ========================================
// DEBUG
// ========================================

FString UBattleCommandMenuWidget::GetCommandMenuString() const
{
	FString Out = FString::Printf(TEXT("[BattleCommandMenu] bound=%s showing=%s rows=%d title=\"%s\" durability: %s"),
								  bBound ? TEXT("yes") : TEXT("no"), bShowing ? TEXT("yes") : TEXT("no"), Rows.Num(), *Title.ToString(),
								  DurabilityLines.IsEmpty() ? TEXT("none")
															: *FText::Join(FText::FromString(TEXT(", ")), DurabilityLines).ToString());
	for (int32 Index = 0; Index < RowLabels.Num(); ++Index)
	{
		Out += FString::Printf(TEXT("\n  [%d] %s%s"), Index, *RowLabels[Index].ToString(),
							   Rows.IsValidIndex(Index) && !Rows[Index].bEnabled ? TEXT(" (greyed)") : TEXT(""));
	}
	return Out;
}

#undef LOCTEXT_NAMESPACE
