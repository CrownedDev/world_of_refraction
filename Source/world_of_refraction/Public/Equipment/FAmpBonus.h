// FAmpBonus.h
// B-382 (Q-164b A, Crown 2026-11-08): Amp lines, the deckbuilder's "+2 Amp" / "+1 Fire Amp". Each line has a
// scope (all hits, physical hits, or one element's); every line whose scope matches the action's resolved
// element is added into one total, as element resistance is summed. The total adds flat to the skill's base
// damage before the charge / tier multipliers, once per action (UActionExecutor::PreviewActionDamage).
// Amp does not raise status buildup.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"
#include "FAmpBonus.generated.h"

namespace AmpRules
{
	/** Authored range per line (the deckbuilder's small numbers). TODO: tune in PIE. UHT meta clamps below mirror it. */
	constexpr int32 MAX_LINE = 5;
}

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FAmpBonus
{
	GENERATED_BODY()

	/** "+N Amp": every action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 All = 0;

	/** "+N Physical Amp": non-elemental actions (element None). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 Physical = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp|Element", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 Fire = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp|Element", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 Water = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp|Element", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 Earth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp|Element", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 Wind = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp|Element", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 Light = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp|Element", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 Darkness = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp|Element", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 Lightning = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp|Element", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 Void = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp|Element", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 Reality = 0;

	/** The one element line for Element (0 for None / Generic: Generic never reaches damage, None is Physical). */
	int32 GetElementLine(ESpellElement Element) const
	{
		switch (Element)
		{
		case ESpellElement::Fire: return Fire;
		case ESpellElement::Water: return Water;
		case ESpellElement::Earth: return Earth;
		case ESpellElement::Wind: return Wind;
		case ESpellElement::Light: return Light;
		case ESpellElement::Darkness: return Darkness;
		case ESpellElement::Lightning: return Lightning;
		case ESpellElement::Void: return Void;
		case ESpellElement::Reality: return Reality;
		default: return 0;
		}
	}

	/** The total for an action of ResolvedElement: All, plus Physical (element None) or that element's line. */
	int32 GetTotalFor(ESpellElement ResolvedElement) const
	{
		return All + (ResolvedElement == ESpellElement::None ? Physical : GetElementLine(ResolvedElement));
	}

	bool IsEmpty() const
	{
		return All == 0 && Physical == 0 && Fire == 0 && Water == 0 && Earth == 0 && Wind == 0 && Light == 0 &&
			   Darkness == 0 && Lightning == 0 && Void == 0 && Reality == 0;
	}

	/** Line-wise add (unclamped), as FResistanceBonus::Accumulate. */
	void Accumulate(const FAmpBonus &Other)
	{
		All += Other.All;
		Physical += Other.Physical;
		Fire += Other.Fire;
		Water += Other.Water;
		Earth += Other.Earth;
		Wind += Other.Wind;
		Light += Other.Light;
		Darkness += Other.Darkness;
		Lightning += Other.Lightning;
		Void += Other.Void;
		Reality += Other.Reality;
	}

	/** "+2 Amp, +1 Fire" style (debug; empty lines left out). */
	FString ToString() const
	{
		FString Out;
		auto Add = [&Out](int32 Value, const TCHAR *Name)
		{
			if (Value != 0)
			{
				Out += FString::Printf(TEXT("%s%+d %s"), Out.IsEmpty() ? TEXT("") : TEXT(", "), Value, Name);
			}
		};
		Add(All, TEXT("Amp"));
		Add(Physical, TEXT("Physical"));
		Add(Fire, TEXT("Fire"));
		Add(Water, TEXT("Water"));
		Add(Earth, TEXT("Earth"));
		Add(Wind, TEXT("Wind"));
		Add(Light, TEXT("Light"));
		Add(Darkness, TEXT("Darkness"));
		Add(Lightning, TEXT("Lightning"));
		Add(Void, TEXT("Void"));
		Add(Reality, TEXT("Reality"));
		return Out.IsEmpty() ? TEXT("no Amp") : Out;
	}
};
