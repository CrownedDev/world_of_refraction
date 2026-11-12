// CombatCameraDebug.cpp

#include "Debug/Combat/CombatCameraDebug.h"
#include "Combat/Camera/CombatCameraDirector.h"
#include "Combat/CombatOrchestrator.h"
#include "LevelSequence.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Save/SaveSubsystem.h"
#include "Combat/WoRFightSubsystem.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float COMBAT_CAMERA_SCREEN_SECONDS = 8.0f;

	FAutoConsoleCommandWithWorld GPrintCombatCameraCmd(
		TEXT("wor.Camera.Print"),
		TEXT("DEBUG: print the combat camera's mode, viewed character, settings, playing sequence and the last cinematic window (B-370)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				UCombatCameraDebug::PrintCameraState(World);
			}));

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraStyleCmd(
		TEXT("wor.Camera.Style"),
		TEXT("PLACEHOLDER setting (B-398, until the D-024 settings screen): turn view. 0 = centred-wide (default), 1 = over-the-shoulder, ")
			TEXT("2 = tactical side-on. Saved; works in a fight or outside one (the next fight starts with it)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				const int32 Style = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
				if (Style < 0 || Style > static_cast<int32>(ETurnViewStyle::Tactical))
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] wor.Camera.Style: style not 0-2."));
					return;
				}
				const ETurnViewStyle NewStyle = static_cast<ETurnViewStyle>(Style);
				UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
				USaveSubsystem *Save = GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
				if (UCombatCameraDirector *Director = UCombatCameraDebug::FindDirector(World))
				{
					Director->SetTurnViewStyle(NewStyle); // saves it too
				}
				else if (Save)
				{
					Save->SetTurnViewStyle(NewStyle); // B-398: outside a fight; the next fight starts with it
				}
				else
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] wor.Camera.Style: no combat orchestrator and no save."));
					return;
				}
				UE_LOG(LogWoRCombat, Display, TEXT("[CombatCamera] %s: %s (saved setting: %s)"), *FCombatCameraRules::GetTurnViewSettingName().ToString(),
					   *FCombatCameraRules::GetTurnViewStyleName(NewStyle).ToString(),
					   Save ? *FCombatCameraRules::GetTurnViewStyleName(Save->GetTurnViewStyle()).ToString() : TEXT("no save"));
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
	/** wor.Camera.<Name> <int>: parses the value, applies it to the director, then prints the state.
	 *  Name must be a literal. Only ever initialise a global with the result: it relies on C++17
	 *  guaranteed elision, and a copied temporary would unregister the command when destroyed. */
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

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraHandoverCmd(
		TEXT("wor.Camera.Handover"),
		TEXT("DEBUG (B-404): preview the wide beat and the handover blend to party member N (no value: the member after the viewed one). ")
			TEXT("The turn and the HUD don't change; the next turn or setting change ends it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				UCombatCameraDirector *Director = UCombatCameraDebug::FindDirector(World);
				const int32 PartyIndex = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : INDEX_NONE;
				if (!Director || !Director->PreviewHandover(PartyIndex))
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] wor.Camera.Handover: no fight, no such party member, it is the viewed one, or no wide beat (movement off)."));
					return;
				}
				UCombatCameraDebug::PrintCameraState(World);
			}));

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraSkipAfterHitCmd = MakeCameraSettingCmd(
		TEXT("wor.Camera.SkipAfterHit"),
		TEXT("DEBUG: 1 = cut a cinematic once its caster's first hit lands, 0 = run to its end (default)."),
		[](UCombatCameraDirector &Director, int32 Value) { Director.SetSkipCinematicAfterHit(Value != 0); });

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraBossCinematicCmd(
		TEXT("wor.Camera.BossCinematic"),
		TEXT("DEBUG (B-406): preview a boss cinematic of the fight's first boss: 'opening', 'phase N' (N from 0), or 'stop'. ")
		TEXT("An empty field holds with the HUD hidden for the preview time instead."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				UCombatCameraDirector *Director = UCombatCameraDebug::FindDirector(World);
				const FString Which = Args.Num() > 0 ? Args[0].ToLower() : FString();
				if (!Director || Which.IsEmpty())
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] wor.Camera.BossCinematic: no combat orchestrator, or no 'opening' / 'phase N' / 'stop'."));
					return;
				}
				if (Which == TEXT("stop"))
				{
					Director->FinishBossCinematic();
				}
				else if (Which == TEXT("opening"))
				{
					Director->PreviewBossCinematic(EBossCinematicKind::Opening, INDEX_NONE);
				}
				else if (Which == TEXT("phase") && Args.Num() > 1)
				{
					Director->PreviewBossCinematic(EBossCinematicKind::Phase, FCString::Atoi(*Args[1]));
				}
				else
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] wor.Camera.BossCinematic: use 'opening', 'phase N' or 'stop'."));
					return;
				}
				UCombatCameraDebug::PrintCameraState(World);
			}));

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraKillShotCmd = MakeCameraSettingCmd(
		TEXT("wor.Camera.KillShot"),
		TEXT("DEBUG (B-405): preview a kill shot on the first enemy, then back to the turn view. 0 = Normal (wide, darkened), 1 = Boss (close, break-apart; nobody hidden)."),
		[](UCombatCameraDirector &Director, int32 Value) { Director.PreviewKillShot(Value != 0 ? EKillShotKind::Boss : EKillShotKind::Normal); });
}

namespace
{
	/** B-403: the sample fight wor.Camera.ShotPreview frames: caster at the origin, a target ahead on +X (melee
	 *  distance) and a second one beside it for the spread. */
	const FVector SHOT_PREVIEW_TARGET(800.0f, 0.0f, 0.0f);
	const FVector SHOT_PREVIEW_SECOND_TARGET(800.0f, 300.0f, 0.0f);

	FString DescribePreviewShot(const TCHAR *Label, const FCameraShot &Shot)
	{
		return FString::Printf(TEXT("%s: %s -> %s over %.2f s | fov %.0f | shake %.2f | blur %.2f"), Label,
							   *Shot.Start.GetLocation().ToCompactString(), *Shot.End.GetLocation().ToCompactString(),
							   Shot.BlendSeconds, Shot.FieldOfView, Shot.ShakeScale, Shot.MotionBlur);
	}

	/** B-403: every code-built shot for the sample fight with default tuning, one line each. */
	FString GetCameraShotPreviewString()
	{
		FCameraShotContext Melee;
		Melee.Targets.Add(SHOT_PREVIEW_TARGET);
		FCameraShotContext Ranged = Melee;
		Ranged.bRanged = true;
		FCameraShotContext Crit = Melee;
		Crit.bCritical = true;
		FCameraShotContext Spread = Melee;
		Spread.Targets.Add(SHOT_PREVIEW_SECOND_TARGET);
		const FSkillCameraTuning Tuning;
		struct FPreviewEntry
		{
			const TCHAR *Label;
			FCameraShot Shot;
		};
		const FPreviewEntry Shots[] = {
			{TEXT("melee wind-up (rush)"), FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Melee, Tuning)},
			{TEXT("spell wind-up"), FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, Ranged, Tuning)},
			{TEXT("cast"), FCombatCameraRules::ComputeShot(ECameraShotMoment::Cast, Ranged, Tuning)},
			{TEXT("impact"), FCombatCameraRules::ComputeShot(ECameraShotMoment::Impact, Melee, Tuning)},
			{TEXT("impact, crit"), FCombatCameraRules::ComputeShot(ECameraShotMoment::Impact, Crit, Tuning)},
			{TEXT("spread (holds)"), FCombatCameraRules::ComputeShot(ECameraShotMoment::Spread, Spread, Tuning)}};
		TArray<FString> Lines;
		for (const FPreviewEntry &Entry : Shots)
		{
			Lines.Add(DescribePreviewShot(Entry.Label, Entry.Shot));
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FAutoConsoleCommand GCombatCameraShotPreviewCmd(
		TEXT("wor.Camera.ShotPreview"),
		TEXT("DEBUG (B-403): print every code-built shot for a sample fight (caster at the origin, a target 800 cm ahead): start, end, blend, FOV, shake, motion blur. No fight needed."),
		FConsoleCommandDelegate::CreateLambda(
			[]()
			{
				const FString Preview = GetCameraShotPreviewString();
				UE_LOG(LogWoRCombat, Display, TEXT("[CombatCamera] Shot preview:\n%s"), *Preview);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(INDEX_NONE, COMBAT_CAMERA_SCREEN_SECONDS, FColor::Cyan, Preview);
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraMenuDepthCmd = MakeCameraSettingCmd(
		TEXT("wor.Camera.MenuDepth"),
		TEXT("DEBUG (B-399): what the command menu sends. 1 = a sub-menu is open (one step closer and lower), 0 = the main menu. ")
			TEXT("Moves only on your own turn view; wor.Camera.Print shows the depth and the menu moves this fight."),
		[](UCombatCameraDirector &Director, int32 Value) { Director.SetMenuDepth(Value); });

	FAutoConsoleCommandWithWorldAndArgs GCombatCameraTargetCycleCmd(
		TEXT("wor.Camera.TargetCycle"),
		TEXT("DEBUG (B-400): in an open single-target picker, highlight the next target (1, the default) or the previous one (-1), ")
			TEXT("as hovering or focusing its button does. With 2+ targets the camera cuts (Over-the-Shoulder) or re-aims to it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				UCombatCommandMenuSubsystem *Menu = World ? WoRFight::Get<UCombatCommandMenuSubsystem>(World) : nullptr;
				if (!Menu || !Menu->GetHighlightedTarget())
				{
					UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] wor.Camera.TargetCycle: no single-target picker open."));
					return;
				}
				Menu->CycleHighlightedTarget(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1);
				UCombatCameraDebug::PrintCameraState(World);
			}));
}

FString UCombatCameraDebug::GetCameraStateString(const UCombatCameraDirector *Director)
{
	if (!Director)
	{
		return TEXT("No combat camera (no combat orchestrator in this world)");
	}
	return FString::Printf(TEXT("%s | mode %s | viewing %s | style %s | %s | %s | movement %s | cinematics %s, enemy %s, skip after hit %s | sequence %s | %s | %s | %d cameras | %s | %s | %s"),
						   Director->IsInCombat() ? TEXT("In combat") : TEXT("Not in combat"),
						   *UEnum::GetValueAsString(Director->GetCurrentMode()),
						   *GetNameSafe(Director->GetViewedActor()),
						   *UEnum::GetValueAsString(Director->GetTurnViewStyle()),
						   *Director->GetTurnLayoutString(), // B-398
						   *Director->GetTargetViewString(), // B-400
						   Director->IsCameraMovementOff() ? TEXT("off") : TEXT("on"),
						   *UEnum::GetValueAsString(Director->SkillCinematics),
						   Director->bEnemySkillCinematics ? TEXT("on") : TEXT("off"),
						   Director->bSkipCinematicAfterHit ? TEXT("on") : TEXT("off"),
						   Director->GetPlayingSequence() ? *Director->GetPlayingSequence()->GetName() : TEXT("none"),
						   *Director->GetShotString(), // B-371
						   *Director->GetHandoverString(), // B-404
						   Director->GetCameraCount(),
						   *Director->GetCinematicWindowString(), // B-370
						   *Director->GetBossFramingString(), // B-405
						   *Director->GetBossCinematicString()); // B-406
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
