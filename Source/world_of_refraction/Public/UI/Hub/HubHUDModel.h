// HubHUDModel.h
// B-443 (D-181, UIScreens #14, mockup screen_mockups/hub/hud/1_hub_hud_prompt.png): what the hub and trial-map
// HUD shows, with no widget in it, so the specs read it headless. UHubHUDWidget draws it.
//
// Two things beside the party strip (which exists): the floating interact prompt by what the player faces
// (key + verb + what: "X Talk Blacksmith"), and the money chip, shown only near a vendor or just after the
// balance changed.
// Debug: wor.UI.PrintHubHUD logs GetHubHUDString.

#pragma once

#include "CoreMinimal.h"
#include "Hub/InteractPrompt.h"
#include "UI/Combat/Battle/BattleHUDCard.h"
#include "UI/WoRIcon.h"

class APawn;
class UParty;
class UTexture2D;
class UWorld;
struct FFieldAttackEntry;
enum class ECurrencyType : uint8;

struct FHubHUDPrompt
{
	/** False: nothing in reach has a prompt. */
	bool bShown = false;
	/** "Talk", "Rest". */
	FText Verb;
	/** "Blacksmith"; may be empty. */
	FText What;
	/** Where the thing stands; the prompt floats above it. */
	FVector WorldLocation = FVector::ZeroVector;
	/** The nearest thing in reach is a vendor (the money shows). */
	bool bNearVendor = false;
};

/** B-503i: the field attack chip, a card after the party strip: an icon, the name, the element as a dot.
 *  OBSOLETE (B-503k, D-217): the strip above the card replaced the chip; delete with BuildFieldAttackChip after PIE. */
struct FHubHUDFieldAttackChip
{
	FText Name;
	/** The item's own icon; null: Icon, from the icon table. */
	UTexture2D *OwnIcon = nullptr;
	/** The table's icon: a weapon's type, else the entry's element badge (None for neither). */
	FWoRIcon Icon;
	/** Shown in the icon's place when there is no texture ("Sword", "Ring"); may be empty. */
	FText IconText;
	bool bElemental = false;
	FText ElementName;
	FLinearColor ElementColor = FLinearColor::White;
};

struct FHubHUDMoney
{
	ECurrencyType Currency = ECurrencyType(0);
	FText Value;
	FText Label;
};

namespace WoRHubHUD
{
	/** How long the money stays up after it changed. TODO: tune */
	inline constexpr float MONEY_SHOWN_SECONDS = 4.0f;

	/** How far above the thing's origin the prompt floats. TODO: tune per interactable (a head height) */
	inline constexpr float PROMPT_HEIGHT_CM = 110.0f;

	/** The prompt for the nearest thing in Pawn's reach. A nearest thing with no prompt (a memory spot) shows none,
	 *  and doesn't fall through to a farther one: the key would act on the nearest. */
	WORLD_OF_REFRACTION_API FHubHUDPrompt BuildPrompt(APawn *Pawn);

	/** The interact key's label for the device in use: "X" on a gamepad, "E" on the keyboard (InputMap.md). */
	WORLD_OF_REFRACTION_API FText GetInteractKeyLabel(bool bGamepad);

	/** B-491 (D-213): a prompt as one line, for the on-screen hints that aren't the floating prompt: the key, the
	 *  verb, the thing ("E  Open · Chest"; "E  Rest"). Empty for no prompt. */
	WORLD_OF_REFRACTION_API FText FormatPromptLine(const FInteractPrompt &Prompt, bool bGamepad = false);

	/** The money the place spends: Gold in the hub, Prisms in a run (D-159). */
	WORLD_OF_REFRACTION_API FHubHUDMoney BuildMoney(const UWorld *World);

	/** The money shows near a vendor, or for MONEY_SHOWN_SECONDS after it changed. */
	WORLD_OF_REFRACTION_API bool ShouldShowMoney(bool bNearVendor, float SecondsSinceChange);

	/** B-463b (UIScreens #28): in a run, the tracked quest's "Title: current goal"; empty in the hub or with none. */
	WORLD_OF_REFRACTION_API FText BuildQuestLine(const UWorld *World);

	/** B-503g: the party strip, one card per member in party order (members with no data are left out).
	 *  B-503j (D-216): the battle HUD's card; its data from the carried vitals (none: full, as the pause menu).
	 *  B-503m4 (D-217): the played member (PlayedSlot) is always the first card, the others after in party order.
	 *  D-229: a player character's card reads You or its player's tag in World (null: You). D-254: nothing under the
	 *  name (no Spirit of ... / Body of ...). */
	WORLD_OF_REFRACTION_API TArray<FBattleHUDCard> BuildPartyStrip(const UParty *Party, int32 PlayedSlot = INDEX_NONE,
																   const UWorld *World = nullptr);

	/** B-503m4: the party slot of the member Pawn plays (INDEX_NONE: none). */
	WORLD_OF_REFRACTION_API int32 FindPlayedMemberSlot(const APawn *Pawn);

	/** The strip's card level. B-597 (Crown, 2026-11-24): the Trial level on every map, the hub included (it was the
	 *  name-only Hub level there, D-216). */
	WORLD_OF_REFRACTION_API EBattleCardLevel GetCardLevel(const UWorld *World);

	/** B-503k (D-217): the field attack strip above a card (FFieldAttackEntry::Name: "Sword", "Fire Sword", "Fire")
	 *  shows with the Trial level (B-597: the hub too). */
	inline bool ShowsFieldAttackStrip(EBattleCardLevel Level) { return Level == EBattleCardLevel::Trial; }

	/** B-503k: the party strip's card (BuildPartyStrip's order) of the member in MemberSlot, played: B-503m4 (D-217)
	 *  the first card; INDEX_NONE for none. */
	WORLD_OF_REFRACTION_API int32 GetFieldAttackCardIndex(const UParty *Party, int32 MemberSlot);

	/** B-503k: the card of the member Pawn plays, who makes the field attack (INDEX_NONE: none). */
	WORLD_OF_REFRACTION_API int32 FindFieldAttackCard(const APawn *Pawn);

	// B-503i: the chip sizes to its content between these widths (layout units: 1920 wide at 16:9, at any
	// resolution); a longer name ends in "...". The party cards are the battle card's width (B-503j).
	inline constexpr float FIELD_ATTACK_CHIP_MIN_WIDTH = 207.0f;
	inline constexpr float FIELD_ATTACK_CHIP_MAX_WIDTH = 360.0f;
	/** The mockup's 12 px at 720p between cards, and before the chip. */
	inline constexpr float PARTY_STRIP_GAP = 18.0f;
	/** The HUD's layout width at 16:9 (the DPI curve scales 1080p's units to the screen). */
	inline constexpr float HUD_LAYOUT_WIDTH = 1920.0f;

	/** The widest the strip's row can get: Cards cards, the gaps and the chip at its maximum. */
	WORLD_OF_REFRACTION_API float GetPartyStripMaxWidth(int32 Cards);

	/** B-503i: the chip for Entry: its name ("Unarmed" for none), its icon and the element dot's colour.
	 *  OBSOLETE (B-503k): no caller but its spec; delete after B-503k's PIE pass. */
	WORLD_OF_REFRACTION_API FHubHUDFieldAttackChip BuildFieldAttackChip(const FFieldAttackEntry &Entry);

	WORLD_OF_REFRACTION_API FString GetHubHUDString(const UWorld *World);
}
