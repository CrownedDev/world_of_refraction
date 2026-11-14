// PartyScreenModel.cpp

#include "UI/Party/PartyScreenModel.h"

#include "Character/CharacterData.h"
#include "Combat/Grid/CombatGridConstants.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Online/WoRSessionSubsystem.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
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
		Row.StartLine = Row.bHasCell ? FText::Format(LOCTEXT("Starts", "Starts: {0}"), WoRGameNames::GetCombatRowName(Member.PreferredRow))
									 : LOCTEXT("StartsAuto", "Starts: placed for you");
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
		return LOCTEXT("FightRunning", "Seats are fixed while a fight runs.");
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
	}
	Out += FString::Printf(TEXT("\n  Invite: %s"), *Page.InviteStatus.ToString());
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
