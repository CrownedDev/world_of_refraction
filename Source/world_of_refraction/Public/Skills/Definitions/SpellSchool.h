// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SpellSchool.generated.h"

/** D-163 (Crown, 2026-11-05; changes D-146): six schools, each a spell's main identity, shown on spells
 *  (WoRGameNames::GetSpellSchoolName): Destruction (damage), Conjuration (creation: a conjured thing, which may
 *  deal damage), Enhancement (buffs), Restoration (heals, cleanses), Impairment (weakening: stat debuffs) and
 *  Manipulation (control: taunt, forcing a target, loss of control). Append only (saved in spell assets).
 *  Status can come from any school. The AI never reads the school; it reads a spell's effects. The 6-per-school
 *  innate cap stays (SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL). */
UENUM(BlueprintType)
enum class ESpellSchool : uint8
{
    /** The damage school (D-163: brought back; B-356c had retired it onto Conjuration). */
    Destruction UMETA(DisplayName = "Destruction"),
    Enhancement UMETA(DisplayName = "Enhancement"),
    Restoration UMETA(DisplayName = "Restoration"),
    /** Creation: the conjured thing is the identity, even when it deals damage (D-163). */
    Conjuration UMETA(DisplayName = "Conjuration"),
    /** Crowd and conceptual control: taunt, forcing a target onto someone else, loss of control (D-163, B-374). */
    Manipulation UMETA(DisplayName = "Manipulation"),
    /** Debuff control: weakening the target, stat debuffs (D-163, B-374). */
    Impairment UMETA(DisplayName = "Impairment")
};

namespace SpellSchools
{
    /** The school a spell shows and plays as. No school is retired since D-163 (B-374): every school is itself.
     *  Kept as the one place a future retirement would map (append-only enum). */
    inline ESpellSchool ToCurrent(ESpellSchool School)
    {
        return School;
    }

    /** True for a retired school (it still loads; re-tag the asset). None since D-163. */
    inline bool IsRetired(ESpellSchool School)
    {
        return ToCurrent(School) != School;
    }
}
