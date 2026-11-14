// CharacterFormWidget.cpp

#include "UI/Hub/CharacterFormWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "CharacterFormWidget"

namespace CharacterFormStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.75f);
	constexpr int32 FontSize = 20;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
	constexpr float PercentScale = 100.0f;
}

namespace CharacterFormBuild
{
	UTextBlock *MakeText(UWidgetTree *Tree)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = CharacterFormStyle::FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		return Text;
	}

	void AddRow(UVerticalBox *Box, UWidget *Row)
	{
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Row))
		{
			BoxSlot->SetPadding(FMargin(CharacterFormStyle::RowPadding));
		}
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = MakeText(Tree);
		Text->SetText(Label);
		Button->AddChild(Text);
		AddRow(Box, Button);
		return Button;
	}

	FAutoConsoleCommandWithWorld GOpenFormsCmd(
		TEXT("wor.UI.FormsOld"), // OBSOLETE (B-444): wor.UI.Forms opens UFormsScreenWidget; removed after Crown's PIE pass
		TEXT("PLACEHOLDER UI (B-279b): open the old skins screen (forms, meters, reveal, wear; Manny / Quinn)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				UCharacterFormWidget::Open(World ? World->GetFirstPlayerController() : nullptr);
			}));
}

UCharacterFormWidget *UCharacterFormWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	for (TObjectIterator<UCharacterFormWidget> It; It; ++It)
	{
		if (It->IsInViewport() && It->GetOwningPlayer() == PC)
		{
			return *It;
		}
	}
	UCharacterFormWidget *Widget = CreateWidget<UCharacterFormWidget>(PC, UCharacterFormWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(CharacterFormStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UCharacterFormWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(CharacterFormStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		TitleText = CharacterFormBuild::MakeText(WidgetTree);
		CharacterFormBuild::AddRow(Box, TitleText);
		BodyText = CharacterFormBuild::MakeText(WidgetTree);
		CharacterFormBuild::AddRow(Box, BodyText);

		CharacterFormBuild::AddButton(WidgetTree, Box, LOCTEXT("NextButton", "Next form"))
			->OnClicked.AddDynamic(this, &UCharacterFormWidget::HandleNextClicked);
		CharacterFormBuild::AddButton(WidgetTree, Box, LOCTEXT("WearButton", "Wear this form"))
			->OnClicked.AddDynamic(this, &UCharacterFormWidget::HandleWearClicked);
		CharacterFormBuild::AddButton(WidgetTree, Box, LOCTEXT("TakeOffButton", "Take the skin off"))
			->OnClicked.AddDynamic(this, &UCharacterFormWidget::HandleTakeOffClicked);
		CharacterFormBuild::AddButton(WidgetTree, Box, LOCTEXT("MannyButton", "Base body: Manny"))
			->OnClicked.AddDynamic(this, &UCharacterFormWidget::HandleMannyClicked);
		CharacterFormBuild::AddButton(WidgetTree, Box, LOCTEXT("QuinnButton", "Base body: Quinn"))
			->OnClicked.AddDynamic(this, &UCharacterFormWidget::HandleQuinnClicked);
		CharacterFormBuild::AddButton(WidgetTree, Box, LOCTEXT("CloseButton", "Close"))
			->OnClicked.AddDynamic(this, &UCharacterFormWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UCharacterFormWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SelectNextForm();
	ShowPage();
}

UCharacterFormSubsystem *UCharacterFormWidget::GetForms() const
{
	const UGameInstance *GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UCharacterFormSubsystem>() : nullptr;
}

bool UCharacterFormWidget::SelectNextForm()
{
	const UCharacterFormSubsystem *Forms = GetForms();
	if (!Forms || Forms->GetForms().IsEmpty())
	{
		Selected = nullptr;
		ShowPage();
		return false;
	}
	const TArray<TObjectPtr<UCharacterFormData>> &All = Forms->GetForms();
	const int32 Current = All.IndexOfByKey(Selected);
	Selected = All[(Current + 1) % All.Num()];
	Status = FText::GetEmpty();
	ShowPage();
	return true;
}

EFormSwitchRefusal UCharacterFormWidget::WearSelected()
{
	UCharacterFormSubsystem *Forms = GetForms();
	const EFormSwitchRefusal Refusal = Forms ? Forms->TryWearForm(Selected) : EFormSwitchRefusal::NotAtSwitchSpot;
	ShowStatus(Refusal == EFormSwitchRefusal::None ? LOCTEXT("Worn", "Worn.") : CharacterFormSwitch::GetRefusalText(Refusal));
	return Refusal;
}

EFormSwitchRefusal UCharacterFormWidget::TakeOffSkin()
{
	UCharacterFormSubsystem *Forms = GetForms();
	const EFormSwitchRefusal Refusal = Forms ? Forms->TryWearForm(nullptr) : EFormSwitchRefusal::NotAtSwitchSpot;
	ShowStatus(Refusal == EFormSwitchRefusal::None ? LOCTEXT("TakenOff", "Back to your own form.") : CharacterFormSwitch::GetRefusalText(Refusal));
	return Refusal;
}

bool UCharacterFormWidget::PickBaseForm(EPlayerBaseForm Base)
{
	UCharacterFormSubsystem *Forms = GetForms();
	const bool bSet = Forms && Forms->TrySetBaseForm(Base);
	ShowStatus(bSet ? LOCTEXT("BaseSet", "Base body changed.") : LOCTEXT("BaseRefused", "Change your body in the hub or at a rest spot, outside a fight."));
	return bSet;
}

void UCharacterFormWidget::ShowStatus(const FText &InStatus)
{
	Status = InStatus;
	ShowPage();
}

void UCharacterFormWidget::ShowPage()
{
	const UCharacterFormSubsystem *Forms = GetForms();
	const UCharacterFormData *Worn = Forms ? Forms->GetWornForm() : nullptr;
	const FText BaseName = Forms && Forms->GetBaseForm() == EPlayerBaseForm::Quinn ? LOCTEXT("Quinn", "Quinn") : LOCTEXT("Manny", "Manny");
	const FText WornLine = FText::Format(LOCTEXT("WornLine", "Wearing: {0} (base body {1})"),
										 Worn ? Worn->FormName : LOCTEXT("NoSkin", "your own form"), BaseName);

	TArray<FText> Lines;
	Lines.Add(WornLine);
	if (!Selected || !Forms)
	{
		Title = LOCTEXT("NoForms", "Forms");
		Lines.Add(LOCTEXT("NoFormsBody", "No character forms are known yet."));
	}
	else
	{
		const bool bUnlocked = Forms->IsUnlocked(Selected);
		Title = FText::Format(LOCTEXT("FormTitle", "{0}: {1}%"), Selected->FormName,
							  FText::AsNumber(FMath::FloorToInt(Forms->GetProgress(Selected) * CharacterFormStyle::PercentScale)));
		if (bUnlocked)
		{
			Lines.Add(LOCTEXT("RevealHeader", "Unlocked. Their loadout:"));
			Lines.Add(Selected->GetRevealedLoadoutText());
		}
		else
		{
			Lines.Add(LOCTEXT("LockedBody", "Locked. Wear their pieces in fights you win to fill the meter."));
		}
	}
	if (!Status.IsEmpty())
	{
		Lines.Add(Status);
	}
	Body = FText::Join(FText::FromString(TEXT("\n")), Lines);
	if (TitleText && BodyText)
	{
		TitleText->SetText(Title);
		BodyText->SetText(Body);
	}
}

void UCharacterFormWidget::HandleNextClicked()
{
	SelectNextForm();
}

void UCharacterFormWidget::HandleWearClicked()
{
	WearSelected();
}

void UCharacterFormWidget::HandleTakeOffClicked()
{
	TakeOffSkin();
}

void UCharacterFormWidget::HandleMannyClicked()
{
	PickBaseForm(EPlayerBaseForm::Manny);
}

void UCharacterFormWidget::HandleQuinnClicked()
{
	PickBaseForm(EPlayerBaseForm::Quinn);
}

void UCharacterFormWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

#undef LOCTEXT_NAMESPACE
