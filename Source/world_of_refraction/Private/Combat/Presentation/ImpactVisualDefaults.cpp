// ImpactVisualDefaults.cpp

#include "Combat/Presentation/ImpactVisualDefaults.h"
#include "NiagaraSystem.h"
#include "UObject/SoftObjectPtr.h"

const UImpactVisualDefaults *UImpactVisualDefaults::Get()
{
	// Loaded once; a missing asset stays null (a skill without an Impact entry then plays nothing).
	static TSoftObjectPtr<UImpactVisualDefaults> Defaults{FSoftObjectPath(DEFAULTS_PATH)};
	return Defaults.LoadSynchronous();
}
