// GateScreenModel.cpp

#include "UI/Trial/GateScreenModel.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Memory/HiddenQuestSubsystem.h"
#include "Trial/DefeatRules.h"
#include "Trial/TrialData.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Party/PartyScreenModel.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRGateScreen"

namespace GateScreenBuild
{
	/** "x0.8", "x1.0", "x0.75". */
	constexpr int32 MULTIPLIER_MIN_DIGITS = 1;
	constexpr int32 MULTIPLIER_MAX_DIGITS = 2;

	FText Multiplier(float Value)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = MULTIPLIER_MIN_DIGITS;
		Options.MaximumFractionalDigits = MULTIPLIER_MAX_DIGITS;
		return FText::Format(LOCTEXT("Multiplier", "x{0}"), FText::AsNumber(Value, &Options));
	}
}

TArray<UTrialData *> WoRGateScreen::GatherGates()
{
	const IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UTrialData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);
	Assets.Sort([](const FAssetData &A, const FAssetData &B) { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });
	TArray<UTrialData *> Gates;
	for (const FAssetData &Asset : Assets)
	{
		UTrialData *Gate = Cast<UTrialData>(Asset.GetAsset());
		if (Gate && !Gate->Level.IsNull())
		{
			Gates.Add(Gate);
		}
	}
	return Gates;
}

bool WoRGateScreen::IsUnlocked(const UGameInstance *GameInstance, const FTrialVariation &Trial)
{
	if (Trial.UnlockQuestId.IsNone())
	{
		return true;
	}
	const UHiddenQuestSubsystem *Quests = GameInstance ? GameInstance->GetSubsystem<UHiddenQuestSubsystem>() : nullptr;
	return Quests && Quests->IsFinished(Trial.UnlockQuestId);
}

int32 WoRGateScreen::GetFirstOpenTrial(const UGameInstance *GameInstance, const UTrialData *Gate)
{
	if (!Gate)
	{
		return INDEX_NONE;
	}
	return Gate->Variations.IndexOfByPredicate([GameInstance](const FTrialVariation &Trial) { return IsUnlocked(GameInstance, Trial); });
}

FGateScreenPage WoRGateScreen::BuildPage(UWorld *World, const TArray<UTrialData *> &Gates, const UTrialData *Gate)
{
	FGateScreenPage Page;
	const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
	for (const UTrialData *Each : Gates)
	{
		Page.Gates.Add(Each && !Each->Name.IsEmpty() ? Each->Name : LOCTEXT("UnnamedGate", "Gate"));
	}
	if (Gate)
	{
		for (const FTrialVariation &Trial : Gate->Variations)
		{
			FGateTrialRow &Row = Page.Trials.AddDefaulted_GetRef();
			Row.bLocked = !IsUnlocked(GameInstance, Trial);
			// D-191: never the boss; a locked trial keeps its name until it opens.
			Row.Name = Row.bLocked ? LOCTEXT("LockedName", "???") : Trial.Name;
			Row.Line = !Row.bLocked					? Trial.Description
					   : !Trial.UnlockHint.IsEmpty() ? Trial.UnlockHint
													 : LOCTEXT("LockedLine", "Unlocks later in your journey.");
		}
	}
	for (int32 Index = 0; Index < TrialGate::DIFFICULTY_COUNT; ++Index)
	{
		const ETrialDifficulty Difficulty = static_cast<ETrialDifficulty>(Index);
		FGateDifficultyCard &Card = Page.Difficulties.AddDefaulted_GetRef();
		Card.Name = TrialGate::GetDifficultyName(Difficulty);
		Card.DamageLine = FText::Format(LOCTEXT("DamageLine", "Enemy damage {0}"),
										GateScreenBuild::Multiplier(TrialGate::GetEnemyStrengthMultiplier(Difficulty)));
		Card.PayoutLine = FText::Format(LOCTEXT("PayoutLine", "Payout {0}"), GateScreenBuild::Multiplier(TrialGate::GetPayoutMultiplier(Difficulty)));
		Card.bPaysMore = TrialGate::GetPayoutMultiplier(Difficulty) > TrialGate::GetPayoutMultiplier(TrialGate::DEFAULT_DIFFICULTY);
	}
	for (const FPartyScreenMember &Member : WoRPartyScreen::BuildPage(World).Members)
	{
		FGateMember &Row = Page.Members.AddDefaulted_GetRef();
		Row.Name = Member.Name;
		Row.ClassName = Member.ClassName;
		Row.PlayedBy = Member.bAISeat ? FText::GetEmpty() : LOCTEXT("You", "You"); // D-191: an AI seat shows nothing
	}
	Page.WipesLine = FText::Format(LOCTEXT("WipesLine", "{0}  ({1} in the final fight)"), FText::AsNumber(DefeatRules::MAX_WIPES_PER_RUN),
								   FText::AsNumber(DefeatRules::MAX_MAIN_BOSS_LOSSES_PER_RUN));
	Page.DifficultyNote = LOCTEXT("DifficultyNote", "Enemies hit harder and fight smarter; they don't get more HP. You can't change this once inside.");
	Page.RunNote = LOCTEXT("RunNote", "To leave a trial with your rewards, extract at a rest spot left where a great foe fell.");
	return Page;
}

FText WoRGateScreen::GetEnterRefusal(const UGameInstance *GameInstance, const UTrialData *Gate, int32 TrialIndex)
{
	if (!Gate || Gate->Level.IsNull())
	{
		return LOCTEXT("NoLevel", "This gate has no way in yet.");
	}
	if (Gate->Variations.IsEmpty())
	{
		return FText::GetEmpty(); // a gate with no trials asks for the difficulty only
	}
	if (!Gate->Variations.IsValidIndex(TrialIndex))
	{
		return LOCTEXT("NoTrial", "Pick a trial first.");
	}
	return IsUnlocked(GameInstance, Gate->Variations[TrialIndex]) ? FText::GetEmpty() : LOCTEXT("Locked", "That trial is still locked.");
}

FText WoRGateScreen::Enter(UGameInstance *GameInstance, UTrialData *Gate, int32 TrialIndex, ETrialDifficulty Difficulty)
{
	const FText Refusal = GetEnterRefusal(GameInstance, Gate, TrialIndex);
	if (!Refusal.IsEmpty())
	{
		return Refusal;
	}
	UTrialRunSubsystem *TrialRun = GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	if (!TrialRun)
	{
		return LOCTEXT("NoRun", "The trial can't start right now.");
	}
	UE_LOG(LogWoR, Log, TEXT("[GateScreen] Enter %s, trial %d, %s"), *Gate->GetName(), TrialIndex,
		   *TrialGate::GetDifficultyName(Difficulty).ToString());
	TrialRun->SetGateChoice(Gate->Variations.IsEmpty() ? INDEX_NONE : TrialIndex, Difficulty);
	TrialRun->EnterTrial(Gate);
	return FText::GetEmpty();
}

FString WoRGateScreen::GetGateScreenString(UWorld *World)
{
	const TArray<UTrialData *> Gates = GatherGates();
	FString Out = FString::Printf(TEXT("[GateScreen] %d gate(s)"), Gates.Num());
	for (const UTrialData *Gate : Gates)
	{
		const FGateScreenPage Page = BuildPage(World, Gates, Gate);
		Out += FString::Printf(TEXT("\n  %s:"), *Gate->Name.ToString());
		for (const FGateTrialRow &Row : Page.Trials)
		{
			Out += FString::Printf(TEXT("\n    %s%s: %s"), Row.bLocked ? TEXT("[locked] ") : TEXT(""), *Row.Name.ToString(), *Row.Line.ToString());
		}
	}
	const FGateScreenPage Any = BuildPage(World, Gates, nullptr);
	for (const FGateMember &Member : Any.Members)
	{
		Out += FString::Printf(TEXT("\n  Going in: %s, %s%s"), *Member.Name.ToString(), *Member.ClassName.ToString(),
							   Member.PlayedBy.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Member.PlayedBy.ToString()));
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GGatePrintCommand(
	TEXT("wor.UI.PrintGate"),
	TEXT("B-466: log the Gate screen: every gate, its trials (locked ones and how they unlock) and who goes in."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UE_LOG(LogWoR, Display, TEXT("%s"), *WoRGateScreen::GetGateScreenString(World)); }));
#endif

#undef LOCTEXT_NAMESPACE
