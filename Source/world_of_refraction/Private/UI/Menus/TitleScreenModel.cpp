// TitleScreenModel.cpp

#include "UI/Menus/TitleScreenModel.h"

#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Trial/TrialData.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRTitleScreen"

namespace
{
	constexpr double SECONDS_PER_MINUTE = 60.0;
	constexpr double SECONDS_PER_HOUR = 3600.0;
}

TArray<FTitleScreenRow> WoRTitleScreen::BuildRows(const FTitleSaveSummary &Save)
{
	TArray<FTitleScreenRow> Rows;
	if (Save.bHasSave)
	{
		TArray<FText> Parts;
		Parts.Add(Save.Place);
		if (!Save.PartyNames.IsEmpty())
		{
			Parts.Add(FText::Join(LOCTEXT("NameSeparator", ", "), Save.PartyNames));
		}
		Parts.Add(GetPlayTimeText(Save.PlaySeconds));
		Rows.Add({ETitleScreenRow::Continue, LOCTEXT("Continue", "Continue"), FText::Join(LOCTEXT("LineSeparator", "  ·  "), Parts)});
	}
	Rows.Add({ETitleScreenRow::NewGame, LOCTEXT("NewGame", "New Game")});
	// TODO: B-459 builds joining a friend's game; until then the row says so.
	Rows.Add({ETitleScreenRow::JoinFriend, LOCTEXT("JoinFriend", "Join a friend"), FText::GetEmpty(),
			  LOCTEXT("JoinNotBuilt", "Joining from here comes with the co-op screens.")});
	Rows.Add({ETitleScreenRow::Settings, LOCTEXT("Settings", "Settings")});
	// TODO: B-470 builds the credits.
	Rows.Add({ETitleScreenRow::Credits, LOCTEXT("Credits", "Credits"), FText::GetEmpty(),
			  LOCTEXT("CreditsNotBuilt", "The credits aren't on this screen yet.")});
	Rows.Add({ETitleScreenRow::Quit, LOCTEXT("Quit", "Quit")});
	return Rows;
}

FTitleSaveSummary WoRTitleScreen::Summarize(const FWoRSaveData &Data, bool bHasSave, double PlaySeconds)
{
	FTitleSaveSummary Summary;
	Summary.bHasSave = bHasSave;
	Summary.PlaySeconds = PlaySeconds;
	const UTrialData *Trial = Data.Run.bHasRun ? Data.Run.Trial.ResolveAs<UTrialData>() : nullptr;
	Summary.Place = Trial && !Trial->Name.IsEmpty() ? Trial->Name : LOCTEXT("PlaceHub", "The hub");
	for (const FSavedPartyMember &Member : Data.Party.Members)
	{
		if (const UCharacterData *Character = Member.CharacterData.ResolveAs<UCharacterData>())
		{
			Summary.PartyNames.Add(FText::FromString(Character->Name));
		}
	}
	return Summary;
}

FTitleSaveSummary WoRTitleScreen::SummarizeSave(const UGameInstance *GameInstance)
{
	const USaveSubsystem *Saves = GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
	return Saves ? Summarize(Saves->GetData(), Saves->HasSave(), Saves->GetPlaySeconds()) : FTitleSaveSummary();
}

FText WoRTitleScreen::GetPlayTimeText(double PlaySeconds)
{
	if (PlaySeconds < SECONDS_PER_HOUR)
	{
		return FText::Format(LOCTEXT("PlayMinutes", "{0} min"), FMath::FloorToInt32(PlaySeconds / SECONDS_PER_MINUTE));
	}
	return FText::Format(LOCTEXT("PlayHours", "{0} h"), FMath::FloorToInt32(PlaySeconds / SECONDS_PER_HOUR));
}

FText WoRTitleScreen::GetNewGameWarning(const FTitleSaveSummary &Save)
{
	return FText::Format(LOCTEXT("NewGameWarning", "There is one save. Starting over replaces it: {0}, {1} {1}|plural(one=character,other=characters), everything in the home chest."),
						 GetPlayTimeText(Save.PlaySeconds), Save.PartyNames.Num());
}

FText WoRTitleScreen::GetOldVersionText(int32 WipedFromVersion)
{
	return FText::Format(LOCTEXT("OldVersion", "version {0}"), WipedFromVersion);
}

FText WoRTitleScreen::GetNeededVersionText()
{
	return FText::Format(LOCTEXT("NeededVersion", "version {0} or newer"), WoRSaveVersion::MIN_SUPPORTED);
}

FString WoRTitleScreen::GetTitleScreenString(const UGameInstance *GameInstance)
{
	const FTitleSaveSummary Save = SummarizeSave(GameInstance);
	FString Out = FString::Printf(TEXT("[Title] save: %s"), Save.bHasSave ? TEXT("yes") : TEXT("none"));
	const USaveSubsystem *Saves = GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
	if (Saves && Saves->WasWiped())
	{
		Out += FString::Printf(TEXT(" (a version %d save was reset)"), Saves->GetWipedFromVersion());
	}
	for (const FTitleScreenRow &Row : BuildRows(Save))
	{
		Out += FString::Printf(TEXT("\n  %s"), *Row.Label.ToString());
		if (!Row.Line.IsEmpty())
		{
			Out += FString::Printf(TEXT("  |  %s"), *Row.Line.ToString());
		}
		if (!Row.CanUse())
		{
			Out += FString::Printf(TEXT("  (refused: %s)"), *Row.Refusal.ToString());
		}
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GTitlePrintCommand(
	TEXT("wor.UI.PrintTitle"),
	TEXT("B-467: log the title screen's rows and what it says about the save."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoRUI, Log, TEXT("%s"), *WoRTitleScreen::GetTitleScreenString(World ? World->GetGameInstance() : nullptr));
	}));
#endif

#undef LOCTEXT_NAMESPACE
