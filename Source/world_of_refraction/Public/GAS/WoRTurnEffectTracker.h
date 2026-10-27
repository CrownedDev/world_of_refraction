// WoRTurnEffectTracker.h
// GAS stage 3 (B-042): counts the holder's turns for the lasting GameplayEffects on its ability
// system component. GAS counts seconds, so each lasting GE is Infinite and this component keeps its
// FTurnSpan (the one counting rule, D-051 / D-055): at the holder's turn start a finished span's GE
// is removed by handle, before that turn's payouts; at the holder's turn end the span counts the turn.
// USkillEffectManager calls both hooks from its own turn start/end, so bonus and Execution turns
// count the same way. A GE removed some other way (a cleanse, death) leaves the map through the
// ASC's removed event. Per-fight state on the battle actor (D-064).
// Debug: GetDebugString, PrintState (wor.GAS.PrintTurnEffects).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveGameplayEffectHandle.h"
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

	/** The holder's turn starts: remove every GE whose span is over. */
	void OnHolderTurnStart();

	/** The holder's turn ends: every tracked span counts that turn. */
	void OnHolderTurnEnd();

	/** Remove a tracked GE now. False when the handle isn't tracked here. */
	bool RemoveTracked(FActiveGameplayEffectHandle Handle);

	/** Holder turns left on a tracked GE; -1 when the handle isn't tracked here. */
	int32 GetTurnsLeft(FActiveGameplayEffectHandle Handle) const;

	int32 NumTracked() const { return Spans.Num(); }

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

	TMap<FActiveGameplayEffectHandle, FTurnSpan> Spans;

	/** The ASC whose removed event this component listens to. */
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle RemovedHandle;
};
