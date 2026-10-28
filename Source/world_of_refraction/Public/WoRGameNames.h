// WoRGameNames.h
// Player-facing names for the game's shared words (D-062): one LOCTEXT per enum value, so they
// survive cooked builds, where UMETA(DisplayName) metadata is stripped and
// UEnum::GetDisplayNameTextByValue falls back to the raw identifier ("DOT", "SkipTurn").
// Generated from each enum's UMETA DisplayName (B-126); edit the text here, not in the enum.
// Reality reads "Unknown" to players (D-095); code and docs keep Reality.
// A new enum value needs a case here: WoR.Text.GameNames fails until it has one.

#pragma once

#include "CoreMinimal.h"
#include "Character/ECharacterClass.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/ESkillEffectType.h"

// Forward-declared so this header stays light (it is included by SkillEffectDisplayNames.h).
enum class EDefenseType : uint8;
enum class ECombatRow : uint8;
enum class ESpellSchool : uint8;
enum class EMerchantType : uint8;

namespace WoRGameNames
{
    /** "Fire", "Reality", ... Empty only for a value with no case (a bug the spec catches). */
    WORLD_OF_REFRACTION_API FText GetElementName(ESpellElement Element);

    /** "Damage Over Time", "Skip Turn", ... Element-specific names ("Burn") stay in SkillEffectDisplayNames. */
    WORLD_OF_REFRACTION_API FText GetSkillEffectTypeName(ESkillEffectType EffectType);

    /** B-248 (DRY audit A8): an element's status-break name (its bar-cap trigger, BarCapTriggerResolver), e.g.
     *  Fire "Burn", Water "Frozen" (D-090). Empty for an element with no flavour name (the trigger's name shows). */
    WORLD_OF_REFRACTION_API FText GetElementBreakName(ESpellElement Element);

    /** "Generic", "Refractor" (code: Caster), "Resonator". */
    WORLD_OF_REFRACTION_API FText GetCharacterClassName(ECharacterClass CharacterClass);

    /** "Guard", "Dodge" (the legacy Parry also reads "Guard"). */
    WORLD_OF_REFRACTION_API FText GetDefenseTypeName(EDefenseType DefenseType);

    /** "Front Row", "Middle Row", "Back Row". */
    WORLD_OF_REFRACTION_API FText GetCombatRowName(ECombatRow Row);

    /** "Destruction", "Enhancement", ... */
    WORLD_OF_REFRACTION_API FText GetSpellSchoolName(ESpellSchool School);

    /** The merchant and its wares in brackets: "Blacksmith (weapons, augment stones)". */
    WORLD_OF_REFRACTION_API FText GetMerchantTypeName(EMerchantType MerchantType);
}
