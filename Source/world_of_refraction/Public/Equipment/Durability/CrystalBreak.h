// CrystalBreak.h
// Between-combat crystal destruction (Phase B, B-035 split out of ACombatOrchestrator): every
// attachment that broke during the fight is cleared from its weapon or ring and force-dismantled
// (§4.2 / §5.3b). A broken evolution is kept (Q-159). RepairOn is the between-combat +repair, which
// also brings a broken evolution back (Q-159c).

#pragma once

#include "CoreMinimal.h"

class AActor;

namespace CrystalBreak
{
	/** Destroys Actor's broken attachments and broken primary evolution; returns how many broke.
	 *  0 for an actor with no loadout. */
	WORLD_OF_REFRACTION_API int32 DestroyBrokenOn(AActor *Actor);

	/** Repairs Actor's attached gems (not broken ones: those were destroyed) and evolutions
	 *  (broken included, Q-159c), plus its primary-slot evolution, by Amount each; returns how
	 *  many gained durability. 0 for an actor with no loadout. */
	WORLD_OF_REFRACTION_API int32 RepairOn(AActor *Actor, int32 Amount);
}
