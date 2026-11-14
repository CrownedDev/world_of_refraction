// CharacterSheetWidget.cpp

#include "UI/Menus/CharacterSheetWidget.h"

#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionPipsRules.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRCharacterSheetWidget"

namespace CharacterSheetStyle
{
	// This screen's own layout (screen_mockups/menus/character, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float HeaderGap = 27.0f;
	constexpr float PanelGap = 30.0f;
	constexpr float PanelPadding = 30.0f;
	constexpr float IdentityWidth = 528.0f;
	constexpr float PillarsWidth = 630.0f;
	constexpr float ResistanceWidth = 558.0f;
	constexpr float VitalLabelWidth = 60.0f;
	constexpr float VitalBarWidth = 390.0f;
	constexpr float LineLabelWidth = 255.0f;
	constexpr float ActionPipSize = 21.0f;
	constexpr float ActionPointBarWidth = 150.0f;
	constexpr float ActionPointBarHeight = 9.0f;
	constexpr float PillarNameWidth = 180.0f;
	constexpr float PillarPipSize = 24.0f;
	constexpr float StatIndent = 30.0f;
	constexpr float ResistLabelWidth = 210.0f;
	constexpr float ResistHalfBarWidth = 90.0f;
	constexpr float ResistBarHeight = 12.0f;
	constexpr float ResistMidlineWidth = 2.0f;
	constexpr float ResistValueWidth = 105.0f;
	constexpr float ResistLineGap = 21.0f;
	constexpr int32 StatsPerPillar = 4;
	constexpr int32 PillarCount = 3;
	constexpr int32 MaxCurses = 3;

	UBorder *MakePanel(UWidgetTree *Tree, UVerticalBox *&OutColumn)
	{
		UBorder *Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(PanelPadding));
		OutColumn = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(OutColumn);
		return Panel;
	}

	/** "Label ........ value" on one line; returns the value text. */
	UTextBlock *AddLabelledValue(UWidgetTree *Tree, UVerticalBox *Column, const FText &Label, UWidget *Middle = nullptr, float TopGap = LineGap)
	{
		UHorizontalBox *Line = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *LabelText = MakeText(Tree, LineFontSize, DimColor);
		LabelText->SetText(Label);
		AddToRow(Line, MakeSized(Tree, LineLabelWidth, 0.0f, LabelText));
		if (Middle)
		{
			AddToRow(Line, Middle);
		}
		UTextBlock *Value = MakeText(Tree, LineFontSize, TextColor, true);
		UHorizontalBoxSlot *ValueSlot = AddToRow(Line, Value);
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetHorizontalAlignment(HAlign_Right);
		AddToColumn(Column, Line, FMargin(0.0f, TopGap, 0.0f, 0.0f));
		return Value;
	}
}

UCharacterSheetWidget::UCharacterSheetWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself
}

UCharacterSheetWidget *UCharacterSheetWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[CharacterSheet] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<UCharacterSheetWidget>(Root->PushToMenu(UCharacterSheetWidget::StaticClass()));
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
	AddToColumn(Main, MemberRow);

	RefusalText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
	AddToColumn(Main, RefusalText, FMargin(0.0f, LineGap, 0.0f, 0.0f));

	UHorizontalBox *Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddToColumn(Main, Body, FMargin(0.0f, HeaderGap, 0.0f, SectionGap))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddToRow(Body, MakeSized(WidgetTree, IdentityWidth, 0.0f, BuildIdentityPanel()), FMargin(0.0f), VAlign_Fill);
	AddToRow(Body, MakeSized(WidgetTree, PillarsWidth, 0.0f, BuildPillarsPanel()), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);
	AddToRow(Body, MakeSized(WidgetTree, ResistanceWidth, 0.0f, BuildResistancePanel()), FMargin(PanelGap, 0.0f, 0.0f, 0.0f), VAlign_Fill);

	// Bottom right: the hints. TODO: "R3 Where it comes from" and "Y Loadout" when those are built.
	UTextBlock *Hints = MakeHintText(WidgetTree, LineFontSize, DimColor); // B-481: the keys of the device in use
	WoRKeyHints::SetHint(Hints, LOCTEXT("Hints", "{SubTabs}  Character        {Back}  Back"));
	AddToColumn(Main, Hints)->SetHorizontalAlignment(HAlign_Right);
}

UWidget *UCharacterSheetWidget::BuildIdentityPanel()
{
	using namespace CharacterSheetStyle;

	UVerticalBox *Column = nullptr;
	UBorder *Panel = MakePanel(WidgetTree, Column);
	NameText = MakeText(WidgetTree, NameFontSize, TextColor, true);
	ClassLineText = MakeText(WidgetTree, LineFontSize, DimColor);
	AddToColumn(Column, NameText);
	AddToColumn(Column, ClassLineText, FMargin(0.0f, 0.0f, 0.0f, SectionGap));

	const auto AddVital = [this, Column](const FText &Label, const FLinearColor &Color, TObjectPtr<UProgressBar> &OutBar, TObjectPtr<UTextBlock> &OutValue)
	{
		UHorizontalBox *Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *LabelText = MakeText(WidgetTree, SmallFontSize, DimColor, true);
		LabelText->SetText(Label);
		UProgressBar *Bar = nullptr;
		UTextBlock *Value = nullptr;
		UWidget *BarWithValue = MakeBarWithValue(WidgetTree, Color, Bar, Value);
		AddToRow(Row, MakeSized(WidgetTree, VitalLabelWidth, 0.0f, LabelText));
		AddToRow(Row, MakeSized(WidgetTree, VitalBarWidth, BarWithValueHeight, BarWithValue));
		AddToColumn(Column, Row, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		OutBar = Bar;
		OutValue = Value;
	};
	AddVital(LOCTEXT("HP", "HP"), HPColor, HPBar, HPValue);
	AddVital(LOCTEXT("EP", "EP"), EPColor, EPBar, EPValue);

	UHorizontalBox *Pips = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 Index = 0; Index < ActionPipsConstants::MAX_ACTIONS_CAP; ++Index)
	{
		UBorder *Pip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Pip->SetBrushColor(CyanColor);
		ActionPips.Add(Pip);
		AddToRow(Pips, MakeSized(WidgetTree, ActionPipSize, ActionPipSize, Pip), FMargin(0.0f, 0.0f, WordGap, 0.0f));
	}
	MaxActionsValue = AddLabelledValue(WidgetTree, Column, LOCTEXT("MaxActions", "Max Actions"), Pips, SectionGap);
	ActionPointBar = MakeBar(WidgetTree, CyanColor);
	ActionPointValue = AddLabelledValue(WidgetTree, Column, LOCTEXT("NextActionPoint", "next action point"),
										MakeSized(WidgetTree, ActionPointBarWidth, ActionPointBarHeight, ActionPointBar));
	ActionRegainValue = AddLabelledValue(WidgetTree, Column, LOCTEXT("ActionRegain", "Regains per turn"));

	UTextBlock *CurseHeading = MakeText(WidgetTree, LineFontSize, DimColor, true);
	CurseHeading->SetText(LOCTEXT("Curses", "Curses"));
	AddToColumn(Column, CurseHeading, FMargin(0.0f, SectionGap + SectionGap, 0.0f, 0.0f));
	for (int32 Index = 0; Index < MaxCurses; ++Index)
	{
		CurseTitles.Add(MakeText(WidgetTree, LineFontSize, TextColor, true));
		CurseDescriptions.Add(MakeWrappedText(WidgetTree, SmallFontSize, DimColor));
		AddToColumn(Column, CurseTitles.Last(), FMargin(0.0f, LineGap, 0.0f, 0.0f));
		AddToColumn(Column, CurseDescriptions.Last());
	}
	CurseNote = MakeWrappedText(WidgetTree, SmallFontSize, OffColor);
	AddToColumn(Column, CurseNote, FMargin(0.0f, LineGap, 0.0f, 0.0f));
	return Panel;
}

UWidget *UCharacterSheetWidget::BuildPillarsPanel()
{
	using namespace CharacterSheetStyle;

	UVerticalBox *Column = nullptr;
	UBorder *Panel = MakePanel(WidgetTree, Column);
	for (int32 PillarIndex = 0; PillarIndex < PillarCount; ++PillarIndex)
	{
		FPillarWidgets Widgets;
		UHorizontalBox *Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
		AddToRow(Head, MakeSized(WidgetTree, PillarNameWidth, 0.0f, Name));
		for (int32 Pip = 0; Pip < WorldStatConstants::LIVE_MAX; ++Pip)
		{
			UBorder *PipWidget = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Widgets.Pips.Add(PipWidget);
			AddToRow(Head, MakeSized(WidgetTree, PillarPipSize, PillarPipSize, PipWidget), FMargin(0.0f, 0.0f, WordGap, 0.0f));
		}
		UTextBlock *Level = MakeText(WidgetTree, LineFontSize, TextColor, true);
		UHorizontalBoxSlot *LevelSlot = AddToRow(Head, Level);
		LevelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LevelSlot->SetHorizontalAlignment(HAlign_Right);
		AddToColumn(Column, Head, FMargin(0.0f, PillarIndex == 0 ? 0.0f : SectionGap, 0.0f, 0.0f));

		for (int32 StatIndex = 0; StatIndex < StatsPerPillar; ++StatIndex)
		{
			UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UTextBlock *Label = MakeText(WidgetTree, LineFontSize, DimColor);
			UTextBlock *Value = MakeText(WidgetTree, LineFontSize, TextColor, true);
			AddToRow(Line, Label, FMargin(StatIndent, 0.0f, 0.0f, 0.0f));
			UHorizontalBoxSlot *ValueSlot = AddToRow(Line, Value);
			ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ValueSlot->SetHorizontalAlignment(HAlign_Right);
			AddToColumn(Column, Line, FMargin(0.0f, LineGap, 0.0f, 0.0f));
			Widgets.StatLines.Add(Line);
			Widgets.StatLabels.Add(Label);
			Widgets.StatValues.Add(Value);
		}
		Widgets.Name = Name;
		Widgets.Level = Level;
		PillarWidgets.Add(MoveTemp(Widgets));
	}
	return Panel;
}

UWidget *UCharacterSheetWidget::BuildResistancePanel()
{
	using namespace CharacterSheetStyle;

	UVerticalBox *Column = nullptr;
	UBorder *Panel = MakePanel(WidgetTree, Column);
	UTextBlock *Heading = MakeText(WidgetTree, RowFontSize, TextColor, true);
	Heading->SetText(LOCTEXT("Resistance", "Resistance"));
	UTextBlock *Subheading = MakeText(WidgetTree, SmallFontSize, DimColor);
	Subheading->SetText(LOCTEXT("ResistanceSub", "less damage taken, by type"));
	AddToColumn(Column, Heading);
	AddToColumn(Column, Subheading, FMargin(0.0f, 0.0f, 0.0f, LineGap));

	const int32 ScopeLines = WoRCharacterSheet::SCOPE_LINE_COUNT;
	for (int32 Index = 0; Index < WoRCharacterSheet::RESISTANCE_LINE_COUNT; ++Index)
	{
		FResistanceWidgets Widgets;
		UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock *Label = MakeText(WidgetTree, LineFontSize, TextColor);
		UProgressBar *WeakBar = MakeBar(WidgetTree, LowHPColor);
		WeakBar->SetBarFillType(EProgressBarFillType::RightToLeft);
		UProgressBar *ResistBar = MakeBar(WidgetTree, GoodColor);
		UBorder *Midline = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Midline->SetBrushColor(DimColor);
		UTextBlock *Value = MakeText(WidgetTree, LineFontSize, OffColor, true);
		AddToRow(Line, MakeSized(WidgetTree, ResistLabelWidth, 0.0f, Label));
		AddToRow(Line, MakeSized(WidgetTree, ResistHalfBarWidth, ResistBarHeight, WeakBar));
		AddToRow(Line, MakeSized(WidgetTree, ResistMidlineWidth, ResistBarHeight + ResistBarHeight, Midline));
		AddToRow(Line, MakeSized(WidgetTree, ResistHalfBarWidth, ResistBarHeight, ResistBar));
		UHorizontalBoxSlot *ValueSlot = AddToRow(Line, MakeSized(WidgetTree, ResistValueWidth, 0.0f, Value));
		ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueSlot->SetHorizontalAlignment(HAlign_Right);
		Value->SetJustification(ETextJustify::Right);
		// The scope lines (All, Physical, Spell) sit apart from the elements.
		AddToColumn(Column, Line, FMargin(0.0f, Index == ScopeLines ? ResistLineGap + SectionGap : ResistLineGap, 0.0f, 0.0f));
		Widgets.Root = Line;
		Widgets.Label = Label;
		Widgets.WeakBar = WeakBar;
		Widgets.ResistBar = ResistBar;
		Widgets.Value = Value;
		ResistanceWidgets.Add(MoveTemp(Widgets));
	}
	return Panel;
}

void UCharacterSheetWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	Refresh();
	UE_LOG(LogWoRUI, Log, TEXT("[CharacterSheet] Opened (%d member(s))"), Members.Num());
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
	if (!NameText)
	{
		return; // headless: no tree
	}
	RefusalText->SetText(Page.Refusal);
	SetShown(RefusalText, !Page.Refusal.IsEmpty());

	const FPauseMenuMember &Who = Page.Who;
	const FText ValueFormat = LOCTEXT("VitalValue", "{0} / {1}");
	NameText->SetText(Who.Tier.IsEmpty() ? Who.Name : FText::Format(LOCTEXT("NameAndTier", "{0} ({1})"), Who.Name, Who.Tier));
	ClassLineText->SetText(Who.ClassLine);
	const float HPFraction = Who.MaxHP > 0 ? static_cast<float>(Who.HP) / Who.MaxHP : 0.0f;
	HPBar->SetPercent(HPFraction);
	HPValue->SetText(FText::Format(ValueFormat, FText::AsNumber(Who.HP), FText::AsNumber(Who.MaxHP)));
	EPBar->SetPercent(Who.MaxEP > 0 ? static_cast<float>(Who.EP) / Who.MaxEP : 0.0f);
	EPValue->SetText(FText::Format(ValueFormat, FText::AsNumber(Who.EP), FText::AsNumber(Who.MaxEP)));

	for (int32 Index = 0; Index < ActionPips.Num(); ++Index)
	{
		SetShown(ActionPips[Index], Index < Page.MaxActions);
	}
	MaxActionsValue->SetText(FText::AsNumber(Page.MaxActions));
	ActionPointBar->SetPercent(Page.PointsPerAction > 0 ? static_cast<float>(Page.ActionPointProgress) / Page.PointsPerAction : 0.0f);
	ActionPointValue->SetText(FText::Format(ValueFormat, FText::AsNumber(Page.ActionPointProgress), FText::AsNumber(Page.PointsPerAction)));
	ActionRegainValue->SetText(FText::AsNumber(Page.ActionRegain));

	for (int32 Index = 0; Index < CurseTitles.Num(); ++Index)
	{
		const bool bShown = Page.Curses.IsValidIndex(Index);
		SetShown(CurseTitles[Index], bShown);
		SetShown(CurseDescriptions[Index], bShown);
		if (bShown)
		{
			CurseTitles[Index]->SetText(Page.Curses[Index].Title);
			CurseDescriptions[Index]->SetText(Page.Curses[Index].Description);
		}
	}
	CurseNote->SetText(Page.Curses.Num() > 0 ? LOCTEXT("CurseNote", "Cleared at the Spiritualist") : LOCTEXT("NoCurses", "None"));

	for (int32 PillarIndex = 0; PillarIndex < PillarWidgets.Num(); ++PillarIndex)
	{
		const FPillarWidgets &Widgets = PillarWidgets[PillarIndex];
		const bool bHasPillar = Page.Pillars.IsValidIndex(PillarIndex);
		const FCharacterSheetPillar Pillar = bHasPillar ? Page.Pillars[PillarIndex] : FCharacterSheetPillar();
		if (!Widgets.Name.IsValid() || !Widgets.Level.IsValid())
		{
			continue;
		}
		Widgets.Name->SetText(Pillar.Name);
		Widgets.Level->SetText(bHasPillar ? FText::Format(ValueFormat, FText::AsNumber(Pillar.Level), FText::AsNumber(Pillar.MaxLevel)) : FText::GetEmpty());
		for (int32 Pip = 0; Pip < Widgets.Pips.Num(); ++Pip)
		{
			if (UBorder *PipWidget = Widgets.Pips[Pip].Get())
			{
				SetShown(PipWidget, bHasPillar);
				PipWidget->SetBrushColor(Pip < Pillar.Level ? CyanColor : TrackColor);
			}
		}
		for (int32 StatIndex = 0; StatIndex < Widgets.StatLines.Num(); ++StatIndex)
		{
			const bool bShown = Pillar.Stats.IsValidIndex(StatIndex);
			SetShown(Widgets.StatLines[StatIndex].Get(), bShown);
			if (bShown && Widgets.StatLabels[StatIndex].IsValid() && Widgets.StatValues[StatIndex].IsValid())
			{
				Widgets.StatLabels[StatIndex]->SetText(Pillar.Stats[StatIndex].Label);
				Widgets.StatValues[StatIndex]->SetText(Pillar.Stats[StatIndex].Value);
			}
		}
	}

	for (int32 Index = 0; Index < ResistanceWidgets.Num(); ++Index)
	{
		const FResistanceWidgets &Widgets = ResistanceWidgets[Index];
		const bool bShown = Page.Resistances.IsValidIndex(Index);
		SetShown(Widgets.Root.Get(), bShown);
		if (!bShown || !Widgets.Label.IsValid() || !Widgets.WeakBar.IsValid() || !Widgets.ResistBar.IsValid() || !Widgets.Value.IsValid())
		{
			continue;
		}
		const FCharacterSheetResistance &Line = Page.Resistances[Index];
		const bool bNothing = FMath::IsNearlyZero(Line.Fraction);
		Widgets.Label->SetText(Line.Label);
		Widgets.Label->SetColorAndOpacity(FSlateColor(bNothing ? DimColor : TextColor));
		const float BarFullAt = FMath::Max(Line.Cap, UE_KINDA_SMALL_NUMBER);
		Widgets.WeakBar->SetPercent(FMath::Clamp(-Line.Fraction / BarFullAt, 0.0f, 1.0f));
		Widgets.ResistBar->SetPercent(FMath::Clamp(Line.Fraction / BarFullAt, 0.0f, 1.0f));
		Widgets.Value->SetText(WoRCharacterSheet::FormatPercent(Line.Fraction, /*bSigned=*/true));
		Widgets.Value->SetColorAndOpacity(FSlateColor(bNothing ? OffColor : Line.Fraction > 0.0f ? GoodColor : LowHPColor));
	}
}

void UCharacterSheetWidget::MoveMember(int32 Delta)
{
	if (Members.Num() < 2)
	{
		return;
	}
	MemberIndex = ((MemberIndex + Delta) % Members.Num() + Members.Num()) % Members.Num();
	Refresh();
}

FReply UCharacterSheetWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsSubTabPrevKey(Key) || IsSubTabNextKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			MoveMember(IsSubTabNextKey(Key) ? 1 : -1);
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

int32 UCharacterSheetWidget::FindMemberTabUnder(const FPointerEvent &MouseEvent) const
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

FReply UCharacterSheetWidget::NativeOnMouseButtonDown(const FGeometry &InGeometry, const FPointerEvent &InMouseEvent)
{
	const int32 Tab = FindMemberTabUnder(InMouseEvent);
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || Tab == INDEX_NONE)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	MoveMember(Tab - MemberIndex);
	return FReply::Handled();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GCharacterSheetOpenCommand(
	TEXT("wor.UI.Sheet"),
	TEXT("B-442: open the character sheet, as the game menu's Character page does."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!UCharacterSheetWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[CharacterSheet] wor.UI.Sheet: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
