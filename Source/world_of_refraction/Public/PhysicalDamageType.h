// PhysicalDamageType.h
// Physical damage types for weapon attacks (Generic characters)

#pragma once

#include "CoreMinimal.h"
#include "PhysicalDamageType.generated.h"

/**
 * Physical damage types for weapon-based attacks
 * Each type builds up toward a specific status effect:
 * - Slash → Bleed (DOT)
 * - Pierce → Armor Break (Defense debuff)
 * - Blunt → Stun (Skip turn)
 */
UENUM(BlueprintType)
enum class EPhysicalDamageType : uint8
{
	Slash	UMETA(DisplayName = "Slash (Builds Bleed)"),
	Pierce	UMETA(DisplayName = "Pierce (Builds Armor Break)"),
	Blunt	UMETA(DisplayName = "Blunt (Builds Stun)")
};
