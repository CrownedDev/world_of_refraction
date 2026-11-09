// CurseClearWidget.cpp

#include "UI/Hub/CurseClearWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Currency/CurrencyWallet.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Memory/CurseData.h"
#include "Memory/CurseSubsystem.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "CurseClear"

namespace CurseClearStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace CurseClearBuild
{
	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = CurseClearStyle::FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(CurseClearStyle::RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = CurseClearStyle::FontSize;
		Text->SetFont(Font);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(CurseClearStyle::RowPadding));
		}
		return Button;
	}

	UCurseSubsystem *CursesOf(const UObject *Context)
	{
		const UWorld *World = Context ? Context->GetWorld() : nullptr;
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UCurseSubsystem>() : nullptr;
	}

	/** The curse asset's title (Crown's text), else its id. */
	FText NameOf(FName CurseId)
	{
		for (const UCurseData *Curse : UCurseData::GatherAll())
		{
			if (Curse && Curse->CurseId == CurseId && !Curse->Title.IsEmpty())
			{
				return Curse->Title;
			}
		}
		return FText::FromName(CurseId);
	}

	/** B-390 (D-169): "F-Tier, clearing costs 10 Pillar Essence" (the asset defaults when none is authored). */
	FText PriceOf(const UCurseSubsystem &Curses, FName CurseId)
	{
		const UCurseData *Curse = Curses.FindCurseAsset(CurseId);
		const UCurseData *Priced = Curse ? Curse : GetDefault<UCurseData>();
		return FText::Format(LOCTEXT("Price", "{0}, clearing costs {1} {2}"),
							 FText::AsCultureInvariant(TierHelpers::GetTierDisplayString(Priced->Tier)),
							 FText::AsNumber(Priced->GetClearPrice()), WoRGameNames::GetCurrencyName(Priced->GetClearCurrency()));
	}
}

UCurseClearWidget *UCurseClearWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UCurseClearWidget *Widget = CreateWidget<UCurseClearWidget>(PC, UCurseClearWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(CurseClearStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

bool UCurseClearWidget::CanClearHere(const UObject *Context)
{
	const UWorld *World = Context ? Context->GetWorld() : nullptr;
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	return !(Run && Run->IsRunActive()); // no run subsystem (a test world, the menu): not in a run, so the hub's rule
}

TSharedRef<SWidget> UCurseClearWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(CurseClearStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		CurseText = CurseClearBuild::AddText(WidgetTree, Box);
		CurseClearBuild::AddButton(WidgetTree, Box, LOCTEXT("Next", "Next curse"))
			->OnClicked.AddDynamic(this, &UCurseClearWidget::HandleNextClicked);
		ClearButton = CurseClearBuild::AddButton(WidgetTree, Box, LOCTEXT("Clear", "Clear this curse"));
		ClearButton->OnClicked.AddDynamic(this, &UCurseClearWidget::HandleClearClicked);
		StatusLine = CurseClearBuild::AddText(WidgetTree, Box);
		CurseClearBuild::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))
			->OnClicked.AddDynamic(this, &UCurseClearWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UCurseClearWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

TArray<FName> UCurseClearWidget::GatherHeld() const
{
	TArray<FName> Held;
	if (const UCurseSubsystem *Curses = CurseClearBuild::CursesOf(this))
	{
		Held = Curses->GetHeldCurses().Array();
	}
	Held.Sort([](const FName &A, const FName &B) { return A.LexicalLess(B); });
	return Held;
}

void UCurseClearWidget::Refresh()
{
	const TArray<FName> Held = GatherHeld();
	SelectedIndex = Held.IsEmpty() ? 0 : SelectedIndex % Held.Num();
	const bool bCanClear = !Held.IsEmpty() && CanClearHere(this);
	const UCurseSubsystem *Curses = CurseClearBuild::CursesOf(this);
	if (CurseText)
	{
		CurseText->SetText(Held.IsEmpty() || !Curses
			? LOCTEXT("NoCurses", "You hold no curses.")
			: FText::Format(LOCTEXT("Selected", "Curse {0} of {1}: {2} ({3})"), FText::AsNumber(SelectedIndex + 1), FText::AsNumber(Held.Num()),
							CurseClearBuild::NameOf(Held[SelectedIndex]), CurseClearBuild::PriceOf(*Curses, Held[SelectedIndex])));
	}
	if (ClearButton)
	{
		ClearButton->SetIsEnabled(bCanClear);
	}
	if (StatusLine)
	{
		StatusLine->SetText(!Held.IsEmpty() && !CanClearHere(this) ? LOCTEXT("InRun", "Curses can be cleared in the hub, not during a run.")
																   : Status);
	}
}

FString UCurseClearWidget::GetCurseClearString() const
{
	return FString::Printf(TEXT("[CurseClear] %s | %s"), CurseText ? *CurseText->GetText().ToString() : TEXT("-"),
						   StatusLine ? *StatusLine->GetText().ToString() : TEXT("-"));
}

void UCurseClearWidget::HandleNextClicked()
{
	++SelectedIndex;
	Status = FText::GetEmpty();
	Refresh();
}

void UCurseClearWidget::HandleClearClicked()
{
	const TArray<FName> Held = GatherHeld();
	UCurseSubsystem *Curses = CurseClearBuild::CursesOf(this);
	if (!Curses || !Held.IsValidIndex(SelectedIndex) || !CanClearHere(this))
	{
		return;
	}
	// B-390 (D-169): paid in the curse's essence by its tier, from the player's wallet.
	const FName CurseId = Held[SelectedIndex];
	FText Refusal;
	if (Curses->PayToClearCurse(CurseId, CurrencyWallet::FindFor(GetOwningPlayerPawn()), Refusal))
	{
		Status = FText::Format(LOCTEXT("Cleared", "{0} is cleared. It may come back."), CurseClearBuild::NameOf(CurseId));
	}
	else
	{
		Status = Refusal;
	}
	Refresh();
	UE_LOG(LogWoR, Log, TEXT("%s"), *GetCurseClearString());
}

void UCurseClearWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

static FAutoConsoleCommandWithWorld GCurseClearUICommand(
	TEXT("wor.UI.ClearCurse"),
	TEXT("PLACEHOLDER UI (Q-378): the hub's service for clearing a curse you hold."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!UCurseClearWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogWoR, Warning, TEXT("[CurseClear] wor.UI.ClearCurse: no player controller"));
			}
		}));

#undef LOCTEXT_NAMESPACE
