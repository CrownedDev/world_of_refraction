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
	/** B-370 (D-162): your character is targeted: behind it, the attacker in front (several: frame them all). */
	Defence UMETA(DisplayName = "Defence")
};

/** Turn-view setting (D-074 (2)): centred-wide is the default. */
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
	Spread
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
	 *  ComputeDefenceView. */
	static FTransform ComputeView(ECombatCameraMode Mode, ETurnViewStyle Style, const FCombatCameraFrame &Frame);

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

	/** B-370: behind and beside the defenders' centre, facing the attacker; pulled back further the wider the
	 *  defenders stand, so several targeted characters are all in frame (Q-370 (3)). */
	static FTransform ComputeDefenceView(const TArray<FVector> &Defenders, const FVector &Attacker);

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
};
