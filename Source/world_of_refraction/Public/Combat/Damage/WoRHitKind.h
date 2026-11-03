// WoRHitKind.h
// B-273 (S6): a hit's kind in one struct. Buildup reads both axes (a Fire sword swing builds Fire and
// Slash, SB-11); damage reads the one damage type. FromPair is today's rule (the element wins, else the
// physical subtype). An Ice hit is {Water, None, Ice}: Water-keyed and Water-sourced (D-029, ED-04),
// dealing Ice. Spec WoR.Resistance.HitKind.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Resistance/EWoRDamageType.h"
#include "WoRHitKind.generated.h"

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FWoRHitKind
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	ESpellElement Element = ESpellElement::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	EPhysicalDamageType Physical = EPhysicalDamageType::None;

	/** What the hit deals. FromPair derives it; an override (Ice) is set after. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	EWoRDamageType DamageType = EWoRDamageType::None;

	static FWoRHitKind FromPair(ESpellElement InElement, EPhysicalDamageType InPhysical)
	{
		FWoRHitKind Kind;
		Kind.Element = InElement;
		Kind.Physical = InPhysical;
		Kind.DamageType = WoRDamageType::ToDamageType(InElement, InPhysical);
		return Kind;
	}

	/** A Water-keyed hit that deals Ice (D-029). */
	static FWoRHitKind Ice(EPhysicalDamageType InPhysical = EPhysicalDamageType::None)
	{
		FWoRHitKind Kind = FromPair(ESpellElement::Water, InPhysical);
		Kind.DamageType = EWoRDamageType::Ice;
		return Kind;
	}

	/** The element axis as a damage type: the dealt type when it is elemental (Ice included), else the
	 *  element's own; None for a non-elemental hit. */
	EWoRDamageType ElementType() const
	{
		const bool bDealtIsElemental = DamageType != EWoRDamageType::None && !WoRDamageType::IsPhysical(DamageType);
		return bDealtIsElemental ? DamageType : WoRDamageType::ElementToDamageType(Element);
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%s / %s -> %s"), *UEnum::GetValueAsString(Element), *UEnum::GetValueAsString(Physical),
							   *UEnum::GetValueAsString(DamageType));
	}
};
