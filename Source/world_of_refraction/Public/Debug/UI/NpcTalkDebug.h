// NpcTalkDebug.h
// B-471c: wor.Talk.Print prints the talk box every vendor in the level would open (WoRNpcTalk::Build: role, choices,
// V's topics with NEW, the log), without walking up to one.

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace NpcTalkDebug
{
	WORLD_OF_REFRACTION_API void PrintTalkPages(UWorld *World);
}
