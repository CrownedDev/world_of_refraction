// PartyScreenModel.cpp

#include "UI/Party/PartyScreenModel.h"

#include "Character/CharacterData.h"
#include "Combat/Grid/CombatGridConstants.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Online/WoRSessionSubsystem.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "UI/Menus/LoadoutV2GridModel.h"
#include "UI/Menus/LoadoutV2SkillsModel.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRPartyScreen"

namespace
{
	UPartySessionSubsystem *PartySessionOf(UWorld *World)
	{
		UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	}

	/** B-628 (mockup "Front row, middle"): a start cell's place down its row, top to bottom on the grid. */
	FText GetStartPlaceName(int32 Column)
	{
		static_assert(CombatGridConstants::GRID_COLUMNS == 3, "one name per place down a row");
		switch (Column)
		{
		case 0:
			return LOCTEXT("PlaceTop", "top");
		case 1:
			return LOCTEXT("PlaceMiddle", "middle");
		default:
			return LOCTEXT("PlaceBottom", "bottom");
		}
	}

	/** B-628: the slotted skills of the Skills tab's Entry; empty when there is no such entry (no skill slots). */
	FText GetSlottedSkillsLine(const FLoadoutV2SkillsPage &Skills, int32 Entry)
	{
		if (!Skills.Entries.IsValidIndex(Entry))
		{
			return FText::GetEmpty();
		}
		TArray<FText> Names;
		for (const FLoadoutV2SkillRowRef &Ref : WoRLoadoutV2Skills::GetRows(Skills, Entry))
		{
			const FLoadoutScreenRow *Row = WoRLoadoutV2Skills::FindRow(Skills, Ref);
			if (Row && Row->Skill)
			{
				Names.Add(Row->Name);
			}
		}
		return Names.IsEmpty() ? LOCTEXT("NoSkills", "No skills slotted") : FText::Join(LOCTEXT("SkillSeparator", ", "), Names);
	}
}

FPartyScreenPage WoRPartyScreen::BuildPage(const UParty *Party, bool bHosting)
{
	FPartyScreenPage Page;
	Page.Title = LOCTEXT("Title", "Party");
	Page.Subtitle = FText::Format(LOCTEXT("Subtitle", "Up to {0} fight together"), PartyConstants::MAX_PARTY_MEMBERS);
	Page.InviteStatus = bHosting ? LOCTEXT("Hosting", "hosting") : LOCTEXT("NoSession", "no session");
	if (!Party)
	{
		return Page;
	}
	for (int32 SlotIndex = 0; SlotIndex < Party->Members.Num(); ++SlotIndex)
	{
		const FPartyMember &Member = Party->Members[SlotIndex];
		FPartyScreenMember &Row = Page.Members.AddDefaulted_GetRef();
		Row.bLeader = SlotIndex == PartyConstants::LEADER_SLOT;
		Row.bAISeat = !Row.bLeader && Member.bAISeat;
		Row.bFallen = Member.IsFallen();
		Row.Name = Member.CharacterData ? FText::FromString(Member.CharacterData->Name) : LOCTEXT("NoData", "<no data>");
		Row.ClassName = Member.CharacterData ? WoRGameNames::GetCharacterClassName(Member.CharacterData->CharacterClass) : FText::GetEmpty();
		// D-200: an AI member shows no level text anywhere.
		Row.PlayedBy = Row.bLeader ? LOCTEXT("Leader", "You (leader)") : Row.bAISeat ? LOCTEXT("AI", "AI") : LOCTEXT("You", "You");
		Row.bHasCell = Member.HasFormation();
		Row.Row = Member.PreferredRow;
		Row.Column = Row.bHasCell ? FMath::Clamp(Member.PreferredColumn, 0, CombatGridConstants::GRID_COLUMNS - 1) : 0;
		Row.StartValue = Row.bHasCell ? FText::Format(LOCTEXT("StartCell", "{0}, {1}"), WoRGameNames::GetCombatRowName(Member.PreferredRow),
													  GetStartPlaceName(Row.Column))
									  : LOCTEXT("StartsAuto", "Auto");
		Row.StartLine = FText::Format(LOCTEXT("Starts", "Starts: {0}"), Row.StartValue);
		Row.FieldSkillLine = FText::Format(LOCTEXT("FieldSkill", "Field skill: {0}"), WoRGameNames::GetFieldMovementName(Member.FieldSkill.Type));
		Row.GuardLine = Member.GuardStyle == EGuardStyle::Unarmed ? LOCTEXT("UnarmedGuard", "Unarmed guard") : LOCTEXT("WeaponGuard", "Weapon guard");
	}
	return Page;
}

FPartyScreenPage WoRPartyScreen::BuildPage(UWorld *World)
{
	const UPartySessionSubsystem *PartySession = PartySessionOf(World);
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UWoRSessionSubsystem *Session = GI ? GI->GetSubsystem<UWoRSessionSubsystem>() : nullptr;
	return BuildPage(PartySession ? PartySession->GetLocalParty() : nullptr, Session && Session->IsHosting());
}

void WoRPartyScreen::GetNextCell(bool bHasCell, ECombatRow Row, int32 Column, ECombatRow &OutRow, int32 &OutColumn)
{
	using namespace CombatGridConstants;
	const int32 Next = bHasCell ? (static_cast<int32>(Row) * GRID_COLUMNS + Column + 1) % POSITIONS_PER_TEAM : 0;
	OutRow = static_cast<ECombatRow>(Next / GRID_COLUMNS);
	OutColumn = Next % GRID_COLUMNS;
}

FText WoRPartyScreen::ToggleSeat(UWorld *World, int32 SlotIndex)
{
	UPartySessionSubsystem *PartySession = PartySessionOf(World);
	if (!PartySession)
	{
		return LOCTEXT("NoParty", "No party.");
	}
	if (SlotIndex == PartyConstants::LEADER_SLOT)
	{
		return LOCTEXT("LeaderStays", "The leader can't be given to the AI.");
	}
	if (PartySession->IsFightRunning())
	{
		return LOCTEXT("FightRunning", "Not during a fight.");
	}
	if (!PartySession->SetMemberAISeat(SlotIndex, !PartySession->IsMemberAISeat(SlotIndex)))
	{
		return LOCTEXT("SeatRefused", "That seat can't be changed now.");
	}
	return FText::GetEmpty();
}

void WoRPartyScreen::MoveOnGrid(UWorld *World, int32 SlotIndex)
{
	UPartySessionSubsystem *PartySession = PartySessionOf(World);
	if (!PartySession)
	{
		return;
	}
	ECombatRow Row = ECombatRow::Back;
	int32 Column = 0;
	const bool bHasCell = PartySession->GetMemberFormation(SlotIndex, Row, Column);
	GetNextCell(bHasCell, Row, Column, Row, Column);
	PartySession->SetMemberFormation(SlotIndex, Row, Column);
}

FPartyScreenLoadout WoRPartyScreen::BuildLoadout(const FText &Title, const FLoadoutV2WeaponsPage &Weapons, const FLoadoutV2SkillsPage &Skills)
{
	FPartyScreenLoadout Loadout;
	Loadout.Title = Title;
	for (int32 Index = 0; Index < Weapons.Entries.Num(); ++Index)
	{
		const FLoadoutV2Entry &Piece = Weapons.Entries[Index];
		FPartyScreenLoadoutEntry &Entry = Loadout.Entries.AddDefaulted_GetRef();
		Entry.Name = Piece.Name;
		Entry.TypeLine = WoRLoadoutV2Grid::GetTypeTierLine(Piece.TypeIcon.Name, Piece.Tier);
		Entry.SkillsLine = GetSlottedSkillsLine(Skills, WoRLoadoutV2Skills::FindEntryFor(Skills, Index));
	}
	for (int32 Index = 0; Index < Skills.Entries.Num(); ++Index)
	{
		if (Skills.Entries[Index].bInnate)
		{
			FPartyScreenLoadoutEntry &Entry = Loadout.Entries.AddDefaulted_GetRef();
			Entry.Name = Skills.Entries[Index].Name;
			Entry.SkillsLine = GetSlottedSkillsLine(Skills, Index);
		}
	}
	return Loadout;
}

FPartyScreenLoadout WoRPartyScreen::BuildLoadout(UWorld *World, int32 SlotIndex)
{
	const UPartySessionSubsystem *PartySession = PartySessionOf(World);
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	const UCharacterData *Data = Party && Party->Members.IsValidIndex(SlotIndex) ? Party->Members[SlotIndex].CharacterData.Get() : nullptr;
	if (!Data)
	{
		return FPartyScreenLoadout();
	}
	const FLoadoutScreenMember Member{UPoolSubsystem::GetCharacterKey(Party->Members[SlotIndex]), FText::FromString(Data->Name), Data->CharacterClass};
	UInventoryComponent *Inventory = nullptr;
	if (!WoRLoadoutScreen::FindMemberLoadout(World, Member, Inventory) || !Inventory)
	{
		return FPartyScreenLoadout();
	}
	const FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex]; // FindMemberLoadout checked the index
	const FLoadoutV2WeaponsPage Weapons = WoRLoadoutV2::BuildWeaponsPage(Active, Member.Class);
	return BuildLoadout(WoRLoadoutScreen::FormatLoadoutTitle(Active.LoadoutName, Inventory->ActiveLoadoutIndex + 1, Inventory->SavedLoadouts.Num()),
						Weapons, WoRLoadoutV2Skills::Build(WoRLoadoutSkills::BuildSkillsPage(World, Member), Weapons));
}

FText WoRPartyScreen::Dismiss(UWorld *World, int32 SlotIndex)
{
	UPartySessionSubsystem *PartySession = PartySessionOf(World);
	if (!PartySession)
	{
		return LOCTEXT("NoParty", "No party.");
	}
	if (SlotIndex == PartyConstants::LEADER_SLOT)
	{
		return LOCTEXT("LeaderCantGo", "The leader can't be dismissed.");
	}
	if (PartySession->IsFightRunning())
	{
		return LOCTEXT("FightRunning", "Not during a fight.");
	}
	if (PartySession->IsPlayerSlot(SlotIndex))
	{
		return LOCTEXT("FriendSeat", "A friend plays this seat.");
	}
	if (!PartySession->DismissMember(SlotIndex))
	{
		return LOCTEXT("DismissRefused", "That member can't leave now.");
	}
	return FText::GetEmpty();
}

FString WoRPartyScreen::GetPartyScreenString(UWorld *World)
{
	const FPartyScreenPage Page = BuildPage(World);
	FString Out = FString::Printf(TEXT("[PartyScreen] %s (%s), %d member(s)"), *Page.Title.ToString(), *Page.Subtitle.ToString(), Page.Members.Num());
	for (int32 Index = 0; Index < Page.Members.Num(); ++Index)
	{
		const FPartyScreenMember &Member = Page.Members[Index];
		Out += FString::Printf(TEXT("\n  [%d] %s (%s) played by %s | %s%s | %s | %s%s"), Index, *Member.Name.ToString(),
							   *Member.ClassName.ToString(), *Member.PlayedBy.ToString(), *Member.StartLine.ToString(),
							   Member.bHasCell ? *FString::Printf(TEXT(" cell %d"), Member.Column) : TEXT(""),
							   *Member.FieldSkillLine.ToString(), *Member.GuardLine.ToString(), Member.bFallen ? TEXT(" (fallen)") : TEXT(""));
		const FPartyScreenLoadout Loadout = BuildLoadout(World, Index);
		Out += FString::Printf(TEXT("\n      loadout: %s"), Loadout.Title.IsEmpty() ? TEXT("<none>") : *Loadout.Title.ToString());
		for (const FPartyScreenLoadoutEntry &Entry : Loadout.Entries)
		{
			Out += FString::Printf(TEXT("\n        %s %s | %s"), *Entry.Name.ToString(), *Entry.TypeLine.ToString(), *Entry.SkillsLine.ToString());
		}
	}
	Out +=FString::Printf(TEXT("\n  Invite: %s"), *Page.InviteStatus.ToString());
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GPartyScreenPrintCommand(
	TEXT("wor.UI.PrintParty"),
	TEXT("B-458: log what the party screen shows (members, who plays them, start cells, field skills)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoRUI, Log, TEXT("%s"), *WoRPartyScreen::GetPartyScreenString(World));
	}));
#endif

#undef LOCTEXT_NAMESPACE
