// NpcTalkWidget.cpp

#include "UI/Hub/NpcTalkWidget.h"

#include "Character/CharacterData.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Merchant/MerchantData.h"
#include "Merchant/MerchantInteractable.h"
#include "Player/WoRHubController.h"
#include "Shop/MerchantShopSubsystem.h"
#include "Story/StoryLinesData.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/Menus/VendorScreenModel.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRNpcTalkScreen"

namespace NpcTalkStyle
{
	// This screen's own layout (mockup hub/hud/2_npc_talk, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float BoxWidth = 1150.0f;
	constexpr float BoxPadding = 36.0f;
	constexpr float ChoiceWidth = 420.0f;
	constexpr float ChoicePadding = 15.0f;
	constexpr float ColumnGap = 30.0f;
	/** A choice row is made per line the longest list needs; more topics than this scroll by moving the light. */
	constexpr int32 MaxRows = 8;
}

UNpcTalkWidget::UNpcTalkWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself
}

UNpcTalkWidget *UNpcTalkWidget::Open(APlayerController *PC, UMerchantData *InMerchant, APawn *InInstigator)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[NpcTalk] Not opened: no AWoRHubController with a root layout"));
		return nullptr;
	}
	UNpcTalkWidget *Screen = Cast<UNpcTalkWidget>(Root->PushToMenu(UNpcTalkWidget::StaticClass()));
	if (Screen)
	{
		Screen->SetStory(StoryUnlocks::GatherStoryData());
		Screen->SetSource(InMerchant, InInstigator);
	}
	return Screen;
}

UNpcTalkWidget *UNpcTalkWidget::OpenForCharacter(APlayerController *PC, UCharacterData *InCharacter)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root || !InCharacter)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[NpcTalk] Not opened: no root layout or no character"));
		return nullptr;
	}
	UNpcTalkWidget *Screen = Cast<UNpcTalkWidget>(Root->PushToMenu(UNpcTalkWidget::StaticClass()));
	if (Screen)
	{
		Screen->SetStory(StoryUnlocks::GatherStoryData());
		Screen->SetCharacter(InCharacter);
	}
	return Screen;
}

void UNpcTalkWidget::SetCharacter(UCharacterData *InCharacter)
{
	Character = InCharacter;
	Merchant.Reset();
	Instigator.Reset();
	View = ENpcTalkView::Choices;
	Selected = 0;
	Spoken = FText::GetEmpty();
	Refresh();
	UE_LOG(LogWoRUI, Log, TEXT("[NpcTalk] Opened for a character: %s"), *WoRNpcTalk::GetTalkString(Page));
}

void UNpcTalkWidget::SetSource(UMerchantData *InMerchant, APawn *InInstigator)
{
	Character.Reset();
	Merchant = InMerchant;
	Instigator = InInstigator;
	View = ENpcTalkView::Choices;
	Selected = 0;
	Spoken = FText::GetEmpty();
	Refresh();
	UE_LOG(LogWoRUI, Log, TEXT("[NpcTalk] Opened: %s"), *WoRNpcTalk::GetTalkString(Page));
}

void UNpcTalkWidget::SetStory(const TArray<UStoryLinesData *> &InStory)
{
	Story.Reset();
	for (UStoryLinesData *Data : InStory)
	{
		Story.Add(Data);
	}
	Refresh();
}

UMemoryJournalSubsystem *UNpcTalkWidget::GetJournal() const
{
	const UWorld *World = GetWorld();
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UMemoryJournalSubsystem>() : nullptr;
}

TSharedRef<SWidget> UNpcTalkWidget::RebuildWidget()
{
	using namespace NpcTalkStyle;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// No backdrop: the world stays visible (UIScreens #20).
		UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
		Screen->SetBrushColor(FLinearColor::Transparent);
		Screen->SetPadding(FMargin(ScreenPadding));
		WidgetTree->RootWidget = Screen;
		UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Screen->SetContent(Main);
		AddToColumn(Main, WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UHorizontalBox *Bottom = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UBorder *Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Box->SetBrushColor(PanelColor);
		Box->SetPadding(FMargin(BoxPadding));
		UVerticalBox *BoxColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Box->SetContent(BoxColumn);
		RoleText = MakeText(WidgetTree, RowFontSize, CyanColor, true);
		AddToColumn(BoxColumn, RoleText, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		SpokenText = MakeWrappedText(WidgetTree, RowFontSize, TextColor);
		AddToColumn(BoxColumn, SpokenText);
		AddToRow(Bottom, MakeSized(WidgetTree, BoxWidth, 0.0f, Box), FMargin(0.0f, 0.0f, ColumnGap, 0.0f), VAlign_Bottom);

		ChoiceList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		for (int32 Index = 0; Index < MaxRows; ++Index)
		{
			UBorder *Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Row->SetBrushColor(PlateColor);
			Row->SetPadding(FMargin(ChoicePadding));
			UTextBlock *Label = MakeText(WidgetTree, RowFontSize, TextColor, true);
			Row->SetContent(Label);
			ChoiceRows.Add(Row);
			ChoiceLabels.Add(Label);
			AddToColumn(ChoiceList, Row, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		}
		AddToRow(Bottom, MakeSized(WidgetTree, ChoiceWidth, 0.0f, ChoiceList), FMargin(0.0f), VAlign_Bottom);
		AddToColumn(Main, Bottom);

		HintsText = MakeText(WidgetTree, LineFontSize, DimColor);
		AddToColumn(Main, HintsText, FMargin(0.0f, LineGap, 0.0f, 0.0f))->SetHorizontalAlignment(HAlign_Right);
	}
	return Super::RebuildWidget();
}

void UNpcTalkWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	View = ENpcTalkView::Choices;
	Selected = 0;
	Spoken = FText::GetEmpty();
	Refresh(); // Open sets the source right after (the log line is SetSource's)
}

UWidget *UNpcTalkWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UNpcTalkWidget *>(this); // the screen itself takes the keys (NativeOnKeyDown)
}

int32 UNpcTalkWidget::GetLineCount() const
{
	switch (View)
	{
	case ENpcTalkView::Topics:
		return Page.Topics.Num();
	case ENpcTalkView::Log:
		return 0;
	case ENpcTalkView::Choices:
	default:
		return Page.Choices.Num();
	}
}

FText UNpcTalkWidget::GetLineText(int32 Line) const
{
	const FText NewMark = LOCTEXT("NewMark", "NEW");
	if (View == ENpcTalkView::Topics)
	{
		if (!Page.Topics.IsValidIndex(Line))
		{
			return FText::GetEmpty();
		}
		const FNpcTalkTopic &Topic = Page.Topics[Line];
		return Topic.bNew ? FText::Format(LOCTEXT("TopicNew", "{0}   {1}"), Topic.Name, NewMark) : Topic.Name;
	}
	if (!Page.Choices.IsValidIndex(Line))
	{
		return FText::GetEmpty();
	}
	const FText Name = WoRNpcTalk::GetChoiceName(Page.Choices[Line]);
	return Page.Choices[Line] == ENpcTalkChoice::Talk && Page.bTalkHasNew ? FText::Format(LOCTEXT("ChoiceNew", "{0}   {1}"), Name, NewMark) : Name;
}

void UNpcTalkWidget::Refresh()
{
	using namespace NpcTalkStyle;
	Page = Character.IsValid() ? WoRNpcTalk::BuildForCharacter(Character.Get(), ObjectPtrDecay(Story), GetJournal())
							   : WoRNpcTalk::Build(Merchant.Get(), ObjectPtrDecay(Story), GetJournal());
	const int32 Count = GetLineCount();
	Selected = Count > 0 ? FMath::Clamp(Selected, 0, Count - 1) : 0;
	if (!RoleText)
	{
		return; // headless
	}
	RoleText->SetText(Page.Role);
	FText Shown = Spoken.IsEmpty() ? Page.Greeting : Spoken;
	if (View == ENpcTalkView::Log)
	{
		Shown = Page.Log.IsEmpty() ? LOCTEXT("LogEmpty", "Nothing heard yet.") : FText::Join(FText::FromString(TEXT("\n\n")), Page.Log);
	}
	else if (View == ENpcTalkView::Topics && Page.Topics.IsEmpty())
	{
		Shown = LOCTEXT("NoTopics", "Nothing to talk about yet.");
	}
	SpokenText->SetText(Shown);
	// Keep the lit line in the rows shown: a long topic list moves its window with the light.
	const int32 First = FMath::Clamp(Selected - MaxRows + 1, 0, FMath::Max(0, Count - MaxRows));
	for (int32 Index = 0; Index < ChoiceRows.Num(); ++Index)
	{
		const int32 Line = First + Index;
		const bool bShown = Line < Count;
		SetShown(ChoiceRows[Index], bShown);
		if (bShown)
		{
			ChoiceLabels[Index]->SetText(GetLineText(Line));
			ChoiceRows[Index]->SetBrushColor(Line == Selected ? FocusColor : PlateColor);
		}
	}
	switch (View)
	{
	case ENpcTalkView::Topics:
		HintsText->SetText(LOCTEXT("HintsTopics", "A  Hear        Y / R  Log        B / Esc  Back"));
		break;
	case ENpcTalkView::Log:
		HintsText->SetText(LOCTEXT("HintsLog", "Y / R / B / Esc  Back"));
		break;
	case ENpcTalkView::Choices:
	default:
		HintsText->SetText(LOCTEXT("HintsChoices", "A  Select        Y / R  Log        B / Esc  Leave"));
		break;
	}
}

void UNpcTalkWidget::MoveSelection(int32 Delta)
{
	const int32 Count = GetLineCount();
	if (Count > 0)
	{
		Selected = ((Selected + Delta) % Count + Count) % Count;
		Refresh();
	}
}

void UNpcTalkWidget::PressA()
{
	if (View == ENpcTalkView::Topics)
	{
		if (!Page.Topics.IsValidIndex(Selected))
		{
			return;
		}
		const FNpcTalkTopic Topic = Page.Topics[Selected];
		Spoken = Topic.Line;
		if (UMemoryJournalSubsystem *Journal = GetJournal())
		{
			WoRNpcTalk::Hear(*Journal, Topic);
			UE_LOG(LogWoRUI, Log, TEXT("[NpcTalk] %s %s heard"), Topic.bConversation ? TEXT("Conversation") : TEXT("V line"), *Topic.LineId.ToString());
		}
		Refresh(); // the next line may unlock, the NEW mark goes
		return;
	}
	if (View != ENpcTalkView::Choices || !Page.Choices.IsValidIndex(Selected))
	{
		return;
	}
	switch (Page.Choices[Selected])
	{
	case ENpcTalkChoice::Shop:
		OpenShop();
		return;
	case ENpcTalkChoice::Talk:
		View = ENpcTalkView::Topics;
		// Start on the NEW line when there is one.
		Selected = FMath::Max(0, Page.Topics.IndexOfByPredicate([](const FNpcTalkTopic &Topic) { return Topic.bNew; }));
		Refresh();
		return;
	case ENpcTalkChoice::Leave:
	default:
		DeactivateWidget();
		return;
	}
}

void UNpcTalkWidget::OpenShop()
{
	UMerchantData *Shopkeeper = Merchant.Get();
	APawn *Pawn = Instigator.Get();
	UWorld *World = GetWorld();
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UMerchantShopSubsystem *Shop = GI ? GI->GetSubsystem<UMerchantShopSubsystem>() : nullptr;
	// The vendor's screen opens only for a vendor with tabs (VendorScreenWidget); else the box stays and says so.
	if (!Shopkeeper || !Pawn || !Shop || WoRVendorScreen::GetTabs(Shopkeeper->MerchantType).IsEmpty())
	{
		Spoken = LOCTEXT("ShopClosed", "The shop can't open right now.");
		UE_LOG(LogWoRUI, Warning, TEXT("[NpcTalk] Shop: missing %s"), !Shopkeeper ? TEXT("merchant") : !Pawn ? TEXT("pawn") : TEXT("shop subsystem"));
		Refresh();
		return;
	}
	DeactivateWidget(); // the vendor's screen takes the menu layer
	Shop->OpenForMerchant(Shopkeeper, Pawn);
}

void UNpcTalkWidget::PressLog()
{
	if (View == ENpcTalkView::Log)
	{
		View = ViewBeforeLog;
		Selected = SelectedBeforeLog;
	}
	else
	{
		ViewBeforeLog = View;
		SelectedBeforeLog = Selected;
		View = ENpcTalkView::Log;
		Selected = 0;
	}
	Refresh();
}

bool UNpcTalkWidget::NativeOnHandleBackAction()
{
	if (View == ENpcTalkView::Log)
	{
		PressLog();
		return true;
	}
	if (View == ENpcTalkView::Topics)
	{
		View = ENpcTalkView::Choices;
		Selected = Page.Choices.IndexOfByKey(ENpcTalkChoice::Talk);
		Spoken = FText::GetEmpty();
		Refresh();
		return true;
	}
	return Super::NativeOnHandleBackAction(); // Leave
}

FReply UNpcTalkWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveSelection(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsConfirmKey(Key) || IsSecondActionKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			IsConfirmKey(Key) ? PressA() : PressLog();
		}
		return FReply::Handled();
	}
	if (IsBackKey(Key))
	{
		// Reached only when CommonUI's own Back action didn't take the key (BP_CommonInputData unset, B-103).
		if (!InKeyEvent.IsRepeat())
		{
			HandleBackAction();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GNpcTalkOpenCommand(
	TEXT("wor.UI.NpcTalk"),
	TEXT("B-471c: open the talk box for the vendor the player stands by (V with no shop when none): Shop / Talk / Leave, Y the log."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn *Pawn = PC ? PC->GetPawn() : nullptr;
		AMerchantInteractable *Vendor = Pawn ? AMerchantInteractable::FindNearestInRange(Pawn) : nullptr;
		if (!UNpcTalkWidget::Open(PC, Vendor ? Vendor->Merchant.Get() : nullptr, Pawn))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[NpcTalk] wor.UI.NpcTalk: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
