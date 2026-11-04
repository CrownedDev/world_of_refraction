// CombatControl.h
// B-071: who drives a combatant this fight. One answer for the turn router, the AI
// defense synthesis and the local player's defense input.
//
// A pawn is player-driven when the PlayerController possesses it (the leader) OR its
// UCharacterDataComponent::bPlayerDriven flag is set (a Player-controlled party member
// the PC doesn't possess, D-004). Anything else is bot-driven. A non-pawn actor (the
// test harness's bare combatant) is neither: callers keep their own non-pawn rule.
//
// TODO(co-op): bPlayerDriven means "the LOCAL player"; remote players need an owner id.

#pragma once

#include "CoreMinimal.h"
#include "Character/CharacterDataComponent.h"
#include "GameFramework/Pawn.h"

namespace CombatControl
{
	inline bool IsPlayerDriven(const APawn *Pawn)
	{
		if (!Pawn)
		{
			return false;
		}
		if (Pawn->IsPlayerControlled())
		{
			return true;
		}
		const UCharacterDataComponent *CharComp = Pawn->FindComponentByClass<UCharacterDataComponent>();
		return CharComp && CharComp->bPlayerDriven;
	}

	inline bool IsPlayerDriven(const AActor *Actor)
	{
		return IsPlayerDriven(Cast<APawn>(Actor));
	}

	/** B-346: an AI seat's difficulty for this fight (Medium when none was set, or no character data). */
	inline EAIDifficulty GetAISeatDifficulty(const AActor *Actor)
	{
		const UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		return CharComp ? CharComp->AISeatDifficulty : EAIDifficulty::Medium;
	}

	inline void SetAISeatDifficulty(AActor *Actor, EAIDifficulty Difficulty)
	{
		if (UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr)
		{
			CharComp->AISeatDifficulty = Difficulty;
		}
	}

	/** Set or clear the per-fight Player flag. No effect on possession: a possessed pawn stays player-driven. */
	inline void SetPlayerDriven(AActor *Actor, bool bPlayerDriven)
	{
		if (UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr)
		{
			CharComp->bPlayerDriven = bPlayerDriven;
		}
	}
}
