// CrystalBreak.h
// Between-combat crystal destruction (Phase B, B-035 split out of ACombatOrchestrator): every
// attachment that broke during the fight is cleared from its weapon or ring and force-dismantled
// (§4.2 / §5.3b), and a broken primary evolution (e.g. Broken Darkness) is cleared and dismantled.

#pragma once

#include "CoreMinimal.h"

class AActor;

namespace CrystalBreak
{
	/** Destroys Actor's broken attachments and broken primary evolution; returns how many broke.
	 *  0 for an actor with no loadout. */
	WORLD_OF_REFRACTION_API int32 DestroyBrokenOn(AActor *Actor);
}
