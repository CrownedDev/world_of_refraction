// CombatCameraTypes.h
// Combat camera rules (Design/CombatCamera.md, D-074): which mode the viewer sees, which
// template sequence a skill falls back to, and where a mode puts the camera. Pure functions so
// the rules are testable without a world.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/EActionType.h"
#include "CombatCameraTypes.generated.h"

class USkillDataBase;

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
	Static UMETA(DisplayName = "Static")
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

struct WORLD_OF_REFRACTION_API FCombatCameraRules
{
	/** Mode the viewer sees while ActingActor's turn runs. */
	static ECombatCameraMode SelectMode(bool bActorIsViewers, bool bSequencePlaying, bool bMovementOff);

	/** Template a skill falls back to: spells use Spell, other skills their melee/ranged execution. */
	static ECameraTemplateKind TemplateKindFor(const USkillDataBase *Skill, EActionType ActionType);

	/** Camera transform for a mode. Action has no transform of its own (the sequence owns the view). */
	static FTransform ComputeView(ECombatCameraMode Mode, ETurnViewStyle Style, const FCombatCameraFrame &Frame);
};
