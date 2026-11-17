// PlayerBuild.h
// B-554 (D-228, one-player stage 3): the player's character data is built, not authored. A copy of the shared player
// asset (UPartySessionSubsystem::PlayerCharacter, DA_Character_Player) takes the picks' name and class and the class
// table's numbers for that class and element (UClassDefaultLoadoutData::FindNumbers): world levels, action speed, the
// innate element and Broken Darkness, the portrait. Every reader of UCharacterData then works unchanged
// (OnePlayerCharacterPlan.md section 2). The copy has a fixed name (BUILT_NAME), so the pool's key for the player stays
// the same across loads; a save stores the picks and the load builds it again.
// B-556 (stage 4): its inventory is made from the picks and the class table (MakeInventory), not an old Lord's.
// Debug: wor.Player.PrintBuilt.

#pragma once

#include "CoreMinimal.h"

class UCharacterData;
class UGameInstance;
class UWorld;
class UInventoryData;
class UClassDefaultLoadoutData;
struct FClassElementNumbers;
struct FGuid;
struct FPartyMember;
struct FPlayerPicks;
enum class ECharacterClass : uint8;
enum class ESpellElement : uint8;

namespace WoRPlayerBuild
{
	/** The built player's object name, in the transient package. */
	WORLD_OF_REFRACTION_API extern const TCHAR *BUILT_NAME;

	/** Template copied, named ObjectName (an older object of that name is renamed away first), with the picks and the
	 *  class numbers filled in. Numbers null: the template's levels and portrait stay. An ActionSpeed of 0 is the
	 *  normal speed (no Action Speed points: animations at their own pace). Null without a template or picks. */
	WORLD_OF_REFRACTION_API UCharacterData *BuildFrom(const FPlayerPicks &Picks, const UCharacterData *Template,
													  const FClassElementNumbers *Numbers, int32 ActionSpeed,
													  UInventoryData *Inventory, FName ObjectName);

	/** BuildFrom with the game's own assets: the player asset, the class table and an inventory made from the picks
	 *  (MakeInventory). A recruit (B-557) already built under its name is returned as it is, never built twice. Null, with a warning, when the picks are unset or the player asset is missing. */
	WORLD_OF_REFRACTION_API UCharacterData *Build(const FPlayerPicks &Picks, const UGameInstance *GameInstance);

	/** B-556 (D-228, stage 4): the built player's own inventory: the picked pieces as its starting gear (no base kit),
	 *  a Resonator's picked crystals in its stock, and one authored loadout with the class numbers' innate spells and
	 *  Broken Darkness pools (Numbers null: none). Transient. */
	WORLD_OF_REFRACTION_API UInventoryData *MakeInventory(const FPlayerPicks &Picks, const UClassDefaultLoadoutData &Table,
														  const FClassElementNumbers *Numbers);

	/** B-557 (D-238): the object name Build gives these picks: BUILT_NAME for the player, BUILT_NAME_Recruit_<class>_
	 *  <element> for a hub recruit (bRecruit), so a recruit never takes the player's name and a load finds it again.
	 *  B-561: BUILT_NAME_Guest_<key>_<class>_<element> for a co-op guest (bGuest): its GuestKey, the joining player's
	 *  name (D-228, 2026-11-17; an older save's guest without one: its typed name). */
	WORLD_OF_REFRACTION_API FName GetBuiltName(const FPlayerPicks &Picks);

	/** B-557 (D-238): the class table's recruit row for this character's class (a Refractor: and element, Broken
	 *  Darkness), or null (no row: the character stays as it is). */
	WORLD_OF_REFRACTION_API const FPlayerPicks *FindRecruitPicks(const UCharacterData &Character, const UGameInstance *GameInstance);

	/** A character made by BuildFrom (its name starts with BUILT_NAME and it lives in the transient package). */
	WORLD_OF_REFRACTION_API bool IsBuilt(const UCharacterData *Character);

	/** D-229 (Crown): a player's own character, built from picks and not a hub recruit. */
	WORLD_OF_REFRACTION_API bool IsPlayerCharacter(const UCharacterData *Character);

	/** D-229 (Crown, "How a player's card reads"): a player character's card reads "You" to its own player and its
	 *  player's gamer tag to everyone else ("Player" with none); a recruit or an enemy reads its own name. */
	WORLD_OF_REFRACTION_API FText MakeCardName(const UCharacterData &Character, bool bPlayerCharacter, bool bViewersOwn,
											   const FString &OwnerTag);

	/** D-229: MakeCardName for World's local viewer. The owner is the player claiming MemberId (else Character); none
	 *  claims it, or the first local player does: the viewer's own. The tag is the owner's PlayerState name. */
	WORLD_OF_REFRACTION_API FText GetCardName(const UCharacterData *Character, const FGuid &MemberId, const UWorld *World);

	/** D-241 (Crown): the word after "Spirit of": a Refractor's element ("Fire"; Reality "Unknown"; "Broken Darkness"),
	 *  a Generic's or Resonator's class name. */
	WORLD_OF_REFRACTION_API FText GetSpiritName(ECharacterClass Class, ESpellElement Element, bool bBrokenDarkness);

	/** D-241: a character's identity, two lines: "Spirit of Fire" and, with a body name, "Body of Male Nobody". */
	WORLD_OF_REFRACTION_API FText MakeIdentityLines(const FText &SpiritName, const FText &BodyName);

	/** D-241: the body this game's player wears: its worn form's name ("Orthos"), else Male / Female Nobody; empty with
	 *  no form system. */
	WORLD_OF_REFRACTION_API FText GetLocalBodyName(const UGameInstance *GameInstance);

	/** D-229 / D-241: the lines under a player character's name on its card: MakeIdentityLines. The body shows on the
	 *  viewer's own character only (another player's body isn't known here). Empty for a recruit or an enemy. */
	WORLD_OF_REFRACTION_API FText GetDetailLine(const UCharacterData *Character, const FGuid &MemberId, const UWorld *World);

	/** D-241: a party member's name in a list (pickers, seats): GetCardName ("You", the gamer tag, a recruit's own name);
	 *  when the same player has more than one character in Members, its identity lines under it to tell them apart. */
	WORLD_OF_REFRACTION_API FText GetMemberListName(const FPartyMember &Member, TConstArrayView<FPartyMember> Members, const UWorld *World);

	/** OBSOLETE (D-229, replaced by GetCardName: each viewer reads You or the tag; no longer called, delete after PIE).
	 *  Two players' characters of one kind read apart by the player's tag: a built player character
	 *  (picks set, not a recruit) that shares its kind name with another is named "Fire Refractor (tag)", the tag its
	 *  GuestKey for a guest and HostTag for the host's own; with no clash, the kind name alone. True when a name
	 *  changed. */
	WORLD_OF_REFRACTION_API bool RetagSameNames(TArrayView<FPartyMember> Members, const FString &HostTag);

	/** Debug: the built character's name, class, element, levels, action speed and inventory, one line. */
	WORLD_OF_REFRACTION_API FString GetBuiltString(const UCharacterData *Character);
}
