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
class UCombatCommandMenuSubsystem;
struct FAction;

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

	/** B-399 (study R2, R3): the command menu's depth (0 main, 1 a sub-menu; UCombatCommandMenuSubsystem::
	 *  OnMenuDepthChanged, or wor.Camera.MenuDepth). On the viewer's own turn view, one short blend to the turn view
	 *  for that depth; then the camera holds still until the depth changes again. Static, a cinematic or a held shot:
	 *  the depth is kept and nothing moves. */
	void SetMenuDepth(int32 NewDepth);
	int32 GetMenuDepth() const { return MenuDepth; }
	/** B-399 (debug, specs): the menu-depth camera moves this fight. */
	int32 GetMenuStepCount() const { return MenuStepCount; }

	/** B-400 (study R3 / §9, Q-400): the command menu's highlighted target (UCombatCommandMenuSubsystem::
	 *  OnTargetHighlighted). On the viewer's own turn view with at least MinTargetsForTargetView targets: the target
	 *  view (ComputeTargetView; Over-the-Shoulder cuts, the others re-aim briefly). Null (the picker closed) while a
	 *  target view shows: back to the turn view at the menu's depth. Static, a cinematic or a held shot: nothing moves. */
	void SetHighlightedTarget(AActor *Target, int32 TargetCount);
	AActor *GetHighlightedTarget() const { return HighlightedTarget.Get(); }
	bool IsTargetViewShowing() const { return bTargetViewShowing; }
	/** B-400 (debug, specs): the target-view camera moves this fight (to a target, between targets and back). */
	int32 GetTargetMoveCount() const { return TargetMoveCount; }
	/** B-400 (wor.Camera): e.g. "target view: Goblin_2 of 3 (cut, 4 moves)" or "target view: none". */
	FString GetTargetViewString() const;

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

	/** B-399: the command menu's depth changed; only the viewed character's menu moves the camera. */
	UFUNCTION()
	void HandleMenuDepthChanged(AActor *Actor, int32 NewDepth);

	/** B-399: blends the viewed character's turn camera to the turn view for MenuDepth on the shot timer (StepShot). */
	void StepTurnViewForMenu();

	/** B-400: the highlighted target changed; only the viewed character's menu moves the camera. */
	UFUNCTION()
	void HandleTargetHighlighted(AActor *Actor, AActor *Target, int32 TargetCount);

	/** B-399 / B-400: moves the viewed character's turn camera to End: a cut when Seconds is 0, else a blend on the
	 *  shot timer (StepShot), from where the camera is. False when there's no camera to move. */
	bool MoveTurnCamera(const FTransform &End, float Seconds);

	/** B-400: the turn view at MenuDepth, by a cut, when a target view shows (confirm: the action's shots start from
	 *  the turn view, as before B-400). */
	void CutBackFromTargetView();

	/** B-400: the target's feet and height from its collision bounds (a default height without any). */
	static void GetTargetShape(const AActor *Target, FVector &OutFeet, float &OutHeight);

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

	// B-399: the command menu's depth (calm menus).
	TWeakObjectPtr<UCombatCommandMenuSubsystem> BoundMenu;
	int32 MenuDepth = 0;
	int32 MenuStepCount = 0;

	// B-400: the target picker's highlight (target cut).
	TWeakObjectPtr<AActor> HighlightedTarget;
	int32 HighlightTargetCount = 0;
	bool bTargetViewShowing = false;
	int32 TargetMoveCount = 0;
};
