// LevelDropWidget.cpp

#include "UI/Hub/LevelDropWidget.h"

#include "Player/RunStateComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Character/CharacterData.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Trial/LevelDropPlacement.h"
#include "Trial/TrialRestSpot.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/TrialRunSubsystem.h"
#include "Currency/CurrencyTypes.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "LevelDropWidget"

namespace LevelDropStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace LevelDropBuild
{
	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = LevelDropStyle::FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(LevelDropStyle::RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = LevelDropStyle::FontSize;
		Text->SetFont(Font);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(LevelDropStyle::RowPadding));
		}
		return Button;
	}
}

ULevelDropWidget *ULevelDropWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	ULevelDropWidget *Widget = CreateWidget<ULevelDropWidget>(PC, ULevelDropWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(LevelDropStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> ULevelDropWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(LevelDropStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		WaitingText = LevelDropBuild::AddText(WidgetTree, Box);
		MemberText = LevelDropBuild::AddText(WidgetTree, Box);
		LevelDropBuild::AddButton(WidgetTree, Box, LOCTEXT("NextMember", "Next member"))
			->OnClicked.AddDynamic(this, &ULevelDropWidget::HandleNextMemberClicked);
		LevelDropBuild::AddButton(WidgetTree, Box, WoRLevelDrops::GetPillarName(EWorldPillar::Mind))
			->OnClicked.AddDynamic(this, &ULevelDropWidget::HandleMindClicked);
		LevelDropBuild::AddButton(WidgetTree, Box, WoRLevelDrops::GetPillarName(EWorldPillar::Body))
			->OnClicked.AddDynamic(this, &ULevelDropWidget::HandleBodyClicked);
		LevelDropBuild::AddButton(WidgetTree, Box, WoRLevelDrops::GetPillarName(EWorldPillar::Spirit))
			->OnClicked.AddDynamic(this, &ULevelDropWidget::HandleSpiritClicked);
		UButton *Buy = LevelDropBuild::AddButton(WidgetTree, Box, LOCTEXT("Buy", "Purchase point"));
		Buy->OnClicked.AddDynamic(this, &ULevelDropWidget::HandleBuyClicked);
		BuyLabel = Cast<UTextBlock>(Buy->GetChildAt(0));
		StatusLine = LevelDropBuild::AddText(WidgetTree, Box);
		LevelDropBuild::AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))->OnClicked.AddDynamic(this, &ULevelDropWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void ULevelDropWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

TArray<const FPartyMember *> ULevelDropWidget::GatherMembers() const
{
	TArray<const FPartyMember *> Members;
	const UGameInstance *GI = GetGameInstance();
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	if (Party)
	{
		for (const FPartyMember &Member : Party->Members)
		{
			if (Member.CharacterData)
			{
				Members.Add(&Member);
			}
		}
	}
	return Members;
}

const FPartyMember *ULevelDropWidget::SelectedMember() const
{
	const TArray<const FPartyMember *> Members = GatherMembers();
	return Members.IsValidIndex(MemberIndex) ? Members[MemberIndex] : nullptr;
}

void ULevelDropWidget::Refresh()
{
	if (!WaitingText || !MemberText || !StatusLine)
	{
		return;
	}
	const FPartyMember *Member = SelectedMember();
	// B-223b: each character places its own levels; Next member steps through whose levels these are.
	WaitingText->SetText(Member ? FText::Format(LOCTEXT("Waiting", "{0}'s pillar levels to place: {1}"), FText::FromString(Member->CharacterData->Name),
												FText::AsNumber(URunStateComponent::GetUnspentLevels(this, UPoolSubsystem::GetCharacterKey(*Member))))
								: LOCTEXT("WaitingNone", "No party member"));

	if (!Member)
	{
		MemberText->SetText(LOCTEXT("NoMember", "No party member"));
	}
	else
	{
		const UWorld *World = GetWorld();
		// B-364: a pillar past 7 shows its over-cap points.
		auto PillarLevel = [World, Member](EWorldPillar Pillar)
		{
			const FText Level = FText::AsNumber(WoRLevelDrops::GetLiveLevel(World, *Member, Pillar));
			const int32 Over = WoRLevelDrops::GetOverCap(World, *Member, Pillar);
			return Over > 0 ? FText::Format(LOCTEXT("PillarOverCap", "{0} (+{1} over-cap)"), Level, FText::AsNumber(Over)) : Level;
		};
		MemberText->SetText(FText::Format(LOCTEXT("MemberRow", "{0}: {1} {2}, {3} {4}, {5} {6}"), FText::FromString(Member->CharacterData->Name),
										  WoRLevelDrops::GetPillarName(EWorldPillar::Mind), PillarLevel(EWorldPillar::Mind),
										  WoRLevelDrops::GetPillarName(EWorldPillar::Body), PillarLevel(EWorldPillar::Body),
										  WoRLevelDrops::GetPillarName(EWorldPillar::Spirit), PillarLevel(EWorldPillar::Spirit)));
	}
	if (BuyLabel)
	{
		BuyLabel->SetText(Member ? FText::Format(LOCTEXT("BuyPrice", "Purchase point ({0} {1})"), FText::AsNumber(WoRLevelDrops::GetPointPrice(GetWorld(), *Member)),
												 WoRGameNames::GetCurrencyName(ECurrencyType::Gold))
								 : LOCTEXT("BuyNone", "Purchase point"));
	}
	StatusLine->SetText(StatusText);
}

FString ULevelDropWidget::GetLevelDropWidgetString() const
{
	FString Out = TEXT("[LevelDrop] ") + (WaitingText ? WaitingText->GetText().ToString() : FString()) + TEXT(" | ") +
				  (MemberText ? MemberText->GetText().ToString() : FString());
	if (!StatusText.IsEmpty())
	{
		Out += TEXT(" | ") + StatusText.ToString();
	}
	return Out;
}

void ULevelDropWidget::Place(EWorldPillar Pillar)
{
	const FPartyMember *Member = SelectedMember();
	if (!Member)
	{
		return;
	}
	const FText Refusal = WoRLevelDrops::TryPlaceLevel(GetWorld(), *Member, Pillar);
	StatusText = Refusal.IsEmpty()
					 ? FText::Format(LOCTEXT("Placed", "{0} +1 {1}"), FText::FromString(Member->CharacterData->Name), WoRLevelDrops::GetPillarName(Pillar))
					 : Refusal;
	Refresh();
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetLevelDropWidgetString());
}

void ULevelDropWidget::HandleNextMemberClicked()
{
	const int32 Count = GatherMembers().Num();
	MemberIndex = Count > 0 ? (MemberIndex + 1) % Count : 0;
	StatusText = FText::GetEmpty();
	Refresh();
}

void ULevelDropWidget::HandleMindClicked() { Place(EWorldPillar::Mind); }

void ULevelDropWidget::HandleBodyClicked() { Place(EWorldPillar::Body); }

void ULevelDropWidget::HandleSpiritClicked() { Place(EWorldPillar::Spirit); }

void ULevelDropWidget::HandleBuyClicked()
{
	const FPartyMember *Member = SelectedMember();
	if (!Member)
	{
		return;
	}
	const FText Refusal = WoRLevelDrops::TryBuyPoint(GetWorld(), *Member);
	StatusText = Refusal.IsEmpty() ? FText::Format(LOCTEXT("Bought", "{0}: +1 point to place"), FText::FromString(Member->CharacterData->Name)) : Refusal;
	Refresh();
}

void ULevelDropWidget::HandleCloseClicked()
{
	WoRRestSpot::StepAway(GetWorld()); // B-363 review: placing and buying end with the rest spot's level screen
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

static FAutoConsoleCommandWithWorld GLevelDropUICommand(
	TEXT("wor.UI.LevelDrop"),
	TEXT("PLACEHOLDER UI: place the pillar levels a won fight dropped on a party member's Mind, Body or Spirit (B-223)."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!ULevelDropWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogTemp, Warning, TEXT("[LevelDrop] wor.UI.LevelDrop: no player controller"));
			}
		}));

#undef LOCTEXT_NAMESPACE
