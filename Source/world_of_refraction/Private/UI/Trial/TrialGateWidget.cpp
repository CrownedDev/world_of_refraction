// TrialGateWidget.cpp

#include "UI/Trial/TrialGateWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"

#define LOCTEXT_NAMESPACE "TrialGate"

namespace TrialGateStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace TrialGateBuild
{
	UTextBlock *MakeText(UWidgetTree *Tree)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = TrialGateStyle::FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		return Text;
	}

	void AddRow(UVerticalBox *Box, UWidget *Row)
	{
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Row))
		{
			BoxSlot->SetPadding(FMargin(TrialGateStyle::RowPadding));
		}
	}

	UTextBlock *AddButton(UWidgetTree *Tree, UVerticalBox *Box, UButton *&OutButton)
	{
		OutButton = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Label = MakeText(Tree);
		OutButton->AddChild(Label);
		AddRow(Box, OutButton);
		return Label;
	}
}

UTrialGateWidget *UTrialGateWidget::Open(APlayerController *PC, UTrialData *Trial)
{
	if (!PC || !Trial)
	{
		return nullptr;
	}
	UTrialGateWidget *Widget = CreateWidget<UTrialGateWidget>(PC, UTrialGateWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->Trial = Trial;
	Widget->VariationIndex = Trial->Variations.IsEmpty() ? INDEX_NONE : 0;
	Widget->AddToViewport(TrialGateStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UTrialGateWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(TrialGateStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		TitleText = TrialGateBuild::MakeText(WidgetTree);
		TrialGateBuild::AddRow(Box, TitleText);

		UButton *Button = nullptr;
		VariationText = TrialGateBuild::AddButton(WidgetTree, Box, Button);
		VariationButton = Button;
		VariationButton->OnClicked.AddDynamic(this, &UTrialGateWidget::HandleVariationClicked);

		DifficultyText = TrialGateBuild::AddButton(WidgetTree, Box, Button);
		Button->OnClicked.AddDynamic(this, &UTrialGateWidget::HandleDifficultyClicked);

		TrialGateBuild::AddButton(WidgetTree, Box, Button)->SetText(LOCTEXT("Enter", "Enter the trial"));
		Button->OnClicked.AddDynamic(this, &UTrialGateWidget::HandleEnterClicked);

		TrialGateBuild::AddButton(WidgetTree, Box, Button)->SetText(LOCTEXT("Cancel", "Cancel"));
		Button->OnClicked.AddDynamic(this, &UTrialGateWidget::HandleCancelClicked);
	}
	return Super::RebuildWidget();
}

void UTrialGateWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

void UTrialGateWidget::Refresh()
{
	if (!Trial || !TitleText)
	{
		return;
	}
	TitleText->SetText(Trial->Name);

	const bool bHasVariations = Trial->Variations.IsValidIndex(VariationIndex);
	VariationButton->SetVisibility(bHasVariations ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bHasVariations)
	{
		const FTrialVariation &Variation = Trial->Variations[VariationIndex];
		VariationText->SetText(FText::Format(LOCTEXT("VariationRow", "Variation {0}/{1}: {2}\n{3}"),
											 FText::AsNumber(VariationIndex + 1), FText::AsNumber(Trial->Variations.Num()),
											 Variation.Name, Variation.Description));
	}

	DifficultyText->SetText(FText::Format(LOCTEXT("DifficultyRow", "Difficulty: {0} (enemies x{1}, payout x{2})"),
										  TrialGate::GetDifficultyName(Difficulty),
										  FText::AsNumber(TrialGate::GetEnemyStrengthMultiplier(Difficulty)),
										  FText::AsNumber(TrialGate::GetPayoutMultiplier(Difficulty))));
}

FString UTrialGateWidget::GetTrialGateString() const
{
	FString Out = TEXT("[TrialGate]");
	for (const UTextBlock *Text : {TitleText.Get(), VariationText.Get(), DifficultyText.Get()})
	{
		if (Text)
		{
			Out += TEXT("\n  ") + Text->GetText().ToString();
		}
	}
	return Out;
}

void UTrialGateWidget::HandleVariationClicked()
{
	if (Trial && !Trial->Variations.IsEmpty())
	{
		VariationIndex = (VariationIndex + 1) % Trial->Variations.Num();
		Refresh();
	}
}

void UTrialGateWidget::HandleDifficultyClicked()
{
	Difficulty = TrialGate::NextDifficulty(Difficulty);
	Refresh();
}

void UTrialGateWidget::HandleEnterClicked()
{
	const UGameInstance *GI = GetGameInstance();
	UTrialRunSubsystem *TrialRun = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	UTrialData *ChosenTrial = Trial;
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetTrialGateString());
	Close();
	if (TrialRun && ChosenTrial)
	{
		TrialRun->SetGateChoice(VariationIndex, Difficulty);
		TrialRun->EnterTrial(ChosenTrial);
	}
}

void UTrialGateWidget::HandleCancelClicked()
{
	Close();
}

void UTrialGateWidget::Close()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

#undef LOCTEXT_NAMESPACE
