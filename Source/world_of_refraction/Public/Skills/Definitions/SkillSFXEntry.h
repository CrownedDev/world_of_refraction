// SkillSFXEntry.h
// B-618: one sound on a skill's SFXArray (USkillDataBase). A Notify entry is played by a UCombatNotify
// (Family=SFX, Index=N) on the montage: N is the entry's place in the array (a release, a swing). An Impact entry
// plays where the skill lands: each melee hit, or the projectile's arrival (SkillSounds, ImpactVisuals::SpawnForImpact).

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/SkillVFXEntry.h"
#include "SkillSFXEntry.generated.h"

class USoundBase;

UENUM(BlueprintType)
enum class ESFXRole : uint8
{
	/** Played by an SFX notify whose Index is this entry's place in the array. */
	Notify UMETA(DisplayName = "Notify"),
	/** Played at the target on each landed hit (Hit or Crit), a projectile's arrival too; not on a Guard or Dodge. */
	Impact UMETA(DisplayName = "Impact")
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSkillSFXEntry
{
	GENERATED_BODY()

	/** Designer label shown in the array ("Release", "Whoosh", "Hit"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFX")
	FString Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFX")
	ESFXRole Role = ESFXRole::Notify;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFX")
	TSoftObjectPtr<USoundBase> Sound;

	/** Where a Notify entry plays (an Impact entry always plays at the target's HitPoint). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFX", meta = (EditCondition = "Role == ESFXRole::Notify"))
	EVFXAttach Attach = EVFXAttach::Caster;

	/** CasterSocket only: follows this socket of the caster's mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFX", meta = (EditCondition = "Attach == EVFXAttach::CasterSocket"))
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFX", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFX", meta = (ClampMin = "0.1"))
	float PitchMultiplier = 1.0f;
};
