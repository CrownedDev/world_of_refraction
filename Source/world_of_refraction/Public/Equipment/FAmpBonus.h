// FAmpBonus.h
// B-382 (Q-164b A, Crown 2026-11-08): Amp lines, the deckbuilder's "+2 Amp" / "+1 Fire Amp". Each line has a
// scope (all actions, physical ones, spells, or one element's: the same scopes as EDamageScope, B-383); every
// line whose scope matches the action is added into one total, as element resistance is summed. The total adds flat to the skill's base
// damage before the charge / tier multipliers, once per action (UActionExecutor::PreviewActionDamage).
// Amp does not raise status buildup.

#pragma once

#include "CoreMinimal.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Inventory/ItemTier.h"
#include "FAmpBonus.generated.h"

// Authored range per line: -5..+5, the deckbuilder's small numbers (UHT meta clamps take literals). TODO: tune in PIE.

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FAmpBonus
{
	GENERATED_BODY()

	/** "+N Amp": every action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 All = 0;

	/** "+N Physical Amp": actions that are not spells (attacks and abilities, infused or not): the Raw axis (B-383). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 Physical = 0;

	/** "+N Spell Amp": spells (B-383). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amp", meta = (ClampMin = "-5", ClampMax = "5"))
	int32 Spell = 0;

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

	/** The one element line for Element (0 for None / Generic: Generic never reaches damage). */
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

	/** The total for an action of ResolvedElement: All, plus Spell (a spell) or Physical (not a spell), plus that
	 *  element's line. */
	int32 GetTotalFor(ESpellElement ResolvedElement, bool bIsSpell) const
	{
		return All + (bIsSpell ? Spell : Physical) + GetElementLine(ResolvedElement);
	}

	bool IsEmpty() const
	{
		return All == 0 && Physical == 0 && Spell == 0 && Fire == 0 && Water == 0 && Earth == 0 && Wind == 0 && Light == 0 &&
			   Darkness == 0 && Lightning == 0 && Void == 0 && Reality == 0;
	}

	/** Line-wise add (unclamped), as FResistanceBonus::Accumulate. */
	void Accumulate(const FAmpBonus &Other)
	{
		All += Other.All;
		Physical += Other.Physical;
		Spell += Other.Spell;
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
		Add(Spell, TEXT("Spell"));
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

namespace AmpRules
{
	/** B-387 (Q-387, the deckbuilder's TierLadders): the Amp a socketed crystal (or a seated evolution) grants to its own
	 *  element, by its tier: F 0, E 0, D 1, C 1, B 2, A 2, S 3. */
	inline int32 GetCrystalTierAmp(EItemTier Tier)
	{
		constexpr int32 LADDER[] = {0, 0, 1, 1, 2, 2, 3};
		const int32 Index = static_cast<int32>(Tier);
		return Index >= 0 && Index < UE_ARRAY_COUNT(LADDER) ? LADDER[Index] : 0;
	}

	/** Adds Value to Amp's line for Element (nothing for None / Generic, which have no line). */
	inline void AddElementLine(FAmpBonus &Amp, ESpellElement Element, int32 Value)
	{
		switch (Element)
		{
		case ESpellElement::Fire: Amp.Fire += Value; break;
		case ESpellElement::Water: Amp.Water += Value; break;
		case ESpellElement::Earth: Amp.Earth += Value; break;
		case ESpellElement::Wind: Amp.Wind += Value; break;
		case ESpellElement::Light: Amp.Light += Value; break;
		case ESpellElement::Darkness: Amp.Darkness += Value; break;
		case ESpellElement::Lightning: Amp.Lightning += Value; break;
		case ESpellElement::Void: Amp.Void += Value; break;
		case ESpellElement::Reality: Amp.Reality += Value; break;
		default: break;
		}
	}
}
