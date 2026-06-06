// EAttachedItemKind.h
// Discriminator enum for FAttachedItem (design-time) and FRuntimeAttachedItem
// (runtime). Distinguishes empty slots from refined-crystal, evolution-item, and
// whetstone attachments. Mutually exclusive — a slot is exactly one Kind.

#pragma once

#include "CoreMinimal.h"
#include "EAttachedItemKind.generated.h"

UENUM(BlueprintType)
enum class EAttachedItemKind : uint8
{
    None      UMETA(DisplayName = "None"),
    Refined   UMETA(DisplayName = "Refined"),
    Evolution UMETA(DisplayName = "Evolution"),
    // Appended after Evolution (=3); position is the serialized value, so do not
    // reorder/insert above — SaveGame Kind on disk must stay stable.
    Whetstone UMETA(DisplayName = "Whetstone")
};
