// SaveDebug.cpp

#include "Debug/Save/SaveDebug.h"
#include "Save/SaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	USaveSubsystem *GetSave(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
	}

	FAutoConsoleCommandWithWorld GSavePrintCmd(
		TEXT("wor.Save.Print"),
		TEXT("DEBUG: print what the save slot holds (B-090)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { USaveDebug::PrintSaveState(World); }));

	FAutoConsoleCommandWithWorld GSaveNowCmd(
		TEXT("wor.Save.Now"),
		TEXT("DEBUG: capture the party, wallet and pool and write the save slot now (B-090)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			if (USaveSubsystem *Save = GetSave(World))
			{
				Save->ResumeWrites();
				UE_LOG(LogWoR, Display, TEXT("[SaveDebug] wor.Save.Now: %s"),
					   Save->SaveNow(World, false) ? TEXT("written") : TEXT("not written (no local player owns the slot)"));
			}
		}));

	FAutoConsoleCommandWithWorld GSaveDeleteCmd(
		TEXT("wor.Save.Delete"),
		TEXT("DEBUG: delete the save slot; the next start begins fresh (B-090)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			if (USaveSubsystem *Save = GetSave(World))
			{
				Save->DeleteSave();
			}
		}));
}

FString USaveDebug::GetSaveString(const FWoRSaveData &Data)
{
	TArray<FString> Out;
	Out.Add(FString::Printf(TEXT("Party: %s, %d member(s), let AI finish %s"),
							Data.Party.bHasParty ? *Data.Party.DisplayName.ToString() : TEXT("<none>"),
							Data.Party.Members.Num(), Data.Party.bLetAIFinish ? TEXT("on") : TEXT("off")));
	for (const FSavedPartyMember &M : Data.Party.Members)
	{
		Out.Add(FString::Printf(TEXT("  %s  HP %d  EP %d  cell %d/%d  %s"), *M.CharacterData.Id.ToString(), M.CurrentHP,
								M.CurrentEP, static_cast<int32>(M.PreferredRow), M.PreferredColumn,
								M.bPlayerControlled ? TEXT("Player") : TEXT("AI")));
		if (M.CarriedGear.bCarried || M.CarriedAbsorption.bCarried)
		{
			Out.Add(FString::Printf(TEXT("    carried: %d worn attachment(s), primary evolution %s, %d absorption stack(s)"),
									M.CarriedGear.Attachments.Num(), M.CarriedGear.bHasPrimaryEvolution ? TEXT("yes") : TEXT("no"),
									M.CarriedAbsorption.AbsorptionStacks));
		}
	}
	Out.Add(FString::Printf(TEXT("Wallet: %s, %d balance(s)"), Data.Wallet.bHasWallet ? TEXT("banked") : TEXT("empty"),
							Data.Wallet.Balances.Num()));
	for (const FSavedBalance &B : Data.Wallet.Balances)
	{
		Out.Add(FString::Printf(TEXT("  currency %d key %d: %d"), B.Currency, B.SubKey, B.Amount));
	}
	const FSavedPool &P = Data.Pool;
	Out.Add(FString::Printf(TEXT("Pool: %d weapon(s), %d ring(s), %d evolution(s), %d spell(s), %d ability(ies), %d crystal stack(s)"),
							P.Weapons.Num(), P.Rings.Num(), P.Evolutions.Num(), P.Spells.Num(), P.Abilities.Num(), P.Crystals.Num()));
	const FSavedRun &R = Data.Run;
	Out.Add(R.bHasRun
				? FString::Printf(TEXT("Run (B-208): %s  seed %d  variation %d  difficulty %d  %d beaten, %d found, %d memory(ies)  Gold %d"),
								  *R.Trial.Path.ToString(), R.RunSeed, R.VariationIndex, R.Difficulty, R.BeatenEncounters.Num(),
								  R.FoundFeatures.Num(), R.RunMemories.Num(), R.RunGold)
				: FString(TEXT("Run (B-208): none (a reload starts in the hub)")));
	return FString::Join(Out, TEXT("\n"));
}

void USaveDebug::PrintSaveState(const UObject *WorldContextObject)
{
	UWorld *World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const USaveSubsystem *Save = GetSave(World);
	if (!Save)
	{
		UE_LOG(LogWoR, Warning, TEXT("[SaveDebug] wor.Save.Print: no save subsystem."));
		return;
	}
	UE_LOG(LogWoR, Display, TEXT("[SaveDebug] Slot '%s' (%s)\n%s"), USaveSubsystem::SLOT_NAME,
		   Save->IsBoundToPlayer() ? TEXT("on disk") : TEXT("in memory only"), *GetSaveString(Save->GetData()));
}
