// TurnStartEnergy.cpp

#include "Combat/Mechanics/TurnStartEnergy.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "GameFramework/Actor.h"
#include "WoRLog.h"

int32 TurnStartEnergy::GetAmount(const AActor *Actor)
{
	const UCharacterDataComponent *Char = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	if (!Char || Char->MaxEP <= 0)
	{
		return 0;
	}
	return FMath::Max(CombatConstants::TURN_START_EP_MIN,
					  FMath::RoundToInt(Char->MaxEP * CombatConstants::TURN_START_EP_SHARE_OF_MAX));
}

int32 TurnStartEnergy::Apply(AActor *Actor)
{
	UCharacterDataComponent *Char = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const int32 Amount = GetAmount(Actor);
	// A holder the turn-start payouts killed gains nothing.
	if (!Char || Amount <= 0 || !Char->bIsAlive)
	{
		return 0;
	}
	const int32 Before = Char->CurrentEP;
	Char->ServerGainEnergy(Amount);
	const int32 Gained = Char->CurrentEP - Before;
	UE_LOG(LogWoRCombat, Verbose, TEXT("[TurnStartEnergy] %s: +%d EP at turn start (offered %d; EP %d/%d)"),
		   *Actor->GetName(), Gained, Amount, Char->CurrentEP, Char->MaxEP);
	return Gained;
}
