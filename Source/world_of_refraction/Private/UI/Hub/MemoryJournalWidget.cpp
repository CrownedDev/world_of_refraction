// MemoryJournalWidget.cpp

#include "UI/Hub/MemoryJournalWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Story/StoryLinesData.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "MemoryJournal"

namespace MemoryJournalStyle
{
	// TODO: style pass (placeholder until the CommonUI screens).
	const FLinearColor OverlayColor(0.0f, 0.0f, 0.0f, 0.75f);
	constexpr int32 FontSize = 20;
	constexpr float RowPadding = 6.0f;
	constexpr int32 ZOrder = 50;
}

namespace MemoryJournalBuild
{
	UTextBlock *MakeText(UWidgetTree *Tree)
	{
		UTextBlock *Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = MemoryJournalStyle::FontSize;
		Text->SetFont(Font);
		Text->SetAutoWrapText(true);
		return Text;
	}

	void AddRow(UVerticalBox *Box, UWidget *Row)
	{
		if (UVerticalBoxSlot *BoxSlot = Box->AddChildToVerticalBox(Row))
		{
			BoxSlot->SetPadding(FMargin(MemoryJournalStyle::RowPadding));
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
}

UMemoryJournalWidget *UMemoryJournalWidget::Open(APlayerController *PC)
{
	if (!PC)
	{
		return nullptr;
	}
	UMemoryJournalWidget *Widget = CreateWidget<UMemoryJournalWidget>(PC, UMemoryJournalWidget::StaticClass());
	if (!Widget)
	{
		return nullptr;
	}
	Widget->SetStory(StoryUnlocks::GatherStoryData());
	Widget->AddToViewport(MemoryJournalStyle::ZOrder);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	return Widget;
}

TSharedRef<SWidget> UMemoryJournalWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Overlay"));
		Overlay->SetBrushColor(MemoryJournalStyle::OverlayColor);
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Overlay;

		UVerticalBox *Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
		Overlay->SetContent(Box);

		TitleText = MemoryJournalBuild::MakeText(WidgetTree);
		MemoryJournalBuild::AddRow(Box, TitleText);
		BodyText = MemoryJournalBuild::MakeText(WidgetTree);
		MemoryJournalBuild::AddRow(Box, BodyText);

		MemoryJournalBuild::AddButton(WidgetTree, Box, LOCTEXT("JournalButton", "Journal"))
			->OnClicked.AddDynamic(this, &UMemoryJournalWidget::HandleJournalClicked);
		MemoryJournalBuild::AddButton(WidgetTree, Box, LOCTEXT("VButton", "Talk to V"))
			->OnClicked.AddDynamic(this, &UMemoryJournalWidget::HandleVClicked);
		MemoryJournalBuild::AddButton(WidgetTree, Box, LOCTEXT("ConversationsButton", "Conversations (next)"))
			->OnClicked.AddDynamic(this, &UMemoryJournalWidget::HandleConversationsClicked);
		MemoryJournalBuild::AddButton(WidgetTree, Box, LOCTEXT("NextButton", "Next"))
			->OnClicked.AddDynamic(this, &UMemoryJournalWidget::HandleNextClicked);
		MemoryJournalBuild::AddButton(WidgetTree, Box, LOCTEXT("CloseButton", "Close"))
			->OnClicked.AddDynamic(this, &UMemoryJournalWidget::HandleCloseClicked);
	}
	return Super::RebuildWidget();
}

void UMemoryJournalWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ShowJournalPage();
}

void UMemoryJournalWidget::SetStory(const TArray<UStoryLinesData *> &InStory)
{
	Story = InStory;
	ConversationIndex = INDEX_NONE;
	LineIndex = 0;
}

UMemoryJournalSubsystem *UMemoryJournalWidget::GetJournal() const
{
	const UGameInstance *GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
}

void UMemoryJournalWidget::SetPage(const FText &NewTitle, const FText &NewBody)
{
	Title = NewTitle;
	Body = NewBody;
	if (TitleText && BodyText)
	{
		TitleText->SetText(Title);
		BodyText->SetText(Body);
	}
}

void UMemoryJournalWidget::ShowJournalPage()
{
	Page = EPage::Journal;
	const UMemoryJournalSubsystem *Journal = GetJournal();
	if (!Journal)
	{
		SetPage(LOCTEXT("JournalTitle", "Memory journal"), LOCTEXT("NoJournal", "(no journal)"));
		return;
	}
	TArray<FText> Lines;
	Lines.Add(LOCTEXT("MemoriesHeader", "Memories:"));
	for (const UMemoryData *Memory : Journal->GetMemories())
	{
		if (Memory)
		{
			Lines.Add(FText::Format(LOCTEXT("MemoryRow", "- {0}: {1}"), Memory->Title, Memory->Text));
		}
	}
	Lines.Add(LOCTEXT("LoreHeader", "Lore:"));
	for (const FJournalLoreEntry &Entry : Journal->GetLore())
	{
		Lines.Add(FText::Format(LOCTEXT("LoreRow", "- {0}: {1}"), Entry.Title, Entry.Body));
	}
	Lines.Add(LOCTEXT("PlayedHeader", "Conversations:"));
	for (const FStoryConversation *Talk : StoryUnlocks::GetUnlockedConversations(ObjectPtrDecay(Story), *Journal))
	{
		if (Journal->HasPlayedConversation(Talk->ConversationId))
		{
			Lines.Add(FText::Format(LOCTEXT("PlayedRow", "- {0}"), Talk->Title));
		}
	}
	SetPage(FText::Format(LOCTEXT("JournalCount", "Memory journal ({0} memories, {1} lore)"),
						  FText::AsNumber(Journal->GetMemories().Num()), FText::AsNumber(Journal->GetLore().Num())),
			FText::Join(FText::FromString(TEXT("\n")), Lines));
}

bool UMemoryJournalWidget::HearNextVLine()
{
	Page = EPage::V;
	UMemoryJournalSubsystem *Journal = GetJournal();
	const FVLine *Line = Journal ? StoryUnlocks::FindNextVLine(ObjectPtrDecay(Story), *Journal) : nullptr;
	if (!Line)
	{
		SetPage(LOCTEXT("VTitle", "V"), LOCTEXT("VNothing", "V has nothing new to say."));
		return false;
	}
	Journal->MarkVLineHeard(Line->LineId);
	UE_LOG(LogWoR, Log, TEXT("[MemoryJournal] V line %s heard"), *Line->LineId.ToString());
	SetPage(LOCTEXT("VTitle", "V"), Line->Text);
	return true;
}

bool UMemoryJournalWidget::SelectNextConversation()
{
	Page = EPage::Conversation;
	const UMemoryJournalSubsystem *Journal = GetJournal();
	const int32 Unlocked = Journal ? StoryUnlocks::GetUnlockedConversations(ObjectPtrDecay(Story), *Journal).Num() : 0;
	if (Unlocked == 0)
	{
		ConversationIndex = INDEX_NONE;
		SetPage(LOCTEXT("TalkTitle", "Conversations"), LOCTEXT("NoTalks", "No conversation yet. Recover more memories."));
		return false;
	}
	ConversationIndex = (ConversationIndex + 1) % Unlocked;
	LineIndex = 0;
	ShowConversationLine();
	return true;
}

bool UMemoryJournalWidget::AdvanceConversation()
{
	UMemoryJournalSubsystem *Journal = GetJournal();
	const TArray<const FStoryConversation *> Unlocked =
		Journal ? StoryUnlocks::GetUnlockedConversations(ObjectPtrDecay(Story), *Journal) : TArray<const FStoryConversation *>();
	if (Page != EPage::Conversation || !Unlocked.IsValidIndex(ConversationIndex) ||
		LineIndex >= Unlocked[ConversationIndex]->Lines.Num())
	{
		return false;
	}
	++LineIndex;
	if (LineIndex >= Unlocked[ConversationIndex]->Lines.Num())
	{
		Journal->MarkConversationPlayed(Unlocked[ConversationIndex]->ConversationId);
		UE_LOG(LogWoR, Log, TEXT("[MemoryJournal] Conversation %s played"), *Unlocked[ConversationIndex]->ConversationId.ToString());
	}
	ShowConversationLine();
	return true;
}

void UMemoryJournalWidget::ShowConversationLine()
{
	const UMemoryJournalSubsystem *Journal = GetJournal();
	const TArray<const FStoryConversation *> Unlocked =
		Journal ? StoryUnlocks::GetUnlockedConversations(ObjectPtrDecay(Story), *Journal) : TArray<const FStoryConversation *>();
	if (!Unlocked.IsValidIndex(ConversationIndex))
	{
		return;
	}
	const FStoryConversation &Talk = *Unlocked[ConversationIndex];
	const FText Heading = FText::Format(LOCTEXT("TalkHeading", "{0} ({1}/{2}){3}"), Talk.Title,
										FText::AsNumber(ConversationIndex + 1), FText::AsNumber(Unlocked.Num()),
										Journal->HasPlayedConversation(Talk.ConversationId) ? FText::GetEmpty() : LOCTEXT("TalkNew", " - new"));
	SetPage(Heading, Talk.Lines.IsValidIndex(LineIndex) ? Talk.Lines[LineIndex] : LOCTEXT("TalkEnd", "(end)"));
}

FString UMemoryJournalWidget::GetJournalWidgetString() const
{
	return FString::Printf(TEXT("[MemoryJournal] %s\n%s"), *Title.ToString(), *Body.ToString());
}

void UMemoryJournalWidget::HandleJournalClicked()
{
	ShowJournalPage();
}

void UMemoryJournalWidget::HandleVClicked()
{
	HearNextVLine();
}

void UMemoryJournalWidget::HandleConversationsClicked()
{
	SelectNextConversation();
}

void UMemoryJournalWidget::HandleNextClicked()
{
	if (Page == EPage::V)
	{
		HearNextVLine();
	}
	else if (Page == EPage::Conversation)
	{
		AdvanceConversation();
	}
}

void UMemoryJournalWidget::HandleCloseClicked()
{
	if (APlayerController *PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GJournalOpenCommand(
	TEXT("wor.Journal.Open"),
	TEXT("Open the placeholder memory journal: memories and lore to reread, V's next line, conversations (B-194)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UMemoryJournalWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoR, Warning, TEXT("[MemoryJournal] wor.Journal.Open: no player controller (no PIE world?)."));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
