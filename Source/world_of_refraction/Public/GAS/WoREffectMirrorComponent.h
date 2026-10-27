// WoREffectMirrorComponent.h
// GAS stage 3 (B-042c): the facade's first half. Keeps one GameplayEffect on the owner's ASC for
// every effect the enum path (USkillEffectManager) stores on the owner whose type has a family GE
// (FWoREffectSpecBuilder::GetEffectClass): same identity, element and stacked value. The enum path
// still owns lifetime, stacking and ticks, so the mirrored GEs are Infinite, untracked and never
// tick; they come and go with the stored effects (OnEffectApplied / Removed / StacksChanged /
// DurationChanged on the owner). Q-042c: with `wor.Effects.UseGAS 1` the effect manager's type
// and total reads (HasEffectOfType, IsStunned, IsSilenced, GetTotalStatModifier[ForElement]) read
// these GEs for ported types; 0 (the default) keeps every read on the enum path.
// Debug: UWoREffectDebug (wor.CompareEffects).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GAS/WoREffectContext.h"
#include "WoREffectMirrorComponent.generated.h"

class UAbilitySystemComponent;
struct FActiveSkillEffect;

/** One stored enum-path effect as the mirror sees it: who it is and what it adds up to. */
struct FWoRMirroredEffect
{
	FWoREffectIdentity Identity;
	float Magnitude = 0.0f;
	FActiveGameplayEffectHandle Handle;

	bool Matches(const FWoRMirroredEffect &Other) const;
};

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UWoREffectMirrorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWoREffectMirrorComponent();

	/** Bring the owner's mirrored GEs in line with its stored enum-path effects. */
	UFUNCTION(BlueprintCallable, Category = "GAS")
	void SyncMirror();

	int32 NumMirrored() const { return Mirrored.Num(); }

	/** What the owner's stored effects should look like on GAS: one entry per ported, stored effect. */
	static TArray<FWoRMirroredEffect> ReadEnumEffects(const AActor *Actor);

	/** The value of `wor.Effects.UseGAS`; tests flip it with SetReadsFromGAS. */
	static bool ReadsFromGAS();
	static void SetReadsFromGAS(bool bEnabled);

	/** The ASC an effect-manager read of Type on Actor should use instead of the enum path: set only
	 *  when the switch is on, Type is ported, and Actor carries a mirror. */
	static const UAbilitySystemComponent *GetGASReadSource(const AActor *Actor, ESkillEffectType Type);

	/** The GAS twins of HasEffectOfType: the type's tag when it has one, else any GE of that type. */
	static bool HasEffectOfType(const UAbilitySystemComponent &ASC, ESkillEffectType Type);

	FString GetDebugString() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleEffectChanged(AActor *Target, const FActiveSkillEffect &Effect);
	UFUNCTION()
	void HandleEffectCountChanged(AActor *Target, const FActiveSkillEffect &Effect, int32 NewCount);

	void BindSources();
	void UnbindSources();

	TArray<FWoRMirroredEffect> Mirrored;
};
