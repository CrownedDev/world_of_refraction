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

class UWoRMMC_ClampedStatTerm;

enum class ESubStat : uint8;
enum class EWorldPillar : uint8;

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
	/** The sources effect for Stat; null when Stat isn't composed (UWoRAttributeSet_Stats::GetComposedStats). */
	static TSubclassOf<UGameplayEffect> GetClassFor(ESubStat Stat);

	/** A pillar's sources effect (B-042t): gear = the equipment layer, stone = the crystal layer. */
	static TSubclassOf<UGameplayEffect> GetClassFor(EWorldPillar Pillar);

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

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_StatusMultiplier : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_StatusMultiplier();
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_Defense : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_Defense();
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_CritDamage : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_CritDamage();
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_Luck : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_Luck();
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_Reflex : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_Reflex();
};

/** On the EfficiencyReduction attribute (B-042s); Efficiency derives from it (UWoRGE_DerivedEfficiency). */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_Efficiency : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_Efficiency();
};

/** Turn speed (B-042v): the turn manager's cached base, the attached stone and the net buff; gear stays at 1 (it
 *  is inside the cached base). Not in GetComposedStats: its layers come from UTurnManager::GetTurnSpeedLayers. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_TurnSpeed : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_TurnSpeed();
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_Mind : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_Mind();
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_Body : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_Body();
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_Spirit : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_Spirit();
};

/** General status resistance (B-042w): the capped Spirit stat, gear points and attached stones on
 *  UWoRAttributeSet_Resistance::StatusResistance (WoRResistance::GetGeneralStatusResistanceLayers); transient stays 1. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_StatSources_StatusResistance : public UWoRGE_StatSources
{
	GENERATED_BODY()

public:
	UWoRGE_StatSources_StatusResistance();
};

/** B-042w (Q-042w, ResistanceRedesign.md §3.2): one resistance source as one Infinite effect adding (AddBase) its
 *  bucket to every damage type's resistance, each magnitude by SetByCaller keyed by the type's DamageType.* tag
 *  (WoRTags::DamageTypeToTag); the buff and boss sources add to StatusResistBonus (SetByCaller.Magnitude). The
 *  resistance attributes' base is 0, so the sum of the sources is WoRResistance's sum. */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UWoRGE_ResistanceSources : public UWoRGameplayEffect
{
	GENERATED_BODY()

protected:
	UWoRGE_ResistanceSources();

	void AddTypeModifiers();
	void AddStatusResistBonusModifier();
};

/** #4 typed resistance buffs minus debuffs, and #7 ModifyStatusResist. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_ResistanceSources_Effect : public UWoRGE_ResistanceSources
{
	GENERATED_BODY()

public:
	UWoRGE_ResistanceSources_Effect();
};

/** #5 the class table. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_ResistanceSources_Class : public UWoRGE_ResistanceSources
{
	GENERATED_BODY()

public:
	UWoRGE_ResistanceSources_Class();
};

/** #6 gear rolls. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_ResistanceSources_Gear : public UWoRGE_ResistanceSources
{
	GENERATED_BODY()

public:
	UWoRGE_ResistanceSources_Gear();
};

/** #8 boss rising resistance. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_ResistanceSources_Boss : public UWoRGE_ResistanceSources
{
	GENERATED_BODY()

public:
	UWoRGE_ResistanceSources_Boss();
};

/** B-042u (Q-042u): a max pool's layers (FWoRPoolLayers) as one Infinite effect on MaxHealth / MaxEnergy over
 *  the stat portion: the gear's flat amount added to the base (AddBase, SetByCaller.Stat.Gear), then one multiply
 *  (MultiplyCompound, SetByCaller.Stat.Stone: the stone and buff percents summed). The attribute set rounds it. */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UWoRGE_PoolSources : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	/** The pool sources effect for Attribute (MaxHealth, MaxEnergy); null for any other. */
	static TSubclassOf<UGameplayEffect> GetClassFor(const FGameplayAttribute &Attribute);

protected:
	void AddPoolModifiers(const FGameplayAttribute &Attribute);
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_PoolSources_MaxHealth : public UWoRGE_PoolSources
{
	GENERATED_BODY()

public:
	UWoRGE_PoolSources_MaxHealth();
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_PoolSources_MaxEnergy : public UWoRGE_PoolSources
{
	GENERATED_BODY()

public:
	UWoRGE_PoolSources_MaxEnergy();
};

/** B-042s (Q-042s): a stat derived from other attributes, as one Infinite effect the attribute sync
 *  applies once. Each term is a magnitude calculation that captures its source live (not snapshot) and
 *  clamps it as the getter does (UWoRMMC_ClampedStatTerm), so the derived value follows it with no re-sync. */
UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UWoRGE_DerivedStat : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	/** The derived-stat effect for Attribute (CritChance, Efficiency); null for any other. */
	static TSubclassOf<UGameplayEffect> GetClassFor(const FGameplayAttribute &Attribute);

protected:
	UWoRGE_DerivedStat();

	/** Target += the term's clamped source x coefficient, before the multipliers (AddBase). */
	void AddClampedTerm(const FGameplayAttribute &Target, TSubclassOf<UWoRMMC_ClampedStatTerm> Term);
};

/** Crit chance = (CRIT_CHANCE_BASE + Luck x CRIT_CHANCE_LUCK_BONUS) x the buff layer
 *  (SetByCaller.Stat.Transient, UDamageCalculator::GetCritChanceTransientFactor), clamped [0, 1] by the set. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_DerivedCritChance : public UWoRGE_DerivedStat
{
	GENERATED_BODY()

public:
	UWoRGE_DerivedCritChance();
};

/** Efficiency = 1 - EfficiencyReduction. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_DerivedEfficiency : public UWoRGE_DerivedStat
{
	GENERATED_BODY()

public:
	UWoRGE_DerivedEfficiency();
};
