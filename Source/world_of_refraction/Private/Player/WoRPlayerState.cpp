// WoRPlayerState.cpp

#include "Player/WoRPlayerState.h"

#include "Currency/CurrencyComponent.h"

AWoRPlayerState::AWoRPlayerState()
{
	Wallet = CreateDefaultSubobject<UCurrencyComponent>(TEXT("Wallet"));
}
