// WoRGameplayEffects.h
// GAS stage 3 (B-042): native GameplayEffect classes, one per effect family
// (docs/analysis/EffectsOverhaul.md §10.2). Every lasting GE is Infinite with no Period: the turn
// tracker (UWoRTurnEffectTracker) counts the holder's turns and removes it by handle, because GAS
// counts seconds. GE components are created in the constructor and added to GEComponents, the
// supported native pattern (UGameplayEffect::PostInitProperties checks it).
// Debug: `showdebug abilitysystem`, UWoRTurnEffectTracker::GetDebugString.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "WoRGameplayEffects.generated.h"

/** Stun (D-001): grants Status.Stun to the holder while it lasts. The first native GE (step 1 spike);
 *  nothing applies it in play yet, the facade routes Stun here in a later B-042 step. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_Stun : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_Stun();
};
