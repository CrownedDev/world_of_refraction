// ScalingDebug.h
// Baseline-capture / regression harness for the unified-scaling-tiers arc.
// Snapshot a skill's FinalDamage through the REAL DamageCalculator path BEFORE
// stage b2 (the tier-scaling formula change), then re-run AFTER b2 with an EMPTY
// StatScaling array — the numbers must match (the empty-array no-op guard).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ScalingDebug.generated.h"

class AActor;
class USkillDataBase;

/**
 * Debug utilities for the stat-scaling-tier system. CompareScalingSnapshot runs the
 * actual UDamageCalculator::CalculateDamage for a skill (Attacker vs Defender) and logs
 * the attacker-side multiplier + FinalDamage — the exact chain stage b2 modifies. Mirrors
 * the UDamageSplitDebug pattern (BlueprintFunctionLibrary of static logging helpers).
 */
UCLASS()
class WORLD_OF_REFRACTION_API UScalingDebug : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Run UDamageCalculator::CalculateDamage for Skill as (Attacker vs Defender) with
     * defense / resistance / crit neutralized, so the printed FinalDamage reflects ONLY the
     * attacker-side scaling chain (Step 1 — where b2 inserts the tier term). Logs the skill
     * name, resolved ActionType, base damage in, AttackerDamageMultiplier, and FinalDamage.
     *
     * Baseline-capture for the unified-scaling-tiers arc: snapshot the number BEFORE stage b2,
     * then re-run AFTER b2 with an EMPTY StatScaling array — it MUST match (empty-array no-op).
     * Goes through the SAME formula b2 edits, so a mismatch is a real regression. Read-only.
     */
    UFUNCTION(BlueprintCallable, Category = "Debug|Scaling")
    static void CompareScalingSnapshot(AActor *Attacker, AActor *Defender, const USkillDataBase *Skill);

    /** One-line FString form of the same snapshot (skill / base / mult / final) for inline inspection. */
    UFUNCTION(BlueprintCallable, Category = "Debug|Scaling")
    static FString GetScalingSnapshotString(AActor *Attacker, AActor *Defender, const USkillDataBase *Skill);
};
