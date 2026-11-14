// RecruitCardDebug.h
// B-471 (D-179): the recruit card without its widget (B-471b). wor.Party.PrintRecruitCard prints the card of the recruit
// the player faces; wor.Party.RecruitInPlaceOf <slot> takes it with the NPC at that slot stepping out (the card's
// choice). Both act on the nearest recruit in range of the first local player's pawn.

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace RecruitCardDebug
{
	WORLD_OF_REFRACTION_API void PrintNearestRecruitCard(UWorld *World);
	WORLD_OF_REFRACTION_API bool RecruitNearestInPlaceOf(UWorld *World, int32 StepOutSlot);
}
