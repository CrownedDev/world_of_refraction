// PauseMenuModel.cpp

#include "UI/Menus/PauseMenuModel.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/EWorldPillar.h"
#include "Combat/Tier/CombatTier.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Currency/EconomyService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Memory/HiddenQuestSubsystem.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRFrontEndGameMode.h"
#include "Player/WoRPlayerState.h"
#include "Save/SaveSubsystem.h"
#include "Trial/DefeatRules.h"
#include "Trial/LevelDropPlacement.h"
#include "Trial/TrialData.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialRunSubsystem.h"
#include "Character/PlayerBuild.h"
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

FText WoRPauseMenu::GetTrackedLine(const UWorld *World)
{
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UHiddenQuestSubsystem *Quests = GI ? GI->GetSubsystem<UHiddenQuestSubsystem>() : nullptr;
	return Quests ? Quests->GetTrackedLine() : FText::GetEmpty();
}

TArray<FPauseMenuPage> WoRPauseMenu::BuildPages(bool bInRun, bool bCanEndRun, const FText &TrackedLine, bool bHasFrontEnd)
{
	const FText GuestCantQuit = LOCTEXT("GuestCantQuit", "Only the host can end the run.");

	TArray<FPauseMenuPage> Pages;
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Resume, LOCTEXT("Resume", "Resume"), LOCTEXT("ResumePreview", "Back to the game.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Loadout, LOCTEXT("Loadout", "Loadout"),
					   LOCTEXT("LoadoutPreview", "Gear, sockets and saved loadouts.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Inventory, LOCTEXT("Inventory", "Inventory"),
					   LOCTEXT("InventoryPreview", "Gear, pouch and chest.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Character, LOCTEXT("Character", "Character"),
					   LOCTEXT("CharacterPreview", "Pillars, substats and resistances.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Forms, LOCTEXT("Body", "Body"), // D-241: was "Spirit" (D-234: was "Forms")
					   LOCTEXT("BodyPreview", "Wear Male Nobody, Female Nobody or a body you have unlocked.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Party, LOCTEXT("Party", "Party"),
					   LOCTEXT("PartyPreview", "Who plays each member, and where they start a fight.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Journal, LOCTEXT("Journal", "Journal"),
					   TrackedLine.IsEmpty() ? LOCTEXT("JournalPreview", "Quests, memories, lore and conversations.")
											 : FText::Format(LOCTEXT("JournalTracked", "Tracking: {0}"), TrackedLine)));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Settings, LOCTEXT("Settings", "Settings"),
					   LOCTEXT("SettingsPreview", "Controls, camera, display, audio and accessibility.")));
	Pages.Add(MakePauseMenuPage(EPauseMenuPage::Quit, LOCTEXT("Quit", "Quit to title"), GetQuitWarning(bInRun, bHasFrontEnd),
					   bInRun && !bCanEndRun ? GuestCantQuit : FText::GetEmpty()));
	return Pages;
}

TArray<FPauseMenuPage> WoRPauseMenu::BuildPages(bool bInRun, bool bCanEndRun, const FText &TrackedLine, bool bHasFrontEnd, bool bCanJoin, bool bSeated)
{
	TArray<FPauseMenuPage> Pages = BuildPages(bInRun, bCanEndRun, TrackedLine, bHasFrontEnd);
	if (bCanJoin)
	{
		Pages.Insert(MakePauseMenuPage(EPauseMenuPage::Join, LOCTEXT("Join", "Join the party"),
						 LOCTEXT("JoinPreview", "Take a seat in your friend's party as one of your characters, or a new one."),
						 bSeated ? LOCTEXT("JoinSeated", "You already play a member of this party.") : FText::GetEmpty()),
					 FMath::Max(0, Pages.IndexOfByPredicate([](const FPauseMenuPage &Page) { return Page.Page == EPauseMenuPage::Quit; })));
	}
	return Pages;
}

FText WoRPauseMenu::GetQuitWarning(bool bInRun, bool bHasFrontEnd)
{
	return bInRun ? LOCTEXT("QuitWarningRun", "Quitting ends the run. You lose its Prisms and rewards. "
													   "The only way out with your rewards is to extract at a rest spot left where a great foe fell.")
		 : bHasFrontEnd ? LOCTEXT("QuitWarningToTitle", "This saves and goes back to the title.")
				  : LOCTEXT("QuitWarningHub", "There is no title screen yet, so this saves and closes the game.");
}

FText WoRPauseMenu::GetExtractNote()
{
	return LOCTEXT("ExtractNote", "To leave the trial with your rewards, extract at a rest spot left where a great foe fell.");
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
	// B-543 (Crown): every balance, the place's money first (Gold in the hub, Prisms in a run, D-159).
	const auto AddBalances = [&Header, Wallet, World]()
	{
		for (const ECurrencyType Currency : {UEconomyService::GetPlaceCurrency(World), ECurrencyType::Diamond, ECurrencyType::GearEssence,
											 ECurrencyType::ElementalEssence, ECurrencyType::PillarEssence}) // B-550: one Dormant chip
		{
			Header.Chips.Add({FText::AsNumber(Wallet ? Wallet->GetBalance(Currency) : 0), WoRGameNames::GetCurrencyName(Currency)});
		}
	};

	const UTrialRunSubsystem *Run = FindPauseMenuRun(World);
	if (!Run || !Run->IsRunActive())
	{
		Header.Title = WoRGameNames::GetHubPlaceName(); // no subtitle in the hub (D-236)
		AddBalances();
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
	AddBalances();
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
		Row.Name = WoRPlayerBuild::GetCardName(Data, Member.MemberId, World); // D-229: You or the player's tag
		const FText ClassName = WoRGameNames::GetCharacterClassName(Data->CharacterClass);
		// D-241: a player character reads as its identity (Spirit of ... / Body of ...), on the party list and its sheet.
		Row.ClassLine = WoRPlayerBuild::IsPlayerCharacter(Data) ? WoRPlayerBuild::GetDetailLine(Data, Member.MemberId, World)
						: Data->HasInnateElement() ? FText::Format(LOCTEXT("ElementAndClass", "{0} {1}"),
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
	if (WoRFrontEnd::GetQuitTarget(/*bInRun=*/false, WoRFrontEnd::HasFrontEnd(GI)) == WoRFrontEnd::EQuitTo::FrontEnd &&
		WoRFrontEnd::TravelToFrontEnd(World))
	{
		UE_LOG(LogWoRUI, Log, TEXT("[PauseMenu] Quit outside a run: saved, back to the title"));
		return;
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
	const FText Tracked = GetTrackedLine(World);
	if (!Tracked.IsEmpty())
	{
		Out += TEXT("\n  tracking: ") + Tracked.ToString();
	}
	for (const FPauseMenuPage &Page : BuildPages(Header.bInRun, bCanEndRun, Tracked, WoRFrontEnd::HasFrontEnd(World ? World->GetGameInstance() : nullptr)))
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
