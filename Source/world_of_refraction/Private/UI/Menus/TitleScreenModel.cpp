// TitleScreenModel.cpp

#include "UI/Menus/TitleScreenModel.h"

#include "Character/CharacterData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Player/WoRPlayerState.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Trial/LevelDropPlacement.h"
#include "Trial/TrialData.h"
#include "UI/Menus/NewGameScreenModel.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRTitleScreen"

namespace
{
	constexpr double SECONDS_PER_MINUTE = 60.0;
	constexpr double SECONDS_PER_HOUR = 3600.0;
	const TCHAR *TITLE_CONFIG_SECTION = TEXT("WoR.Title");
	const TCHAR *MAX_SLOTS_KEY = TEXT("MaxCharacterSlots");
}

int32 WoRTitleScreen::GetMaxSlots()
{
	int32 Configured = 0;
	if (GConfig)
	{
		GConfig->GetInt(TITLE_CONFIG_SECTION, MAX_SLOTS_KEY, Configured, GGameIni);
	}
	return Configured > 0 ? Configured : WoRNewGame::GetClassChoiceCount();
}

bool WoRTitleScreen::ShouldOpenAtStart(bool bEnabled, bool bAutomationTesting, const FString &MapPackage, const FString &DefaultMapPackage)
{
	return bEnabled && !bAutomationTesting && !MapPackage.IsEmpty() && MapPackage == DefaultMapPackage;
}

TArray<FTitleScreenRow> WoRTitleScreen::BuildRows(const FTitleSaveSummary &Save, int32 MaxSlots)
{
	TArray<FTitleSaveSummary> Slots;
	if (Save.bHasSave)
	{
		Slots.Add(Save);
	}
	return BuildSlotRows(Slots, MaxSlots);
}

TArray<FTitleScreenRow> WoRTitleScreen::BuildSlotRows(const TArray<FTitleSaveSummary> &Saves, int32 MaxSlots)
{
	const int32 Slots = MaxSlots == INDEX_NONE ? GetMaxSlots() : MaxSlots;
	TArray<FTitleScreenRow> Rows;
	int32 Filled = 0;
	for (const FTitleSaveSummary &Save : Saves)
	{
		if (!Save.bHasSave || Filled >= Slots)
		{
			continue;
		}
		// B-546 (Crown): a slot shows the name and the last run's levels, nothing else.
		const FText Name = Save.LeaderName.IsEmpty() ? LOCTEXT("Continue", "Continue") : Save.LeaderName;
		FTitleScreenRow Row{ETitleScreenRow::Continue, Name, GetRunLevelsText(Save)};
		Row.Slot = Save.Slot;
		Rows.Add(Row);
		++Filled;
	}
	if (Filled < Slots)
	{
		Rows.Add({ETitleScreenRow::NewGame, LOCTEXT("NewSlot", "+"), LOCTEXT("NewSlotLine", "New character")});
	}
	FTitleScreenRow Settings{ETitleScreenRow::Settings, LOCTEXT("Settings", "Settings")};
	FTitleScreenRow Credits{ETitleScreenRow::Credits, LOCTEXT("Credits", "Credits")};
	FTitleScreenRow Quit{ETitleScreenRow::Quit, LOCTEXT("Quit", "Quit")};
	for (FTitleScreenRow *Button : {&Settings, &Credits, &Quit})
	{
		Button->bButton = true;
		Rows.Add(*Button);
	}
	return Rows;
}

TArray<FTitleScreenRow> WoRTitleScreen::BuildJoinRows(const TArray<FTitleSaveSummary> &Slots, int32 MaxSlots)
{
	TArray<FTitleScreenRow> Rows = BuildSlotRows(Slots, MaxSlots);
	Rows.RemoveAll([](const FTitleScreenRow &Row) { return Row.bButton; });
	return Rows;
}

bool WoRTitleScreen::FindSavedPicks(const FWoRSaveData &Data, FPlayerPicks &OutPicks)
{
	const FSavedPartyMember *Member = Data.Party.Members.FindByPredicate([](const FSavedPartyMember &Saved) { return Saved.Picks.IsSet(); });
	if (Member)
	{
		OutPicks = Member->Picks;
	}
	return Member != nullptr;
}

bool WoRTitleScreen::ReadSlotPicks(const UGameInstance *GameInstance, int32 Slot, FPlayerPicks &OutPicks)
{
	const USaveSubsystem *Saves = GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
	FWoRSaveData Data;
	double PlaySeconds = 0.0;
	return Saves && Saves->ReadSlot(Slot, Data, PlaySeconds) && FindSavedPicks(Data, OutPicks);
}

bool WoRTitleScreen::SendJoin(APlayerController *PC, const FPlayerPicks &Picks)
{
	AWoRPlayerState *Player = PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr;
	if (!Player || !Picks.IsSet())
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[Title] Join not sent: %s"), Player ? TEXT("no character picked") : TEXT("no player state"));
		return false;
	}
	UE_LOG(LogWoRUI, Log, TEXT("[Title] Asking the host to join as %s"), *Picks.Name);
	Player->RequestJoinWithPicks(Picks);
	return true;
}

FTitleSaveSummary WoRTitleScreen::Summarize(const FWoRSaveData &Data, bool bHasSave, double PlaySeconds, bool bRunSettled)
{
	FTitleSaveSummary Summary;
	Summary.bHasSave = bHasSave;
	Summary.PlaySeconds = PlaySeconds;
	// B-408a (D-172, Q-408): a run in the save is settled as a quit, never resumed: Continue is always the hub.
	Summary.Place = WoRGameNames::GetHubPlaceName();
	Summary.bLastRunLost = bHasSave && (Data.Run.bHasRun || bRunSettled);
	for (const FSavedPartyMember &Member : Data.Party.Members)
	{
		// B-554 (D-228): a built player's name is its picks' (its saved path is the built copy's, not an asset).
		const UCharacterData *Character = Member.Picks.IsSet() ? nullptr : Member.CharacterData.ResolveAs<UCharacterData>();
		if (Character || Member.Picks.IsSet())
		{
			const FText Name = FText::FromString(Member.Picks.IsSet() ? Member.Picks.Name : Character->Name);
			Summary.PartyNames.Add(Name);
			if (Summary.LeaderName.IsEmpty()) // the first resolved member leads (B-529)
			{
				Summary.LeaderName = Name;
				// The run keys a character by its path, "#member id" after it for a copy (UPoolSubsystem::GetCharacterKey);
				// a built player's path is its fixed built name (WoRPlayerBuild::BUILT_NAME).
				// shortcut: the first entry for the leader's path.
				const FString LeaderPath = Character ? Character->GetPathName() : Member.CharacterData.Path.ToString();
				const FSavedRunLevels *Levels = Data.Run.bHasRun
					? Data.Run.RunLevels.FindByPredicate([&LeaderPath](const FSavedRunLevels &Entry)
					{
						const FString Key = Entry.CharacterKey.ToString();
						return Key == LeaderPath || Key.StartsWith(LeaderPath + TEXT("#"));
					})
					: nullptr;
				if (Member.bHasKeptLevels)
				{
					// B-555 (D-237): the levels kept from the last finished run (a run left open is settled as a quit,
					// which keeps these).
					Summary.bHasRunLevels = true;
					Summary.RunMind = Member.KeptMind;
					Summary.RunBody = Member.KeptBody;
					Summary.RunSpirit = Member.KeptSpirit;
				}
				else if (Levels)
				{
					Summary.bHasRunLevels = true;
					Summary.RunMind = Levels->Mind;
					Summary.RunBody = Levels->Body;
					Summary.RunSpirit = Levels->Spirit;
				}
			}
		}
	}
	return Summary;
}

FTitleSaveSummary WoRTitleScreen::SummarizeSave(const UGameInstance *GameInstance)
{
	const USaveSubsystem *Saves = GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
	if (!Saves)
	{
		return FTitleSaveSummary();
	}
	// A handed-over copy's run resumes (Q-208), so it is not lost: only the own slot's run is settled as a quit.
	const bool bOwnRunInSave = Saves->GetData().Run.bHasRun && !Saves->IsPlayingHostCopy();
	FTitleSaveSummary Summary = Summarize(Saves->GetData(), Saves->HasSave(), Saves->GetPlaySeconds(), Saves->WasClosedRunSettled());
	Summary.bLastRunLost = Saves->HasSave() && (bOwnRunInSave || Saves->WasClosedRunSettled());
	return Summary;
}

TArray<FTitleSaveSummary> WoRTitleScreen::SummarizeSlots(const UGameInstance *GameInstance)
{
	TArray<FTitleSaveSummary> Slots;
	const USaveSubsystem *Saves = GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
	if (!Saves)
	{
		return Slots;
	}
	const int32 MaxSlots = GetMaxSlots();
	for (int32 Index = 0; Index < MaxSlots; ++Index)
	{
		FTitleSaveSummary Summary;
		if (Index == Saves->GetActiveSlot() && Saves->IsBoundToPlayer())
		{
			Summary = SummarizeSave(GameInstance);
		}
		else
		{
			FWoRSaveData Data;
			double PlaySeconds = 0.0;
			if (!Saves->ReadSlot(Index, Data, PlaySeconds))
			{
				continue;
			}
			Summary = Summarize(Data, /*bHasSave=*/true, PlaySeconds);
		}
		if (Summary.bHasSave)
		{
			Summary.Slot = Index;
			Slots.Add(Summary);
		}
	}
	return Slots;
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

FText WoRTitleScreen::GetDeleteWarning(const FTitleSaveSummary &Save)
{
	return FText::Format(LOCTEXT("DeleteWarning", "Delete {0}? {1}, everything in the home chest. This can't be undone."),
						 Save.LeaderName.IsEmpty() ? LOCTEXT("ThisCharacter", "this character") : Save.LeaderName, GetPlayTimeText(Save.PlaySeconds));
}

FText WoRTitleScreen::GetRunLevelsText(const FTitleSaveSummary &Save)
{
	const FText NoLevel = LOCTEXT("NoRunLevel", "-");
	auto Level = [&Save, &NoLevel](int32 Value) { return Save.bHasRunLevels ? FText::AsNumber(Value) : NoLevel; };
	return FText::Format(LOCTEXT("RunLevels", "{0} {1}  ·  {2} {3}  ·  {4} {5}"),
						 WoRLevelDrops::GetPillarName(EWorldPillar::Mind), Level(Save.RunMind),
						 WoRLevelDrops::GetPillarName(EWorldPillar::Body), Level(Save.RunBody),
						 WoRLevelDrops::GetPillarName(EWorldPillar::Spirit), Level(Save.RunSpirit));
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
	Out += FString::Printf(TEXT(", most slots %d"), GetMaxSlots());
	for (const FTitleScreenRow &Row : BuildSlotRows(SummarizeSlots(GameInstance)))
	{
		Out += FString::Printf(TEXT("\n  %s%s"), Row.bButton ? TEXT("[button] ") : TEXT(""), *Row.Label.ToString());
		if (Row.Slot != INDEX_NONE)
		{
			Out += FString::Printf(TEXT(" (slot %d)"), Row.Slot);
		}
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
