// WoRBuildupExecution.h
// GAS stage 3 (B-042l, ResistanceRedesign.md §3.3-3.4): the status buildup execution wraps
// WoRBuildupMath::Compute. The inputs that are attributes come from the ability system components:
// the source's StatusMultiplier (unless the caller baked it in), and the target's StatusResistance,
// the hit's type resistances (Resist_<element>, + Resist_<physical type> when it has one) and
// StatusResistBonus. The rest (element-keyed StatusMultiplier buffs, the Broken Darkness stack factor)
// travel in the effect context as an FWoRBuildupRequest gathered by UStatusBuildupManager. The output
// is Buildup += the result. Immunities, the cap trigger and decay stay on UStatusBuildupManager, and
// nothing in play applies this yet (B-042h); the spec proves both paths give the same number.
// Debug: GetRequestString; WoRBuildupMath::GetInputsString.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "Combat/Damage/EPhysicalDamageType.h"
#include "GAS/WoRGameplayEffects.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/BuildupMath.h"
#include "WoRBuildupExecution.generated.h"

class AActor;
class UAbilitySystemComponent;
class UStatusBuildupManager;

/** What a buildup execution needs besides attributes: UStatusBuildupManager's gathered inputs. */
struct FWoRBuildupRequest
{
	FBuildupMathInputs Inputs;
	ESpellElement Element = ESpellElement::None;
	EPhysicalDamageType PhysicalType = EPhysicalDamageType::None;
	/** The hit has a source: only then is the spec's source StatusMultiplier read. */
	bool bHasSource = false;
	/** The caller baked the source StatusMultiplier into Amount (bSkipBaseStatAmp): keep 1. */
	bool bSkipBaseStatAmp = false;
};

UCLASS()
class WORLD_OF_REFRACTION_API UWoRBuildupExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters &ExecutionParams,
										FGameplayEffectCustomExecutionOutput &OutExecutionOutput) const override;

	/** Request's inputs with the attribute-backed fields read from Source and Target. A side whose
	 *  attributes aren't synced (UWoRAttributeSyncComponent::ReadSyncedAttribute) keeps the gathered value. */
	static FBuildupMathInputs ReadAttributes(const FWoRBuildupRequest &Request, const UAbilitySystemComponent *Source,
											 const UAbilitySystemComponent *Target);

	/** The request for one hit, gathered as AddStatusBuildup does (immunities not checked). */
	static FWoRBuildupRequest MakeRequest(const UStatusBuildupManager &Buildup, AActor *Source, AActor *Target, float Amount,
										  ESpellElement Element, EPhysicalDamageType PhysicalType, bool bSkipBaseStatAmp = false);

	/** Build the buildup spec on Source with Request in its context. */
	static FGameplayEffectSpecHandle MakeBuildupSpec(UAbilitySystemComponent &Source, const FWoRBuildupRequest &Request);

	static FString GetRequestString(const FWoRBuildupRequest &Request);
};

/** One buildup hit: Instant, runs UWoRBuildupExecution. */
UCLASS()
class WORLD_OF_REFRACTION_API UWoRGE_Buildup : public UWoRGameplayEffect
{
	GENERATED_BODY()

public:
	UWoRGE_Buildup();
};
