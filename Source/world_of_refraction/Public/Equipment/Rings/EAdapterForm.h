// EAdapterForm.h
// D-107: the spell-channelling gear is an adapter; a ring is one form of it. The form sets only the look and the cast
// animation (URingData mesh / montage); the mechanics are the same for every form. Append-only (saved in assets).

#pragma once

#include "CoreMinimal.h"
#include "EAdapterForm.generated.h"

UENUM(BlueprintType)
enum class EAdapterForm : uint8
{
	Ring UMETA(DisplayName = "Ring"),
	Pistol UMETA(DisplayName = "Pistol"),
	Trainer UMETA(DisplayName = "Trainer")
};
