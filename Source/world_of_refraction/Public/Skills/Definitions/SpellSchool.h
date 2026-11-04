// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SpellSchool.generated.h"

/** D-146 (corrected by Crown, 2026-11-04): exactly three schools, Conjuration (damage), Enhancement and
 *  Restoration, player-facing labels on spells (WoRGameNames::GetSpellSchoolName). Append only (saved in
 *  spell assets): a retired school stays and maps onto one of the three (SpellSchools::ToCurrent, Q-356).
 *  The AI never reads the school; it reads a spell's effects. */
UENUM(BlueprintType)
enum class ESpellSchool : uint8
{
    /** OBSOLETE (B-356c): retired by D-146 as corrected; reads as Conjuration (Q-356). Re-tag assets in the editor. */
    Destruction UMETA(DisplayName = "Destruction (retired: Conjuration)"),
    Enhancement UMETA(DisplayName = "Enhancement"),
    Restoration UMETA(DisplayName = "Restoration"),
    /** The damage school (D-146 corrected; B-356c). */
    Conjuration UMETA(DisplayName = "Conjuration")
};

namespace SpellSchools
{
    /** The school a spell shows and plays as: a retired school's mapped one (Q-356), else itself. */
    inline ESpellSchool ToCurrent(ESpellSchool School)
    {
        return School == ESpellSchool::Destruction ? ESpellSchool::Conjuration : School;
    }

    /** True for a school D-146 retired (it still loads; re-tag the asset). */
    inline bool IsRetired(ESpellSchool School)
    {
        return ToCurrent(School) != School;
    }
}