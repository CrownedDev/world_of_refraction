// RunEndDebug.cpp

#include "Debug/Trial/RunEndDebug.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Pool/PoolSubsystem.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRLog.h"

namespace
{
	// TODO: test asset; the same plain sword wor.Debug.TestGear gives.
	const TCHAR *FOUND_WEAPON_DEFAULT_PATH = TEXT("/Game/Data/Pool/Generic/Weapons/Sword/DA_Weapon_ArmingSword.DA_Weapon_ArmingSword");
	constexpr float RUN_END_DEBUG_SCREEN_SECONDS = 8.0f;

	UTrialRunSubsystem *FindRun(UWorld *World)
	{
		UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UTrialRunSubsystem>() : nullptr;
	}

	void ReportRunEnd(const FString &Text) // own name: a unity blob with FieldMovementDebug's Report clashed
	{
		UE_LOG(LogWoR, Log, TEXT("[RunEndDebug] %s"), *Text);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, RUN_END_DEBUG_SCREEN_SECONDS, FColor::Cyan, Text);
		}
	}
}

FString WoRRunEndDebug::EndRun(UWorld *World, const FString &How)
{
	UTrialRunSubsystem *Run = FindRun(World);
	if (!Run || !Run->IsRunActive())
	{
		return TEXT("no run is open");
	}
	if (World->GetNetMode() == NM_Client)
	{
		return TEXT("only the host can end the run");
	}
	if (Run->IsLeavingEarly() || Run->IsAbandoned())
	{
		return TEXT("the run is already ending");
	}
	if (How.Equals(TEXT("win"), ESearchCase::IgnoreCase))
	{
		Run->MarkRunBossBeaten();
		Run->ExitTrialNextTick(); // without a HubLevel it logs an error and the run stays open
		return TEXT("win: the main boss counts as beaten; the run ends next tick (kept)");
	}
	if (How.Equals(TEXT("extract"), ESearchCase::IgnoreCase))
	{
		Run->LeaveRunEarly();
		return Run->IsLeavingEarly() ? TEXT("extract: leaving at a rest spot; the run ends next tick (kept)")
									 : TEXT("extract refused (see the [TrialRun] log line)");
	}
	if (How.Equals(TEXT("wipes"), ESearchCase::IgnoreCase))
	{
		while (!Run->IsRunWipedOut())
		{
			Run->RecordWipe(EEncounterRank::Normal);
		}
		Run->ExitTrialNextTick();
		return FString::Printf(TEXT("wipes: %d wipe(s), out of wipes; the run ends next tick (%s)"), Run->GetRunWipes(),
							   Run->IsRunLost() ? TEXT("lost") : TEXT("kept: the boss was already beaten"));
	}
	if (How.Equals(TEXT("quit"), ESearchCase::IgnoreCase))
	{
		Run->AbandonRun();
		return Run->IsAbandoned() ? TEXT("quit: the run is quit; it ends next tick (quit)")
								  : TEXT("quit refused (see the [TrialRun] log line)");
	}
	return FString::Printf(TEXT("'%s' is not an ending: use win, extract, wipes or quit"), *How);
}

FString WoRRunEndDebug::GrantFoundWeapon(UWorld *World, const FString &AssetPath)
{
	const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn *Pawn = PC ? PC->GetPawn() : nullptr;
	UInventoryComponent *Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	UPoolSubsystem *Pool = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UPoolSubsystem>() : nullptr;
	if (!Inventory || !Pool)
	{
		return TEXT("no played character with an inventory and a pool here");
	}
	if (!Pool->IsRunLayerActive())
	{
		return TEXT("no run is open: a weapon given now would be home gear, not a find");
	}
	const FString Path = AssetPath.IsEmpty() ? FString(FOUND_WEAPON_DEFAULT_PATH) : AssetPath;
	UWeaponData *Weapon = LoadObject<UWeaponData>(nullptr, *Path);
	if (!Weapon)
	{
		return FString::Printf(TEXT("%s is not a weapon asset"), *Path);
	}
	if (!Inventory->AddWeapon(Weapon))
	{
		return FString::Printf(TEXT("%s refused (no room)"), *Weapon->GetName());
	}
	const FWeaponInventoryEntry &Found = Inventory->Weapons.Last();
	return FString::Printf(TEXT("%s found (%s, %s, run-only %s): equip it on the loadout screen"), *Weapon->GetName(),
						   *UEnum::GetValueAsString(Found.Tier), *Found.PersistentID.ToString(),
						   Pool->IsRunOnly(Found.PersistentID) ? TEXT("yes") : TEXT("NO"));
}

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorldAndArgs GEndRunCmd(
		TEXT("wor.Trial.EndRun"),
		TEXT("DEBUG (B-510b): ends the open run through the real end path. <win|extract|wipes|quit>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				ReportRunEnd(WoRRunEndDebug::EndRun(World, Args.IsEmpty() ? FString() : Args[0]));
			}));

	FAutoConsoleCommandWithWorldAndArgs GGrantFoundWeaponCmd(
		TEXT("wor.Trial.GrantFoundWeapon"),
		TEXT("DEBUG (B-510b): the played character finds a weapon in the run (run-only, as trial loot). [asset path], default a plain sword."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				ReportRunEnd(WoRRunEndDebug::GrantFoundWeapon(World, Args.IsEmpty() ? FString() : Args[0]));
			}));
}
#endif // !UE_BUILD_SHIPPING
