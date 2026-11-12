// CombatCameraDirector.cpp

#include "Combat/Camera/CombatCameraDirector.h"
#include "Combat/Camera/CombatCameraComponent.h"
#include "Combat/Camera/CameraFramingChecks.h"
#include "HAL/IConsoleManager.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Actions/ActionStructs.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/WoRFightSubsystem.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Character/CharacterDataComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
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
}

namespace
{
	const USkillDataBase *GetActionSkill(const FAction &Action)
	{
		return Action.ActionType == EActionType::Spell ? static_cast<const USkillDataBase *>(Action.SpellData) : Action.SkillData;
	}
}

namespace CombatCameraBindings
{
	const FName CasterTag(TEXT("Caster"));
	const FName TargetsTag(TEXT("Targets"));
}

void UCombatCameraDirector::BeginCombat(const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty, UActionExecutor *Executor)
{
	EndCombat();
	bInCombat = true;
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

	if (APlayerController *Viewer = GetViewer())
	{
		ViewTargetBeforeCombat = Viewer->GetViewTarget();
	}
	UE_LOG(LogWoRCombat, Log, TEXT("[CombatCamera] Fight started: %d cameras"), Cameras.Num());
}

void UCombatCameraDirector::EndCombat()
{
	if (!bInCombat)
	{
		return;
	}
	bInCombat = false;
	ClearDefenceHandOff();
	StopSequence();
	StopShotPattern();

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
	if (bInCombat && !PlayingSequence && !IsShotPlaying())
	{
		ShowTurnView();
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
	ViewedActor = Actor;
	ClearDefenceHandOff();
	StopSequence();
	ShowTurnView();
}

void UCombatCameraDirector::HandleActionStarted(AActor *Executor, const FAction &Action, int32 EnergyCost)
{
	if (!bInCombat)
	{
		return;
	}
	ClearDefenceHandOff();
	// An Execution turn (a ritual firing) has no turn-start broadcast: frame whoever acts.
	const bool bNewActor = Executor && Executor != ViewedActor.Get();
	if (bNewActor)
	{
		ViewedActor = Executor;
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
		StopShotPattern();
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
	StopShotPattern(); // B-371: the end of a pattern blends back to the turn view
	AActor *Actor = ViewedActor.Get();
	APlayerController *Viewer = GetViewer();
	UCombatCameraComponent *Camera = Actor ? Actor->FindComponentByClass<UCombatCameraComponent>() : nullptr;
	if (!Viewer || !Camera)
	{
		return;
	}
	CurrentMode = FCombatCameraRules::SelectMode(IsViewersCharacter(Actor), false, bCameraMovementOff);
	const FTransform View = FCombatCameraRules::ComputeView(CurrentMode, TurnViewStyle, BuildFrame(Actor));
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
}

void UCombatCameraDirector::StopSequence(bool bDeferDestroy)
{
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
	return World ? World->GetFirstPlayerController() : nullptr;
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
	if (!PlayShot(Camera, ECameraShotMoment::WindUp, FCombatCameraRules::ComputeShot(ECameraShotMoment::WindUp, BuildShotContext(false), ShotTuning)))
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
	ShotCamera = Camera;
	ShotMoment = Moment;
	// B-402 (R10, R11): no shake and no FOV punch while the viewer defends; a held punch is dropped, not replayed (Q-402).
	const bool bDefending = IsViewerDefending();
	if (bDefending && (Shot.ShakeScale > 0.0f || !FMath::IsNearlyEqual(Shot.FieldOfView, FCombatCameraRules::DefaultFieldOfView)))
	{
		++DefencePunchesDropped;
		UE_LOG(LogWoRCombat, Verbose, TEXT("[CombatCamera] %s shot held steady: the viewer is defending"), *UEnum::GetValueAsString(Moment));
	}
	ActiveShot = FCombatCameraRules::SteadyForDefence(Shot, bDefending);
	ShotElapsed = 0.0f;
	ACameraActor *Existing = Camera->GetCameraActor();
	const bool bAlreadyViewed = Existing && Viewer->GetViewTarget() == Existing;
	if (bAlreadyViewed)
	{
		ActiveShot.Start = Existing->GetActorTransform(); // move from where the camera is: no jump
	}
	ACameraActor *CameraActor = Camera->ShowView(ECombatCameraMode::Action, ActiveShot.Start);
	if (!CameraActor)
	{
		return false;
	}
	CameraActor->GetCameraComponent()->SetFieldOfView(ActiveShot.FieldOfView);
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
	}
}

void UCombatCameraDirector::StopShotPattern()
{
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShotTimer);
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
	return Context;
}

FString UCombatCameraDirector::GetShotString() const
{
	if (!IsShotPlaying())
	{
		return TEXT("shot: none");
	}
	const UCombatCameraComponent *Camera = ShotCamera.Get();
	return FString::Printf(TEXT("shot: %s for %s on %s's camera (fov %.0f, shake %.2f%s)"), *UEnum::GetValueAsString(ShotMoment),
						   *GetNameSafe(ShotCaster.Get()), Camera ? *GetNameSafe(Camera->GetOwner()) : TEXT("no"),
						   ActiveShot.FieldOfView, ActiveShot.ShakeScale, ShotProjectile.IsValid() ? TEXT(", following the projectile") : TEXT(""));
}
