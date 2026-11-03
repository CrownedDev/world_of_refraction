// InventoryWidget.cpp

#include "UI/Hub/InventoryWidget.h"

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
#include "Pool/PoolSubsystem.h"

#define LOCTEXT_NAMESPACE "InventoryScreen"

namespace InventoryScreenStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.7f);
	constexpr int32 FontSize = 22;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
	/** One of a stack per verb unless the whole-stack toggle is on. */
	constexpr int32 SingleItem = 1;
}

namespace InventoryScreenBuild
{
	UTextBlock *AddText(UWidgetTree *Tree, UVerticalBox *Box)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = InventoryScreenStyle::FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Text))
		{
			BoxSlot->SetPadding(FMargin(InventoryScreenStyle::RowPadding));
		}
		return Text;
	}

	UButton *AddButton(UWidgetTree *Tree, UVerticalBox *Box, const FText &Label)
	{
		UButton *Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = InventoryScreenStyle::FontSize;
		Text->SetFont(Font);
		Text->SetText(Label);
		Button->AddChild(Text);
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Button))
		{
			BoxSlot->SetPadding(FMargin(InventoryScreenStyle::RowPadding));
		}
		return Button;
	}
}

UInventoryWidget *UInventoryWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UInventoryWidget *Widget = CreateWidget<UInventoryWidget>(PC, UInventoryWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->AddToViewport(InventoryScreenStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UInventoryWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		using namespace InventoryScreenBuild;
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(InventoryScreenStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		MemberText = AddText(WidgetTree, Box);
		AddButton(WidgetTree, Box, LOCTEXT("NextMember", "Next character"))->OnClicked.AddDynamic(this, &UInventoryWidget::HandleNextMemberClicked);
		RowText = AddText(WidgetTree, Box);
		AddButton(WidgetTree, Box, LOCTEXT("NextRow", "Next item"))->OnClicked.AddDynamic(this, &UInventoryWidget::HandleNextRowClicked);
		ReceiverText = AddText(WidgetTree, Box);
		AddButton(WidgetTree, Box, LOCTEXT("NextReceiver", "Next receiver"))->OnClicked.AddDynamic(this, &UInventoryWidget::HandleNextReceiverClicked);

		// One button per verb, in GetAllVerbs order (dynamic delegates need one handler each).
		VerbButtons.Reset();
		for (const EInventoryVerb Verb : WoRInventoryVerbs::GetAllVerbs())
		{
			UButton *Button = AddButton(WidgetTree, Box, WoRInventoryVerbs::GetVerbName(Verb));
			switch (Verb)
			{
			case EInventoryVerb::Give:
				Button->OnClicked.AddDynamic(this, &UInventoryWidget::HandleGiveClicked);
				break;
			case EInventoryVerb::Store:
				Button->OnClicked.AddDynamic(this, &UInventoryWidget::HandleStoreClicked);
				break;
			case EInventoryVerb::Take:
				Button->OnClicked.AddDynamic(this, &UInventoryWidget::HandleTakeClicked);
				break;
			case EInventoryVerb::Dismantle:
				Button->OnClicked.AddDynamic(this, &UInventoryWidget::HandleDismantleClicked);
				break;
			case EInventoryVerb::Convert:
				Button->OnClicked.AddDynamic(this, &UInventoryWidget::HandleConvertClicked);
				break;
			}
			VerbButtons.Add(Button);
		}
		WholeStackButton = AddButton(WidgetTree, Box, LOCTEXT("WholeStack", "Whole stack: on / off"));
		WholeStackButton->OnClicked.AddDynamic(this, &UInventoryWidget::HandleWholeStackClicked);
		StatusLine = AddText(WidgetTree, Box);
		AddButton(WidgetTree, Box, LOCTEXT("Close", "Close"))->OnClicked.AddDynamic(this, &UInventoryWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

TArray<TPair<FName, FText>> UInventoryWidget::GatherMembers() const
{
	TArray<TPair<FName, FText>> Members;
	const UGameInstance *GI = GetGameInstance();
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	if (const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
	{
		for (const FPartyMember &Member : Party->Members)
		{
			if (Member.CharacterData)
			{
				// TODO: character names are FString assets; FText with B-127 (D-061).
				Members.Emplace(UPoolSubsystem::GetCharacterKey(Member.CharacterData), FText::FromString(Member.CharacterData->Name));
			}
		}
	}
	return Members;
}

FName UInventoryWidget::MemberKey() const
{
	const TArray<TPair<FName, FText>> Members = GatherMembers();
	return Members.IsValidIndex(MemberIndex) ? Members[MemberIndex].Key : NAME_None;
}

FName UInventoryWidget::ReceiverKey() const
{
	const TArray<TPair<FName, FText>> Members = GatherMembers();
	return Members.IsValidIndex(ReceiverIndex) ? Members[ReceiverIndex].Key : NAME_None;
}

void UInventoryWidget::Refresh()
{
	const TArray<TPair<FName, FText>> Members = GatherMembers();
	MemberIndex = Members.IsEmpty() ? 0 : FMath::Clamp(MemberIndex, 0, Members.Num() - 1);
	ReceiverIndex = Members.IsEmpty() ? 0 : FMath::Clamp(ReceiverIndex, 0, Members.Num() - 1);
	const TArray<WoRInventoryVerbs::FInventoryRow> Rows = WoRInventoryVerbs::GatherRows(GetWorld(), MemberKey());
	RowIndex = Rows.IsEmpty() ? 0 : FMath::Clamp(RowIndex, 0, Rows.Num() - 1);

	MemberText->SetText(Members.IsEmpty() ? LOCTEXT("NoParty", "No party")
										  : FText::Format(LOCTEXT("MemberRow", "Character: {0}"), Members[MemberIndex].Value));
	ReceiverText->SetText(Members.IsValidIndex(ReceiverIndex) ? FText::Format(LOCTEXT("ReceiverRow", "Give to: {0}"), Members[ReceiverIndex].Value)
															  : FText::GetEmpty());
	if (Rows.IsEmpty())
	{
		RowText->SetText(LOCTEXT("Empty", "Nothing here"));
	}
	else
	{
		const WoRInventoryVerbs::FInventoryRow &Row = Rows[RowIndex];
		RowText->SetText(FText::Format(LOCTEXT("ItemRow", "{0}: {1} ({2} of {3})"),
									   Row.bInChest ? LOCTEXT("Chest", "Chest") : LOCTEXT("Own", "Own"), Row.Label, RowIndex + 1, Rows.Num()));
	}
	const TArray<EInventoryVerb> Applicable = Rows.IsValidIndex(RowIndex)
												  ? WoRInventoryVerbs::GetVerbsFor(GetWorld(), MemberKey(), Rows[RowIndex])
												  : TArray<EInventoryVerb>();
	const TArray<EInventoryVerb> &AllVerbs = WoRInventoryVerbs::GetAllVerbs();
	for (int32 Index = 0; Index < VerbButtons.Num() && Index < AllVerbs.Num(); ++Index)
	{
		VerbButtons[Index]->SetIsEnabled(Applicable.Contains(AllVerbs[Index]));
	}
	WholeStackButton->SetIsEnabled(Rows.IsValidIndex(RowIndex) && !Rows[RowIndex].IsGear());
	StatusLine->SetText(FText::Format(LOCTEXT("StatusRow", "{0}{1}"), bWholeStack ? LOCTEXT("WholeOn", "[whole stack] ") : FText::GetEmpty(), StatusText));
}

void UInventoryWidget::RunVerb(EInventoryVerb Verb)
{
	const TArray<WoRInventoryVerbs::FInventoryRow> Rows = WoRInventoryVerbs::GatherRows(GetWorld(), MemberKey());
	if (!Rows.IsValidIndex(RowIndex))
	{
		return;
	}
	const WoRInventoryVerbs::FInventoryRow Row = Rows[RowIndex];
	const int32 Count = bWholeStack ? Row.Count : InventoryScreenStyle::SingleItem;
	const FText Refusal = WoRInventoryVerbs::Run(GetWorld(), Verb, MemberKey(), ReceiverKey(), Row, Count);
	StatusText = Refusal.IsEmpty() ? FText::Format(LOCTEXT("Done", "{0}: {1}"), WoRInventoryVerbs::GetVerbName(Verb), Row.Label) : Refusal;
	Refresh();
	UE_LOG(LogTemp, Log, TEXT("%s"), *GetInventoryString());
}

FString UInventoryWidget::GetInventoryString() const
{
	FString Out = TEXT("[Inventory] ") + MemberText->GetText().ToString() + TEXT(" | ") + RowText->GetText().ToString() + TEXT(" | ") +
				  ReceiverText->GetText().ToString();
	if (!StatusText.IsEmpty())
	{
		Out += TEXT("\n  ") + StatusText.ToString();
	}
	return Out;
}

void UInventoryWidget::HandleNextMemberClicked()
{
	const int32 Count = GatherMembers().Num();
	MemberIndex = Count > 0 ? (MemberIndex + 1) % Count : 0;
	RowIndex = 0;
	StatusText = FText::GetEmpty();
	Refresh();
}

void UInventoryWidget::HandleNextRowClicked()
{
	const int32 Count = WoRInventoryVerbs::GatherRows(GetWorld(), MemberKey()).Num();
	RowIndex = Count > 0 ? (RowIndex + 1) % Count : 0;
	StatusText = FText::GetEmpty();
	Refresh();
}

void UInventoryWidget::HandleNextReceiverClicked()
{
	const int32 Count = GatherMembers().Num();
	ReceiverIndex = Count > 0 ? (ReceiverIndex + 1) % Count : 0;
	StatusText = FText::GetEmpty();
	Refresh();
}

void UInventoryWidget::HandleGiveClicked() { RunVerb(EInventoryVerb::Give); }

void UInventoryWidget::HandleStoreClicked() { RunVerb(EInventoryVerb::Store); }

void UInventoryWidget::HandleTakeClicked() { RunVerb(EInventoryVerb::Take); }

void UInventoryWidget::HandleDismantleClicked() { RunVerb(EInventoryVerb::Dismantle); }

void UInventoryWidget::HandleConvertClicked() { RunVerb(EInventoryVerb::Convert); }

void UInventoryWidget::HandleWholeStackClicked()
{
	bWholeStack = !bWholeStack;
	Refresh();
}

void UInventoryWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GInventoryUICommand(
	TEXT("wor.UI.Inventory"),
	TEXT("PLACEHOLDER UI (B-339): a character's store and the shared chest with Give, Store, Take, Dismantle and Convert."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			if (!UInventoryWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Inventory] wor.UI.Inventory: no player controller"));
			}
		}));

static FAutoConsoleCommandWithWorld GInventoryPrintCommand(
	TEXT("wor.Inventory.PrintVerbs"),
	TEXT("DEBUG (B-339): each party member's store and the chest, with the verbs each row allows."),
	FConsoleCommandWithWorldDelegate::CreateLambda(
		[](UWorld *World)
		{
			const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
			const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
			const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
			if (!Party)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Inventory] wor.Inventory.PrintVerbs: no local party"));
				return;
			}
			for (const FPartyMember &Member : Party->Members)
			{
				if (Member.CharacterData)
				{
					UE_LOG(LogTemp, Log, TEXT("%s"), *WoRInventoryVerbs::GetRowsString(World, UPoolSubsystem::GetCharacterKey(Member.CharacterData)));
				}
			}
		}));
#endif // !UE_BUILD_SHIPPING

#undef LOCTEXT_NAMESPACE
