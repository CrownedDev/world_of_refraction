// HubHUDModel.cpp

#include "UI/Hub/HubHUDModel.h"

#include "Character/CharacterData.h"
#include "Currency/CurrencyTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Field/FieldMovementComponent.h"
#include "Field/FieldSkillTypes.h"
#include "Infusion/ElementColors.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Hub/HubInteract.h"
#include "UI/Menus/PauseMenuModel.h"
#include "UI/WoRIconTable.h"
#include "UI/WoRKeyHintText.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRHubHUD"

FHubHUDPrompt WoRHubHUD::BuildPrompt(APawn *Pawn)
{
	FHubHUDPrompt Prompt;
	const TArray<HubInteract::FInteractCandidate> Candidates = HubInteract::GatherCandidates(Pawn);
	if (Candidates.Num() == 0)
	{
		return Prompt;
	}
	// The nearest only: it is what the interact key acts on (HubInteract::InteractWithNearest).
	const HubInteract::FInteractCandidate &Nearest = Candidates[0];
	Prompt.bNearVendor = Nearest.bVendor;
	if (Nearest.Prompt.IsEmpty())
	{
		return Prompt;
	}
	Prompt.bShown = true;
	Prompt.Verb = WoRGameNames::GetInteractVerbName(Nearest.Prompt.Verb);
	Prompt.What = Nearest.Prompt.What;
	Prompt.WorldLocation = Nearest.Location + FVector(0.0f, 0.0f, PROMPT_HEIGHT_CM);
	return Prompt;
}

FText WoRHubHUD::GetInteractKeyLabel(bool bGamepad)
{
	// InputMap.md: Interact is E / X (Square). B-481: the key names live in one table (WoRKeyHints).
	return WoRKeyHints::Format(FText::AsCultureInvariant(TEXT("{Interact}")), bGamepad);
}

FText WoRHubHUD::FormatPromptLine(const FInteractPrompt &Prompt, bool bGamepad)
{
	if (Prompt.IsEmpty())
	{
		return FText::GetEmpty();
	}
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Verb"), WoRGameNames::GetInteractVerbName(Prompt.Verb));
	Arguments.Add(TEXT("What"), Prompt.What);
	return WoRKeyHints::Format(Prompt.What.IsEmpty() ? LOCTEXT("PromptLineVerb", "{Interact}  {Verb}") : LOCTEXT("PromptLine", "{Interact}  {Verb} · {What}"),
							   bGamepad, Arguments);
}

FHubHUDMoney WoRHubHUD::BuildMoney(const UWorld *World)
{
	// The game menu's first chip is the place's money (Gold in the hub, Prisms in a run): one read for both.
	const FPauseMenuHeader Header = WoRPauseMenu::BuildHeader(World);
	FHubHUDMoney Money;
	Money.Currency = Header.bInRun ? ECurrencyType::Gold : ECurrencyType::Prisms; // the names swap (D-159)
	if (Header.Chips.Num() > 0)
	{
		Money.Value = Header.Chips[0].Value;
		Money.Label = Header.Chips[0].Label;
	}
	return Money;
}

bool WoRHubHUD::ShouldShowMoney(bool bNearVendor, float SecondsSinceChange)
{
	return bNearVendor || (SecondsSinceChange >= 0.0f && SecondsSinceChange < MONEY_SHOWN_SECONDS);
}

FText WoRHubHUD::BuildQuestLine(const UWorld *World)
{
	return WoRPauseMenu::IsInRun(World) ? WoRPauseMenu::GetTrackedLine(World) : FText::GetEmpty();
}

TArray<FHubHUDPartyCard> WoRHubHUD::BuildPartyStrip(const UParty *Party)
{
	TArray<FHubHUDPartyCard> Cards;
	if (!Party)
	{
		return Cards;
	}
	for (const FPartyMember &Member : Party->Members)
	{
		const UCharacterData *Data = Member.CharacterData;
		if (!Data)
		{
			continue;
		}
		FHubHUDPartyCard &Card = Cards.AddDefaulted_GetRef();
		Card.Name = FText::FromString(Data->Name);
		Card.DotColor = ElementColors::GetColorForElement(Data->GetElement());
		const int32 MaxHP = Data->CalculateMaxHealth();
		Card.HPShare = Member.CurrentHP == PartyConstants::NO_CARRIED_VITALS || MaxHP <= 0
						   ? 1.0f
						   : FMath::Clamp(static_cast<float>(Member.CurrentHP) / static_cast<float>(MaxHP), 0.0f, 1.0f);
	}
	return Cards;
}

float WoRHubHUD::GetPartyStripMaxWidth(int32 Cards)
{
	return Cards * (PARTY_CARD_MAX_WIDTH + PARTY_STRIP_GAP) + FIELD_ATTACK_CHIP_MAX_WIDTH;
}

FHubHUDFieldAttackChip WoRHubHUD::BuildFieldAttackChip(const FFieldAttackEntry &Entry)
{
	FHubHUDFieldAttackChip Chip;
	Chip.Name = Entry.Name.IsEmpty() ? LOCTEXT("FieldAttackUnarmed", "Unarmed") : Entry.Name;
	Chip.OwnIcon = Entry.Icon;
	Chip.bElemental = FieldSkillRules::IsElemental(Entry.Element);
	if (Chip.bElemental)
	{
		Chip.ElementName = WoRGameNames::GetElementName(Entry.Element);
		Chip.ElementColor = ElementColors::GetColorForElement(Entry.Element);
	}
	switch (Entry.Kind)
	{
	case EFieldAttackKind::Weapon:
		if (Entry.Name.IsEmpty())
		{
			return Chip; // the plain swing: no weapon, no icon
		}
		Chip.Icon.Kind = EWoRIconKind::WeaponType;
		Chip.Icon.WeaponType = Entry.WeaponType;
		Chip.Icon.Name = WoRGameNames::GetWeaponTypeName(Entry.WeaponType);
		Chip.IconText = Chip.Icon.Name;
		return Chip;
	case EFieldAttackKind::Ring:
		Chip.IconText = LOCTEXT("FieldAttackRing", "Ring");
		break;
	case EFieldAttackKind::Evolution:
		Chip.IconText = LOCTEXT("FieldAttackEvolution", "Evolution");
		break;
	case EFieldAttackKind::Innate:
	default:
		break; // the name already says "Innate"
	}
	if (Chip.bElemental)
	{
		Chip.Icon.Kind = EWoRIconKind::Element;
		Chip.Icon.Element = Entry.Element;
		Chip.Icon.Name = Chip.ElementName;
	}
	return Chip;
}

FString WoRHubHUD::GetHubHUDString(const UWorld *World)
{
	const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
	const FHubHUDPrompt Prompt = BuildPrompt(PC ? PC->GetPawn() : nullptr);
	const FHubHUDMoney Money = BuildMoney(World);
	const FText Quest = BuildQuestLine(World);
	const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GameInstance ? GameInstance->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	FString Party;
	for (const FHubHUDPartyCard &Card : BuildPartyStrip(PartySession ? PartySession->GetLocalParty() : nullptr))
	{
		Party += FString::Printf(TEXT("%s%s %.0f%%"), Party.IsEmpty() ? TEXT("") : TEXT(", "), *Card.Name.ToString(), Card.HPShare * 100.0f);
	}
	const APawn *Pawn = PC ? PC->GetPawn() : nullptr;
	const UFieldMovementComponent *FieldMovement = Pawn ? Pawn->FindComponentByClass<UFieldMovementComponent>() : nullptr;
	FString FieldAttack = TEXT("none");
	if (FieldMovement)
	{
		const FHubHUDFieldAttackChip Chip = BuildFieldAttackChip(FieldMovement->GetFieldAttackEntry());
		const bool bIcon = Chip.OwnIcon || WoRIcons::Find(Chip.Icon);
		FieldAttack = FString::Printf(TEXT("%s (%s; icon: %s)"), *Chip.Name.ToString(), Chip.bElemental ? *Chip.ElementName.ToString() : TEXT("no element"),
									  bIcon ? TEXT("texture") : (Chip.IconText.IsEmpty() ? TEXT("none") : *Chip.IconText.ToString()));
	}
	return FString::Printf(TEXT("[HubHUD] prompt: %s; near a vendor: %s; money: %s %s; quest: %s; party: %s; field attack: %s"),
						   Prompt.bShown ? *FString::Printf(TEXT("%s %s"), *Prompt.Verb.ToString(), *Prompt.What.ToString()) : TEXT("none"),
						   Prompt.bNearVendor ? TEXT("yes") : TEXT("no"), *Money.Value.ToString(), *Money.Label.ToString(),
						   Quest.IsEmpty() ? TEXT("none") : *Quest.ToString(), Party.IsEmpty() ? TEXT("none") : *Party, *FieldAttack);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GHubHUDPrintCommand(
	TEXT("wor.UI.PrintHubHUD"),
	TEXT("DEBUG (B-443): the interact prompt for what the player faces now, and the money chip's text."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoRUI, Display, TEXT("%s"), *WoRHubHUD::GetHubHUDString(World));
	}));
#endif

#undef LOCTEXT_NAMESPACE
