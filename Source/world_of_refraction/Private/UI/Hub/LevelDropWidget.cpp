// LevelDropWidget.cpp

#include "UI/Hub/LevelDropWidget.h"

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
#include "Pool/PoolSubsystem.h"
#include "Trial/TrialRunSubsystem.h"

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

TArray<FPartyMember> ULevelDropWidget::GatherMembers() const
{
	TArray<FPartyMember> Members;
	const UGameInstance *GI = GetGameInstance();
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	if (Party)
	{
		for (const FPartyMember &Member : Party->Members)
		{
			if (Member.CharacterData)
			{
				Members.Add(Member);
			}
		}
	}
	return Members;
}

bool ULevelDropWidget::GetSelectedMember(FPartyMember &OutMember) const
{
	const TArray<FPartyMember> Members = GatherMembers();
	if (!Members.IsValidIndex(MemberIndex))
	{
		return false;
	}
	OutMember = Members[MemberIndex];
	return true;
}

void ULevelDropWidget::Refresh()
{
	if (!WaitingText || !MemberText || !StatusLine)
	{
		return;
	}
	const UGameInstance *GI = GetGameInstance();
	const UTrialRunSubsystem *Run = GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	FPartyMember Member;
	const bool bHasMember = GetSelectedMember(Member);
	// B-223b: each character places its own levels; Next member steps through whose levels these are.
	// B-360c: by member, so a second copy of a character shows (and spends) its own.
	WaitingText->SetText(bHasMember ? FText::Format(LOCTEXT("Waiting", "{0}'s pillar levels to place: {1}"), FText::FromString(Member.CharacterData->Name),
													FText::AsNumber(Run ? Run->GetUnspentLevels(UPoolSubsystem::GetCharacterKey(Member)) : 0))
									: LOCTEXT("WaitingNone", "No party member"));

	if (!bHasMember)
	{
		MemberText->SetText(LOCTEXT("NoMember", "No party member"));
	}
	else
	{
		const UWorld *World = GetWorld();
		MemberText->SetText(FText::Format(LOCTEXT("MemberRow", "{0}: {1} {2}, {3} {4}, {5} {6}"), FText::FromString(Member.CharacterData->Name),
										  WoRLevelDrops::GetPillarName(EWorldPillar::Mind),
										  FText::AsNumber(WoRLevelDrops::GetLiveLevel(World, Member, EWorldPillar::Mind)),
										  WoRLevelDrops::GetPillarName(EWorldPillar::Body),
										  FText::AsNumber(WoRLevelDrops::GetLiveLevel(World, Member, EWorldPillar::Body)),
										  WoRLevelDrops::GetPillarName(EWorldPillar::Spirit),
										  FText::AsNumber(WoRLevelDrops::GetLiveLevel(World, Member, EWorldPillar::Spirit))));
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
	FPartyMember Member;
	if (!GetSelectedMember(Member))
	{
		return;
	}
	const FText Refusal = WoRLevelDrops::TryPlaceLevel(GetWorld(), Member, Pillar);
	StatusText = Refusal.IsEmpty()
					 ? FText::Format(LOCTEXT("Placed", "{0} +1 {1}"), FText::FromString(Member.CharacterData->Name), WoRLevelDrops::GetPillarName(Pillar))
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

void ULevelDropWidget::HandleCloseClicked()
{
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
