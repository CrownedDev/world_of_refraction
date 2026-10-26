// CurrencyWallet.h
// D-046 (B-151): which wallet an actor spends from and earns into. Every caller that used
// FindComponentByClass<UCurrencyComponent> asks here instead.
//
//   1. A pawn whose PlayerState is an AWoRPlayerState: that player's wallet.
//   2. A member of the local party (an AI-played ally has no PlayerState): the party leader's
//      wallet, through the leader's PlayerState.
//   3. Otherwise the actor's own UCurrencyComponent (enemies, a Blueprint game mode that has
//      not picked AWoRPlayerState, test actors). May be null.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UCurrencyComponent;

namespace CurrencyWallet
{
	WORLD_OF_REFRACTION_API UCurrencyComponent *FindFor(const AActor *Actor);

	/** Debug: which wallet Actor uses and why, plus its balances. */
	WORLD_OF_REFRACTION_API FString GetWalletOwnerString(const AActor *Actor);
}
