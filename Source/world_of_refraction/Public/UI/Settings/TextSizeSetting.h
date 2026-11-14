// TextSizeSetting.h
// B-453d (D-190): the text size setting. It scales the game's UI (every UMG / CommonUI screen and the HUD) through
// UUserInterfaceSettings::ApplicationScale, which multiplies the project's DPI curve for the game viewport only (the
// editor's own panels are not scaled). Applied when the save loads and when the row changes; put back to Normal when
// the game instance ends, so the editor's project setting is left as it was.

#pragma once

#include "CoreMinimal.h"
#include "TextSizeSetting.generated.h"

/** How large the text is; Normal is the default (FSavedSettings::TextSize). */
UENUM(BlueprintType)
enum class ETextSizeLevel : uint8
{
	Normal,
	Large,
	Larger
};

namespace WoRTextSize
{
	WORLD_OF_REFRACTION_API FText GetSettingName();
	WORLD_OF_REFRACTION_API FText GetLevelName(ETextSizeLevel Level);
	/** The UI scale for Level (Normal 1). */
	WORLD_OF_REFRACTION_API float GetScale(ETextSizeLevel Level);
	/** Sets the game viewport's UI scale for Level. */
	WORLD_OF_REFRACTION_API void Apply(ETextSizeLevel Level);
	/** Debug: the level and the scale in use now. */
	WORLD_OF_REFRACTION_API FString GetTextSizeString(ETextSizeLevel Level);
}
