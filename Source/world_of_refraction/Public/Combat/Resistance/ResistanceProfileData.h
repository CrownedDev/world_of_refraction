// ResistanceProfileData.h
// B-216 (B-095 folded in): the class / innate-element resistance table as a data asset, editable in
// the Details panel. A new asset starts with the C++ table's values (ClassInnateResistanceTable.h),
// so creating it changes no number; the C++ table stays the fallback while no asset exists.
// The game reads the one asset at RESISTANCE_PROFILE_ASSET_PATH.
// Debug: the Details-panel buttons below; wor.CompareResistance.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Character/ECharacterClass.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "ResistanceProfileData.generated.h"

/** One profile: percent per column (+resist / -weak), same columns as FResistanceRow. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FResistanceProfileRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element", meta = (Units = "Percent"))
	float Fire = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element", meta = (Units = "Percent"))
	float Water = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element", meta = (Units = "Percent"))
	float Earth = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element", meta = (Units = "Percent"))
	float Wind = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element", meta = (Units = "Percent"))
	float Light = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element", meta = (Units = "Percent"))
	float Darkness = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element", meta = (Units = "Percent"))
	float Lightning = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element", meta = (Units = "Percent"))
	float Void = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element", meta = (Units = "Percent"))
	float Reality = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (Units = "Percent"))
	float Slash = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (Units = "Percent"))
	float Pierce = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (Units = "Percent"))
	float Impact = 0.0f;

	ClassInnateResistanceTable::FResistanceRow ToTableRow() const;
	static FResistanceProfileRow FromTableRow(const ClassInnateResistanceTable::FResistanceRow &Row);
};

UCLASS(BlueprintType)
class WORLD_OF_REFRACTION_API UResistanceProfileData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UResistanceProfileData();

	/** Overrides every class while the character is Broken Darkness. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resistance")
	FResistanceProfileRow BrokenDarkness;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resistance")
	FResistanceProfileRow GenericClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resistance")
	FResistanceProfileRow ResonatorClass;

	/** Casters by their innate element. A missing element has no profile (all zero), as in the C++ table. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resistance")
	TMap<ESpellElement, FResistanceProfileRow> CasterByInnateElement;

	/** Same precedence as ClassInnateResistanceTable::ResolveRow: BD > Generic > Resonator > Caster. */
	ClassInnateResistanceTable::FResistanceRow ResolveRow(ECharacterClass Class, ESpellElement InnateElement, bool bIsBrokenDarkness) const;

	/** The asset at RESISTANCE_PROFILE_ASSET_PATH, or null while none exists (callers use the C++ table). */
	static const UResistanceProfileData *GetActive();

	/** Every profile, one line each (percent). */
	FString GetResistanceProfileString() const;

	UFUNCTION(CallInEditor, Category = "Debug")
	void PrintProfiles() const;

	/** Logs each profile that differs from the C++ table. */
	UFUNCTION(CallInEditor, Category = "Debug")
	void CompareWithCodeTable() const;

	/** Puts every profile back to the C++ table's values. */
	UFUNCTION(CallInEditor, Category = "Debug")
	void ResetToCodeTable();

private:
	void FillFromCodeTable();
};
