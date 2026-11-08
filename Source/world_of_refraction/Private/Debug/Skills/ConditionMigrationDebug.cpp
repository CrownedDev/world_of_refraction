// ConditionMigrationDebug.cpp
// B-337e: wor.Conditions.ListOld lists every effect asset still on the old conditions, i.e. filled from them on this
// load and not resaved (SkillConditions::GetUnmigratedEffectsString). Empty after the editor resave pass.

#include "Skills/Effects/SkillConditions.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	FAutoConsoleCommand GListOldConditionsCmd(
		TEXT("wor.Conditions.ListOld"),
		TEXT("DEBUG (B-337e): list the effect assets whose conditions were filled from the old field on load (resave them)."),
		FConsoleCommandDelegate::CreateLambda([]() { UE_LOG(LogWoR, Display, TEXT("%s"), *SkillConditions::GetUnmigratedEffectsString()); }));
}
