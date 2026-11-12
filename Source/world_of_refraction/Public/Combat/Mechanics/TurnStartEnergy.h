// TurnStartEnergy.h
// D-089 (changed 2026-11-09), B-173: Energy refills only at the start of each of the holder's turns, a share of its
// Max EP (CombatConstants::TURN_START_EP_SHARE_OF_MAX). EP from basic attacks or Perfect Guards is not a base rule;
// gear effects grant it. Paid through UCharacterDataComponent::ServerGainEnergy, so its gates hold: Broken Darkness
// gains none (absorption is its EP), and a Resonator with nothing to spend EP on gains none.
// Called by ACombatOrchestrator::ProcessStartOfTurnEffects, so every turn kind of the holder refills.
// Debug: Debug/Combat/TurnStartEnergyDebug.h (wor.Energy.Print).

#pragma once

#include "CoreMinimal.h"

class AActor;

namespace TurnStartEnergy
{
	/** The EP Actor would gain at its turn start, before ServerGainEnergy's gates; 0 without character data. */
	WORLD_OF_REFRACTION_API int32 GetAmount(const AActor *Actor);

	/** Gives Actor its turn-start EP; returns the EP actually gained (0 when dead, a gate refused it, or EP is full). */
	WORLD_OF_REFRACTION_API int32 Apply(AActor *Actor);
}
