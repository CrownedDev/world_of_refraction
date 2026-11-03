// ElementColorVision.h
// B-294 (D-106): checks the element palette the way analysis/ElementPalette_2026-10-29.md did. Colour-blind vision is
// simulated with Machado, Oliveira & Fernandes 2009 (severity 1, applied in linear RGB) and colours are compared with
// CIEDE2000 (Sharma, Wu & Dalal 2005, CIELAB / D65). Under READABLE_DELTA_E two colours can't be told apart at
// damage-number size.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"

namespace ElementColorVision
{
	enum class EColorVision : uint8
	{
		Normal,
		Protan,
		Deutan,
		Tritan
	};

	constexpr float READABLE_DELTA_E = 10.0f;

	struct FPairDistance
	{
		ESpellElement A = ESpellElement::None;
		ESpellElement B = ESpellElement::None;
		EColorVision Where = EColorVision::Normal;
		float DeltaE = 0.0f;
	};

	/** The nine elements (no None / Generic). */
	WORLD_OF_REFRACTION_API const TArray<ESpellElement> &Elements();

	/** Linear colour as seen with Vision (linear, clamped 0..1). */
	WORLD_OF_REFRACTION_API FLinearColor Simulate(const FLinearColor &Linear, EColorVision Vision);

	/** CIEDE2000 between two linear colours. */
	WORLD_OF_REFRACTION_API float DeltaE2000(const FLinearColor &A, const FLinearColor &B);

	/** Every pair of the nine element colours at its smallest distance over the four visions, closest first. */
	WORLD_OF_REFRACTION_API TArray<FPairDistance> ElementPairsClosestFirst();

	/** Each element's sRGB hex as seen in normal, protan, deutan and tritan vision, then the closest pairs. */
	WORLD_OF_REFRACTION_API FString GetElementPaletteString();
}
