// LoadoutScreenWidget.cpp

#include "UI/Menus/LoadoutScreenWidget.h"

#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Loadout/LoadoutConstants.h"
#include "Party/Party.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutScreenWidget"

namespace LoadoutScreenStyle
{
	// This screen's own layout (screen_mockups/menus/loadout, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float ListWidth = 978.0f;
	constexpr float RowHeight = 126.0f;
	constexpr float RowGap = 12.0f;
	constexpr float RowPadding = 21.0f;
	constexpr float NameColumnWidth = 480.0f;
	constexpr float SocketWidth = 190.0f;
	constexpr float PanelGap = 45.0f;
	constexpr float PanelWidth = 753.0f;
	constexpr float PanelPadding = 30.0f;
	constexpr float DetailLabelWidth = 225.0f;
	constexpr float PipWidth = 51.0f;
	constexpr float PipHeight = 36.0f;
	constexpr float LoadoutBoxWidth = 708.0f;
	constexpr float HeaderGap = 27.0f;
	constexpr int32 SocketsPerRow = 2;
	constexpr int32 MaxDetailLines = 6;
	constexpr int32 MaxBudgetPips = LoadoutConstants::LOADOUT_TOTAL_BUDGET;
}

namespace LoadoutScreenSaved
{
	/** B-440e: a saved loadout as a list row (the active one carries the ACTIVE chip; no cost, no sockets). */
	FLoadoutScreenRow ToScreenRow(const FLoadoutSavedRow &Saved)
	{
		FLoadoutScreenRow Row;
		Row.Kind = Saved.IsNewLine() ? ELoadoutScreenRowKind::EmptyGear : ELoadoutScreenRowKind::Weapon;
		Row.Name = Saved.Name;
		Row.KindLine = Saved.Note;
		Row.bActive = Saved.bActive;
		Row.bCountsToBudget = false;
		Row.Note = Saved.IsNewLine() ? LOCTEXT("SavedNewNote", "A makes an empty loadout; Y then names it.")
				   : Saved.bLore	 ? LOCTEXT("SavedLoreNote", "A uses it. X makes a copy of your own, which you can change and rename.")
									 : LOCTEXT("SavedNote", "A uses it, X duplicates it, Y renames it.");
		return Row;
	}
}

ULoadoutScreenWidget::ULoadoutScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself; its rows are plain borders
}

ULoadoutScreenWidget *ULoadoutScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[LoadoutScreen] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<ULoadoutScreenWidget>(Root->PushToMenu(ULoadoutScreenWidget::StaticClass()));
}

TSharedRef<SWidget> ULoadoutScreenWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void ULoadoutScreenWidget::BuildTree()
{
	using namespace LoadoutScreenStyle;

	UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
	Screen->SetBrushColor(ScreenColor);
	Screen->SetPadding(FMargin(ScreenPadding));
	WidgetTree->RootWidget = Screen;
	UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Screen->SetContent(Main);

	// Top: tabs and budget on the left, the saved loadout on the right.
	UHorizontalBox *Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Header);
	UVerticalBox *TabsColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddToRow(Header, TabsColumn, FMargin(0.0f), VAlign_Top)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UHorizontalBox *MemberRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(MemberRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabPrev", "LT")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (int32 Index = 0; Index < PartyConstants::MAX_PARTY_MEMBERS; ++Index)
	{
		UTextBlock *Label = nullptr;
		MemberTabs.Add(MakeTab(WidgetTree, Label));
		MemberTabLabels.Add(Label);
		AddToRow(MemberRow, MemberTabs.Last(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	AddToRow(MemberRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabNext", "RT")), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(TabsColumn, MemberRow);

	UHorizontalBox *PageRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToRow(PageRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabPrev", "LB")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (const FText &PageName : {LOCTEXT("PageGear", "Gear"), LOCTEXT("PageSkills", "Skills")})
	{
		UTextBlock *Label = nullptr;
		PageTabs.Add(MakeTab(WidgetTree, Label));
		PageTabLabels.Add(Label);
		Label->SetText(PageName);
		AddToRow(PageRow, PageTabs.Last(), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	AddToRow(PageRow, MakeKeyLabel(WidgetTree, LOCTEXT("KeyTabNext", "RB")), FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(TabsColumn, PageRow, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	UHorizontalBox *BudgetRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BudgetLabel = MakeText(WidgetTree, LineFontSize, DimColor, true);
	AddToRow(BudgetRow, BudgetLabel, FMargin(0.0f, 0.0f, SectionGap, 0.0f));
	for (int32 Index = 0; Index < MaxBudgetPips; ++Index)
	{
		UBorder *Pip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		BudgetPips.Add(Pip);
		AddToRow(BudgetRow, MakeSized(WidgetTree, PipWidth, PipHeight, Pip), FMargin(0.0f, 0.0f, TabGap, 0.0f));
	}
	BudgetText = MakeText(WidgetTree, LineFontSize, TextColor, true);
	AddToRow(BudgetRow, BudgetText, FMargin(TabGap, 0.0f, 0.0f, 0.0f));
	AddToColumn(TabsColumn, BudgetRow, FMargin(0.0f, SectionGap, 0.0f, 0.0f));

	UBorder *LoadoutBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	LoadoutBox->SetBrushColor(PanelColor);
	LoadoutBox->SetPadding(FMargin(PanelPadding, SectionGap));
	UVerticalBox *LoadoutColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	LoadoutBox->SetContent(LoadoutColumn);
	UTextBlock *LoadoutHeading = MakeText(WidgetTree, SmallFontSize, DimColor, true);
	LoadoutHeading->SetText(LOCTEXT("LoadoutHeading", "Loadout"));
	LoadoutTitleText = MakeText(WidgetTree, RowFontSize, TextColor, true);
	AddToColumn(LoadoutColumn, LoadoutHeading);
	AddToColumn(LoadoutColumn, LoadoutTitleText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToRow(Header, MakeSized(WidgetTree, LoadoutBoxWidth, 0.0f, LoadoutBox), FMargin(0.0f), VAlign_Top);

	// The rows, and the focused row's details.
	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body, FMargin(0.0f, HeaderGap, 0.0f, SectionGap))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBox *ListColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	// TODO: B-440d, the lists as tab plates (mockup 3); a line of text for now.
	SkillTabsText = MakeWrappedText(WidgetTree, LineFontSize, TextColor, true);
	AddToColumn(ListColumn, SkillTabsText, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	RefusalText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	AddToColumn(ListColumn, RefusalText, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	PickerTitle = MakeText(WidgetTree, LineFontSize, DimColor, true);
	AddToColumn(ListColumn, PickerTitle, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	// B-440e: the saved loadout's new name; shown only while renaming.
	NameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("LoadoutNameBox"));
	NameBox->SetHintText(LOCTEXT("NameHint", "Loadout name"));
	NameBox->OnTextCommitted.AddDynamic(this, &ULoadoutScreenWidget::HandleNameCommitted);
	AddToColumn(ListColumn, NameBox, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	SetShown(NameBox, false);
	RowList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddToColumn(ListColumn, RowList)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Body, MakeSized(WidgetTree, ListWidth, 0.0f, ListColumn), FMargin(0.0f), VAlign_Fill);

	UBorder *Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(PanelPadding));
	UVerticalBox *PanelColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(PanelColumn);
	DetailName = MakeText(WidgetTree, NameFontSize, TextColor, true);
	DetailKindLine = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, DetailName);
	AddToColumn(PanelColumn, DetailKindLine, FMargin(0.0f, 0.0f, 0.0f, SectionGap));
	for (int32 Index = 0; Index < MaxDetailLines; ++Index)
	{
		UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		DetailLabels.Add(MakeText(WidgetTree, LineFontSize, DimColor));
		DetailValues.Add(MakeWrappedText(WidgetTree, LineFontSize, TextColor, true));
		AddToRow(Line, MakeSized(WidgetTree, DetailLabelWidth, 0.0f, DetailLabels.Last()), FMargin(0.0f), VAlign_Top);
		AddToRow(Line, DetailValues.Last(), FMargin(0.0f), VAlign_Top)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AddToColumn(PanelColumn, Line, FMargin(0.0f, 0.0f, 0.0f, LineGap));
	}
	UTextBlock *CostHeading = MakeText(WidgetTree, LineFontSize, DimColor, true);
	CostHeading->SetText(LOCTEXT("CostHeading", "Cost"));
	DetailCostLine = MakeText(WidgetTree, RowFontSize, TextColor, true);
	DetailNote = MakeWrappedText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(PanelColumn, CostHeading, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToColumn(PanelColumn, DetailCostLine, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	AddToColumn(PanelColumn, DetailNote, FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	AddToRow(Body, MakeSized(WidgetTree, PanelWidth, 0.0f, Panel), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	// Bottom right: the hints.
	HintsText = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(Main, HintsText)->SetHorizontalAlignment(HAlign_Right);
}

ULoadoutScreenWidget::FRowWidgets ULoadoutScreenWidget::BuildRow()
{
	using namespace LoadoutScreenStyle;

	FRowWidgets Widgets;
	UVerticalBox *Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock *Heading = MakeText(WidgetTree, LineFontSize, DimColor, true);
	Heading->SetText(LOCTEXT("ApparelHeading", "Apparel"));
	AddToColumn(Root, Heading, FMargin(0.0f, LineGap, 0.0f, LineGap));

	UBorder *Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetPadding(FMargin(RowPadding));
	Frame->SetVerticalAlignment(VAlign_Center);
	UHorizontalBox *Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Columns);

	UVerticalBox *NameColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox *NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
	UBorder *ActiveChip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ActiveChip->SetBrushColor(ScreenColor);
	ActiveChip->SetPadding(FMargin(WordGap, 0.0f));
	UTextBlock *ActiveLabel = MakeText(WidgetTree, SmallFontSize, CyanColor, true);
	ActiveLabel->SetText(LOCTEXT("Active", "ACTIVE"));
	ActiveChip->SetContent(ActiveLabel);
	AddToRow(NameRow, Name);
	AddToRow(NameRow, ActiveChip, FMargin(SectionGap, 0.0f, 0.0f, 0.0f));
	UTextBlock *KindLine = MakeText(WidgetTree, SmallFontSize, DimColor);
	AddToColumn(NameColumn, NameRow);
	AddToColumn(NameColumn, KindLine);
	AddToRow(Columns, MakeSized(WidgetTree, NameColumnWidth, 0.0f, NameColumn));

	for (int32 Index = 0; Index < SocketsPerRow; ++Index)
	{
		UVerticalBox *Socket = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Widgets.SocketNames.Add(MakeText(WidgetTree, LineFontSize, TextColor, true));
		Widgets.SocketLabels.Add(MakeText(WidgetTree, SmallFontSize, DimColor));
		AddToColumn(Socket, Widgets.SocketNames.Last().Get());
		AddToColumn(Socket, Widgets.SocketLabels.Last().Get());
		UWidget *SocketBox = MakeSized(WidgetTree, SocketWidth, 0.0f, Socket);
		Widgets.SocketBoxes.Add(SocketBox);
		AddToRow(Columns, SocketBox);
	}

	UTextBlock *Cost = MakeText(WidgetTree, RowFontSize, TextColor, true);
	UHorizontalBoxSlot *CostSlot = AddToRow(Columns, Cost);
	CostSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CostSlot->SetHorizontalAlignment(HAlign_Right);

	AddToColumn(Root, MakeSized(WidgetTree, ListWidth, RowHeight, Frame), FMargin(0.0f, 0.0f, 0.0f, RowGap));
	RowList->AddChild(Root);

	Widgets.Root = Root;
	Widgets.Heading = Heading;
	Widgets.Frame = Frame;
	Widgets.Name = Name;
	Widgets.ActiveChip = ActiveChip;
	Widgets.KindLine = KindLine;
	Widgets.Cost = Cost;
	return Widgets;
}

void ULoadoutScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshData();
	RefreshSelection();
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutScreen] Opened (%d member(s))"), Members.Num());
}

UWidget *ULoadoutScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<ULoadoutScreenWidget *>(this);
}

void ULoadoutScreenWidget::RefreshData()
{
	using namespace LoadoutScreenStyle;

	UWorld *World = GetWorld();
	Members = WoRLoadoutScreen::BuildMembers(World);
	MemberIndex = Members.Num() > 0 ? FMath::Clamp(MemberIndex, 0, Members.Num() - 1) : 0;
	Page = Members.IsValidIndex(MemberIndex) ? WoRLoadoutScreen::BuildGearPage(World, Members[MemberIndex]) : FLoadoutScreenPage();
	if (!Members.IsValidIndex(MemberIndex))
	{
		Page.Refusal = LOCTEXT("NoParty", "There is no party to show.");
	}
	if (bSkillsPage)
	{
		// B-440d: the list shows the lit skill list's slots; the points (not the pips) show in the budget line.
		Skills = Members.IsValidIndex(MemberIndex) ? WoRLoadoutSkills::BuildSkillsPage(World, Members[MemberIndex]) : FLoadoutSkillsPage();
		SkillTab = Skills.Tabs.Num() > 0 ? FMath::Clamp(SkillTab, 0, Skills.Tabs.Num() - 1) : 0;
		Page.Rows = Skills.Tabs.IsValidIndex(SkillTab) ? Skills.Tabs[SkillTab].Rows : TArray<FLoadoutScreenRow>();
		Page.FirstApparelRow = INDEX_NONE;
		Page.Budget = 0;
		if (Page.Refusal.IsEmpty())
		{
			Page.Refusal = !Skills.Refusal.IsEmpty() ? Skills.Refusal
						   : Skills.Tabs.Num() == 0 ? LOCTEXT("NoSkillLists", "No skill lists: a weapon or an adapter brings one.")
													: FText::GetEmpty();
		}
	}
	SelectedRow = Page.Rows.Num() > 0 ? FMath::Clamp(SelectedRow, 0, Page.Rows.Num() - 1) : 0;

	if (LoadoutTitleText)
	{
		LoadoutTitleText->SetText(Page.LoadoutTitle);
	}
	for (int32 Index = 0; Index < MemberTabs.Num(); ++Index)
	{
		const bool bShown = Members.IsValidIndex(Index);
		SetShown(MemberTabs[Index], bShown);
		if (bShown)
		{
			MemberTabLabels[Index]->SetText(Members[Index].Name);
			MemberTabs[Index]->SetBrushColor(Index == MemberIndex ? FocusColor : PlateColor);
			MemberTabLabels[Index]->SetColorAndOpacity(FSlateColor(Index == MemberIndex ? TextColor : DimColor));
		}
	}
	for (int32 Index = 0; Index < PageTabs.Num(); ++Index)
	{
		const bool bLit = Index == (bSkillsPage ? 1 : 0);
		PageTabs[Index]->SetBrushColor(bLit ? FocusColor : PlateColor);
		PageTabLabels[Index]->SetColorAndOpacity(FSlateColor(bLit ? TextColor : DimColor));
	}
	if (SkillTabsText)
	{
		TArray<FText> Lists;
		for (int32 Index = 0; Index < Skills.Tabs.Num(); ++Index)
		{
			const FText List = FText::Format(LOCTEXT("SkillTab", "{0} {1}"), Skills.Tabs[Index].Title, Skills.Tabs[Index].Filled);
			Lists.Add(Index == SkillTab ? FText::Format(LOCTEXT("SkillTabLit", "[ {0} ]"), List) : List);
		}
		SkillTabsText->SetText(FText::Join(LOCTEXT("SkillTabGap", "    "), Lists));
		SetShown(SkillTabsText, bSkillsPage && Lists.Num() > 0);
	}
	RefreshList();
}

void ULoadoutScreenWidget::RefreshList()
{
	using namespace LoadoutScreenStyle;

	if (HintsText)
	{
		HintsText->SetText(bRenaming	 ? LOCTEXT("HintsRenaming", "Enter  Keep the name        Esc  Cancel")
						   : bSavedList	 ? LOCTEXT("HintsSaved", "A  Use        X  Duplicate        Y / R  Rename        B / Esc  Back")
						   : bPicking	 ? LOCTEXT("HintsPicking", "A  Put in        B / Esc  Cancel")
						   : bSkillsPage ? LOCTEXT("HintsSkills", "A  Change        X  Remove        Left / Right  Lists        LB / RB  Page        LT / RT  Character        Y / R  Loadouts        B / Esc  Back")
										 : LOCTEXT("Hints", "A  Change        X  Unequip        Left / Right  Sockets        LB / RB  Page        LT / RT  Character        Y / R  Loadouts        B / Esc  Back"));
	}
	if (RefusalText && PickerTitle)
	{
		const FText Shown = !Message.IsEmpty() ? Message : bSavedList ? Saved.Refusal : Page.Refusal;
		RefusalText->SetText(Shown);
		SetShown(RefusalText, !Shown.IsEmpty());
		const bool bHasRow = Page.Rows.IsValidIndex(SelectedRow);
		const FText RowName = bHasRow ? Page.Rows[SelectedRow].Name : FText::GetEmpty();
		const FText SocketName = bHasRow && Page.Rows[SelectedRow].Sockets.IsValidIndex(SelectedSocket) ? Page.Rows[SelectedRow].Sockets[SelectedSocket].Label : FText::GetEmpty();
		const FText ListName = Skills.Tabs.IsValidIndex(SkillTab) ? Skills.Tabs[SkillTab].Title : FText::GetEmpty();
		PickerTitle->SetText(Choices.Num() == 0 ? (bSkillsPage ? LOCTEXT("PickerNoSkill", "You know nothing that fits here.") : LOCTEXT("PickerNothing", "Nothing you own fits here."))
							 : bPickingSocket   ? FText::Format(LOCTEXT("PickerSocketTitle", "{0} · {1} socket:"), RowName, SocketName)
							 : bSkillsPage		? FText::Format(LOCTEXT("PickerSkillTitle", "{0} · in place of {1}:"), ListName, RowName)
												: FText::Format(LOCTEXT("PickerTitle", "In place of {0}:"), RowName));
		if (bSavedList)
		{
			PickerTitle->SetText(FText::Format(LOCTEXT("SavedTitle", "Saved loadouts ({0} of {1} of your own):"), FText::AsNumber(Saved.CustomCount),
											   FText::AsNumber(Saved.MaxCustom)));
		}
		SetShown(PickerTitle, bPicking || bSavedList);
	}
	RefreshBudget();

	if (!RowList)
	{
		return; // headless: no tree
	}
	while (RowWidgets.Num() < GetListCount())
	{
		RowWidgets.Add(BuildRow());
	}
	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		const bool bShown = Index < GetListCount();
		SetShown(RowWidgets[Index].Root.Get(), bShown);
		if (!bShown)
		{
			continue;
		}
		if (bSavedList)
		{
			FillRow(RowWidgets[Index], LoadoutScreenSaved::ToScreenRow(Saved.Rows[Index]), false);
		}
		else if (bPicking)
		{
			FillRow(RowWidgets[Index], Choices[Index].Row, false);
			if (!Choices[Index].Refusal.IsEmpty() && RowWidgets[Index].Name.IsValid())
			{
				RowWidgets[Index].Name->SetColorAndOpacity(FSlateColor(OffColor)); // refused: stays, greyed (UI_Theme §5)
			}
		}
		else
		{
			FillRow(RowWidgets[Index], Page.Rows[Index], Index == Page.FirstApparelRow);
		}
	}
}

void ULoadoutScreenWidget::RefreshBudget()
{
	using namespace LoadoutScreenStyle;

	// In the picker the lit line's budget shows in amber before it is confirmed (UIScreens #2).
	const bool bPreview = bPicking && Choices.IsValidIndex(SelectedChoice);
	if (bSkillsPage)
	{
		// B-440d: a Refractor's innate points (mockup 3); the other classes' skills cost nothing.
		const bool bPoints = Skills.MaxPoints > 0;
		const bool bPointsPreview = bPreview && bPoints && Skills.Tabs.IsValidIndex(SkillTab) && Skills.Tabs[SkillTab].List == ELoadoutSkillList::Innate;
		const int32 Points = bPointsPreview ? Choices[SelectedChoice].NewSpent : Skills.Points;
		for (UBorder *Pip : BudgetPips)
		{
			SetShown(Pip, false);
		}
		SetShown(BudgetLabel, bPoints);
		SetShown(BudgetText, bPoints);
		if (BudgetLabel && BudgetText)
		{
			BudgetLabel->SetText(LOCTEXT("InnatePoints", "Innate spells"));
			BudgetText->SetText(bPointsPreview ? FText::Format(LOCTEXT("PointsPreview", "{0} → {1} of {2} points"), Skills.Points, Points, Skills.MaxPoints)
											   : FText::Format(LOCTEXT("PointsValue", "{0} of {1} points"), Points, Skills.MaxPoints));
			BudgetText->SetColorAndOpacity(FSlateColor(bPointsPreview || Points > Skills.MaxPoints ? WarnColor : TextColor));
		}
		return;
	}
	SetShown(BudgetLabel, true);
	SetShown(BudgetText, true);
	if (BudgetLabel)
	{
		BudgetLabel->SetText(LOCTEXT("Budget", "Budget"));
	}
	const int32 Shown = bPreview ? Choices[SelectedChoice].NewSpent : Page.Spent;
	for (int32 Index = 0; Index < BudgetPips.Num(); ++Index)
	{
		SetShown(BudgetPips[Index], Index < Page.Budget);
		BudgetPips[Index]->SetBrushColor(Index < Shown ? (bPreview ? WarnColor : CyanColor) : TrackColor);
	}
	if (BudgetText)
	{
		BudgetText->SetText(bPreview ? FText::Format(LOCTEXT("BudgetPreview", "{0} → {1} of {2}"), FText::AsNumber(Page.Spent), FText::AsNumber(Shown),
													 FText::AsNumber(Page.Budget))
									 : FText::Format(LOCTEXT("BudgetValue", "{0} of {1}"), FText::AsNumber(Page.Spent), FText::AsNumber(Page.Budget)));
		BudgetText->SetColorAndOpacity(FSlateColor(bPreview || Shown > Page.Budget ? WarnColor : TextColor));
	}
}

void ULoadoutScreenWidget::FillRow(const FRowWidgets &Widgets, const FLoadoutScreenRow &Row, bool bHeading) const
{
	using namespace LoadoutScreenStyle;

	if (!Widgets.Name.IsValid() || !Widgets.KindLine.IsValid() || !Widgets.Cost.IsValid())
	{
		return;
	}
	const bool bEmptySlot = Row.Kind == ELoadoutScreenRowKind::EmptyApparel || Row.Kind == ELoadoutScreenRowKind::EmptyGear || Row.Kind == ELoadoutScreenRowKind::EmptySkill;
	SetShown(Widgets.Heading.Get(), bHeading);
	Widgets.Name->SetText(Row.Name);
	Widgets.Name->SetColorAndOpacity(FSlateColor(bEmptySlot ? OffColor : TextColor));
	SetShown(Widgets.ActiveChip.Get(), Row.bActive);
	Widgets.KindLine->SetText(Row.KindLine);
	for (int32 Index = 0; Index < Widgets.SocketBoxes.Num(); ++Index)
	{
		const bool bShown = Row.Sockets.IsValidIndex(Index);
		SetShown(Widgets.SocketBoxes[Index].Get(), bShown);
		if (bShown && Widgets.SocketNames[Index].IsValid() && Widgets.SocketLabels[Index].IsValid())
		{
			const FLoadoutScreenSocket &Socket = Row.Sockets[Index];
			Widgets.SocketNames[Index]->SetText(Socket.bEmpty ? LOCTEXT("SocketEmpty", "empty") : Socket.Name);
			Widgets.SocketNames[Index]->SetColorAndOpacity(FSlateColor(Socket.bEmpty ? OffColor : TextColor));
			Widgets.SocketLabels[Index]->SetText(Socket.Label);
		}
	}
	Widgets.Cost->SetText(Row.bCountsToBudget ? FText::AsNumber(Row.Cost) : Row.CostLine);
	Widgets.Cost->SetColorAndOpacity(FSlateColor(Row.bCountsToBudget ? TextColor : DimColor));
}

void ULoadoutScreenWidget::RefreshSelection(bool bScrollIntoView)
{
	using namespace LoadoutScreenStyle;

	const int32 Lit = GetListSelection();
	for (int32 Index = 0; Index < RowWidgets.Num(); ++Index)
	{
		if (UBorder *Frame = RowWidgets[Index].Frame.Get())
		{
			Frame->SetBrushColor(Index == Lit ? FocusColor : PlateColor);
		}
		for (int32 Socket = 0; Socket < RowWidgets[Index].SocketLabels.Num(); ++Socket)
		{
			if (UTextBlock *Label = RowWidgets[Index].SocketLabels[Socket].Get())
			{
				const bool bLitSocket = !bPicking && Index == Lit && Socket == SelectedSocket;
				Label->SetColorAndOpacity(FSlateColor(bLitSocket ? CyanColor : DimColor)); // B-440c: the lit socket
			}
		}
	}
	RefreshBudget();
	if (!DetailName)
	{
		return; // headless: no tree
	}
	const FLoadoutScreenChoice *Choice = !bSavedList && bPicking && Choices.IsValidIndex(SelectedChoice) ? &Choices[SelectedChoice] : nullptr;
	const bool bSavedRow = bSavedList && Saved.Rows.IsValidIndex(SelectedSaved);
	const bool bHasRow = Choice || bSavedRow || (!bSavedList && !bPicking && Page.Rows.IsValidIndex(SelectedRow));
	const FLoadoutScreenRow Row = Choice	  ? Choice->Row
								  : bSavedRow ? LoadoutScreenSaved::ToScreenRow(Saved.Rows[SelectedSaved])
								  : bHasRow	  ? Page.Rows[SelectedRow]
											  : FLoadoutScreenRow();
	DetailName->SetText(bHasRow && !Row.Tier.IsEmpty() ? FText::Format(LOCTEXT("NameAndTier", "{0} ({1})"), Row.Name, Row.Tier) : Row.Name);
	DetailKindLine->SetText(Row.KindLine);
	for (int32 Index = 0; Index < DetailLabels.Num(); ++Index)
	{
		const bool bShown = Row.Details.IsValidIndex(Index);
		SetShown(DetailLabels[Index], bShown);
		SetShown(DetailValues[Index], bShown);
		if (bShown)
		{
			DetailLabels[Index]->SetText(Row.Details[Index].Label);
			DetailValues[Index]->SetText(Row.Details[Index].Value);
		}
	}
	DetailCostLine->SetText(Row.CostLine);
	const bool bSocketLit = !bPicking && Row.Sockets.IsValidIndex(SelectedSocket);
	const FText SocketNote = bSocketLit ? FText::Format(LOCTEXT("SocketNote", "{0} socket: {1}."), Row.Sockets[SelectedSocket].Label,
														Row.Sockets[SelectedSocket].bEmpty ? LOCTEXT("SocketNoteEmpty", "empty, A fills it")
																						   : Row.Sockets[SelectedSocket].Name)
										: FText::GetEmpty();
	const bool bInnateList = Skills.Tabs.IsValidIndex(SkillTab) && Skills.Tabs[SkillTab].List == ELoadoutSkillList::Innate;
	const bool bSkillNote = bSkillsPage && Choice && Choice->Refusal.IsEmpty() && !bInnateList; // a weapon's or adapter's skill costs nothing
	DetailNote->SetText(bSocketLit ? SocketNote
				: !Choice || bSkillNote ? Row.Note
				: !Choice->Refusal.IsEmpty() ? Choice->Refusal
				: bSkillsPage ? FText::Format(LOCTEXT("PointsAfter", "Points after: {0} of {1}."), Choice->NewSpent, Skills.MaxPoints)
											  : FText::Format(LOCTEXT("BudgetAfter", "Budget after: {0} of {1}."), FText::AsNumber(Choice->NewSpent),
															  FText::AsNumber(Page.Budget)));
	DetailNote->SetColorAndOpacity(FSlateColor(Choice && !bSkillNote ? WarnColor : DimColor));
	if (bScrollIntoView && RowList && RowWidgets.IsValidIndex(Lit))
	{
		if (UWidget *RowRoot = RowWidgets[Lit].Root.Get())
		{
			RowList->ScrollWidgetIntoView(RowRoot, /*AnimateScroll=*/false);
		}
	}
}

void ULoadoutScreenWidget::MoveSelection(int32 Delta)
{
	const int32 Count = GetListCount();
	if (Count > 0)
	{
		SelectRow(((GetListSelection() + Delta) % Count + Count) % Count);
	}
}

void ULoadoutScreenWidget::SelectRow(int32 Index, bool bScrollIntoView)
{
	if (Index < 0 || Index >= GetListCount() || Index == GetListSelection())
	{
		return;
	}
	(bSavedList ? SelectedSaved : bPicking ? SelectedChoice : SelectedRow) = Index;
	if (!bPicking && !bSavedList)
	{
		SelectedSocket = INDEX_NONE; // a new row is lit as a whole
	}
	bTakeOffArmed = false;
	if (!Message.IsEmpty())
	{
		Message = FText::GetEmpty(); // a refusal belongs to the line it was about
		RefreshList();
	}
	RefreshSelection(bScrollIntoView);
}

void ULoadoutScreenWidget::MoveMember(int32 Delta)
{
	if (Members.Num() < 2 || bSavedList)
	{
		return;
	}
	MemberIndex = ((MemberIndex + Delta) % Members.Num() + Members.Num()) % Members.Num();
	SelectedRow = 0;
	SelectedSocket = INDEX_NONE;
	SkillTab = 0;
	bPicking = false;
	bPickingSocket = false;
	bTakeOffArmed = false;
	Message = FText::GetEmpty();
	RefreshData();
	RefreshSelection();
}

void ULoadoutScreenWidget::PressChange()
{
	if (bSavedList)
	{
		PressUseSaved(); // B-440e
		return;
	}
	if (!Members.IsValidIndex(MemberIndex) || !Page.Rows.IsValidIndex(SelectedRow))
	{
		return;
	}
	const FLoadoutScreenRow &Row = Page.Rows[SelectedRow];
	if (bSkillsPage)
	{
		// B-440d: the lit slot takes a learned skill.
		if (!bPicking)
		{
			Choices = WoRLoadoutSkills::BuildSkillChoices(GetWorld(), Members[MemberIndex], Skills, SkillTab, Row);
			SelectedChoice = FMath::Max(0, Choices.IndexOfByPredicate([](const FLoadoutScreenChoice &Choice) { return Choice.Refusal.IsEmpty(); }));
			bPicking = true;
			Message = FText::GetEmpty();
			RefreshList();
			RefreshSelection();
			return;
		}
		if (!Choices.IsValidIndex(SelectedChoice) || !Skills.Tabs.IsValidIndex(SkillTab))
		{
			return;
		}
		Message = !Choices[SelectedChoice].Refusal.IsEmpty()
					  ? Choices[SelectedChoice].Refusal
					  : WoRLoadoutSkills::ApplySkill(GetWorld(), Members[MemberIndex], Skills.Tabs[SkillTab], Row, Choices[SelectedChoice].Row.Skill);
		if (Message.IsEmpty())
		{
			ClosePicker();
			return;
		}
		RefreshList();
		RefreshSelection();
		return;
	}
	if (!bPicking && Row.Sockets.IsValidIndex(SelectedSocket))
	{
		// B-440c: a filled socket is emptied first (X), since taking a gem off destroys it (Q-440c).
		Message = Row.Sockets[SelectedSocket].bEmpty ? FText::GetEmpty() : LOCTEXT("SocketTakeOffFirst", "Take what's in this socket off first (X).");
		if (Message.IsEmpty())
		{
			Choices = WoRLoadoutScreen::BuildSocketChoices(GetWorld(), Members[MemberIndex], Row, SelectedSocket);
			const int32 FirstAllowed = Choices.IndexOfByPredicate([](const FLoadoutScreenChoice &Choice) { return Choice.Refusal.IsEmpty(); });
			SelectedChoice = FMath::Max(0, FirstAllowed);
			bPicking = true;
			bPickingSocket = true;
		}
		RefreshList();
		RefreshSelection();
		return;
	}
	if (!bPicking)
	{
		if (Row.Place == ELoadoutScreenPlace::None)
		{
			Message = LOCTEXT("ApparelNotYet", "Apparel can't be changed here yet."); // TODO: B-440f, the apparel picker
			RefreshList();
			return;
		}
		Choices = WoRLoadoutScreen::BuildGearChoices(GetWorld(), Members[MemberIndex], Row);
		const int32 FirstAllowed = Choices.IndexOfByPredicate([](const FLoadoutScreenChoice &Choice) { return Choice.Refusal.IsEmpty(); });
		SelectedChoice = FMath::Max(0, FirstAllowed);
		bPicking = true;
		Message = FText::GetEmpty();
		RefreshList();
		RefreshSelection();
		return;
	}
	if (!Choices.IsValidIndex(SelectedChoice))
	{
		return;
	}
	Message = bPickingSocket ? WoRLoadoutScreen::ApplySocketChoice(GetWorld(), Members[MemberIndex], Row, SelectedSocket, Choices[SelectedChoice])
							 : WoRLoadoutScreen::ApplyGearChoice(GetWorld(), Members[MemberIndex], Row, Choices[SelectedChoice]);
	if (Message.IsEmpty())
	{
		UE_LOG(LogWoRUI, Log, TEXT("[LoadoutScreen] %s: %s put in place of %s"), *Members[MemberIndex].Name.ToString(),
			   *Choices[SelectedChoice].Row.Name.ToString(), *Row.Name.ToString());
		ClosePicker();
		return;
	}
	RefreshList();
	RefreshSelection();
}

void ULoadoutScreenWidget::PressUnequip()
{
	if (bSavedList)
	{
		PressDuplicateSaved(); // B-440e
		return;
	}
	if (bPicking || !Members.IsValidIndex(MemberIndex) || !Page.Rows.IsValidIndex(SelectedRow))
	{
		return;
	}
	if (bSkillsPage)
	{
		// B-440d: nothing is lost (an innate spell stays learned; a replaced built-in comes back), so one press.
		Message = Skills.Tabs.IsValidIndex(SkillTab) ? WoRLoadoutSkills::ApplySkill(GetWorld(), Members[MemberIndex], Skills.Tabs[SkillTab], Page.Rows[SelectedRow], nullptr)
													 : FText::GetEmpty();
		RefreshData();
		RefreshSelection();
		return;
	}
	if (Page.Rows[SelectedRow].Sockets.IsValidIndex(SelectedSocket))
	{
		if (!bTakeOffArmed)
		{
			// Q-440c: the first X says what comes off; the second does it.
			FText Refusal;
			const FText Warning = WoRLoadoutScreen::GetTakeOffSocketWarning(Page.Rows[SelectedRow], SelectedSocket, Refusal);
			bTakeOffArmed = !Warning.IsEmpty();
			Message = bTakeOffArmed ? Warning : Refusal;
			RefreshList();
			return;
		}
		bTakeOffArmed = false;
		Message = WoRLoadoutScreen::TakeOffSocket(GetWorld(), Members[MemberIndex], Page.Rows[SelectedRow], SelectedSocket);
		RefreshData();
		RefreshSelection();
		return;
	}
	Message = Page.Rows[SelectedRow].Place == ELoadoutScreenPlace::None
				  ? LOCTEXT("ApparelNotYet", "Apparel can't be changed here yet.")
				  : WoRLoadoutScreen::UnequipRow(GetWorld(), Members[MemberIndex], Page.Rows[SelectedRow]);
	RefreshData();
	RefreshSelection();
}

void ULoadoutScreenWidget::MoveSocket(int32 Delta)
{
	if (bPicking || bSavedList || !Page.Rows.IsValidIndex(SelectedRow))
	{
		return;
	}
	const FLoadoutScreenRow &Row = Page.Rows[SelectedRow];
	const bool bHasSockets = (Row.Kind == ELoadoutScreenRowKind::Weapon || Row.Kind == ELoadoutScreenRowKind::Adapter) && Row.GearID.IsValid();
	// The row itself, then each socket (apparel's socket is B-440f's).
	const int32 Stops = bHasSockets ? Row.Sockets.Num() + 1 : 1;
	const int32 Stop = ((SelectedSocket + 1 + Delta) % Stops + Stops) % Stops;
	SelectedSocket = Stop - 1;
	bTakeOffArmed = false;
	Message = FText::GetEmpty();
	RefreshList();
	RefreshSelection(false);
}

void ULoadoutScreenWidget::SwitchPage()
{
	if (bPicking || bSavedList)
	{
		return;
	}
	bSkillsPage = !bSkillsPage;
	SelectedRow = 0;
	SelectedSocket = INDEX_NONE;
	SkillTab = 0;
	bTakeOffArmed = false;
	Message = FText::GetEmpty();
	RefreshData();
	RefreshSelection();
}

void ULoadoutScreenWidget::MoveSkillTab(int32 Delta)
{
	if (bPicking || bSavedList || Skills.Tabs.Num() < 2)
	{
		return;
	}
	SkillTab = ((SkillTab + Delta) % Skills.Tabs.Num() + Skills.Tabs.Num()) % Skills.Tabs.Num();
	SelectedRow = 0;
	Message = FText::GetEmpty();
	RefreshData();
	RefreshSelection();
}

void ULoadoutScreenWidget::PressSavedLoadouts()
{
	if (bRenaming || bPicking || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	if (!bSavedList)
	{
		bSavedList = true;
		Message = FText::GetEmpty();
		RefreshSaved();
		const int32 Active = Saved.Rows.IndexOfByPredicate([](const FLoadoutSavedRow &Row) { return Row.bActive; });
		SelectedSaved = FMath::Max(0, Active);
		RefreshList();
		RefreshSelection();
		return;
	}
	// In the list: rename the lit loadout.
	if (!Saved.Rows.IsValidIndex(SelectedSaved) || Saved.Rows[SelectedSaved].IsNewLine())
	{
		Message = LOCTEXT("RenameNewLine", "Make the loadout first (A), then name it.");
		RefreshList();
		return;
	}
	bRenaming = true;
	Message = FText::GetEmpty();
	if (NameBox)
	{
		NameBox->SetText(Saved.Rows[SelectedSaved].Name);
		NameBox->SetVisibility(ESlateVisibility::Visible); // not SetShown: the box takes clicks and the keys
		NameBox->SetKeyboardFocus();
	}
	RefreshList();
}

void ULoadoutScreenWidget::CommitRename(const FText &NewName)
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	if (!bSavedList || !Saved.Rows.IsValidIndex(SelectedSaved) || !Members.IsValidIndex(MemberIndex) ||
		!WoRLoadoutSaved::FindComponents(GetWorld(), Members[MemberIndex], Loadout, Inventory))
	{
		CancelRename();
		return;
	}
	Message = WoRLoadoutSaved::Rename(*Loadout, *Inventory, Saved.Rows[SelectedSaved].LoadoutIndex, NewName);
	CancelRename(); // closes the box; the message stays
}

void ULoadoutScreenWidget::CancelRename()
{
	if (!bRenaming)
	{
		return;
	}
	bRenaming = false;
	if (NameBox)
	{
		SetShown(NameBox, false);
		SetFocus(); // the keys come back to the screen
	}
	RefreshData();
	RefreshSaved();
	RefreshList();
	RefreshSelection();
}

void ULoadoutScreenWidget::HandleNameCommitted(const FText &Text, ETextCommit::Type CommitMethod)
{
	if (!bRenaming)
	{
		return; // hiding the box moves its focus away, which commits again
	}
	CommitMethod == ETextCommit::OnEnter ? CommitRename(Text) : CancelRename();
}

void ULoadoutScreenWidget::RefreshSaved()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	if (Members.IsValidIndex(MemberIndex) && WoRLoadoutSaved::FindComponents(GetWorld(), Members[MemberIndex], Loadout, Inventory))
	{
		Saved = WoRLoadoutSaved::BuildList(*Inventory);
	}
	else
	{
		Saved = FLoadoutSavedList();
		Saved.Refusal = LOCTEXT("NoSavedLoadouts", "This character keeps no loadouts.");
	}
	SelectedSaved = Saved.Rows.Num() > 0 ? FMath::Clamp(SelectedSaved, 0, Saved.Rows.Num() - 1) : 0;
}

void ULoadoutScreenWidget::CloseSavedList()
{
	bSavedList = false;
	bRenaming = false;
	SetShown(NameBox, false);
	SelectedRow = 0;
	SelectedSocket = INDEX_NONE;
	SkillTab = 0;
	RefreshData(); // the page of the loadout now in use
	RefreshSelection();
}

void ULoadoutScreenWidget::PressUseSaved()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	if (!Saved.Rows.IsValidIndex(SelectedSaved) || !Members.IsValidIndex(MemberIndex) ||
		!WoRLoadoutSaved::FindComponents(GetWorld(), Members[MemberIndex], Loadout, Inventory))
	{
		return;
	}
	if (Saved.Rows[SelectedSaved].IsNewLine())
	{
		int32 NewIndex = INDEX_NONE;
		Message = WoRLoadoutSaved::CreateNew(*Loadout, *Inventory, NewIndex);
		SelectedSaved = NewIndex != INDEX_NONE ? NewIndex : SelectedSaved; // the new one lit, ready for Y
		RefreshSaved();
		RefreshList();
		RefreshSelection();
		return;
	}
	Message = WoRLoadoutSaved::SetActive(*Loadout, *Inventory, Saved.Rows[SelectedSaved].LoadoutIndex);
	if (Message.IsEmpty())
	{
		CloseSavedList(); // back to the page, now of that loadout
		return;
	}
	RefreshList();
}

void ULoadoutScreenWidget::PressDuplicateSaved()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	if (!Saved.Rows.IsValidIndex(SelectedSaved) || Saved.Rows[SelectedSaved].IsNewLine() || !Members.IsValidIndex(MemberIndex) ||
		!WoRLoadoutSaved::FindComponents(GetWorld(), Members[MemberIndex], Loadout, Inventory))
	{
		return;
	}
	int32 NewIndex = INDEX_NONE;
	Message = WoRLoadoutSaved::Duplicate(*Loadout, *Inventory, Saved.Rows[SelectedSaved].LoadoutIndex, NewIndex);
	SelectedSaved = NewIndex != INDEX_NONE ? NewIndex : SelectedSaved;
	RefreshSaved();
	RefreshList();
	RefreshSelection();
}

void ULoadoutScreenWidget::ClosePicker()
{
	bPicking = false;
	bPickingSocket = false;
	Choices.Reset();
	SelectedChoice = 0;
	RefreshData(); // the page after the change, if one went in
	RefreshSelection();
}

bool ULoadoutScreenWidget::NativeOnHandleBackAction()
{
	if (bRenaming)
	{
		CancelRename(); // B-440e
		return true;
	}
	if (bSavedList)
	{
		CloseSavedList();
		return true;
	}
	if (!bPicking)
	{
		return Super::NativeOnHandleBackAction();
	}
	Message = FText::GetEmpty();
	ClosePicker();
	return true;
}

FReply ULoadoutScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key))
	{
		MoveSelection(-1);
		return FReply::Handled();
	}
	if (IsDownKey(Key))
	{
		MoveSelection(1);
		return FReply::Handled();
	}
	if (IsSubTabPrevKey(Key) || IsSubTabNextKey(Key))
	{
		if (!InKeyEvent.IsRepeat() && !bPicking)
		{
			MoveMember(IsSubTabNextKey(Key) ? 1 : -1);
		}
		return FReply::Handled();
	}
	if (IsTabPrevKey(Key) || IsTabNextKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			SwitchPage(); // two pages: either key turns to the other
		}
		return FReply::Handled();
	}
	if (IsLeftKey(Key) || IsRightKey(Key))
	{
		bSkillsPage ? MoveSkillTab(IsRightKey(Key) ? 1 : -1) : MoveSocket(IsRightKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsConfirmKey(Key) || IsToggleKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			IsConfirmKey(Key) ? PressChange() : PressUnequip();
		}
		return FReply::Handled();
	}
	if (IsSecondActionKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			PressSavedLoadouts(); // B-440e: open the saved loadouts; in them, rename
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

int32 ULoadoutScreenWidget::FindRowUnder(const FPointerEvent &MouseEvent) const
{
	// A row scrolled out of view keeps its geometry past the list's edge: only the list's own area counts.
	if (!RowList || !RowList->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < RowWidgets.Num() && Index < GetListCount(); ++Index)
	{
		const UBorder *Frame = RowWidgets[Index].Frame.Get();
		if (Frame && Frame->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 ULoadoutScreenWidget::FindMemberTabUnder(const FPointerEvent &MouseEvent) const
{
	for (int32 Index = 0; Index < MemberTabs.Num() && Index < Members.Num(); ++Index)
	{
		if (MemberTabs[Index] && MemberTabs[Index]->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FReply ULoadoutScreenWidget::NativeOnMouseMove(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	// Only a moving mouse picks a row, so a resting cursor doesn't fight the keys.
	if (!InMouseEvent.GetCursorDelta().IsZero())
	{
		SelectRow(FindRowUnder(InMouseEvent), /*bScrollIntoView=*/false); // the list mustn't move under the cursor
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply ULoadoutScreenWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	const int32 Tab = bPicking || bSavedList ? INDEX_NONE : FindMemberTabUnder(InMouseEvent);
	if (Tab != INDEX_NONE)
	{
		MoveMember(Tab - MemberIndex);
		return FReply::Handled();
	}
	const int32 Row = FindRowUnder(InMouseEvent);
	if (Row == INDEX_NONE)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	SelectRow(Row, /*bScrollIntoView=*/false);
	PressChange(); // a click is A on that row (InputMap: Select)
	return FReply::Handled();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GLoadoutSavedPrintCommand(
	TEXT("wor.UI.LoadoutSaved.Print"),
	TEXT("B-440e: print each party member's saved loadouts as the loadout screen's Y list shows them."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		for (const FLoadoutScreenMember &Member : WoRLoadoutScreen::BuildMembers(World))
		{
			ULoadoutComponent *Loadout = nullptr;
			UInventoryComponent *Inventory = nullptr;
			if (WoRLoadoutSaved::FindComponents(World, Member, Loadout, Inventory))
			{
				UE_LOG(LogWoRUI, Display, TEXT("[LoadoutSaved] %s"), *WoRLoadoutSaved::GetSavedListString(*Inventory));
			}
		}
	}));

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GLoadoutScreenOpenCommand(
	TEXT("wor.UI.Loadout"),
	TEXT("B-440a: open the loadout screen (gear page; B-440b: A Change, X Unequip), as the game menu's Loadout page does."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!ULoadoutScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[LoadoutScreen] wor.UI.Loadout: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
