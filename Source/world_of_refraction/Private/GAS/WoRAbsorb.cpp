// WoRAbsorb.cpp

#include "GAS/WoRAbsorb.h"
#include "UObject/Class.h"

namespace
{
	constexpr int32 UNLIMITED_USES = 0;
}

void FWoRAbsorbCharge::Fill()
{
	UsesLeft = MaxUses;
	PoolLeft = Mode == EWoRChargeMode::Pool ? Magnitude : 0;
}

bool FWoRAbsorbCharge::Catches(ESpellElement HitElement, EPhysicalDamageType HitPhysical) const
{
	switch (Verb)
	{
	case EWoRAbsorbVerb::Barrier:
		return true;
	case EWoRAbsorbVerb::Shield:
		// Physical = no element + a physical type (EffectsOverhaul.md §2.5, until D-037's damage type).
		return HitElement == ESpellElement::None && HitPhysical != EPhysicalDamageType::None;
	case EWoRAbsorbVerb::Ward:
		return HitElement == Element && Element != ESpellElement::None;
	default:
		return false;
	}
}

bool FWoRAbsorbCharge::IsLive() const
{
	if (Mode == EWoRChargeMode::Pool)
	{
		return PoolLeft > 0;
	}
	return MaxUses == UNLIMITED_USES || UsesLeft > 0;
}

bool FWoRAbsorbCharge::IsUsedUp() const
{
	if (Mode == EWoRChargeMode::Pool)
	{
		return PoolLeft <= 0;
	}
	// A timed counted charge waits for its refill at the holder's turn start.
	return !bTimed && MaxUses != UNLIMITED_USES && UsesLeft <= 0;
}

FString FWoRAbsorbCharge::ToString() const
{
	const FString Scope = Verb == EWoRAbsorbVerb::Ward ? FString::Printf(TEXT(" %s"), *UEnum::GetDisplayValueAsText(Element).ToString())
													   : FString();
	const FString Spend = Mode == EWoRChargeMode::Pool
							  ? FString::Printf(TEXT("pool %d/%d"), PoolLeft, Magnitude)
							  : (MaxUses == UNLIMITED_USES ? FString::Printf(TEXT("up to %d, unlimited"), Magnitude)
														   : FString::Printf(TEXT("up to %d, %d/%d use(s)"), Magnitude, UsesLeft, MaxUses));
	return FString::Printf(TEXT("%s%s: %s%s (#%d)"), *UEnum::GetDisplayValueAsText(Verb).ToString(), *Scope, *Spend,
						   bTimed ? TEXT(", timed") : TEXT(""), Arrival);
}

namespace WoRAbsorb
{
	int32 Spend(TArrayView<FWoRAbsorbCharge *> Charges, int32 Damage, ESpellElement HitElement, EPhysicalDamageType HitPhysical)
	{
		TArray<FWoRAbsorbCharge *> Ordered(Charges.GetData(), Charges.Num());
		Ordered.RemoveAll([](const FWoRAbsorbCharge *Charge) { return Charge == nullptr; });
		Ordered.Sort([](const FWoRAbsorbCharge &A, const FWoRAbsorbCharge &B)
		{
			return A.Verb != B.Verb ? A.Verb < B.Verb : A.Arrival < B.Arrival;
		});

		int32 Remaining = FMath::Max(0, Damage);
		for (FWoRAbsorbCharge *Charge : Ordered)
		{
			if (Remaining <= 0)
			{
				break;
			}
			if (!Charge->IsLive() || !Charge->Catches(HitElement, HitPhysical))
			{
				continue;
			}
			if (Charge->Mode == EWoRChargeMode::Pool)
			{
				const int32 Negated = FMath::Min(Remaining, Charge->PoolLeft);
				Charge->PoolLeft -= Negated;
				Remaining -= Negated;
				continue;
			}
			Remaining -= FMath::Min(Remaining, Charge->Magnitude);
			if (Charge->MaxUses != UNLIMITED_USES)
			{
				--Charge->UsesLeft;
			}
		}
		return FMath::Max(0, Damage) - Remaining;
	}

	bool IsValidCharge(const FWoRAbsorbCharge &Charge)
	{
		// Hits arrive with their element resolved, so a Generic ("inherit") Ward would never catch one.
		const bool bWardElement = Charge.Element != ESpellElement::None && Charge.Element != ESpellElement::Generic;
		return Charge.Magnitude > 0 && Charge.MaxUses >= 0 && (Charge.Verb != EWoRAbsorbVerb::Ward || bWardElement);
	}
}
