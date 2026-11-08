// NotifyValidationDebug.cpp
// B-368: wor.Presentation.Validate prints every skill asset whose montage notifies don't fit its arrays
// (NotifyValidation::GetValidationReportString).

#include "Combat/Presentation/NotifyValidation.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	FAutoConsoleCommand GValidatePresentationCmd(
		TEXT("wor.Presentation.Validate"),
		TEXT("DEBUG: list every skill whose montage notifies don't fit its arrays (Impact 0..HitCount-1 in order, Cast / VFX indices in range)."),
		FConsoleCommandDelegate::CreateLambda([]() { UE_LOG(LogWoR, Display, TEXT("%s"), *NotifyValidation::GetValidationReportString()); }));
}
