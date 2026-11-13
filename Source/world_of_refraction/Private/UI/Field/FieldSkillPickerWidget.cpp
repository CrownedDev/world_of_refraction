// FieldSkillPickerWidget.cpp

#include "UI/Field/FieldSkillPickerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Combat/WoRFightSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Field/FieldMovementComponent.h"
#include "Field/FieldSkillUnlockSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "FieldSkillPicker"

namespace FieldSkillPickerStyle
{
	// TODO: style pass (placeholder until the CommonUI loadout screen).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 55;
	constexpr int32 SourceCount = 3; // Innate, Weapon, Ring
}

namespace
{
	UTextBlock *NewLabel(UWidgetTree *Tree)
	{
		UTextBlock *Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FieldSkillPickerStyle::FontSize;
		Label->SetFont(Font);
		return Label;
	}

	void AddRow(UVerticalBox *Box, UWidget *Row)
	{
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Row))
		{
			BoxSlot->SetPadding(FMargin(FieldSkillPickerStyle::RowPadding));
		}
	}

	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Label = NewLabel(Tree);
		AddRow(Box, Label);
		return Label;
	}

	UTextBlock *AddButton(UWidgetTree *Tree, UVerticalBox *Box, UButton *&OutButton)
	{
		OutButton = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Label = NewLabel(Tree);
		OutButton->AddChild(Label);
		AddRow(Box, OutButton);
		return Label;
	}

	FText SourceName(EFieldSkillSource Source)
	{
		switch (Source)
		{
		case EFieldSkillSource::Innate:
			return LOCTEXT("SourceInnate", "Innate");
		case EFieldSkillSource::Weapon:
			return LOCTEXT("SourceWeapon", "Weapon crystal");
		case EFieldSkillSource::Ring:
			return LOCTEXT("SourceRing", "Ring");
		}
		return FText::GetEmpty();
	}

	FText TypeName(EFieldMovementType Type)
	{
		return StaticEnum<EFieldMovementType>()->GetDisplayNameTextByValue(static_cast<int64>(Type));
	}
}

UFieldSkillPickerWidget *UFieldSkillPickerWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UFieldSkillPickerWidget *Widget = CreateWidget<UFieldSkillPickerWidget>(PC, UFieldSkillPickerWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(FieldSkillPickerStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UFieldSkillPickerWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(FieldSkillPickerStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		UButton *SourceButton = nullptr;
		SourceText = AddButton(WidgetTree, Box, SourceButton);
		SourceButton->OnClicked.AddDynamic(this, &UFieldSkillPickerWidget::HandleSourceClicked);

		UButton *TypeButton = nullptr;
		TypeText = AddButton(WidgetTree, Box, TypeButton);
		TypeButton->OnClicked.AddDynamic(this, &UFieldSkillPickerWidget::HandleTypeClicked);

		StatusText = AddText(WidgetTree, Box);

		UButton *CloseButton = nullptr;
		AddButton(WidgetTree, Box, CloseButton)->SetText(LOCTEXT("Close", "Close"));
		CloseButton->OnClicked.AddDynamic(this, &UFieldSkillPickerWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UFieldSkillPickerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (const UFieldMovementComponent *FieldMovement = GetFieldMovement())
	{
		ShownSource = FieldMovement->GetActiveFieldSkill().Source;
	}
	if (UFieldSkillUnlockSubsystem *Unlocks = WoRFight::Get<UFieldSkillUnlockSubsystem>(this))
	{
		Unlocks->RecordFromPool(); // ranks reached since the last scan show at once
	}
	Refresh();
}

UFieldMovementComponent *UFieldSkillPickerWidget::GetFieldMovement() const
{
	const APlayerController *PC = GetOwningPlayer();
	return PC && PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UFieldMovementComponent>() : nullptr;
}

TArray<EFieldMovementType> UFieldSkillPickerWidget::GetPickableTypes() const
{
	TArray<EFieldMovementType> Types;
	const UFieldMovementComponent *FieldMovement = GetFieldMovement();
	const UFieldSkillUnlockSubsystem *Unlocks = WoRFight::Get<UFieldSkillUnlockSubsystem>(this);
	if (!FieldMovement || !Unlocks)
	{
		return Types;
	}
	const ESpellElement Element = FieldMovement->GetSourceElement(ShownSource);
	for (const EFieldMovementType Type : Unlocks->GetUnlockedTypes(Element))
	{
		if (FieldSkillRules::CheckPick(Type, Element, true) == EFieldSkillRefusal::None)
		{
			Types.Add(Type);
		}
	}
	return Types;
}

void UFieldSkillPickerWidget::HandleSourceClicked()
{
	ShownSource = static_cast<EFieldSkillSource>((static_cast<int32>(ShownSource) + 1) % FieldSkillPickerStyle::SourceCount);
	LastRefusal = EFieldSkillRefusal::None;
	Refresh();
}

void UFieldSkillPickerWidget::HandleTypeClicked()
{
	UFieldMovementComponent *FieldMovement = GetFieldMovement();
	const TArray<EFieldMovementType> Types = GetPickableTypes();
	if (!FieldMovement || Types.Num() == 0)
	{
		return;
	}
	const FFieldSkillChoice Current = FieldMovement->GetActiveFieldSkill();
	const int32 CurrentIndex = Current.Source == ShownSource ? Types.IndexOfByKey(Current.Type) : INDEX_NONE;
	const EFieldMovementType Next = Types[(CurrentIndex + 1) % Types.Num()];
	LastRefusal = FieldMovement->SetActiveFieldSkill(ShownSource, Next);
	Refresh();
	UE_LOG(LogWoR, Log, TEXT("[FieldSkillPicker] %s"), *GetPickerString());
}

void UFieldSkillPickerWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

void UFieldSkillPickerWidget::Refresh()
{
	const UFieldMovementComponent *FieldMovement = GetFieldMovement();
	if (!FieldMovement)
	{
		SourceText->SetText(LOCTEXT("NoFieldPawn", "No field character (open this on the map)"));
		TypeText->SetText(FText::GetEmpty());
		StatusText->SetText(FText::GetEmpty());
		return;
	}
	const ESpellElement Element = FieldMovement->GetSourceElement(ShownSource);
	SourceText->SetText(FText::Format(LOCTEXT("SourceRow", "Source: {0} ({1})"), SourceName(ShownSource),
									  FieldSkillRules::IsElemental(Element) ? WoRGameNames::GetElementName(Element)
																			  : LOCTEXT("NoElement", "no element")));

	const FFieldSkillChoice Current = FieldMovement->GetActiveFieldSkill();
	TypeText->SetText(FText::Format(LOCTEXT("TypeRow", "Movement: {0} from {1} (click for the next unlocked)"),
									TypeName(Current.Type), SourceName(Current.Source)));

	TArray<FText> Names;
	for (const EFieldMovementType Type : GetPickableTypes())
	{
		Names.Add(TypeName(Type));
	}
	const FText Unlocked = FText::Join(LOCTEXT("ListSeparator", ", "), Names);
	StatusText->SetText(LastRefusal == EFieldSkillRefusal::None
							? FText::Format(LOCTEXT("UnlockedRow", "Unlocked for this source: {0}"), Unlocked)
							: FText::Format(LOCTEXT("RefusedRow", "Refused ({0}). Unlocked for this source: {1}"),
											FText::FromString(FieldSkillRules::RefusalName(LastRefusal)), Unlocked));
}

FString UFieldSkillPickerWidget::GetPickerString() const
{
	return FString::Printf(TEXT("%s | %s | %s"), SourceText ? *SourceText->GetText().ToString() : TEXT(""),
						   TypeText ? *TypeText->GetText().ToString() : TEXT(""),
						   StatusText ? *StatusText->GetText().ToString() : TEXT(""));
}

static FAutoConsoleCommandWithWorld GFieldSkillPickerCommand(
	TEXT("wor.UI.FieldSkill"),
	TEXT("PLACEHOLDER UI (B-214b): open the field skill picker for the player's character (a source, then an unlocked "
		 "movement type)."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!UFieldSkillPickerWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogWoR, Warning, TEXT("[FieldSkillPicker] wor.UI.FieldSkill: no player controller"));
			}
		}));

#undef LOCTEXT_NAMESPACE
