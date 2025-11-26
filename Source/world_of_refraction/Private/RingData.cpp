// RingData.cpp
// Implementation for ring data asset

#include "RingData.h"
#include "SpellData.h"
#include "CrystalData.h"

// Break chance constants
namespace RingBreakConstants
{
	constexpr float BASE_BREAK_CHANCE_PER_TIER = 0.15f;  // 15% per tier gap
	constexpr float INFUSION_BREAK_BONUS = 0.10f;        // +10% when infused
	constexpr float S_TIER_INFUSION_BREAK = 0.05f;       // 5% even for S-tier when infused
	constexpr float CUSTOM_SPELL_BREAK_BONUS = 0.05f;    // +5% for custom spells
	constexpr float DURABILITY_DAMAGE_ON_BREAK_CHECK = 5.0f; // Durability loss per cast
}

FString URingData::GetElementName() const
{
	const UEnum* EnumPtr = StaticEnum<ERefractionElement>();
	if (EnumPtr)
	{
		FString Name = EnumPtr->GetNameStringByValue(static_cast<int64>(Element));
		Name.RemoveFromStart(TEXT("ERefractionElement::"));
		return Name;
	}
	return TEXT("Unknown");
}

TArray<USpellData*> URingData::GetAvailableSpells() const
{
	TArray<USpellData*> Result;
	
	for (USpellData* Spell : DefaultSpells)
	{
		if (Spell)
		{
			Result.Add(Spell);
		}
	}
	
	for (USpellData* Spell : CustomSpells)
	{
		if (Spell)
		{
			Result.Add(Spell);
		}
	}
	
	return Result;
}

int32 URingData::GetSpellCount() const
{
	return GetAvailableSpells().Num();
}

bool URingData::CanCastSpell(USpellData* Spell) const
{
	if (!Spell || bIsBroken)
	{
		return false;
	}

	// Check if spell matches ring's element or is universal
	if (Spell->bIsUniversalSpell)
	{
		return true;
	}

	return Spell->Element == Element;
}

bool URingData::IsCustomSpell(USpellData* Spell) const
{
	if (!Spell)
	{
		return false;
	}

	return CustomSpells.Contains(Spell);
}

float URingData::GetDurabilityPercent() const
{
	if (MaxDurability <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(CurrentDurability / MaxDurability, 0.0f, 1.0f);
}

void URingData::ApplyDurabilityDamage(float Damage)
{
	if (bIsBroken)
	{
		return;
	}

	CurrentDurability = FMath::Max(0.0f, CurrentDurability - Damage);
	
	if (CurrentDurability <= 0.0f)
	{
		ForceBreak();
	}
}

bool URingData::CheckBreak()
{
	if (bIsBroken)
	{
		return true;
	}

	if (CurrentDurability <= 0.0f)
	{
		ForceBreak();
		return true;
	}

	return false;
}

void URingData::ForceBreak()
{
	if (bIsBroken)
	{
		return;
	}

	bIsBroken = true;
	CurrentDurability = 0.0f;

	UE_LOG(LogTemp, Warning, TEXT("Ring '%s' has broken!"), *RingName);
}

void URingData::ResetRingState()
{
	CurrentDurability = MaxDurability;
	bIsBroken = false;
}

float URingData::CalculateBreakChance(USpellData* Spell, bool bIsInfused) const
{
	using namespace RingBreakConstants;

	if (!Spell || bIsBroken)
	{
		return 0.0f;
	}

	float BreakChance = 0.0f;

	// Get spell tier (need to add this to SpellData)
	// For now, assume spells have an implicit tier based on cost/power
	// TODO: Use Spell->Tier when SpellData is updated
	ETier SpellTier = ETier::E; // Placeholder - should come from Spell->Tier

	// Calculate tier gap
	int32 TierGap = TierHelpers::GetTierGap(Tier, SpellTier);
	
	if (TierGap > 0)
	{
		// Spell is higher tier than ring
		BreakChance += TierGap * BASE_BREAK_CHANCE_PER_TIER;
	}

	// Infusion always adds break risk
	if (bIsInfused)
	{
		BreakChance += INFUSION_BREAK_BONUS;
		
		// Even S-tier rings can break when infused
		if (Tier == ETier::S && BreakChance == 0.0f)
		{
			BreakChance = S_TIER_INFUSION_BREAK;
		}
	}

	// Custom spells add extra risk
	if (IsCustomSpell(Spell))
	{
		BreakChance += CUSTOM_SPELL_BREAK_BONUS;
	}

	// Low durability increases break chance
	float DurabilityPercent = GetDurabilityPercent();
	if (DurabilityPercent < 0.25f)
	{
		BreakChance += 0.10f; // +10% when below 25% durability
	}
	else if (DurabilityPercent < 0.50f)
	{
		BreakChance += 0.05f; // +5% when below 50% durability
	}

	return FMath::Clamp(BreakChance, 0.0f, 1.0f);
}

bool URingData::RollForBreak(float BreakChance)
{
	if (bIsBroken || BreakChance <= 0.0f)
	{
		return false;
	}

	// Always apply some durability damage
	ApplyDurabilityDamage(RingBreakConstants::DURABILITY_DAMAGE_ON_BREAK_CHECK);

	// Roll for break
	float Roll = FMath::FRand();
	if (Roll < BreakChance)
	{
		ForceBreak();
		return true;
	}

	return false;
}

#if WITH_EDITOR
EDataValidationResult URingData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Name validation
	if (RingName.IsEmpty() || RingName == TEXT("Unnamed Ring"))
	{
		Context.AddWarning(FText::FromString(TEXT("Ring should have a meaningful name")));
	}

	// Element validation - rings shouldn't be Generic or BrokenDarkness
	if (Element == ERefractionElement::Generic)
	{
		Context.AddError(FText::FromString(TEXT("Rings cannot have Generic element")));
		Result = EDataValidationResult::Invalid;
	}
	if (Element == ERefractionElement::BrokenDarkness)
	{
		Context.AddError(FText::FromString(TEXT("Rings cannot have BrokenDarkness element")));
		Result = EDataValidationResult::Invalid;
	}

	// Spell validation
	int32 TotalSpells = 0;
	for (USpellData* Spell : DefaultSpells)
	{
		if (Spell)
		{
			TotalSpells++;
			// Check element match
			if (!Spell->bIsUniversalSpell && Spell->Element != Element)
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Default spell '%s' element doesn't match ring element"),
					*Spell->SpellName)));
				Result = EDataValidationResult::Invalid;
			}
		}
	}
	for (USpellData* Spell : CustomSpells)
	{
		if (Spell)
		{
			TotalSpells++;
		}
	}

	if (TotalSpells == 0)
	{
		Context.AddWarning(FText::FromString(TEXT("Ring has no spells assigned")));
	}
	else if (TotalSpells > MaxSpellSlots)
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("Ring has %d spells but only %d slots"),
			TotalSpells, MaxSpellSlots)));
		Result = EDataValidationResult::Invalid;
	}

	// Crystal validation
	if (CrystalSlot)
	{
		// Check crystal tier vs ring tier
		int32 CrystalTierGap = TierHelpers::GetTierGap(Tier, CrystalSlot->Tier);
		if (CrystalTierGap > 2)
		{
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("Crystal tier is %d ranks higher than ring tier - high break risk"),
				CrystalTierGap)));
		}
	}

	// Icon check
	if (Icon == nullptr)
	{
		Context.AddWarning(FText::FromString(TEXT("Ring has no icon assigned")));
	}

	return Result;
}
#endif
