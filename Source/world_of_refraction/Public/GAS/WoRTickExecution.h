// WoRTickExecution.h
// GAS stage 3 (B-042n): a DOT's and a HOT's tick run WoRTickMath at the holder's turn start, as the enum
// path does. A DOT tick reads the holder's resistance to its element from the attributes (Resist_<type>, Ice
// also resists Water: UWoRDamageExecution::ReadElementResistance); a HOT tick reads the holder's
// HealingReceivedBuff total as the enum path does (GetTotalStatModifier, GAS mirror or enum store). The value,
// the element and the share-of-max-HP flag travel in the tick spec's context as an FWoRTickRequest. The
// output is Health -= the damage or += the heal. A DOT's BuildupPerTick stays on the enum path until the status bar is GAS's own (B-042o, Q-042n).
// B-042yd: the other payouts run here too (IsPaidByTick): HP drain and self-damage take Health, a percent
// HP restore heals a share of the holder's max HP (amplified like the enum path's), energy restore / drain
// and the percent EP restore change Energy. The max pools are read from the game's character data at the
// tick (Q-042yc: they stay the game's). The Vitals set hands each change to the game's HP / EP path (Q-042y).
// Debug: GetRequestString; WoRTickMath::GetInputsString.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/ESkillEffectType.h"
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
	/** What the tick pays: a DOT, a HOT (HealthRestore) or another payout (B-042yd). */
	ESkillEffectType PayoutType = ESkillEffectType::DOT;

	bool IsHeal() const
	{
		return PayoutType == ESkillEffectType::HealthRestore || PayoutType == ESkillEffectType::RestoreHPPercent;
	}
};

/** The holder's max pools at the tick, from the game's character data (Q-042yc). */
struct FWoRTickPools
{
	int32 MaxHP = 0;
	int32 MaxEP = 0;
};

/** One tick's result: which Vitals attribute it changes and by how much (signed). */
struct FWoRTickPayout
{
	FGameplayAttribute Attribute;
	float Change = 0.0f;
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRTickExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters &ExecutionParams,
										FGameplayEffectCustomExecutionOutput &OutExecutionOutput) const override;

	/** The effect types a tick spec pays (USkillEffectManager::IsPayoutType). */
	static bool IsPaidByTick(ESkillEffectType Type);

	/** Request's inputs with the holder's element resistance (DOT) or healing received (heals) read from Holder. */
	static FTickMathInputs ReadAttributes(const FWoRTickRequest &Request, const UAbilitySystemComponent *Holder);

	/** Holder's max HP / EP: the character data's, or the MaxHealth / MaxEnergy attributes on a bare ASC. */
	static FWoRTickPools ReadPools(const UAbilitySystemComponent *Holder);

	/** The request for Effect's tick on Holder, gathered as USkillEffectManager's tick does. */
	static FWoRTickRequest MakeRequest(const FActiveSkillEffect &Effect, AActor *Holder);

	/** The signed Health change one DOT / HOT tick makes: -damage or +heal. */
	static float GetHealthChange(const FWoRTickRequest &Request, const FTickMathInputs &Inputs);

	/** What one tick of any payout type changes, with the enum path's numbers (USkillEffectManager::ApplyEffectLogic).
	 *  A gain never goes below 0 and a loss never above, as the game's Server* calls ignore those. */
	static FWoRTickPayout GetPayout(const FWoRTickRequest &Request, const FTickMathInputs &Inputs, const FWoRTickPools &Pools);

	static FString GetRequestString(const FWoRTickRequest &Request);
};
