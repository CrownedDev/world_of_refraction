// ApparelBonusLine.h
// B-475 (D-208, Crown 2026-11-14): besides HP, Defence and Amp, each apparel piece rolls one bonus line when it first
// becomes owned (the drop): a sub-stat (never Turn Speed, retired) or +5% to one pillar's composed value (like an
// over-cap point, ER-24c; it adds no pillar points, so it never counts toward a skill's requirements). Two copies of one
// apparel can roll different lines (Genshin's artifact sub-stats, Diablo's affixes). Debug: wor.Apparel.PrintBonusLines.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/ActionStatModifiers.h"
#include "Character/EWorldPillar.h"
#include "ApparelBonusLine.generated.h"

struct FEquipmentStatBonus;
struct FRandomStream;

UENUM(BlueprintType)
enum class EApparelBonusKind : uint8
{
	None,
	SubStat,
	Pillar
};

/** The one rolled line on an apparel piece (saved with the piece, FSavedGearDetail::ApparelBonusLine). */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FApparelBonusLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Apparel")
	EApparelBonusKind Kind = EApparelBonusKind::None;

	/** Kind SubStat: which one. */
	UPROPERTY(BlueprintReadOnly, Category = "Apparel")
	ESubStat SubStat = ESubStat::None;

	/** Kind Pillar: which one. */
	UPROPERTY(BlueprintReadOnly, Category = "Apparel")
	EWorldPillar Pillar = EWorldPillar::Mind;

	/** A sub-stat's points, or the pillar's percent. */
	UPROPERTY(BlueprintReadOnly, Category = "Apparel")
	float Value = 0.0f;

	bool IsSet() const { return Kind != EApparelBonusKind::None; }
	bool operator==(const FApparelBonusLine &Other) const
	{
		return Kind == Other.Kind && SubStat == Other.SubStat && Pillar == Other.Pillar && FMath::IsNearlyEqual(Value, Other.Value);
	}
};

namespace ApparelBonusLine
{
	/** D-208 default (reference-backed): a pillar line is +5% (the over-cap point's value). */
	constexpr float PILLAR_LINE_PERCENT = 5.0f;
	/** D-208: sub-stat lines are small. TODO: tune with the stat pass. */
	constexpr float SUB_STAT_LINE_POINTS = 2.0f;

	/** The sub-stats a line may roll: every live one but Turn Speed (retired, and the stat Honkai players chased). */
	WORLD_OF_REFRACTION_API const TArray<ESubStat> &GetRollableSubStats();

	/** One line, every rollable sub-stat and every pillar equally likely. */
	WORLD_OF_REFRACTION_API FApparelBonusLine Roll(FRandomStream &Stream);

	/** The line for a newly owned piece: seeded by its id, so the same piece always rolls the same line. */
	WORLD_OF_REFRACTION_API FApparelBonusLine RollFor(const FGuid &PersistentID);

	/** Adds the line to Lines (a sub-stat's field, or a pillar's percent field). */
	WORLD_OF_REFRACTION_API void AddTo(const FApparelBonusLine &Line, FEquipmentStatBonus &Lines);

	/** "+2 Luck" or "+5% Mind"; empty for no line. */
	WORLD_OF_REFRACTION_API FText GetText(const FApparelBonusLine &Line);
}
