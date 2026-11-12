// BossFraming.h
// B-405 (study R13, R14; Q-405): boss framing and kill shots, as pure rules testable without a world.
// - Camera height: a boss or mini-boss turn view sits low and looks up; a normal enemy's stays at eye level. An enemy's
//   data can set its own height (a tall normal enemy); otherwise its encounter rank decides.
// - Kill shots on the last enemy's death, before the fight's rewards (FeelGuide 6): Normal = a wide, darkened hold;
//   Boss = a close final-blow shot, then the break-apart beat.
// Kept light (no camera types included) so CharacterData.h can include it for the height field.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Encounter/EEncounterRank.h"
#include "BossFraming.generated.h"

struct FCombatCameraFrame;
enum class ETurnViewStyle : uint8;

/** B-405: how high the turn-view camera sits against an enemy. Append-only: authored on character data. */
UENUM(BlueprintType)
enum class EEnemyCameraHeight : uint8
{
	/** The enemy's encounter rank decides: Boss and Mini-boss Low, Normal and Elite EyeLevel. */
	ByRank UMETA(DisplayName = "By rank"),
	EyeLevel UMETA(DisplayName = "Eye level"),
	/** Low and looking up, so the enemy towers in the frame. */
	Low UMETA(DisplayName = "Low, looking up")
};

/** B-405 (R14): the shot on the last enemy's death. */
UENUM(BlueprintType)
enum class EKillShotKind : uint8
{
	/** Wide and still, a short darkened hold. */
	Normal UMETA(DisplayName = "Normal"),
	/** Close on the final blow, then the enemy breaks apart. */
	Boss UMETA(DisplayName = "Boss")
};

struct WORLD_OF_REFRACTION_API FBossFramingRules
{
	/** TODO: tune in PIE (study §12: a normal kill ~1.5-2.5 s to the victory, a boss kill ~5-7 s). */
	static constexpr float NormalKillShotSeconds = 1.5f;
	static constexpr float BossKillShotSeconds = 5.0f;
	/** When the boss breaks apart, from the start of its kill shot. TODO: tune in PIE. */
	static constexpr float BreakApartDelaySeconds = 2.0f;
	/** The blend into a kill shot (camera movement off: a cut). TODO: tune in PIE. */
	static constexpr float KillShotBlendSeconds = 0.3f;
	/** The Normal kill shot's darkened look: colour saturation (1 = unchanged) and vignette. TODO: tune in PIE. */
	static constexpr float DarkenedSaturation = 0.35f;
	static constexpr float DarkenedVignette = 0.8f;

	/** An authored height wins; ByRank reads the rank (Boss and Mini-boss Low, else EyeLevel). Never returns ByRank. */
	static EEnemyCameraHeight ResolveHeight(EEnemyCameraHeight Authored, EEncounterRank Rank);

	/** The turn view's height for a fight: Low when any living enemy is Low, else EyeLevel. */
	static EEnemyCameraHeight FightHeight(const TArray<EEnemyCameraHeight> &LivingEnemyHeights);

	/** The turn view for Height. EyeLevel returns View unchanged. Low drops the camera to just above the character and
	 *  aims it up at the enemies' upper bodies. The Tactical side-on view is an overview and keeps its height. */
	static FTransform ApplyHeight(const FTransform &View, EEnemyCameraHeight Height, ETurnViewStyle Style, const FCombatCameraFrame &Frame);

	/** Boss and Mini-boss get the Boss kill shot; Normal and Elite the Normal one. */
	static EKillShotKind KillShotFor(EEncounterRank Rank);

	/** How long the kill shot holds before the fight ends and its rewards show. */
	static float KillShotSeconds(EKillShotKind Kind);

	/** Only the Normal kill shot darkens (the boss's close shot keeps its colour for the break-apart). */
	static bool DarkensView(EKillShotKind Kind);

	/** Only the Boss kill shot breaks the enemy apart. */
	static bool BreaksApart(EKillShotKind Kind) { return Kind == EKillShotKind::Boss; }

	/** Normal: wide and high from the viewer's side of the arena, looking at the fallen enemy. Boss: close, low and to
	 *  one side of the enemy, looking up at it. Frame is built from the viewer's side (TowardEnemies). */
	static FTransform ComputeKillShot(EKillShotKind Kind, const FCombatCameraFrame &Frame, const FVector &Victim);
};
