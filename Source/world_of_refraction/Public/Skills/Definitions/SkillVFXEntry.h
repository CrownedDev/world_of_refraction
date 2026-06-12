// SkillVFXEntry.h
// Role-classified, index-ordered VFX entry for the skill VFX array (D5).
// A UCombatNotify (Family=VFX, Index=N) selects entry N; Role drives
// code-spawned visuals (e.g. projectile impact fires Impact-role entries).

#pragma once

#include "CoreMinimal.h"
#include "SkillVFXEntry.generated.h"

class UNiagaraSystem;

/** Where a VFX/SFX entry spawns/attaches. Only Target requires a target.
 *  Hoisted from the spike (CombatOrchestrator.h) — production-owned here;
 *  the spike structs include this header. */
UENUM(BlueprintType)
enum class EVFXAttach : uint8
{
    Caster       UMETA(DisplayName = "Caster"),
    Target       UMETA(DisplayName = "Target"),
    ImpactPoint  UMETA(DisplayName = "Impact Point"),
    CasterSocket UMETA(DisplayName = "Caster Socket"),
    World        UMETA(DisplayName = "World Location")
};

/** What kind of visual an entry is (D5). Role CLASSIFIES; index DISTINGUISHES —
 *  notifies pick entries by array position, code-spawned visuals look up by
 *  role. Trail is NOT a role — it lives on the Cast entry / projectile
 *  (Stage 11), since it must follow the moving projectile. */
UENUM(BlueprintType)
enum class EVFXRole : uint8
{
    /** Launch flash at the caster/socket. */
    Muzzle,
    /** Hit/landing burst on the target — doubles as the melee hit visual. */
    Impact,
    /** Auras, charge glows, ambient visuals; no mechanical tie. */
    Cosmetic
};

/**
 * One entry in a skill's VFX array. Index-ordered: array position IS identity
 * for VFX notifies (UCombatNotify index-by-position). Consumed by the
 * fused-montage runner (Stage 12); until then nothing reads it.
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSkillVFXEntry
{
    GENERATED_BODY()

    /** Designer note ("blue burst on finisher") — never read by code. */
    UPROPERTY(EditAnywhere, Category = "VFX")
    FString Label;

    UPROPERTY(EditAnywhere, Category = "VFX")
    EVFXRole Role = EVFXRole::Cosmetic;

    /** Soft ref — the runner resolves at spawn time (Stage 12). */
    UPROPERTY(EditAnywhere, Category = "VFX")
    TSoftObjectPtr<UNiagaraSystem> VFX;

    UPROPERTY(EditAnywhere, Category = "VFX")
    EVFXAttach Attach = EVFXAttach::Caster;

    UPROPERTY(EditAnywhere, Category = "VFX", meta = (EditCondition = "Attach==EVFXAttach::CasterSocket"))
    FName SocketName;

    /** Tint via ElementColors/HybridSpellColors, or false = use as-authored. */
    UPROPERTY(EditAnywhere, Category = "VFX")
    bool bElementTinted = true;

    /** Visual scale (D6) — cosmetic only, never the hitbox. */
    UPROPERTY(EditAnywhere, Category = "VFX", meta = (ClampMin = "0.1"))
    float Scale = 1.0f;
};
