// DamageNumberView.h
// B-391b (Q-111, FeelGuide 5, Q-391b): how one floating damage number looks, as plain data with no widgets, so specs
// check it headless (WoR.UI.DamageNumber). UBattleHUDWidget only copies a style into a pooled text block.
// - colour: the hit's damage type (the element colour table; Physical and typeless hits white);
// - size and brightness: the element multiplier (weak bigger, brighter, a glow; resisted smaller, dimmer);
// - immune (a multiplier at or below 0): grey and small;
// - heals: green "+N".
// No "Weak" / "Resist" words or icons (rules 02, FeelGuide 5): the player reads weakness from the number.

#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatPresentationComponent.h"

/** How well a hit landed, as the number shows it. */
enum class EDamageNumberTier : uint8
{
	Weak,
	Neutral,
	Resisted,
	Immune,
	Heal
};

/** Everything a damage number's text block needs. */
struct FDamageNumberStyle
{
	FText Text;
	EDamageNumberTier Tier = EDamageNumberTier::Neutral;
	FLinearColor Colour = FLinearColor::White;
	FLinearColor OutlineColour = FLinearColor::Black;
	int32 OutlineSize = 0;
	int32 FontSize = 0;
	float Scale = 1.0f;
	float Brightness = 1.0f;
	bool bGlow = false;
};

namespace DamageNumberView
{
	// TODO: tune in PIE (placeholder until the CommonUI HUD, D-024). Sizes in Slate units, times in seconds.
	/** |multiplier - 1| within this reads as neutral (rounding noise from stacked modifiers). */
	constexpr float NEUTRAL_BAND = 0.05f;
	/** At or below this the target is immune (unreachable under today's 50% resistance cap; kept for the rule). */
	constexpr float IMMUNE_MULTIPLIER = 0.0f;
	/** Scale = 1 + (multiplier - 1) x this: at the 50% cap a weak hit is 1.5x, a resisted one 0.5x before clamping. */
	constexpr float SCALE_PER_MULTIPLIER = 1.0f;
	constexpr float MIN_SCALE = 0.65f;
	constexpr float MAX_SCALE = 1.6f;
	constexpr float IMMUNE_SCALE = 0.6f;
	constexpr float HEAL_SCALE = 0.85f;      // E33: small green heal numbers
	constexpr float CRIT_SCALE_BONUS = 0.25f; // a crit is bigger, whatever the element did
	/** Brightness = 1 + (multiplier - 1) x this: above 1 the colour moves toward white, below 1 it darkens. */
	constexpr float BRIGHTNESS_PER_MULTIPLIER = 1.0f;
	constexpr float MIN_BRIGHTNESS = 0.5f;
	constexpr float MAX_BRIGHTNESS = 1.5f;
	constexpr int32 BASE_FONT_SIZE = 28;
	constexpr int32 OUTLINE_SIZE = 2;      // E33's dark outline, so a white number reads on a bright sky
	constexpr int32 GLOW_OUTLINE_SIZE = 4; // the weak hit's glow: a wide outline in its own colour
	constexpr float GLOW_ALPHA = 0.8f;

	inline const FLinearColor HEAL_COLOUR(0.25f, 0.85f, 0.3f);
	inline const FLinearColor IMMUNE_COLOUR(0.35f, 0.35f, 0.35f);
	inline const FLinearColor OUTLINE_COLOUR(0.0f, 0.0f, 0.0f, 0.85f);
	/** A resisted hit's colour moves toward this mid grey (not toward black, which vanished under the dark outline). */
	inline const FLinearColor DIM_COLOUR(0.3f, 0.3f, 0.3f);

	// Motion (one HUD timer steps every live number; no Tick).
	constexpr float LIFETIME_SECONDS = 1.5f;   // E33: numbers stay about 1-2 s
	constexpr float RISE_DISTANCE = 60.0f;     // drifts up this far over its life
	constexpr float FADE_START_FRACTION = 0.6f; // fully opaque until this share of its life, then fades out
	constexpr float STACK_SPACING = 30.0f;     // several numbers on one target stack upward (E33)
	constexpr float STEP_SECONDS = 1.0f / 30.0f;
	constexpr int32 MAX_NUMBERS = 16;          // pooled text blocks; the oldest is reused when all are live

	/** Weak / Neutral / Resisted / Immune from the multiplier; Heal for a heal. */
	WORLD_OF_REFRACTION_API EDamageNumberTier ClassifyHit(const FCombatHitShown &Hit);

	/** The hit's colour: its element's (ElementTable), Ice its own; Physical and typeless hits white (Q-111). */
	WORLD_OF_REFRACTION_API FLinearColor GetDamageTypeColour(EWoRDamageType DamageType);

	/** Size from the multiplier, clamped; IMMUNE_SCALE at or below IMMUNE_MULTIPLIER. */
	WORLD_OF_REFRACTION_API float GetScale(float ElementMultiplier);

	/** Brightness from the multiplier, clamped (1 = the plain element colour; above moves toward white, below toward
	 *  DIM_COLOUR). */
	WORLD_OF_REFRACTION_API float GetBrightness(float ElementMultiplier);

	/** The full look of one number. */
	WORLD_OF_REFRACTION_API FDamageNumberStyle MakeStyle(const FCombatHitShown &Hit);

	/** False for a hit that took nothing (a fully negated or buildup-only hit), unless the target is immune. */
	WORLD_OF_REFRACTION_API bool ShouldShow(const FCombatHitShown &Hit);

	/** How far up a number of this age has drifted (Slate units, positive = up). */
	WORLD_OF_REFRACTION_API float GetRiseOffset(float AgeSeconds);

	/** Opacity at this age: 1 until FADE_START_FRACTION of its life, then down to 0 at LIFETIME_SECONDS. */
	WORLD_OF_REFRACTION_API float GetOpacity(float AgeSeconds);

	/** Where a new number on a target starts: one row above the highest of its live numbers (LiveStackIndices),
	 *  so it never lands on a row still in use after a lower one expired; 0 when none are up. */
	WORLD_OF_REFRACTION_API int32 NextStackIndex(const TArray<int32> &LiveStackIndices);

	/** Debug: "Fire 1.50x weak: '45' size 42 bright 1.50 glow". */
	WORLD_OF_REFRACTION_API FString DescribeStyle(const FCombatHitShown &Hit, const FDamageNumberStyle &Style);
}
