// LoadoutV2ScreenWidget.cpp

#include "UI/Menus/LoadoutV2ScreenWidget.h"

#include "Components/Border.h"
#include "Engine/World.h"
#include "Equipment/Weapons/WeaponData.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutV2Screen"

namespace LoadoutV2ScreenStyle
{
	// This screen's own layout (mockups menus/loadout/v2, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float ListWidth = 1500.0f;
	constexpr float RowPadding = 21.0f;
	constexpr float RowGap = 12.0f;
	constexpr float NameColumnWidth = 900.0f;

	FText SocketWord(ELoadoutV2SocketIcon Icon)
	{
		switch (Icon)
		{
		case ELoadoutV2SocketIcon::Crystal:
			return LOCTEXT("SocketCrystal", "Crystal");
		case ELoadoutV2SocketIcon::Stone:
			return LOCTEXT("SocketStone", "Stone");
		case ELoadoutV2SocketIcon::Evolution:
			return LOCTEXT("SocketEvolution", "Evolution");
		case ELoadoutV2SocketIcon::Empty:
		default:
			return LOCTEXT("SocketEmpty", "empty");
		}
	}

	FText JoinNames(const TArray<FText> &Names)
	{
		return Names.IsEmpty() ? LOCTEXT("NoneShort", "none") : FText::Join(LOCTEXT("NameSeparator", ", "), Names);
	}
}

ULoadoutV2ScreenWidget::ULoadoutV2ScreenWidget(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true); // the screen takes the keys itself
}

ULoadoutV2ScreenWidget *ULoadoutV2ScreenWidget::Open(APlayerController *PC)
{
	AWoRHubController *Hub = Cast<AWoRHubController>(PC);
	UWoRRootLayout *Root = Hub ? Hub->GetRootLayout() : nullptr;
	if (!Root)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[LoadoutV2] No AWoRHubController with a root layout - is RootLayoutClass set?"));
		return nullptr;
	}
	return Cast<ULoadoutV2ScreenWidget>(Root->PushToMenu(ULoadoutV2ScreenWidget::StaticClass()));
}

TSharedRef<SWidget> ULoadoutV2ScreenWidget::RebuildWidget()
{
	using namespace LoadoutV2ScreenStyle;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder *Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
		Screen->SetBrushColor(ScreenColor);
		Screen->SetPadding(FMargin(ScreenPadding));
		WidgetTree->RootWidget = Screen;
		UVerticalBox *Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Screen->SetContent(Main);

		UHorizontalBox *Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddToRow(Header, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabPrev", "LT")), FMargin(0.0f, 0.0f, SectionGap, 0.0f));
		MemberText = MakeText(WidgetTree, RowFontSize, TextColor, true);
		AddToRow(Header, MemberText, FMargin(0.0f, 0.0f, SectionGap, 0.0f));
		AddToRow(Header, MakeKeyLabel(WidgetTree, LOCTEXT("KeySubTabNext", "RT")), FMargin(0.0f, 0.0f, SectionGap * 2.0f, 0.0f));
		UTextBlock *Tab = MakeText(WidgetTree, RowFontSize, CyanColor, true);
		Tab->SetText(LOCTEXT("TabWeapons", "Weapons")); // TODO: B-476c, the Apparel and Skills tabs on LB / RB
		AddToRow(Header, Tab)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CellsText = MakeText(WidgetTree, RowFontSize, TextColor, true);
		AddToRow(Header, CellsText);
		AddToColumn(Main, Header);

		TitleText = MakeText(WidgetTree, LineFontSize, DimColor, true);
		AddToColumn(Main, TitleText, FMargin(0.0f, SectionGap, 0.0f, LineGap));
		ListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddToColumn(Main, MakeSized(WidgetTree, ListWidth, 0.0f, ListBox))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CompareText = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
		AddToColumn(Main, CompareText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		MessageText = MakeWrappedText(WidgetTree, LineFontSize, WarnColor);
		AddToColumn(Main, MessageText, FMargin(0.0f, LineGap, 0.0f, 0.0f));
		HintsText = MakeText(WidgetTree, LineFontSize, DimColor);
		AddToColumn(Main, HintsText)->SetHorizontalAlignment(HAlign_Right);
	}
	return Super::RebuildWidget();
}

void ULoadoutV2ScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	Message = FText::GetEmpty();
	RefreshData();
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] Opened: %s"), *WoRLoadoutV2::GetWeaponsPageString(Page));
}

UWidget *ULoadoutV2ScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<ULoadoutV2ScreenWidget *>(this); // the screen itself takes the keys (NativeOnKeyDown)
}

void ULoadoutV2ScreenWidget::RefreshData()
{
	UWorld *World = GetWorld();
	Members = WoRLoadoutScreen::BuildMembers(World);
	MemberIndex = Members.Num() > 0 ? FMath::Clamp(MemberIndex, 0, Members.Num() - 1) : 0;
	Page = FLoadoutV2WeaponsPage();
	UInventoryComponent *Inventory = nullptr;
	const ULoadoutComponent *Loadout = Members.IsValidIndex(MemberIndex) ? WoRLoadoutScreen::FindMemberLoadout(World, Members[MemberIndex], Inventory) : nullptr;
	const FCombatLoadout *Active = Loadout && Inventory && Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)
									   ? &Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex]
									   : nullptr;
	if (Active)
	{
		Page = WoRLoadoutV2::BuildWeaponsPage(*Active, Members[MemberIndex].Class);
		if (bAdding)
		{
			Choices = WoRLoadoutV2::BuildMainHandChoices(*Active, Members[MemberIndex].Class, Inventory->Weapons);
		}
	}
	else
	{
		Choices.Reset();
		if (Message.IsEmpty())
		{
			Message = Members.Num() == 0 ? LOCTEXT("NoParty", "There is no party to show.") : LOCTEXT("MemberNotHere", "This character isn't here.");
		}
	}
	Selected = GetLineCount() > 0 ? FMath::Clamp(Selected, 0, GetLineCount() - 1) : 0;
	if (Picked != INDEX_NONE && !Choices.IsValidIndex(Picked))
	{
		Picked = INDEX_NONE;
	}
	RefreshView();
}

int32 ULoadoutV2ScreenWidget::GetLineCount() const
{
	if (bAdding)
	{
		return Choices.Num();
	}
	return Page.Entries.Num() + (Page.bCanAdd ? 1 : 0); // the Add weapon frame last
}

FText ULoadoutV2ScreenWidget::GetLineName(int32 Line) const
{
	if (bAdding)
	{
		if (!Choices.IsValidIndex(Line))
		{
			return FText::GetEmpty();
		}
		const FText Name = FText::FromString(Choices[Line].Weapon.Weapon ? Choices[Line].Weapon.Weapon->GetDisplayName() : FString());
		return Line == Picked ? FText::Format(LOCTEXT("PickedName", "{0}  PICKED"), Name) : Name;
	}
	if (Page.Entries.IsValidIndex(Line))
	{
		const FLoadoutV2Entry &Entry = Page.Entries[Line];
		return Entry.bActive ? FText::Format(LOCTEXT("ActiveName", "{0}  ACTIVE"), Entry.Name) : Entry.Name;
	}
	return LOCTEXT("AddWeapon", "+ Add weapon");
}

FText ULoadoutV2ScreenWidget::GetLineDetail(int32 Line) const
{
	using namespace LoadoutV2ScreenStyle;
	if (bAdding)
	{
		if (!Choices.IsValidIndex(Line))
		{
			return FText::GetEmpty();
		}
		return Choices[Line].Refusal.IsEmpty() ? FText::Format(LOCTEXT("CellsAfter", "Cells after: {0} of {1}"), FText::AsNumber(Choices[Line].CellsAfter),
															   FText::AsNumber(Page.CellsBudget))
											   : Choices[Line].Refusal;
	}
	if (Page.Entries.IsValidIndex(Line))
	{
		TArray<FText> Sockets;
		for (const ELoadoutV2SocketIcon Icon : Page.Entries[Line].Sockets)
		{
			Sockets.Add(SocketWord(Icon));
		}
		return Sockets.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("SocketsLine", "Sockets: {0}"), FText::Join(LOCTEXT("SocketGap", " · "), Sockets));
	}
	return Page.AddFrameLine;
}

bool ULoadoutV2ScreenWidget::IsLineRefused(int32 Line) const
{
	return bAdding && Choices.IsValidIndex(Line) && !Choices[Line].Refusal.IsEmpty();
}

void ULoadoutV2ScreenWidget::RefreshView()
{
	using namespace LoadoutV2ScreenStyle;
	if (!ListBox)
	{
		return; // headless: no tree
	}
	MemberText->SetText(Members.IsValidIndex(MemberIndex) ? Members[MemberIndex].Name : FText::GetEmpty());
	CellsText->SetText(Page.CellsLine);
	TitleText->SetText(bAdding ? LOCTEXT("WindowTitle", "Add gear · Weapon · Main hand") : FText::GetEmpty());
	SetShown(TitleText, bAdding);
	const int32 Count = GetLineCount();
	while (LineRows.Num() < Count)
	{
		// Rows are made the first time a page needs them and refilled after, never rebuilt.
		UBorder *Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Row->SetPadding(FMargin(RowPadding));
		UHorizontalBox *Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->SetContent(Line);
		UTextBlock *Name = MakeText(WidgetTree, RowFontSize, TextColor, true);
		UTextBlock *Detail = MakeText(WidgetTree, LineFontSize, DimColor);
		AddToRow(Line, MakeSized(WidgetTree, NameColumnWidth, 0.0f, Name), FMargin(0.0f), VAlign_Center);
		AddToRow(Line, Detail, FMargin(0.0f), VAlign_Center)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		AddToColumn(ListBox, Row, FMargin(0.0f, 0.0f, 0.0f, RowGap));
		LineRows.Add(Row);
		LineNames.Add(Name);
		LineDetails.Add(Detail);
	}
	for (int32 Index = 0; Index < LineRows.Num(); ++Index)
	{
		const bool bShown = Index < Count;
		SetShown(LineRows[Index], bShown);
		if (!bShown)
		{
			continue;
		}
		LineRows[Index]->SetBrushColor(Index == Selected ? FocusColor : PlateColor);
		LineNames[Index]->SetText(GetLineName(Index));
		LineNames[Index]->SetColorAndOpacity(FSlateColor(IsLineRefused(Index) ? OffColor : TextColor)); // refused: stays, greyed
		LineDetails[Index]->SetText(GetLineDetail(Index));
		LineDetails[Index]->SetColorAndOpacity(FSlateColor(IsLineRefused(Index) ? WarnColor : DimColor));
	}

	FText Compare;
	if (bAdding && bComparing && Choices.IsValidIndex(Selected) && Members.IsValidIndex(MemberIndex))
	{
		UInventoryComponent *Inventory = nullptr;
		WoRLoadoutScreen::FindMemberLoadout(GetWorld(), Members[MemberIndex], Inventory);
		const FCombatLoadout *Active = Inventory && Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex)
										   ? &Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex]
										   : nullptr;
		if (Active && Active->PrimaryWeapon.IsValid())
		{
			const FLoadoutV2Compare Result = WoRLoadoutV2::BuildCompare(Active->PrimaryWeapon.WeaponEntry, Choices[Selected].Weapon);
			Compare = FText::Format(LOCTEXT("CompareLine", "Equipped / Selected · Power {0} / {1} · Cells change {2} · Built-in gained: {3} · lost: {4}"),
									FText::AsNumber(Result.EquippedPower), FText::AsNumber(Result.SelectedPower), FText::AsNumber(Result.CellsChange),
									JoinNames(Result.BuiltInsGained), JoinNames(Result.BuiltInsLost));
		}
		else
		{
			Compare = LOCTEXT("CompareNothing", "No weapon equipped to compare with.");
		}
	}
	CompareText->SetText(Compare);
	SetShown(CompareText, !Compare.IsEmpty());
	MessageText->SetText(Message);
	SetShown(MessageText, !Message.IsEmpty());
	HintsText->SetText(bAdding ? LOCTEXT("HintsWindow", "A  Pick        X  Compare        Y  Confirm        B / Esc  Cancel")
							   : LOCTEXT("Hints", "A  Add weapon        LT / RT  Character        B / Esc  Back"));
}

void ULoadoutV2ScreenWidget::MoveSelection(int32 Delta)
{
	const int32 Count = GetLineCount();
	if (Count > 0)
	{
		Selected = ((Selected + Delta) % Count + Count) % Count;
		Message = FText::GetEmpty();
		RefreshView();
	}
}

void ULoadoutV2ScreenWidget::MoveMember(int32 Delta)
{
	if (bAdding || Members.Num() < 2)
	{
		return;
	}
	MemberIndex = ((MemberIndex + Delta) % Members.Num() + Members.Num()) % Members.Num();
	Selected = 0;
	Message = FText::GetEmpty();
	RefreshData();
}

void ULoadoutV2ScreenWidget::PressA()
{
	if (!bAdding)
	{
		const bool bOnFrame = Page.bCanAdd && Selected == Page.Entries.Num();
		if (!bOnFrame)
		{
			// TODO: B-476c, A Edit on an entry (the Add gear window for it).
			Message = Page.Entries.IsValidIndex(Selected) ? LOCTEXT("EditNotYet", "Editing an entry comes with the next part of this screen.") : FText::GetEmpty();
			RefreshView();
			return;
		}
		bAdding = true;
		Selected = 0;
		Picked = INDEX_NONE;
		bComparing = false;
		Message = FText::GetEmpty();
		RefreshData();
		if (Choices.Num() == 0)
		{
			Message = LOCTEXT("NoSpareWeapon", "You own no other weapon to add.");
			RefreshView();
		}
		return;
	}
	if (!Choices.IsValidIndex(Selected))
	{
		return;
	}
	Picked = Picked == Selected ? INDEX_NONE : Selected; // A again on the picked piece: Remove
	Message = Picked == INDEX_NONE ? FText::GetEmpty() : Choices[Picked].Refusal;
	RefreshView();
}

void ULoadoutV2ScreenWidget::PressCompare()
{
	if (!bAdding)
	{
		return; // TODO: B-476c, X Details on an entry
	}
	bComparing = !bComparing;
	RefreshView();
}

void ULoadoutV2ScreenWidget::PressConfirm()
{
	if (!bAdding || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	if (!Choices.IsValidIndex(Picked))
	{
		Message = LOCTEXT("PickFirst", "Pick a main hand first (A).");
		RefreshView();
		return;
	}
	if (!Choices[Picked].Refusal.IsEmpty())
	{
		Message = Choices[Picked].Refusal;
		RefreshView();
		return;
	}
	UInventoryComponent *Inventory = nullptr;
	ULoadoutComponent *Loadout = WoRLoadoutScreen::FindMemberLoadout(GetWorld(), Members[MemberIndex], Inventory);
	if (!Loadout || !Inventory)
	{
		Message = LOCTEXT("MemberNotHere", "This character isn't here.");
		RefreshView();
		return;
	}
	FLoadoutV2Draft Draft;
	Draft.Kind = ELoadoutV2DraftKind::Weapon;
	Draft.MainHand = Choices[Picked].Weapon;
	Draft.Socket1 = Draft.MainHand.AttachedItem; // the piece's own sockets (the window's socket rows are B-476c)
	Draft.Socket2 = Draft.MainHand.StoneSocket;
	const ECharacterClass Class = Members[MemberIndex].Class;
	FText Refusal;
	const bool bDone = Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [&Draft, Class](FCombatLoadout &Changed)
											  {
												  FText Ignored;
												  WoRLoadoutV2::ApplyDraft(Changed, Class, Draft, Ignored);
											  },
											  Refusal);
	if (!bDone)
	{
		Message = Refusal;
		RefreshView();
		return;
	}
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: %s added as a weapon entry"), *Members[MemberIndex].Name.ToString(), *GetNameSafe(Draft.MainHand.Weapon));
	CloseWindow();
}

void ULoadoutV2ScreenWidget::CloseWindow()
{
	bAdding = false;
	Choices.Reset();
	Picked = INDEX_NONE;
	bComparing = false;
	Selected = 0;
	RefreshData();
}

bool ULoadoutV2ScreenWidget::NativeOnHandleBackAction()
{
	if (!bAdding)
	{
		return Super::NativeOnHandleBackAction();
	}
	Message = FText::GetEmpty();
	CloseWindow();
	return true;
}

FReply ULoadoutV2ScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (IsUpKey(Key) || IsDownKey(Key))
	{
		MoveSelection(IsDownKey(Key) ? 1 : -1);
		return FReply::Handled();
	}
	if (IsSubTabPrevKey(Key) || IsSubTabNextKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			MoveMember(IsSubTabNextKey(Key) ? 1 : -1);
		}
		return FReply::Handled();
	}
	if (IsConfirmKey(Key) || IsToggleKey(Key) || IsSecondActionKey(Key))
	{
		if (!InKeyEvent.IsRepeat())
		{
			IsConfirmKey(Key) ? PressA() : IsToggleKey(Key) ? PressCompare() : PressConfirm();
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
static FAutoConsoleCommandWithWorld GLoadoutV2OpenCommand(
	TEXT("wor.UI.LoadoutV2"),
	TEXT("B-476b: open the v2 loadout screen's Weapons tab (beside today's wor.UI.Loadout): A on Add weapon, then A Pick, X Compare, Y Confirm."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!ULoadoutV2ScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[LoadoutV2] wor.UI.LoadoutV2: not opened (no hub controller with a root layout)"));
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
