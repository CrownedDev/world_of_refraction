// TextSizeSetting.cpp

#include "UI/Settings/TextSizeSetting.h"

#include "Engine/GameInstance.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Save/SaveSubsystem.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRTextSize"

namespace TextSizeScales
{
	/** B-453d (Q-453d): about 15% a step, as The Last of Us Part II's and Spider-Man's text size options grow. */
	constexpr float NORMAL = 1.0f;
	constexpr float LARGE = 1.15f;
	constexpr float LARGER = 1.3f;
}

FText WoRTextSize::GetSettingName()
{
	return LOCTEXT("TextSize", "Text size");
}

FText WoRTextSize::GetLevelName(ETextSizeLevel Level)
{
	switch (Level)
	{
	case ETextSizeLevel::Large:
		return LOCTEXT("TextSizeLarge", "Large");
	case ETextSizeLevel::Larger:
		return LOCTEXT("TextSizeLarger", "Larger");
	case ETextSizeLevel::Normal:
	default:
		return LOCTEXT("TextSizeNormal", "Normal");
	}
}

float WoRTextSize::GetScale(ETextSizeLevel Level)
{
	switch (Level)
	{
	case ETextSizeLevel::Large:
		return TextSizeScales::LARGE;
	case ETextSizeLevel::Larger:
		return TextSizeScales::LARGER;
	case ETextSizeLevel::Normal:
	default:
		return TextSizeScales::NORMAL;
	}
}

namespace
{
	/** The project's own ApplicationScale, read before the first change, so the levels multiply it and the last
	 *  Restore puts it back exactly. */
	TOptional<float> GProjectTextScale;
	/** Who has applied a level and not restored yet (each game instance's save subsystem, PIE clients included): the
	 *  scale is one per process, so it goes back only when the last one ends. */
	TSet<const UObject *> GTextScaleOwners;
}

void WoRTextSize::Apply(ETextSizeLevel Level, const UObject *Owner)
{
	UUserInterfaceSettings *Settings = GetMutableDefault<UUserInterfaceSettings>();
	if (!Settings)
	{
		return;
	}
	if (!GProjectTextScale.IsSet())
	{
		GProjectTextScale = Settings->ApplicationScale;
	}
	GTextScaleOwners.Add(Owner);
	const float Scale = GProjectTextScale.GetValue() * GetScale(Level);
	if (!FMath::IsNearlyEqual(Settings->ApplicationScale, Scale))
	{
		Settings->ApplicationScale = Scale; // read by the game viewport's DPI scale every frame
		UE_LOG(LogWoRUI, Log, TEXT("[TextSize] %s"), *GetTextSizeString(Level));
	}
}

void WoRTextSize::Restore(const UObject *Owner)
{
	GTextScaleOwners.Remove(Owner);
	if (GTextScaleOwners.Num() > 0 || !GProjectTextScale.IsSet())
	{
		return; // another game instance still shows its own level
	}
	if (UUserInterfaceSettings *Settings = GetMutableDefault<UUserInterfaceSettings>())
	{
		Settings->ApplicationScale = GProjectTextScale.GetValue();
	}
	GProjectTextScale.Reset();
}

FString WoRTextSize::GetTextSizeString(ETextSizeLevel Level)
{
	const UUserInterfaceSettings *Settings = GetDefault<UUserInterfaceSettings>();
	return FString::Printf(TEXT("text size %s (scale %.2f; UI application scale now %.2f)"), *GetLevelName(Level).ToString(), GetScale(Level),
						   Settings ? Settings->ApplicationScale : 0.0f);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GTextSizePrintCommand(
	TEXT("wor.UI.TextSize"),
	TEXT("B-453d: print the saved text size, its scale and the UI application scale in use."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const USaveSubsystem *Save = GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
		UE_LOG(LogWoRUI, Display, TEXT("[TextSize] %s (%d game instance(s) applying)"),
			   *WoRTextSize::GetTextSizeString(Save ? Save->GetTextSizeLevel() : ETextSizeLevel::Normal), GTextScaleOwners.Num());
	}));
#endif

#undef LOCTEXT_NAMESPACE
