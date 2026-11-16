// SkillGearRequirement.h
// D-224 (asset overhaul §5.2): a skill names the gear it needs; the gear names no skills. Read through
// USkillDataBase::GetGearRequirement and checked by SkillRequirementRules::IsMet.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Equipment/Rings/EAdapterForm.h"
#include "Skills/Definitions/ESpellElement.h"
#include "SkillGearRequirement.generated.h"

class UEvolutionItemData;

/** What kind of piece a skill needs to sit in. Append-only (saved in assets). */
UENUM(BlueprintType)
enum class ESkillGearKind : uint8
{
	/** Not authored yet: the old fields decide (an ability's RequiredWeaponType, a spell's RequiredEvolutionCrystal). */
	None UMETA(DisplayName = "None (old fields)"),
	Weapon UMETA(DisplayName = "Weapon"),
	Adapter UMETA(DisplayName = "Adapter"),
	Evolution UMETA(DisplayName = "Evolution")
};

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FSkillGearRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gear Requirement")
	ESkillGearKind Kind = ESkillGearKind::None;

	/** Off: any weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gear Requirement", meta = (EditCondition = "Kind == ESkillGearKind::Weapon", EditConditionHides))
	bool bNeedsWeaponType = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gear Requirement",
			  meta = (EditCondition = "Kind == ESkillGearKind::Weapon && bNeedsWeaponType", EditConditionHides))
	EWeaponType WeaponType = EWeaponType::Sword;

	/** Off: any adapter form. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gear Requirement", meta = (EditCondition = "Kind == ESkillGearKind::Adapter", EditConditionHides))
	bool bNeedsForm = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gear Requirement",
			  meta = (EditCondition = "Kind == ESkillGearKind::Adapter && bNeedsForm", EditConditionHides))
	EAdapterForm Form = EAdapterForm::Ring;

	/** Null: any evolution. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gear Requirement", meta = (EditCondition = "Kind == ESkillGearKind::Evolution", EditConditionHides))
	TObjectPtr<UEvolutionItemData> Evolution = nullptr;

	/** The element of the piece's socket. None: any. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gear Requirement")
	ESpellElement Element = ESpellElement::None;
};
