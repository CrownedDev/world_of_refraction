// WoRGameplayEffects.h
// GAS stage 3 (B-042): native GameplayEffect classes, one per effect family
// (docs/analysis/EffectsOverhaul.md §10.2). Every lasting GE is Infinite with no Period: the turn
// tracker (UWoRTurnEffectTracker) counts the holder's turns and removes it by handle, because GAS
// counts seconds. GE components are created in the constructor and added to GEComponents, the
// supported native pattern (UGameplayEffect::PostInitProperties checks it). Each class carries its
// Effect.Family.* asset tag (Q-040b). FWoREffectSpecBuilder picks the class for an effect type.
// Nothing applies these in play yet; the facade routes effects here in B-042c.
// Debug: `showdebug abilitysystem`, UWoRTurnEffectTracker::GetDebugString.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "WoRGameplayEffects.generated.h"

/** Base for the native family GEs: the constructor helpers that add tag components. */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UWoRGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

protected:
	/** What this GE is (asset tags). Constructor only, once per class (fixed subobject name). */
	void AddAssetTag(const FGameplayTag &Tag);

	/** What the target owns while this GE is on it (granted tags). Constructor only, once per class. */
	void AddGrantedTag(const FGameplayTag &Tag);
};

/** Stun (D-001): grants Status.Stun while it lasts. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_Stun : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_Stun();
};

/** Silenced: grants Status.Silence while it lasts. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_Silence : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_Silence();
};

/** Heal Block: grants Status.HealBlock while it lasts. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_HealBlock : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_HealBlock();
};

/** Any Grant*Immunity effect. The Immunity.* tag it grants is added per spec by the builder
 *  (WoRTags::EffectTypeToTag), so one class covers every immunity. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_Immunity : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_Immunity();
};

/** A stat buff or debuff. It modifies no attribute yet: the effect type travels in the context
 *  and the value in SetByCaller.Magnitude, and FWoREffectSpecBuilder::SumMagnitude adds them up the way
 *  USkillEffectManager::GetTotalStatModifier does. B-042e feeds the sum into the stat MMCs. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatModifier : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_StatModifier();
};

/** Damage over time, the lasting half: grants Status.DOT. The tracker pays the tick spec
 *  (UWoRGE_HealthTick) at each holder turn start while it lasts (Q-044c). */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_DamageOverTime : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_DamageOverTime();
};

/** Heal over time, the lasting half; the tracker pays its tick spec like a DOT's. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_HealOverTime : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_HealOverTime();
};

/** One DOT payout: Instant, adds SetByCaller.Magnitude to Health (negative for damage). Built once
 *  when the effect lands, with its magnitude fixed then (§4.3). TODO: B-042e - the source's boost,
 *  resistance and BuildupPerTick come with the damage execution. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_HealthTick : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_HealthTick();
};

/** One HOT payout: a HealthTick that doesn't land while the holder has Status.HealBlock, so the
 *  HOT goes dormant and resumes, as the enum path's per-tick HealBlock check does.
 *  TODO: B-042e - HealingReceivedBuff and the int rounding of the enum path's heal. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_HealTick : public UWoRGE_HealthTick
{
	GENERATED_BODY()

public:
	UWoRGE_HealTick();
};
