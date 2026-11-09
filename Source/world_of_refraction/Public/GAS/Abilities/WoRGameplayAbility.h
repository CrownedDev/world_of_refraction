// WoRGameplayAbility.h
// B-043b (GAS stage 4 part 2, Q-043b): the one GameplayAbility class a combatant's skills are granted as.
// One spec per skill, the skill asset (USkillDataBase) as the spec's source object, its Ability.Shape.* tags
// on the spec (WoRSkillAbilities::GetShapeTags). Activating it runs the FAction handed over by
// FWoRSkillActivationScope through UActionExecutor::ExecuteActionAsync, and the ability ends when that action
// completes. The executor still pays the costs at commit (SpendEnergy, B-043a's cost GEs), so CheckCost is
// the executor's spendable-EP check and ApplyCost pays nothing: exactly one payment.
// Instanced per actor, server only (Q-043b). Later parts (B-043c..g) split it per delivery shape.
// Granting and the orchestrator hand-off: GAS/Abilities/WoRSkillAbilities.h. Debug: wor.Abilities.Print.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Combat/Actions/ActionExecutor.h"
#include "WoRGameplayAbility.generated.h"

class USkillDataBase;

/** While alive, an activation of Handle runs Action and reports to OnComplete. Lives on the caller's stack
 *  around one TryActivateAbility (WoRSkillAbilities::TryExecuteThroughAbility): TryActivateAbility carries
 *  no payload, and CheckCost (inside CanActivateAbility) never sees gameplay-event data (Q-043b). */
struct WORLD_OF_REFRACTION_API FWoRSkillActivationScope
{
	FWoRSkillActivationScope(FGameplayAbilitySpecHandle InHandle, const FAction &InAction, FOnActionComplete InOnComplete);
	~FWoRSkillActivationScope();

	FWoRSkillActivationScope(const FWoRSkillActivationScope &) = delete;
	FWoRSkillActivationScope &operator=(const FWoRSkillActivationScope &) = delete;

	/** The innermost scope for Handle, or null. */
	static FWoRSkillActivationScope *Find(FGameplayAbilitySpecHandle Handle);

	FGameplayAbilitySpecHandle Handle;
	const FAction &Action;
	FOnActionComplete OnComplete;
	/** Set by the ability once it took the action: the caller must not run it again. */
	bool bConsumed = false;

private:
	FWoRSkillActivationScope *Outer = nullptr;
	static FWoRSkillActivationScope *Innermost;
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UWoRGameplayAbility();

	/** The skill this spec was granted for (its source object), or null. */
	USkillDataBase *GetSkill(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo) const;

	/** The action this instance is running, or null when idle. */
	const FAction *GetRunningAction() const { return bActionRunning ? &RunningAction : nullptr; }

	/** EP check only: the executor's spendable EP against the action's cost (the action being activated, else
	 *  the skill's plain cast, WoRSkillAbilities::MakePlainAction). */
	/** Refused while an earlier action still runs (the ability was cancelled before it finished): that action
	 *  keeps its own caller, and the new one runs the old call. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
									const FGameplayTagContainer *SourceTags = nullptr,
									const FGameplayTagContainer *TargetTags = nullptr,
									FGameplayTagContainer *OptionalRelevantTags = nullptr) const override;

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
						   FGameplayTagContainer *OptionalRelevantTags = nullptr) const override;

	/** Pays nothing: the executor pays the EP at commit (and the backlash after the action), once. */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
						   const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo,
								 const FGameplayAbilityActivationInfo ActivationInfo,
								 const FGameplayEventData *TriggerEventData) override;

private:
	/** The executor's completion: ends this ability, then hands the result on to whoever started the action. */
	void HandleActionComplete(const FActionResult &Result);

	/** Ends the ability with a failed result handed on (no executor, no action, the commit failed). */
	void FailActivation(const FString &Reason);

	/** A UPROPERTY so its targets and skill asset stay referenced while the action runs. */
	UPROPERTY(Transient)
	FAction RunningAction;

	FOnActionComplete ForwardOnComplete;
	bool bActionRunning = false;
};
