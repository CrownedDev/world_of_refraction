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
#include "Combat/Camera/BossFraming.h"
#include "CombatCameraDirector.generated.h"

class UCombatCameraComponent;
class UActionExecutor;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class APlayerController;
class UCameraShakeBase;
class UNiagaraSystem;
class UCameraComponent;

class UCombatCommandMenuSubsystem;

class UBossPhaseComponent;
struct FAction;

/** B-406: the whole combat HUD hides (true) for a boss cinematic and comes back (false) when it ends. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCinematicHudHidden, bool, bHidden);
/** B-406: a boss cinematic ended (its sequence, the cap, or a stop): the fight may go on. Not sent when the fight ends. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBossCinematicFinished, EBossCinematicKind, Kind);

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
	/** B-400: shows the highlighted target's view again after a turn-view reset (a setting changed with the picker open). */
	void ReapplyHighlightedTarget();
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

	/** B-405 (R14, Q-405): the last enemy fell. Shows the kill shot for Rank on Victim's camera (Normal: wide and
	 *  darkened; Boss: close and low, then BreakApartEffect and the enemy hidden). Returns how long to hold before the
	 *  fight ends; 0 when nothing shows (no viewer, as in headless tests), so the fight ends at once as before. */
	float PlayKillShot(AActor *Victim, EEncounterRank Rank);

	/** B-405 (wor.Camera.KillShot): Kind on the first enemy, for its length, then back to the turn view. The
	 *  break-apart effect plays but nobody is hidden. */
	void PreviewKillShot(EKillShotKind Kind);

	/** B-405 (wor.Camera): the turn view's height and the last kill shot, e.g. "height Low | kill shot Boss on Boss_1". */
	FString GetBossFramingString() const;

	/** B-405 (R14): the effect a boss breaks apart into in its kill shot (D-057: soft). Empty: the boss stays as it fell.
	 *  TODO: a Niagara system (editor step); a per-boss effect on UBossData if bosses need their own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|KillShot")
	TSoftObjectPtr<UNiagaraSystem> BreakApartEffect;

	/** B-407 (wor.Camera.SignatureCap): the cap and the last signature sequence, e.g. "cap 2.00 s | last LS_Nova
	 *  4.00 s by Hero: cut at 2.00 s". */
	FString GetSignatureCapString() const;

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

	/** B-404 (study R16, F4, Q-404): once per turn start, on the frame the camera starts its move to the new turn's
	 *  view (after the wide beat, when there is one). The HUD binds it for the turn-strip step and the active-card
	 *  highlight, so all three move together. From: the actor the camera followed before (null on the first turn).
	 *  The one exception: a fight that ends during the beat (won, lost or fled) drops its handover on purpose, as the
	 *  HUD is going away. A wor.Camera.Handover preview never fires it. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Camera")
	FOnTurnHandover OnTurnHandover;

	/** B-404 (wor.Camera): e.g. "handover: wide beat Hero_1 -> Hero_2, blend 0.50 s in 0.32 s; 4 sent this fight". */
	FString GetHandoverString() const;
	int32 GetHandoverCount() const { return HandoverCount; }
	/** B-404: a wide beat is playing and its handover has not fired yet. */
	bool IsHandoverPending() const { return bHandoverPending; }

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
	/** B-402: a low side-on shot of the viewer's targeted characters and the attacker (several: a wide one). */
	void ShowDefenceView();
	/** B-402 (R10, R11): one of the viewer's characters is defending: the Defence hand-off is pending or showing, or
	 *  one of them has a defense window open. Shots then play without shake or FOV punch. */
	bool IsViewerDefending() const;

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
	/** B-407: a player or team signature sequence reached the cap; cut back as if it had finished. */
	void HandleSignatureCapReached();
	FCombatCameraFrame BuildFrame(AActor *Focus) const;
	/** B-405: the turn view's height from the living enemies' data and ranks (FBossFramingRules::FightHeight). */
	EEnemyCameraHeight GetFightCameraHeight() const;
	/** Trial merge (B-399 + B-400 + B-405): the turn view at the menu depth, with the fight's camera height. */
	FTransform GetTurnViewWithHeight(const FCombatCameraFrame &Frame) const;
	/** B-405: the kill shot's view, look and break-apart timer. False when there is no viewer or camera. */
	bool ShowKillShot(AActor *Victim, EKillShotKind Kind, bool bHideOnBreakApart);
	void BreakApart();
	void EndKillShotPreview();
	static void SetDarkened(UCameraComponent *Camera, bool bDarkened);
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

	// B-407: the signature cap's cut timer and the last sequence it judged (for wor.Camera.SignatureCap).
	FTimerHandle SignatureCapTimer;
	FString LastSignatureName;
	FString LastSignatureCaster;
	float LastSignatureSeconds = 0.0f;
	float LastSignatureCutSeconds = 0.0f;
	bool bLastSignatureEnemy = false;
	bool bLastSignatureCut = false;
	/** B-407: the last capped sequence was stopped before its cap (debug only). */
	bool bLastSignatureStoppedEarly = false;
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
	/** B-402 (wor.Camera): shakes / FOV punches dropped this fight because the viewer was defending. */
	int32 DefencePunchesDropped = 0;

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

	// B-405: the kill shot now showing (or the last one, for wor.Camera).
	TWeakObjectPtr<AActor> KillShotVictim;
	EKillShotKind LastKillShot = EKillShotKind::Normal;
	bool bHasLastKillShot = false;
	bool bKillShotHidesVictim = false;
	FTimerHandle BreakApartTimer;
	FTimerHandle KillShotPreviewTimer;

	// B-399: the command menu's depth (calm menus).
	TWeakObjectPtr<UCombatCommandMenuSubsystem> BoundMenu;
	int32 MenuDepth = 0;
	int32 MenuStepCount = 0;

	// B-400: the target picker's highlight (target cut).
	TWeakObjectPtr<AActor> HighlightedTarget;
	int32 HighlightTargetCount = 0;
	bool bTargetViewShowing = false;
	int32 TargetMoveCount = 0;

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
