// WoRTurnEffectTracker.h
// GAS stage 3 (B-042): counts the holder's turns for the lasting GameplayEffects on its ability
// system component. GAS counts seconds, so each lasting GE is Infinite and this component keeps its
// FTurnSpan (the one counting rule, D-051 / D-055): at the holder's turn start a finished span's GE
// is removed by handle, before that turn's payouts; at the holder's turn end the span counts the turn.
// USkillEffectManager calls both hooks from its own turn start/end, so bonus and Execution turns
// count the same way. A GE removed some other way (a cleanse, death) leaves the map through the
// ASC's removed event. A tracked GE may carry a tick spec (a DOT or HOT payout, B-042b): it is
// applied at each holder turn start the GE survives, after the finished spans end (Q-044c), in the
// order the effects arrived, and no more once the holder's Health reaches 0.
// B-042z: it also holds the holder's fires-once uses (D-011) and its armed gear conditionals (C3a); the
// effect manager writes them here and in its own sets, and reads them here at wor.Effects.UseGAS 1.
// Per-fight state on the battle actor (D-064).
// Debug: GetDebugString, PrintState, `wor.DumpTurnEffects` (every combatant, with its GEs).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffectTypes.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Skills/Effects/TurnSpan.h"
#include "WoRTurnEffectTracker.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
struct FActiveGameplayEffect;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UWoRTurnEffectTracker : public UActorComponent
{
	GENERATED_BODY()

public:
	UWoRTurnEffectTracker();

	/** Apply EffectClass to the owner for Turns of its turns (at least 1), with Instigator as the
	 *  effect's source. Invalid handle when the owner has no ASC, the class is null, Turns < 1, or
	 *  the GE isn't Infinite (only Infinite GEs can be turn-counted). */
	FActiveGameplayEffectHandle ApplyForHolderTurns(TSubclassOf<UGameplayEffect> EffectClass, int32 Turns, AActor *Instigator);

	/** Apply a built spec (FWoREffectSpecBuilder) to the owner for Turns of its turns. TickSpec, when
	 *  valid, is paid at each holder turn start while the GE lasts. Same refusals as above. */
	FActiveGameplayEffectHandle ApplySpecForHolderTurns(const FGameplayEffectSpecHandle &Spec, int32 Turns,
														const FGameplayEffectSpecHandle &TickSpec = FGameplayEffectSpecHandle());

	/** Apply a built spec with a span already under way (B-042h: the stored enum copy's span, which may
	 *  have counted its landing turn and read 0, held until the holder's next turn starts). */
	FActiveGameplayEffectHandle ApplySpecForSpan(const FGameplayEffectSpecHandle &Spec, const FTurnSpan &Span,
												 const FGameplayEffectSpecHandle &TickSpec = FGameplayEffectSpecHandle());

	/** Restart a tracked GE's span (a refresh). False when the handle isn't tracked here. */
	bool SetSpan(FActiveGameplayEffectHandle Handle, const FTurnSpan &Span);

	/** The holder's turn starts: remove every GE whose span is over, then pay the survivors' ticks. */
	void OnHolderTurnStart();

	/** The holder's turn ends: every tracked span counts that turn. */
	void OnHolderTurnEnd();

	/** Remove a tracked GE now. False when the handle isn't tracked here. */
	bool RemoveTracked(FActiveGameplayEffectHandle Handle);

	/** Holder turns left on a tracked GE; -1 when the handle isn't tracked here. */
	int32 GetTurnsLeft(FActiveGameplayEffectHandle Handle) const;

	int32 NumTracked() const { return Tracked.Num(); }

	/** B-042z: a fires-once use (EffectID + source key, D-011) spent this fight on this holder. */
	bool HasFiredOnce(int32 EffectID, int32 SourceKey) const { return FiredOnce.Contains(FIntPoint(EffectID, SourceKey)); }
	void RecordFiredOnce(int32 EffectID, int32 SourceKey);
	int32 NumFiredOnce() const { return FiredOnce.Num(); }

	/** B-042z: the gear conditionals armed at battle prep (replaces any earlier set). Null until armed. */
	void ArmConditionals(const TArray<FGatheredEffect> &Conditionals);
	const TArray<FGatheredEffect> *GetArmedConditionals() const { return bArmed ? &ArmedConditionals : nullptr; }
	void DisarmConditionals();

	/** B-042z: a new fight (or its end): the fires-once uses and the armed conditionals go. */
	void ResetFightRecords();

	/** One line per tracked GE: its class and its span. */
	FString GetDebugString() const;

	UFUNCTION(BlueprintCallable, Category = "GAS|Debug", meta = (DevelopmentOnly))
	void PrintState() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UAbilitySystemComponent *GetOwnerASC() const;
	void BindRemovedEvent();
	void HandleEffectRemoved(const FActiveGameplayEffect &Removed);

	struct FTrackedEffect
	{
		FTurnSpan Span;
		FGameplayEffectSpecHandle TickSpec;
		/** When it arrived, for the tick order (TMap order isn't arrival order). */
		int32 Arrival = 0;
	};

	bool IsHolderDown() const;

	TMap<FActiveGameplayEffectHandle, FTrackedEffect> Tracked;
	int32 NextArrival = 0;

	/** Fires-once uses: X = EffectID, Y = source key (0 for a doesn't-stack effect, D-053). */
	TSet<FIntPoint> FiredOnce;
	TArray<FGatheredEffect> ArmedConditionals;
	bool bArmed = false;

	/** The ASC whose removed event this component listens to. */
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle RemovedHandle;
};
