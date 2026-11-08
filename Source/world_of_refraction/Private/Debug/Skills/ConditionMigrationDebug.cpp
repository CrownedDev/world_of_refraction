// ConditionMigrationDebug.cpp
// B-337e: wor.Conditions.ListOld lists every effect asset still on the old conditions, i.e. filled from them on this
// load and not resaved (SkillConditions::GetUnmigratedEffectsString). Empty after the editor resave pass.
// Q-337e: wor.Conditions.ListAnyOf (the sweep's Any of list) and wor.Conditions.Tree (trigger, then indented checks).

#include "Skills/Effects/SkillConditions.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	FAutoConsoleCommand GListOldConditionsCmd(
		TEXT("wor.Conditions.ListOld"),
		TEXT("DEBUG (B-337e): list the effect assets whose conditions were filled from the old field on load (resave them)."),
		FConsoleCommandDelegate::CreateLambda([]() { UE_LOG(LogWoR, Display, TEXT("%s"), *SkillConditions::GetUnmigratedEffectsString()); }));

	FAutoConsoleCommand GListAnyOfCmd(
		TEXT("wor.Conditions.ListAnyOf"),
		TEXT("DEBUG (Q-337e): list every effect using Any of (migrated OR rows), as trees; the content sweep rewrites them without 'or'."),
		FConsoleCommandDelegate::CreateLambda([]() { UE_LOG(LogWoR, Display, TEXT("%s"), *SkillConditions::GetAnyOfEffectsString()); }));

	FAutoConsoleCommand GConditionTreesCmd(
		TEXT("wor.Conditions.Tree"),
		TEXT("DEBUG (Q-337e): every authored effect's conditions as the deckbuilder shows them: the trigger, then the indented checks."),
		FConsoleCommandDelegate::CreateLambda([]() { UE_LOG(LogWoR, Display, TEXT("%s"), *SkillConditions::GetConditionTreesString()); }));
}
