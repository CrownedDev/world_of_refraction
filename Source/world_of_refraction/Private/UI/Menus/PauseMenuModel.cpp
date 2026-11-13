// PauseMenuModel.cpp

#include "UI/Menus/PauseMenuModel.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/EWorldPillar.h"
#include "Combat/Tier/CombatTier.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRPlayerState.h"
#include "Save/SaveSubsystem.h"
#include "Trial/DefeatRules.h"
#include "Trial/LevelDropPlacement.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRPauseMenu"

namespace
{
	const UTrialRunSubsystem *FindPauseMenuRun(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	}

	const UCurrencyComponent *FindPauseMenuWallet(const UWorld *World)
	{
		const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!PC)
		{
			return nullptr;
		}
		if (const UCurrencyComponent *Wallet = CurrencyWallet::FindFor(PC->GetPawn()))
		{
			return Wallet;
		}
		const AWoRPlayerState *Player = PC->GetPlayerState<AWoRPlayerState>();
		return Player ? Player->GetWallet() : nullptr;
	}

	FPauseMenuPage MakePauseMenuPage(EPauseMenuPage Page, const FText &Label, const FText &Preview, const FText &Refusal = FText::GetEmpty())
	{
		FPauseMenuPage Row;
		Row.Page = Page;
		Row.Label = Label;
		Row.Preview = Preview;
		Row.Refusal = Refusal;
		return Row;
	}
}

TArray<FPauseMenuPage> WoRPauseMenu::BuildPages(bool bInRun, bool bCanEndRun)
{
	// TODO: B-452 (Settings) takes this refusal away when it lands.
	const FText NotBuilt = LOCTEXT("NotBuilt", "This screen isn't built yet.");
	const FText GuestCantQuit = LOCTEXT("GuestCantQuit", "Only the host can end the run.");

	TArray<FPauseMenuPage> Pages;
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Resume, LOCTEXT("Resume", "Resume"), LOCTEXT("ResumePreview", "Back to the game.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Loadout, LOCTEXT("Loadout", "Loadout"),
					   LOCTEXT("LoadoutPreview", "Weapons, adapters and apparel, their sockets, and your saved loadouts.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Inventory, LOCTEXT("Inventory", "Inventory"),
					   LOCTEXT("InventoryPreview", "Each character's gear and pouch, and the home chest.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Character, LOCTEXT("Character", "Character"),
					   LOCTEXT("CharacterPreview", "Pillars, substats and resistances.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Party, LOCTEXT("Party", "Party"),
					   LOCTEXT("PartyPreview", "Who plays each member, and where they start a fight.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Journal, LOCTEXT("Journal", "Journal"),
					   LOCTEXT("JournalPreview", "Quests, memories, lore and conversations.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Settings, LOCTEXT("Settings", "Settings"),
					   LOCTEXT("SettingsPreview", "Controls, camera, display, audio and accessibility."), NotBuilt));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Quit, LOCTEXT("Quit", "Quit to title"), GetQuitWarning(bInRun),
					   bInRun && !bCanEndRun ? GuestCantQuit : FText::GetEmpty()));
	return Pages;
}

FText WoRPauseMenu::GetQuitWarning(bool bInRun)
{
	return bInRun ? LOCTEXT("QuitWarningRun", "Quitting now ends the run and you lose everything from it: its Prisms, its rewards, all of it. "
													   "The only way out with your rewards is to extract at a rest spot.")
				  : LOCTEXT("QuitWarningHub", "There is no title screen yet, so this saves and closes the game.");
}

FText WoRPauseMenu::GetExtractNote()
{
	return LOCTEXT("ExtractNote", "To leave the trial with your rewards, extract at a rest spot.");
}

bool WoRPauseMenu::IsInRun(const UWorld *World)
{
	const UTrialRunSubsystem *Run = FindPauseMenuRun(World);
	return Run && Run->IsRunActive();
}

FPauseMenuHeader WoRPauseMenu::BuildHeader(const UWorld *World)
{
	FPauseMenuHeader Header;
	const UCurrencyComponent *Wallet = FindPauseMenuWallet(World);
	const auto AddBalance = [&Header, Wallet](ECurrencyType Currency)
	{
		Header.Chips.Add({FText::AsNumber(Wallet ? Wallet->GetBalance(Currency) : 0), WoRGameNames::GetCurrencyName(Currency)});
	};

	const UTrialRunSubsystem *Run = FindPauseMenuRun(World);
	if (!Run || !Run->IsRunActive())
	{
		Header.Title = LOCTEXT("HubTitle", "Void Realm");
		Header.Subtitle = LOCTEXT("HubSubtitle", "The hub");
		AddBalance(ECurrencyType::Prisms); // shown as Gold (D-159)
		AddBalance(ECurrencyType::GearEssence);
		AddBalance(ECurrencyType::SkillEssence);
		return Header;
	}

	Header.bInRun = true;
	const UTrialData *Trial = Run->GetActiveTrial();
	Header.Title = Trial && !Trial->Name.IsEmpty() ? Trial->Name : LOCTEXT("TrialTitle", "Trial");
	const FText Difficulty = TrialGate::GetDifficultyName(Run->GetTrialDifficulty());
	const bool bHasVariation = Trial && Trial->Variations.IsValidIndex(Run->GetVariationIndex());
	Header.Subtitle = bHasVariation ? FText::Format(LOCTEXT("VariationAndDifficulty", "{0} · {1}"),
													 Trial->Variations[Run->GetVariationIndex()].Name, Difficulty)
									: Difficulty;
	AddBalance(ECurrencyType::Gold); // shown as Prisms (D-159)
	AddBalance(ECurrencyType::GearEssence);
	Header.Chips.Add({FText::Format(LOCTEXT("WipesLeftValue", "{0} of {1}"),
									FText::AsNumber(FMath::Max(DefeatRules::MAX_WIPES_PER_RUN - Run->GetRunWipes(), 0)),
									FText::AsNumber(DefeatRules::MAX_WIPES_PER_RUN)),
					  LOCTEXT("WipesLeftLabel", "wipes left")});
	return Header;
}

TArray<FPauseMenuMember> WoRPauseMenu::BuildParty(const UWorld *World)
{
	TArray<FPauseMenuMember> Rows;
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
	if (!Party)
	{
		return Rows;
	}
	for (const FPartyMember &Member : Party->Members)
	{
		const UCharacterData *Data = Member.CharacterData;
		if (!Data)
		{
			continue;
		}
		FPauseMenuMember Row;
		Row.Name = FText::FromString(Data->Name);
		const FText ClassName = WoRGameNames::GetCharacterClassName(Data->CharacterClass);
		Row.ClassLine = Data->HasInnateElement() ? FText::Format(LOCTEXT("ElementAndClass", "{0} {1}"),
																  WoRGameNames::GetElementName(Data->GetElement()), ClassName)
												 : ClassName;
		// As the party strip: a member with no carried vitals (the hub) reads full.
		Row.MaxHP = Data->CalculateMaxHealth();
		Row.MaxEP = Data->CalculateMaxEnergy();
		Row.HP = Member.CurrentHP == PartyConstants::NO_CARRIED_VITALS ? Row.MaxHP : Member.CurrentHP;
		Row.EP = Member.CurrentEP == PartyConstants::NO_CARRIED_VITALS ? Row.MaxEP : Member.CurrentEP;
		Row.Mind = WoRLevelDrops::GetLiveLevel(World, Member, EWorldPillar::Mind);
		Row.Body = WoRLevelDrops::GetLiveLevel(World, Member, EWorldPillar::Body);
		Row.Spirit = WoRLevelDrops::GetLiveLevel(World, Member, EWorldPillar::Spirit);
		if (const UCharacterDataComponent *Spawned = WoRLevelDrops::FindSpawnedMember(World, Member))
		{
			Row.Tier = WoRCombatTier::GetCombatTierText(WoRCombatTier::GetCombatTier(Spawned->GetOwner()));
		}
		Rows.Add(MoveTemp(Row));
	}
	return Rows;
}

void WoRPauseMenu::Quit(UWorld *World, APlayerController *PC)
{
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	if (!GI)
	{
		return;
	}
	UTrialRunSubsystem *Run = GI->GetSubsystem<UTrialRunSubsystem>();
	if (Run && Run->IsRunActive())
	{
		Run->AbandonRun();
		return;
	}
	if (USaveSubsystem *Save = GI->GetSubsystem<USaveSubsystem>())
	{
		Save->SaveNow(World, /*bAsync=*/false);
	}
	UE_LOG(LogWoRUI, Log, TEXT("[PauseMenu] Quit outside a run: saved, closing the game"));
	UKismetSystemLibrary::QuitGame(World, PC, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

FString WoRPauseMenu::GetPauseMenuString(const UWorld *World)
{
	const FPauseMenuHeader Header = BuildHeader(World);
	FString Out = FString::Printf(TEXT("[PauseMenu] %s (%s)%s"), *Header.Title.ToString(), *Header.Subtitle.ToString(),
								  Header.bInRun ? TEXT(", in a run") : TEXT(""));
	for (const FPauseMenuChip &Chip : Header.Chips)
	{
		Out += FString::Printf(TEXT("\n  %s %s"), *Chip.Value.ToString(), *Chip.Label.ToString());
	}
	const bool bCanEndRun = !World || World->GetNetMode() != NM_Client;
	for (const FPauseMenuPage &Page : BuildPages(Header.bInRun, bCanEndRun))
	{
		Out += FString::Printf(TEXT("\n  page %s%s"), *Page.Label.ToString(),
							   Page.CanOpen() ? TEXT("") : *FString::Printf(TEXT(" (refused: %s)"), *Page.Refusal.ToString()));
	}
	for (const FPauseMenuMember &Member : BuildParty(World))
	{
		Out += FString::Printf(TEXT("\n  %s [%s] %s: HP %d/%d, EP %d/%d, Mind %d Body %d Spirit %d"), *Member.Name.ToString(),
							   *Member.Tier.ToString(), *Member.ClassLine.ToString(), Member.HP, Member.MaxHP, Member.EP, Member.MaxEP,
							   Member.Mind, Member.Body, Member.Spirit);
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GPauseMenuPrintCommand(
	TEXT("wor.UI.PrintPause"),
	TEXT("DEBUG (B-439): what the game menu would show now: the place and its money, the pages, the party."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoRUI, Display, TEXT("%s"), *WoRPauseMenu::GetPauseMenuString(World));
	}));
#endif

#undef LOCTEXT_NAMESPACE
