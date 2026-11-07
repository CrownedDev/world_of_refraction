// CombatCameraTypes.h
// Combat camera rules (Design/CombatCamera.md, D-074): which mode the viewer sees, which
// template sequence a skill falls back to, and where a mode puts the camera. Pure functions so
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
	/** The wind-up's end: the end of the RitualCastMontage leg, when the skill has one (Q-370b). */
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

	/** Mode the viewer sees while ActingActor's turn runs. */
	static ECombatCameraMode SelectMode(bool bActorIsViewers, bool bSequencePlaying, bool bMovementOff);

	/** Whether a skill's cinematic plays (D-078): the cinematics setting, then enemy cinematics. */
	static bool ShouldPlayCinematic(ESkillCinematics Setting, bool bMovementOff, bool bCasterIsEnemy, bool bEnemyCinematicsOn);

	/** Template a skill falls back to: spells use Spell, other skills their melee/ranged execution. */
	static ECameraTemplateKind TemplateKindFor(const USkillDataBase *Skill, EActionType ActionType);

	/** Camera transform for a mode. Action has no transform of its own (the sequence owns the view). Defence uses
	 *  ComputeDefenceView. */
	static FTransform ComputeView(ECombatCameraMode Mode, ETurnViewStyle Style, const FCombatCameraFrame &Frame);

	/** B-370: the cinematic window's moments read from a skill's montages. Empty for a skill with no SkillMontage. */
	static FCinematicWindowTiming ReadWindowTiming(const USkillDataBase *Skill);

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
};
