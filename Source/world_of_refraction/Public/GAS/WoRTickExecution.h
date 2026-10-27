// WoRTickExecution.h
// GAS stage 3 (B-042n): a DOT's and a HOT's tick run WoRTickMath at the holder's turn start, as the enum
// path does. A DOT tick reads the holder's resistance to its element from the attributes (Resist_<type>, Ice
// also resists Water: UWoRDamageExecution::ReadElementResistance); a HOT tick reads the holder's
// HealingReceivedBuff total as the enum path does (GetTotalStatModifier, GAS mirror or enum store). The value,
// the element and the share-of-max-HP flag travel in the tick spec's context as an FWoRTickRequest. The
// output is Health -= the damage or += the heal. A DOT's BuildupPerTick stays on the enum path until the status bar is GAS's own (B-042o, Q-042n).
// Nothing in play applies these yet (B-042h); the spec proves both paths give the same number.
// Debug: GetRequestString; WoRTickMath::GetInputsString.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/TickMath.h"
#include "WoRTickExecution.generated.h"

class AActor;
class UAbilitySystemComponent;
struct FActiveSkillEffect;

/** What a tick execution needs besides attributes. */
struct FWoRTickRequest
{
	/** Gathered as the enum path does when the effect lands: the fallback when the holder's attributes aren't
	 *  synced (then a DOT keeps the resistance it landed with, Q-042n). */
	FTickMathInputs Inputs;
	ESpellElement Element = ESpellElement::None;
	/** A HOT tick (heal); otherwise a DOT tick (damage). */
	bool bHeal = false;
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRTickExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters &ExecutionParams,
										FGameplayEffectCustomExecutionOutput &OutExecutionOutput) const override;

	/** Request's inputs with the holder's element resistance (DOT) or healing received (HOT) read from Holder. */
	static FTickMathInputs ReadAttributes(const FWoRTickRequest &Request, const UAbilitySystemComponent *Holder);

	/** The request for Effect's tick on Holder, gathered as USkillEffectManager's tick does. */
	static FWoRTickRequest MakeRequest(const FActiveSkillEffect &Effect, AActor *Holder);

	/** The signed Health change one tick makes: -damage or +heal. */
	static float GetHealthChange(const FWoRTickRequest &Request, const FTickMathInputs &Inputs);

	static FString GetRequestString(const FWoRTickRequest &Request);
};
