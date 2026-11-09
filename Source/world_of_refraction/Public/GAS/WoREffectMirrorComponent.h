// WoREffectMirrorComponent.h
// GAS stage 3 (B-042c): the facade's first half. Keeps one GameplayEffect on the owner's ASC for
// every effect the enum path (USkillEffectManager) stores on the owner whose type has a family GE
// (FWoREffectSpecBuilder::GetEffectClass): same identity, element and stacked value. The enum path
// still owns lifetime, stacking and ticks, so the mirrored GEs are Infinite, untracked and never
// tick; they come and go with the stored effects (OnEffectApplied / Removed / StacksChanged /
// DurationChanged on the owner). Q-042c: the effect manager's type and total reads (HasEffectOfType,
// IsStunned, IsSilenced, GetTotalStatModifier[ForElement]) read these GEs for ported types on an owner
// with a mirror (B-042g2: always; the `wor.Effects.UseGAS` switch is gone, and an actor without a
// mirror keeps the enum path until B-042g5). A GAS read first
// resyncs the actor's mirror, so a read between a change and its event (turn order re-reading
// speed, the HP/EP pool on its own OnEffectApplied) sees the change too.
// The GAS reads count every GE with a WoR context on the ASC and the owner's tags: once anything
// else puts such GEs on it (B-042d), they count too, so the mirror must give way by then (B-042h).
// B-042h (Q-042h): on an owner with a UWoRTurnEffectTracker, GAS owns the lifetime. A
// timed effect's GE is turn-counted by the tracker from the stored effect's span. A refresh in
// ApplyEffect pushes its new span to the tracker. The stored enum effect becomes a read-only copy:
// its turns follow the tracker (GetTrackedTurnsLeft), and when the tracker ends a GE at the holder's
// turn start, ReleaseEndedCopies removes the copy through the effect manager. Removing a stored
// effect (cleanse, death, combat end) still ends its GE through the sync. Payouts stay on the enum
// path until B-042y routes the tick's Health write through the real HP path (Q-042hb).
// B-042ye: the per-effect reads (the AI's tick and turns, the debuff count, the character panel's list)
// go through ReadHeldEffects / PreviewDotTick.
// Debug: UWoREffectDebug (wor.Effects.Compare).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GAS/WoREffectContext.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "WoREffectMirrorComponent.generated.h"

class UAbilitySystemComponent;
class UWoRTurnEffectTracker;

/** B-318: a client received a new held-effects snapshot (the character panel redraws its list). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeldSnapshotChanged, AActor *, Holder);
struct FActiveSkillEffect;

/** One stored enum-path effect as the mirror sees it: who it is and what it adds up to. */
struct FWoRMirroredEffect
{
	FWoREffectIdentity Identity;
	float Magnitude = 0.0f;
	FActiveGameplayEffectHandle Handle;
	/** Its GE is turn-counted by the owner's tracker (GAS owns its lifetime, B-042h). */
	bool bTracked = false;
	/** A DOT's or HOT's payout (UWoRTickExecution), paid by PayTick (B-042y). */
	FGameplayEffectSpecHandle TickSpec;

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

	/** B-261: what a sync reads (the stored effects, the live GE count, the tracker), hashed; a read
	 *  skips the sync while it matches the last sync's. */
	uint32 GetSyncFingerprint() const;
	/** Debug / spec: how many full syncs ran. */
	int32 GetSyncCount() const { return SyncCount; }

	/** Holder turns left on the tracked GE of Effect's identity; INDEX_NONE when it has none. */
	int32 GetTrackedTurnsLeft(const FActiveSkillEffect &Effect) const;

	/** B-042ye (Q-042x, Q-042ye): Stored as GAS holds it: an effect whose GE GAS ended is left out, a live one
	 *  takes the tracker's turns left, the rest stay as stored. Resyncs first (Q-042yb). A read mid-sync gets
	 *  Stored unchanged. B-318 (Q-318): on a client (the mirror is the server's) it is the server's read,
	 *  replicated (GetHeldSnapshot); Stored is ignored there. */
	TArray<FActiveSkillEffect> ReadHeldEffects(const TArray<FActiveSkillEffect> &Stored);

	/** B-318: the server's last GAS read of the owner's held effects (each effect with its turns left), refreshed
	 *  by every sync and replicated to every client (other players' characters get no GEs in Mixed
	 *  replication, so a client can't read its own copy). */
	const TArray<FActiveSkillEffect> &GetHeldSnapshot() const { return HeldSnapshot; }

	/** Fired on a client when a new snapshot arrives (the effect manager's events don't fire there for the server's
	 *  effects). */
	UPROPERTY(BlueprintAssignable, Category = "GAS")
	FOnHeldSnapshotChanged OnHeldSnapshotChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

	/** B-042ye: the HP one tick of the DOT Effect takes now, from its GE's tick spec (UWoRTickExecution), or
	 *  INDEX_NONE where PayTick wouldn't pay it through GAS (no live GE with a tick spec, no Vitals set, no
	 *  character data), so the caller uses the enum path's number. */
	int32 PreviewDotTick(const FActiveSkillEffect &Effect);

	/** Remove, through the effect manager, the stored copies of the effects whose tracked GE GAS
	 *  ended (the tracker at the holder's turn start). Called right after the tracker's turn start. */
	void ReleaseEndedCopies();

	/** A re-apply of an identity GAS ended (refresh or stack) starts it again: its GE comes back. */
	void ForgetEnded(const FActiveSkillEffect &Effect);

	/** B-042y: pay one tick of Effect now through its GE's tick spec (the number from UWoRTickExecution; the
	 *  Vitals set hands the Health / Energy change to the HP / EP path). B-042yd: every payout type, and an
	 *  instant (never stored) pays through a tick spec of its own. False when no live mirrored GE of it carries
	 *  one (or the owner has no Vitals set), so the caller pays on the enum path. */
	bool PayTick(const FActiveSkillEffect &Effect);

	/** What the owner's stored effects should look like on GAS: one entry per ported, stored effect. */
	static TArray<FWoRMirroredEffect> ReadEnumEffects(const AActor *Actor);

	/** The ASC an effect-manager read of Type on Actor should use instead of the enum path: set only
	 *  when Type is ported and Actor carries a mirror (resynced first). */
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

	/** ReadHeldEffects' server answer without the resync (the sync itself refreshes HeldSnapshot with it). */
	TArray<FActiveSkillEffect> BuildHeld(const TArray<FActiveSkillEffect> &Stored);

	UPROPERTY(ReplicatedUsing = OnRep_HeldSnapshot)
	TArray<FActiveSkillEffect> HeldSnapshot;

	UFUNCTION()
	void OnRep_HeldSnapshot();
	/** An ASC listener that re-enters the effect manager mid-sync re-runs the sync instead. */
	bool bSyncing = false;
	bool bResyncRequested = false;
	/** The fingerprint the last finished sync left (unset until one finishes). */
	TOptional<uint32> SyncedFingerprint;
	int32 SyncCount = 0;
};
