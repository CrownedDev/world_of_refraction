// CombatCameraTypes.h
// Combat camera rules (Design/CombatCamera.md, D-074): which mode the viewer sees, which
// template sequence a skill falls back to, where a mode puts the camera, and (B-371) the code-built shot patterns. Pure functions so
// the rules are testable without a world.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/EActionType.h"
#include "CombatCameraTypes.generated.h"

class USkillDataBase;
class UAnimMontage;

UENUM(BlueprintType)
enum class ECombatCameraMode : uint8
{
	None UMETA(DisplayName = "None"),
	/** One of the viewer's characters is choosing an action. */
	Turn UMETA(DisplayName = "Turn"),
	/** A skill's Level Sequence is playing. */
	Action UMETA(DisplayName = "Action"),
	/** Someone else's turn: pan out wide. */
	NotYourTurn UMETA(DisplayName = "Not Your Turn"),
	/** Camera movement off: one fixed wide view. */
	Static UMETA(DisplayName = "Static"),
	/** B-370 (D-162), B-402: your character is targeted: a low side-on shot of it and the attacker (several: a wide
	 *  side-on shot of them all). */
	Defence UMETA(DisplayName = "Defence"),
	/** B-405 (R14): the last enemy fell; the kill shot holds before the fight ends. */
	KillShot UMETA(DisplayName = "Kill shot")
};

/** Turn-view setting (D-074 (2)): centred-wide is the default. B-398: a player setting, saved (FSavedSettings);
 *  its player-facing names are FCombatCameraRules::GetTurnViewStyleName. */
UENUM(BlueprintType)
enum class ETurnViewStyle : uint8
{
	CentredWide UMETA(DisplayName = "Centred, wide"),
	OverTheShoulder UMETA(DisplayName = "Over the shoulder"),
	Tactical UMETA(DisplayName = "Tactical side-on")
};

/** Template sequence a skill without its own sequence uses (D-074 (1)). */
UENUM(BlueprintType)
enum class ECameraTemplateKind : uint8
{
	Melee UMETA(DisplayName = "Melee"),
	Ranged UMETA(DisplayName = "Ranged"),
	Spell UMETA(DisplayName = "Spell")
};

/** Whether skill cinematics play (D-078 setting 1). */
UENUM(BlueprintType)
enum class ESkillCinematics : uint8
{
	/** Off when camera movement is off, on otherwise (the default). */
	FollowCameraMovement,
	On,
	Off
};

/** B-371 (Q-371): a code-built shot's framing. Default reads the skill: a spell or ranged skill holds wide, a
 *  melee skill stays close. */
UENUM(BlueprintType)
enum class ECameraShotStyle : uint8
{
	Default UMETA(DisplayName = "Default (by skill type)"),
	Close UMETA(DisplayName = "Close"),
	Wide UMETA(DisplayName = "Wide")
};

/** B-371: which side of the caster the wind-up camera stands on. */
UENUM(BlueprintType)
enum class ECameraShotSide : uint8
{
	Right UMETA(DisplayName = "Right shoulder"),
	Left UMETA(DisplayName = "Left shoulder")
};

/** B-371 (SkillPresentationPipeline.md §5): the moments a code-built shot follows. */
UENUM(BlueprintType)
enum class ECameraShotMoment : uint8
{
	/** Over the caster's shoulder, slow push in. */
	WindUp,
	/** Hold on the caster, or follow the projectile. */
	Cast,
	/** Frame the target; a small shake and FOV punch (larger on a crit). */
	Impact,
	/** Several targets: pull out until all are in frame. */
	Spread,
	/** B-406 (study R9): an enemy's named attack opens on a front medium shot of the enemy, pushing in. */
	NamedAttack
};

/** B-406 (study R17, R18; FeelGuide 9): a per-boss cinematic, opt-in in the boss's data. */
UENUM(BlueprintType)
enum class EBossCinematicKind : uint8
{
	None,
	/** Before the first turn; turn 1 waits for it. */
	Opening,
	/** When a boss phase is reached; the next action waits for it. */
	Phase
};

/** B-371 (Q-371): per-skill tuning of the code-built shots, without Sequencer. A skill with its own CameraSequence
 *  plays that instead (signature moves). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSkillCameraTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	ECameraShotStyle Style = ECameraShotStyle::Default;

	/** Scales every shot's distance (1 = the pattern's own). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float DistanceScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	ECameraShotSide Side = ECameraShotSide::Right;

	/** Scales the impact shake and FOV punch (0 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float ShakeScale = 1.0f;

	/** The Cast shot follows the projectile (off: it holds on the caster). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bFollowProjectile = true;

	/** B-406 (study R9, Crown's Q3): a named attack. Used by an enemy, its wind-up is a front shot of the enemy (the
	 *  threat announced) instead of the over-the-shoulder push-in. No effect on a party member's skill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bNamedAttack = false;
};

/** B-371: what a code-built shot needs to know about the action. */
struct WORLD_OF_REFRACTION_API FCameraShotContext
{
	FVector Caster = FVector::ZeroVector;
	TArray<FVector> Targets;
	/** Where the projectile is now (Cast with bFollowProjectile); unset: hold on the caster. */
	TOptional<FVector> Projectile;
	/** A spell or ranged skill (the Default style frames it wide). */
	bool bRanged = false;
	bool bCritical = false;
	/** B-403 (study R5, R15): a melee wind-up may rush in. Off while camera movement is off: the slow push instead. */
	bool bAllowRush = true;
};

/** B-371: one code-built shot: the camera moves from Start to End over BlendSeconds (a push-in when they differ). */
struct WORLD_OF_REFRACTION_API FCameraShot
{
	FTransform Start = FTransform::Identity;
	FTransform End = FTransform::Identity;
	float BlendSeconds = 0.0f;
	float FieldOfView = 90.0f;
	/** The shake to play with it (0 = none). */
	float ShakeScale = 0.0f;
	/** B-403: the camera's motion-blur amount while the shot moves (0 = the project's own blur). */
	float MotionBlur = 0.0f;
};

/** Where the fight is, from the camera's point of view. */
struct WORLD_OF_REFRACTION_API FCombatCameraFrame
{
	/** The character the camera belongs to. */
	FVector Focus = FVector::ZeroVector;
	/** Unit vector from the viewer's side toward the other side. */
	FVector TowardEnemies = FVector::ForwardVector;
	/** Centre of the other side. */
	FVector EnemyCentre = FVector::ZeroVector;
	/** Centre of everyone in the fight. */
	FVector ArenaCentre = FVector::ZeroVector;
};

/** B-370: a skill's cinematic-window moments, in wall-clock seconds from the skill's start (the chain
 *  RitualCastMontage → SkillMontage → ReturnMontage at BaseAnimSpeed). */
struct WORLD_OF_REFRACTION_API FCinematicWindowTiming
{
	/** A Cinematic Window notify state is placed (the first one found counts). */
	bool bHasAuthoredWindow = false;
	float AuthoredStart = 0.0f;
	float AuthoredEnd = 0.0f;
	/** The wind-up's end: the end of the RitualCastMontage leg, when it plays (Q-370b). */
	bool bHasWindUpEnd = false;
	float WindUpEnd = 0.0f;
	/** The first Impact or Cast notify, minus its telegraph lead. */
	bool bHasDefendable = false;
	float FirstDefendable = 0.0f;
};

/** B-404 (study R12, R16, Q-404): how the camera reaches a new turn's view. With a wide beat: into WideView over
 *  BeatBlendSeconds, held until BeatSeconds, then the handover blend to TurnView over HandoverBlendSeconds. Without
 *  one: straight to TurnView, as before. */
struct WORLD_OF_REFRACTION_API FTurnChangePlan
{
	bool bWideBeat = false;
	FTransform WideView = FTransform::Identity;
	FTransform TurnView = FTransform::Identity;
	float BeatBlendSeconds = 0.0f;
	/** From the turn start to the handover blend's start (the blend into the wide plus its hold). */
	float BeatSeconds = 0.0f;
	float HandoverBlendSeconds = 0.0f;
};

/** B-401 (study R4, Q-401): how the camera comes back after one action of a turn that goes on (B-170c action pips).
 *  bReturn: one short blend from where the last shot ended to TurnView, the same turn view the turn started on; no
 *  wide beat. Without it the camera stays as it is (an enemy's watch view, the static view with movement off). */
struct WORLD_OF_REFRACTION_API FSameTurnReturnPlan
{
	bool bReturn = false;
	FTransform TurnView = FTransform::Identity;
	float BlendSeconds = 0.0f;
};

struct WORLD_OF_REFRACTION_API FCombatCameraRules
{
	/** D-162 / Q-370: the least time between the window's end and the first defendable moment.
	 *  TODO: tune in PIE (first guess 0.75 s). */
	static constexpr float MinDefenseReadSeconds = 0.75f;

	/** The montage runner's own play-rate clamp (UActionExecutor::PlayActionMontageOnActor). Shared with
	 *  NotifyValidation (B-368) so both read montage time the same way. */
	static constexpr float MinPlayRate = 0.1f;

	/** The runner plays the return leg unscaled (UActionExecutor::PlayReturnStep). */
	static constexpr float ReturnLegRate = 1.0f;

	/** Mode the viewer sees while ActingActor's turn runs. */
	static ECombatCameraMode SelectMode(bool bActorIsViewers, bool bSequencePlaying, bool bMovementOff);

	/** Whether a skill's cinematic plays (D-078): the cinematics setting, then enemy cinematics. */
	static bool ShouldPlayCinematic(ESkillCinematics Setting, bool bMovementOff, bool bCasterIsEnemy, bool bEnemyCinematicsOn);

	/** Template a skill falls back to: spells use Spell, other skills their melee/ranged execution. */
	static ECameraTemplateKind TemplateKindFor(const USkillDataBase *Skill, EActionType ActionType);

	/** Camera transform for a mode. Action has no transform of its own (the sequence owns the view). Defence uses
	 *  ComputeDefenceView. Turn is ComputeTurnView (MenuDepth is read only there). */
	static FTransform ComputeView(ECombatCameraMode Mode, ETurnViewStyle Style, const FCombatCameraFrame &Frame, int32 MenuDepth = 0);

	/** B-399: the deepest menu step the turn view takes (a sub-menu); deeper menus hold it. */
	static constexpr int32 MaxMenuDepth = 1;

	/** B-398 (study R1): the turn view for each turn-view setting; the one place a turn view is framed.
	 *  CentredWide: behind and above the character, facing the enemy centre (D-074 default).
	 *  OverTheShoulder: the acting character in the lower-left third, back to the camera, the enemy centre in the
	 *  middle of the frame; scaled to the character's distance from the enemy centre, so every party member at any
	 *  range gets the same layout. Tactical: side-on, both teams in view.
	 *  B-399 (study R2): MenuDepth 1 (a sub-menu is open, UCombatCommandMenuSubsystem::OnMenuDepthChanged) steps the
	 *  camera in toward the character and lower, aimed at the same point; 0 is the base view; clamped to
	 *  0..MaxMenuDepth. */
	static FTransform ComputeTurnView(ETurnViewStyle Style, const FCombatCameraFrame &Frame, int32 MenuDepth = 0);

	/** B-400 (study R3 as changed in §9): the fewest valid targets for which the single-target picker moves the
	 *  camera; with one, the camera holds on the turn view. */
	static constexpr int32 MinTargetsForTargetView = 2;

	/** B-400 (study R3 / §9, Q-400): the view while the single-target picker highlights a target whose feet are at
	 *  TargetFeet, TargetHeight tall. Fewer than MinTargetsForTargetView targets: ComputeTurnView(Style, Frame,
	 *  MenuDepth), unchanged. OverTheShoulder: a front medium shot of the target (its whole body and the HP bar above
	 *  it in frame), from the side of the fight it faces. CentredWide / Tactical: the turn view from the same place,
	 *  re-aimed part of the way toward the target. */
	static FTransform ComputeTargetView(ETurnViewStyle Style, const FCombatCameraFrame &Frame, const FVector &TargetFeet, float TargetHeight,
										int32 MenuDepth, int32 TargetCount);
	/** ComputeTargetView from a given turn view (the director passes its turn view with B-405's boss height, so a
	 *  re-aim keeps that height and the way back doesn't jump). */
	static FTransform ComputeTargetViewFrom(ETurnViewStyle Style, const FCombatCameraFrame &Frame, const FTransform &TurnView,
											const FVector &TargetFeet, float TargetHeight, int32 TargetCount);

	/** B-400: the move to, between and back from target views: 0 (a cut) for OverTheShoulder (study §12: one cut per
	 *  press); a very short re-aim blend for the other styles (§9: a setting, not a rule). */
	static float GetTargetViewBlendSeconds(ETurnViewStyle Style);

	/** B-400: the point above a target's head that its HP bar needs in frame. */
	static FVector GetTargetHealthBarPoint(const FVector &TargetFeet, float TargetHeight);

	/** B-398 (D-061 / D-062): the setting's player-facing name, and each style's. */
	static FText GetTurnViewSettingName();
	static FText GetTurnViewStyleName(ETurnViewStyle Style);

	/** B-370: the cinematic window's moments read from a skill's montages, as the runner plays them: the ritual and
	 *  skill legs at PlayRate (BaseAnimSpeed × the caster's speed stat), the return leg at 1.0; a deferred fire
	 *  skips the ritual leg (it played on the arm turn). Empty for a skill with no SkillMontage. */
	static FCinematicWindowTiming ReadWindowTiming(const USkillDataBase *Skill, float PlayRate, bool bSkipRitualLeg);

	/** B-370 (D-162, Q-370): when the window ends. The placed range's end, else the wind-up end, else the first
	 *  defendable moment; then cut short so it ends MinDefenseReadSeconds before the first defendable moment, never
	 *  below 0. False when there is neither a placed range nor anything to defend (no hand-off). */
	static bool ResolveWindowEnd(const FCinematicWindowTiming &Timing, float &OutWindowEnd);

	/** B-370: the view a viewer gets at the window's end. Camera movement off: Static (D-078). A viewer whose own
	 *  character is targeted: Defence. Anyone else keeps a playing cinematic (Action) or watches (NotYourTurn). */
	static ECombatCameraMode SelectHandOffMode(bool bViewerDefends, bool bSequencePlaying, bool bMovementOff);

	/** B-402 (study R10, R10b; Q2, Q-402): a low side-on shot of the defenders and the attacker, from the attacker's
	 *  weapon side, swung a little behind the defenders so they sit nearer. One defender: a two-shot. Several: a wider
	 *  shot. Either way the camera stands back far enough that every defender and the attacker project inside the
	 *  frame at DefaultFieldOfView. */
	static FTransform ComputeDefenceView(const TArray<FVector> &Defenders, const FVector &Attacker);

	/** B-402 (R10, R11; Q-402): a shot shown while the viewer defends keeps still: no shake, no FOV punch and no melee
	 *  rush blur (B-403). A held punch is dropped, not replayed; the next shot after the combo plays its own as usual. */
	static FCameraShot SteadyForDefence(const FCameraShot &Shot, bool bViewerDefending);

	/** The field of view every view but an impact punch uses. */
	static constexpr float DefaultFieldOfView = 90.0f;

	/** B-371 (Q-371): a skill plays the code-built shots unless it has its own CameraSequence (a signature move). */
	static bool UsesShotPatterns(const USkillDataBase *Skill);

	/** B-371: the moment an impact plays: Spread for several targets, else Impact. */
	static ECameraShotMoment ImpactMomentFor(int32 TargetCount);

	/** B-371 (SkillPresentationPipeline.md §5): the code-built shot for Moment. Wind-up: over the caster's shoulder
	 *  (Tuning.Side), pushing in: slowly for a spell or ranged skill, a short fast rush with motion blur for a melee
	 *  one (B-403, R5). Cast: holds on the caster, or looks at the projectile. Impact (B-403, R6): a low side-on /
	 *  three-quarter-front two-shot of the caster and the first target, a small FOV punch and a light shake, larger
	 *  on a crit. Spread (B-403, R8): pulls back until every target is in frame and holds still (no shake, no punch).
	 *  Distances scale by Tuning.DistanceScale, Close / Wide by Tuning.Style; the shake by Tuning.ShakeScale. */
	static FCameraShot ComputeShot(ECameraShotMoment Moment, const FCameraShotContext &Context, const FSkillCameraTuning &Tuning);

	/** B-371: the camera at Alpha (0 to 1, clamped) along Shot, eased in and out. */
	static FTransform BlendShot(const FCameraShot &Shot, float Alpha);

	/** B-404 (study R12, R16, Q-404): the camera's way into a new turn. A wide beat (the NotYourTurn wide, framed
	 *  from the new actor's side) plays only between turns: someone acted since the last turn start, the turn passes
	 *  to another actor, and the new view is a Turn view. Never between actions of one actor (B-401), never with
	 *  movement off (Static), and not before an enemy's turn (its watch view is already the wide). */
	static FTurnChangePlan PlanTurnChange(bool bSomeoneActed, bool bSameActor, ECombatCameraMode NextMode, ETurnViewStyle Style,
										  const FCombatCameraFrame &Frame);
	/** B-406: the first shot of a skill's pattern: an enemy's named attack opens on the NamedAttack front shot, every
	 *  other skill on the over-the-shoulder WindUp. */
	static ECameraShotMoment OpeningMomentFor(const FSkillCameraTuning &Tuning, bool bCasterIsEnemy);

	/** B-406: the longest a boss cinematic may hold the fight; a broken or over-long sequence is cut here.
	 *  TODO: tune in PIE (the study's phase change runs about 15 s). */
	static constexpr float BossCinematicMaxSeconds = 20.0f;

	/** B-406 (FeelGuide 9, on trial): whether a boss cinematic plays. Only with a sequence set and a local viewer to
	 *  watch it (a headless fight starts at once), and only while skill cinematics are on (D-078: camera movement off
	 *  skips them unless that setting is On). */
	static bool ShouldPlayBossCinematic(bool bHasSequence, bool bHasLocalViewer, ESkillCinematics Setting, bool bMovementOff);

	/** B-401 (study R4, Q-401): the camera's way back when the actor whose turn it is gets asked again after an action
	 *  (bActedSinceAsked). A return only for a Turn view: TurnView is PlanTurnChange's for the same actor, so every
	 *  action of a turn starts from the same transform. Never a wide beat; only a turn change does the full move. */
	static FSameTurnReturnPlan PlanSameTurnReturn(bool bActedSinceAsked, ECombatCameraMode Mode, ETurnViewStyle Style,
												  const FCombatCameraFrame &Frame);
};
