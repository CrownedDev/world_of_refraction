// HybridSpellColors.h
// Color system for Broken Darkness hybrid spell VFX
// Provides primary (element) and secondary (darkness) colors for Niagara/materials

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Infusion/ElementColors.h"
#include "HybridSpellColors.generated.h"

/**
 * Color data for hybrid spell VFX
 * Used by Niagara systems and materials to render BD spells
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FHybridSpellColorData
{
	GENERATED_BODY()

	/** Primary color (the absorbed element) */
	UPROPERTY(BlueprintReadOnly, Category = "Colors")
	FLinearColor PrimaryColor = FLinearColor::White;

	/** Secondary color (darkness overlay) */
	UPROPERTY(BlueprintReadOnly, Category = "Colors")
	FLinearColor SecondaryColor = FLinearColor::Black;

	/** Blended color (element + darkness mixed) */
	UPROPERTY(BlueprintReadOnly, Category = "Colors")
	FLinearColor BlendedColor = FLinearColor::Black;

	/** How much darkness to blend in (0 = pure element, 1 = pure darkness) */
	UPROPERTY(BlueprintReadOnly, Category = "Colors")
	float DarknessBlend = 0.5f;

	/** Is this a forbidden element (Light/Void)? Affects VFX intensity */
	UPROPERTY(BlueprintReadOnly, Category = "Colors")
	bool bIsForbidden = false;

	/** The source element */
	UPROPERTY(BlueprintReadOnly, Category = "Colors")
	ESpellElement Element = ESpellElement::Generic;

	FHybridSpellColorData() = default;

	FHybridSpellColorData(ESpellElement InElement, FLinearColor InPrimary, FLinearColor InSecondary,
						  FLinearColor InBlended, float InBlend, bool InForbidden)
		: PrimaryColor(InPrimary), SecondaryColor(InSecondary), BlendedColor(InBlended), DarknessBlend(InBlend), bIsForbidden(InForbidden), Element(InElement)
	{
	}
};

/**
 * Static helper class for Broken Darkness hybrid spell colors
 */
UCLASS()
class WORLD_OF_REFRACTION_API UHybridSpellColors : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ==================== CONSTANTS ====================

	/** Base darkness color for all BD spells */
	static inline const FLinearColor DarknessBase = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);

	/** Standard blend amount for normal hybrids */
	static constexpr float NormalDarknessBlend = 0.4f;

	/** Stronger blend for forbidden elements (more ominous) */
	static constexpr float ForbiddenDarknessBlend = 0.6f;

	/** Edge glow multiplier for forbidden elements */
	static constexpr float ForbiddenGlowIntensity = 1.5f;

	// ==================== CORE FUNCTIONS ====================

	/**
	 * Get complete color data for a hybrid spell
	 * @param AbsorbedElement The element that was absorbed
	 * @return Complete color data for VFX/materials
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Colors")
	static FHybridSpellColorData GetHybridSpellColors(ESpellElement AbsorbedElement);

	/**
	 * Get the primary (element) color for a hybrid spell
	 * Same as ElementColors::GetColorForElement but exposed for BP
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Colors")
	static FLinearColor GetPrimaryColor(ESpellElement Element);

	/**
	 * Get the darkness overlay color
	 * Slightly tinted based on element for visual cohesion
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Colors")
	static FLinearColor GetDarknessOverlayColor(ESpellElement Element);

	/**
	 * Calculate blended color (element + darkness)
	 * @param ElementColor The base element color
	 * @param BlendAmount How much darkness (0-1)
	 * @return Blended color
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Colors")
	static FLinearColor CalculateBlendedColor(FLinearColor ElementColor, float BlendAmount);

	/**
	 * Check if element is forbidden (Light/Void)
	 * Forbidden elements have more intense darkness effects
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Colors")
	static bool IsForbiddenElement(ESpellElement Element);

	// ==================== NIAGARA HELPERS ====================

	/**
	 * Get Niagara-ready color parameters
	 * Returns colors pre-configured for common Niagara setups
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Colors|Niagara")
	static void GetNiagaraColorParams(ESpellElement AbsorbedElement,
									  FLinearColor &OutCoreColor,
									  FLinearColor &OutEdgeColor,
									  FLinearColor &OutTrailColor,
									  float &OutGlowIntensity);

	/**
	 * Get particle spawn color (slightly brighter for visibility)
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Colors|Niagara")
	static FLinearColor GetParticleSpawnColor(ESpellElement Element);

	/**
	 * Get particle death color (darker, fading into shadow)
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Colors|Niagara")
	static FLinearColor GetParticleDeathColor(ESpellElement Element);

	// ==================== MATERIAL HELPERS ====================

	/**
	 * Get material parameter collection values for a hybrid spell
	 * Can be used to drive material instances
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Colors|Material")
	static void GetMaterialParams(ESpellElement AbsorbedElement,
								  FLinearColor &OutBaseColor,
								  FLinearColor &OutEmissiveColor,
								  float &OutDarknessAmount,
								  float &OutEmissiveIntensity);

	// ==================== SPECIAL CASES ====================

	/**
	 * Get color for Pure Broken Darkness (no element absorbed)
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Colors")
	static FHybridSpellColorData GetPureBrokenDarknessColors();

	/**
	 * Get color when in Overload state (more intense)
	 */
	UFUNCTION(BlueprintPure, Category = "BrokenDarkness|Colors")
	static FHybridSpellColorData GetOverloadColors(ESpellElement AbsorbedElement);

	// ==================== UNIFIED INFUSION COLORS ====================

	/**
	 * Get infusion colors for any character (unified entry point)
	 * Handles both normal characters and Broken Darkness
	 * @param Element The infusion element
	 * @param bIsBrokenDarkness Is this a BD character?
	 * @return Color data (BD gets darkness overlay, normal gets pure element)
	 */
	UFUNCTION(BlueprintPure, Category = "Infusion|Colors")
	static FHybridSpellColorData GetInfusionColors(ESpellElement Element, bool bIsBrokenDarkness);

	/**
	 * Get weapon infusion colors specifically
	 * Slightly different blend than spell VFX (weapon needs more visibility)
	 */
	UFUNCTION(BlueprintPure, Category = "Infusion|Colors")
	static FHybridSpellColorData GetWeaponInfusionColors(ESpellElement Element, bool bIsBrokenDarkness);

	/**
	 * Get ability infusion colors
	 * Medium blend between weapon (visible) and spell (dark)
	 */
	UFUNCTION(BlueprintPure, Category = "Infusion|Colors")
	static FHybridSpellColorData GetAbilityInfusionColors(ESpellElement Element, bool bIsBrokenDarkness);

	/**
	 * Get Niagara parameters for weapon infusion VFX
	 */
	UFUNCTION(BlueprintPure, Category = "Infusion|Colors|Niagara")
	static void GetWeaponInfusionNiagaraParams(ESpellElement Element, bool bIsBrokenDarkness,
											   FLinearColor &OutWeaponGlow,
											   FLinearColor &OutTrailColor,
											   FLinearColor &OutParticleColor,
											   float &OutGlowIntensity);

	/** Darkness blend for BD weapon infusion (less than spells for visibility) */
	static constexpr float WeaponDarknessBlend = 0.3f;

	/** Darkness blend for BD ability infusion */
	static constexpr float AbilityDarknessBlend = 0.35f;
};
