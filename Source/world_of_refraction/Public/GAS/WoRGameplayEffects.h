// WoRGameplayEffects.h
// GAS stage 3 (B-042): native GameplayEffect classes, one per effect family
// (docs/analysis/EffectsOverhaul.md §10.2). Every lasting GE is Infinite with no Period: the turn
// tracker (UWoRTurnEffectTracker) counts the holder's turns and removes it by handle, because GAS
// counts seconds. GE components are created in the constructor and added to GEComponents, the
// supported native pattern (UGameplayEffect::PostInitProperties checks it). Each class carries its
// Effect.Family.* asset tag (Q-040b). FWoREffectSpecBuilder picks the class for an effect type.
// The B-042c mirror applies them for every stored effect; B-042k gave every effect type a family.
// Debug: `showdebug abilitysystem`, UWoRTurnEffectTracker::GetDebugString.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "WoRGameplayEffects.generated.h"

enum class ESubStat : uint8;

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

// B-042k (Q-042k): one class per role for the types whose effects run game code. They grant
// nothing and modify nothing: readers ask HasEffectOfType / GetTotalStatModifier, and the enum
// path still runs the game code until B-042h. Action and Periodic types mostly run at once and
// are rarely stored.

/** Reflect, Reflect Physical, Reflect Spell: a share of a hit sent back. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_Reflect : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_Reflect();
};

/** Last Stand: the holder survives a killing blow once. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_LastStand : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_LastStand();
};

/** A rider on hits dealt or taken: Lifesteal, Absorb (to EP), Apply Burn / Chill / Stun, Retaliation. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_OnHit : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_OnHit();
};

/** Changes how the holder's attacks resolve: Guaranteed Crit, Ignore Defense, Double Hit. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_AttackModifier : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_AttackModifier();
};

/** Changes the holder's turns: Random Skill (Void broken), Skip Turn, Extra Action. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_Control : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_Control();
};

/** Restores, drains or deals something on its windows: energy / HP restore and drain, Heal, Self Damage, Burst, status buildup up or down. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_Periodic : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_Periodic();
};

/** A one-shot action held as an effect: the cleanses, Remove * Debuff, Push Back, Pull Forward. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_Action : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_Action();
};

/** B-042p (StatMathsRedesign.md §2.1, Q-042p): a composed stat's gear, stone and transient layers
 *  (FWoRStatLayers) as one Infinite effect of three MultiplyCompound modifiers on its attribute,
 *  each magnitude by SetByCaller.Stat.*. The attribute sync applies one per stat over the capped
 *  stat base; the attribute set clamps the result as Compose() does. */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UWoRGE_StatSources : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	/** The sources effect for Stat; null when Stat isn't one of the four Pattern P stats. */
	static TSubclassOf<UGameplayEffect> GetClassFor(ESubStat Stat);

protected:
	void AddSourceModifiers(const FGameplayAttribute &Attribute);
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_SpellDamage : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_SpellDamage();
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_RawDamage : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_RawDamage();
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_ActionSpeed : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_ActionSpeed();
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_SpellSpeed : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_SpellSpeed();
};
