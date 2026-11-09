// CombatCameraDirector.h
// Picks which combat camera the viewer sees and in which mode (Design/CombatCamera.md §1-3).
// Lives on the combat orchestrator; driven by turn start and action start. Plays the
// skill's own Level Sequence (a signature move) bound to the caster and targets, else the
// code-built shot patterns (B-371, Q-371: wind-up, Cast, Impact / Spread; the next turn blends back).

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
struct FAction;

/** B-404 (study R16, F4): the turn passes From -> To; From is null on a fight's first turn and may equal To. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTurnHandover, AActor *, From, AActor *, To);

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
	/** B-398: also the player's saved setting (USaveSubsystem), so it holds for the next fight. */
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

	/** Turn-view style (D-074 (2)). B-398: each fight starts from the player's saved setting (USaveSubsystem); this
	 *  value is used only where there is no save subsystem. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera|Settings", meta = (ToolTip = "Set by the player's saved setting at each fight start (wor.Camera.Style); not editable here"))
	ETurnViewStyle TurnViewStyle = ETurnViewStyle::CentredWide;

	/** B-398 (wor.Camera): where the viewed character and the enemy centre land in its turn view, e.g. "layout
	 *  OverTheShoulder: Hero_1 lower-left (-0.44, -0.61), enemy centre middle-centre (0.00, 0.00)". */
	FString GetTurnLayoutString() const;

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

	/** B-404 (study R16, F4, Q-404): once per turn start, on the frame the camera starts its move to the new turn's
	 *  view (after the wide beat, when there is one). The HUD binds it for the turn-strip step and the active-card
	 *  highlight, so all three move together. From: the actor the camera followed before (null on the first turn). */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Camera")
	FOnTurnHandover OnTurnHandover;

	/** B-404 (wor.Camera): e.g. "handover: wide beat Hero_1 -> Hero_2, blend 0.50 s in 0.32 s; 4 sent this fight". */
	FString GetHandoverString() const;
	int32 GetHandoverCount() const { return HandoverCount; }

	/** B-404 (wor.Camera.Handover): plays the wide beat and the handover blend to party member PartyIndex's turn
	 *  view (INDEX_NONE: the member after the viewed one), as a turn change would. The turn, the HUD and the event
	 *  stay as they are; the next turn start or setting change ends it. False when it can't play. */
	bool PreviewHandover(int32 PartyIndex);

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

	/** B-404: the turn start's camera move (FCombatCameraRules::PlanTurnChange): a wide beat, then the handover
	 *  blend on WideBeatTimer; or the turn view at once. bBroadcast false for a preview. */
	void StartTurnChange(AActor *From, AActor *To, bool bSomeoneActed, bool bBroadcast);
	/** B-404: the beat's end: the handover blend (unless something else took the view) and the event. */
	void HandleWideBeatEnd();
	/** B-404: a pending handover runs now (an action or a new turn comes before the beat ends): once per turn start. */
	void FlushPendingHandover();
	void BroadcastHandover(AActor *From, AActor *To);
	/** B-404: moves Camera's actor from where it is to End over Seconds on the shot timer (StepShot); the viewer
	 *  already looks through it, or is blending to it. */
	void MoveCameraOnShotTimer(UCombatCameraComponent *Camera, const FTransform &End, float Seconds);

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

	// B-404: the wide beat and the party handover of the current turn start.
	FTimerHandle WideBeatTimer;
	FTurnChangePlan PendingTurnChange;
	TWeakObjectPtr<AActor> HandoverFrom;
	TWeakObjectPtr<AActor> HandoverTo;
	bool bHandoverPending = false;
	bool bHandoverBroadcasts = false;
	bool bActedSinceTurnStart = false;
	int32 HandoverCount = 0;
};
