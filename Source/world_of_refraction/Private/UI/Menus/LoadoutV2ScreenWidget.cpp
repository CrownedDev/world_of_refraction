// LoadoutV2ScreenWidget.cpp

#include "UI/Menus/LoadoutV2ScreenWidget.h"

#include "Algo/IndexOf.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Engine/World.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/LoadoutConstants.h"
#include "Misc/ScopeExit.h"
#include "Player/WoRHubController.h"
#include "UI/Menus/LoadoutApparelModel.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "UI/WoRRootLayout.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRLoadoutV2Screen"

bool ULoadoutV2ScreenWidget::bSimpleView = false;

namespace LoadoutV2ScreenStyle
{
	// This screen's own layout (mockups menus/loadout/v2, 1280x720, at 1080p); the shared look is WoRMenuStyle.
	using namespace WoRMenuStyle;
	constexpr float ListWidth = 840.0f;
	constexpr float BoxWidth = 860.0f;
	constexpr float ColumnGap = 48.0f;
	constexpr float RowPadding = 18.0f;
	constexpr float RowGap = 10.0f;
	constexpr float NameColumnWidth = 460.0f;
	constexpr float BoxPadding = 24.0f;
	/** Spaces per depth of an effect-tree line in the details box. */
	constexpr int32 IndentSpaces = 3;

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

	FText WeaponName(const FWeaponInventoryEntry &Weapon)
	{
		return Weapon.Weapon ? FText::FromString(Weapon.Weapon->GetDisplayName()) : FText::GetEmpty();
	}

	FText AttachmentText(const FRuntimeAttachedItem &Item)
	{
		if (Item.IsEmpty())
		{
			return LOCTEXT("SlotEmpty", "—");
		}
		if (Item.IsEvolution())
		{
			return Item.Evolution.Item ? FText::FromString(Item.Evolution.Item->ItemName) : LOCTEXT("SocketEvolution", "Evolution");
		}
		return Item.IsAugmentStone() ? LOCTEXT("SocketStone", "Stone") : LOCTEXT("SocketCrystal", "Crystal");
	}

	/** What A again on a filled socket takes off (today's rule, WoRLoadoutScreen::GetTakeOffSocketWarning, said for A):
	 *  an evolution goes back with wear; a gem and the stone come off together and are used up (CL-28b). Empty with
	 *  OutRefusal when the socket can't be emptied. */
	FText SocketRemoveWarning(const FLoadoutScreenRow &Row, int32 Socket, FText &OutRefusal)
	{
		if (WoRLoadoutScreen::GetTakeOffSocketWarning(Row, Socket, OutRefusal).IsEmpty())
		{
			return FText::GetEmpty();
		}
		const FLoadoutScreenSocket &Crystal = Row.Sockets[LoadoutScreenSockets::CRYSTAL];
		return Crystal.bEvolution ? FText::Format(LOCTEXT("WarnEvolutionOff", "A again: {0} goes back to your evolutions with some wear (it breaks at none)."), Crystal.Name)
								  : LOCTEXT("WarnCrystalOff", "A again: what's in both sockets comes off and is destroyed.");
	}

	/** B-440f: an apparel piece's short line: its tier and what its socket holds ("B · Garnet", "C · empty"). */
	FText ApparelLine(const FLoadoutScreenRow &Row)
	{
		const FLoadoutScreenSocket *Socket = Row.Sockets.IsValidIndex(LoadoutScreenSockets::CRYSTAL) ? &Row.Sockets[LoadoutScreenSockets::CRYSTAL] : nullptr;
		return FText::Format(LOCTEXT("ApparelLine", "{0} · {1}"), Row.Tier, Socket && !Socket->bEmpty ? Socket->Name : LOCTEXT("SocketEmpty", "empty"));
	}

	/** The option lines Y opens, in order. */
	const ELoadoutV2Option OptionOrder[] = {ELoadoutV2Option::SetActive, ELoadoutV2Option::SavedLoadouts, ELoadoutV2Option::RemoveAllGear,
											ELoadoutV2Option::Help, ELoadoutV2Option::CosmeticsShortcuts};
	constexpr int32 OptionCount = UE_ARRAY_COUNT(OptionOrder);
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
		TabsText = MakeText(WidgetTree, RowFontSize, CyanColor, true); // filled in RefreshView (B-440f: the tab in use)
		// TODO: B-476d: Skills on LB / RB; Cosmetics stays greyed (later).
		AddToRow(Header, TabsText)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CellsText = MakeText(WidgetTree, RowFontSize, TextColor, true);
		AddToRow(Header, CellsText);
		AddToColumn(Main, Header);

		TitleText = MakeText(WidgetTree, LineFontSize, DimColor, true);
		AddToColumn(Main, TitleText, FMargin(0.0f, SectionGap, 0.0f, LineGap));
		// B-440e: the saved loadout's new name; shown only while renaming.
		NameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("LoadoutNameBox"));
		NameBox->SetHintText(LOCTEXT("NameHint", "Loadout name"));
		NameBox->OnTextCommitted.AddDynamic(this, &ULoadoutV2ScreenWidget::HandleNameCommitted);
		AddToColumn(Main, NameBox, FMargin(0.0f, 0.0f, 0.0f, LineGap)); // no wrapper: collapsed, it leaves no gap
		SetShown(NameBox, false);

		// B-476c (UIScreens B-479c): the list on the left, the box on the right.
		UHorizontalBox *Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		ListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddToRow(Columns, MakeSized(WidgetTree, ListWidth, 0.0f, ListBox), FMargin(0.0f, 0.0f, ColumnGap, 0.0f), VAlign_Top);
		UBorder *Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Box->SetBrushColor(PanelColor);
		Box->SetPadding(FMargin(BoxPadding));
		UVerticalBox *BoxColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Box->SetContent(BoxColumn);
		BoxTitleText = MakeText(WidgetTree, RowFontSize, TextColor, true);
		AddToColumn(BoxColumn, BoxTitleText, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		BoxLines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddToColumn(BoxColumn, BoxLines);
		AddToRow(Columns, MakeSized(WidgetTree, BoxWidth, 0.0f, Box), FMargin(0.0f), VAlign_Top);
		AddToColumn(Main, Columns)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

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
	Mode = ELoadoutV2Mode::List;
	bEditing = false;
	Draft = FLoadoutV2Draft();
	Kinds.Reset();
	Slots.Reset();
	Items.Reset();
	PickedSocket = INDEX_NONE;
	bComparing = false;
	bRemoveArmed = false;
	bRemoveAllArmed = false;
	bRenaming = false;
	WoRMenuStyle::SetShown(NameBox, false);
	RefreshData();
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] Opened: %s"), *WoRLoadoutV2::GetWeaponsPageString(Page));
}

UWidget *ULoadoutV2ScreenWidget::NativeGetDesiredFocusTarget() const
{
	return const_cast<ULoadoutV2ScreenWidget *>(this); // the screen itself takes the keys (NativeOnKeyDown)
}

bool ULoadoutV2ScreenWidget::FindActive(ULoadoutComponent *&OutLoadout, UInventoryComponent *&OutInventory, const FCombatLoadout *&OutActive) const
{
	OutLoadout = nullptr;
	OutInventory = nullptr;
	OutActive = nullptr;
	if (!Members.IsValidIndex(MemberIndex))
	{
		return false;
	}
	OutLoadout = WoRLoadoutScreen::FindMemberLoadout(GetWorld(), Members[MemberIndex], OutInventory);
	if (!OutLoadout || !OutInventory)
	{
		return false;
	}
	OutActive = &OutInventory->SavedLoadouts[OutInventory->ActiveLoadoutIndex]; // FindMemberLoadout checked the index
	return true;
}

void ULoadoutV2ScreenWidget::RefreshData()
{
	Members = WoRLoadoutScreen::BuildMembers(GetWorld());
	MemberIndex = Members.Num() > 0 ? FMath::Clamp(MemberIndex, 0, Members.Num() - 1) : 0;
	Page = FLoadoutV2WeaponsPage();
	LoadoutName = FText::GetEmpty();
	ApparelRows.Reset();
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (FindActive(Loadout, Inventory, Active))
	{
		LoadoutName = FText::FromString(Active->LoadoutName);
		Page = WoRLoadoutV2::BuildWeaponsPage(*Active, Members[MemberIndex].Class);
		// B-440f: the apparel places as today's gear page lists them (worn first, then the empty ones).
		ApparelRows = WoRLoadoutScreen::BuildGearPage(*Active, Members[MemberIndex].Class).Rows.FilterByPredicate(&WoRLoadoutApparel::IsApparelRow);
		if (Mode == ELoadoutV2Mode::WindowKind)
		{
			Kinds = WoRLoadoutV2::BuildAddKinds(*Active, Members[MemberIndex].Class);
		}
		else if (Mode == ELoadoutV2Mode::WindowItems || Mode == ELoadoutV2Mode::WindowAsk)
		{
			BuildItems();
		}
		else if (Mode == ELoadoutV2Mode::Saved)
		{
			Saved = WoRLoadoutSaved::BuildList(*Inventory);
		}
		else if (Mode == ELoadoutV2Mode::ApparelItems)
		{
			BuildApparelItems();
		}
	}
	else
	{
		Kinds.Reset();
		Items.Reset();
		Saved = FLoadoutSavedList();
		bRenaming = false; // the member left: no name to keep, and the keys go back to the screen
		WoRMenuStyle::SetShown(NameBox, false);
		if (IsAdding() || Mode == ELoadoutV2Mode::Saved || Mode == ELoadoutV2Mode::ApparelItems)
		{
			Mode = ELoadoutV2Mode::List; // the member left: nothing to edit
		}
		if (Message.IsEmpty())
		{
			Message = Members.Num() == 0 ? LOCTEXT("NoParty", "There is no party to show.") : LOCTEXT("MemberNotHere", "This character isn't here.");
		}
	}
	Selected = GetLineCount() > 0 ? FMath::Clamp(Selected, 0, GetLineCount() - 1) : 0;
	if (!Items.IsValidIndex(PickedSocket))
	{
		PickedSocket = INDEX_NONE;
	}
	RefreshView();
}

const FLoadoutV2Entry *ULoadoutV2ScreenWidget::GetFocusedEntry() const
{
	const int32 Line = Mode == ELoadoutV2Mode::List ? Selected : ListSelected;
	return Tab == ELoadoutV2Tab::Weapons && Page.Entries.IsValidIndex(Line) ? &Page.Entries[Line] : nullptr; // the Apparel tab lists no entry
}

int32 ULoadoutV2ScreenWidget::GetWornApparelCount() const
{
	return ApparelRows.FilterByPredicate([](const FLoadoutScreenRow &Row) { return Row.Kind == ELoadoutScreenRowKind::Apparel; }).Num();
}

const FLoadoutScreenRow *ULoadoutV2ScreenWidget::GetFocusedApparelRow() const
{
	if (Tab != ELoadoutV2Tab::Apparel)
	{
		return nullptr;
	}
	const int32 Line = Mode == ELoadoutV2Mode::List ? Selected : ListSelected;
	const int32 Worn = GetWornApparelCount();
	// The worn pieces come first; the Add apparel frame after them stands for the first empty place.
	return ApparelRows.IsValidIndex(FMath::Min(Line, Worn)) ? &ApparelRows[FMath::Min(Line, Worn)] : nullptr;
}

void ULoadoutV2ScreenWidget::RefreshDetails()
{
	Details.Reset();
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	const FLoadoutV2Entry *Entry = GetFocusedEntry();
	if (Entry && FindActive(Loadout, Inventory, Active))
	{
		Details = WoRLoadoutV2::BuildEntryDetails(*Active, *Entry, bSimpleView);
	}
}

// ==================== LINES ====================

int32 ULoadoutV2ScreenWidget::GetLineCount() const
{
	switch (Mode)
	{
	case ELoadoutV2Mode::Options:
		return LoadoutV2ScreenStyle::OptionCount;
	case ELoadoutV2Mode::Saved:
		return Saved.Rows.Num();
	case ELoadoutV2Mode::WindowKind:
		return Kinds.Num();
	case ELoadoutV2Mode::WindowSlots:
		return Slots.Num();
	case ELoadoutV2Mode::WindowItems:
	case ELoadoutV2Mode::WindowAsk:
	case ELoadoutV2Mode::ApparelItems:
		return Items.Num();
	case ELoadoutV2Mode::List:
	default:
		if (Tab == ELoadoutV2Tab::Apparel)
		{
			const int32 Worn = GetWornApparelCount();
			return Worn + (Worn < ApparelRows.Num() ? 1 : 0); // the Add apparel frame last, while a place is free
		}
		return Page.Entries.Num() + (Page.bCanAdd ? 1 : 0); // the Add weapon frame last
	}
}

FText ULoadoutV2ScreenWidget::GetLineName(int32 Line) const
{
	using namespace LoadoutV2ScreenStyle;
	switch (Mode)
	{
	case ELoadoutV2Mode::Options:
		switch (OptionOrder[Line])
		{
		case ELoadoutV2Option::SetActive:
			return LOCTEXT("OptionSetActive", "Set active");
		case ELoadoutV2Option::SavedLoadouts:
			return LOCTEXT("OptionSaved", "Saved loadouts");
		case ELoadoutV2Option::RemoveAllGear:
			return LOCTEXT("OptionRemoveAll", "Remove all gear");
		case ELoadoutV2Option::Help:
			return LOCTEXT("OptionHelp", "Help");
		case ELoadoutV2Option::CosmeticsShortcuts:
		default:
			return LOCTEXT("OptionCosmetics", "Cosmetics shortcuts");
		}
	case ELoadoutV2Mode::Saved:
		return Saved.Rows.IsValidIndex(Line) ? Saved.Rows[Line].Name : FText::GetEmpty();
	case ELoadoutV2Mode::WindowKind:
		return Kinds.IsValidIndex(Line) ? Kinds[Line].Name : FText::GetEmpty();
	case ELoadoutV2Mode::WindowSlots:
		return Slots.IsValidIndex(Line) ? GetSlotName(Slots[Line]) : FText::GetEmpty();
	case ELoadoutV2Mode::WindowItems:
	case ELoadoutV2Mode::WindowAsk:
	case ELoadoutV2Mode::ApparelItems:
		if (!Items.IsValidIndex(Line))
		{
			return FText::GetEmpty();
		}
		if (Items[Line].bEquipped)
		{
			return FText::Format(LOCTEXT("EquippedName", "{0}  EQUIPPED"), Items[Line].Name);
		}
		return Line == PickedSocket ? FText::Format(LOCTEXT("PickedName", "{0}  PICKED"), Items[Line].Name) : Items[Line].Name;
	case ELoadoutV2Mode::List:
	default:
		if (Tab == ELoadoutV2Tab::Apparel)
		{
			return Line < GetWornApparelCount() && ApparelRows.IsValidIndex(Line) ? ApparelRows[Line].Name : LOCTEXT("AddApparel", "+ Add apparel");
		}
		if (Page.Entries.IsValidIndex(Line))
		{
			const FLoadoutV2Entry &Entry = Page.Entries[Line];
			return Entry.bActive ? FText::Format(LOCTEXT("ActiveName", "{0}  ACTIVE"), Entry.Name) : Entry.Name;
		}
		return LOCTEXT("AddWeapon", "+ Add weapon");
	}
}

FText ULoadoutV2ScreenWidget::GetLineDetail(int32 Line) const
{
	using namespace LoadoutV2ScreenStyle;
	switch (Mode)
	{
	case ELoadoutV2Mode::Options:
	{
		const FLoadoutV2Entry *Entry = GetFocusedEntry();
		switch (OptionOrder[Line])
		{
		case ELoadoutV2Option::SetActive:
			return Entry && Entry->Kind == ELoadoutV2EntryKind::Weapon && !Entry->bActive
					   ? FText::Format(LOCTEXT("OptionSetActiveLine", "{0} becomes the active weapon (L3)"), Entry->Name)
					   : LOCTEXT("OptionSetActiveOff", "Focus a weapon entry that isn't active");
		case ELoadoutV2Option::SavedLoadouts:
			return LOCTEXT("OptionSavedList", "Use another, make a new one, duplicate or rename"); // B-440e
		case ELoadoutV2Option::RemoveAllGear:
			return bRemoveAllArmed ? LOCTEXT("OptionRemoveAllArmed", "A again: every entry but the first comes out")
								   : LOCTEXT("OptionRemoveAllLine", "Asks first");
		case ELoadoutV2Option::Help:
			return LOCTEXT("OptionHelpLine", "What each part of this screen does");
		case ELoadoutV2Option::CosmeticsShortcuts:
		default:
			return LOCTEXT("OptionCosmeticsLine", "Later");
		}
	}
	case ELoadoutV2Mode::Saved:
		return Saved.Rows.IsValidIndex(Line) ? Saved.Rows[Line].Note : FText::GetEmpty();
	case ELoadoutV2Mode::WindowKind:
		return Kinds.IsValidIndex(Line) ? Kinds[Line].Refusal : FText::GetEmpty();
	case ELoadoutV2Mode::WindowSlots:
		return Slots.IsValidIndex(Line) ? GetSlotValue(Slots[Line]) : FText::GetEmpty();
	case ELoadoutV2Mode::WindowItems:
	case ELoadoutV2Mode::WindowAsk:
	case ELoadoutV2Mode::ApparelItems:
		if (!Items.IsValidIndex(Line))
		{
			return FText::GetEmpty();
		}
		return Items[Line].Refusal.IsEmpty() ? Items[Line].Line : Items[Line].Refusal;
	case ELoadoutV2Mode::List:
	default:
		if (Tab == ELoadoutV2Tab::Apparel)
		{
			const int32 Worn = GetWornApparelCount();
			return Line < Worn && ApparelRows.IsValidIndex(Line)
					   ? ApparelLine(ApparelRows[Line])
					   : FText::Format(LOCTEXT("AddApparelLine", "{0}/{1} left"), FText::AsNumber(ApparelRows.Num() - Worn), FText::AsNumber(ApparelRows.Num()));
		}
		if (Page.Entries.IsValidIndex(Line))
		{
			TArray<FText> Sockets;
			for (const ELoadoutV2SocketIcon Icon : Page.Entries[Line].Sockets)
			{
				Sockets.Add(SocketWord(Icon));
			}
			return FText::Join(LOCTEXT("SocketGap", " · "), Sockets);
		}
		return Page.AddFrameLine;
	}
}

bool ULoadoutV2ScreenWidget::IsLineRefused(int32 Line) const
{
	using namespace LoadoutV2ScreenStyle;
	switch (Mode)
	{
	case ELoadoutV2Mode::Options:
	{
		const ELoadoutV2Option Option = OptionOrder[Line];
		const FLoadoutV2Entry *Entry = GetFocusedEntry();
		const bool bSetActive = Option == ELoadoutV2Option::SetActive && Entry && Entry->Kind == ELoadoutV2EntryKind::Weapon && !Entry->bActive;
		return Option == ELoadoutV2Option::CosmeticsShortcuts || (Option == ELoadoutV2Option::SetActive && !bSetActive);
	}
	case ELoadoutV2Mode::WindowKind:
		return Kinds.IsValidIndex(Line) && !Kinds[Line].Refusal.IsEmpty();
	case ELoadoutV2Mode::WindowItems:
	case ELoadoutV2Mode::WindowAsk:
	case ELoadoutV2Mode::ApparelItems:
		return Items.IsValidIndex(Line) && !Items[Line].Refusal.IsEmpty();
	default:
		return false;
	}
}

FText ULoadoutV2ScreenWidget::GetTitle() const
{
	if (Mode == ELoadoutV2Mode::List)
	{
		return FText::GetEmpty();
	}
	if (Mode == ELoadoutV2Mode::Options)
	{
		return LOCTEXT("TitleOptions", "Options");
	}
	if (Mode == ELoadoutV2Mode::Saved)
	{
		return FText::Format(LOCTEXT("TitleSaved", "Saved loadouts ({0} of {1} of your own)"), FText::AsNumber(Saved.CustomCount), FText::AsNumber(Saved.MaxCustom));
	}
	if (Mode == ELoadoutV2Mode::ApparelItems)
	{
		const FLoadoutScreenRow *Row = GetFocusedApparelRow();
		return Row && Row->Kind == ELoadoutScreenRowKind::Apparel ? FText::Format(LOCTEXT("TitleEdit", "Edit · {0}"), Row->Name) : LOCTEXT("TitleAddApparel", "Add apparel");
	}
	const FText Window = bEditing ? FText::Format(LOCTEXT("TitleEdit", "Edit · {0}"), EditEntry.Name) : LOCTEXT("TitleAdd", "Add gear");
	if (Mode == ELoadoutV2Mode::WindowItems || Mode == ELoadoutV2Mode::WindowAsk)
	{
		return Slots.IsValidIndex(SlotIndex) ? FText::Format(LOCTEXT("TitleSlot", "{0} · {1}"), Window, GetSlotName(Slots[SlotIndex])) : Window;
	}
	return Window;
}

FText ULoadoutV2ScreenWidget::GetHints() const
{
	switch (Mode)
	{
	case ELoadoutV2Mode::Options:
		return LOCTEXT("HintsOptions", "A  Select        B / Esc  Close");
	case ELoadoutV2Mode::Saved:
		if (bRenaming)
		{
			return LOCTEXT("HintsRenaming", "Enter  Keep the name        Esc  Cancel");
		}
		return Saved.Rows.IsValidIndex(Selected) && Saved.Rows[Selected].IsNewLine()
				   ? LOCTEXT("HintsSavedNew", "A  New loadout        B / Esc  Back")
				   : LOCTEXT("HintsSaved", "A  Use        X  Duplicate        Y  Rename        B / Esc  Back");
	case ELoadoutV2Mode::WindowKind:
		return LOCTEXT("HintsKind", "A  Pick        B / Esc  Cancel");
	case ELoadoutV2Mode::WindowSlots:
		return LOCTEXT("HintsSlots", "A  Open        Y  Confirm        B / Esc  Cancel");
	case ELoadoutV2Mode::WindowItems:
		return Items.IsValidIndex(Selected) && Items[Selected].bEquipped
				   ? LOCTEXT("HintsItemsEquipped", "A  Remove        X  Compare        Y  Confirm        B / Esc  Back")
				   : LOCTEXT("HintsItems", "A  Pick        X  Compare        Y  Confirm        B / Esc  Back");
	case ELoadoutV2Mode::WindowAsk:
		return LOCTEXT("HintsAsk", "A  Attach        B / Esc  Not now");
	case ELoadoutV2Mode::ApparelItems:
		return Items.IsValidIndex(Selected) && Items[Selected].bEquipped ? LOCTEXT("HintsApparelEquipped", "A  Remove        B / Esc  Back")
																		 : LOCTEXT("HintsApparelItems", "A  Pick        B / Esc  Back");
	case ELoadoutV2Mode::List:
	default:
	{
		if (IsOnAddFrame() || (Tab == ELoadoutV2Tab::Apparel && Selected >= GetWornApparelCount()))
		{
			return LOCTEXT("HintsFrame", "A  Equip        Y  Options        LT / RT  Character        B / Esc  Back");
		}
		const FLoadoutV2Entry *Entry = GetFocusedEntry();
		const bool bCanSetActive = Entry && Entry->Kind == ELoadoutV2EntryKind::Weapon && !Entry->bActive && Page.Entries.ContainsByPredicate(
																														[](const FLoadoutV2Entry &Each) { return Each.bActive; });
		const FText View = bSimpleView ? LOCTEXT("ViewFull", "Full") : LOCTEXT("ViewSimple", "Simple");
		if (Tab == ELoadoutV2Tab::Apparel)
		{
			return FText::Format(LOCTEXT("HintsApparelList", "A  Change / Remove        X  {0}        Y  Options        B / Esc  Back"), View);
		}
		return bCanSetActive ? FText::Format(LOCTEXT("HintsListActive", "A  Edit        X  {0}        L3 / 6  Set active        Y  Options        B / Esc  Back"), View)
							 : FText::Format(LOCTEXT("HintsList", "A  Edit        X  {0}        Y  Options        B / Esc  Back"), View);
	}
	}
}

// ==================== VIEW ====================

void ULoadoutV2ScreenWidget::RefreshView()
{
	using namespace LoadoutV2ScreenStyle;
	RefreshDetails();
	if (!ListBox)
	{
		return; // headless: no tree
	}
	MemberText->SetText(Members.IsValidIndex(MemberIndex) ? Members[MemberIndex].Name : FText::GetEmpty());
	TabsText->SetText(Tab == ELoadoutV2Tab::Apparel ? LOCTEXT("TabsApparel", "LB   Weapons  ·  [ Apparel ]   RB     (Skills · Cosmetics: later)")
													: LOCTEXT("TabsWeapons", "LB   [ Weapons ]  ·  Apparel   RB     (Skills · Cosmetics: later)"));
	CellsText->SetText(LoadoutName.IsEmpty() ? Page.CellsLine : FText::Format(LOCTEXT("NameAndCells", "{0}     {1}"), LoadoutName, Page.CellsLine));
	TitleText->SetText(GetTitle());
	SetShown(TitleText, Mode != ELoadoutV2Mode::List);
	const int32 Count = GetLineCount();
	while (LineRows.Num() < Count)
	{
		// Rows are made the first time a list needs them and refilled after, never rebuilt.
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
		const bool bRefused = IsLineRefused(Index);
		LineRows[Index]->SetBrushColor(Index == Selected ? FocusColor : PlateColor);
		LineNames[Index]->SetText(GetLineName(Index));
		LineNames[Index]->SetColorAndOpacity(FSlateColor(bRefused ? OffColor : TextColor)); // refused: stays, greyed
		LineDetails[Index]->SetText(GetLineDetail(Index));
		LineDetails[Index]->SetColorAndOpacity(FSlateColor(bRefused && (IsAdding() || Mode == ELoadoutV2Mode::ApparelItems) ? WarnColor : DimColor));
	}

	// The box: the focused entry's details, or in the window the entry being built.
	TArray<TPair<FText, bool>> BoxText; // a line, and whether it is lit
	FText BoxTitle;
	if (IsAdding())
	{
		BoxTitle = bEditing ? EditEntry.Name : LOCTEXT("BoxNewEntry", "New entry");
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			const bool bLit = Mode != ELoadoutV2Mode::WindowSlots && Index == SlotIndex;
			BoxText.Add({FText::Format(LOCTEXT("BoxSlotLine", "{0}:  {1}"), GetSlotName(Slots[Index]), GetSlotValue(Slots[Index])), bLit});
		}
	}
	else if (Tab == ELoadoutV2Tab::Apparel)
	{
		// B-440f: the focused piece (in the list of pieces: the lit one), or the Add apparel frame.
		const FLoadoutScreenRow *Place = GetFocusedApparelRow();
		const FLoadoutScreenRow *Row = Mode == ELoadoutV2Mode::ApparelItems ? (Items.IsValidIndex(Selected) ? &Items[Selected].Socket.Row : nullptr) : Place;
		if (Row && Row->Kind == ELoadoutScreenRowKind::Apparel)
		{
			BoxTitle = Row->Name;
			if (!bSimpleView)
			{
				BoxText.Add({FText::Format(LOCTEXT("BoxApparelKind", "{0}  {1}"), Row->KindLine, Row->Tier), true});
				for (const FLoadoutScreenSocket &Socket : Row->Sockets)
				{
					BoxText.Add({FText::Format(LOCTEXT("BoxStat", "{0}   {1}"), Socket.Label, Socket.bEmpty ? LOCTEXT("SocketEmpty", "empty") : Socket.Name), false});
				}
			}
			// TODO: B-476d: the piece's HP / Defence / Amp lines (tier-scaled in ULoadoutComponent) and its effects' text.
			for (const FLoadoutScreenDetail &Detail : Row->Details)
			{
				BoxText.Add({FText::Format(LOCTEXT("BoxStat", "{0}   {1}"), Detail.Label, Detail.Value), false});
			}
			if (!bSimpleView)
			{
				BoxText.Add({Row->CostLine, false});
			}
		}
		else if (Row)
		{
			BoxTitle = LOCTEXT("BoxAddApparel", "Add apparel");
			BoxText.Add({LOCTEXT("BoxAddApparelLine", "A piece you own. Apparel sits outside the cells."), false});
		}
	}
	else
	{
		const FLoadoutV2Entry *Entry = GetFocusedEntry();
		BoxTitle = Entry ? Entry->Name : (IsOnAddFrame() ? LOCTEXT("BoxAdd", "Add weapon") : FText::GetEmpty());
		if (IsOnAddFrame())
		{
			BoxText.Add({FText::Format(LOCTEXT("BoxAddLine", "A new entry: a weapon, an adapter or the evolution. {0}"), Page.AddFrameLine), false});
		}
		for (const FLoadoutV2DetailLine &Line : Details)
		{
			FText Shown;
			switch (Line.Kind)
			{
			case ELoadoutV2DetailKind::Piece:
				Shown = FText::Format(LOCTEXT("BoxPiece", "{0}  {1}"), Line.Label, Line.Value);
				break;
			case ELoadoutV2DetailKind::Effect:
				Shown = FText::Format(LOCTEXT("BoxEffect", "{0}{1}"), FText::FromString(FString::ChrN(Line.Depth * IndentSpaces, TEXT(' '))), Line.Value);
				break;
			case ELoadoutV2DetailKind::Stat:
			case ELoadoutV2DetailKind::Socket:
			case ELoadoutV2DetailKind::SkillSlots:
			default:
				Shown = FText::Format(LOCTEXT("BoxStat", "{0}   {1}"), Line.Label, Line.Value);
				break;
			}
			BoxText.Add({Shown, Line.Kind == ELoadoutV2DetailKind::Piece});
		}
	}
	BoxTitleText->SetText(BoxTitle);
	while (BoxTexts.Num() < BoxText.Num())
	{
		UTextBlock *Text = MakeWrappedText(WidgetTree, LineFontSize, TextColor);
		AddToColumn(BoxLines, Text, FMargin(0.0f, 0.0f, 0.0f, LineGap));
		BoxTexts.Add(Text);
	}
	for (int32 Index = 0; Index < BoxTexts.Num(); ++Index)
	{
		const bool bShown = BoxText.IsValidIndex(Index);
		SetShown(BoxTexts[Index], bShown);
		if (bShown)
		{
			BoxTexts[Index]->SetText(BoxText[Index].Key);
			BoxTexts[Index]->SetColorAndOpacity(FSlateColor(BoxText[Index].Value ? CyanColor : TextColor));
		}
	}

	FText Compare;
	if (Mode == ELoadoutV2Mode::WindowItems && bComparing && Items.IsValidIndex(Selected) && Slots.IsValidIndex(SlotIndex))
	{
		const ELoadoutV2Slot GearSlot = Slots[SlotIndex];
		const FWeaponInventoryEntry *Equipped = nullptr;
		ULoadoutComponent *Loadout = nullptr;
		UInventoryComponent *Inventory = nullptr;
		const FCombatLoadout *Active = nullptr;
		FLoadoutV2Draft Current;
		if (bEditing && FindActive(Loadout, Inventory, Active))
		{
			Current = WoRLoadoutV2::DraftFromEntry(*Active, EditEntry);
			Equipped = GearSlot == ELoadoutV2Slot::MainHand ? &Current.MainHand : GearSlot == ELoadoutV2Slot::OffHand ? &Current.OffHand : nullptr;
		}
		else if (!bEditing && GearSlot == ELoadoutV2Slot::MainHand && FindActive(Loadout, Inventory, Active))
		{
			// A new entry: against the weapon in hand (a Generic's second weapon when it is the ACTIVE entry).
			const bool bSecondInHand = Page.Entries.ContainsByPredicate([](const FLoadoutV2Entry &Entry)
																		{ return Entry.bActive && Entry.Place == ELoadoutV2Place::Secondary; });
			Equipped = bSecondInHand ? &Active->SecondaryWeapon.WeaponEntry : &Active->PrimaryWeapon.WeaponEntry;
		}
		if ((GearSlot == ELoadoutV2Slot::MainHand || GearSlot == ELoadoutV2Slot::OffHand) && Equipped && Equipped->IsValid() && Items[Selected].Weapon.IsValid())
		{
			const FLoadoutV2Compare Result = WoRLoadoutV2::BuildCompare(*Equipped, Items[Selected].Weapon);
			Compare = FText::Format(LOCTEXT("CompareLine", "Equipped {0} / Selected {1} · Power {2} / {3} · Cells change {4} · Built-in gained: {5} · lost: {6}"),
									WeaponName(*Equipped), Items[Selected].Name, FText::AsNumber(Result.EquippedPower), FText::AsNumber(Result.SelectedPower),
									FText::AsNumber(Result.CellsChange), JoinNames(Result.BuiltInsGained), JoinNames(Result.BuiltInsLost));
		}
		else
		{
			// TODO: B-476d: Compare for adapters, evolutions and socket pieces (UIScreens: power, element, cells, passive).
			Compare = LOCTEXT("CompareNothing", "Compare shows two weapons: nothing equipped here to set beside it.");
		}
	}
	CompareText->SetText(Compare);
	SetShown(CompareText, !Compare.IsEmpty());
	MessageText->SetText(Message);
	SetShown(MessageText, !Message.IsEmpty());
	HintsText->SetText(GetHints());
}

// ==================== KEYS ====================

void ULoadoutV2ScreenWidget::MoveSelection(int32 Delta)
{
	const int32 Count = GetLineCount();
	if (Count > 0 && Mode != ELoadoutV2Mode::WindowAsk)
	{
		Selected = ((Selected + Delta) % Count + Count) % Count;
		Message = FText::GetEmpty();
		const bool bWasArmed = bRemoveArmed;
		bRemoveArmed = false;
		bRemoveAllArmed = false;
		if (bWasArmed && Mode == ELoadoutV2Mode::WindowItems)
		{
			RefreshData(); // the EQUIPPED line drops its "A again" warning
			return;
		}
		RefreshView();
	}
}

void ULoadoutV2ScreenWidget::MoveMember(int32 Delta)
{
	if (Mode != ELoadoutV2Mode::List || Members.Num() < 2)
	{
		return;
	}
	MemberIndex = ((MemberIndex + Delta) % Members.Num() + Members.Num()) % Members.Num();
	Selected = 0;
	Message = FText::GetEmpty();
	RefreshData();
}

void ULoadoutV2ScreenWidget::SwitchTab()
{
	if (Mode != ELoadoutV2Mode::List)
	{
		return;
	}
	Tab = Tab == ELoadoutV2Tab::Weapons ? ELoadoutV2Tab::Apparel : ELoadoutV2Tab::Weapons; // two tabs: either key turns
	Selected = 0;
	Message = FText::GetEmpty();
	RefreshData();
}

void ULoadoutV2ScreenWidget::PressA()
{
	switch (Mode)
	{
	case ELoadoutV2Mode::ApparelItems:
		PickApparel();
		return;
	case ELoadoutV2Mode::List:
		if (Tab == ELoadoutV2Tab::Apparel)
		{
			OpenApparelItems();
		}
		else if (IsOnAddFrame())
		{
			OpenWindowForNew();
		}
		else if (const FLoadoutV2Entry *Entry = GetFocusedEntry())
		{
			OpenWindowForEntry(*Entry);
		}
		return;
	case ELoadoutV2Mode::Options:
		SelectOption();
		return;
	case ELoadoutV2Mode::Saved:
		UseSaved();
		return;
	case ELoadoutV2Mode::WindowKind:
		if (!Kinds.IsValidIndex(Selected))
		{
			return;
		}
		if (!Kinds[Selected].Refusal.IsEmpty())
		{
			Message = Kinds[Selected].Refusal;
			RefreshView();
			return;
		}
		StartDraft(Kinds[Selected].Kind);
		return;
	case ELoadoutV2Mode::WindowSlots:
		if (!Slots.IsValidIndex(Selected))
		{
			return;
		}
		SlotIndex = Selected;
		Mode = ELoadoutV2Mode::WindowItems;
		Selected = 0;
		bComparing = false;
		bRemoveArmed = false;
		PickedSocket = INDEX_NONE;
		Message = FText::GetEmpty();
		RefreshData();
		if (Items.IsEmpty())
		{
			Message = LOCTEXT("NothingToPick", "You own nothing that goes here.");
			RefreshView();
		}
		return;
	case ELoadoutV2Mode::WindowItems:
		if (!Items.IsValidIndex(Selected))
		{
			return;
		}
		if (Items[Selected].bEquipped)
		{
			RemoveEquipped();
		}
		else
		{
			PickItem();
		}
		return;
	case ELoadoutV2Mode::WindowAsk:
		ApplyWindowDraft();
		return;
	default:
		return;
	}
}

void ULoadoutV2ScreenWidget::PressCompare()
{
	if (Mode == ELoadoutV2Mode::List)
	{
		bSimpleView = !bSimpleView; // UIScreens B-479c: X switches the view, remembered
		RefreshView();
		return;
	}
	if (Mode == ELoadoutV2Mode::WindowItems)
	{
		bComparing = !bComparing;
		RefreshView();
		return;
	}
	if (Mode == ELoadoutV2Mode::Saved)
	{
		DuplicateSaved();
	}
}

void ULoadoutV2ScreenWidget::PressConfirm()
{
	if (Mode == ELoadoutV2Mode::List)
	{
		if (!Members.IsValidIndex(MemberIndex))
		{
			return;
		}
		ListSelected = Selected;
		Mode = ELoadoutV2Mode::Options;
		Selected = 0;
		bRemoveAllArmed = false;
		Message = FText::GetEmpty();
		RefreshView();
		return;
	}
	if (Mode == ELoadoutV2Mode::WindowSlots || Mode == ELoadoutV2Mode::WindowItems)
	{
		ConfirmWindow();
		return;
	}
	if (Mode == ELoadoutV2Mode::Saved)
	{
		StartRename();
	}
}

void ULoadoutV2ScreenWidget::PressSetActive()
{
	if (Mode != ELoadoutV2Mode::List && Mode != ELoadoutV2Mode::Options)
	{
		return;
	}
	const FLoadoutV2Entry *Entry = GetFocusedEntry();
	if (!Entry || Entry->Kind != ELoadoutV2EntryKind::Weapon || !Members.IsValidIndex(MemberIndex))
	{
		Message = LOCTEXT("SetActiveNotWeapon", "Only a weapon entry can be set active.");
		RefreshView();
		return;
	}
	// Today's screen's rule (B-474, D-207): the weapon in hand is the entry a fight starts with.
	FLoadoutScreenRow Row;
	Row.Kind = ELoadoutScreenRowKind::Weapon;
	Row.Place = Entry->Place == ELoadoutV2Place::Secondary ? ELoadoutScreenPlace::Secondary : ELoadoutScreenPlace::Primary;
	Row.bActive = Entry->bActive || !Page.Entries.ContainsByPredicate([](const FLoadoutV2Entry &Each) { return Each.bActive; });
	const FText Name = Entry->Name;
	Message = WoRLoadoutScreen::SetRowActive(GetWorld(), Members[MemberIndex], Row);
	if (Message.IsEmpty())
	{
		UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: %s set active"), *Members[MemberIndex].Name.ToString(), *Name.ToString());
	}
	if (Mode == ELoadoutV2Mode::Options)
	{
		Mode = ELoadoutV2Mode::List;
		Selected = ListSelected;
	}
	RefreshData();
}

bool ULoadoutV2ScreenWidget::NativeOnHandleBackAction()
{
	Message = FText::GetEmpty();
	if (bRenaming)
	{
		CancelRename(); // B-440e
		return true;
	}
	switch (Mode)
	{
	case ELoadoutV2Mode::Options:
		Mode = ELoadoutV2Mode::List;
		Selected = ListSelected;
		bRemoveAllArmed = false;
		RefreshData();
		return true;
	case ELoadoutV2Mode::ApparelItems:
		Mode = ELoadoutV2Mode::List;
		Selected = ListSelected;
		Items.Reset();
		RefreshData();
		return true;
	case ELoadoutV2Mode::Saved:
		Mode = ELoadoutV2Mode::Options; // one step back, on the Saved loadouts line
		// The cast: a C array's index type isn't int32.
		Selected = FMath::Max(0, static_cast<int32>(Algo::IndexOf(LoadoutV2ScreenStyle::OptionOrder, ELoadoutV2Option::SavedLoadouts)));
		RefreshView();
		return true;
	case ELoadoutV2Mode::WindowAsk:
		Mode = ELoadoutV2Mode::WindowItems; // Not now: the pick stays
		RefreshView();
		return true;
	case ELoadoutV2Mode::WindowItems:
		Mode = ELoadoutV2Mode::WindowSlots;
		Selected = SlotIndex;
		Items.Reset();
		PickedSocket = INDEX_NONE;
		bComparing = false;
		bRemoveArmed = false;
		RefreshView();
		return true;
	case ELoadoutV2Mode::WindowKind:
	case ELoadoutV2Mode::WindowSlots:
		CloseWindow();
		return true;
	case ELoadoutV2Mode::List:
	default:
		return Super::NativeOnHandleBackAction();
	}
}

FReply ULoadoutV2ScreenWidget::NativeOnKeyDown(const FGeometry &InGeometry, const FKeyEvent &InKeyEvent)
{
	using namespace WoRMenuStyle;
	const FKey Key = InKeyEvent.GetKey();
	if (bRenaming)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent); // B-440e: typing a name; the box keeps Enter and Esc
	}
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
	if ((IsTabPrevKey(Key) || IsTabNextKey(Key)) && Mode == ELoadoutV2Mode::List) // B-440f; elsewhere the keys pass through as before
	{
		if (!InKeyEvent.IsRepeat())
		{
			SwitchTab();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::V && Mode == ELoadoutV2Mode::List) // the Details key on a keyboard (InputMap): the view, as X
	{
		if (!InKeyEvent.IsRepeat())
		{
			PressCompare();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_LeftThumbstick || Key == EKeys::Six) // B-474 (D-207): the fight's switch key
	{
		if (!InKeyEvent.IsRepeat())
		{
			PressSetActive();
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

// ==================== OPTIONS ====================

void ULoadoutV2ScreenWidget::SelectOption()
{
	using namespace LoadoutV2ScreenStyle;
	if (Selected < 0 || Selected >= OptionCount || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	switch (OptionOrder[Selected])
	{
	case ELoadoutV2Option::SetActive:
		PressSetActive();
		return;
	case ELoadoutV2Option::RemoveAllGear:
	{
		if (!bRemoveAllArmed)
		{
			bRemoveAllArmed = true; // asks first
			RefreshView();
			return;
		}
		bRemoveAllArmed = false;
		ULoadoutComponent *Loadout = nullptr;
		UInventoryComponent *Inventory = nullptr;
		const FCombatLoadout *Active = nullptr;
		if (!FindActive(Loadout, Inventory, Active))
		{
			Message = LOCTEXT("MemberNotHere", "This character isn't here.");
			RefreshView();
			return;
		}
		const ECharacterClass Class = Members[MemberIndex].Class;
		int32 Removed = 0;
		FText Refusal;
		if (!Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [Class, &Removed](FCombatLoadout &Changed) { Removed = WoRLoadoutV2::RemoveAllGear(Changed, Class); },
									Refusal))
		{
			Message = Refusal;
		}
		else
		{
			Message = Removed > 0 ? FText::Format(LOCTEXT("RemovedAll", "{0} entries came out. The first stays: every loadout keeps one."), FText::AsNumber(Removed))
								  : LOCTEXT("RemovedNone", "Nothing to remove: every loadout keeps its first entry.");
			UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: Remove all gear took out %d entries"), *Members[MemberIndex].Name.ToString(), Removed);
		}
		Mode = ELoadoutV2Mode::List;
		Selected = 0;
		RefreshData();
		return;
	}
	case ELoadoutV2Option::Help:
		Message = LOCTEXT("HelpText", "Left: your entries; the box shows the focused one. A edits an entry, or adds one on the frame. X switches the full and "
									  "simple view. L3 puts a weapon entry in hand. Cells: only what sits in the sockets costs (crystal or stone 1, "
									  "evolution 2).");
		RefreshView();
		return;
	case ELoadoutV2Option::SavedLoadouts:
		OpenSaved();
		return;
	case ELoadoutV2Option::CosmeticsShortcuts:
	default:
		Message = LOCTEXT("OptionNotYet", "Not here yet.");
		RefreshView();
		return;
	}
}

// ==================== APPAREL (B-440f) ====================

void ULoadoutV2ScreenWidget::OpenApparelItems()
{
	if (!GetFocusedApparelRow())
	{
		return;
	}
	ListSelected = Selected;
	Mode = ELoadoutV2Mode::ApparelItems;
	Selected = 0;
	PickedSocket = INDEX_NONE;
	Message = FText::GetEmpty();
	RefreshData();
	if (Items.IsEmpty())
	{
		Mode = ELoadoutV2Mode::List;
		Selected = ListSelected;
		Message = LOCTEXT("NoApparelToPick", "You own no apparel that isn't worn already.");
		RefreshView();
	}
}

void ULoadoutV2ScreenWidget::BuildApparelItems()
{
	Items.Reset();
	const FLoadoutScreenRow *Row = GetFocusedApparelRow();
	if (!Row || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	if (Row->Kind == ELoadoutScreenRowKind::Apparel) // the worn piece first: A Remove
	{
		FLoadoutV2WindowItem &Worn = Items.AddDefaulted_GetRef();
		Worn.Name = Row->Name;
		Worn.Line = LoadoutV2ScreenStyle::ApparelLine(*Row);
		Worn.bEquipped = true;
		Worn.Socket.Row = *Row;
	}
	for (const FLoadoutScreenChoice &Choice : WoRLoadoutApparel::BuildApparelChoices(GetWorld(), Members[MemberIndex], *Row))
	{
		FLoadoutV2WindowItem &Item = Items.AddDefaulted_GetRef();
		Item.Name = Choice.Row.Name;
		Item.Line = LoadoutV2ScreenStyle::ApparelLine(Choice.Row);
		Item.Refusal = Choice.Refusal;
		Item.Socket = Choice;
	}
}

void ULoadoutV2ScreenWidget::PickApparel()
{
	const FLoadoutScreenRow *Row = GetFocusedApparelRow();
	if (!Row || !Items.IsValidIndex(Selected) || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	const FLoadoutScreenRow Place = *Row; // copies: RefreshData rebuilds the rows and the list
	const FLoadoutV2WindowItem Item = Items[Selected];
	Message = Item.bEquipped ? WoRLoadoutApparel::TakeOffRow(GetWorld(), Members[MemberIndex], Place)
							 : WoRLoadoutApparel::ApplyApparelChoice(GetWorld(), Members[MemberIndex], Place, Item.Socket);
	if (Message.IsEmpty())
	{
		UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: apparel %s %s"), *Members[MemberIndex].Name.ToString(), *Item.Name.ToString(),
			   Item.bEquipped ? TEXT("taken off") : TEXT("put on"));
		Mode = ELoadoutV2Mode::List;
		Selected = ListSelected; // RefreshData keeps it in range (a taken-off piece's line is gone)
		Items.Reset();
	}
	RefreshData();
}

// ==================== SAVED LOADOUTS (B-440e) ====================

void ULoadoutV2ScreenWidget::OpenSaved()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!FindActive(Loadout, Inventory, Active))
	{
		Message = LOCTEXT("MemberNotHere", "This character isn't here.");
		RefreshView();
		return;
	}
	Mode = ELoadoutV2Mode::Saved;
	Selected = FMath::Max(0, Inventory->ActiveLoadoutIndex); // the one in use lit
	Message = FText::GetEmpty();
	RefreshData();
}

void ULoadoutV2ScreenWidget::UseSaved()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!Saved.Rows.IsValidIndex(Selected) || !FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	const FLoadoutSavedRow Row = Saved.Rows[Selected];
	if (Row.IsNewLine())
	{
		int32 NewIndex = INDEX_NONE;
		Message = WoRLoadoutSaved::CreateNew(*Loadout, *Inventory, NewIndex);
		Selected = NewIndex != INDEX_NONE ? NewIndex : Selected; // the new one lit, ready for Y
		RefreshData();
		return;
	}
	Message = WoRLoadoutSaved::SetActive(*Loadout, *Inventory, Row.LoadoutIndex);
	if (Message.IsEmpty())
	{
		Message = FText::Format(LOCTEXT("SavedNowUsed", "{0} is now in use."), Row.Name);
		Mode = ELoadoutV2Mode::List; // back to the entries, now of that loadout
		Selected = 0;
		ListSelected = 0;
	}
	RefreshData();
}

void ULoadoutV2ScreenWidget::DuplicateSaved()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!Saved.Rows.IsValidIndex(Selected) || Saved.Rows[Selected].IsNewLine() || !FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	int32 NewIndex = INDEX_NONE;
	Message = WoRLoadoutSaved::Duplicate(*Loadout, *Inventory, Saved.Rows[Selected].LoadoutIndex, NewIndex);
	Selected = NewIndex != INDEX_NONE ? NewIndex : Selected;
	RefreshData();
}

void ULoadoutV2ScreenWidget::StartRename()
{
	if (bRenaming || !Saved.Rows.IsValidIndex(Selected))
	{
		return;
	}
	if (Saved.Rows[Selected].IsNewLine())
	{
		Message = LOCTEXT("RenameNewLine", "Make the loadout first (A), then name it.");
		RefreshView();
		return;
	}
	bRenaming = true;
	Message = FText::GetEmpty();
	if (NameBox)
	{
		NameBox->SetText(Saved.Rows[Selected].Name);
		NameBox->SetVisibility(ESlateVisibility::Visible); // not SetShown: the box takes clicks and the keys
		NameBox->SetKeyboardFocus();
	}
	RefreshView();
}

void ULoadoutV2ScreenWidget::CommitRename(const FText &NewName)
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (bRenaming && Saved.Rows.IsValidIndex(Selected) && FindActive(Loadout, Inventory, Active))
	{
		Message = WoRLoadoutSaved::Rename(*Loadout, *Inventory, Saved.Rows[Selected].LoadoutIndex, NewName);
	}
	CancelRename(); // closes the box; the message stays
}

void ULoadoutV2ScreenWidget::CancelRename()
{
	if (!bRenaming)
	{
		return;
	}
	bRenaming = false;
	if (NameBox)
	{
		WoRMenuStyle::SetShown(NameBox, false);
		if (UWidget *Target = GetDesiredFocusTarget())
		{
			Target->SetFocus(); // the keys come back to the screen
		}
	}
	RefreshData();
}

void ULoadoutV2ScreenWidget::HandleNameCommitted(const FText &Text, ETextCommit::Type CommitMethod)
{
	if (!bRenaming)
	{
		return; // hiding the box moves its focus away, which commits again
	}
	CommitMethod == ETextCommit::OnEnter ? CommitRename(Text) : CancelRename();
}

// ==================== THE ADD GEAR WINDOW ====================

void ULoadoutV2ScreenWidget::OpenWindowForNew()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	ListSelected = Selected;
	bEditing = false;
	EditEntry = FLoadoutV2Entry();
	Draft = FLoadoutV2Draft();
	Slots.Reset();
	Items.Reset();
	Mode = ELoadoutV2Mode::WindowKind;
	Selected = 0;
	Message = FText::GetEmpty();
	RefreshData();
	// Start on the first kind that fits (UIScreens: the window asks Weapon, Adapter or Evolution).
	const int32 FirstOpen = Kinds.IndexOfByPredicate([](const FLoadoutV2KindChoice &Kind) { return Kind.Refusal.IsEmpty(); });
	Selected = FirstOpen == INDEX_NONE ? 0 : FirstOpen;
	RefreshView();
}

void ULoadoutV2ScreenWidget::OpenWindowForEntry(const FLoadoutV2Entry &Entry)
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	ListSelected = Selected;
	bEditing = true;
	EditEntry = Entry;
	Draft = WoRLoadoutV2::DraftFromEntry(*Active, Entry);
	StartDraft(Draft.Kind);
}

void ULoadoutV2ScreenWidget::StartDraft(ELoadoutV2DraftKind Kind)
{
	if (!bEditing)
	{
		Draft = FLoadoutV2Draft();
		Draft.Kind = Kind;
	}
	Slots.Reset();
	switch (Kind)
	{
	case ELoadoutV2DraftKind::Weapon:
		Slots = {ELoadoutV2Slot::MainHand, ELoadoutV2Slot::OffHand, ELoadoutV2Slot::Socket1, ELoadoutV2Slot::Socket2};
		break;
	case ELoadoutV2DraftKind::Adapter:
		Slots = {ELoadoutV2Slot::Adapter, ELoadoutV2Slot::Socket1, ELoadoutV2Slot::Socket2};
		break;
	case ELoadoutV2DraftKind::Evolution:
	default:
		Slots = {ELoadoutV2Slot::Evolution};
		break;
	}
	SlotIndex = 0;
	Items.Reset();
	PickedSocket = INDEX_NONE;
	bComparing = false;
	bRemoveArmed = false;
	Mode = ELoadoutV2Mode::WindowSlots;
	Selected = 0;
	Message = FText::GetEmpty();
	RefreshView();
}

FText ULoadoutV2ScreenWidget::GetSlotName(ELoadoutV2Slot GearSlot) const
{
	switch (GearSlot)
	{
	case ELoadoutV2Slot::MainHand:
		return LOCTEXT("SlotMainHand", "Main hand");
	case ELoadoutV2Slot::OffHand:
		return LOCTEXT("SlotOffHand", "Off hand");
	case ELoadoutV2Slot::Adapter:
		return WoRGameNames::GetAdapterWord();
	case ELoadoutV2Slot::Evolution:
		return Members.IsValidIndex(MemberIndex) ? FText::Format(LOCTEXT("SlotEvolution", "{0}'s evolution"), Members[MemberIndex].Name)
												 : LOCTEXT("SlotEvolutionPlain", "Evolution");
	case ELoadoutV2Slot::Socket1:
		return LOCTEXT("SlotSocket1", "Augment socket 1");
	case ELoadoutV2Slot::Socket2:
	default:
		return LOCTEXT("SlotSocket2", "Augment socket 2");
	}
}

FText ULoadoutV2ScreenWidget::GetSlotValue(ELoadoutV2Slot GearSlot) const
{
	using namespace LoadoutV2ScreenStyle;
	switch (GearSlot)
	{
	case ELoadoutV2Slot::MainHand:
		return Draft.MainHand.IsValid() ? WeaponName(Draft.MainHand) : LOCTEXT("SlotPick", "pick one");
	case ELoadoutV2Slot::OffHand:
		return Draft.OffHand.IsValid() ? WeaponName(Draft.OffHand) : LOCTEXT("SlotOffHandEmpty", "empty (a weapon alone)");
	case ELoadoutV2Slot::Adapter:
		return Draft.Adapter.Ring ? FText::FromString(Draft.Adapter.Ring->GetDisplayName()) : LOCTEXT("SlotPick", "pick one");
	case ELoadoutV2Slot::Evolution:
		return Draft.Socket1.IsEvolution() && Draft.Socket1.Evolution.Item ? FText::FromString(Draft.Socket1.Evolution.Item->ItemName) : LOCTEXT("SlotPick", "pick one");
	case ELoadoutV2Slot::Socket1:
		return AttachmentText(Draft.Socket1);
	case ELoadoutV2Slot::Socket2:
	default:
		return AttachmentText(Draft.Socket2);
	}
}

bool ULoadoutV2ScreenWidget::FindScreenRow(FLoadoutScreenRow &OutRow) const
{
	if (!bEditing || !Members.IsValidIndex(MemberIndex))
	{
		return false;
	}
	const FLoadoutScreenPage GearPage = WoRLoadoutScreen::BuildGearPage(GetWorld(), Members[MemberIndex]);
	const ELoadoutScreenRowKind Kind = EditEntry.Kind == ELoadoutV2EntryKind::Adapter ? ELoadoutScreenRowKind::Adapter : ELoadoutScreenRowKind::Weapon;
	const ELoadoutScreenPlace Place = EditEntry.Place == ELoadoutV2Place::Secondary	 ? ELoadoutScreenPlace::Secondary
									  : EditEntry.Place == ELoadoutV2Place::Adapter ? ELoadoutScreenPlace::Adapter
																					 : ELoadoutScreenPlace::Primary;
	const FLoadoutScreenRow *Found = GearPage.Rows.FindByPredicate([this, Kind, Place](const FLoadoutScreenRow &Row)
																	{ return Row.Kind == Kind && Row.Place == Place && (Place != ELoadoutScreenPlace::Adapter || Row.AdapterIndex == EditEntry.AdapterIndex); });
	if (!Found)
	{
		return false;
	}
	OutRow = *Found;
	return true;
}

void ULoadoutV2ScreenWidget::BuildItems()
{
	using namespace LoadoutV2ScreenStyle;
	// The picked socket line is found again by its piece after the rebuild (the list can change order).
	const bool bHadPick = Items.IsValidIndex(PickedSocket);
	const FLoadoutScreenChoice PickedBefore = bHadPick ? Items[PickedSocket].Socket : FLoadoutScreenChoice();
	ON_SCOPE_EXIT
	{
		PickedSocket = bHadPick ? Items.IndexOfByPredicate([&PickedBefore](const FLoadoutV2WindowItem &Item)
														  { return !Item.bEquipped && Item.Socket.Crystal == PickedBefore.Crystal && Item.Socket.EvolutionID == PickedBefore.EvolutionID; })
								: INDEX_NONE;
	};
	Items.Reset();
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!Slots.IsValidIndex(SlotIndex) || !FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	const ECharacterClass Class = Members[MemberIndex].Class;
	const FLoadoutV2Draft Current = bEditing ? WoRLoadoutV2::DraftFromEntry(*Active, EditEntry) : FLoadoutV2Draft();
	// The draft with one piece changed, checked as Y Confirm would check it.
	const auto Check = [this, Active, Class](const FLoadoutV2Draft &Changed)
	{
		int32 Cells = 0;
		const FText Refusal = bEditing ? WoRLoadoutV2::CheckEntryEdit(*Active, Class, EditEntry, Changed, Cells) : WoRLoadoutV2::CheckDraft(*Active, Class, Changed, Cells);
		return TPair<FText, int32>(Refusal, Cells);
	};
	const auto CellsLine = [this](int32 Cells)
	{ return FText::Format(LOCTEXT("ItemCellsAfter", "Cells after: {0} of {1}"), FText::AsNumber(Cells), FText::AsNumber(Page.CellsBudget)); };
	const auto IsElsewhere = [Active, &Current](const FGuid &ID)
	{
		// In another entry of this loadout (this entry's own pieces may move between its hands).
		if (!ID.IsValid() || ID == Current.MainHand.PersistentID || ID == Current.OffHand.PersistentID || ID == Current.Adapter.PersistentID)
		{
			return false;
		}
		FCombatLoadout Probe = *Active;
		return UInventoryComponent::StripFromLoadout(Probe, ID);
	};

	switch (Slots[SlotIndex])
	{
	case ELoadoutV2Slot::MainHand:
	case ELoadoutV2Slot::OffHand:
	{
		const bool bMain = Slots[SlotIndex] == ELoadoutV2Slot::MainHand;
		const FWeaponInventoryEntry &InSlot = bMain ? Current.MainHand : Current.OffHand;
		if (InSlot.IsValid())
		{
			FLoadoutV2WindowItem &Equipped = Items.AddDefaulted_GetRef();
			Equipped.bEquipped = true;
			Equipped.Weapon = InSlot;
			Equipped.Name = WeaponName(InSlot);
			Equipped.Line = InSlot.Weapon ? WoRGameNames::GetWeaponTypeName(InSlot.Weapon->WeaponType) : FText::GetEmpty();
		}
		const TArray<FLoadoutV2OffHandChoice> OffHands = bMain ? TArray<FLoadoutV2OffHandChoice>() : WoRLoadoutV2::BuildOffHandChoices(Draft.MainHand, Inventory->Weapons, Class);
		for (const FWeaponInventoryEntry &Owned : Inventory->Weapons)
		{
			if (!Owned.Weapon || (InSlot.IsValid() && Owned.PersistentID == InSlot.PersistentID) || Owned.PersistentID == (bMain ? Draft.OffHand : Draft.MainHand).PersistentID)
			{
				continue;
			}
			FLoadoutV2WindowItem &Item = Items.AddDefaulted_GetRef();
			Item.Weapon = Owned;
			Item.Name = WeaponName(Owned);
			FLoadoutV2Draft Changed = Draft;
			(bMain ? Changed.MainHand : Changed.OffHand) = Owned;
			if (bMain)
			{
				Changed.Socket1 = Owned.AttachedItem; // Q-476c: sockets stay on their piece; a new main hand brings its own
				Changed.Socket2 = Owned.StoneSocket;
			}
			const TPair<FText, int32> Result = Check(Changed);
			Item.Line = FText::Format(LOCTEXT("WeaponItemLine", "{0} · {1}"), WoRGameNames::GetWeaponTypeName(Owned.Weapon->WeaponType), CellsLine(Result.Value));
			const FLoadoutV2OffHandChoice *Pair = OffHands.FindByPredicate([&Owned](const FLoadoutV2OffHandChoice &Each) { return Each.Weapon.PersistentID == Owned.PersistentID; });
			Item.Refusal = IsElsewhere(Owned.PersistentID) ? LOCTEXT("RefuseInOtherEntry", "In another entry.")
						   : Pair && !Pair->Refusal.IsEmpty() ? Pair->Refusal
															  : Result.Key;
		}
		return;
	}
	case ELoadoutV2Slot::Adapter:
	{
		if (Current.Adapter.IsValid())
		{
			FLoadoutV2WindowItem &Equipped = Items.AddDefaulted_GetRef();
			Equipped.bEquipped = true;
			Equipped.Adapter = Current.Adapter;
			Equipped.Name = Current.Adapter.Ring ? FText::FromString(Current.Adapter.Ring->GetDisplayName()) : FText::GetEmpty();
		}
		if (!bEditing)
		{
			for (const FLoadoutV2PieceChoice &Choice : WoRLoadoutV2::BuildAdapterChoices(*Active, Class, Inventory->Rings))
			{
				FLoadoutV2WindowItem &Item = Items.AddDefaulted_GetRef();
				Item.Adapter = Choice.Adapter;
				Item.Name = Choice.Name;
				Item.Line = FText::Format(LOCTEXT("PieceItemLine", "{0} · {1}"), Choice.Line, CellsLine(Choice.CellsAfter));
				Item.Refusal = Choice.Refusal;
			}
			return;
		}
		for (const FRingInventoryEntry &Owned : Inventory->Rings)
		{
			if (!Owned.Ring || Owned.PersistentID == Current.Adapter.PersistentID)
			{
				continue;
			}
			FLoadoutV2WindowItem &Item = Items.AddDefaulted_GetRef();
			Item.Adapter = Owned;
			Item.Name = FText::FromString(Owned.Ring->GetDisplayName());
			FLoadoutV2Draft Changed = Draft;
			Changed.Adapter = Owned;
			Changed.Socket1 = Owned.AttachedItem; // the new adapter brings its own sockets (they live on the piece)
			Changed.Socket2 = Owned.StoneSocket;
			const TPair<FText, int32> Result = Check(Changed);
			Item.Line = FText::Format(LOCTEXT("PieceItemLine", "{0} · {1}"), WoRGameNames::GetAdapterFormName(Owned.Ring->Form), CellsLine(Result.Value));
			Item.Refusal = IsElsewhere(Owned.PersistentID) ? LOCTEXT("RefuseInOtherEntry", "In another entry.") : Result.Key;
		}
		return;
	}
	case ELoadoutV2Slot::Evolution:
	{
		const UEvolutionInventoryComponent *Evolutions = Inventory->GetOwner() ? Inventory->GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
		if (!Evolutions)
		{
			return;
		}
		TSet<FGuid> InUse;
		for (const FEvolutionInventoryEntry &Owned : Evolutions->Entries)
		{
			if (Inventory->IsEvolutionInUse(Owned.InstanceID))
			{
				InUse.Add(Owned.InstanceID);
			}
		}
		for (const FLoadoutV2PieceChoice &Choice : WoRLoadoutV2::BuildEvolutionChoices(*Active, Class, Evolutions->Entries, InUse))
		{
			FLoadoutV2WindowItem &Item = Items.AddDefaulted_GetRef();
			Item.Evolution = Choice.Evolution;
			Item.Name = Choice.Name;
			Item.Line = Choice.Line;
			Item.bEquipped = bEditing && Choice.Evolution.InstanceID == Current.EvolutionInstance;
			if (Item.bEquipped)
			{
				continue; // its own place: in use by this entry, not refused
			}
			if (bEditing && Choice.Refusal.IsEmpty() == false && !InUse.Contains(Choice.Evolution.InstanceID))
			{
				// An edit swaps the evolution in its place: check the swap, not a second one.
				FLoadoutV2Draft Changed = Draft;
				Changed.Socket1 = WoRLoadoutV2::MakeEvolutionAttachment(Choice.Evolution);
				Changed.EvolutionInstance = Choice.Evolution.InstanceID;
				Item.Refusal = Check(Changed).Key;
				continue;
			}
			Item.Refusal = Choice.Refusal;
		}
		Items.StableSort([](const FLoadoutV2WindowItem &A, const FLoadoutV2WindowItem &B) { return A.bEquipped && !B.bEquipped; });
		return;
	}
	case ELoadoutV2Slot::Socket1:
	case ELoadoutV2Slot::Socket2:
	default:
	{
		const int32 Socket = Slots[SlotIndex] == ELoadoutV2Slot::Socket1 ? LoadoutScreenSockets::CRYSTAL : LoadoutScreenSockets::STONE;
		FLoadoutScreenRow Row;
		if (!FindScreenRow(Row))
		{
			// A new entry: its sockets are the picked piece's own; they change once it is in (A Edit on the entry).
			FLoadoutV2WindowItem &Note = Items.AddDefaulted_GetRef();
			Note.Name = AttachmentText(Socket == LoadoutScreenSockets::CRYSTAL ? Draft.Socket1 : Draft.Socket2);
			Note.Refusal = LOCTEXT("SocketAfterIn", "Comes with the piece. Once the entry is in, A Edit on it changes its sockets.");
			return;
		}
		if (Row.Sockets.IsValidIndex(Socket) && !Row.Sockets[Socket].bEmpty)
		{
			FLoadoutV2WindowItem &Equipped = Items.AddDefaulted_GetRef();
			Equipped.bEquipped = true;
			Equipped.Name = Row.Sockets[Socket].Name;
			Equipped.Line = Row.Sockets[Socket].Label;
			if (bRemoveArmed)
			{
				FText Refusal;
				Equipped.Line = SocketRemoveWarning(Row, Socket, Refusal);
				Equipped.Refusal = Refusal;
			}
		}
		for (const FLoadoutScreenChoice &Choice : WoRLoadoutScreen::BuildSocketChoices(GetWorld(), Members[MemberIndex], Row, Socket))
		{
			FLoadoutV2WindowItem &Item = Items.AddDefaulted_GetRef();
			Item.Socket = Choice;
			Item.Name = FText::Format(LOCTEXT("SocketItemName", "{0}  {1}"), Choice.Row.Name, Choice.Row.Tier);
			Item.Line = FText::Format(LOCTEXT("SocketItemLine", "{0} · Cells after: {1} of {2}"), Choice.Row.KindLine, FText::AsNumber(Choice.NewSpent),
									  FText::AsNumber(Page.CellsBudget));
			Item.Refusal = Choice.Refusal;
		}
		if (Items.Num() == 1 && Items[0].bEquipped)
		{
			// Today's socket rule: a full socket is emptied first (its gem goes, CL-28b), then filled.
			Items[0].Line = bRemoveArmed ? Items[0].Line : FText::Format(LOCTEXT("SocketFullLine", "{0} · A Remove to put another in"), Items[0].Line);
		}
		return;
	}
	}
}

void ULoadoutV2ScreenWidget::PickItem()
{
	if (!Items.IsValidIndex(Selected) || !Slots.IsValidIndex(SlotIndex))
	{
		return;
	}
	const FLoadoutV2WindowItem &Item = Items[Selected];
	if (!Item.Refusal.IsEmpty())
	{
		Message = Item.Refusal;
		RefreshView();
		return;
	}
	Message = FText::GetEmpty();
	switch (Slots[SlotIndex])
	{
	case ELoadoutV2Slot::MainHand:
		Draft.MainHand = Item.Weapon;
		Draft.Socket1 = Item.Weapon.AttachedItem; // Q-476c: sockets stay on their piece; a new main hand brings its own
		Draft.Socket2 = Item.Weapon.StoneSocket;
		break;
	case ELoadoutV2Slot::OffHand:
		Draft.OffHand = Item.Weapon;
		break;
	case ELoadoutV2Slot::Adapter:
		Draft.Adapter = Item.Adapter;
		Draft.Socket1 = Item.Adapter.AttachedItem;
		Draft.Socket2 = Item.Adapter.StoneSocket;
		break;
	case ELoadoutV2Slot::Evolution:
		Draft.Socket1 = WoRLoadoutV2::MakeEvolutionAttachment(Item.Evolution);
		Draft.EvolutionInstance = Item.Evolution.InstanceID;
		break;
	case ELoadoutV2Slot::Socket1:
	case ELoadoutV2Slot::Socket2:
	default:
		PickedSocket = PickedSocket == Selected ? INDEX_NONE : Selected; // a socket goes in on Y Confirm
		RefreshView();
		return;
	}
	// A piece is picked: back to the slots, the draft shown in the box.
	Mode = ELoadoutV2Mode::WindowSlots;
	Selected = FMath::Min(SlotIndex + 1, Slots.Num() - 1);
	Items.Reset();
	bComparing = false;
	RefreshView();
}

void ULoadoutV2ScreenWidget::RemoveEquipped()
{
	if (!Slots.IsValidIndex(SlotIndex) || !Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	const ELoadoutV2Slot GearSlot = Slots[SlotIndex];
	if (GearSlot == ELoadoutV2Slot::OffHand)
	{
		Draft.OffHand = FWeaponInventoryEntry(); // goes on Y Confirm (a twin-only main hand refuses then)
		Mode = ELoadoutV2Mode::WindowSlots;
		Selected = SlotIndex;
		Items.Reset();
		RefreshView();
		return;
	}
	if (GearSlot == ELoadoutV2Slot::Socket1 || GearSlot == ELoadoutV2Slot::Socket2)
	{
		const int32 Socket = GearSlot == ELoadoutV2Slot::Socket1 ? LoadoutScreenSockets::CRYSTAL : LoadoutScreenSockets::STONE;
		FLoadoutScreenRow Row;
		if (!FindScreenRow(Row))
		{
			return;
		}
		if (!bRemoveArmed)
		{
			bRemoveArmed = true; // says what comes off first (Q-440c)
			FText Refusal;
			const FText Warning = LoadoutV2ScreenStyle::SocketRemoveWarning(Row, Socket, Refusal);
			Message = Warning.IsEmpty() ? Refusal : Warning;
			bRemoveArmed = !Warning.IsEmpty();
			RefreshData();
			return;
		}
		bRemoveArmed = false;
		Message = WoRLoadoutScreen::TakeOffSocket(GetWorld(), Members[MemberIndex], Row, Socket);
		if (Message.IsEmpty())
		{
			UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: socket %d of %s emptied"), *Members[MemberIndex].Name.ToString(), Socket + 1, *EditEntry.Name.ToString());
			ULoadoutComponent *Loadout = nullptr;
			UInventoryComponent *Inventory = nullptr;
			const FCombatLoadout *Active = nullptr;
			if (FindActive(Loadout, Inventory, Active))
			{
				Draft = WoRLoadoutV2::DraftFromEntry(*Active, EditEntry); // the piece changed: read its sockets again
			}
		}
		RefreshData();
		return;
	}
	// The main piece (a weapon, an adapter, the evolution): the whole entry comes out.
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	const ECharacterClass Class = Members[MemberIndex].Class;
	const FLoadoutV2Entry Entry = EditEntry;
	FText Refusal;
	FCombatLoadout Probe = *Active;
	if (!WoRLoadoutV2::RemoveEntry(Probe, Class, Entry, Refusal))
	{
		Message = Refusal;
		RefreshView();
		return;
	}
	bool bRemoved = false;
	if (!Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [Class, &Entry, &bRemoved](FCombatLoadout &Changed)
								{
									FText Ignored;
									bRemoved = WoRLoadoutV2::RemoveEntry(Changed, Class, Entry, Ignored);
								},
								Refusal))
	{
		Message = Refusal;
		RefreshView();
		return;
	}
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: %s taken out (%s)"), *Members[MemberIndex].Name.ToString(), *Entry.Name.ToString(), bRemoved ? TEXT("done") : TEXT("nothing"));
	Message = FText::Format(LOCTEXT("EntryRemoved", "{0} came out."), Entry.Name);
	CloseWindow();
}

void ULoadoutV2ScreenWidget::ConfirmWindow()
{
	if (!Members.IsValidIndex(MemberIndex))
	{
		return;
	}
	// A socket line picked: it goes into the owned piece now (today's socket rule: the pouch pays, B-440c).
	if (Mode == ELoadoutV2Mode::WindowItems && Items.IsValidIndex(PickedSocket) && Slots.IsValidIndex(SlotIndex))
	{
		const int32 Socket = Slots[SlotIndex] == ELoadoutV2Slot::Socket1 ? LoadoutScreenSockets::CRYSTAL : LoadoutScreenSockets::STONE;
		FLoadoutScreenRow Row;
		if (!FindScreenRow(Row))
		{
			return;
		}
		Message = WoRLoadoutScreen::ApplySocketChoice(GetWorld(), Members[MemberIndex], Row, Socket, Items[PickedSocket].Socket);
		if (Message.IsEmpty())
		{
			UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: %s socketed into %s"), *Members[MemberIndex].Name.ToString(), *Items[PickedSocket].Name.ToString(),
				   *EditEntry.Name.ToString());
			ULoadoutComponent *Loadout = nullptr;
			UInventoryComponent *Inventory = nullptr;
			const FCombatLoadout *Active = nullptr;
			if (FindActive(Loadout, Inventory, Active))
			{
				Draft = WoRLoadoutV2::DraftFromEntry(*Active, EditEntry);
			}
			Mode = ELoadoutV2Mode::WindowSlots;
			Selected = SlotIndex;
			Items.Reset();
			PickedSocket = INDEX_NONE;
		}
		RefreshData();
		return;
	}
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!FindActive(Loadout, Inventory, Active))
	{
		return;
	}
	const ECharacterClass Class = Members[MemberIndex].Class;
	int32 CellsAfter = 0;
	const FText Refusal = bEditing ? WoRLoadoutV2::CheckEntryEdit(*Active, Class, EditEntry, Draft, CellsAfter) : WoRLoadoutV2::CheckDraft(*Active, Class, Draft, CellsAfter);
	if (!Refusal.IsEmpty())
	{
		Message = Refusal;
		RefreshView();
		return;
	}
	const FLoadoutV2Draft Before = bEditing ? WoRLoadoutV2::DraftFromEntry(*Active, EditEntry) : FLoadoutV2Draft();
	const bool bNewEvolution = Draft.Kind == ELoadoutV2DraftKind::Evolution && (!bEditing || Before.EvolutionInstance != Draft.EvolutionInstance);
	if (bNewEvolution)
	{
		// UIScreens: attaching the evolution asks first.
		const FText EvolutionName = Draft.Socket1.Evolution.Item ? FText::FromString(Draft.Socket1.Evolution.Item->ItemName) : FText::GetEmpty();
		Message = FText::Format(LOCTEXT("AskEvolution", "Attach {0} to {1}? Uses {2} cells: {3} left → {4} left."), EvolutionName, Members[MemberIndex].Name,
								FText::AsNumber(LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST), FText::AsNumber(FMath::Max(0, Page.CellsBudget - Page.CellsUsed)),
								FText::AsNumber(FMath::Max(0, Page.CellsBudget - CellsAfter)));
		Mode = ELoadoutV2Mode::WindowAsk;
		RefreshView();
		return;
	}
	ApplyWindowDraft();
}

void ULoadoutV2ScreenWidget::ApplyWindowDraft()
{
	ULoadoutComponent *Loadout = nullptr;
	UInventoryComponent *Inventory = nullptr;
	const FCombatLoadout *Active = nullptr;
	if (!FindActive(Loadout, Inventory, Active))
	{
		Message = LOCTEXT("MemberNotHere", "This character isn't here.");
		Mode = ELoadoutV2Mode::List;
		RefreshData();
		return;
	}
	const ECharacterClass Class = Members[MemberIndex].Class;
	const FLoadoutV2Draft Applied = Draft;
	const FLoadoutV2Entry Entry = EditEntry;
	const bool bEdit = bEditing;
	bool bApplied = false;
	FText ApplyRefusal;
	FText Refusal;
	const bool bDone = Loadout->TryChangeGear(Inventory->ActiveLoadoutIndex, [&Applied, &Entry, bEdit, Class, &bApplied, &ApplyRefusal](FCombatLoadout &Changed)
											  {
												  bApplied = bEdit ? WoRLoadoutV2::ReplaceEntry(Changed, Class, Entry, Applied, ApplyRefusal)
																   : WoRLoadoutV2::ApplyDraft(Changed, Class, Applied, ApplyRefusal);
											  },
											  Refusal);
	if (bDone && !bApplied)
	{
		Message = ApplyRefusal; // the loadout changed since the window read it: nothing went in
		Mode = ELoadoutV2Mode::WindowSlots;
		Selected = SlotIndex;
		RefreshData();
		return;
	}
	if (!bDone)
	{
		Message = Refusal;
		Mode = ELoadoutV2Mode::WindowSlots;
		Selected = SlotIndex;
		RefreshView();
		return;
	}
	const UObject *Piece = Applied.Socket1.Evolution.Item;
	if (Applied.Kind == ELoadoutV2DraftKind::Weapon)
	{
		Piece = Applied.MainHand.Weapon;
	}
	else if (Applied.Kind == ELoadoutV2DraftKind::Adapter)
	{
		Piece = Applied.Adapter.Ring;
	}
	UE_LOG(LogWoRUI, Log, TEXT("[LoadoutV2] %s: entry %s (%s)"), *Members[MemberIndex].Name.ToString(), bEdit ? TEXT("changed") : TEXT("added"), *GetNameSafe(Piece));
	Message = FText::GetEmpty();
	CloseWindow();
}

void ULoadoutV2ScreenWidget::CloseWindow()
{
	Mode = ELoadoutV2Mode::List;
	bEditing = false;
	Kinds.Reset();
	Slots.Reset();
	Items.Reset();
	PickedSocket = INDEX_NONE;
	bComparing = false;
	bRemoveArmed = false;
	Selected = ListSelected;
	RefreshData();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GLoadoutV2OpenCommand(
	TEXT("wor.UI.LoadoutV2"),
	TEXT("B-476b/c: open the v2 loadout screen (beside today's wor.UI.Loadout): A Edit / Equip, X Simple / Full, L3 / 6 Set active, Y Options; "
		 "in the Add gear window A Pick / Remove, X Compare, Y Confirm."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		if (!ULoadoutV2ScreenWidget::Open(World ? World->GetFirstPlayerController() : nullptr))
		{
			UE_LOG(LogWoRUI, Warning, TEXT("[LoadoutV2] wor.UI.LoadoutV2: not opened (no hub controller with a root layout)"));
		}
	}));

static FAutoConsoleCommandWithWorld GLoadoutApparelPrintCommand(
	TEXT("wor.UI.LoadoutApparel.Print"),
	TEXT("B-440f: print each party member's owned apparel and what the active loadout wears (the v2 loadout screen's Apparel tab)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		for (const FLoadoutScreenMember &Member : WoRLoadoutScreen::BuildMembers(World))
		{
			UE_LOG(LogWoRUI, Display, TEXT("[LoadoutApparel] %s"), *WoRLoadoutApparel::GetMemberApparelString(World, Member));
		}
	}));

static FAutoConsoleCommandWithWorld GLoadoutSavedPrintCommand(
	TEXT("wor.UI.LoadoutSaved.Print"),
	TEXT("B-440e: print each party member's saved loadouts as the v2 loadout screen's Options > Saved loadouts list shows them."),
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
#endif

#undef LOCTEXT_NAMESPACE
