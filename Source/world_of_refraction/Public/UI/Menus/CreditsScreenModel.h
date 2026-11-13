// CreditsScreenModel.h
// B-470 (UIScreens #13, mockup screen_mockups/front/credits/1_credits.png): what the credits say, with no widget in it,
// so the specs read it headless. UCreditsScreenWidget scrolls it.
//
// The sections are a C++ table (the game can't read the repo's CREDITS.md files once packaged): keep it in step with
// SourceArt/**/CREDITS.md when art from a new pack ships.
// Debug: wor.UI.PrintCredits logs GetCreditsString.

#pragma once

#include "CoreMinimal.h"

struct FCreditsSection
{
	/** "Created by", "Fonts". */
	FText Heading;
	/** One name or pack a line. */
	TArray<FText> Lines;
};

namespace WoRCredits
{
	/** The scroll's speed, in Slate units a second, and how much faster it goes while Select is held. */
	constexpr float SCROLL_SPEED = 60.0f;
	constexpr float FASTER_SCALE = 5.0f;

	WORLD_OF_REFRACTION_API FText GetGameTitle();
	WORLD_OF_REFRACTION_API TArray<FCreditsSection> BuildSections();

	/** How far the scroll moves in DeltaSeconds. */
	WORLD_OF_REFRACTION_API float GetScrollStep(float DeltaSeconds, bool bFaster);

	WORLD_OF_REFRACTION_API FString GetCreditsString();
}
