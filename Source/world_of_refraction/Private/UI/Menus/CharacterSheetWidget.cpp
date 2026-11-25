// CharacterSheetWidget.cpp

#include "UI/Menus/CharacterSheetWidget.h"

#include "Combat/Actions/ActionPipsRules.h"
#include "Components/ScrollBox.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Player/WoRHubController.h"
#include "Pool/PoolSubsystem.h"
#include "UI/Menus/LoadoutV2ScreenWidget.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRIconTable.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRCharacterSheetWidget"

namespace CharacterSheetStyle
{
	// This screen's own layout (screen_mockups/menus/profile/v1, the loadout screen's v5 columns, 1280x720 at 1080p);
	// the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float LeftWidth = 600.0f;
	constexpr float MiddleWidth = 618.0f;
	constexpr float RightWidth = 504.0f;
	constexpr float PanelGap = 27.0f;
	constexpr float PanelPadding = 24.0f;
	constexpr float PlatePaddingX = 21.0f;
	constexpr float PlatePaddingY = 12.0f;
	constexpr float PlateGap = 10.0f;
	constexpr float BadgeSize = 84.0f;
	constexpr float VitalBarWidth = 405.0f;
	constexpr float EntryBarHeight = 15.0f;
	constexpr float DividerHeight = 2.0f;
	constexpr int32 MaxEntries = WoRCharacterSheet::MAX_GROUP_ENTRIES;
	constexpr int32 MaxSources = 6; // the pillar, gear, stone, buffs, a cap, and one spare

	UBorder *MakePanel(UWidgetTree *Tree, UVerticalBox *&OutColumn)
	{
		UBorder *Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(PanelPadding));
		OutColumn = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(OutColumn);
		return Panel;
	}

	UBorder *MakePlate(UWidgetTree *Tree, UWidget *Content)
	{
		UBorder *Plate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Plate->SetBrushColor(PlateColor);
		Plate->SetPadding(FMargin(PlatePaddingX, PlatePaddingY));
		Plate->SetContent(Content);
		return Plate;
	}

	/** "Label ........ value" on one line; returns the line, with its two texts. */
	UHorizontalBox *MakeLine(UWidgetTree *Tree, int32 FontSize, UTextBlock *&OutLabel, UTextBlock *&OutValue)
	{
		UHorizontalBox *Line = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		OutLabel = MakeText(Tree, FontSize, DimColor);
		OutValue = MakeText(Tree, FontSize, TextColor, true);
		AddToRow(Line, OutLabel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AddToRow(Line, OutValue)->SetHorizontalAlignment(HAlign_Right);
		return Line;
	}

	FLinearColor PlateFill(bool bLit, bool bHovered)
	{
		return bLit ? FocusColor : bHovered ? HoverColor : PlateColor;
	}
}

UCharacterSheetWidget::UCharacterSheetWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself
}

UCharacterSheetWidget *UCharacterSheetWidget::Open(APlayerController *PC, int32 InMemberIndex)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[CharacterSheet] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	UCharacterSheetWidget *Screen = Cast<UCharacterSheetWidget>(Root->PushToMenu(UCharacterSheetWidget::StaticClass()));
	if (Screen && InMemberIndex != Screen->MemberIndex)
	{
		Screen->MemberIndex = InMemberIndex; // Refresh clamps it to the party
		Screen->Refresh();
	}
	return Screen;
}

TSharedRef<SWidget> UCharacterSheetWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UCharacterSheetWidget::BuildTree()
{
	using namespace CharacterSheetStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(ScreenColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	UTextBlock *Title = MakeText(WidgetTree, TitleFontSize, TextColor, true);
	Title->SetText(LOCTEXT("Title", "Profile"));
	AddToColumn(Main, Title);

	UHorizontalBox *MemberRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(MemberRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabPrev", "{SubTabPrev}")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (int32 Index = 0; Index < PartyConstants::MAX_PARTY_MEMBERS; ++Index)
	{
		UTextBlock *Label = nullptr;
		MemberTabs.Add(MakeTab(WidgetTree, Label));
		MemberTabLabels.Add(Label);
		AddToRow(MemberRow, MemberTabs.Last(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	AddToRow(MemberRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabNext", "{SubTabNext}")), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(Main, MemberRow, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	RefusalText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	AddToColumn(Main, RefusalText, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body, FMargin(0.0f, SectionGap, 0.0f, SectionGap))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Body, MakeSized(WidgetTree, LeftWidth, 0.0f, BuildLeft()), FMargin(0.0f), VAlign_Fill);
	AddToRow(Body, MakeSized(WidgetTree, MiddleWidth, 0.0f, BuildMiddle()), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);
	AddToRow(Body, MakeSized(WidgetTree, RightWidth, 0.0f, BuildRight()), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	HintsText = MakeHintBar(WidgetTree); // B-481 / B-483: key caps of the device in use, each clickable
	AddToColumn(Main, HintsText)->SetHorizontalAlignment(HAlign_Right);
}

UWidget *UCharacterSheetWidget::BuildLeft()
{
	using namespace CharacterSheetStyle;

	UVerticalBox *Column = nullptr;
	UBorder *Panel = MakePanel(WidgetTree, Column);

	// Who they are: the element badge, the name and tier, the class line.
	UHorizontalBox *Who = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ElementBadge = MakeIcon(WidgetTree, BadgeSize);
	AddToRow(Who, ElementBadge, FMargin(0.0f, 0.0f, SectionGap, 0.0f), VAlign_Center);
	UVerticalBox *WhoText = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	NameText = MakeText(WidgetTree, NameFontSize, TextColor, true);
	ClassLineText = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(WhoText, NameText);
	AddToColumn(WhoText, ClassLineText);
	AddToRow(Who, WhoText, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToColumn(Column, Who, FMargin(0.0f, 0.0f, 0.0f, SectionGap));

	const auto AddVital = [this, Column](const FLinearColor &Color, TObjectPtr<UProgressBar> &OutBar, TObjectPtr<UTextBlock> &OutValue)
	{
		UProgressBar *Bar = nullptr;
		UTextBlock *Value = nullptr;
		UWidget *BarWithValue = MakeBarWithValue(WidgetTree, Color, Bar, Value);
		AddToColumn(Column, MakeSized(WidgetTree, 0.0f, BarWithValueHeight, BarWithValue), FMargin(0.0f, 0.0f, 0.0f, LineGap));
		OutBar = Bar;
		OutValue = Value;
	};
	AddVital(HPColor, HPBar, HPValue);
	AddVital(EPColor, EPBar, EPValue);
	UTextBlock *ActionsLabel = nullptr;
	UTextBlock *Actions = nullptr;
	UHorizontalBox *ActionsLine = MakeLine(WidgetTree, LineFontSize, ActionsLabel, Actions);
	ActionsLabel->SetText(LOCTEXT("MaxActions", "Max actions"));
	ActionsValue = Actions;
	AddToColumn(Column, ActionsLine, FMargin(0.0f, 0.0f, 0.0f, SectionGap));

	for (int32 Index = 0; Index < WoRCharacterSheet::GROUP_COUNT; ++Index)
	{
		FGroupWidgets Widgets;
		UHorizontalBox *Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox *Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
		UTextBlock *Blurb = MakeText(WidgetTree, SmallFontSize, DimColor);
		AddToColumn(Text, Name);
		AddToColumn(Text, Blurb);
		AddToRow(Row, Text)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UTextBlock *Count = MakeText(WidgetTree, NameFontSize, TextColor, true);
		AddToRow(Row, Count, FMargin(0.0f), VAlign_Center);
		UBorder *Frame = MakePlate(WidgetTree, Row);
		AddToColumn(Column, Frame, FMargin(0.0f, 0.0f, 0.0f, PlateGap));
		Widgets.Frame = Frame;
		Widgets.Name = Name;
		Widgets.Blurb = Blurb;
		Widgets.Count = Count;
		GroupWidgets.Add(MoveTemp(Widgets));
	}
	return Panel;
}

UWidget *UCharacterSheetWidget::BuildMiddle()
{
	using namespace CharacterSheetStyle;

	UVerticalBox *Column = nullptr;
	UBorder *Panel = MakePanel(WidgetTree, Column);
	UHorizontalBox *Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	GroupTitle = MakeText(WidgetTree, NameFontSize, TextColor, true);
	GroupCount = MakeText(WidgetTree, TitleFontSize, CyanColor, true);
	AddToRow(Head, GroupTitle)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Head, GroupCount);
	AddToColumn(Column, Head);
	GroupSubtitle = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(Column, GroupSubtitle, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	NoEntriesText = MakeText(WidgetTree, LineFontSize, OffColor);
	NoEntriesText->SetText(LOCTEXT("NoEntries", "None"));
	AddToColumn(Column, NoEntriesText);

	EntryScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	EntryScroll->SetScrollBarVisibility(ESlateVisibility::Visible); // the 12 resistance lines run past the panel
	AddToColumn(Column, EntryScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	for (int32 Index = 0; Index < MaxEntries; ++Index)
	{
		FEntryWidgets Widgets;
		UVerticalBox *Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock *Label = nullptr;
		UTextBlock *Value = nullptr;
		UHorizontalBox *Line = MakeLine(WidgetTree, RowFontSize, Label, Value);
		Label->SetColorAndOpacity(FSlateColor(TextColor));
		AddToColumn(Content, Line, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		UProgressBar *Bar = MakeBar(WidgetTree, DimColor);
		AddToColumn(Content, MakeSized(WidgetTree, 0.0f, EntryBarHeight, Bar));
		UTextBlock *Text = MakeWrappedText(WidgetTree, SmallFontSize, DimColor);
		AddToColumn(Content, Text);
		UBorder *Frame = MakePlate(WidgetTree, Content);
		EntryScroll->AddChild(Frame);
		Frame->SetPadding(FMargin(PlatePaddingX, PlatePaddingY)); // AddChild leaves the border's own padding as set
		Widgets.Frame = Frame;
		Widgets.Label = Label;
		Widgets.Value = Value;
		Widgets.Bar = Bar;
		Widgets.Text = Text;
		EntryWidgets.Add(MoveTemp(Widgets));
	}
	return Panel;
}

UWidget *UCharacterSheetWidget::BuildRight()
{
	using namespace CharacterSheetStyle;

	UVerticalBox *Column = nullptr;
	UBorder *Panel = MakePanel(WidgetTree, Column);
	UTextBlock *Heading = MakeText(WidgetTree, RowFontSize, TextColor, true);
	Heading->SetText(LOCTEXT("Details", "Details"));
	AddToColumn(Column, Heading);
	DetailName = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(Column, DetailName, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	DetailDescription = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
	AddToColumn(Column, DetailDescription, FMargin(0.0f, 0.0f, 0.0f, SectionGap));

	// Where it comes from: the layers, a divider, the total.
	UVerticalBox *Sources = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *SourcesHeading = MakeText(WidgetTree, LineFontSize, TextColor, true);
	SourcesHeading->SetText(LOCTEXT("WhereFrom", "Where it comes from"));
	AddToColumn(Sources, SourcesHeading, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	for (int32 Index = 0; Index < MaxSources; ++Index)
	{
		FSourceWidgets Widgets;
		UTextBlock *Label = nullptr;
		UTextBlock *Value = nullptr;
		UHorizontalBox *Line = MakeLine(WidgetTree, LineFontSize, Label, Value);
		AddToColumn(Sources, Line, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		Widgets.Row = Line;
		Widgets.Label = Label;
		Widgets.Value = Value;
		SourceWidgets.Add(MoveTemp(Widgets));
	}
	UBorder *Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Divider->SetBrushColor(TrackColor);
	AddToColumn(Sources, MakeSized(WidgetTree, 0.0f, DividerHeight, Divider), FMargin(0.0f, 0.0f, 0.0f, LineGap));
	UTextBlock *TotalLabel = nullptr;
	UTextBlock *Total = nullptr;
	UHorizontalBox *TotalLine = MakeLine(WidgetTree, LineFontSize, TotalLabel, Total);
	TotalLabel->SetText(LOCTEXT("Total", "Total"));
	TotalLabel->SetColorAndOpacity(FSlateColor(TextColor));
	Total->SetColorAndOpacity(FSlateColor(CyanColor));
	TotalValue = Total;
	AddToColumn(Sources, TotalLine);
	SourcesBlock = Sources;
	AddToColumn(Column, Sources);
	return Panel;
}

void UCharacterSheetWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	bOnEntries = false;
	Refresh();
	UE_LOG(LogWoRUI, Log, TEXT("[CharacterSheet] Opened (%d member(s), on %d)"), Members.Num(), MemberIndex);
}

UWidget *UCharacterSheetWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<UCharacterSheetWidget *>(this);
}

void UCharacterSheetWidget::Refresh()
{
	using namespace CharacterSheetStyle;

	UWorld *World = GetWorld();
	Members = WoRPauseMenu::BuildParty(World);
	MemberIndex = Members.Num() > 0 ? FMath::Clamp(MemberIndex, 0, Members.Num() - 1) : 0;
	Page = WoRCharacterSheet::BuildPage(World, MemberIndex);
	Groups = WoRCharacterSheet::BuildGroups(Page);
	// A member with no groups (not in this level) keeps the lit group and entry for the next one.
	if (Groups.Num() > 0)
	{
		GroupIndex = FMath::Clamp(GroupIndex, 0, Groups.Num() - 1);
		const int32 EntryCount = Groups[GroupIndex].Entries.Num();
		EntryIndex = EntryCount > 0 ? FMath::Clamp(EntryIndex, 0, EntryCount - 1) : 0;
		bOnEntries = bOnEntries && EntryCount > 0;
	}
	else
	{
		bOnEntries = false;
	}

	for (int32 Index = 0; Index < MemberTabs.Num(); ++Index)
	{
		const bool bShown = Members.IsValidIndex(Index);
		SetShown(MemberTabs[Index], bShown);
		if (bShown)
		{
			MemberTabLabels[Index]->SetText(Members[Index].Name);
			MemberTabLabels[Index]->SetColorAndOpacity(FSlateColor(Index == MemberIndex ? TextColor : DimColor));
		}
	}
	PaintMemberTabs();
	if (!NameText)
	{
		return; // headless: no tree
	}
	RefusalText->SetText(Page.Refusal);
	SetShown(RefusalText, !Page.Refusal.IsEmpty());

	const FPauseMenuMember &Who = Page.Who;
	const FText ValueFormat = LOCTEXT("VitalValue", "{0} / {1}");
	SetIcon(ElementBadge, WoRIcons::Find(Who.ElementIcon), /*bKeepSpace=*/true);
	NameText->SetText(Who.Tier.IsEmpty() ? Who.Name : FText::Format(LOCTEXT("NameAndTier", "{0} ({1})"), Who.Name, Who.Tier));
	ClassLineText->SetText(Who.SheetLine); // D-254: Spirit of ... / Body of ... stay on the sheet
	HPBar->SetPercent(Who.MaxHP > 0 ? static_cast<float>(Who.HP) / Who.MaxHP : 0.0f);
	HPValue->SetText(FText::Format(ValueFormat, FText::AsNumber(Who.HP), FText::AsNumber(Who.MaxHP)));
	EPBar->SetPercent(Who.MaxEP > 0 ? static_cast<float>(Who.EP) / Who.MaxEP : 0.0f);
	EPValue->SetText(FText::Format(ValueFormat, FText::AsNumber(Who.EP), FText::AsNumber(Who.MaxEP)));
	ActionsValue->SetText(Page.Refusal.IsEmpty() ? FText::Format(LOCTEXT("ActionsValue", "{0}  ·  regains {1} a turn"), FText::AsNumber(Page.MaxActions),
																 FText::AsNumber(Page.ActionRegain))
												 : FText::GetEmpty());
	RefreshSelection();
}

void UCharacterSheetWidget::RefreshSelection()
{
	using namespace CharacterSheetStyle;
	if (!GroupTitle)
	{
		return; // headless
	}
	for (int32 Index = 0; Index < GroupWidgets.Num(); ++Index)
	{
		const FGroupWidgets &Widgets = GroupWidgets[Index];
		const bool bShown = Groups.IsValidIndex(Index);
		SetShown(Widgets.Frame.Get(), bShown);
		if (!bShown || !Widgets.Frame.IsValid() || !Widgets.Name.IsValid() || !Widgets.Blurb.IsValid() || !Widgets.Count.IsValid())
		{
			continue;
		}
		const bool bLit = Index == GroupIndex;
		Widgets.Frame->SetBrushColor(PlateFill(bLit, HoverKind == EHoverKind::Group && HoverIndex == Index));
		Widgets.Name->SetText(Groups[Index].Name);
		Widgets.Blurb->SetText(Groups[Index].Blurb);
		Widgets.Blurb->SetColorAndOpacity(FSlateColor(bLit ? TextColor : DimColor));
		Widgets.Count->SetText(Groups[Index].Count);
		Widgets.Count->SetColorAndOpacity(FSlateColor(bLit ? CyanColor : TextColor));
	}

	const FCharacterSheetGroup *Group = Groups.IsValidIndex(GroupIndex) ? &Groups[GroupIndex] : nullptr;
	GroupTitle->SetText(Group ? Group->Name : FText::GetEmpty());
	GroupCount->SetText(Group ? Group->Count : FText::GetEmpty());
	GroupSubtitle->SetText(Group ? Group->Subtitle : FText::GetEmpty());
	SetShown(NoEntriesText, Group && Group->Entries.IsEmpty());
	for (int32 Index = 0; Index < EntryWidgets.Num(); ++Index)
	{
		const FEntryWidgets &Widgets = EntryWidgets[Index];
		const bool bShown = Group && Group->Entries.IsValidIndex(Index);
		SetShown(Widgets.Frame.Get(), bShown);
		if (!bShown || !Widgets.Frame.IsValid() || !Widgets.Label.IsValid() || !Widgets.Value.IsValid() || !Widgets.Bar.IsValid() || !Widgets.Text.IsValid())
		{
			continue;
		}
		const FCharacterSheetStat &Entry = Group->Entries[Index];
		const bool bLit = Index == EntryIndex;
		// A curse has no number: its text in the bar's place.
		const bool bHasBar = !Entry.Value.IsEmpty();
		Widgets.Frame->SetBrushColor(PlateFill(bLit, HoverKind == EHoverKind::Entry && HoverIndex == Index));
		Widgets.Label->SetText(Entry.Label);
		Widgets.Value->SetText(Entry.Value);
		Widgets.Value->SetColorAndOpacity(FSlateColor(Entry.bWeak ? LowHPColor : TextColor));
		SetShown(Widgets.Bar->GetParent(), bHasBar);
		Widgets.Bar->SetPercent(Entry.Fill);
		Widgets.Bar->SetFillColorAndOpacity(Entry.bWeak ? LowHPColor : bLit ? CyanColor : DimColor);
		SetShown(Widgets.Text.Get(), !bHasBar);
		Widgets.Text->SetText(bHasBar ? FText::GetEmpty() : Entry.Description);
	}
	if (EntryScroll && EntryWidgets.IsValidIndex(EntryIndex) && EntryWidgets[EntryIndex].Frame.IsValid())
	{
		EntryScroll->ScrollWidgetIntoView(EntryWidgets[EntryIndex].Frame.Get(), /*AnimateScroll=*/false);
	}
	WoRKeyHints::SetHint(HintsText, bOnEntries ? LOCTEXT("HintsEntries", "{SubTabs}  Member        {Second}  Loadout        {Back}  Groups")
											   : LOCTEXT("HintsGroups", "{SubTabs}  Member        {Confirm}  Select        {Second}  Loadout        {Back}  Back"));
	RefreshDetails();
}

void UCharacterSheetWidget::RefreshDetails()
{
	if (!DetailName)
	{
		return; // headless
	}
	const FCharacterSheetStat *Entry = GetLitEntry();
	DetailName->SetText(Entry ? Entry->Label : FText::GetEmpty());
	DetailDescription->SetText(Entry ? Entry->Description : FText::GetEmpty());
	const bool bHasSources = Entry && !Entry->Sources.IsEmpty();
	WoRMenuStyle::SetShown(SourcesBlock, bHasSources);
	for (int32 Index = 0; Index < SourceWidgets.Num(); ++Index)
	{
		const FSourceWidgets &Widgets = SourceWidgets[Index];
		const bool bShown = bHasSources && Entry->Sources.IsValidIndex(Index);
		WoRMenuStyle::SetShown(Widgets.Row.Get(), bShown);
		if (bShown && Widgets.Label.IsValid() && Widgets.Value.IsValid())
		{
			Widgets.Label->SetText(Entry->Sources[Index].Label);
			Widgets.Value->SetText(Entry->Sources[Index].Value);
		}
	}
	TotalValue->SetText(bHasSources ? Entry->Value : FText::GetEmpty());
}

const FCharacterSheetStat *UCharacterSheetWidget::GetLitEntry() const
{
	return Groups.IsValidIndex(GroupIndex) && Groups[GroupIndex].Entries.IsValidIndex(EntryIndex) ? &Groups[GroupIndex].Entries[EntryIndex] : nullptr;
}

void UCharacterSheetWidget::MoveMember(int32 Delta)
{
	if (Members.Num() < 2)
	{
		return;
	}
	MemberIndex = ((MemberIndex + Delta) % Members.Num() + Members.Num()) % Members.Num();
	Refresh(); // the lit group and entry stay, so LT / RT compares one stat across the party
}

void UCharacterSheetWidget::MoveInColumn(int32 Delta)
{
	const int32 Count = bOnEntries ? (Groups.IsValidIndex(GroupIndex) ? Groups[GroupIndex].Entries.Num() : 0) : Groups.Num();
	if (Count == 0)
	{
		return;
	}
	const int32 Current = bOnEntries ? EntryIndex : GroupIndex;
	const int32 Next = ((Current + Delta) % Count + Count) % Count;
	if (bOnEntries)
	{
		SelectEntry(Next);
	}
	else
	{
		SelectGroup(Next);
	}
}

void UCharacterSheetWidget::SelectGroup(int32 Group)
{
	if (!Groups.IsValidIndex(Group))
	{
		return;
	}
	if (Group != GroupIndex)
	{
		GroupIndex = Group;
		EntryIndex = 0;
	}
	bOnEntries = false;
	RefreshSelection();
}

void UCharacterSheetWidget::SelectEntry(int32 Entry)
{
	if (!Groups.IsValidIndex(GroupIndex) || !Groups[GroupIndex].Entries.IsValidIndex(Entry))
	{
		return;
	}
	EntryIndex = Entry;
	bOnEntries = true;
	RefreshSelection();
}

void UCharacterSheetWidget::PressSelect()
{
	if (!bOnEntries)
	{
		SelectEntry(EntryIndex);
	}
}

void UCharacterSheetWidget::PressBack()
{
	HandleBackAction();
}

bool UCharacterSheetWidget::NativeOnHandleBackAction()
{
	if (bOnEntries)
	{
		bOnEntries = false; // B-635: Back steps out of the entries before it leaves the screen
		RefreshSelection();
		return true;
	}
	return Super::NativeOnHandleBackAction();
}

void UCharacterSheetWidget::PressOpenLoadout()
{
	const FPartyMember *Member = WoRCharacterSheet::FindPartyMember(GetWorld(), MemberIndex);
	if (!Member)
	{
		UE_LOG(LogWoRUI, Log, TEXT("[CharacterSheet] Loadout: no member %d"), MemberIndex);
		return;
	}
	ULoadoutV2ScreenWidget::OpenWithLoadoutOn(GetOwningPlayer(), UPoolSubsystem::GetCharacterKey(*Member)); // over this screen: Back returns here
}

FReply UCharacterSheetWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveInColumn(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	const bool bAction = IsSubTabPrevKey(Key) || IsSubTabNextKey(Key) || IsConfirmKey(Key) || IsSecondActionKey(Key) || IsBackKey(Key);
	if (!bAction)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	if (!InKeyEvent.IsRepeat())
	{
		if (IsSubTabPrevKey(Key) || IsSubTabNextKey(Key))
		{
			MoveMember(IsSubTabNextKey(Key) ? 1 : -1);
		}
		else if (IsConfirmKey(Key))
		{
			PressSelect();
		}
		else if (IsSecondActionKey(Key))
		{
			PressOpenLoadout();
		}
		else
		{
			// Reached only when CommonUI's own Back action didn't take the key (BP_CommonInputData unset, B-103).
			HandleBackAction();
		}
	}
	return FReply::Handled();
}

int32 UCharacterSheetWidget::FindUnder(const FPointerEvent &MouseEvent, EHoverKind Kind) const
{
	const FVector2D Where = MouseEvent.GetScreenSpacePosition();
	const auto IsUnder = [&Where](const UWidget *Widget)
	{
		return Widget && Widget->IsVisible() && Widget->GetCachedGeometry().IsUnderLocation(Where);
	};
	switch (Kind)
	{
	case EHoverKind::Tab:
		for (int32 Index = 0; Index < MemberTabs.Num() && Index < Members.Num(); ++Index)
		{
			if (IsUnder(MemberTabs[Index]))
			{
				return Index;
			}
		}
		break;
	case EHoverKind::Group:
		for (int32 Index = 0; Index < GroupWidgets.Num() && Index < Groups.Num(); ++Index)
		{
			if (IsUnder(GroupWidgets[Index].Frame.Get()))
			{
				return Index;
			}
		}
		break;
	case EHoverKind::Entry:
		if (IsUnder(EntryScroll)) // a plate scrolled out of view keeps its last drawn place
		{
			for (int32 Index = 0; Index < EntryWidgets.Num(); ++Index)
			{
				if (IsUnder(EntryWidgets[Index].Frame.Get()))
				{
					return Index;
				}
			}
		}
		break;
	default:
		break;
	}
	return INDEX_NONE;
}

FReply UCharacterSheetWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (const int32 Tab = FindUnder(InMouseEvent, EHoverKind::Tab); Tab != INDEX_NONE)
	{
		MoveMember(Tab - MemberIndex);
		return FReply::Handled();
	}
	if (const int32 Group = FindUnder(InMouseEvent, EHoverKind::Group); Group != INDEX_NONE)
	{
		SelectGroup(Group);
		return FReply::Handled();
	}
	if (const int32 Entry = FindUnder(InMouseEvent, EHoverKind::Entry); Entry != INDEX_NONE)
	{
		SelectEntry(Entry);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UCharacterSheetWidget::PaintMemberTabs()
{
	using namespace WoRMenuStyle;
	for (int32 Index = 0; Index < MemberTabs.Num() && Index < Members.Num(); ++Index)
	{
		if (MemberTabs[Index])
		{
			MemberTabs[Index]->SetBrushColor(Index == MemberIndex ? FocusColor : HoverKind == EHoverKind::Tab && Index == HoverIndex ? HoverColor : PlateColor);
		}
	}
}

void UCharacterSheetWidget::SetHover(EHoverKind Kind, int32 Index)
{
	if (Index == INDEX_NONE)
	{
		Kind = EHoverKind::None;
	}
	if (Kind == HoverKind && Index == HoverIndex)
	{
		return;
	}
	HoverKind = Kind;
	HoverIndex = Index;
	PaintMemberTabs();
	RefreshSelection();
}

FReply UCharacterSheetWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (!InMouseEvent.GetCursorDelta().IsZero()) // a resting cursor doesn't light anything
	{
		EHoverKind Kind = EHoverKind::None;
		int32 Index = INDEX_NONE;
		for (const EHoverKind Candidate : {EHoverKind::Tab, EHoverKind::Group, EHoverKind::Entry})
		{
			Index = FindUnder(InMouseEvent, Candidate);
			if (Index != INDEX_NONE)
			{
				Kind = Candidate;
				break;
			}
		}
		SetHover(Kind, Index);
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UCharacterSheetWidget::NativeOnMouseLeave(const FPointerEvent &InMouseEvent)
{
	SetHover(EHoverKind::None, INDEX_NONE);
	Super::NativeOnMouseLeave(InMouseEvent);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GCharacterSheetOpenCommand(
	TEXT("wor.UI.Sheet"),
	TEXT("B-442 / B-635: open the Profile (the character sheet), as the Party screen's A does (on the first member)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UCharacterSheetWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[CharacterSheet] wor.UI.Sheet: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
