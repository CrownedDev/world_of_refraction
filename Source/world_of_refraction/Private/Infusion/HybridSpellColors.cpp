// HybridSpellColors.cpp
// Implementation for Broken Darkness hybrid spell color system

#include "Infusion/HybridSpellColors.h"

namespace
{
	// ---- Darkness overlay ----
	constexpr float DARKNESS_OVERLAY_ELEMENT_TINT = 0.1f;

	// ---- Particle colours ----
	constexpr float PARTICLE_SPAWN_BRIGHTNESS_BOOST = 1.2f;
	constexpr float PARTICLE_DEATH_DARKNESS_BLEND   = 0.7f;

	// ---- Material emissive intensity ----
	constexpr float EMISSIVE_INTENSITY_FORBIDDEN = 2.5f;
	constexpr float EMISSIVE_INTENSITY_NORMAL    = 1.5f;

	// ---- Pure Broken Darkness colours ----
	constexpr FLinearColor PURE_BD_PRIMARY   = FLinearColor(0.08f, 0.02f, 0.12f, 1.0f);
	constexpr FLinearColor PURE_BD_SECONDARY = FLinearColor(0.02f, 0.02f, 0.02f, 1.0f);
	constexpr float        PURE_BD_DARKNESS_BLEND = 0.9f;

	// ---- Overload ----
	constexpr float OVERLOAD_COLOR_BOOST = 1.4f;
	constexpr float OVERLOAD_RED_TINT    = 0.15f;

	// ---- Forbidden-element blend bonus (weapon + ability paths) ----
	constexpr float FORBIDDEN_BLEND_BONUS = 0.15f;

	// ---- Weapon trail / glow ----
	constexpr float WEAPON_TRAIL_DIM         = 0.6f;
	constexpr float BD_WEAPON_GLOW_FORBIDDEN = 1.8f;
	constexpr float BD_WEAPON_GLOW_NORMAL    = 1.3f;
}

// ==================== CORE FUNCTIONS ====================

FHybridSpellColorData UHybridSpellColors::GetHybridSpellColors(ESpellElement AbsorbedElement)
{
	// Pure BD (base state) — Darkness is the seeded base pool (Model B); when the active
	// pool is the base, render pure BD black rather than a blend.
	if (AbsorbedElement == ESpellElement::Darkness)
	{
		return GetPureBrokenDarknessColors();
	}

	bool bForbidden = IsForbiddenElement(AbsorbedElement);
	float BlendAmount = bForbidden ? ForbiddenDarknessBlend : NormalDarknessBlend;

	FLinearColor Primary = GetPrimaryColor(AbsorbedElement);
	FLinearColor Secondary = GetDarknessOverlayColor(AbsorbedElement);
	FLinearColor Blended = CalculateBlendedColor(Primary, BlendAmount);

	return FHybridSpellColorData(
		AbsorbedElement,
		Primary,
		Secondary,
		Blended,
		BlendAmount,
		bForbidden
	);
}

FLinearColor UHybridSpellColors::GetPrimaryColor(ESpellElement Element)
{
	return ElementColors::GetColorForElement(Element);
}

FLinearColor UHybridSpellColors::GetDarknessOverlayColor(ESpellElement Element)
{
	// Base darkness with slight element tint for visual cohesion
	FLinearColor ElementColor = ElementColors::GetColorForElement(Element);
	
	// Very subtle tint (10% element, 90% pure black)
	return FLinearColor(
		DarknessBase.R + (ElementColor.R * DARKNESS_OVERLAY_ELEMENT_TINT),
		DarknessBase.G + (ElementColor.G * DARKNESS_OVERLAY_ELEMENT_TINT),
		DarknessBase.B + (ElementColor.B * DARKNESS_OVERLAY_ELEMENT_TINT),
		1.0f
	);
}

FLinearColor UHybridSpellColors::CalculateBlendedColor(FLinearColor ElementColor, float BlendAmount)
{
	// Lerp between element color and darkness
	return FLinearColor(
		FMath::Lerp(ElementColor.R, DarknessBase.R, BlendAmount),
		FMath::Lerp(ElementColor.G, DarknessBase.G, BlendAmount),
		FMath::Lerp(ElementColor.B, DarknessBase.B, BlendAmount),
		1.0f
	);
}

bool UHybridSpellColors::IsForbiddenElement(ESpellElement Element)
{
	return Element == ESpellElement::Light || Element == ESpellElement::Void;
}

// ==================== NIAGARA HELPERS ====================

void UHybridSpellColors::GetNiagaraColorParams(ESpellElement AbsorbedElement,
	FLinearColor& OutCoreColor,
	FLinearColor& OutEdgeColor,
	FLinearColor& OutTrailColor,
	float& OutGlowIntensity)
{
	FHybridSpellColorData Data = GetHybridSpellColors(AbsorbedElement);

	// Core: Bright element color (center of particles/effects)
	OutCoreColor = Data.PrimaryColor;
	
	// Edge: Darker, more blended (outer rim of particles)
	OutEdgeColor = Data.BlendedColor;
	
	// Trail: Pure darkness (smoke/shadow trails)
	OutTrailColor = Data.SecondaryColor;
	
	// Glow intensity: Higher for forbidden elements
	OutGlowIntensity = Data.bIsForbidden ? ForbiddenGlowIntensity : 1.0f;
}

FLinearColor UHybridSpellColors::GetParticleSpawnColor(ESpellElement Element)
{
	// Spawn bright (element dominant)
	FLinearColor Base = ElementColors::GetColorForElement(Element);
	
	// Boost brightness slightly
	return FLinearColor(
		FMath::Min(Base.R * PARTICLE_SPAWN_BRIGHTNESS_BOOST, 1.0f),
		FMath::Min(Base.G * PARTICLE_SPAWN_BRIGHTNESS_BOOST, 1.0f),
		FMath::Min(Base.B * PARTICLE_SPAWN_BRIGHTNESS_BOOST, 1.0f),
		1.0f
	);
}

FLinearColor UHybridSpellColors::GetParticleDeathColor(ESpellElement Element)
{
	// Fade to darkness (70% toward black)
	FLinearColor Base = ElementColors::GetColorForElement(Element);
	return CalculateBlendedColor(Base, PARTICLE_DEATH_DARKNESS_BLEND);
}

// ==================== MATERIAL HELPERS ====================

void UHybridSpellColors::GetMaterialParams(ESpellElement AbsorbedElement,
	FLinearColor& OutBaseColor,
	FLinearColor& OutEmissiveColor,
	float& OutDarknessAmount,
	float& OutEmissiveIntensity)
{
	FHybridSpellColorData Data = GetHybridSpellColors(AbsorbedElement);

	// Base color: Blended for surface
	OutBaseColor = Data.BlendedColor;
	
	// Emissive: Pure element color for glow
	OutEmissiveColor = Data.PrimaryColor;
	
	// Darkness amount: For material lerp nodes
	OutDarknessAmount = Data.DarknessBlend;
	
	// Emissive intensity: Forbidden elements glow more ominously
	OutEmissiveIntensity = Data.bIsForbidden ? EMISSIVE_INTENSITY_FORBIDDEN : EMISSIVE_INTENSITY_NORMAL;
}

// ==================== SPECIAL CASES ====================

FHybridSpellColorData UHybridSpellColors::GetPureBrokenDarknessColors()
{
	// Pure BD = deep black with subtle purple tint
	FLinearColor PureDark = PURE_BD_PRIMARY;  // Very dark purple
	FLinearColor PureBlack = PURE_BD_SECONDARY;

	return FHybridSpellColorData(
		ESpellElement::Darkness, // .Element is cosmetic/unread; underlying BD element is Darkness
		PureDark,      // Primary: dark purple
		PureBlack,     // Secondary: near black
		PureDark,      // Blended: same as primary
		PURE_BD_DARKNESS_BLEND, // Heavy darkness
		false          // Not forbidden
	);
}

FHybridSpellColorData UHybridSpellColors::GetOverloadColors(ESpellElement AbsorbedElement)
{
	// Start with normal hybrid colors
	FHybridSpellColorData Data = GetHybridSpellColors(AbsorbedElement);

	// Overload = more intense, crackling energy
	// Boost primary color intensity
	Data.PrimaryColor = FLinearColor(
		FMath::Min(Data.PrimaryColor.R * OVERLOAD_COLOR_BOOST, 1.0f),
		FMath::Min(Data.PrimaryColor.G * OVERLOAD_COLOR_BOOST, 1.0f),
		FMath::Min(Data.PrimaryColor.B * OVERLOAD_COLOR_BOOST, 1.0f),
		1.0f
	);

	// Add red tint to secondary (danger/overflow feel)
	Data.SecondaryColor = FLinearColor(
		Data.SecondaryColor.R + OVERLOAD_RED_TINT,
		Data.SecondaryColor.G,
		Data.SecondaryColor.B,
		1.0f
	);

	// Recalculate blend with boosted colors
	Data.BlendedColor = FLinearColor(
		FMath::Lerp(Data.PrimaryColor.R, Data.SecondaryColor.R, Data.DarknessBlend),
		FMath::Lerp(Data.PrimaryColor.G, Data.SecondaryColor.G, Data.DarknessBlend),
		FMath::Lerp(Data.PrimaryColor.B, Data.SecondaryColor.B, Data.DarknessBlend),
		1.0f
	);

	return Data;
}

// ==================== UNIFIED INFUSION COLORS ====================

FHybridSpellColorData UHybridSpellColors::GetInfusionColors(ESpellElement Element, bool bIsBrokenDarkness)
{
	if (!bIsBrokenDarkness)
	{
		// Normal character: pure element color, no darkness
		FLinearColor ElementColor = ElementColors::GetColorForElement(Element);
		return FHybridSpellColorData(
			Element,
			ElementColor,       // Primary: pure element
			ElementColor,       // Secondary: same (no darkness)
			ElementColor,       // Blended: same
			0.0f,               // No darkness blend
			false               // Not forbidden (only BD cares about this)
		);
	}

	// Broken Darkness: use hybrid colors with darkness overlay
	return GetHybridSpellColors(Element);
}

FHybridSpellColorData UHybridSpellColors::GetWeaponInfusionColors(ESpellElement Element, bool bIsBrokenDarkness)
{
	if (!bIsBrokenDarkness)
	{
		// Normal character: pure element color
		FLinearColor ElementColor = ElementColors::GetColorForElement(Element);
		return FHybridSpellColorData(
			Element,
			ElementColor,
			ElementColor,
			ElementColor,
			0.0f,
			false
		);
	}

	// BD: Less darkness than spells for weapon visibility
	bool bForbidden = IsForbiddenElement(Element);
	float BlendAmount = bForbidden ? (WeaponDarknessBlend + FORBIDDEN_BLEND_BONUS) : WeaponDarknessBlend;

	FLinearColor Primary = GetPrimaryColor(Element);
	FLinearColor Secondary = GetDarknessOverlayColor(Element);
	FLinearColor Blended = CalculateBlendedColor(Primary, BlendAmount);

	return FHybridSpellColorData(
		Element,
		Primary,
		Secondary,
		Blended,
		BlendAmount,
		bForbidden
	);
}

FHybridSpellColorData UHybridSpellColors::GetAbilityInfusionColors(ESpellElement Element, bool bIsBrokenDarkness)
{
	if (!bIsBrokenDarkness)
	{
		// Normal character: pure element color
		FLinearColor ElementColor = ElementColors::GetColorForElement(Element);
		return FHybridSpellColorData(
			Element,
			ElementColor,
			ElementColor,
			ElementColor,
			0.0f,
			false
		);
	}

	// BD: Medium darkness (between weapon and spell)
	bool bForbidden = IsForbiddenElement(Element);
	float BlendAmount = bForbidden ? (AbilityDarknessBlend + FORBIDDEN_BLEND_BONUS) : AbilityDarknessBlend;

	FLinearColor Primary = GetPrimaryColor(Element);
	FLinearColor Secondary = GetDarknessOverlayColor(Element);
	FLinearColor Blended = CalculateBlendedColor(Primary, BlendAmount);

	return FHybridSpellColorData(
		Element,
		Primary,
		Secondary,
		Blended,
		BlendAmount,
		bForbidden
	);
}

void UHybridSpellColors::GetWeaponInfusionNiagaraParams(ESpellElement Element, bool bIsBrokenDarkness,
	FLinearColor& OutWeaponGlow,
	FLinearColor& OutTrailColor,
	FLinearColor& OutParticleColor,
	float& OutGlowIntensity)
{
	FHybridSpellColorData Data = GetWeaponInfusionColors(Element, bIsBrokenDarkness);

	// Weapon glow: Primary color (element dominant for visibility)
	OutWeaponGlow = Data.PrimaryColor;

	// Trail: For BD, use darkness-blended. For normal, slightly dimmer element.
	if (bIsBrokenDarkness)
	{
		OutTrailColor = Data.SecondaryColor;
	}
	else
	{
		// Normal: dimmer version of element color
		OutTrailColor = FLinearColor(
			Data.PrimaryColor.R * WEAPON_TRAIL_DIM,
			Data.PrimaryColor.G * WEAPON_TRAIL_DIM,
			Data.PrimaryColor.B * WEAPON_TRAIL_DIM,
			1.0f
		);
	}

	// Particles: Blended for BD, pure for normal
	OutParticleColor = Data.BlendedColor;

	// Glow intensity
	if (bIsBrokenDarkness)
	{
		// BD: Slightly more intense to show through darkness
		OutGlowIntensity = Data.bIsForbidden ? BD_WEAPON_GLOW_FORBIDDEN : BD_WEAPON_GLOW_NORMAL;
	}
	else
	{
		// Normal: Standard intensity
		OutGlowIntensity = 1.0f;
	}
}
