// WoRAbsorb.h
// GAS stage 3 (B-042j, D-092): the absorbing family, Barrier / Shield / Ward, ported from the deckbuilder
// (`04-effects.md` "The charge family", EffectsOverhaul.md §2.5). One mechanism, three scopes:
// Barrier catches any damage, Shield physical damage (no element + a physical type), Ward one element.
// Spent by Uses (each caught damage instance spends one charge and negates up to the magnitude;
// 0 charges = unlimited, spends nothing) or by Pool (one budget drawn down across hits).
// Order: Ward, then Shield, then Barrier; oldest first within a verb. A charge that catches nothing
// isn't spent. In our hit they sit after Guard and Defense (Q-044e B), one damage instance per impact
// (Q-044f A). The charges live on the holder's UWoRTurnEffectTracker, keyed by their absorb GE.
// Debug: FWoRAbsorbCharge::ToString, the tracker's GetDebugString, `wor.Absorb.Add`.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Damage/EPhysicalDamageType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "WoRAbsorb.generated.h"

/** The three absorbing verbs, in spend order (Ward first). */
UENUM(BlueprintType)
enum class EWoRAbsorbVerb : uint8
{
	Ward,
	Shield,
	Barrier
};

/** How an absorber's number is spent (the deckbuilder's ChargeMode). */
UENUM(BlueprintType)
enum class EWoRChargeMode : uint8
{
	/** Each caught damage instance negates up to the magnitude and spends a charge (unless unlimited). */
	Uses,
	/** One budget of the magnitude, drawn down across hits. */
	Pool
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWoRAbsorbCharge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Absorb")
	EWoRAbsorbVerb Verb = EWoRAbsorbVerb::Barrier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Absorb")
	EWoRChargeMode Mode = EWoRChargeMode::Uses;

	/** Ward only: the element it catches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Absorb")
	ESpellElement Element = ESpellElement::None;

	/** Uses: the most one instance negates. Pool: the whole budget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Absorb", meta = (ClampMin = 0))
	int32 Magnitude = 0;

	/** Uses: charges per refill; 0 = unlimited (spends nothing). Ignored for Pool. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Absorb", meta = (ClampMin = 0))
	int32 MaxUses = 0;

	/** Runtime: charges left (Uses) or budget left (Pool). */
	int32 UsesLeft = 0;
	int32 PoolLeft = 0;
	/** Runtime: it has a duration (a counted charge then refills at the holder's turn start). */
	bool bTimed = false;
	/** Runtime: when it was gained, for the oldest-first order. */
	int32 Arrival = 0;

	/** Fills the runtime counts from the authored ones. */
	void Fill();

	/** Whether this scope covers a damage instance of Element / Physical. */
	bool Catches(ESpellElement HitElement, EPhysicalDamageType HitPhysical) const;

	/** Uses with charges left (or unlimited), or Pool with budget left. */
	bool IsLive() const;

	/** Spent for good: an untimed Uses with no charges left, or an empty Pool (it then goes). */
	bool IsUsedUp() const;

	FString ToString() const;
};

namespace WoRAbsorb
{
	/** Spend Charges (any order; sorted here) against one damage instance. Returns the damage negated;
	 *  updates the charges' counts. Stops once nothing is left to negate. */
	WORLD_OF_REFRACTION_API int32 Spend(TArrayView<FWoRAbsorbCharge *> Charges, int32 Damage, ESpellElement HitElement,
										EPhysicalDamageType HitPhysical);

	/** A ward with no element would just be a Barrier: refused (logged) by the appliers. */
	WORLD_OF_REFRACTION_API bool IsValidCharge(const FWoRAbsorbCharge &Charge);
}
