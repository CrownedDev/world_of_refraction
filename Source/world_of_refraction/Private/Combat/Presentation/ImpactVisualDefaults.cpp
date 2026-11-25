// ImpactVisualDefaults.cpp

#include "Combat/Presentation/ImpactVisualDefaults.h"
#include "NiagaraSystem.h"
#include "UObject/SoftObjectPtr.h"
#include "WoRLog.h"

const UImpactVisualDefaults *UImpactVisualDefaults::Get()
{
	// Review: kept alive once loaded (rooted: a soft load alone could be collected and reloaded mid-fight), and a missing
	// asset is looked up only once, not on every impact (an asset authored later needs an editor restart).
	// Desktop review: a weak pointer here, not a static TStrongObjectPtr, whose destructor would run after UObject shutdown.
	static TWeakObjectPtr<UImpactVisualDefaults> Cached;
	static bool bLookedUp = false;
	if (!bLookedUp)
	{
		bLookedUp = true;
		if (UImpactVisualDefaults *Loaded = TSoftObjectPtr<UImpactVisualDefaults>(FSoftObjectPath(ImpactVisualDefaultsAsset::PATH)).LoadSynchronous())
		{
			Loaded->AddToRoot();
			Cached = Loaded;
		}
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
	Out += FString::Printf(TEXT("\n  Cast: Generic preference %s, %d adapter type(s)"),
						   *UEnum::GetDisplayValueAsText(GenericCastVFXPreference).ToString(), AdapterCastVFX.Num());
	for (const TPair<EAdapterForm, TSoftObjectPtr<UNiagaraSystem>> &Row : AdapterCastVFX)
	{
		Out += FString::Printf(TEXT("\n  %s cast: %s"), *UEnum::GetDisplayValueAsText(Row.Key).ToString(), Row.Value.IsNull() ? TEXT("(none)") : *Row.Value.GetAssetName());
	}
	Out += FString::Printf(TEXT("\n  Weapon trail: %s, plain colour %s, wind-up %.2fs"),
						   WeaponTrailVFX.IsNull() ? TEXT("(none)") : *WeaponTrailVFX.GetAssetName(), *PlainTrailColor.ToString(), TrailWindUpSeconds);
	for (const TPair<ESpellElement, TSoftObjectPtr<UNiagaraSystem>> &Row : ElementInfusionGlowVFX)
	{
		Out += FString::Printf(TEXT("\n  %s glow: %s"), *UEnum::GetValueAsString(Row.Key), Row.Value.IsNull() ? TEXT("(none)") : *Row.Value.GetAssetName());
	}
	Out += FString::Printf(TEXT("\n  Physical glow: %s, Raw glow: %s, Ice glow: %s"),
						   PhysicalInfusionGlowVFX.IsNull() ? TEXT("(none)") : *PhysicalInfusionGlowVFX.GetAssetName(),
						   RawInfusionGlowVFX.IsNull() ? TEXT("(none)") : *RawInfusionGlowVFX.GetAssetName(),
						   IceInfusionGlowVFX.IsNull() ? TEXT("(none)") : *IceInfusionGlowVFX.GetAssetName());
	// B-626: the looks by source.
	Out += FString::Printf(TEXT("\n  Ring flames: %d element(s), default %s; body auras: %d element(s), default %s; fingers: %s"),
						   RingFlameLooks.ElementVFX.Num(), RingFlameLooks.DefaultVFX.IsNull() ? TEXT("(none)") : *RingFlameLooks.DefaultVFX.GetAssetName(),
						   BodyAuraLooks.ElementVFX.Num(), BodyAuraLooks.DefaultVFX.IsNull() ? TEXT("(none)") : *BodyAuraLooks.DefaultVFX.GetAssetName(),
						   *FString::JoinBy(RingFingerSockets, TEXT(", "), [](const FName &Socket) { return Socket.ToString(); }));
	for (int32 Row = 0; Row < InfusionLevelVisuals.Num(); ++Row)
	{
		Out += FString::Printf(TEXT("\n  Level %d: intensity x%.2f, size x%.2f"), Row + 1, InfusionLevelVisuals[Row].Intensity, InfusionLevelVisuals[Row].Size);
	}
	return Out;
}

UImpactVisualDefaults::UImpactVisualDefaults()
{
	// TODO: tune. Level 1 keeps the old 1.25x size at plain brightness; level 2 the old 1.5x, brighter.
	constexpr float LEVEL_ONE_INTENSITY = 1.0f;
	constexpr float LEVEL_ONE_SIZE = 1.25f;
	constexpr float LEVEL_TWO_INTENSITY = 1.5f;
	constexpr float LEVEL_TWO_SIZE = 1.5f;
	FInfusionLevelVisual LevelOne;
	LevelOne.Intensity = LEVEL_ONE_INTENSITY;
	LevelOne.Size = LEVEL_ONE_SIZE;
	FInfusionLevelVisual LevelTwo;
	LevelTwo.Intensity = LEVEL_TWO_INTENSITY;
	LevelTwo.Size = LEVEL_TWO_SIZE;
	InfusionLevelVisuals = {LevelOne, LevelTwo};
	RingFingerSockets = InfusionLook::GetDefaultRingFingerSockets();
}

TSoftObjectPtr<UNiagaraSystem> UImpactVisualDefaults::GetInfusionLookVFX(EInfusionLookKind Kind, ESpellElement Element, bool bRaw, bool bIce) const
{
	const FInfusionLookSet *Set = Kind == EInfusionLookKind::Ring ? &RingFlameLooks : (Kind == EInfusionLookKind::Aura ? &BodyAuraLooks : nullptr);
	if (!Set)
	{
		return Kind == EInfusionLookKind::Weapon ? GetInfusionGlowVFX(Element, bRaw, bIce) : TSoftObjectPtr<UNiagaraSystem>();
	}
	return bRaw ? Set->DefaultVFX : Set->GetVFX(Element); // Ice is Water's damage type: Water's row, tinted Ice
}

TSoftObjectPtr<UNiagaraSystem> UImpactVisualDefaults::GetInfusionGlowVFX(ESpellElement Element, bool bRaw, bool bIce) const
{
	if (bRaw)
	{
		return RawInfusionGlowVFX;
	}
	if (bIce && Element == ESpellElement::Water && !IceInfusionGlowVFX.IsNull())
	{
		return IceInfusionGlowVFX;
	}
	if (Element == ESpellElement::None || Element == ESpellElement::Generic)
	{
		return PhysicalInfusionGlowVFX;
	}
	const TSoftObjectPtr<UNiagaraSystem> *Glow = ElementInfusionGlowVFX.Find(Element);
	return Glow ? *Glow : TSoftObjectPtr<UNiagaraSystem>();
}

FInfusionLevelVisual UImpactVisualDefaults::GetInfusionLevelVisual(int32 Level) const
{
	if (Level <= 0 || InfusionLevelVisuals.IsEmpty())
	{
		return FInfusionLevelVisual();
	}
	return InfusionLevelVisuals[FMath::Min(Level, InfusionLevelVisuals.Num()) - 1];
}

FInfusionLevelVisual UImpactVisualDefaults::ForInfusionLevel(int32 Level)
{
	const UImpactVisualDefaults *Defaults = Get();
	return (Defaults ? Defaults : GetDefault<UImpactVisualDefaults>())->GetInfusionLevelVisual(Level);
}

#if WITH_EDITOR
void UImpactVisualDefaults::PrintDefaults() const
{
	UE_LOG(LogWoR, Display, TEXT("[ImpactVFX] %s"), *GetDefaultsString());
}
#endif
