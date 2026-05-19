// EWeaponWieldMode.h
// Defines how a weapon is held — drives mesh-spawn behaviour and ability gating

#pragma once

#include "CoreMinimal.h"
#include "EWeaponWieldMode.generated.h"

/**
 * How a weapon is wielded.
 *
 * Single        — one mesh, right hand only. Off-hand free.
 *                 Used by both one-handed weapons (Sword) and two-handed
 *                 weapons (Greatsword) — two-handedness is handled by
 *                 the animation system, not by this enum.
 *
 * Dual          — two meshes, both hands, same asset (left side reuses
 *                 right-hand mesh at the left socket).
 *
 * OffHandShield — two meshes, asymmetric. Right hand uses the primary
 *                 mesh; left hand uses LeftHandStaticMesh/Skeletal.
 */
UENUM(BlueprintType)
enum class EWeaponWieldMode : uint8
{
    Single        UMETA(DisplayName = "Single"),
    Dual          UMETA(DisplayName = "Dual"),
    OffHandShield UMETA(DisplayName = "Off-Hand Shield")
};
