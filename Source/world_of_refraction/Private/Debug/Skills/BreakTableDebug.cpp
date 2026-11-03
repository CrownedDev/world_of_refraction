// BreakTableDebug.cpp
// B-274: wor.Breaks.Print lists every status-break row (BreakTable::GetBreakTableString).

#include "Skills/Effects/BreakTable.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	FAutoConsoleCommand GPrintBreaksCmd(
		TEXT("wor.Breaks.Print"),
		TEXT("DEBUG: print every status-break row (what a capped bar applies, its size and timing)."),
		FConsoleCommandDelegate::CreateLambda([]() { UE_LOG(LogWoR, Display, TEXT("%s"), *BreakTable::GetBreakTableString()); }));
}
