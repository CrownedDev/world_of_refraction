// CombatCameraDebug.cpp

#include "Debug/Combat/CombatCameraDebug.h"
#include "Combat/Camera/CombatCameraDirector.h"
#include "Combat/CombatOrchestrator.h"
#include "LevelSequence.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float COMBAT_CAMERA_SCREEN_SECONDS = 8.0f;

	FAutoConsoleCommandWithWorld GPrintCombatCameraCmd(
		TEXT("wor.Camera"),
		TEXT("DEBUG: print the combat camera's mode, viewed character, settings and playing sequence."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				UCombatCameraDebug::PrintCameraState(World);
			}));

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraStyleCmd(
		TEXT("wor.Camera.Style"),
		TEXT("DEBUG: turn-view style. 0 = centred-wide (default), 1 = over-the-shoulder, 2 = tactical side-on."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				UCombatCameraDirector *Director = UCombatCameraDebug::FindDirector(World);
				const int32 Style = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
				if (!Director || Style < 0 || Style > static_cast<int32>(ETurnViewStyle::Tactical))
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] wor.Camera.Style: no combat orchestrator, or style not 0-2."));
					return;
				}
				Director->SetTurnViewStyle(static_cast<ETurnViewStyle>(Style));
				UCombatCameraDebug::PrintCameraState(World);
			}));

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraMovementOffCmd(
		TEXT("wor.Camera.MovementOff"),
		TEXT("DEBUG: 1 = camera movement off (one static wide view; no sequences unless wor.Camera.Cinematics is 1), 0 = on."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				UCombatCameraDirector *Director = UCombatCameraDebug::FindDirector(World);
				if (!Director)
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] wor.Camera.MovementOff: no combat orchestrator."));
					return;
				}
				Director->SetCameraMovementOff(Args.Num() > 0 && FCString::Atoi(*Args[0]) != 0);
				UCombatCameraDebug::PrintCameraState(World);
			}));
}

namespace
{
	/** wor.Camera.<Name> <int>: parses the value, applies it to the director, then prints the state. */
	FAutoConsoleCommandWithWorldAndArgs MakeCameraSettingCmd(const TCHAR *Name, const TCHAR *Help, TFunction<void(UCombatCameraDirector &, int32)> Apply)
	{
		return FAutoConsoleCommandWithWorldAndArgs(Name, Help, FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[Name, Apply](const TArray<FString> &Args, UWorld *World)
			{
				UCombatCameraDirector *Director = UCombatCameraDebug::FindDirector(World);
				if (!Director || Args.Num() == 0)
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] %s: no combat orchestrator, or no value."), Name);
					return;
				}
				Apply(*Director, FCString::Atoi(*Args[0]));
				UCombatCameraDebug::PrintCameraState(World);
			}));
	}

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraCinematicsCmd = MakeCameraSettingCmd(
		TEXT("wor.Camera.Cinematics"),
		TEXT("DEBUG: skill cinematics (D-078). 0 = follow camera movement (default), 1 = on, 2 = off."),
		[](UCombatCameraDirector &Director, int32 Value)
		{
			Director.SetSkillCinematics(static_cast<ESkillCinematics>(FMath::Clamp(Value, 0, static_cast<int32>(ESkillCinematics::Off))));
		});

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraEnemyCinematicsCmd = MakeCameraSettingCmd(
		TEXT("wor.Camera.EnemyCinematics"),
		TEXT("DEBUG: 1 = enemy skills play their cinematics (default), 0 = they don't."),
		[](UCombatCameraDirector &Director, int32 Value) { Director.SetEnemySkillCinematics(Value != 0); });

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraSkipAfterHitCmd = MakeCameraSettingCmd(
		TEXT("wor.Camera.SkipAfterHit"),
		TEXT("DEBUG: 1 = cut a cinematic once its caster's first hit lands, 0 = run to its end (default)."),
		[](UCombatCameraDirector &Director, int32 Value) { Director.SetSkipCinematicAfterHit(Value != 0); });
}

FString UCombatCameraDebug::GetCameraStateString(const UCombatCameraDirector *Director)
{
	if (!Director)
	{
		return TEXT("No combat camera (no combat orchestrator in this world)");
	}
	return FString::Printf(TEXT("%s | mode %s | viewing %s | style %s | movement %s | cinematics %s, enemy %s, skip after hit %s | sequence %s | %d cameras"),
						   Director->IsInCombat() ? TEXT("In combat") : TEXT("Not in combat"),
						   *UEnum::GetValueAsString(Director->GetCurrentMode()),
						   *GetNameSafe(Director->GetViewedActor()),
						   *UEnum::GetValueAsString(Director->GetTurnViewStyle()),
						   Director->IsCameraMovementOff() ? TEXT("off") : TEXT("on"),
						   *UEnum::GetValueAsString(Director->SkillCinematics),
						   Director->bEnemySkillCinematics ? TEXT("on") : TEXT("off"),
						   Director->bSkipCinematicAfterHit ? TEXT("on") : TEXT("off"),
						   Director->GetPlayingSequence() ? *Director->GetPlayingSequence()->GetName() : TEXT("none"),
						   Director->GetCameraCount());
}

UCombatCameraDirector *UCombatCameraDebug::FindDirector(const UObject *WorldContextObject)
{
	UWorld *World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ACombatOrchestrator> It(World); It; ++It)
	{
		if (UCombatCameraDirector *Director = It->GetCameraDirector())
		{
			return Director;
		}
	}
	return nullptr;
}

void UCombatCameraDebug::PrintCameraState(const UObject *WorldContextObject)
{
	const FString State = GetCameraStateString(FindDirector(WorldContextObject));
	UE_LOG(LogWoRCombat, Display, TEXT("[CombatCamera] %s"), *State);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, COMBAT_CAMERA_SCREEN_SECONDS, FColor::Cyan, State);
	}
}
