// CombatCameraDirector.cpp

#include "Combat/Camera/CombatCameraDirector.h"
#include "Combat/Camera/CombatCameraComponent.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Actions/ActionStructs.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "WoRLog.h"

namespace CombatCameraTiming
{
	// TODO: tune in PIE (B-157).
	constexpr float TurnBlendSeconds = 0.6f;
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
	StopSequence();

	if (ACombatOrchestrator *Orchestrator = Cast<ACombatOrchestrator>(GetOwner()))
	{
		Orchestrator->OnActorTurnStarted.RemoveDynamic(this, &UCombatCameraDirector::HandleActorTurnStarted);
	}
	if (UActionExecutor *Executor = BoundExecutor.Get())
	{
		Executor->OnActionStarted.RemoveDynamic(this, &UCombatCameraDirector::HandleActionStarted);
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
	if (bInCombat && !PlayingSequence)
	{
		ShowTurnView();
	}
}

void UCombatCameraDirector::SetCameraMovementOff(bool bOff)
{
	bCameraMovementOff = bOff;
	if (!bInCombat)
	{
		return;
	}
	if (bOff)
	{
		StopSequence();
	}
	ShowTurnView();
}

void UCombatCameraDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndCombat();
	Super::EndPlay(EndPlayReason);
}

void UCombatCameraDirector::HandleActorTurnStarted(AActor *Actor, int32 TurnNumber)
{
	ViewedActor = Actor;
	StopSequence();
	ShowTurnView();
}

void UCombatCameraDirector::HandleActionStarted(AActor *Executor, const FAction &Action, int32 EnergyCost)
{
	if (!bInCombat)
	{
		return;
	}
	// An Execution turn (a ritual firing) has no turn-start broadcast: frame whoever acts.
	const bool bNewActor = Executor && Executor != ViewedActor.Get();
	if (bNewActor)
	{
		ViewedActor = Executor;
	}
	ULevelSequence *Sequence = bCameraMovementOff ? nullptr : ResolveSequence(Action);
	if (!Sequence)
	{
		// No sequence for this action: the turn view stays (B-157 adds the templates).
		if (bNewActor)
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
	if (bInCombat)
	{
		ShowTurnView();
	}
}

void UCombatCameraDirector::ShowTurnView()
{
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
	const USkillDataBase *Skill = Action.ActionType == EActionType::Spell ? static_cast<const USkillDataBase *>(Action.SpellData)
																		  : Action.SkillData;
	TSoftObjectPtr<ULevelSequence> Soft;
	if (Skill)
	{
		Soft = Skill->CameraSequence;
	}
	if (Soft.IsNull())
	{
		if (const TSoftObjectPtr<ULevelSequence> *Template = TemplateSequences.Find(FCombatCameraRules::TemplateKindFor(Skill, Action.ActionType)))
		{
			Soft = *Template;
		}
	}
	// TODO: B-140 async loading. Sequences are small; a sync load at action start is fine for now.
	return Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
}

void UCombatCameraDirector::PlaySequence(ULevelSequence *Sequence, AActor *Caster, const TArray<AActor *> &Targets)
{
	StopSequence();
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
