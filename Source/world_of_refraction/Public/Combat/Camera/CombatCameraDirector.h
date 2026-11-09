// CombatCameraDirector.h
// Picks which combat camera the viewer sees and in which mode (Design/CombatCamera.md §1-3).
// Lives on the combat orchestrator; driven by turn start and action start. Plays the
// skill's own Level Sequence (a signature move) bound to the caster and targets, else the
// code-built shot patterns (B-371, Q-371: wind-up, Cast, Impact / Spread; the next turn blends back).
// B-406: per-boss opt-in cinematics (an opening before turn 1, one per phase), with the HUD hidden and the fight held.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "CombatCameraDirector.generated.h"

class UCombatCameraComponent;
class UActionExecutor;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class APlayerController;
class UCameraShakeBase;
class UBossPhaseComponent;
struct FAction;

/** B-406: the whole combat HUD hides (true) for a boss cinematic and comes back (false) when it ends. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCinematicHudHidden, bool, bHidden);
/** B-406: a boss cinematic ended (its sequence, the cap, or a stop): the fight may go on. Not sent when the fight ends. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossCinematicFinished, EBossCinematicKind, Kind);

UCLASS(ClassGroup = (Combat))
class WORLD_OF_REFRACTION_API UCombatCameraDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Gives every combatant a camera and starts following turns. Call before the first turn. */
	void BeginCombat(const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty, UActionExecutor *Executor);

	/** Stops any sequence, removes the cameras and gives the view back. Safe to call twice. */
	void EndCombat();

	// TODO: settings menu (D-024 CommonUI). For now: Details panel or wor.Camera.Style / wor.Camera.MovementOff.
	void SetTurnViewStyle(ETurnViewStyle NewStyle);
	void SetCameraMovementOff(bool bOff);
	void SetSkillCinematics(ESkillCinematics NewSetting);
	void SetEnemySkillCinematics(bool bOn);
	void SetSkipCinematicAfterHit(bool bSkip);

	bool IsInCombat() const { return bInCombat; }
	ETurnViewStyle GetTurnViewStyle() const { return TurnViewStyle; }
	bool IsCameraMovementOff() const { return bCameraMovementOff; }
	ECombatCameraMode GetCurrentMode() const { return CurrentMode; }
	AActor *GetViewedActor() const { return ViewedActor.Get(); }
	const ULevelSequence *GetPlayingSequence() const { return PlayingSequence; }
	int32 GetCameraCount() const { return Cameras.Num(); }

	/** B-370 (wor.Camera): the last action's cinematic window and hand-off, e.g. "window ends 1.25 s (first
	 *  defendable 2.00 s) -> Defence, pending". */
	FString GetCinematicWindowString() const;

	/** Turn-view style (D-074 (2)). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Settings")
	ETurnViewStyle TurnViewStyle = ETurnViewStyle::CentredWide;

	/** Motion-sickness setting: one static wide view. Also skips cinematics unless SkillCinematics is On. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Settings")
	bool bCameraMovementOff = false;

	/** D-078 (1): skill cinematics, separate from camera movement. Untested default (docs/PIE/B-156.md). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Settings")
	ESkillCinematics SkillCinematics = ESkillCinematics::FollowCameraMovement;

	/** D-078 (2): enemy skills play their cinematics too. Untested default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Settings")
	bool bEnemySkillCinematics = true;

	/** D-078 (3): cut a cinematic once its caster's first hit lands, instead of running to its end
	 *  or the next turn. Untested default (off). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Settings")
	bool bSkipCinematicAfterHit = false;

	/** OBSOLETE (B-371, Q-371: code-built shot patterns replace the templates; delete in the D-124 sweep). No
	 *  longer read: a skill without its own CameraSequence plays the shot patterns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Templates")
	TMap<ECameraTemplateKind, TSoftObjectPtr<ULevelSequence>> TemplateSequences;

	/** B-371: the camera shake an impact shot plays, scaled by the shot (larger on a crit). Empty: no shake, the FOV
	 *  punch alone. TODO: a shake class (editor step, B-157). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Shots")
	TSubclassOf<UCameraShakeBase> ImpactShake;

	/** B-371 (wor.Camera): the shot pattern now playing, e.g. "shot: Impact on Goblin (fov 82, shake 0.60)". */
	FString GetShotString() const;

	/** B-406: plays the first opposing boss's opening cinematic (UBossData::OpeningSequence). True: it plays, and turn 1
	 *  waits for OnBossCinematicFinished. False (no boss, field empty, no local viewer, cinematics off): nothing plays. */
	bool PlayBossOpening();

	/** B-406 (wor.Camera.BossCinematic): plays Kind's sequence of the first opposing boss (PhaseIndex for Phase); an
	 *  empty field holds for the preview time with no sequence, so the HUD hide and the hold can be judged. */
	bool PreviewBossCinematic(EBossCinematicKind Kind, int32 PhaseIndex);

	/** B-406: the boss-cinematic hold with no sequence: the HUD hides, the fight waits, and it ends after Seconds or on
	 *  FinishBossCinematic. Used by the preview and by the specs (a headless world has no viewer). */
	void HoldForBossCinematic(EBossCinematicKind Kind, AActor *Boss, float Seconds);

	/** B-406: ends a playing boss cinematic now (also its end and the cap): the HUD comes back, the turn view shows and
	 *  OnBossCinematicFinished fires. Nothing playing: nothing happens. */
	void FinishBossCinematic();

	bool IsBossCinematicPlaying() const { return BossCinematicKind != EBossCinematicKind::None; }
	EBossCinematicKind GetBossCinematicKind() const { return BossCinematicKind; }

	/** B-406 (wor.Camera): e.g. "boss cinematic: Phase 1 of Boss_1 (LS_BossPhase2)" or "boss cinematic: none". */
	FString GetBossCinematicString() const;

	UPROPERTY(BlueprintAssignable, Category = "Camera|Events")
	FOnCinematicHudHidden OnCinematicHudHidden;

	UPROPERTY(BlueprintAssignable, Category = "Camera|Events")
	FOnBossCinematicFinished OnBossCinematicFinished;

	/** B-406 debug: above 0, a boss whose cinematic field is empty still holds this long (no sequence, no viewer
	 *  needed), to judge the HUD hide and the hold before any sequence is made. 0 (default): off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Debug", meta = (ClampMin = "0.0"))
	float BossCinematicPreviewSeconds = 0.0f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleActorTurnStarted(AActor *Actor, int32 TurnNumber);

	UFUNCTION()
	void HandleActionStarted(AActor *Executor, const FAction &Action, int32 EnergyCost);

	UFUNCTION()
	void HandleSequenceFinished();

	UFUNCTION()
	void HandleDamageDealt(AActor *Attacker, AActor *Target, int32 Damage, bool bCritical);

	UFUNCTION()
	void HandleProjectileLaunched(AActor *Caster, AActor *Projectile);

	UFUNCTION()
	void HandleBossPhaseReached(AActor *Boss, int32 PhaseIndex);

	UFUNCTION()
	void HandleBossSequenceFinished();

	/** B-406: plays Sequence as Kind's cinematic for Boss (or the preview hold when it is empty and the preview is on). */
	bool PlayBossCinematic(EBossCinematicKind Kind, AActor *Boss, const TSoftObjectPtr<ULevelSequence> &Sequence, int32 PhaseIndex);
	void EnterBossCinematic(EBossCinematicKind Kind, AActor *Boss, float CapSeconds, int32 PhaseIndex);
	void EndBossCinematic(bool bFromPlayer);
	void StopBossSequence(bool bDeferDestroy);
	UBossPhaseComponent *FindFirstBoss() const;
	bool HasLocalViewer() const;

	/** B-371: starts the code-built shots for Action (the wind-up shot on the caster's camera). False when it
	 *  could not (no camera or no viewer): nothing is left playing. */
	bool StartShotPattern(AActor *Caster, const FAction &Action, const USkillDataBase *Skill);
	/** B-371: shows Shot on Camera: the move runs on a short timer from where that camera is now, when the viewer
	 *  already looks through it, else the view blends to it. */
	bool PlayShot(UCombatCameraComponent *Camera, ECameraShotMoment Moment, const FCameraShot &Shot);
	void StepShot();
	void StopShotPattern();
	bool IsShotPlaying() const { return ShotCaster.IsValid(); }
	FCameraShotContext BuildShotContext(bool bCritical) const;

	/** B-370 (D-162): sets the window-end timer when one of the viewer's characters is targeted by someone else.
	 *  True when the window ends at once and the Defence view is already showing (no cinematic then). */
	bool ScheduleDefenceHandOff(AActor *Executor, const FAction &Action);
	void HandleCinematicWindowEnd();
	void ClearDefenceHandOff();
	/** Behind the viewer's targeted characters, the attacker in front. */
	void ShowDefenceView();

	/** After a settings change: cuts a playing cinematic the settings now skip. */
	void StopCinematicIfNowSkipped();

	/** Shows the turn / not-your-turn / static view for the actor whose turn it is. */
	void ShowTurnView();
	ULevelSequence *ResolveSequence(const FAction &Action) const;
	void PlaySequence(ULevelSequence *Sequence, AActor *Caster, const TArray<AActor *> &Targets);
	/** bDeferDestroy: called from the player's own OnFinished, so the actor goes next tick. */
	void StopSequence(bool bDeferDestroy = false);
	FCombatCameraFrame BuildFrame(AActor *Focus) const;
	bool IsViewersCharacter(const AActor *Actor) const;
	APlayerController *GetViewer() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCombatCameraComponent>> Cameras;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> LocalCombatants;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> OpposingCombatants;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> SequencePlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequence> PlayingSequence;

	TWeakObjectPtr<UActionExecutor> BoundExecutor;
	TWeakObjectPtr<AActor> ViewedActor;
	TWeakObjectPtr<AActor> SequenceCaster;
	uint64 SequenceStartFrame = 0;
	TWeakObjectPtr<AActor> ViewTargetBeforeCombat;
	ECombatCameraMode CurrentMode = ECombatCameraMode::None;
	bool bInCombat = false;

	// B-370: the pending Defence hand-off of the current action.
	FTimerHandle DefenceHandOffTimer;
	TArray<TWeakObjectPtr<AActor>> HandOffDefenders;
	TWeakObjectPtr<AActor> HandOffAttacker;
	bool bHasLastWindow = false;
	float LastWindowEnd = 0.0f;
	FCinematicWindowTiming LastWindowTiming;
	ECombatCameraMode LastHandOffMode = ECombatCameraMode::None;

	// B-371: the shot pattern of the current action.
	TWeakObjectPtr<AActor> ShotCaster;
	TArray<TWeakObjectPtr<AActor>> ShotTargets;
	TWeakObjectPtr<AActor> ShotProjectile;
	TWeakObjectPtr<UCombatCameraComponent> ShotCamera;
	FSkillCameraTuning ShotTuning;
	bool bShotRanged = false;
	bool bShotImpactShown = false;
	bool bShotCastShown = false;
	uint64 ShotStartFrame = 0;
	ECameraShotMoment ShotMoment = ECameraShotMoment::WindUp;
	FCameraShot ActiveShot;
	float ShotElapsed = 0.0f;
	FTimerHandle ShotTimer;

	// B-406: the boss cinematic now playing (per fight, on the orchestrator's component: D-064).
	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> BossSequencePlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> BossSequenceActor;

	EBossCinematicKind BossCinematicKind = EBossCinematicKind::None;
	TWeakObjectPtr<AActor> BossCinematicBoss;
	int32 BossCinematicPhase = INDEX_NONE;
	FString BossCinematicName;
	FTimerHandle BossCinematicCapTimer;
	TArray<TWeakObjectPtr<UBossPhaseComponent>> BoundBossPhases;
};
