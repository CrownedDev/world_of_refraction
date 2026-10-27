// WoRAttributeSyncComponent.h
// GAS stage 2 (B-041): keeps the owner's GAS attributes (WoRAttributeSets.h) equal to the composed
// values the existing getters return. Q-041: the getters stay the source of truth until stage 3
// moves effects onto GameplayEffects; this component is the bridge until then.
// Syncs on: BeginPlay, HP/EP change, effect applied/removed/restacked on the owner, status buildup
// change, loadout change, and the owner's own turn start (a safety net for changes with no event).
// Only the authority writes; clients get the values by replication.
// Debug: UWoRAttributeDebug (wor.CompareAttributes).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeSet.h"
#include "Skills/Definitions/ESpellElement.h"
#include "WoRAttributeSyncComponent.generated.h"

class UAbilitySystemComponent;
struct FActiveSkillEffect;

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

	void BindSources();
	void UnbindSources();
};
