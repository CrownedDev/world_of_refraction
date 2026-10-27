// WoREffectMirrorComponent.h
// GAS stage 3 (B-042c): the facade's first half. Keeps one GameplayEffect on the owner's ASC for
// every effect the enum path (USkillEffectManager) stores on the owner whose type has a family GE
// (FWoREffectSpecBuilder::GetEffectClass): same identity, element and stacked value. The enum path
// still owns lifetime, stacking and ticks, so the mirrored GEs are Infinite, untracked and never
// tick; they come and go with the stored effects (OnEffectApplied / Removed / StacksChanged /
// DurationChanged on the owner). Q-042c: with `wor.Effects.UseGAS 1` the effect manager's type
// and total reads (HasEffectOfType, IsStunned, IsSilenced, GetTotalStatModifier[ForElement]) read
// these GEs for ported types; 0 (the default) keeps every read on the enum path. A GAS read first
// resyncs the actor's mirror, so a read between a change and its event (turn order re-reading
// speed, the HP/EP pool on its own OnEffectApplied) sees the change too.
// The GAS reads count every GE with a WoR context on the ASC and the owner's tags: once anything
// else puts such GEs on it (B-042d), they count too, so the mirror must give way by then (B-042h).
// B-042h (Q-042h): at UseGAS 1 on an owner with a UWoRTurnEffectTracker, GAS owns the lifetime. A
// timed effect's GE is turn-counted by the tracker from the stored effect's span. A refresh in
// ApplyEffect pushes its new span to the tracker. The stored enum effect becomes a read-only copy:
// its turns follow the tracker (GetTrackedTurnsLeft), and when the tracker ends a GE at the holder's
// turn start, ReleaseEndedCopies removes the copy through the effect manager. Removing a stored
// effect (cleanse, death, combat end) still ends its GE through the sync. Payouts stay on the enum
// path until B-042y routes the tick's Health write through the real HP path (Q-042hb).
// Debug: UWoREffectDebug (wor.CompareEffects).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GAS/WoREffectContext.h"
#include "WoREffectMirrorComponent.generated.h"

class UAbilitySystemComponent;
class UWoRTurnEffectTracker;
struct FActiveSkillEffect;

/** One stored enum-path effect as the mirror sees it: who it is and what it adds up to. */
struct FWoRMirroredEffect
{
	FWoREffectIdentity Identity;
	float Magnitude = 0.0f;
	FActiveGameplayEffectHandle Handle;
	/** Its GE is turn-counted by the owner's tracker (GAS owns its lifetime, B-042h). */
	bool bTracked = false;

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

	/** GAS owns Actor's effect lifetime: the switch is on and Actor carries a mirror and a tracker. */
	static bool OwnsLifetime(const AActor *Actor);

	/** Holder turns left on the tracked GE of Effect's identity; INDEX_NONE when it has none. */
	int32 GetTrackedTurnsLeft(const FActiveSkillEffect &Effect) const;

	/** Remove, through the effect manager, the stored copies of the effects whose tracked GE GAS
	 *  ended (the tracker at the holder's turn start). Called right after the tracker's turn start. */
	void ReleaseEndedCopies();

	/** A re-apply of an identity GAS ended (refresh or stack) starts it again: its GE comes back. */
	void ForgetEnded(const FActiveSkillEffect &Effect);

	/** What the owner's stored effects should look like on GAS: one entry per ported, stored effect. */
	static TArray<FWoRMirroredEffect> ReadEnumEffects(const AActor *Actor);

	/** The value of `wor.Effects.UseGAS`; tests flip it with SetReadsFromGAS. */
	static bool ReadsFromGAS();
	static void SetReadsFromGAS(bool bEnabled);

	/** The ASC an effect-manager read of Type on Actor should use instead of the enum path: set only
	 *  when the switch is on, Type is ported, and Actor carries a mirror (resynced first). */
	static const UAbilitySystemComponent *GetGASReadSource(const AActor *Actor, ESkillEffectType Type);

	/** The GAS twins of HasEffectOfType: the type's tag when it has one, else any GE of that type. */
	static bool HasEffectOfType(const UAbilitySystemComponent &ASC, ESkillEffectType Type);

	/** B-042x: a GE of Type a cleanse would remove (no Effect.Origin.Gear asset tag, D-015). */
	static bool HasCleansableEffectOfType(const UAbilitySystemComponent &ASC, ESkillEffectType Type);

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

	UWoRTurnEffectTracker *GetTracker() const;

	TArray<FWoRMirroredEffect> Mirrored;
	/** Identities whose tracked GE GAS ended; their stored copies go in ReleaseEndedCopies. */
	TArray<FWoREffectIdentity> EndedByGAS;
	/** An ASC listener that re-enters the effect manager mid-sync re-runs the sync instead. */
	bool bSyncing = false;
	bool bResyncRequested = false;
};
