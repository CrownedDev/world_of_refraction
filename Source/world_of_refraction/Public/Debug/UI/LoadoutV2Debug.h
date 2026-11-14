// LoadoutV2Debug.h
// B-476: wor.Loadout.PrintV2 prints, per character, the v2 Weapons tab as its model builds it (entries, sockets, the
// ACTIVE tag, skill slots, the cells line and the Add weapon frame), before the v2 widget (B-476b) draws it.

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace LoadoutV2Debug
{
	WORLD_OF_REFRACTION_API void PrintWeaponsPages(UWorld *World);
}
