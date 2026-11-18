// TrialKitSight.h
// B-572 (D-225, TrialPoolDesign §7, stage 8): seeing an enemy's kit. Not free: it comes as two variations of one skill
// (USkillDataBase::KitSight).
//   - Field: used on an enemy on the trial map before the fight. Costs EP. Shows that encounter's first enemy in full
//     and remembers its type for the run: every enemy of that type then shows its shared part (its always-has pieces).
//   - AlwaysOn: built as the rule only (who has it, the range); how it shows as the player gets close waits on Crown
//     (Q-572).
// The kit shown is the run's roll for that encounter (EnemyKitRoll), the one the enemy fights with.
// Placeholder: wor.Trial.InspectEnemy uses the field variation on the nearest enemy (no key bound yet, Q-572).
// Debug: wor.Trial.PrintKitSight; wor.Trial.KitSight forces a variation while no skill asset carries one.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Trial/TrialPoolLook.h"

class AActor;
class APlayerController;
class UCharacterData;
class UPoolSubsystem;
class UWorld;
struct FRolledEnemyKit;

namespace TrialKitSight
{
	// TODO: tune (Q-572).
	constexpr int32 FIELD_EP_COST = 10;
	/** How near the player's character must be to use the field variation on an enemy (cm). */
	constexpr float FIELD_RANGE = 1500.0f;
	/** How near enemies show their gear with the always-on variation (cm). */
	constexpr float ALWAYS_ON_RANGE = 800.0f;

	/** The stronger of two variations: AlwaysOn over Field over None. */
	WORLD_OF_REFRACTION_API EKitSight Strongest(EKitSight A, EKitSight B);

	/** The variation CharacterKey has: the strongest KitSight among the skills it knows. */
	WORLD_OF_REFRACTION_API EKitSight GetKnownSight(const UPoolSubsystem &Pool, FName CharacterKey);

	/** The card for one enemy from its rolled kit. bFull: every piece and skill; else the type's shared part only (its
	 *  always-has pieces, no skills). No rank word (D-187). */
	WORLD_OF_REFRACTION_API FEnemyKitCard MakeCard(const UCharacterData &Enemy, const FRolledEnemyKit &Kit, bool bFull);

	/** Why the field variation can't be used now, for the player; empty when it can. */
	WORLD_OF_REFRACTION_API FText GetFieldRefusal(EKitSight Sight, int32 CurrentEP, float Distance);

	/** Uses the field variation from PC's character on the nearest encounter in range: pays the EP, marks the enemy's
	 *  type seen for the run, opens the card. Empty on success; else the reason, for the player. */
	WORLD_OF_REFRACTION_API FText UseOnNearestEnemy(APlayerController *PC);

	WORLD_OF_REFRACTION_API FString GetKitSightString(const UWorld *World);
}
