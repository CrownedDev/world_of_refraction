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
#include "Equipment/Rings/EAdapterForm.h"

// Forward-declared so this header stays light (it is included by SkillEffectDisplayNames.h).
enum class EDefenseType : uint8;
enum class ECombatRow : uint8;
enum class EFieldMovementType : uint8;
enum class ESpellSchool : uint8;
enum class EMerchantType : uint8;
enum class EEssenceType : uint8;
enum class EInventoryVerb : uint8;
enum class ECurrencyType : uint8;
enum class EQuestKind : uint8;
enum class ECrystalType : uint8;
enum class EWeaponType : uint8;
enum class EWeaponWieldMode : uint8;
enum class EItemTier : uint8;
enum class ESubStat : uint8;
enum class EInteractVerb : uint8;
enum class EVendorService : uint8;

namespace WoRGameNames
{
    /** "Fire", "Reality", ... Empty only for a value with no case (a bug the spec catches). */
    WORLD_OF_REFRACTION_API FText GetElementName(ESpellElement Element);

    /** "Damage Over Time", "Skip Turn", ... Element-specific names ("Fire DoT", "Rooted") stay in SkillEffectDisplayNames. */
    WORLD_OF_REFRACTION_API FText GetSkillEffectTypeName(ESkillEffectType EffectType);

    /** B-248 (DRY audit A8): an element's status-break name (its bar-cap trigger, BarCapTriggerResolver). Q-064a2
     *  (Crown, 2026-11-12): every break is "<Element> Break" ("Fire Break", "Water Break"), no special names. Empty for
     *  Generic, Reality and None (no element break; the trigger's name shows). */
    WORLD_OF_REFRACTION_API FText GetElementBreakName(ESpellElement Element);

    /** Q-064a2 (Crown, 2026-11-12): every damage over time is "<Element> DoT" ("Fire DoT"). Empty for Generic (a
     *  physical DoT keeps its own name), Reality and None. */
    WORLD_OF_REFRACTION_API FText GetElementDoTName(ESpellElement Element);

    /** Q-064a2: "{0} Break" / "{0} DoT" around an element's name-table entry (D-061 / D-062). */
    WORLD_OF_REFRACTION_API FText FormatBreakName(const FText &ElementName);
    WORLD_OF_REFRACTION_API FText FormatDoTName(const FText &ElementName);

    /** "Ice": Ice is a damage type, not an ESpellElement (D-029, B-064a), so its name-table entry lives here. */
    WORLD_OF_REFRACTION_API FText GetIceName();

    /** "Generic", "Refractor" (code: Caster), "Resonator". */
    WORLD_OF_REFRACTION_API FText GetCharacterClassName(ECharacterClass CharacterClass);

    /** "Guard", "Dodge" (the legacy Parry also reads "Guard"). */
    WORLD_OF_REFRACTION_API FText GetDefenseTypeName(EDefenseType DefenseType);

    /** B-365 (D-158): "Flawless Parry" (every hit of one attack Perfect Guarded, B-367b). */
    WORLD_OF_REFRACTION_API FText GetFlawlessParryName();

    /** "Front Row", "Middle Row", "Back Row". */
    WORLD_OF_REFRACTION_API FText GetCombatRowName(ECombatRow Row);

    /** B-458: a field movement's name ("Dash", "Glide", "Mark and Recall"): the field skill picker and the party screen. */
    WORLD_OF_REFRACTION_API FText GetFieldMovementName(EFieldMovementType Type);

    /** The six school names (D-163, B-374): "Destruction", "Conjuration", "Enhancement", "Restoration", "Manipulation", "Impairment". */
    WORLD_OF_REFRACTION_API FText GetSpellSchoolName(ESpellSchool School);

    /** The merchant and its wares in brackets: "Blacksmith (weapons, augment stones)". */
    WORLD_OF_REFRACTION_API FText GetMerchantTypeName(EMerchantType MerchantType);

    /** D-107: the spell-channelling gear is an "Adapter" to the player (the code keeps its Ring names until B-299). */
    WORLD_OF_REFRACTION_API FText GetAdapterWord();
    /** "Adapters" */
    WORLD_OF_REFRACTION_API FText GetAdapterWordPlural();
    /** "Active adapter" (the Resonator's one in use) */
    WORLD_OF_REFRACTION_API FText GetActiveAdapterWord();
    /** B-316: "yourself", the player's own character as a receiver (the shop's buy-for). */
    WORLD_OF_REFRACTION_API FText GetYourselfWord();
    /** B-170e: "Actions", the banked actions a turn spends (action pips, ActionPips.md). */
    WORLD_OF_REFRACTION_API FText GetActionsWord();
    /** B-170e: "End Turn", the button verb that ends the turn and banks the actions left. */
    WORLD_OF_REFRACTION_API FText GetEndTurnVerb();
    /** B-339: the inventory screen's button verbs: "Give", "Store", "Take", "Dismantle", "Convert to Gold". */
    WORLD_OF_REFRACTION_API FText GetInventoryVerbName(EInventoryVerb Verb);
    /** B-366b: the same, Convert naming the currency it pays (UEconomyService::GetPlaceCurrency). The one-argument
     *  form names the run currency. */
    WORLD_OF_REFRACTION_API FText GetInventoryVerbName(EInventoryVerb Verb, ECurrencyType ConvertPays);
    /** An adapter's form: "Ring", "Pistol", "Trainer". */
    WORLD_OF_REFRACTION_API FText GetAdapterFormName(EAdapterForm Form);

    /** OBSOLETE (B-333): typed essences retire (D-125); use GetCurrencyName.
     *  B-269: "Fire Essence", "Quartz Essence" (Generic, D-105), "Unknown Essence" (Reality, D-095). */
    WORLD_OF_REFRACTION_API FText GetEssenceName(EEssenceType Essence);

    /** B-333 (D-125): the seven balances' names: "Gold", "Prisms", "Diamond", "Gear Essence",
     *  "Skill Essence", "Elemental Essence", "Pillar Essence". */
    WORLD_OF_REFRACTION_API FText GetCurrencyName(ECurrencyType Currency);

    /** B-388: "Secret" (the Hidden kind's player-facing name, D-196), "Main", "Side". */
    WORLD_OF_REFRACTION_API FText GetQuestKindName(EQuestKind Kind);

    /** B-440a: a crystal's or stone's kind: "Garnet", "Quartz", "Damage Stone", "Stat Stone". Empty for None.
     *  The FString table (ItemIdentity::GetTypeName) stays for logs and the old widgets. */
    WORLD_OF_REFRACTION_API FText GetCrystalTypeName(ECrystalType Type);

    /** B-440a: "Sword", "Greatsword", "Gauntlets". */
    WORLD_OF_REFRACTION_API FText GetWeaponTypeName(EWeaponType Type);

    /** B-440a (D-180): how a weapon is held, for its kind line: "dual", "with shield". Empty for a single weapon. */
    WORLD_OF_REFRACTION_API FText GetWieldModeName(EWeaponWieldMode Mode);

    /** B-440a: a tier's letter, "F" to "S". */
    WORLD_OF_REFRACTION_API FText GetTierLetter(EItemTier Tier);

    /** B-453f (D-190, Q-453f): an element's short tag for "Element letters on colours": "Fi", "Wa", "Lt" (two letters,
     *  so Light and Lightning differ). Empty for None. */
    WORLD_OF_REFRACTION_API FText GetElementLetter(ESpellElement Element);

    /** B-445: a vendor's services beside Buy: "Sell", "Dismantle". */
    WORLD_OF_REFRACTION_API FText GetVendorServiceName(EVendorService Service);

    /** B-443: the interact prompt's verbs: "Talk", "Enter", "Pick up". Empty for None. */
    WORLD_OF_REFRACTION_API FText GetInteractVerbName(EInteractVerb Verb);

    /** B-442: a sub-stat's name: "Spell Damage", "Defence", "Status Multiplier". Empty for None. */
    WORLD_OF_REFRACTION_API FText GetSubStatName(ESubStat Stat);
}
