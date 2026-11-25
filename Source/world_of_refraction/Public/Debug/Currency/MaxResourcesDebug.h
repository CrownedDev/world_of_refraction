// MaxResourcesDebug.h
// B-623 (Crown, 2026-11-25: "also ensure that we have 9999 or infinete resources"): wor.Debug.MaxResources tops every
// live balance of the played party's wallet (CurrencyBalances::ALL: Gold, Prisms, Diamond, Dormant Essence, Elemental
// and Pillar Essence, Mentor Tokens) up to 9999, then saves, as wor.Debug.SetGearTier does. Every shop, upgrade, repair
// and level cost is paid from these balances; no upgrade consumes a separate material item, so nothing else is topped up.

#pragma once

#include "CoreMinimal.h"

class UCurrencyComponent;

namespace WoRMaxResources
{
	/** The balance each currency is topped up to. */
	constexpr int32 TEST_BALANCE = 9999;

	/** Raises every live balance of Wallet below Target to Target (one above is left as it is). One line per balance
	 *  raised, by its shown name ("Prisms 120 -> 9999"). Server only, as UCurrencyComponent::Add. */
	WORLD_OF_REFRACTION_API TArray<FString> TopUp(UCurrencyComponent &Wallet, int32 Target = TEST_BALANCE);
}
