// HubHUDModel.cpp

#include "UI/Hub/HubHUDModel.h"

#include "Currency/CurrencyTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Hub/HubInteract.h"
#include "Trial/DefeatRules.h"
#include "Trial/TrialRunSubsystem.h"
#include "Engine/GameInstance.h"
#include "UI/Menus/PauseMenuModel.h"
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
	// InputMap.md: Interact is E / X (Square). TODO: glyphs per pad brand with the CommonUI input data.
	return bGamepad ? LOCTEXT("InteractKeyGamepad", "X") : LOCTEXT("InteractKeyKeyboard", "E");
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

FTrialHUDStatus WoRHubHUD::BuildTrialStatus(const UWorld *World)
{
	FTrialHUDStatus Status;
	Status.bInRun = WoRPauseMenu::IsInRun(World);
	if (!Status.bInRun)
	{
		return Status;
	}
	Status.Prisms = BuildMoney(World);
	const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
	const UTrialRunSubsystem *TrialRun = GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	Status.WipesMax = DefeatRules::MAX_WIPES_PER_RUN;
	Status.WipesLeft = FMath::Clamp(Status.WipesMax - (TrialRun ? TrialRun->GetRunWipes() : 0), 0, Status.WipesMax);
	Status.WipesLine = FText::Format(LOCTEXT("WipesLeft", "Wipes left {0} of {1}"), FText::AsNumber(Status.WipesLeft), FText::AsNumber(Status.WipesMax));
	Status.QuestLine = BuildQuestLine(World);
	return Status;
}

FString WoRHubHUD::GetHubHUDString(const UWorld *World)
{
	const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
	const FHubHUDPrompt Prompt = BuildPrompt(PC ? PC->GetPawn() : nullptr);
	const FHubHUDMoney Money = BuildMoney(World);
	const FText Quest = BuildQuestLine(World);
	const FTrialHUDStatus Trial = BuildTrialStatus(World);
	return FString::Printf(TEXT("[HubHUD] prompt: %s; near a vendor: %s; money: %s %s; quest: %s"),
						   Prompt.bShown ? *FString::Printf(TEXT("%s %s"), *Prompt.Verb.ToString(), *Prompt.What.ToString()) : TEXT("none"),
						   Prompt.bNearVendor ? TEXT("yes") : TEXT("no"), *Money.Value.ToString(), *Money.Label.ToString(),
						   Quest.IsEmpty() ? TEXT("none") : *Quest.ToString())
		+ (Trial.bInRun ? FString::Printf(TEXT("; trial: %s %s, %s"), *Trial.Prisms.Value.ToString(), *Trial.Prisms.Label.ToString(),
										  *Trial.WipesLine.ToString())
						: FString());
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
