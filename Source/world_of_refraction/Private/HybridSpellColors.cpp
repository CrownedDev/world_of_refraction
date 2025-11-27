// HybridSpellColors.cpp
// Implementation for Broken Darkness hybrid spell color system

#include "HybridSpellColors.h"

// ==================== CORE FUNCTIONS ====================

FHybridSpellColorData UHybridSpellColors::GetHybridSpellColors(ESpellElement AbsorbedElement)
{
	// Pure BD (no absorption)
	if (AbsorbedElement == ESpellElement::Generic || 
		AbsorbedElement == ESpellElement::BrokenDarkness)
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
		DarknessBase.R + (ElementColor.R * 0.1f),
		DarknessBase.G + (ElementColor.G * 0.1f),
		DarknessBase.B + (ElementColor.B * 0.1f),
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
		FMath::Min(Base.R * 1.2f, 1.0f),
		FMath::Min(Base.G * 1.2f, 1.0f),
		FMath::Min(Base.B * 1.2f, 1.0f),
		1.0f
	);
}

FLinearColor UHybridSpellColors::GetParticleDeathColor(ESpellElement Element)
{
	// Fade to darkness (70% toward black)
	FLinearColor Base = ElementColors::GetColorForElement(Element);
	return CalculateBlendedColor(Base, 0.7f);
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
	OutEmissiveIntensity = Data.bIsForbidden ? 2.5f : 1.5f;
}

// ==================== SPECIAL CASES ====================

FHybridSpellColorData UHybridSpellColors::GetPureBrokenDarknessColors()
{
	// Pure BD = deep black with subtle purple tint
	FLinearColor PureDark = FLinearColor(0.08f, 0.02f, 0.12f, 1.0f);  // Very dark purple
	FLinearColor PureBlack = FLinearColor(0.02f, 0.02f, 0.02f, 1.0f);

	return FHybridSpellColorData(
		ESpellElement::BrokenDarkness,
		PureDark,      // Primary: dark purple
		PureBlack,     // Secondary: near black
		PureDark,      // Blended: same as primary
		0.9f,          // Heavy darkness
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
		FMath::Min(Data.PrimaryColor.R * 1.4f, 1.0f),
		FMath::Min(Data.PrimaryColor.G * 1.4f, 1.0f),
		FMath::Min(Data.PrimaryColor.B * 1.4f, 1.0f),
		1.0f
	);

	// Add red tint to secondary (danger/overflow feel)
	Data.SecondaryColor = FLinearColor(
		Data.SecondaryColor.R + 0.15f,
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
	float BlendAmount = bForbidden ? (WeaponDarknessBlend + 0.15f) : WeaponDarknessBlend;

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
	float BlendAmount = bForbidden ? (AbilityDarknessBlend + 0.15f) : AbilityDarknessBlend;

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
			Data.PrimaryColor.R * 0.6f,
			Data.PrimaryColor.G * 0.6f,
			Data.PrimaryColor.B * 0.6f,
			1.0f
		);
	}

	// Particles: Blended for BD, pure for normal
	OutParticleColor = Data.BlendedColor;

	// Glow intensity
	if (bIsBrokenDarkness)
	{
		// BD: Slightly more intense to show through darkness
		OutGlowIntensity = Data.bIsForbidden ? 1.8f : 1.3f;
	}
	else
	{
		// Normal: Standard intensity
		OutGlowIntensity = 1.0f;
	}
}
