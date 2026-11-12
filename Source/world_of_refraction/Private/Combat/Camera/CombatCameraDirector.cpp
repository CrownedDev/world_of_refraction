// CombatCameraDirector.cpp

#include "Combat/Camera/CombatCameraDirector.h"
#include "Combat/Camera/CombatCameraComponent.h"
#include "Combat/Camera/CameraFramingChecks.h"
#include "Combat/Camera/SignatureCameraCap.h"
#include "HAL/IConsoleManager.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Actions/ActionStructs.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/WoRFightSubsystem.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Boss/BossData.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Engine/LocalPlayer.h"
#include "Character/CharacterData.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Save/SaveSubsystem.h"
#include "Combat/WoRFightSubsystem.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "WoRLog.h"

namespace CombatCameraTiming
{
	// TODO: tune in PIE (B-157).
	constexpr float TurnBlendSeconds = 0.6f;
	// TODO: tune in PIE (B-370). Short: the defender needs the view before the hit.
	constexpr float DefenceBlendSeconds = 0.25f;
	// B-371: how often a moving shot updates (a short timer while it moves, not a tick), and the cut onto another
	// character's camera when the shot then moves on that camera. TODO: tune in PIE (B-157).
	constexpr float ShotStepSeconds = 1.0f / 60.0f;
	constexpr float ShotCutSeconds = 0.2f;
	// B-406: how long wor.Camera.BossCinematic holds for an empty field when BossCinematicPreviewSeconds is 0.
	// TODO: tune (debug only).
	constexpr float BossCinematicPreviewDefaultSeconds = 3.0f;
	// B-399 (study R2): the sub-menu step in, and back out on cancel. TODO: tune in PIE (study estimate ~0.3 s).
	constexpr float MenuStepSeconds = 0.3f;
}

namespace CombatCameraTargetShape
{
	/** B-400: the height a target without collision bounds is framed at, in cm. TODO: tune in PIE. */
	constexpr float DefaultTargetHeight = 180.0f;
}

namespace
{
	const USkillDataBase *GetActionSkill(const FAction &Action)
	{
		return Action.ActionType == EActionType::Spell ? static_cast<const USkillDataBase *>(Action.SpellData) : Action.SkillData;
	}

	/** B-398: the player's settings live in the save; looked up each time, never cached across PIE. */
	USaveSubsystem *FindSaveSubsystem(const UActorComponent *Component)
	{
		const UWorld *World = Component ? Component->GetWorld() : nullptr;
		UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
	}
}

namespace CombatCameraBindings
{
	const FName CasterTag(TEXT("Caster"));
	const FName TargetsTag(TEXT("Targets"));
	// B-406: a boss cinematic's bindings.
	const FName BossTag(TEXT("Boss"));
	const FName PartyTag(TEXT("Party"));
}

void UCombatCameraDirector::BeginCombat(const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty, UActionExecutor *Executor)
{
	EndCombat();
	if (const USaveSubsystem *Save = FindSaveSubsystem(this))
	{
		TurnViewStyle = Save->GetTurnViewStyle(); // B-398: the player's setting, whichever orchestrator runs the fight
	}
	bInCombat = true;
	bActedSinceTurnStart = false; // B-404
	HandoverCount = 0;
	DefencePunchesDropped = 0;
	LocalCombatants.Append(LocalParty);
	OpposingCombatants.Append(OpposingParty);

	for (const TArray<TObjectPtr<AActor>> *Team : {&LocalCombatants, &OpposingCombatants})
	{
		for (AActor *Actor : *Team)
		{
			if (!IsValid(Actor))
			{
				continue;
			}
			UCombatCameraComponent *Camera = Actor->FindComponentByClass<UCombatCameraComponent>();
			if (!Camera)
			{
				// Unique name: a fight can end and restart in one frame, before GC removes the old component.
				Camera = NewObject<UCombatCameraComponent>(Actor, MakeUniqueObjectName(Actor, UCombatCameraComponent::StaticClass(), TEXT("CombatCamera")));
				Camera->RegisterComponent();
			}
			Cameras.Add(Camera);
		}
	}

	// B-406: a boss's phase cinematics (Normal enemies have no phase component, so never get one).
	for (AActor *Actor : OpposingCombatants)
	{
		if (UBossPhaseComponent *Phases = IsValid(Actor) ? Actor->FindComponentByClass<UBossPhaseComponent>() : nullptr)
		{
			Phases->OnBossPhaseReached.AddUniqueDynamic(this, &UCombatCameraDirector::HandleBossPhaseReached);
			BoundBossPhases.Add(Phases);
		}
	}

	if (ACombatOrchestrator *Orchestrator = Cast<ACombatOrchestrator>(GetOwner()))
	{
		Orchestrator->OnActorTurnStarted.AddUniqueDynamic(this, &UCombatCameraDirector::HandleActorTurnStarted);
	}
	if (Executor)
	{
		Executor->OnActionStarted.AddUniqueDynamic(this, &UCombatCameraDirector::HandleActionStarted);
		Executor->OnDamageDealt.AddUniqueDynamic(this, &UCombatCameraDirector::HandleDamageDealt);
		Executor->OnProjectileLaunched.AddUniqueDynamic(this, &UCombatCameraDirector::HandleProjectileLaunched);
		BoundExecutor = Executor;
	}
	// B-399: the command menu's depth, whichever HUD draws it. Looked up per fight, not cached across PIE.
	MenuDepth = CommandMenuRules::MAIN_MENU_DEPTH;
	MenuStepCount = 0;
	HighlightedTarget.Reset(); // B-400
	HighlightTargetCount = 0;
	bTargetViewShowing = false;
	TargetMoveCount = 0;
	if (UCombatCommandMenuSubsystem *Menu = WoRFight::Get<UCombatCommandMenuSubsystem>(this))
	{
		Menu->OnMenuDepthChanged.AddUniqueDynamic(this, &UCombatCameraDirector::HandleMenuDepthChanged);
		Menu->OnTargetHighlighted.AddUniqueDynamic(this, &UCombatCameraDirector::HandleTargetHighlighted); // B-400
		BoundMenu = Menu;
	}

	if (APlayerController *Viewer = GetViewer())
	{
		ViewTargetBeforeCombat = Viewer->GetViewTarget();
	}
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Fight started: %d cameras, turn view %s"), Cameras.Num(), *UEnum::GetValueAsString(TurnViewStyle));
}

void UCombatCameraDirector::EndCombat()
{
	if (!bInCombat)
	{
		return;
	}
	bInCombat = false;
	ClearDefenceHandOff();
	EndBossCinematic(false); // B-406: the HUD comes back; no finish broadcast once the fight is over
	for (const TWeakObjectPtr<UBossPhaseComponent> &Phases : BoundBossPhases)
	{
		if (UBossPhaseComponent *Component = Phases.Get())
		{
			Component->OnBossPhaseReached.RemoveDynamic(this, &UCombatCameraDirector::HandleBossPhaseReached);
		}
	}
	BoundBossPhases.Reset();
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BreakApartTimer); // B-405
		World->GetTimerManager().ClearTimer(KillShotPreviewTimer);
	}
	// B-405: undo what a kill shot left on its victim (debug levels reuse the pawns): the darkened look and the hidden boss.
	if (AActor *Victim = KillShotVictim.Get())
	{
		const UCombatCameraComponent *Camera = Victim->FindComponentByClass<UCombatCameraComponent>();
		if (ACameraActor *CameraActor = Camera ? Camera->GetCameraActor() : nullptr)
		{
			SetDarkened(CameraActor->GetCameraComponent(), false);
		}
		Victim->SetActorHiddenInGame(false);
	}
	KillShotVictim.Reset();
	StopSequence();
	StopShotPattern();
	// B-404: a fight that ends inside a wide beat drops its handover (the HUD is going away).
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WideBeatTimer);
	}
	bHandoverPending = false;
	HandoverFrom.Reset();
	HandoverTo.Reset();

	if (ACombatOrchestrator *Orchestrator = Cast<ACombatOrchestrator>(GetOwner()))
	{
		Orchestrator->OnActorTurnStarted.RemoveDynamic(this, &UCombatCameraDirector::HandleActorTurnStarted);
	}
	if (UActionExecutor *Executor = BoundExecutor.Get())
	{
		Executor->OnActionStarted.RemoveDynamic(this, &UCombatCameraDirector::HandleActionStarted);
		Executor->OnDamageDealt.RemoveDynamic(this, &UCombatCameraDirector::HandleDamageDealt);
		Executor->OnProjectileLaunched.RemoveDynamic(this, &UCombatCameraDirector::HandleProjectileLaunched);
	}
	BoundExecutor.Reset();
	if (UCombatCommandMenuSubsystem *Menu = BoundMenu.Get())
	{
		Menu->OnMenuDepthChanged.RemoveDynamic(this, &UCombatCameraDirector::HandleMenuDepthChanged);
		Menu->OnTargetHighlighted.RemoveDynamic(this, &UCombatCameraDirector::HandleTargetHighlighted);
	}
	BoundMenu.Reset();
	MenuDepth = CommandMenuRules::MAIN_MENU_DEPTH;
	HighlightedTarget.Reset(); // B-400
	HighlightTargetCount = 0;
	bTargetViewShowing = false;

	if (APlayerController *Viewer = GetViewer())
	{
		AActor *Restore = ViewTargetBeforeCombat.IsValid() ? ViewTargetBeforeCombat.Get() : Viewer->GetPawn();
		if (Restore)
		{
			Viewer->SetViewTargetWithBlend(Restore, 0.0f);
		}
	}

	for (UCombatCameraComponent *Camera : Cameras)
	{
		if (IsValid(Camera))
		{
			Camera->ReleaseCamera();
			Camera->DestroyComponent();
		}
	}
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Fight ended: %d cameras removed"), Cameras.Num());
	Cameras.Empty();
	LocalCombatants.Empty();
	OpposingCombatants.Empty();
	ViewedActor.Reset();
	ViewTargetBeforeCombat.Reset();
	CurrentMode = ECombatCameraMode::None;
}

void UCombatCameraDirector::SetTurnViewStyle(ETurnViewStyle NewStyle)
{
	TurnViewStyle = NewStyle;
	if (USaveSubsystem *Save = FindSaveSubsystem(this))
	{
		Save->SetTurnViewStyle(NewStyle); // B-398: kept for the next fight; on disk with the next save
	}
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Turn view: %s"), *FCombatCameraRules::GetTurnViewStyleName(NewStyle).ToString());
	if (bInCombat && !PlayingSequence && !IsShotPlaying())
	{
		ShowTurnView();
		ReapplyHighlightedTarget(); // B-400: an open picker keeps its target view
	}
}

void UCombatCameraDirector::SetCameraMovementOff(bool bOff)
{
	bCameraMovementOff = bOff;
	if (bOff)
	{
		ClearDefenceHandOff(); // B-370: the static view stays (D-078)
	}
	StopCinematicIfNowSkipped();
	// A cinematic still allowed keeps playing; the turn view picks up the new style after it.
	if (bInCombat && !PlayingSequence && !IsShotPlaying())
	{
		ShowTurnView();
		ReapplyHighlightedTarget(); // B-400: an open picker keeps its target view
	}
}

void UCombatCameraDirector::SetSkillCinematics(ESkillCinematics NewSetting)
{
	SkillCinematics = NewSetting;
	StopCinematicIfNowSkipped();
}

void UCombatCameraDirector::SetEnemySkillCinematics(bool bOn)
{
	bEnemySkillCinematics = bOn;
	StopCinematicIfNowSkipped();
}

void UCombatCameraDirector::StopCinematicIfNowSkipped()
{
	if (!bInCombat || (!PlayingSequence && !IsShotPlaying()))
	{
		return;
	}
	const AActor *Caster = PlayingSequence ? SequenceCaster.Get() : ShotCaster.Get();
	const bool bStillPlays = FCombatCameraRules::ShouldPlayCinematic(
		SkillCinematics, bCameraMovementOff, OpposingCombatants.Contains(Caster), bEnemySkillCinematics);
	if (!bStillPlays)
	{
		StopSequence();
		ShowTurnView();
	}
}

void UCombatCameraDirector::SetSkipCinematicAfterHit(bool bSkip)
{
	bSkipCinematicAfterHit = bSkip;
}

void UCombatCameraDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndCombat();
	Super::EndPlay(EndPlayReason);
}

void UCombatCameraDirector::HandleActorTurnStarted(AActor *Actor, int32 TurnNumber)
{
	// B-404: who the camera followed, and whether anyone acted since the last turn start, decide the wide beat.
	FlushPendingHandover();
	AActor *From = ViewedActor.Get();
	const bool bSomeoneActed = bActedSinceTurnStart;
	bActedSinceTurnStart = false;
	ViewedActor = Actor;
	MenuDepth = CommandMenuRules::MAIN_MENU_DEPTH; // B-399: a new turn opens on the main menu's view
	HighlightedTarget.Reset(); // B-400: a new turn has no picker open
	HighlightTargetCount = 0;
	ClearDefenceHandOff();
	if (IsBossCinematicPlaying())
	{
		return; // B-406: the boss cinematic keeps the view; its end shows this turn
	}
	StopSequence();
	StartTurnChange(From, Actor, bSomeoneActed, true); // B-404: the turn view, through a wide beat between turns
}

void UCombatCameraDirector::HandleMenuDepthChanged(AActor *Actor, int32 NewDepth)
{
	if (!bInCombat)
	{
		return;
	}
	if (Actor != ViewedActor.Get())
	{
		UE_LOG(LogWoRCombat, Verbose, TEXT("[CombatCamera] Menu depth %d for %s, not the viewed %s: ignored"), NewDepth,
			   *GetNameSafe(Actor), *GetNameSafe(ViewedActor.Get()));
		return;
	}
	SetMenuDepth(NewDepth);
}

void UCombatCameraDirector::SetMenuDepth(int32 NewDepth)
{
	const int32 Depth = FMath::Clamp(NewDepth, 0, FCombatCameraRules::MaxMenuDepth);
	if (Depth == MenuDepth)
	{
		return;
	}
	MenuDepth = Depth;
	// Movement off (Static), a cinematic, a held shot or someone else's turn: the depth is kept, nothing moves (R3, R15).
	const bool bTurnViewShowing = CurrentMode == ECombatCameraMode::Turn && !PlayingSequence && !IsShotPlaying();
	if (!bInCombat || !bTurnViewShowing)
	{
		UE_LOG(LogWoRCombat, Verbose, TEXT("[CombatCamera] Menu depth %d: camera holds (mode %s)"), Depth, *UEnum::GetValueAsString(CurrentMode));
		return;
	}
	StepTurnViewForMenu();
}

void UCombatCameraDirector::StepTurnViewForMenu()
{
	AActor *Actor = ViewedActor.Get();
	if (!Actor || !MoveTurnCamera(GetTurnViewWithHeight(BuildFrame(Actor)), CombatCameraTiming::MenuStepSeconds))
	{
		return;
	}
	bTargetViewShowing = false; // B-400: a depth step (wor.Camera.MenuDepth) lands on the turn view
	++MenuStepCount;
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Menu depth %d: %s (%.2f s, move %d)"), MenuDepth,
		   MenuDepth > CommandMenuRules::MAIN_MENU_DEPTH ? TEXT("one step closer") : TEXT("back to the turn view"),
		   CombatCameraTiming::MenuStepSeconds, MenuStepCount);
}

bool UCombatCameraDirector::MoveTurnCamera(const FTransform &End, float Seconds)
{
	AActor *Actor = ViewedActor.Get();
	UCombatCameraComponent *Camera = Actor ? Actor->FindComponentByClass<UCombatCameraComponent>() : nullptr;
	ACameraActor *CameraActor = Camera ? Camera->GetCameraActor() : nullptr;
	UWorld *World = GetWorld();
	if (!CameraActor || !World)
	{
		return false;
	}
	// The shot timer's move (StepShot), on the turn camera, from where it is now. No pattern is playing (ShotCaster
	// stays empty), so the next turn view or shot clears it as it clears any shot.
	World->GetTimerManager().ClearTimer(ShotTimer);
	if (Seconds <= 0.0f)
	{
		CameraActor->SetActorTransform(End); // a cut (B-400, study §12)
		CameraActor->GetCameraComponent()->SetFieldOfView(FCombatCameraRules::DefaultFieldOfView);
		return true;
	}
	ShotCamera = Camera;
	ShotMoment = ECameraShotMoment::WindUp;
	ShotProjectile.Reset();
	ActiveShot = FCameraShot();
	ActiveShot.Start = CameraActor->GetActorTransform();
	ActiveShot.End = End;
	ActiveShot.BlendSeconds = Seconds;
	ActiveShot.FieldOfView = FCombatCameraRules::DefaultFieldOfView;
	ShotElapsed = 0.0f;
	World->GetTimerManager().SetTimer(ShotTimer, this, &UCombatCameraDirector::StepShot, CombatCameraTiming::ShotStepSeconds, true);
	return true;
}

void UCombatCameraDirector::HandleTargetHighlighted(AActor *Actor, AActor *Target, int32 TargetCount)
{
	if (!bInCombat)
	{
		return;
	}
	if (Actor != ViewedActor.Get())
	{
		UE_LOG(LogWoRCombat, Verbose, TEXT("[CombatCamera] Target %s highlighted for %s, not the viewed %s: ignored"), *GetNameSafe(Target),
			   *GetNameSafe(Actor), *GetNameSafe(ViewedActor.Get()));
		return;
	}
	SetHighlightedTarget(Target, TargetCount);
}

void UCombatCameraDirector::ReapplyHighlightedTarget()
{
	if (AActor *Target = HighlightedTarget.Get())
	{
		SetHighlightedTarget(Target, HighlightTargetCount);
	}
}

void UCombatCameraDirector::SetHighlightedTarget(AActor *Target, int32 TargetCount)
{
	HighlightedTarget = Target;
	HighlightTargetCount = Target ? TargetCount : 0;
	AActor *Actor = ViewedActor.Get();
	// Movement off (Static), a cinematic, a held shot or someone else's turn: nothing moves (R3, R15).
	const bool bTurnViewShowing = bInCombat && Actor && CurrentMode == ECombatCameraMode::Turn && !PlayingSequence && !IsShotPlaying();
	if (!bTurnViewShowing)
	{
		bTargetViewShowing = false;
		UE_LOG(LogWoRCombat, Verbose, TEXT("[CombatCamera] Target %s: camera holds (mode %s)"), *GetNameSafe(Target), *UEnum::GetValueAsString(CurrentMode));
		return;
	}
	const float Seconds = FCombatCameraRules::GetTargetViewBlendSeconds(TurnViewStyle);
	const FCombatCameraFrame Frame = BuildFrame(Actor);
	if (Target && TargetCount >= FCombatCameraRules::MinTargetsForTargetView)
	{
		FVector Feet;
		float Height = 0.0f;
		GetTargetShape(Target, Feet, Height);
		if (MoveTurnCamera(FCombatCameraRules::ComputeTargetViewFrom(TurnViewStyle, Frame, GetTurnViewWithHeight(Frame), Feet, Height, TargetCount), Seconds))
		{
			bTargetViewShowing = true;
			++TargetMoveCount;
			UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Target view: %s of %d (%s, move %d)"), *Target->GetName(), TargetCount,
				   Seconds > 0.0f ? TEXT("re-aim") : TEXT("cut"), TargetMoveCount);
		}
		return;
	}
	if (!bTargetViewShowing)
	{
		return; // one target (no move, study R3), or the picker closed with no target view showing
	}
	bTargetViewShowing = false;
	if (MoveTurnCamera(GetTurnViewWithHeight(Frame), Seconds))
	{
		++TargetMoveCount;
		UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Target picker closed: back to the turn view (move %d)"), TargetMoveCount);
	}
}

void UCombatCameraDirector::CutBackFromTargetView()
{
	if (!bTargetViewShowing)
	{
		return;
	}
	bTargetViewShowing = false;
	if (AActor *Actor = ViewedActor.Get())
	{
		MoveTurnCamera(GetTurnViewWithHeight(BuildFrame(Actor)), 0.0f);
	}
}

void UCombatCameraDirector::GetTargetShape(const AActor *Target, FVector &OutFeet, float &OutHeight)
{
	OutFeet = Target ? Target->GetActorLocation() : FVector::ZeroVector;
	OutHeight = CombatCameraTargetShape::DefaultTargetHeight;
	if (!Target)
	{
		return;
	}
	FVector Origin;
	FVector Extent;
	Target->GetActorBounds(true, Origin, Extent); // colliding only: a character's capsule
	if (Extent.Z > UE_KINDA_SMALL_NUMBER)
	{
		OutFeet = FVector(Origin.X, Origin.Y, Origin.Z - Extent.Z);
		OutHeight = static_cast<float>(Extent.Z * 2.0);
	}
}

FString UCombatCameraDirector::GetTargetViewString() const
{
	const AActor *Target = HighlightedTarget.Get();
	if (!Target)
	{
		return FString::Printf(TEXT("target view: none (%d moves)"), TargetMoveCount);
	}
	return FString::Printf(TEXT("target view: %s of %d (%s, %s, %d moves)"), *Target->GetName(), HighlightTargetCount,
						   bTargetViewShowing ? TEXT("showing") : TEXT("held"),
						   FCombatCameraRules::GetTargetViewBlendSeconds(TurnViewStyle) > 0.0f ? TEXT("re-aim") : TEXT("cut"), TargetMoveCount);
}

void UCombatCameraDirector::HandleActionStarted(AActor *Executor, const FAction &Action, int32 EnergyCost)
{
	if (!bInCombat)
	{
		return;
	}
	FlushPendingHandover(); // B-404: an action inside the beat: the handover now; the action's own camera follows
	bActedSinceTurnStart = true;
	ClearDefenceHandOff();
	CutBackFromTargetView(); // B-400: confirm cuts from the target view; the action's camera goes on as before
	// An Execution turn (a ritual firing) has no turn-start broadcast: frame whoever acts.
	const bool bNewActor = Executor && Executor != ViewedActor.Get();
	if (bNewActor)
	{
		ViewedActor = Executor;
		MenuDepth = CommandMenuRules::MAIN_MENU_DEPTH; // B-399: another actor's menu step never carries over
	}
	if (IsBossCinematicPlaying())
	{
		return; // B-406: no shots and no Defence hand-off under a boss cinematic
	}
	// B-370: whatever the cinematic, a viewer who must defend gets the Defence view at the window's end. A hit
	// too soon for any cinematic shows it now, and no cinematic plays over it.
	if (ScheduleDefenceHandOff(Executor, Action))
	{
		return;
	}
	const bool bPlayCinematic = FCombatCameraRules::ShouldPlayCinematic(
		SkillCinematics, bCameraMovementOff, OpposingCombatants.Contains(Executor), bEnemySkillCinematics);
	const USkillDataBase *Skill = GetActionSkill(Action);
	const bool bSkillAction = Action.ActionType == EActionType::Spell || Action.ActionType == EActionType::Ability;
	if (bPlayCinematic && bSkillAction && FCombatCameraRules::UsesShotPatterns(Skill))
	{
		// B-371 (Q-371): code-built shots unless the skill has its own sequence.
		if (!StartShotPattern(Executor, Action, Skill) && bNewActor)
		{
			ShowTurnView(); // review fix: nothing to show the shots on, so at least frame the new actor
		}
		return;
	}
	ULevelSequence *Sequence = bPlayCinematic ? ResolveSequence(Action) : nullptr;
	if (!Sequence)
	{
		// No cinematic for this action (an attack, an item, cinematics off): the turn view stays.
		// Review fix: a second action in one turn (a ritual fire, an extra action) ends an earlier skill's shots.
		const bool bShotsWerePlaying = IsShotPlaying();
		if (bShotsWerePlaying)
		{
			StopShotPattern(); // B-404: only a pattern; a handover blend on the shot timer runs on into the turn view
		}
		if (bNewActor || bShotsWerePlaying)
		{
			ShowTurnView();
		}
		UE_LOG(LogWoRCombat, Verbose, TEXT("[CombatCamera] No sequence for %s's action; keeping the turn view"), *GetNameSafe(Executor));
		return;
	}
	PlaySequence(Sequence, Executor, Action.Targets);
}

void UCombatCameraDirector::HandleSequenceFinished()
{
	// Ends when its own sequence does, or when the next turn starts (which stops it). Not on action
	// completion: that broadcast comes after the next turn may already have started a new sequence.
	StopSequence(true);
	if (bInCombat && !DefenceHandOffTimer.IsValid())
	{
		ShowTurnView();
	}
}

void UCombatCameraDirector::HandleDamageDealt(AActor *Attacker, AActor *Target, int32 Damage, bool bCritical)
{
	// B-371: the caster's first hit plays the Impact shot (several targets: Spread), or cuts back when set to.
	if (IsShotPlaying() && Attacker == ShotCaster.Get() && !bShotImpactShown)
	{
		bShotImpactShown = true;
		ShotProjectile.Reset(); // review fix: the impact shot doesn't follow the projectile
		// A hit on the frame the pattern started would cut it unseen; let that one show (as the sequence path does).
		if (bSkipCinematicAfterHit && GFrameCounter != ShotStartFrame)
		{
			ShowTurnView(); // stops the pattern
			return;
		}
		const ECameraShotMoment Moment = FCombatCameraRules::ImpactMomentFor(ShotTargets.Num());
		UCombatCameraComponent *Camera = IsValid(Target) ? Target->FindComponentByClass<UCombatCameraComponent>() : nullptr;
		PlayShot(Camera ? Camera : ShotCamera.Get(), Moment, FCombatCameraRules::ComputeShot(Moment, BuildShotContext(bCritical), ShotTuning));
		return;
	}
	// A hit resolved on the frame the cinematic started (no montage to wait on) would cut it
	// before it is ever seen; let that one run.
	if (!bSkipCinematicAfterHit || !PlayingSequence || Attacker != SequenceCaster.Get() || GFrameCounter == SequenceStartFrame)
	{
		return;
	}
	// Called from inside the hit, not from the player's OnFinished, so the actor can go now.
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] %s's hit landed: cutting %s"), *GetNameSafe(Attacker), *PlayingSequence->GetName());
	StopSequence();
	ShowTurnView();
}

void UCombatCameraDirector::ShowTurnView()
{
	if (IsBossCinematicPlaying())
	{
		return; // B-406: its end shows the turn view
	}
	StopShotPattern(); // B-371: the end of a pattern blends back to the turn view
	bTargetViewShowing = false; // B-400
	AActor *Actor = ViewedActor.Get();
	APlayerController *Viewer = GetViewer();
	UCombatCameraComponent *Camera = Actor ? Actor->FindComponentByClass<UCombatCameraComponent>() : nullptr;
	if (!Viewer || !Camera)
	{
		return;
	}
	CurrentMode = FCombatCameraRules::SelectMode(IsViewersCharacter(Actor), false, bCameraMovementOff);
	const FCombatCameraFrame Frame = BuildFrame(Actor);
	FTransform View = FCombatCameraRules::ComputeView(CurrentMode, TurnViewStyle, Frame, MenuDepth); // B-399
	if (CurrentMode == ECombatCameraMode::Turn)
	{
		View = FBossFramingRules::ApplyHeight(View, GetFightCameraHeight(), TurnViewStyle, Frame); // B-405 (R13)
	}
	if (ACameraActor *CameraActor = Camera->ShowView(CurrentMode, View))
	{
		CameraActor->GetCameraComponent()->SetFieldOfView(FCombatCameraRules::DefaultFieldOfView); // B-371: undo a punch
		const float Blend = CurrentMode == ECombatCameraMode::Static ? 0.0f : CombatCameraTiming::TurnBlendSeconds;
		Viewer->SetViewTargetWithBlend(CameraActor, Blend);
	}
}

ULevelSequence *UCombatCameraDirector::ResolveSequence(const FAction &Action) const
{
	if (Action.ActionType != EActionType::Spell && Action.ActionType != EActionType::Ability)
	{
		return nullptr;
	}
	const USkillDataBase *Skill = GetActionSkill(Action);
	// B-371 (Q-371): only the skill's own sequence (a signature move); the type templates retired for the patterns.
	const TSoftObjectPtr<ULevelSequence> Soft = Skill ? Skill->CameraSequence : TSoftObjectPtr<ULevelSequence>();
	// TODO: B-140 async loading. Sequences are small; a sync load at action start is fine for now.
	return Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
}

void UCombatCameraDirector::PlaySequence(ULevelSequence *Sequence, AActor *Caster, const TArray<AActor *> &Targets)
{
	StopSequence();
	StopShotPattern(); // B-371 (review): a held pattern must not take the view back from a signature sequence
	FMovieSceneSequencePlaybackSettings Settings;
	ALevelSequenceActor *NewActor = nullptr;
	ULevelSequencePlayer *Player = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, NewActor);
	if (!Player || !NewActor)
	{
		UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] Could not create a player for %s"), *GetNameSafe(Sequence));
		return;
	}

	TArray<AActor *> ValidTargets;
	for (AActor *Target : Targets)
	{
		if (IsValid(Target))
		{
			ValidTargets.Add(Target);
		}
	}
	NewActor->SetBindingByTag(CombatCameraBindings::CasterTag, {Caster});
	NewActor->SetBindingByTag(CombatCameraBindings::TargetsTag, ValidTargets);

	SequencePlayer = Player;
	SequenceActor = NewActor;
	PlayingSequence = Sequence;
	SequenceCaster = Caster;
	SequenceStartFrame = GFrameCounter;
	CurrentMode = ECombatCameraMode::Action;
	Player->OnFinished.AddDynamic(this, &UCombatCameraDirector::HandleSequenceFinished);
	Player->Play();
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Playing %s (caster %s, %d targets)"),
		   *Sequence->GetName(), *GetNameSafe(Caster), ValidTargets.Num());

	// B-407 (R7, FeelGuide 9): a player or team signature sequence is cut back at the cap; an enemy's runs to its end.
	LastSignatureName = Sequence->GetName();
	LastSignatureCaster = GetNameSafe(Caster);
	LastSignatureSeconds = SignatureCameraCap::GetSequenceSeconds(Sequence);
	bLastSignatureEnemy = OpposingCombatants.Contains(Caster);
	LastSignatureCutSeconds = SignatureCameraCap::GetCutSeconds(LastSignatureSeconds, bLastSignatureEnemy);
	bLastSignatureCut = false;
	bLastSignatureStoppedEarly = false;
	UWorld *World = GetWorld();
	if (LastSignatureCutSeconds > 0.0f && World && PlayingSequence) // review: a sequence that finished inside Play() has nothing to cut
	{
		World->GetTimerManager().SetTimer(SignatureCapTimer, this, &UCombatCameraDirector::HandleSignatureCapReached, LastSignatureCutSeconds, false);
		UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] %s is %.2f s: cutting back at the %.2f s signature cap"),
			   *LastSignatureName, LastSignatureSeconds, LastSignatureCutSeconds);
	}
}

void UCombatCameraDirector::HandleSignatureCapReached()
{
	SignatureCapTimer.Invalidate();
	if (!PlayingSequence)
	{
		return;
	}
	bLastSignatureCut = true;
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] %s reached the signature cap: cutting back"), *PlayingSequence->GetName());
	// As HandleSequenceFinished, but from a timer, not the player's own OnFinished, so the actor can go now.
	StopSequence();
	if (bInCombat && !DefenceHandOffTimer.IsValid())
	{
		ShowTurnView();
	}
}

FString UCombatCameraDirector::GetSignatureCapString() const
{
	const FString Cap = FString::Printf(TEXT("cap %.2f s"), SignatureCameraCap::MaxSignatureSeconds);
	if (LastSignatureName.IsEmpty())
	{
		return Cap + TEXT(" | no signature sequence played yet");
	}
	FString Outcome;
	if (bLastSignatureEnemy)
	{
		Outcome = TEXT("enemy caster: not capped");
	}
	else if (LastSignatureCutSeconds <= 0.0f)
	{
		Outcome = TEXT("within the cap");
	}
	else
	{
		Outcome = FString::Printf(TEXT("%s at %.2f s"), bLastSignatureCut ? TEXT("cut") : (bLastSignatureStoppedEarly ? TEXT("stopped before the cap") : TEXT("to be cut")), LastSignatureCutSeconds);
	}
	return FString::Printf(TEXT("%s | last %s %.2f s by %s: %s"), *Cap, *LastSignatureName, LastSignatureSeconds, *LastSignatureCaster, *Outcome);
}

void UCombatCameraDirector::StopSequence(bool bDeferDestroy)
{
	if (UWorld *World = GetWorld())
	{
		// B-407: a capped sequence stopped before its cap (a hit, the next turn, a hand-off) says so in the debug line.
		bLastSignatureStoppedEarly = bLastSignatureStoppedEarly || World->GetTimerManager().IsTimerActive(SignatureCapTimer);
		World->GetTimerManager().ClearTimer(SignatureCapTimer); // B-407
	}
	SignatureCapTimer.Invalidate();
	if (SequencePlayer)
	{
		SequencePlayer->OnFinished.RemoveAll(this);
		if (!bDeferDestroy)
		{
			SequencePlayer->Stop();
		}
	}
	if (IsValid(SequenceActor))
	{
		UWorld *World = GetWorld();
		if (bDeferDestroy && World)
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(SequenceActor.Get(), [Actor = SequenceActor.Get()]()
			{
				Actor->Destroy();
			}));
		}
		else
		{
			SequenceActor->Destroy();
		}
	}
	SequencePlayer = nullptr;
	SequenceActor = nullptr;
	PlayingSequence = nullptr;
	SequenceCaster.Reset();
}

FCombatCameraFrame UCombatCameraDirector::BuildFrame(AActor *Focus) const
{
	auto Centre = [](const TArray<TObjectPtr<AActor>> &Team, FVector Fallback)
	{
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (const AActor *Actor : Team)
		{
			if (IsValid(Actor))
			{
				Sum += Actor->GetActorLocation();
				++Count;
			}
		}
		return Count > 0 ? Sum / Count : Fallback;
	};

	FCombatCameraFrame Frame;
	Frame.Focus = Focus->GetActorLocation();
	// Always framed from the viewer's side, so turns never flip the view across the arena.
	const FVector ViewerSide = Centre(LocalCombatants, Frame.Focus);
	const FVector OtherSide = Centre(OpposingCombatants, Frame.Focus + FVector::ForwardVector);
	Frame.TowardEnemies = (OtherSide - ViewerSide).GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	Frame.EnemyCentre = OtherSide;
	Frame.ArenaCentre = (ViewerSide + OtherSide) * 0.5f;
	return Frame;
}

bool UCombatCameraDirector::IsViewersCharacter(const AActor *Actor) const
{
	// TODO: co-op/PvP (D-065): the viewer's own characters, not the whole local side.
	return LocalCombatants.Contains(Actor);
}

APlayerController *UCombatCameraDirector::GetViewer() const
{
	// Same viewer the combat HUD uses (UCombatHUD). TODO: co-op/PvP (D-065).
	UWorld *World = GetWorld();
	// The local player's controller: on a dedicated server there is none, so no server-side camera or kill-shot hold
	// (GetFirstPlayerController would return a remote client's controller there).
	if (APlayerController *TestViewer = ViewerForTests.Get())
	{
		return TestViewer;
	}
	return World && GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
}

bool UCombatCameraDirector::ScheduleDefenceHandOff(AActor *Executor, const FAction &Action)
{
	bHasLastWindow = false;
	LastHandOffMode = ECombatCameraMode::None;
	// Only someone else's skill is defended: the attacker is not one of the viewer's characters.
	const USkillDataBase *Skill = GetActionSkill(Action);
	if (!Executor || IsViewersCharacter(Executor) || !Skill)
	{
		return false;
	}
	// A ritual's arm turn plays the ritual leg only and lands nothing; its fire turn hands off (review fix).
	const bool bArmTurn = Skill->ActivationDelay > 0 && !Action.bIsDeferredFire;
	if (bArmTurn)
	{
		return false;
	}
	TArray<TWeakObjectPtr<AActor>> Defenders;
	for (AActor *Target : Action.Targets)
	{
		if (IsValid(Target) && IsViewersCharacter(Target))
		{
			Defenders.Add(Target);
		}
	}
	// The rate the runner plays at (UActionExecutor::PlaySpellAnimation / PlayAbilityAnimation, before ActionMods).
	float PlayRate = Skill->BaseAnimSpeed;
	if (const UCharacterDataComponent *CasterComp = Executor->FindComponentByClass<UCharacterDataComponent>())
	{
		PlayRate *= Action.ActionType == EActionType::Spell ? CasterComp->GetEffectiveSpellSpeed() : CasterComp->GetEffectiveActionSpeed();
	}
	LastWindowTiming = FCombatCameraRules::ReadWindowTiming(Skill, PlayRate, Action.bIsDeferredFire);
	bHasLastWindow = FCombatCameraRules::ResolveWindowEnd(LastWindowTiming, LastWindowEnd);
	LastHandOffMode = FCombatCameraRules::SelectHandOffMode(!Defenders.IsEmpty(), false, bCameraMovementOff);
	if (!bHasLastWindow || LastHandOffMode != ECombatCameraMode::Defence)
	{
		return false; // nothing to defend, or the static view (D-078) stays
	}

	HandOffDefenders = MoveTemp(Defenders);
	HandOffAttacker = Executor;
	UWorld *World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (LastWindowEnd <= 0.0f)
	{
		HandleCinematicWindowEnd(); // the first hit comes too soon for any cinematic
		return true;
	}
	World->GetTimerManager().SetTimer(DefenceHandOffTimer, this, &UCombatCameraDirector::HandleCinematicWindowEnd, LastWindowEnd, false);
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] %s targets %d of the viewer's characters: Defence view in %.2f s"),
		   *GetNameSafe(Executor), HandOffDefenders.Num(), LastWindowEnd);
	return false;
}

void UCombatCameraDirector::HandleCinematicWindowEnd()
{
	DefenceHandOffTimer.Invalidate();
	if (!bInCombat)
	{
		return;
	}
	StopSequence();
	StopShotPattern(); // B-371
	ShowDefenceView();
}

void UCombatCameraDirector::ClearDefenceHandOff()
{
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DefenceHandOffTimer);
	}
	DefenceHandOffTimer.Invalidate();
	HandOffDefenders.Reset();
	HandOffAttacker.Reset();
}

void UCombatCameraDirector::ShowDefenceView()
{
	if (IsBossCinematicPlaying())
	{
		return; // B-406: no defence view until the HUD is back
	}
	AActor *Attacker = HandOffAttacker.Get();
	TArray<FVector> Defenders;
	UCombatCameraComponent *Camera = nullptr;
	for (const TWeakObjectPtr<AActor> &Defender : HandOffDefenders)
	{
		if (AActor *Actor = Defender.Get())
		{
			Defenders.Add(Actor->GetActorLocation());
			Camera = Camera ? Camera : Actor->FindComponentByClass<UCombatCameraComponent>();
		}
	}
	APlayerController *Viewer = GetViewer();
	if (!Viewer || !Camera || !Attacker)
	{
		UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] Defence view skipped: viewer %s, defender camera %s, attacker %s"),
			   Viewer ? TEXT("ok") : TEXT("none"), Camera ? TEXT("ok") : TEXT("none"), *GetNameSafe(Attacker));
		return;
	}
	CurrentMode = ECombatCameraMode::Defence;
	const FTransform View = FCombatCameraRules::ComputeDefenceView(Defenders, Attacker->GetActorLocation());
	if (ACameraActor *CameraActor = Camera->ShowView(CurrentMode, View))
	{
		CameraActor->GetCameraComponent()->SetFieldOfView(FCombatCameraRules::DefaultFieldOfView); // B-371
		Viewer->SetViewTargetWithBlend(CameraActor, CombatCameraTiming::DefenceBlendSeconds);
	}
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Defence view (%s): %d defender(s) against %s"),
		   Defenders.Num() > 1 ? TEXT("wide side-on") : TEXT("side-on two-shot"), Defenders.Num(), *GetNameSafe(Attacker));
}

bool UCombatCameraDirector::IsViewerDefending() const
{
	if (CurrentMode == ECombatCameraMode::Defence || DefenceHandOffTimer.IsValid())
	{
		return true;
	}
	const UDefenseSystem *Defense = WoRFight::Get<UDefenseSystem>(this);
	if (!Defense)
	{
		return false;
	}
	for (AActor *Actor : LocalCombatants)
	{
		if (IsValid(Actor) && IsViewersCharacter(Actor) && Defense->IsDefenseWindowOpen(Actor))
		{
			return true;
		}
	}
	return false;
}

FString UCombatCameraDirector::GetCinematicWindowString() const
{
	// B-402: whether shots are held still, and how many punches that has held back this fight.
	const FString Steady = FString::Printf(TEXT(" | defending %s, %d punch(es) dropped"), IsViewerDefending() ? TEXT("yes (steady)") : TEXT("no"),
										   DefencePunchesDropped);
	if (!bHasLastWindow)
	{
		return TEXT("window: none (nothing placed and nothing to defend, or no action yet)") + Steady;
	}
	const FString Defendable = LastWindowTiming.bHasDefendable ? FString::Printf(TEXT("%.2f s"), LastWindowTiming.FirstDefendable) : TEXT("none");
	const TCHAR *Source = LastWindowTiming.bHasAuthoredWindow ? TEXT("placed") : LastWindowTiming.bHasWindUpEnd ? TEXT("wind-up end") : TEXT("read time only");
	return FString::Printf(TEXT("window ends %.2f s (%s; first defendable %s; read %.2f s) -> %s%s%s"), LastWindowEnd, Source, *Defendable,
						   FCombatCameraRules::MinDefenseReadSeconds, *UEnum::GetValueAsString(LastHandOffMode),
						   DefenceHandOffTimer.IsValid() ? TEXT(", pending") : TEXT(""), *Steady);
}

FTransform UCombatCameraDirector::GetTurnViewWithHeight(const FCombatCameraFrame &Frame) const
{
	return FBossFramingRules::ApplyHeight(FCombatCameraRules::ComputeTurnView(TurnViewStyle, Frame, MenuDepth), GetFightCameraHeight(),
										  TurnViewStyle, Frame);
}

FString UCombatCameraDirector::GetTurnLayoutString() const
{
	AActor *Actor = ViewedActor.Get();
	if (!bInCombat || !Actor)
	{
		return TEXT("layout: no turn view");
	}
	const FCombatCameraFrame Frame = BuildFrame(Actor);
	// The real turn view: B-405's fight height on top (a boss fight is low and looks up).
	const FTransform View = GetTurnViewWithHeight(Frame); // B-399, B-405
	auto Describe = [&View](const FVector &Point)
	{
		FVector2D Screen;
		return CameraFraming::ProjectToScreen(View, FCombatCameraRules::DefaultFieldOfView, CameraFraming::DEFAULT_ASPECT, Point, Screen)
				   ? CameraFraming::GetThirdsCell(Screen)
				   : FString(TEXT("behind the camera"));
	};
	return FString::Printf(TEXT("layout %s, menu depth %d (%d menu moves): %s %s, enemy centre %s"), *UEnum::GetValueAsString(TurnViewStyle),
						   MenuDepth, MenuStepCount, *Actor->GetName(), *Describe(Frame.Focus), *Describe(Frame.EnemyCentre));
}

bool UCombatCameraDirector::StartShotPattern(AActor *Caster, const FAction &Action, const USkillDataBase *Skill)
{
	StopSequence();
	StopShotPattern();
	UCombatCameraComponent *Camera = Caster ? Caster->FindComponentByClass<UCombatCameraComponent>() : nullptr;
	if (!Camera || !Skill)
	{
		return false;
	}
	ShotCaster = Caster;
	for (AActor *Target : Action.Targets)
	{
		if (IsValid(Target))
		{
			ShotTargets.Add(Target);
		}
	}
	ShotCamera = Camera;
	ShotTuning = Skill->CameraTuning;
	bShotRanged = Action.ActionType == EActionType::Spell || Skill->ExecutionType == EAbilityExecutionType::Ranged;
	ShotStartFrame = GFrameCounter;
	// B-406: an enemy's named attack opens on a front shot of the enemy, everything else on the wind-up.
	const ECameraShotMoment Opening = FCombatCameraRules::OpeningMomentFor(ShotTuning, OpposingCombatants.Contains(Caster));
	if (!PlayShot(Camera, Opening, FCombatCameraRules::ComputeShot(Opening, BuildShotContext(false), ShotTuning)))
	{
		StopShotPattern(); // review fix: no viewer or camera, nothing left half-playing
		return false;
	}
	CurrentMode = ECombatCameraMode::Action;
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Shot pattern for %s's %s (%d target(s))"), *GetNameSafe(Caster), *GetNameSafe(Skill), ShotTargets.Num());
	return true;
}

void UCombatCameraDirector::HandleProjectileLaunched(AActor *Caster, AActor *Projectile)
{
	// B-371: the Cast shot, looking at the first projectile of the pattern's caster while it flies.
	if (!IsShotPlaying() || Caster != ShotCaster.Get() || bShotImpactShown || bShotCastShown)
	{
		return;
	}
	bShotCastShown = true; // review fix: once per pattern, followed or not
	ShotProjectile = ShotTuning.bFollowProjectile ? Projectile : nullptr;
	PlayShot(ShotCamera.Get(), ECameraShotMoment::Cast, FCombatCameraRules::ComputeShot(ECameraShotMoment::Cast, BuildShotContext(false), ShotTuning));
}

namespace
{
	TAutoConsoleVariable<int32> CVarCheckFraming(TEXT("wor.Camera.CheckFraming"), 0,
												 TEXT("B-372: 1 logs each code-built shot's framing (points on screen, clear sight, camera clear)."));

	/** B-403: a shot's motion blur on its camera while it moves; 0 hands the blur back to the project's own. */
	void SetShotMotionBlur(ACameraActor *CameraActor, float Amount)
	{
		if (!CameraActor || !CameraActor->GetCameraComponent())
		{
			return;
		}
		FPostProcessSettings &PostProcess = CameraActor->GetCameraComponent()->PostProcessSettings;
		PostProcess.bOverride_MotionBlurAmount = Amount > 0.0f;
		if (Amount > 0.0f)
		{
			PostProcess.MotionBlurAmount = Amount;
		}
	}
}

bool UCombatCameraDirector::PlayShot(UCombatCameraComponent *Camera, ECameraShotMoment Moment, const FCameraShot &Shot)
{
	if (CVarCheckFraming.GetValueOnGameThread() != 0)
	{
		// Review: the caster's and targets' own meshes and weapons don't count as blocking the view of them.
		TArray<const AActor *> Ignored;
		if (const AActor *Caster = ShotCaster.Get())
		{
			Ignored.Add(Caster);
		}
		for (const TWeakObjectPtr<AActor> &Target : ShotTargets)
		{
			if (const AActor *Actor = Target.Get())
			{
				Ignored.Add(Actor);
			}
		}
		const CameraFraming::FFramingReport Report = CameraFraming::CheckShot(GetWorld(), Moment, Shot, BuildShotContext(false), Ignored);
		UE_LOG(LogWoR, Display, TEXT("[CameraFraming] %s: %s"), *UEnum::GetValueAsString(Moment), *Report.ToString());
	}
	APlayerController *Viewer = GetViewer();
	if (!Viewer || !Camera)
	{
		return false;
	}
	UWorld *World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(ShotTimer);
	}
	if (ShotCamera.IsValid() && ShotCamera.Get() != Camera)
	{
		SetShotMotionBlur(ShotCamera->GetCameraActor(), 0.0f); // B-403: the rush's blur stays on the camera it left
	}
	ShotCamera = Camera;
	ShotMoment = Moment;
	// B-402 (R10, R11): no shake and no FOV punch while the viewer defends; a held punch is dropped, not replayed (Q-402).
	const bool bDefending = IsViewerDefending();
	if (bDefending && (Shot.ShakeScale > 0.0f || Shot.MotionBlur > 0.0f || !FMath::IsNearlyEqual(Shot.FieldOfView, FCombatCameraRules::DefaultFieldOfView)))
	{
		++DefencePunchesDropped;
		UE_LOG(LogWoRCombat, Verbose, TEXT("[CombatCamera] %s shot held steady: the viewer is defending"), *UEnum::GetValueAsString(Moment));
	}
	ActiveShot = FCombatCameraRules::SteadyForDefence(Shot, bDefending);
	ShotElapsed = 0.0f;
	ACameraActor *Existing = Camera->GetCameraActor();
	const bool bAlreadyViewed = Existing && Viewer->GetViewTarget() == Existing;
	// Move from where the camera is (no jump), except the melee rush (B-403, R5): it cuts to its own start, 420 cm back,
	// and pushes in from there, so it reads the same whatever the turn view was.
	if (bAlreadyViewed && ActiveShot.MotionBlur <= 0.0f)
	{
		ActiveShot.Start = Existing->GetActorTransform();
	}
	ACameraActor *CameraActor = Camera->ShowView(ECombatCameraMode::Action, ActiveShot.Start);
	if (!CameraActor)
	{
		return false;
	}
	CameraActor->GetCameraComponent()->SetFieldOfView(ActiveShot.FieldOfView);
	SetShotMotionBlur(CameraActor, ActiveShot.MotionBlur); // B-403 (R5): the melee rush
	const bool bMoves = !ActiveShot.Start.Equals(ActiveShot.End) || ShotProjectile.IsValid();
	if (!bAlreadyViewed)
	{
		Viewer->SetViewTargetWithBlend(CameraActor, bMoves ? CombatCameraTiming::ShotCutSeconds : ActiveShot.BlendSeconds);
	}
	if (ImpactShake && ActiveShot.ShakeScale > 0.0f && Viewer->PlayerCameraManager)
	{
		Viewer->PlayerCameraManager->StartCameraShake(ImpactShake, ActiveShot.ShakeScale);
	}
	if (bMoves && World)
	{
		World->GetTimerManager().SetTimer(ShotTimer, this, &UCombatCameraDirector::StepShot, CombatCameraTiming::ShotStepSeconds, true);
	}
	return true;
}

void UCombatCameraDirector::StepShot()
{
	UCombatCameraComponent *Camera = ShotCamera.Get();
	ACameraActor *CameraActor = Camera ? Camera->GetCameraActor() : nullptr;
	UWorld *World = GetWorld();
	if (!CameraActor || !World)
	{
		StopShotPattern();
		return;
	}
	ShotElapsed += CombatCameraTiming::ShotStepSeconds;
	const bool bFollowing = ShotMoment == ECameraShotMoment::Cast && ShotProjectile.IsValid();
	if (bFollowing)
	{
		ActiveShot.End = FCombatCameraRules::ComputeShot(ECameraShotMoment::Cast, BuildShotContext(false), ShotTuning).End;
	}
	const float Alpha = ShotElapsed / FMath::Max(ActiveShot.BlendSeconds, CombatCameraTiming::ShotStepSeconds);
	CameraActor->SetActorTransform(FCombatCameraRules::BlendShot(ActiveShot, Alpha));
	if (Alpha >= 1.0f && !bFollowing)
	{
		World->GetTimerManager().ClearTimer(ShotTimer); // the shot holds where it ended
		SetShotMotionBlur(CameraActor, 0.0f);			// B-403: the rush is over
	}
}

void UCombatCameraDirector::StopShotPattern()
{
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShotTimer);
	}
	if (UCombatCameraComponent *Camera = ShotCamera.Get())
	{
		SetShotMotionBlur(Camera->GetCameraActor(), 0.0f); // B-403: the turn and Defence views never blur
	}
	ShotCaster.Reset();
	ShotTargets.Reset();
	ShotProjectile.Reset();
	ShotCamera.Reset();
	bShotImpactShown = false;
	bShotCastShown = false;
	ShotElapsed = 0.0f;
}

FCameraShotContext UCombatCameraDirector::BuildShotContext(bool bCritical) const
{
	FCameraShotContext Context;
	if (const AActor *Caster = ShotCaster.Get())
	{
		Context.Caster = Caster->GetActorLocation();
	}
	for (const TWeakObjectPtr<AActor> &Target : ShotTargets)
	{
		if (const AActor *Actor = Target.Get())
		{
			Context.Targets.Add(Actor->GetActorLocation());
		}
	}
	if (const AActor *Projectile = ShotProjectile.Get())
	{
		Context.Projectile = Projectile->GetActorLocation();
	}
	Context.bRanged = bShotRanged;
	Context.bCritical = bCritical;
	Context.bAllowRush = !bCameraMovementOff; // B-403 (R15)
	return Context;
}

FString UCombatCameraDirector::GetShotString() const
{
	if (!IsShotPlaying())
	{
		return TEXT("shot: none");
	}
	const UCombatCameraComponent *Camera = ShotCamera.Get();
	return FString::Printf(TEXT("shot: %s for %s on %s's camera (fov %.0f, shake %.2f, blur %.2f over %.2f s%s)"), *UEnum::GetValueAsString(ShotMoment),
						   *GetNameSafe(ShotCaster.Get()), Camera ? *GetNameSafe(Camera->GetOwner()) : TEXT("no"),
						   ActiveShot.FieldOfView, ActiveShot.ShakeScale, ActiveShot.MotionBlur, ActiveShot.BlendSeconds,
						   ShotProjectile.IsValid() ? TEXT(", following the projectile") : TEXT(""));
}

EEnemyCameraHeight UCombatCameraDirector::GetFightCameraHeight() const
{
	const ACombatOrchestrator *Orchestrator = Cast<ACombatOrchestrator>(GetOwner());
	TArray<EEnemyCameraHeight> Heights;
	for (const AActor *Enemy : OpposingCombatants)
	{
		if (!IsValid(Enemy))
		{
			continue;
		}
		const UCharacterDataComponent *Data = Enemy->FindComponentByClass<UCharacterDataComponent>();
		if (Data && !Data->bIsAlive)
		{
			continue; // the boss fell: its minions are framed at their own height
		}
		const EEnemyCameraHeight Authored = Data && Data->CharacterData ? Data->CharacterData->EnemyCameraHeight : EEnemyCameraHeight::ByRank;
		const EEncounterRank Rank = Orchestrator ? Orchestrator->GetEnemyRank(Enemy) : EEncounterRank::Normal;
		Heights.Add(FBossFramingRules::ResolveHeight(Authored, Rank));
	}
	return FBossFramingRules::FightHeight(Heights);
}

float UCombatCameraDirector::PlayKillShot(AActor *Victim, EEncounterRank Rank)
{
	// B-406 review: the winning action can also cross a boss phase; its cinematic ends here (HUD back) so the kill
	// shot is not cut away when it would end later.
	EndBossCinematic(false);
	if (!bInCombat ||!ShowKillShot(Victim, FBossFramingRules::KillShotFor(Rank), true))
	{
		return 0.0f;
	}
	const float Hold = FBossFramingRules::KillShotSeconds(LastKillShot);
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Kill shot %s on %s (rank %s): holding %.1f s before the fight ends"),
		   *UEnum::GetValueAsString(LastKillShot), *GetNameSafe(Victim), *UEnum::GetValueAsString(Rank), Hold);
	return Hold;
}

void UCombatCameraDirector::PreviewKillShot(EKillShotKind Kind)
{
	AActor *Enemy = nullptr;
	for (AActor *Opposing : OpposingCombatants)
	{
		if (IsValid(Opposing))
		{
			Enemy = Opposing;
			break;
		}
	}
	UWorld *World = GetWorld();
	if (!bInCombat || !World || !ShowKillShot(Enemy, Kind, false))
	{
		UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] Kill shot preview: not in a fight, or no viewer or camera."));
		return;
	}
	World->GetTimerManager().SetTimer(KillShotPreviewTimer, this, &UCombatCameraDirector::EndKillShotPreview, FBossFramingRules::KillShotSeconds(Kind), false);
}

bool UCombatCameraDirector::ShowKillShot(AActor *Victim, EKillShotKind Kind, bool bHideOnBreakApart)
{
	APlayerController *Viewer = GetViewer();
	UCombatCameraComponent *Camera = IsValid(Victim) ? Victim->FindComponentByClass<UCombatCameraComponent>() : nullptr;
	if (!Viewer || !Camera)
	{
		return false;
	}
	ClearDefenceHandOff();
	StopSequence();
	StopShotPattern();
	const FCombatCameraFrame Frame = BuildFrame(Victim);
	// Camera movement off (D-078): the one static view holds instead; the hold and the look stay.
	const FTransform View = bCameraMovementOff ? FCombatCameraRules::ComputeView(ECombatCameraMode::Static, TurnViewStyle, Frame)
											   : FBossFramingRules::ComputeKillShot(Kind, Frame, Victim->GetActorLocation());
	ACameraActor *CameraActor = Camera->ShowView(ECombatCameraMode::KillShot, View);
	if (!CameraActor)
	{
		return false;
	}
	KillShotVictim = Victim;
	LastKillShot = Kind;
	bHasLastKillShot = true;
	bKillShotHidesVictim = bHideOnBreakApart;
	CurrentMode = ECombatCameraMode::KillShot;
	CameraActor->GetCameraComponent()->SetFieldOfView(FCombatCameraRules::DefaultFieldOfView);
	SetDarkened(CameraActor->GetCameraComponent(), FBossFramingRules::DarkensView(Kind));
	Viewer->SetViewTargetWithBlend(CameraActor, bCameraMovementOff ? 0.0f : FBossFramingRules::KillShotBlendSeconds);
	if (FBossFramingRules::BreaksApart(Kind))
	{
		if (UWorld *World = GetWorld())
		{
			World->GetTimerManager().SetTimer(BreakApartTimer, this, &UCombatCameraDirector::BreakApart, FBossFramingRules::BreakApartDelaySeconds, false);
		}
	}
	return true;
}

void UCombatCameraDirector::BreakApart()
{
	AActor *Victim = KillShotVictim.Get();
	if (!bInCombat || !Victim)
	{
		return;
	}
	// TODO: B-140 async loading. Loaded once per boss kill; a sync load here is a short hitch at most.
	UNiagaraSystem *Effect = BreakApartEffect.IsNull() ? nullptr : BreakApartEffect.LoadSynchronous();
	if (!Effect)
	{
		UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] %s's kill shot: no BreakApartEffect set, so it stays as it fell"), *GetNameSafe(Victim));
		return;
	}
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(Victim, Effect, Victim->GetActorLocation(), Victim->GetActorRotation());
	if (bKillShotHidesVictim)
	{
		Victim->SetActorHiddenInGame(true); // it broke apart; the fight ends within the hold
	}
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] %s breaks apart (%s)%s"), *GetNameSafe(Victim), *Effect->GetName(),
		   bKillShotHidesVictim ? TEXT("") : TEXT(", preview: not hidden"));
}

void UCombatCameraDirector::EndKillShotPreview()
{
	if (const AActor *Victim = KillShotVictim.Get())
	{
		const UCombatCameraComponent *Camera = Victim->FindComponentByClass<UCombatCameraComponent>();
		if (ACameraActor *CameraActor = Camera ? Camera->GetCameraActor() : nullptr)
		{
			SetDarkened(CameraActor->GetCameraComponent(), false);
		}
	}
	KillShotVictim.Reset();
	if (bInCombat && CurrentMode == ECombatCameraMode::KillShot)
	{
		ShowTurnView(); // a turn that started meanwhile already moved the view
	}
}

void UCombatCameraDirector::SetDarkened(UCameraComponent *Camera, bool bDarkened)
{
	if (!Camera)
	{
		return;
	}
	const float Saturation = bDarkened ? FBossFramingRules::DarkenedSaturation : 1.0f;
	Camera->PostProcessSettings.bOverride_ColorSaturation = bDarkened;
	Camera->PostProcessSettings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0f);
	Camera->PostProcessSettings.bOverride_VignetteIntensity = bDarkened;
	Camera->PostProcessSettings.VignetteIntensity = FBossFramingRules::DarkenedVignette;
}

FString UCombatCameraDirector::GetBossFramingString() const
{
	const FString KillShot = bHasLastKillShot
								 ? FString::Printf(TEXT("kill shot %s on %s"), *UEnum::GetValueAsString(LastKillShot), *GetNameSafe(KillShotVictim.Get()))
								 : FString(TEXT("kill shot none"));
	return FString::Printf(TEXT("height %s | %s"), bInCombat ? *UEnum::GetValueAsString(GetFightCameraHeight()) : TEXT("-"), *KillShot);
}

// ========================================
// B-406: per-boss cinematics (opening, phase change), on trial
// ========================================

UBossPhaseComponent *UCombatCameraDirector::FindFirstBoss() const
{
	for (AActor *Actor : OpposingCombatants)
	{
		if (UBossPhaseComponent *Phases = IsValid(Actor) ? Actor->FindComponentByClass<UBossPhaseComponent>() : nullptr)
		{
			return Phases;
		}
	}
	return nullptr;
}

bool UCombatCameraDirector::HasLocalViewer() const
{
	const APlayerController *Viewer = GetViewer();
	return Viewer && Viewer->GetLocalPlayer(); // a headless world's controller has none (as UCombatHUD checks)
}

bool UCombatCameraDirector::PlayBossOpening()
{
	if (!bInCombat)
	{
		return false;
	}
	for (AActor *Actor : OpposingCombatants)
	{
		const UBossPhaseComponent *Phases = IsValid(Actor) ? Actor->FindComponentByClass<UBossPhaseComponent>() : nullptr;
		const UBossData *Data = Phases ? Phases->GetBossData() : nullptr;
		if (Data && (!Data->OpeningSequence.IsNull() || BossCinematicPreviewSeconds > 0.0f))
		{
			return PlayBossCinematic(EBossCinematicKind::Opening, Actor, Data->OpeningSequence, INDEX_NONE);
		}
	}
	return false;
}

bool UCombatCameraDirector::PreviewBossCinematic(EBossCinematicKind Kind, int32 PhaseIndex)
{
	UBossPhaseComponent *Phases = FindFirstBoss();
	if (!bInCombat || !Phases || Kind == EBossCinematicKind::None)
	{
		UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] Boss cinematic preview: no fight or no boss in it"));
		return false;
	}
	const UBossData *Data = Phases->GetBossData();
	const TSoftObjectPtr<ULevelSequence> Sequence = Kind == EBossCinematicKind::Opening
		? (Data ? Data->OpeningSequence : TSoftObjectPtr<ULevelSequence>())
		: Phases->GetPhaseCinematic(PhaseIndex);
	if (Sequence.IsNull())
	{
		const float Seconds = BossCinematicPreviewSeconds > 0.0f ? BossCinematicPreviewSeconds : CombatCameraTiming::BossCinematicPreviewDefaultSeconds;
		HoldForBossCinematic(Kind, Phases->GetOwner(), Seconds);
		BossCinematicPhase = PhaseIndex;
		return true;
	}
	return PlayBossCinematic(Kind, Phases->GetOwner(), Sequence, PhaseIndex);
}

void UCombatCameraDirector::HandleBossPhaseReached(AActor *Boss, int32 PhaseIndex)
{
	const UBossPhaseComponent *Phases = IsValid(Boss) ? Boss->FindComponentByClass<UBossPhaseComponent>() : nullptr;
	if (bInCombat && Phases)
	{
		PlayBossCinematic(EBossCinematicKind::Phase, Boss, Phases->GetPhaseCinematic(PhaseIndex), PhaseIndex);
	}
}

bool UCombatCameraDirector::PlayBossCinematic(EBossCinematicKind Kind, AActor *Boss, const TSoftObjectPtr<ULevelSequence> &Sequence, int32 PhaseIndex)
{
	if (!bInCombat || !Boss)
	{
		return false;
	}
	if (Sequence.IsNull() && BossCinematicPreviewSeconds > 0.0f)
	{
		HoldForBossCinematic(Kind, Boss, BossCinematicPreviewSeconds); // debug preview of an empty field
		BossCinematicPhase = PhaseIndex;
		return true;
	}
	if (!FCombatCameraRules::ShouldPlayBossCinematic(!Sequence.IsNull(), HasLocalViewer(), SkillCinematics, bCameraMovementOff))
	{
		if (!Sequence.IsNull())
		{
			UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] %s cinematic of %s skipped (no local viewer, or cinematics off)"),
				   *UEnum::GetValueAsString(Kind), *GetNameSafe(Boss));
		}
		return false;
	}
	// TODO: B-140 async loading; one sync load per boss moment is fine for the trial.
	ULevelSequence *Loaded = Sequence.LoadSynchronous();
	FMovieSceneSequencePlaybackSettings Settings;
	ALevelSequenceActor *NewActor = nullptr;
	ULevelSequencePlayer *Player = Loaded ? ULevelSequencePlayer::CreateLevelSequencePlayer(this, Loaded, Settings, NewActor) : nullptr;
	if (!Player || !NewActor)
	{
		UE_LOG(LogWoRCombat, Warning, TEXT("[CombatCamera] Boss cinematic %s did not load or play; the fight goes on"), *Sequence.ToString());
		return false;
	}
	TArray<AActor *> Party;
	for (AActor *Member : LocalCombatants)
	{
		if (IsValid(Member))
		{
			Party.Add(Member);
		}
	}
	NewActor->SetBindingByTag(CombatCameraBindings::BossTag, {Boss});
	NewActor->SetBindingByTag(CombatCameraBindings::PartyTag, Party);

	// A second phase reached under a playing cinematic replaces it (the boss's newest state); the hold carries on.
	StopBossSequence(false);
	ClearDefenceHandOff();
	StopSequence();
	StopShotPattern();
	EnterBossCinematic(Kind, Boss, FCombatCameraRules::BossCinematicMaxSeconds, PhaseIndex);
	BossSequencePlayer = Player;
	BossSequenceActor = NewActor;
	BossCinematicName = Loaded->GetName();
	Player->OnFinished.AddDynamic(this, &UCombatCameraDirector::HandleBossSequenceFinished);
	Player->Play();
	return true;
}

void UCombatCameraDirector::HoldForBossCinematic(EBossCinematicKind Kind, AActor *Boss, float Seconds)
{
	if (!bInCombat || Kind == EBossCinematicKind::None)
	{
		return;
	}
	StopBossSequence(false);
	ClearDefenceHandOff();
	StopSequence();
	StopShotPattern();
	EnterBossCinematic(Kind, Boss, FMath::Min(Seconds, FCombatCameraRules::BossCinematicMaxSeconds), INDEX_NONE);
	BossCinematicName = TEXT("preview hold, no sequence");
}

void UCombatCameraDirector::EnterBossCinematic(EBossCinematicKind Kind, AActor *Boss, float CapSeconds, int32 PhaseIndex)
{
	const bool bWasPlaying = IsBossCinematicPlaying();
	BossCinematicKind = Kind;
	BossCinematicBoss = Boss;
	BossCinematicPhase = PhaseIndex;
	CurrentMode = ECombatCameraMode::Action;
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().SetTimer(BossCinematicCapTimer, this, &UCombatCameraDirector::FinishBossCinematic, CapSeconds, false);
	}
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Boss cinematic %s of %s: HUD hidden, the fight waits (cap %.1f s)"),
		   *UEnum::GetValueAsString(Kind), *GetNameSafe(Boss), CapSeconds);
	if (!bWasPlaying)
	{
		OnCinematicHudHidden.Broadcast(true);
	}
}

void UCombatCameraDirector::HandleBossSequenceFinished()
{
	EndBossCinematic(true);
}

void UCombatCameraDirector::FinishBossCinematic()
{
	EndBossCinematic(false);
}

void UCombatCameraDirector::EndBossCinematic(bool bFromPlayer)
{
	if (!IsBossCinematicPlaying())
	{
		return;
	}
	const EBossCinematicKind Kind = BossCinematicKind;
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BossCinematicCapTimer);
	}
	StopBossSequence(bFromPlayer);
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Boss cinematic %s of %s ended: HUD back"), *UEnum::GetValueAsString(Kind),
		   *GetNameSafe(BossCinematicBoss.Get()));
	BossCinematicKind = EBossCinematicKind::None;
	BossCinematicBoss.Reset();
	BossCinematicPhase = INDEX_NONE;
	BossCinematicName.Reset();
	OnCinematicHudHidden.Broadcast(false);
	if (bInCombat)
	{
		ShowTurnView(); // before turn 1 nobody is viewed yet: turn 1's own start shows it
		OnBossCinematicFinished.Broadcast(Kind);
	}
}

void UCombatCameraDirector::StopBossSequence(bool bDeferDestroy)
{
	if (BossSequencePlayer)
	{
		BossSequencePlayer->OnFinished.RemoveAll(this);
		if (!bDeferDestroy)
		{
			BossSequencePlayer->Stop();
		}
	}
	if (IsValid(BossSequenceActor))
	{
		UWorld *World = GetWorld();
		if (bDeferDestroy && World)
		{
			// Called from the player's own OnFinished: the actor goes next tick (as StopSequence does).
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(BossSequenceActor.Get(), [Actor = BossSequenceActor.Get()]()
			{
				Actor->Destroy();
			}));
		}
		else
		{
			BossSequenceActor->Destroy();
		}
	}
	BossSequencePlayer = nullptr;
	BossSequenceActor = nullptr;
}

FString UCombatCameraDirector::GetBossCinematicString() const
{
	if (!IsBossCinematicPlaying())
	{
		return TEXT("boss cinematic: none");
	}
	const FString Phase = BossCinematicPhase != INDEX_NONE ? FString::Printf(TEXT(" %d"), BossCinematicPhase) : FString();
	return FString::Printf(TEXT("boss cinematic: %s%s of %s (%s)"), *UEnum::GetValueAsString(BossCinematicKind), *Phase,
						   *GetNameSafe(BossCinematicBoss.Get()), *BossCinematicName);
}

void UCombatCameraDirector::StartTurnChange(AActor *From, AActor *To, bool bSomeoneActed, bool bBroadcast)
{
	// B-404 (study R12, R16, Q-404): between turns, the camera goes wide, then blends behind the next hero.
	UCombatCameraComponent *Camera = To ? To->FindComponentByClass<UCombatCameraComponent>() : nullptr;
	APlayerController *Viewer = GetViewer();
	UWorld *World = GetWorld();
	FTurnChangePlan Plan;
	if (Camera && Viewer && World)
	{
		const ECombatCameraMode NextMode = FCombatCameraRules::SelectMode(IsViewersCharacter(To), false, bCameraMovementOff);
		Plan = FCombatCameraRules::PlanTurnChange(bSomeoneActed, To == From, NextMode, TurnViewStyle, BuildFrame(To));
	}
	if (!Plan.bWideBeat)
	{
		ShowTurnView(); // as before B-404: first turn, same actor, movement off, an enemy's turn
		if (bBroadcast)
		{
			BroadcastHandover(From, To);
		}
		return;
	}

	StopShotPattern(); // the beat replaces the last pattern's hold, as the turn view did
	PendingTurnChange = Plan;
	HandoverFrom = From;
	HandoverTo = To;
	bHandoverPending = true;
	bHandoverBroadcasts = bBroadcast;
	CurrentMode = ECombatCameraMode::NotYourTurn; // the wide; HandleWideBeatEnd checks nothing else took the view
	ACameraActor *Existing = Camera->GetCameraActor();
	if (Existing && Viewer->GetViewTarget() == Existing)
	{
		MoveCameraOnShotTimer(Camera, Plan.WideView, Plan.BeatBlendSeconds); // already looking through it: no jump
	}
	else if (ACameraActor *CameraActor = Camera->ShowView(ECombatCameraMode::NotYourTurn, Plan.WideView))
	{
		CameraActor->GetCameraComponent()->SetFieldOfView(FCombatCameraRules::DefaultFieldOfView);
		Viewer->SetViewTargetWithBlend(CameraActor, Plan.BeatBlendSeconds);
	}
	World->GetTimerManager().SetTimer(WideBeatTimer, this, &UCombatCameraDirector::HandleWideBeatEnd, Plan.BeatSeconds, false);
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Wide beat: %s -> %s, handover blend in %.2f s%s"), *GetNameSafe(From), *GetNameSafe(To),
		   Plan.BeatSeconds, bBroadcast ? TEXT("") : TEXT(" (preview)"));
}

void UCombatCameraDirector::HandleWideBeatEnd()
{
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WideBeatTimer);
	}
	if (!bHandoverPending)
	{
		return;
	}
	bHandoverPending = false;
	AActor *From = HandoverFrom.Get();
	AActor *To = HandoverTo.Get();
	HandoverFrom.Reset();
	HandoverTo.Reset();
	UCombatCameraComponent *Camera = To ? To->FindComponentByClass<UCombatCameraComponent>() : nullptr;
	// A setting change, a sequence or the Defence view that took the view during the beat keeps it.
	const bool bStillWide = bInCombat && CurrentMode == ECombatCameraMode::NotYourTurn && !bCameraMovementOff && !PlayingSequence &&
							!IsShotPlaying() && !DefenceHandOffTimer.IsValid();
	if (bStillWide && Camera)
	{
		CurrentMode = ECombatCameraMode::Turn;
		// B-405 / B-399: the turn view as it stands now (boss height, menu depth), not the plan's plain one.
		MoveCameraOnShotTimer(Camera, GetTurnViewWithHeight(BuildFrame(To)), PendingTurnChange.HandoverBlendSeconds);
	}
	// Same frame as the blend's start: the HUD steps the turn strip and lifts the card now (study R16, F4).
	if (bHandoverBroadcasts)
	{
		BroadcastHandover(From, To);
	}
}

void UCombatCameraDirector::FlushPendingHandover()
{
	if (bHandoverPending)
	{
		HandleWideBeatEnd();
	}
}

void UCombatCameraDirector::BroadcastHandover(AActor *From, AActor *To)
{
	++HandoverCount;
	UE_LOG(LogWoRCombat, Verbose, TEXT("[CombatCamera] Handover %s -> %s (%d this fight)"), *GetNameSafe(From), *GetNameSafe(To), HandoverCount);
	OnTurnHandover.Broadcast(From, To);
}

void UCombatCameraDirector::MoveCameraOnShotTimer(UCombatCameraComponent *Camera, const FTransform &End, float Seconds)
{
	ACameraActor *CameraActor = Camera ? Camera->GetCameraActor() : nullptr;
	UWorld *World = GetWorld();
	if (!CameraActor || !World)
	{
		return;
	}
	// B-371's shot timer and blend (StepShot / BlendShot); no pattern is playing, so nothing follows a projectile.
	World->GetTimerManager().ClearTimer(ShotTimer);
	ShotCamera = Camera;
	ShotProjectile.Reset();
	ActiveShot = FCameraShot();
	ActiveShot.Start = CameraActor->GetActorTransform();
	ActiveShot.End = End;
	ActiveShot.BlendSeconds = Seconds;
	ActiveShot.FieldOfView = FCombatCameraRules::DefaultFieldOfView;
	ShotElapsed = 0.0f;
	CameraActor->GetCameraComponent()->SetFieldOfView(ActiveShot.FieldOfView);
	World->GetTimerManager().SetTimer(ShotTimer, this, &UCombatCameraDirector::StepShot, CombatCameraTiming::ShotStepSeconds, true);
}

FString UCombatCameraDirector::GetHandoverString() const
{
	FString State = TEXT("handover: none pending");
	if (bHandoverPending)
	{
		const UWorld *World = GetWorld();
		const float Left = World ? World->GetTimerManager().GetTimerRemaining(WideBeatTimer) : 0.0f;
		State = FString::Printf(TEXT("handover: wide beat %s -> %s, blend %.2f s in %.2f s%s"), *GetNameSafe(HandoverFrom.Get()),
								*GetNameSafe(HandoverTo.Get()), PendingTurnChange.HandoverBlendSeconds, Left,
								bHandoverBroadcasts ? TEXT("") : TEXT(" (preview)"));
	}
	return FString::Printf(TEXT("%s; %d sent this fight%s"), *State, HandoverCount, bActedSinceTurnStart ? TEXT("; acted this turn") : TEXT(""));
}

bool UCombatCameraDirector::PreviewHandover(int32 PartyIndex)
{
	if (!bInCombat || LocalCombatants.IsEmpty())
	{
		return false;
	}
	int32 Index = PartyIndex;
	if (Index == INDEX_NONE)
	{
		const int32 Viewed = LocalCombatants.IndexOfByKey(ViewedActor.Get());
		Index = (Viewed + 1) % LocalCombatants.Num(); // not in the party (an enemy's turn): the first member
	}
	AActor *To = LocalCombatants.IsValidIndex(Index) ? LocalCombatants[Index].Get() : nullptr;
	if (!IsValid(To) || To == ViewedActor.Get())
	{
		return false;
	}
	FlushPendingHandover();
	StartTurnChange(ViewedActor.Get(), To, true, false);
	return bHandoverPending;
}
