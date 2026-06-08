// EAttachedItemKind.h
// Discriminator enum for FAttachedItem (design-time) and FRuntimeAttachedItem
// (runtime). Distinguishes empty slots from refined-crystal, evolution-item, and
// augment-stone attachments. Mutually exclusive — a slot is exactly one Kind.

#pragma once

#include "CoreMinimal.h"
#include "EAttachedItemKind.generated.h"

UENUM(BlueprintType)
enum class EAttachedItemKind : uint8
{
    None      UMETA(DisplayName = "None"),
    Crystal   UMETA(DisplayName = "Crystal"),
    Evolution UMETA(DisplayName = "Evolution"),
    // Appended after Evolution (=3); position is the serialized value, so do not
    // reorder/insert above — SaveGame Kind on disk must stay stable.
    AugmentStone UMETA(DisplayName = "Augment Stone"),
    // Appended after AugmentStone (=4); same serialized-position rule. A Fusion slot
    // exposes FFusionId (two FCrystalId halves + a bonus stat) instead of a single
    // crystal. New appended value — no EnumRedirect needed.
    Fusion UMETA(DisplayName = "Fusion")
};
