// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SpellSchool.generated.h"

/** D-146: exactly three schools, player-facing labels on spells (WoRGameNames::GetSpellSchoolName).
 *  Append only (saved in spell assets): a retired school stays and maps onto one of the three
 *  (SpellSchools::ToCurrent, Q-356). The AI never reads the school; it reads a spell's effects. */
UENUM(BlueprintType)
enum class ESpellSchool : uint8
{
    Destruction UMETA(DisplayName = "Destruction"),
    Enhancement UMETA(DisplayName = "Enhancement"),
    Restoration UMETA(DisplayName = "Restoration"),
    /** OBSOLETE (B-356): retired by D-146; reads as Enhancement (Q-356). Re-tag assets in the editor. */
    Conjuration UMETA(DisplayName = "Conjuration (retired: Enhancement)")
};

namespace SpellSchools
{
    /** The school a spell shows and plays as: a retired school's mapped one (Q-356), else itself. */
    inline ESpellSchool ToCurrent(ESpellSchool School)
    {
        return School == ESpellSchool::Conjuration ? ESpellSchool::Enhancement : School;
    }

    /** True for a school D-146 retired (it still loads; re-tag the asset). */
    inline bool IsRetired(ESpellSchool School)
    {
        return ToCurrent(School) != School;
    }
}