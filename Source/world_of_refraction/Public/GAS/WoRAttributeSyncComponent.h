// WoRAttributeSyncComponent.h
// GAS stage 2 (B-041): keeps the owner's GAS attributes (WoRAttributeSets.h) equal to the composed
// values the existing getters return. Q-041: the getters stay the source of truth until stage 3
// moves effects onto GameplayEffects; this component is the bridge until then.
// Syncs on: BeginPlay, HP/EP change, effect applied/removed/restacked on the owner, status buildup
// change, loadout change, the owner's turn speed re-cached (B-042v), and the owner's own turn start
// (a safety net for changes with no event).
// Only the authority writes; clients get the values by replication.
// B-042p: SpellDamage, RawDamage, ActionSpeed and SpellSpeed are composed on GAS instead: the base
// holds the capped stat (FWoRStatLayers::StatBase) and a UWoRGE_StatSources effect multiplies in the
// gear, stone and buff layers, so their current value still equals the getter (Q-042p). B-042t: the
// pillars the same way, over their world base. B-042u: MaxHealth / MaxEnergy from the layers the last
// RecomputeMaxPools used (a UWoRGE_PoolSources effect over the stat portion, Q-042u). B-042w: the
// resistances from one UWoRGE_ResistanceSources effect per source over a 0 base, and general status
// resistance on the stat-sources shape (Q-042w).
// Debug: UWoRAttributeDebug (wor.CompareAttributes).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeSet.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "Character/StatLayers.h"
#include "Skills/Definitions/ESpellElement.h"
#include "WoRAttributeSyncComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
struct FActiveSkillEffect;
enum class ESubStat : uint8;

/** The stat-sources effect applied for one composed stat or pillar, and the layers it carries. */
struct FWoRAppliedStatSources
{
	FActiveGameplayEffectHandle Handle;
	FWoRStatLayers Layers;
};

/** The pool sources effect applied for one max pool, and the layers it carries. */
struct FWoRAppliedPoolSources
{
	FActiveGameplayEffectHandle Handle;
	FWoRPoolLayers Layers;
};

/** One resistance source effect, and the magnitudes it carries (B-042w). */
struct FWoRAppliedResistanceSources
{
	FActiveGameplayEffectHandle Handle;
	TMap<FGameplayTag, float> Magnitudes;
};

/** The derived-stat effect applied for one attribute, and the buff layer it carries (crit chance only). */
struct FWoRAppliedDerivedStat
{
	FActiveGameplayEffectHandle Handle;
	float Transient = 1.0f;
};

/** One attribute and the value its source getter gives right now. */
struct FWoRAttributeSourceValue
{
	FGameplayAttribute Attribute;
	float Value = 0.0f;
};

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UWoRAttributeSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Each resistance source effect with the magnitudes it should carry for Owner, by SetByCaller tag (B-042w). */
	static TArray<TPair<TSubclassOf<UGameplayEffect>, TMap<FGameplayTag, float>>> GetResistanceSourceMagnitudes(AActor *Owner);

	UWoRAttributeSyncComponent();

	/** Push every source value into the owner's ability system component. */
	UFUNCTION(BlueprintCallable, Category = "GAS")
	void SyncAttributes();

	/** What each mirrored attribute should hold for Actor, read from the existing getters. Empty when
	 *  Actor has no UCharacterDataComponent. Shared with the debug compare so both read one list. */
	static TArray<FWoRAttributeSourceValue> ReadSourceValues(const AActor *Actor);

	/** Out = Attribute, when ASC holds its set and its owner has this sync and character data (so the
	 *  attribute holds the getter's value). Otherwise false and Out untouched: an unsynced set holds
	 *  0s, so the executions keep their gathered inputs (B-042e, B-042l). */
	static bool ReadSyncedAttribute(const UAbilitySystemComponent *ASC, const FGameplayAttribute &Attribute, float &Out);

	/** Gives the owner's ASC both attribute sets (once) and its actor info. Null when the owner has no ASC. */
	static UAbilitySystemComponent *PrepareAbilitySystem(AActor *Owner);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleVitalChanged(int32 Current, int32 Max);
	UFUNCTION()
	void HandleEffectChanged(AActor *Target, const FActiveSkillEffect &Effect);
	UFUNCTION()
	void HandleEffectStacksChanged(AActor *Target, const FActiveSkillEffect &Effect, int32 NewStacks);
	UFUNCTION()
	void HandleBuildupChanged(AActor *Target, float CurrentBuildup, float MaxBuildup, ESpellElement PendingElement, AActor *Source);
	UFUNCTION()
	void HandleLoadoutChanged(int32 NewLoadoutIndex);
	UFUNCTION()
	void HandleTurnStarted(AActor *Actor, int32 TurnNumber);
	UFUNCTION()
	void HandleSpeedChanged(AActor *Actor);

	void BindSources();
	void UnbindSources();

	/** Base = the capped stat (or the pillar's world base); applies its sources effect once and keeps its
	 *  layers current (B-042p, B-042t). */
	void SyncComposedStat(UAbilitySystemComponent &ASC, const FGameplayAttribute &Attribute, const FWoRStatLayers &Layers,
						  TSubclassOf<UGameplayEffect> SourcesClass);
	/** Removes Attribute's stat or pool sources effect, if any. */
	void RemoveStatSources(UAbilitySystemComponent &ASC, const FGameplayAttribute &Attribute);

	/** Each resistance source effect carries its buckets; their attributes' base is 0 (B-042w). */
	void SyncResistanceSources(UAbilitySystemComponent &ASC);
	void SyncResistanceSource(UAbilitySystemComponent &ASC, TSubclassOf<UGameplayEffect> SourceClass,
							  const TMap<FGameplayTag, float> &Magnitudes);

	/** Base = the pool's stat portion; applies its pool sources effect once and keeps its layers current (B-042u). */
	void SyncPoolStat(UAbilitySystemComponent &ASC, const FGameplayAttribute &Attribute, const FWoRPoolLayers &Layers,
					  TSubclassOf<UGameplayEffect> PoolClass);

	/** Base = the derived stat's constant part; applies its derived effect once and keeps crit chance's
	 *  buff layer current (B-042s). */
	void SyncDerivedStat(UAbilitySystemComponent &ASC, const FGameplayAttribute &Attribute, TSubclassOf<UGameplayEffect> DerivedClass);

	TMap<FGameplayAttribute, FWoRAppliedStatSources> AppliedStatSources;
	TMap<FGameplayAttribute, FWoRAppliedPoolSources> AppliedPoolSources;
	TMap<const UClass *, FWoRAppliedResistanceSources> AppliedResistanceSources;
	TMap<FGameplayAttribute, FWoRAppliedDerivedStat> AppliedDerivedStats;
};
