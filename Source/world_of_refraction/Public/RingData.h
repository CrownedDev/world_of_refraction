// RingData.h
// Ring data asset for Resonator class spell casting

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemTier.h"
#include "SpellElement.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "RingData.generated.h"

class USpellData;
class UItemData;

// Break calculation constants
namespace RingBreakConstants
{
	constexpr float BASE_BREAK_CHANCE_PER_TIER = 0.15f;
	constexpr float INFUSION_BREAK_BONUS = 0.10f;
	constexpr float S_TIER_INFUSION_BREAK = 0.05f;
	constexpr float CUSTOM_SPELL_BREAK_BONUS = 0.05f;
	constexpr float LOW_DURABILITY_THRESHOLD = 0.25f;
	constexpr float MED_DURABILITY_THRESHOLD = 0.50f;
	constexpr float LOW_DURABILITY_BREAK_BONUS = 0.10f;
	constexpr float MED_DURABILITY_BREAK_BONUS = 0.05f;
}

/**
 * Ring Data Asset - Resonator's primary spell source
 */
UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API URingData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ==================== IDENTITY ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FString RingName = TEXT("Unnamed Ring");

	/** Ring's element - determines spell element and cannot be Generic or BrokenDarkness */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ESpellElement Element = ESpellElement::Fire;

	/** Ring tier - affects break chance when casting higher-tier spells */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	EItemTier Tier = EItemTier::E_Tier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FString Description = TEXT("Ring description...");

	// ==================== SPELLS ====================

	/** Default spells - always safe to cast (no break risk from tier mismatch) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
	TArray<USpellData *> DefaultSpells;

	/** Custom/upgraded spells - may exceed ring tier (break risk) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells")
	TArray<USpellData *> CustomSpells;

	/** Maximum spell slots (default + custom combined) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spells", meta = (ClampMin = "1", ClampMax = "12"))
	int32 MaxSpellSlots = 6;

	// ==================== CRYSTAL SLOT ====================

	/** Refined crystal that defines ring's element */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crystal")
	UItemData *SlottedCrystal = nullptr;

	// ==================== DURABILITY ====================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability", meta = (ClampMin = "1"))
	int32 MaxDurability = 100;

	UPROPERTY(BlueprintReadOnly, Category = "Durability|Runtime")
	int32 CurrentDurability = 100;

	UPROPERTY(BlueprintReadOnly, Category = "Durability|Runtime")
	bool bIsBroken = false;

	// ==================== SPELL ACCESS ====================

	UFUNCTION(BlueprintPure, Category = "Ring|Spells")
	TArray<USpellData *> GetAvailableSpells() const
	{
		TArray<USpellData *> AllSpells;
		AllSpells.Append(DefaultSpells);
		AllSpells.Append(CustomSpells);
		return AllSpells;
	}

	UFUNCTION(BlueprintPure, Category = "Ring|Spells")
	bool CanCastSpell(USpellData *Spell) const
	{
		if (!Spell || bIsBroken)
			return false;
		return DefaultSpells.Contains(Spell) || CustomSpells.Contains(Spell);
	}

	UFUNCTION(BlueprintPure, Category = "Ring|Spells")
	bool IsCustomSpell(USpellData *Spell) const
	{
		return CustomSpells.Contains(Spell);
	}

	// ==================== DURABILITY ====================

	UFUNCTION(BlueprintPure, Category = "Ring|Durability")
	float GetDurabilityPercent() const
	{
		return MaxDurability > 0 ? static_cast<float>(CurrentDurability) / MaxDurability : 0.0f;
	}

	UFUNCTION(BlueprintCallable, Category = "Ring|Durability")
	void ApplyDurabilityDamage(int32 Damage)
	{
		CurrentDurability = FMath::Max(0, CurrentDurability - Damage);
		if (CurrentDurability <= 0)
		{
			bIsBroken = true;
		}
	}

	// ==================== BREAK SYSTEM ====================

	UFUNCTION(BlueprintPure, Category = "Ring|Break")
	float CalculateBreakChance(USpellData *Spell, bool bIsInfused) const;

	UFUNCTION(BlueprintCallable, Category = "Ring|Break")
	bool RollForBreak(float BreakChance)
	{
		if (BreakChance <= 0.0f)
			return false;
		if (BreakChance >= 1.0f)
		{
			bIsBroken = true;
			return true;
		}

		bool bBroke = FMath::FRand() < BreakChance;
		if (bBroke)
		{
			bIsBroken = true;
		}
		return bBroke;
	}

	UFUNCTION(BlueprintCallable, Category = "Ring|Break")
	void ForceBreak()
	{
		bIsBroken = true;
		CurrentDurability = 0;
	}

	UFUNCTION(BlueprintCallable, Category = "Ring|State")
	void ResetRingState()
	{
		CurrentDurability = MaxDurability;
		bIsBroken = false;
	}

	UFUNCTION(BlueprintPure, Category = "Ring")
	FString GetTierString() const { return TierHelpers::GetTierDisplayString(Tier); }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext &Context) const override
	{
		EDataValidationResult Result = Super::IsDataValid(Context);

		// Element validation
		if (Element == ESpellElement::Generic)
		{
			Context.AddError(FText::FromString(TEXT("Ring cannot have Generic element")));
			Result = EDataValidationResult::Invalid;
		}
		if (Element == ESpellElement::BrokenDarkness)
		{
			Context.AddError(FText::FromString(TEXT("Ring cannot have BrokenDarkness element")));
			Result = EDataValidationResult::Invalid;
		}

		// Spell count validation
		int32 TotalSpells = DefaultSpells.Num() + CustomSpells.Num();
		if (TotalSpells > MaxSpellSlots)
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Too many spells (%d) for spell slots (%d)"), TotalSpells, MaxSpellSlots)));
			Result = EDataValidationResult::Invalid;
		}

		if (DefaultSpells.Num() == 0)
		{
			Context.AddWarning(FText::FromString(TEXT("Ring has no default spells")));
		}

		return Result;
	}
#endif
};
