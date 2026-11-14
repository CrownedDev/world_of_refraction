// TextSizeSetting.cpp

#include "UI/Settings/TextSizeSetting.h"

#include "Engine/UserInterfaceSettings.h"
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

void WoRTextSize::Apply(ETextSizeLevel Level)
{
	UUserInterfaceSettings *Settings = GetMutableDefault<UUserInterfaceSettings>();
	if (!Settings)
	{
		return;
	}
	const float Scale = GetScale(Level);
	if (!FMath::IsNearlyEqual(Settings->ApplicationScale, Scale))
	{
		Settings->ApplicationScale = Scale; // read by the game viewport's DPI scale every frame
		UE_LOG(LogWoRUI, Log, TEXT("[TextSize] %s"), *GetTextSizeString(Level));
	}
}

FString WoRTextSize::GetTextSizeString(ETextSizeLevel Level)
{
	const UUserInterfaceSettings *Settings = GetDefault<UUserInterfaceSettings>();
	return FString::Printf(TEXT("text size %s (scale %.2f; UI application scale now %.2f)"), *GetLevelName(Level).ToString(), GetScale(Level),
						   Settings ? Settings->ApplicationScale : 0.0f);
}

#undef LOCTEXT_NAMESPACE
