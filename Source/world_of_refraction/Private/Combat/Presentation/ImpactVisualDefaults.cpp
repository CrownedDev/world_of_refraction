// ImpactVisualDefaults.cpp

#include "Combat/Presentation/ImpactVisualDefaults.h"
#include "NiagaraSystem.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/StrongObjectPtr.h"
#include "WoRLog.h"

const UImpactVisualDefaults *UImpactVisualDefaults::Get()
{
	// Review: kept alive once loaded (a soft load alone could be collected and reloaded mid-fight), and a missing asset
	// is looked up only once, not on every impact.
	static TStrongObjectPtr<UImpactVisualDefaults> Cached;
	static bool bLookedUp = false;
	if (!bLookedUp)
	{
		bLookedUp = true;
		Cached.Reset(TSoftObjectPtr<UImpactVisualDefaults>(FSoftObjectPath(ImpactVisualDefaultsAsset::PATH)).LoadSynchronous());
		if (!Cached.IsValid())
		{
			UE_LOG(LogWoR, Log, TEXT("[ImpactVFX] No impact defaults at %s: a skill without an Impact entry plays nothing"), ImpactVisualDefaultsAsset::PATH);
		}
	}
	return Cached.Get();
}

FString UImpactVisualDefaults::GetDefaultsString() const
{
	FString Out = FString::Printf(TEXT("%s: %d element hit(s), generic %s, crit x%.2f"), *GetName(), ElementHitVFX.Num(),
								  GenericHitVFX.IsNull() ? TEXT("(none)") : *GenericHitVFX.GetAssetName(), CritImpactVFXScale);
	for (const TPair<ESpellElement, TSoftObjectPtr<UNiagaraSystem>> &Row : ElementHitVFX)
	{
		Out += FString::Printf(TEXT("\n  %s: %s"), *UEnum::GetValueAsString(Row.Key), Row.Value.IsNull() ? TEXT("(none)") : *Row.Value.GetAssetName());
	}
	return Out;
}

#if WITH_EDITOR
void UImpactVisualDefaults::PrintDefaults() const
{
	UE_LOG(LogWoR, Display, TEXT("[ImpactVFX] %s"), *GetDefaultsString());
}
#endif
