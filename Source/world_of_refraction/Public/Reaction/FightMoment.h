// FightMoment.h
// B-212 (Q-191 changed by Crown, Q-212): when a reaction or boss trigger fires. Fight start is the
// default (checked before the fight, as before); every other moment waits in UFightTriggers and
// fires the first time it happens, once per fight.

#pragma once

#include "CoreMinimal.h"
#include "FightMoment.generated.h"

/** Append new values at the end (saved in assets). */
UENUM(BlueprintType)
enum class EFightMoment : uint8
{
	FightStart,
	/** The side's turn starts. */
	TurnStart,
	/** The side deals damage. */
	Hit,
	/** The side takes damage. */
	DamageTaken,
	/** The side's HP crosses below ThresholdPercent (not already below at the start). */
	HPBelow,
	/** The side's HP crosses back above ThresholdPercent. */
	HPAbove,
	/** The boss (for a reaction: the faced character) reaches a phase. */
	BossPhase
};

/** Whose moment: for a reaction, Ours = the wearer and Theirs = the faced character; for a boss
 *  trigger, Ours = any party member and Theirs = the boss. */
UENUM(BlueprintType)
enum class EFightMomentSide : uint8
{
	Ours,
	Theirs
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FFightMoment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "When")
	EFightMoment Moment = EFightMoment::FightStart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "When")
	EFightMomentSide Side = EFightMomentSide::Ours;

	/** HPBelow / HPAbove: percent of max HP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "When", meta = (ClampMin = "0", ClampMax = "100"))
	float ThresholdPercent = 50.0f;

	bool IsAtFightStart() const { return Moment == EFightMoment::FightStart; }

	/** Debug: the moment in words. */
	FString ToString() const
	{
		const FString Name = UEnum::GetDisplayValueAsText(Moment).ToString();
		if (IsAtFightStart())
		{
			return Name;
		}
		const TCHAR *Whose = Side == EFightMomentSide::Ours ? TEXT("ours") : TEXT("theirs");
		const bool bThreshold = Moment == EFightMoment::HPBelow || Moment == EFightMoment::HPAbove;
		return bThreshold ? FString::Printf(TEXT("%s %.0f%% (%s)"), *Name, ThresholdPercent, Whose)
						  : FString::Printf(TEXT("%s (%s)"), *Name, Whose);
	}
};
